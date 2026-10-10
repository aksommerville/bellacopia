/* battle_blackjack.c
 * This battle is one hand of blackjack.
 * How it plays out in the game, is after each winning hand, you're prompted to raise the stakes.
 * See activity_sidequests.c:begin_blackjack().
 */

#include "game/batsup/battle_internal.h"
#include "game/batsup/cards.h"

#define OPTION_LIMIT 2
#define HAND_LIMIT 12

#define OPTION_DRAW 357 /* strix in strings:battle */
#define OPTION_STAND 358

struct battle_blackjack {
  struct battle hdr;
  
  struct option {
    int texid,w,h,strix;
  } optionv[OPTION_LIMIT];
  int optionc;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid_hand;
    uint32_t color;
    int y; // Middle of my row.
    uint8_t handv[HAND_LIMIT];
    int handc;
    int sum; // Includes (handv[0]) only if (exposed).
    int exposed; // Is (handv[0]) face up?
    int blackout;
    int optionp; // (0..optionc-1) if menu visible, <0 if standing.
    double delay; // CPU
    double bustclock;
    int bustframe;
    int soulsold; // If you sold your soul to the devil for this game, each draw is extra lucky.
  } playerv[2];
  
  struct deck deck;
};

#define BATTLE ((struct battle_blackjack*)battle)

/* Delete.
 */
 
static void _blackjack_del(struct battle *battle) {
  struct option *option=BATTLE->optionv;
  int i=BATTLE->optionc;
  for (;i-->0;option++) {
    egg_texture_del(option->texid);
  }
}

/* Set a randomish delay for the CPU.
 */
 
static void player_delay(struct battle *battle,struct player *player) {
  double n=(rand()&0xffff)/65535.0;
  player->delay=0.500+(1.0-n)+1.000*n;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left, ie bottom.
    player->who=0;
    player->y=(FBH*3)/4;
  } else { // Right, ie top.
    player->who=1;
    player->y=FBH/4;
  }
  if (player->skill>=0.900) player->soulsold=1; // bias 0x10 does it, ie a purchased goodluck.
  if (player->human=human) { // Human.
    player->blackout=1;
    player->exposed=1;
  } else { // CPU.
    player_delay(battle,player);
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x886438ff;
        player->tileid_hand=0x22;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid_hand=0x20;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid_hand=0x21;
      } break;
  }
}

/* Refresh player's sum.
 * Counts only the visible cards.
 */
 
static void player_refresh_sum(struct battle *battle,struct player *player) {
  const uint8_t *cardid=player->handv;
  int i=player->handc;
  int acec=0;
  player->sum=0;
  for (;i-->0;cardid++) {
    int rank=RANK_FROM_CARDID(*cardid);
    if (rank<=0) { // Count Aces low initially, but record how many.
      player->sum+=1;
      acec++;
    } else if (rank<10) {
      player->sum+=rank+1;
    } else {
      player->sum+=10;
    }
  }
  // Now we can add 10 for each Ace, but don't exceed 21.
  while (acec&&(player->sum<=11)) { acec--; player->sum+=10; }
}

/* Add option.
 */
 
static int blackjack_add_option(struct battle *battle,int strix) {
  if (BATTLE->optionc>=OPTION_LIMIT) return -1;
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_battle,strix);
  struct option *option=BATTLE->optionv+BATTLE->optionc++;
  option->strix=strix;
  if ((option->texid=font_render_to_texture(0,g_font,src,srcc,FBW,FBH,0xffffffff))<0) return -1;
  egg_texture_get_size(&option->w,&option->h,option->texid);
  return 0;
}

/* New.
 */
 
static int _blackjack_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  if (blackjack_add_option(battle,OPTION_STAND)<0) return -1;
  if (blackjack_add_option(battle,OPTION_DRAW)<0) return -1;
  
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  deck_shuffle(&BATTLE->deck);
  
  /* Unceremoniously deal two cards to each player.
   * These initial cards don't care whether you sold your soul; they can't bust you.
   */
  l->handv[l->handc++]=deck_draw(&BATTLE->deck);
  r->handv[r->handc++]=deck_draw(&BATTLE->deck);
  l->handv[l->handc++]=deck_draw(&BATTLE->deck);
  r->handv[r->handc++]=deck_draw(&BATTLE->deck);
  player_refresh_sum(battle,l);
  player_refresh_sum(battle,r);

  return 0;
}

/* Draw one card from the deck and return it.
 * We'll look for a card maximally favorable to the given player.
 */
 
static uint8_t blackjack_draw_cheat(struct battle *battle,struct player *player) {
  int bestp=-1;
  int bestsum=0;
  int p=BATTLE->deck.cardidp;
  for (;p<52;p++) {
    uint8_t cardid=BATTLE->deck.cardidv[p];
    int rank=RANK_FROM_CARDID(cardid);
    if ((rank==0)&&(player->sum<=10)) { // High ace and we need it. Cool.
      bestp=p;
      break;
    }
    if (rank>=10) rank=10;
    else rank+=1;
    int possible=player->sum+rank;
    if (possible>21) continue; // Nope, don't want this one.
    if (possible>bestsum) { // We want this one.
      bestp=p;
      bestsum=possible;
      if (possible==21) break; // And there won't be anything better, stop looking.
    }
  }
  if (bestp<0) return deck_draw(&BATTLE->deck); // Ooops. Well, give them whatever.
  uint8_t cardid=BATTLE->deck.cardidv[bestp];
  // Exchange these two deck slots, then advance past it.
  BATTLE->deck.cardidv[bestp]=BATTLE->deck.cardidv[BATTLE->deck.cardidp];
  BATTLE->deck.cardidv[BATTLE->deck.cardidp]=cardid;
  BATTLE->deck.cardidp++;
  return cardid;
}

/* Request another card.
 */
 
static void blackjack_hit(struct battle *battle,struct player *player) {
  if ((player->handc>=HAND_LIMIT)||(deck_remaining(&BATTLE->deck)<1)) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  if (player->soulsold) {
    player->handv[player->handc++]=blackjack_draw_cheat(battle,player);
  } else {
    player->handv[player->handc++]=deck_draw(&BATTLE->deck);
  }
  bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  if (player->handc>=HAND_LIMIT) { // Drew up to the limit -- crazy! -- Stand automatically.
    player->optionp=-1;
  }
  player_refresh_sum(battle,player);
  if (player->sum>=21) { // If we bust, or hit 21 exactly, stand automatically.
    player->optionp=-1;
  }
}

/* Finish my turn.
 */
 
static void blackjack_stand(struct battle *battle,struct player *player) {
  player->optionp=-1;
  bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Activate selection.
 */
 
static void blackjack_activate(struct battle *battle,struct player *player) {
  if ((player->optionp<0)||(player->optionp>=BATTLE->optionc)) return;
  switch (BATTLE->optionv[player->optionp].strix) {
    case OPTION_DRAW: blackjack_hit(battle,player); break;
    case OPTION_STAND: blackjack_stand(battle,player); break;
  }
}

/* Move cursor.
 */
 
static void blackjack_move(struct battle *battle,struct player *player,int d) {
  if (player->optionp<0) return;
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->optionp+=d;
  // Clamp, don't wrap. There should be just two options.
  if (player->optionp<0) player->optionp=0;
  else if (player->optionp>=BATTLE->optionc) player->optionp=BATTLE->optionc-1;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) blackjack_activate(battle,player);
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) blackjack_move(battle,player,-1);
    if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) blackjack_move(battle,player,1);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Tick down my delay. When it expires, reset it and proceed.
  if (player->delay>0.0) {
    player->delay-=elapsed;
    return;
  }
  player_delay(battle,player);
  
  // Stand on 17 or greater.
  int choice=OPTION_DRAW;
  if (player->sum>=17) choice=OPTION_STAND;
  
  // Find that option in the list. Don't assume any order.
  int noptionp=-1;
  const struct option *option=BATTLE->optionv;
  int i=0;
  for (;i<BATTLE->optionc;i++,option++) {
    if (option->strix==choice) {
      noptionp=i;
      break;
    }
  }
  if (noptionp<0) blackjack_stand(battle,player); // oops
  else if (noptionp<player->optionp) blackjack_move(battle,player,-1);
  else if (noptionp>player->optionp) blackjack_move(battle,player,1);
  else blackjack_activate(battle,player);
}

/* Finish game. Both players are exposed.
 */
 
static void blackjack_finalize(struct battle *battle,struct player *l,struct player *r) {
  l->exposed=1;
  r->exposed=1;
  if (l->sum>21) battle->outcome=(r->sum>21)?0:-1;
  else if (r->sum>21) battle->outcome=1;
  else if (l->sum>r->sum) battle->outcome=1;
  else if (l->sum<r->sum) battle->outcome=-1;
  else battle->outcome=0;
}

/* Update.
 */
 
static void _blackjack_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->optionp<0) {
      // Tick the bust clock whether we need it or not.
      if ((player->bustclock-=elapsed)<=0.0) {
        player->bustclock+=0.200;
        if (++(player->bustframe)>=2) player->bustframe=0;
      }
    } else {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
  }
  
  // Both players standing, finish the game.
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->optionp<0)&&(r->optionp<0)) {
      blackjack_finalize(battle,l,r);
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  /* Hand.
   */
  if (player->handc) {
    // In theory, a hand can have 12 cards. That would be wider than our framebuffer if we place them side by side.
    const int maxspacing=NS_sys_tilesize*3+2;
    int xspacing=(FBW-20)/player->handc;
    if (xspacing>maxspacing) xspacing=maxspacing;
    int handw=xspacing*(player->handc-1)+NS_sys_tilesize*3;
    int x=(FBW>>1)-(handw>>1);
    int y=player->y-NS_sys_tilesize*2;
    int i=0;
    for (;i<player->handc;i++,x+=xspacing) {
      uint8_t cardid=player->handv[i];
      if (!i&&!player->exposed) cardid=0xff;
      card_render(x,y,cardid);
    }
  }
  
  /* If busted, do a cheery overlay celebrating it.
   */
  if ((player->optionp<0)&&(player->sum>21)&&player->exposed) {
    int msgw=NS_sys_tilesize*4;
    int msgh=NS_sys_tilesize*2;
    int msgx=(FBW>>1)-(msgw>>1);
    int msgy=player->y-(msgh>>1);
    msgx+=NS_sys_tilesize>>1;
    msgy+=NS_sys_tilesize>>1;
    graf_set_image(g_graf,RID_image_battle_casino);
    uint32_t color=player->bustframe?0xffff00ff:0xff0000ff;
    graf_fancy(g_graf,msgx+0*NS_sys_tilesize,msgy+0*NS_sys_tilesize,0x50,0,0,NS_sys_tilesize,0,color);
    graf_fancy(g_graf,msgx+1*NS_sys_tilesize,msgy+0*NS_sys_tilesize,0x51,0,0,NS_sys_tilesize,0,color);
    graf_fancy(g_graf,msgx+2*NS_sys_tilesize,msgy+0*NS_sys_tilesize,0x52,0,0,NS_sys_tilesize,0,color);
    graf_fancy(g_graf,msgx+3*NS_sys_tilesize,msgy+0*NS_sys_tilesize,0x53,0,0,NS_sys_tilesize,0,color);
    graf_fancy(g_graf,msgx+0*NS_sys_tilesize,msgy+1*NS_sys_tilesize,0x60,0,0,NS_sys_tilesize,0,color);
    graf_fancy(g_graf,msgx+1*NS_sys_tilesize,msgy+1*NS_sys_tilesize,0x61,0,0,NS_sys_tilesize,0,color);
    graf_fancy(g_graf,msgx+2*NS_sys_tilesize,msgy+1*NS_sys_tilesize,0x62,0,0,NS_sys_tilesize,0,color);
    graf_fancy(g_graf,msgx+3*NS_sys_tilesize,msgy+1*NS_sys_tilesize,0x63,0,0,NS_sys_tilesize,0,color);
  }
  
  /* Menu.
   */
  if (player->optionp>=0) {
    const int marginl=20,marginr=5;
    const int margint=3,marginb=2;
    int menuw=0,menuh=0;
    const struct option *option=BATTLE->optionv;
    int i=BATTLE->optionc;
    for (;i-->0;option++) {
      if (option->w>menuw) menuw=option->w;
      menuh+=option->h;
    }
    menuw+=marginl+marginr;
    menuh+=margint+marginb;
    int boxx=(FBW>>1)-(menuw>>1);
    int boxy=player->y-(menuh>>1);
    graf_fill_rect(g_graf,boxx,boxy,menuw,menuh,0x000000ff);
    int x=boxx+marginl;
    int y=boxy+margint;
    for (i=0,option=BATTLE->optionv;i<BATTLE->optionc;i++,option++) {
      graf_set_input(g_graf,option->texid);
      graf_decal(g_graf,x,y,0,0,option->w,option->h);
      if (i==player->optionp) {
        graf_set_image(g_graf,RID_image_battle_casino);
        graf_tile(g_graf,boxx+(marginl>>1),y+(option->h>>1),player->tileid_hand,0);
      }
      y+=option->h;
    }
  }
}

/* Render.
 */
 
static void _blackjack_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,0x044c1dff);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_blackjack={
  .name="blackjack",
  .objlen=sizeof(struct battle_blackjack),
  .id=107,
  .strix_name=356,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz_a,
  .imageid_default=0,
  .del=_blackjack_del,
  .init=_blackjack_init,
  .update=_blackjack_update,
  .render=_blackjack_render,
};

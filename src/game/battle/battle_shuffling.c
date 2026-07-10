/* battle_shuffling.c
 * Tap A once to cut and once to merge.
 * I think we can score it based on adjacency: Each card in the final order gets 0, 1, or 2 points based on whether its neighbors changed.
 * If you cut the deck exactly in half and merge them so the top card of each half touch, that is a perfect shuffle.
 * So the initial deck is composed of sequential integers, matching their index.
 */

#include "game/bellacopia.h"

#define DECK_SIZE 40
#define DP_SLOW 4.000 /* rad/sec */
#define DP_FAST 1.500

struct battle_shuffling {
  struct battle hdr;
  uint32_t cardcolorv[DECK_SIZE];
  double timeout;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t deck[DECK_SIZE]; // Values 0..DECK_SIZE-1, starts with index as value.
    int cutp; // <0 initially. Otherwise index in (deck) of the first card of the second half.
    double p; // Radians, -pi..pi. Zero and pi are centered, ie ideal.
    double dp; // rad/sec, motion speed per difficulty. Constant. Direction is known from (p) which is circular.
    uint32_t color;
    uint8_t tileid; // Hand.
    uint8_t xform;
    int xouter,xinner,xhand; // Horizontal positions in framebuffer pixels. Center of feature.
    int finished;
    int score; // Zero until (finished). Higher is better.
    // man only:
    int input_blackout;
    // cpu only:
    double pvp;
    double cutat,mergeat;
  } playerv[2];
};

#define BATTLE ((struct battle_shuffling*)battle)

/* Delete.
 */
 
static void _shuffling_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->cutp=-1;
  player->p=(((rand()&0xffff)-32768)*M_PI)/32768.0; // Random start position, mostly just for aesthetics.
  player->dp=DP_SLOW*(1.0-player->skill)+DP_FAST*player->skill;
  player->xinner=(FBW*2)/5;
  player->xouter=FBW/5;
  player->xhand=player->xinner-40;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->xinner=FBW-player->xinner;
    player->xouter=FBW-player->xouter;
    player->xhand=FBW-player->xhand;
    player->xform=EGG_XFORM_XREV; // Everything points right naturally.
  }
  if (player->human=human) { // Human.
    player->input_blackout=1;
  } else { // CPU.
    // Select our cut and merge positions, in signed radians.
    double error=0.020+0.600*(1.0-player->skill);
    double n=(rand()&0xffff)/65535.0;
    player->cutat=error+n*error;
    n=(rand()&0xffff)/65535.0;
    player->mergeat=error+n*error;
    switch (rand()&3) {
      case 1: player->cutat=-player->cutat; break;
      case 2: player->mergeat=-player->mergeat; break;
      case 3: player->cutat=-player->cutat; player->mergeat=-player->mergeat; break;
    }
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x886438ff;
        player->tileid=0x22;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x20;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x21;
      } break;
  }
  uint8_t *dst=player->deck;
  int i=0;
  for (;i<DECK_SIZE;i++,dst++) *dst=i;
}

/* New.
 */
 
static int _shuffling_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->timeout=20.0;
  
  uint32_t *dst=BATTLE->cardcolorv;
  uint8_t luma=0xff;
  int i=0;
  for (;i<DECK_SIZE;i++,dst++,luma-=5) {
    *dst=(luma<<24)|(luma<<16)|(luma<<8)|0xff;
  }
  
  return 0;
}

/* Merge and flatten the two halves, and set (finished).
 */
 
static void player_merge(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);

  /* Do exactly what happens at render, to compare positions.
   * (mergep) will be where in the front half the back half starts interleaving.
   * May be negative.
   */
  double dy=sin(player->p)*DECK_SIZE;
  int fronty=lround(dy);
  int backy=lround(-dy);
  fronty-=player->cutp;
  backy-=(DECK_SIZE-player->cutp);
  int mergep=((backy-fronty)>>1);
  
  /* Assemble the final arrangement in a new buffer, then copy over.
   */
  uint8_t tmp[DECK_SIZE];
  int tmpc=0,frontp=0,backp=player->cutp;
  while ((mergep<0)&&(backp<DECK_SIZE)) {
    tmp[tmpc++]=player->deck[backp++];
    mergep++;
  }
  while ((mergep>0)&&(frontp<player->cutp)) {
    tmp[tmpc++]=player->deck[frontp++];
    mergep--;
  }
  while ((frontp<player->cutp)&&(backp<DECK_SIZE)) {
    tmp[tmpc++]=player->deck[frontp++];
    tmp[tmpc++]=player->deck[backp++];
  }
  while (frontp<player->cutp) {
    tmp[tmpc++]=player->deck[frontp++];
  }
  while (backp<DECK_SIZE) {
    tmp[tmpc++]=player->deck[backp++];
  }
  memcpy(player->deck,tmp,DECK_SIZE);
  
  /* Tabulate final score.
   */
  player->score=1;
  if (player->deck[0]!=0) player->score++;
  if (player->deck[DECK_SIZE-1]!=DECK_SIZE-1) player->score++;
  int i=DECK_SIZE;
  while (i-->1) {
    if (player->deck[i]!=player->deck[i-1]+1) player->score++;
  }
  
  player->finished=1;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {

  /* Cutting?
   */
  if (player->cutp<0) {
    if (player->input_blackout) {
      if (!(g.input[player->human]&EGG_BTN_SOUTH)) player->input_blackout=0;
    } else if ((g.input[player->human]&EGG_BTN_SOUTH)&&!(g.pvinput[player->human]&EGG_BTN_SOUTH)) {
      bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->cutp=(DECK_SIZE>>1)-(int)(sin(player->p)*(DECK_SIZE>>1));
      // Must leave at least one card in each half.
      if (player->cutp<1) player->cutp=1;
      else if (player->cutp>=DECK_SIZE) player->cutp=DECK_SIZE-1;
    }
    
  /* Merging?
   */
  } else {
    if ((g.input[player->human]&EGG_BTN_SOUTH)&&!(g.pvinput[player->human]&EGG_BTN_SOUTH)) {
      player_merge(battle,player);
    }
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->cutp<0) {
    if (BATTLE->timeout<18.0) { // Chill for two seconds, just to not look rushed.
      // Then cut it on the way down:
      if ((player->pvp<player->cutat)&&(player->p>=player->cutat)) {
        bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->cutp=(DECK_SIZE>>1)-(int)(sin(player->p)*(DECK_SIZE>>1));
        // Must leave at least one card in each half.
        if (player->cutp<1) player->cutp=1;
        else if (player->cutp>=DECK_SIZE) player->cutp=DECK_SIZE-1;
      }
    }
  } else if (BATTLE->timeout<15.0) { // Wait for an uncertain amount of time, 3 seconds in the fastest case.
    // Merge it on the way up:
    if ((player->pvp>=player->mergeat)&&(player->p<player->mergeat)) {
      player_merge(battle,player);
    }
  }
  player->pvp=player->p;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  player->p+=player->dp*elapsed;
  if (player->p>M_PI) player->p-=M_PI*2.0;
}

/* Update.
 */
 
static void _shuffling_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* If time runs out, anybody finished wins.
   */
  if ((BATTLE->timeout-=elapsed)<=0.0) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->finished&&r->finished) battle->outcome=0; // Not possible.
    else if (l->finished) battle->outcome=1;
    else if (r->finished) battle->outcome=-1;
    else battle->outcome=0;
    return;
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (!player->finished) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
      else player_update_cpu(battle,player,elapsed);
      if (!player->finished) player_update_common(battle,player,elapsed); // Check (finished) again in case it changed during the controller update.
    }
  }
  
  /* Check completion. Both players are allowed to finish; no perk for being first.
   * TODO (finished) isn't quite right, let there be some animated wind-down.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->finished&&r->finished) {
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
    return;
  }
}

/* Render a deck. Just a stack of horizontal lines.
 */
 
static void shuffling_render_deck(struct battle *battle,struct player *player,int x,int y,const uint8_t *cardv,int cardc) {
  graf_set_image(&g.graf,0);
  int xa=x-20;
  int xz=x+20;
  y-=cardc; // half and double
  for (;cardc-->0;y+=2,cardv++) {
    graf_line(&g.graf,xa,y,BATTLE->cardcolorv[*cardv],xz,y,BATTLE->cardcolorv[*cardv]);
  }
}

/* Render player's hand.
 */
 
static void shuffling_render_hand(struct battle *battle,struct player *player,int y) {
  graf_set_image(&g.graf,RID_image_battle_casino);
  graf_tile(&g.graf,player->xhand,y,player->tileid,player->xform);
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int midy=FBH>>1;
  
  /* If finished, show the whole deck with no animation.
   * Plus the score, above it.
   */
  if (player->finished) {
    int x=(player->xinner+player->xouter)>>1;
    shuffling_render_deck(battle,player,x,midy,player->deck,DECK_SIZE);
    graf_set_image(&g.graf,RID_image_fonttiles);
    if (player->score>=10) graf_tile(&g.graf,x-4,30,'0'+player->score/10,0);
    graf_tile(&g.graf,x+4,30,'0'+player->score%10,0);

  /* First we show the whole deck stationary, and the hand oscillating.
   */
  } else if (player->cutp<0) {
    int dy=(int)(sin(player->p)*DECK_SIZE);
    shuffling_render_hand(battle,player,midy+dy);
    shuffling_render_deck(battle,player,player->xinner,midy,player->deck,DECK_SIZE);
    
  /* Once cut, we show two decks oscillating a half-turn out of phase.
   */
  } else {
    double dy=sin(player->p)*DECK_SIZE;
    shuffling_render_deck(battle,player,player->xinner,(int)(midy+dy),player->deck,player->cutp);
    shuffling_render_deck(battle,player,player->xouter,(int)(midy-dy),player->deck+player->cutp,DECK_SIZE-player->cutp);
  }
  
  /* If time is low and we're not finished, show a "hurry!" banner.
   */
  if (!player->finished&&(BATTLE->timeout<5.0)) {
    int frame=(int)(BATTLE->timeout*6.0)%4;
    if (frame>=1) {
      graf_set_image(&g.graf,RID_image_battle_casino);
      int x=((player->xinner+player->xouter)>>1)-24;
      int y=30;
      uint8_t tileid=0x30;
      int i=4;
      for (;i-->0;x+=NS_sys_tilesize,tileid++) {
        graf_tile(&g.graf,x,y,tileid,0);
      }
    }
  }
}

/* Render.
 */
 
static void _shuffling_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x203028ff);
  graf_set_image(&g.graf,RID_image_battle_casino);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_shuffling={
  .name="shuffling",
  .objlen=sizeof(struct battle_shuffling),
  .id=NS_battle_shuffling,
  .strix_name=167,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .no_timeout=1,
  .input=battle_input_a,
  .del=_shuffling_del,
  .init=_shuffling_init,
  .update=_shuffling_update,
  .render=_shuffling_render,
};

/* battle_slapping.c
 * Tap A first when the indicated card appears.
 */

#include "game/batsup/battle_internal.h"
#include "game/batsup/cards.h"

/* The pile will appear to be infinite, but they're lying on each other.
 * Only so many will actually be recorded, including the currently in-flight card if there is one.
 */
#define CARD_LIMIT 8

struct battle_slapping {
  struct battle hdr;
  int choice;
  
  struct {
    int texid;
    int x,y,w,h;
  } msg;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid_facecard; // 3x3 tiles
    uint8_t tileid_hand; // 3x3 tiles, natural orientation for left player
    uint8_t tileid_rank; // 1 tile, replaces the "A","1","2",etc.
    int slap;
    double ready; // For CPU. If >0.0, we're slapping this one, just count down first.
    int slapp; // For CPU, index in (deck).
  } playerv[2];
  
  uint8_t target; // cardid
  
  /* (cardv) is only the stationary ones in the pile.
   * The animated card entering the pile is separate -- once it enters (cardv) it's officially in play.
   * (rx,ry) are the offset from center, should be small.
   */
  struct card {
    uint8_t cardid;
    int rx,ry;
  } cardv[CARD_LIMIT];
  int cardc;
  int cardp; // Bottom of the pile, ie the next slot to use for a new card.
  
  /* The in-flight card.
   * We pull it from the deck and advance (deckp) as soon as it becomes visible.
   * It enters (cardv) upon landing, that's also the moment it turns face-up.
   */
  struct {
    int cardid; // <0 if none
    int rx,ry; // Determine target position from this.
    double x,y; // Current center in framebuffer pixels.
    double dx,dy; // px/s
  } inflight;
  
  /* We initially populate the deck in a random order.
   * Each time (deckp) reaches 52, we reset and make up a new order.
   * That's fantastically unlikely. Maybe impossible.
   */
  struct deck deck;
};

#define BATTLE ((struct battle_slapping*)battle)

/* Delete.
 */
 
static void _slapping_del(struct battle *battle) {
  egg_texture_del(BATTLE->msg.texid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
  
    // Where is the target card?
    int tix=0;
    int i=0; for (;i<52;i++) {
      if (BATTLE->deck.cardidv[i]==BATTLE->target) {
        tix=i;
        break;
      }
    }
  
    // Will we slap the right card? If not, slap the one right after it. (tix) is always near the front of the deck.
    // Timing is not chosen until the card appears.
    const double threshlo=0.400;
    const double threshhi=0.600;
    if (player->skill<=threshlo) player->slapp=tix+1;
    else if (player->skill>=threshhi) player->slapp=tix;
    else {
      double q=threshlo+((rand()&0xffff)/65535.0)*(threshhi-threshlo);
      if (q<player->skill) player->slapp=tix;
      else player->slapp=tix+1;
    }
  
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xa151ccff;
        player->tileid_facecard=0x4a;
        player->tileid_hand=0x7a;
        player->tileid_rank=0x42;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid_facecard=0x44;
        player->tileid_hand=0x74;
        player->tileid_rank=0x40;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid_facecard=0x47;
        player->tileid_hand=0x77;
        player->tileid_rank=0x41;
      } break;
  }
}

/* Pick the target card.
 */
 
static void slapping_pick_target(struct battle *battle) {
  int p=3+rand()%10;
  BATTLE->target=BATTLE->deck.cardidv[p];
}

/* Generate message.
 */
 
static int slapping_generate_message(struct battle *battle) {
  int suit=SUIT_FROM_CARDID(BATTLE->target);
  int rank=RANK_FROM_CARDID(BATTLE->target);
  char text[256];
  struct text_insertion insv[]={
    {.mode='r',.r={.rid=RID_strings_battle,.strix=204+rank}},
    {.mode='r',.r={.rid=RID_strings_battle,.strix=200+suit}},
  };
  int textc=text_format_res(text,sizeof(text),RID_strings_battle,217,insv,2);
  if ((textc<0)||(textc>sizeof(text))) {
    text[0]='?';
    textc=1;
  }
  BATTLE->msg.texid=font_render_to_texture(0,g_font,text,textc,FBW,font_get_line_height(g_font),0xa5bd83ff);
  egg_texture_get_size(&BATTLE->msg.w,&BATTLE->msg.h,BATTLE->msg.texid);
  BATTLE->msg.x=(FBW>>1)-(BATTLE->msg.w>>1);
  BATTLE->msg.y=(FBH>>2)-(BATTLE->msg.h>>1);
  return 0;
}

/* New.
 */
 
static int _slapping_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  BATTLE->inflight.cardid=-1;
  deck_shuffle(&BATTLE->deck);
  slapping_pick_target(battle);
  slapping_generate_message(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    bm_sound_pan(RID_sound_whack,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->slap=1;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->ready>0.0) {
    if ((player->ready-=elapsed)<=0.0) {
      bm_sound_pan(RID_sound_whack,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->slap=1;
    }
  } else if (BATTLE->deck.cardidp==player->slapp+2) { // +2 rather than +1 because (deckp) advances at the draw, not the landing
    const double best=0.200;
    const double worst=0.800;
    player->ready=best+(1.0-player->skill)*(worst-best);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
}

/* Deal the next card from (deck), populate (inflight).
 */
 
static void slapping_deal(struct battle *battle) {
  if (deck_remaining(&BATTLE->deck)<1) deck_shuffle(&BATTLE->deck);
  BATTLE->inflight.cardid=deck_draw(&BATTLE->deck);
  BATTLE->inflight.rx=(rand()%11)-5;
  BATTLE->inflight.ry=(rand()%11)-5;
  BATTLE->inflight.y=NS_sys_tilesize*-2.0;
  BATTLE->inflight.x=NS_sys_tilesize*2.0+rand()%(FBW-NS_sys_tilesize*4);
  double dstx=(FBW>>1)+BATTLE->inflight.rx;
  double dsty=(FBH>>1)+BATTLE->inflight.ry;
  BATTLE->inflight.dx=dstx-BATTLE->inflight.x;
  BATTLE->inflight.dy=dsty-BATTLE->inflight.y;
  double d2=BATTLE->inflight.dx*BATTLE->inflight.dx+BATTLE->inflight.dy*BATTLE->inflight.dy;
  double distance=sqrt(d2);
  double flighttime=0.5+((rand()&0xffff)*1.5)/65535.0;
  BATTLE->inflight.dx/=flighttime;
  BATTLE->inflight.dy/=flighttime;
}

/* Advance the in-flight card.
 * Writes out to (cardv) and neuters (inflight) when it lands.
 */
 
static void slapping_update_inflight(struct battle *battle,double elapsed) {
  
  BATTLE->inflight.x+=BATTLE->inflight.dx*elapsed;
  BATTLE->inflight.y+=BATTLE->inflight.dy*elapsed;
  
  // Are we there yet?
  double dsty=(FBH>>1)+BATTLE->inflight.ry;
  if (BATTLE->inflight.y<dsty) return;
  double dstx=(FBW>>1)+BATTLE->inflight.rx;
  if ((BATTLE->inflight.dx<0.0)&&(BATTLE->inflight.x>dstx)) return;
  if ((BATTLE->inflight.dx>0.0)&&(BATTLE->inflight.x<dstx)) return;
  
  // Add to (cardv).
  bm_sound_pan(RID_sound_collect,0.0);
  BATTLE->cardv[BATTLE->cardp].cardid=BATTLE->inflight.cardid;
  BATTLE->cardv[BATTLE->cardp].rx=BATTLE->inflight.rx;
  BATTLE->cardv[BATTLE->cardp].ry=BATTLE->inflight.ry;
  if (++(BATTLE->cardp)>=CARD_LIMIT) BATTLE->cardp=0;
  if (BATTLE->cardc<CARD_LIMIT) BATTLE->cardc++;
  BATTLE->inflight.cardid=-1;
}

/* Which cardid is on top of the pile? -1 if nothing drawn yet.
 */
 
static int slapping_get_top_card(struct battle *battle) {
  if (BATTLE->cardc<1) return -1;
  int p=BATTLE->cardp-1;
  if (p<0) p=CARD_LIMIT-1;
  return BATTLE->cardv[p].cardid;
}

/* Update.
 */
 
static void _slapping_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  if (!l->slap&&!r->slap) {
    if (BATTLE->inflight.cardid<0) {
      slapping_deal(battle);
    } else {
      slapping_update_inflight(battle,elapsed);
    }
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  if (l->slap) {
    //if (r->slap) { // When they both slap, (l) is drawn first and appears to win. Make her actually win too.
    if (slapping_get_top_card(battle)==BATTLE->target) {
      battle->outcome=1;
    } else {
      battle->outcome=-1;
    }
  } else if (r->slap) {
    if (slapping_get_top_card(battle)==BATTLE->target) {
      battle->outcome=-1;
    } else {
      battle->outcome=1;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  if (player->slap) {
    int dstx=FBW>>1;
    if (player->who) dstx+=10; else dstx-=10;
    int dsty=(FBH>>1)+10;
    dstx-=NS_sys_tilesize+(NS_sys_tilesize>>1);
    dsty-=NS_sys_tilesize+(NS_sys_tilesize>>1);
    uint8_t xform=player->who?EGG_XFORM_XREV:0;
    int srcx=(player->tileid_hand&0x0f)*NS_sys_tilesize;
    int srcy=(player->tileid_hand>>4)*NS_sys_tilesize;
    graf_decal_xform(g_graf,dstx,dsty,srcx,srcy,NS_sys_tilesize*3,NS_sys_tilesize*3,xform);
  }
}

/* Render the stationary cards midscreen.
 */
 
static void slapping_render_pile(struct battle *battle) {
  const int midx=FBW>>1;
  const int midy=FBH>>1;
  const int ts=NS_sys_tilesize;
  const int ht=NS_sys_tilesize>>1;
  const int t15=ts+ht;
  graf_decal(g_graf,midx-t15,midy-2*ts,ts*13,ts*8,ts*3,ts*4);
  int i=BATTLE->cardc;
  int p=BATTLE->cardp;
  if (i<CARD_LIMIT) p=0; // Don't draw the empty slots at the end.
  struct card *card=BATTLE->cardv+p;
  for (;i-->0;p++,card++) {
    if (p>=CARD_LIMIT) {
      p=0;
      card=BATTLE->cardv;
    }
    card_render(midx-t15+card->rx,midy-2*ts+card->ry,card->cardid);
  }
}

/* Render.
 */
 
static void _slapping_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,0x0b4c1eff);
  graf_set_input(g_graf,BATTLE->msg.texid);
  graf_decal(g_graf,BATTLE->msg.x,BATTLE->msg.y,0,0,BATTLE->msg.w,BATTLE->msg.h);
  graf_set_image(g_graf,RID_image_battle_casino);
  slapping_render_pile(battle);
  if (BATTLE->inflight.cardid>=0) {
    int x=(int)BATTLE->inflight.x-NS_sys_tilesize-(NS_sys_tilesize>>1);
    int y=(int)BATTLE->inflight.y-(NS_sys_tilesize<<1);
    card_render(x,y,0xff);
  }
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_slapping={
  .name="slapping",
  .objlen=sizeof(struct battle_slapping),
  .id=40,
  .strix_name=166,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_a,
  .del=_slapping_del,
  .init=_slapping_init,
  .update=_slapping_update,
  .render=_slapping_render,
};

/* battle_slapping.c
 * Tap A first when the indicated card appears.
 */

#include "game/bellacopia.h"

/* The pile will appear to be infinite, but they're lying on each other.
 * Only so many will actually be recorded, including the currently in-flight card if there is one.
 */
#define CARD_LIMIT 8

/* Rank in 0..12 and suit in 0..3.
 * Suits 0,1 are red and 2,3 black.
 * The tiles are arranged so.
 */
#define RANK_FROM_CARDID(cardid) ((cardid)>>2)
#define SUIT_FROM_CARDID(cardid) ((cardid)&3)
#define COLOR_FROM_CARDID(cardid) (((cardid)&2)?0x000000ff:0xc00010ff)

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
  uint8_t deck[52];
  int deckp;
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
      if (BATTLE->deck[i]==BATTLE->target) {
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

/* Shuffle the deck from scratch.
 */
 
static void slapping_shuffle(struct battle *battle) {
  uint8_t initial[52];
  int i=52;
  while (i-->0) initial[i]=i;
  for (i=52;i;) {
    int p=rand()%i;
    i--;
    BATTLE->deck[i]=initial[p];
    memmove(initial+p,initial+p+1,51-p);
  }
  BATTLE->deckp=0;
  // There's a chance that the start of the deck contains cards visible in the pile.
  // Since reshuffling shouldn't happen at all, and there wouldn't be any real consequences to it, I'll just let it happen.
}

/* Pick the target card.
 */
 
static void slapping_pick_target(struct battle *battle) {
  int p=3+rand()%10;
  BATTLE->target=BATTLE->deck[p];
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
  BATTLE->msg.texid=font_render_to_texture(0,g.font,text,textc,FBW,font_get_line_height(g.font),0xa5bd83ff);
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
  slapping_shuffle(battle);
  slapping_pick_target(battle);
  slapping_generate_message(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player->slap=1;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->ready>0.0) {
    if ((player->ready-=elapsed)<=0.0) {
      player->slap=1;
    }
  } else if (BATTLE->deckp==player->slapp+2) { // +2 rather than +1 because (deckp) advances at the draw, not the landing
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
  if (BATTLE->deckp>=52) slapping_shuffle(battle);
  BATTLE->inflight.cardid=BATTLE->deck[BATTLE->deckp++];
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
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  if (l->slap) {
    if (r->slap) {
      // Both wrong or both right, either way it's a tie.
      battle->outcome=0;
    } else if (slapping_get_top_card(battle)==BATTLE->target) {
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
    graf_decal_xform(&g.graf,dstx,dsty,srcx,srcy,NS_sys_tilesize*3,NS_sys_tilesize*3,xform);
  }
}

/* Render a card face-up.
 */
 
static void slapping_render_card(struct battle *battle,int x,int y,uint8_t cardid) {
  int rank=RANK_FROM_CARDID(cardid);
  int suit=SUIT_FROM_CARDID(cardid);
  uint32_t color=COLOR_FROM_CARDID(cardid);
  graf_decal(&g.graf,x,y,NS_sys_tilesize*13,0,NS_sys_tilesize*3,NS_sys_tilesize*4);
  int x1=x+6;
  int y1=y+7;
  graf_fancy(&g.graf,x1,y1,0x00+rank,0,0,NS_sys_tilesize,color,0x808080ff);
  x1+=7;
  graf_fancy(&g.graf,x1,y1,0x10+suit,0,0,NS_sys_tilesize,color,0x808080ff);
  switch (rank) {
    /* Common ranks show the suit in a fixed pattern.
     * There are three columns always in the same places.
     * Seven rows -- the odd rows are spaced halfway. Don't mix odd and even rows.
     * Aside from Ace, this arrangement matches a very normal-looking Bicycle deck I had laying around.
     */
    #define _(col,row) { \
      int X=x+11+col*12; \
      int Y=y+20+row*5; \
      graf_fancy(&g.graf,X,Y,0x10+suit,0,0,NS_sys_tilesize,color,0x808080ff); \
    }
    case 0: _(1,3) break;
    case 1: _(1,0) _(1,6) break;
    case 2: _(1,0) _(1,3) _(1,6) break;
    case 3: _(0,0) _(2,0) _(0,6) _(2,6) break;
    case 4: _(0,0) _(2,0) _(0,6) _(2,6) _(1,3) break;
    case 5: _(0,0) _(2,0) _(0,6) _(2,6) _(0,3) _(2,3) break;
    case 6: _(0,0) _(2,0) _(0,6) _(2,6) _(0,3) _(2,3) _(1,1) break;
    case 7: _(0,0) _(2,0) _(0,6) _(2,6) _(0,3) _(2,3) _(1,1) _(1,5) break;
    case 8: _(0,0) _(0,2) _(0,4) _(0,6) _(2,0) _(2,2) _(2,4) _(2,6) _(1,3) break;
    case 9: _(0,0) _(0,2) _(0,4) _(0,6) _(2,0) _(2,2) _(2,4) _(2,6) _(1,1) _(1,5) break;
    #undef _
    // Face cards are a 3x3 decal, and don't have variations or color:
    case 10: graf_decal(&g.graf,x,y+NS_sys_tilesize,NS_sys_tilesize*4,NS_sys_tilesize,NS_sys_tilesize*3,NS_sys_tilesize*3); break;
    case 11: graf_decal(&g.graf,x,y+NS_sys_tilesize,NS_sys_tilesize*7,NS_sys_tilesize,NS_sys_tilesize*3,NS_sys_tilesize*3); break;
    case 12: graf_decal(&g.graf,x,y+NS_sys_tilesize,NS_sys_tilesize*10,NS_sys_tilesize,NS_sys_tilesize*3,NS_sys_tilesize*3); break;
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
  graf_decal(&g.graf,midx-t15,midy-2*ts,ts*13,ts*8,ts*3,ts*4);
  int i=BATTLE->cardc;
  int p=BATTLE->cardp;
  if (i<CARD_LIMIT) p=0; // Don't draw the empty slots at the end.
  struct card *card=BATTLE->cardv+p;
  for (;i-->0;p++,card++) {
    if (p>=CARD_LIMIT) {
      p=0;
      card=BATTLE->cardv;
    }
    slapping_render_card(battle,midx-t15+card->rx,midy-2*ts+card->ry,card->cardid);
  }
}

/* Render.
 */
 
static void _slapping_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x0b4c1eff);
  graf_set_input(&g.graf,BATTLE->msg.texid);
  graf_decal(&g.graf,BATTLE->msg.x,BATTLE->msg.y,0,0,BATTLE->msg.w,BATTLE->msg.h);
  graf_set_image(&g.graf,RID_image_battle_casino);
  slapping_render_pile(battle);
  if (BATTLE->inflight.cardid>=0) {
    int x=(int)BATTLE->inflight.x-NS_sys_tilesize-(NS_sys_tilesize>>1);
    int y=(int)BATTLE->inflight.y-(NS_sys_tilesize<<1);
    graf_decal(&g.graf,x,y,NS_sys_tilesize*13,NS_sys_tilesize*4,NS_sys_tilesize*3,NS_sys_tilesize*4);
  }
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_slapping={
  .name="slapping",
  .objlen=sizeof(struct battle_slapping),
  .strix_name=166,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_slapping_del,
  .init=_slapping_init,
  .update=_slapping_update,
  .render=_slapping_render,
};

/* battle_flapjack.c
 * Tap A to flip your pancake. Goal is to heat each side evenly, with minimal flip count.
 */

#include "game/bellacopia.h"

#define CAKE_LIMIT 6
#define FLIP_RATE 2.500
#define COOKRATE_WORST 0.250
#define COOKRATE_BEST  0.200

struct battle_flapjack {
  struct battle hdr;
  int choice;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    int hint; // If nonzero, we indicate the cook-side doneness visually.
    struct cake {
      int indicator; // -1,0,1 = invalid,cooking,valid. No real cake if indicator present.
      double indicatorclock;
      double doneness[2];
      double cookrate;
      int side; // 0,1, which side is cooking. The other one is visible.
      double flip; // 0..1..0
      double dflip; // (side) toggles when we go from positive to negative
    } cakev[CAKE_LIMIT];
    int cakec;
    int cakep;
    double flipclock; // Decorative, just counts down while the spatula shows tilted.
    double cookratelo,cookratehi;
    int validc,invalidc;
    double fingerclock;
    double fingertime;
  } playerv[2];
};

#define BATTLE ((struct battle_flapjack*)battle)

/* Delete.
 */
 
static void _flapjack_del(struct battle *battle) {
}

/* Reset cake.
 */
 
static void cake_reset(struct battle *battle,struct player *player,struct cake *cake) {
  cake->doneness[0]=0.0;
  cake->doneness[1]=0.0;
  cake->indicatorclock=0.0;
  cake->indicator=0;
  cake->flip=0.0;
  cake->cookrate=player->cookratelo+((rand()&0xffff)*(player->cookratehi-player->cookratelo))/65535.0;
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
    player->fingertime=0.300*(1.0-player->skill)+0.200*player->skill;
    player->fingerclock=player->fingertime;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x77573cff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
  player->cakec=3;
  player->cookratelo=COOKRATE_WORST*(1.0-player->skill)+COOKRATE_BEST*player->skill;
  player->cookratehi=player->cookratelo*1.500;
  struct cake *cake=player->cakev;
  int i=player->cakec;
  for (;i-->0;cake++) cake_reset(battle,player,cake);
}

/* New.
 */
 
static int _flapjack_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  // Show cook-side doneness indicators at up to half difficulty. (NB it's always exactly half during play, at the moment).
  if (battle->args.difficulty<=0x80) {
    BATTLE->playerv[0].hint=1;
    BATTLE->playerv[1].hint=1;
  }
  
  return 0;
}

/* Move player.
 */
 
static void player_move(struct battle *battle,struct player *player,int d) {
  if (player->cakec<1) return;
  player->cakep+=d;
  if (player->cakep<0) player->cakep=player->cakec-1;
  else if (player->cakep>=player->cakec) player->cakep=0;
  bm_sound_pan(RID_sound_uimotion,player->who?0.250:-0.250);
  player->flipclock=0.0;
}

/* Flip the focussed flapjack.
 */
 
static int rate_cake(struct battle *battle,struct player *player,struct cake *cake) {
  // If either side is burned, it's garbage.
  if (cake->doneness[0]>=2.0) return -1;
  if (cake->doneness[1]>=2.0) return -1;
  // If either side is less than golden brown, keep cooking.
  if (cake->doneness[0]<1.0) return 0;
  if (cake->doneness[1]<1.0) return 0;
  // Ah, that's-a nice-a pancake.
  return 1;
}
 
static void player_flip(struct battle *battle,struct player *player) {
  if ((player->cakep<0)||(player->cakep>=player->cakec)||(player->flipclock>0.0)) {
    bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
    return;
  }
  struct cake *cake=player->cakev+player->cakep;
  if (cake->indicator||(cake->flip>0.0)) {
    bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
    return;
  }
  cake->flip=0.001;
  cake->dflip=1.0;
  player->flipclock=0.500;
  bm_sound_pan(RID_sound_jump,player->who?0.250:-0.250);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (input!=pvinput) {
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1);
    if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1);
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_flip(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // First, can't do anything except at intervals dictated by fingerclock.
  if (player->fingerclock>0.0) {
    player->fingerclock-=elapsed;
    return;
  }
  player->fingerclock+=player->fingertime;
  
  /* If every cook-side is below half, chill.
   * Otherwise move to the cookedest cake.
   */
  int hotp=-1;
  int i=player->cakec;
  while (i-->0) {
    struct cake *cake=player->cakev+i;
    if (cake->indicator) continue;
    if (cake->flip>0.0) continue;
    double done=cake->doneness[cake->side];
    if (done<0.500) continue;
    if ((hotp>=0)&&(done<player->cakev[hotp].doneness[player->cakev[hotp].side])) continue;
    hotp=i;
  }
  if (hotp<0) return;
  if (player->cakep<hotp) {
    player_move(battle,player,1);
    return;
  }
  if (player->cakep>hotp) {
    player_move(battle,player,-1);
    return;
  }
  
  /* Flip it, or flip it not.
   * Flipping immediately is actually pretty good.
   * Makes the CPU player unnecessarily eager. So he's always beatable but also doesn't look broken or anything.
   */
  player_flip(battle,player);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->flipclock>0.0) {
    player->flipclock-=elapsed;
  }
  struct cake *cake=player->cakev;
  int i=player->cakec;
  for (;i-->0;cake++) {
    if (cake->indicatorclock>0.0) {
      if ((cake->indicatorclock-=elapsed)<=0.0) {
        cake_reset(battle,player,cake);
      }
    } else if (cake->flip>0.0) {
      if (cake->dflip>0.0) {
        if ((cake->flip+=elapsed*FLIP_RATE)>=1.0) {
          if (cake->indicator=rate_cake(battle,player,cake)) {
            if (cake->indicator<0) {
              player->invalidc++;
              bm_sound_pan(RID_sound_ouch,player->who?0.250:-0.250);
            } else {
              player->validc++;
              bm_sound_pan(RID_sound_collect,player->who?0.250:-0.250);
            }
            cake->indicatorclock=1.000;
          } else {
            cake->side^=1;
            cake->dflip=-1.0;
          }
        }
      } else {
        if ((cake->flip-=elapsed*FLIP_RATE)<=0.0) {
        }
      }
    } else {
      cake->doneness[cake->side]+=cake->cookrate*elapsed;
      // Dead-man switch: If the cook-side doneness exceeds 2, kill the cake.
      if (cake->doneness[cake->side]>=2.0) {
        player->invalidc++;
        bm_sound_pan(RID_sound_ouch,player->who?0.250:-0.250);
        cake->indicator=-1;
        cake->indicatorclock=1.000;
      }
    }
  }
}

/* Update.
 */
 
static void _flapjack_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Game ends when the fifth cake gets scored. Ties are narrowly possible.
  int cakec0=BATTLE->playerv[0].validc+BATTLE->playerv[0].invalidc;
  int cakec1=BATTLE->playerv[1].validc+BATTLE->playerv[1].invalidc;
  if ((cakec0>=5)||(cakec1>=5)) {
    int score0=BATTLE->playerv[0].validc;
    int score1=BATTLE->playerv[1].validc;
    if (score0>score1) {
      battle->outcome=1;
    } else if (score0<score1) {
      battle->outcome=-1;
    } else {
      battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static struct cake_gradient {
  double doneness;
  double r,g,b;
} cake_gradientv[]={
  {0.000,0.999,0.920,0.850},
  {1.000,0.850,0.700,0.400},
  {2.000,0.200,0.150,0.050},
};

static struct cake_gradient steam_gradientv[]={
  {0.000,0.999,0.980,0.950},
  {0.750,0.999,0.980,0.950},
  {1.000,0.850,0.700,0.400},
  {1.750,0.850,0.700,0.400},
  {2.000,0.200,0.150,0.050},
};

static uint32_t gradient(double t,const struct cake_gradient *src,int srcc) {
  const struct cake_gradient *lo=src;
  const struct cake_gradient *hi=src;
  for (;srcc-->0;src++) {
    if (src->doneness<=t) lo=src;
    hi=src;
    if (src->doneness>t) break;
  }
  int r,g,b;
  if (lo==hi) {
    r=(int)(lo->r*255.0);
    g=(int)(lo->g*255.0);
    b=(int)(lo->b*255.0);
  } else {
    t=(t-lo->doneness)/(hi->doneness-lo->doneness);
    double inv=1.0-t;
    r=(int)((lo->r*inv+hi->r*t)*255.0);
    g=(int)((lo->g*inv+hi->g*t)*255.0);
    b=(int)((lo->b*inv+hi->b*t)*255.0);
  }
  if (r<0) r=0; else if (r>0xff) r=0xff;
  if (g<0) g=0; else if (g>0xff) g=0xff;
  if (b<0) b=0; else if (b>0xff) b=0xff;
  return (r<<24)|(g<<16)|(b<<8)|0xff;
}
 
static uint32_t cake_color(double doneness) {
  return gradient(doneness,cake_gradientv,sizeof(cake_gradientv)/sizeof(struct cake_gradient));
}

static uint32_t steam_color(double doneness) {
  return gradient(doneness,steam_gradientv,sizeof(steam_gradientv)/sizeof(struct cake_gradient));
}
 
static void player_render(struct battle *battle,struct player *player) {
  int fldw=FBW>>1;
  int fldh=FBH;
  int fldx=fldw*player->who;
  int fldy=0;
  int ht=NS_sys_tilesize>>1;
  
  struct cake *cake=player->cakev;
  int i=0;
  int cakey0=fldy+(fldh>>1);
  for (;i<player->cakec;i++,cake++) {
    int cakex=fldx+((i+1)*fldw)/(player->cakec+1);
    if (i==player->cakep) { // Spatula.
      uint8_t tileid=0xc0;
      if (player->flipclock>0.0) tileid+=1;
      graf_fancy(&g.graf,cakex,cakey0+NS_sys_tilesize  ,tileid+0x00,0,0,NS_sys_tilesize,0,player->color);
      graf_fancy(&g.graf,cakex,cakey0+NS_sys_tilesize*2,tileid+0x10,0,0,NS_sys_tilesize,0,player->color);
    }
    if (cake->indicator) { // Accept or reject indicator.
      uint8_t tileid=(cake->indicator<0)?0xfb:0xfc;
      graf_fancy(&g.graf,cakex,cakey0,tileid,0,0,NS_sys_tilesize,0,0x808080ff); // Using fancy just to share the batch, could be plain tile.
    } else { // Cooking or flipping.
      uint32_t color=cake_color(cake->doneness[cake->side^1]);
      uint8_t tileid=0xc2;
      int frame=(int)(cake->flip*7.0);
      if (frame<0) frame=0; else if (frame>6) frame=6;
      tileid+=frame*2;
      int cakey=cakey0-cake->flip*10.0;
      graf_fancy(&g.graf,cakex-ht,cakey-ht,tileid+0x00,0,0,NS_sys_tilesize,0,color);
      graf_fancy(&g.graf,cakex+ht,cakey-ht,tileid+0x01,0,0,NS_sys_tilesize,0,color);
      graf_fancy(&g.graf,cakex-ht,cakey+ht,tileid+0x10,0,0,NS_sys_tilesize,0,color);
      graf_fancy(&g.graf,cakex+ht,cakey+ht,tileid+0x11,0,0,NS_sys_tilesize,0,color);
      if (player->hint&&(cake->flip<=0.0)) {
        int hintx=cakex;
        int hinty=cakey-NS_sys_tilesize-(NS_sys_tilesize>>1);
        double dn=cake->doneness[cake->side];
        uint32_t color=steam_color(cake->doneness[cake->side]);
        graf_fancy(&g.graf,hintx,hinty,0xff,(g.framec&8)?EGG_XFORM_XREV:0,0,NS_sys_tilesize,0,color);
      }
    }
  }
  
  int scorec=player->validc+player->invalidc;
  if (scorec) {
    int scorey=fldy+NS_sys_tilesize;
    int spacing=NS_sys_tilesize+1;
    int scorew=scorec*spacing-1;
    int scorex=fldx+(fldw>>1)-(scorew>>1)+(NS_sys_tilesize>>1);
    for (i=0;i<scorec;i++,scorex+=spacing) {
      uint8_t tileid=(i<player->validc)?0xfd:0xfe;
      graf_fancy(&g.graf,scorex,scorey,tileid,0,0,NS_sys_tilesize,0,0);
    }
  }
}

/* Render.
 */
 
static void _flapjack_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x403c38ff);
  graf_set_image(&g.graf,RID_image_battle_fractia);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_flapjack={
  .name="flapjack",
  .objlen=sizeof(struct battle_flapjack),
  .strix_name=150,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_flapjack_del,
  .init=_flapjack_init,
  .update=_flapjack_update,
  .render=_flapjack_render,
};

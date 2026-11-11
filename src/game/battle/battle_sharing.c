/* battle_sharing.c
 */

#include "game/batsup/battle_internal.h"

#define CIVILIAN_LIMIT 32

struct battle_sharing {
  struct battle hdr;
  
  struct civilian {
    int x,y;
    uint8_t tileid;
    uint8_t xform;
    int berry; // -1,0,1 = none,left,right
  } civilianv[CIVILIAN_LIMIT];
  int civilianc;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double animclock;
    int animframe;
    double x,y;
    int indx,indy;
    double walkspeed;
    int score;
    struct civilian *cputarget; // Who I'm after.
    int cpudx,cpudy; // Direction I'm travelling, for jitter mitigation.
    double cpupause;
    double cpupauselo,cpupausehi;
  } playerv[2];
};

#define BATTLE ((struct battle_sharing*)battle)

/* Delete.
 */
 
static void _sharing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=FBW*0.333;
  player->y=FBH*0.5;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  player->walkspeed=70.0*(1.0-player->skill)+130.0*player->skill;
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cpupause=0.500;
    player->cpupausehi=0.400*(1.0-player->skill)+0.200*player->skill;
    player->cpupauselo=player->cpupausehi*0.500;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x653713ff;
        player->tileid=0xc0;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x56;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x58;
      } break;
  }
}

/* Initialize civilian.
 * <0 if we give up trying to find vacant space.
 * (civilian) must not be counted in (BATTLE->civilianv) yet.
 */
 
static int sharing_init_civilian(struct battle *battle,struct civilian *civilian) {
  const int margin=8;
  const int topmargin=20; // Extra room on top for the scoreboard.
  const int mindist=12;
  const int mindist2=mindist*mindist;
  const int pdist=30;
  const int pdist2=pdist*pdist;
  const uint8_t tileidv[]={
    0x08,0x09,0x0a,0x0b,0x23,0x24,0x25,0x26,
    0x2a,0x2b,0x2c,0x2d,0x2e,0x2f,
    0x30,0x31,0x32,0x33,0x5f,
  };
  int panic=40;
  while (panic-->0) {
    // Random location, and if anyone else is too close, try again.
    civilian->x=margin+rand()%(FBW-margin*2);
    civilian->y=topmargin+rand()%(FBH-margin-topmargin);
    int ok=1;
    const struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      int dx=player->x-civilian->x;
      int dy=player->y-civilian->y;
      int d2=dx*dx+dy*dy;
      if (d2<pdist2) {
        ok=0;
        break;
      }
    }
    if (ok) {
      const struct civilian *other=BATTLE->civilianv;
      for (i=BATTLE->civilianc;i-->0;other++) {
        int dx=other->x-civilian->x;
        int dy=other->y-civilian->y;
        int d2=dx*dx+dy*dy;
        if (d2<mindist2) {
          ok=0;
          break;
        }
      }
    }
    if (ok) {
      civilian->xform=(rand()&EGG_XFORM_XREV);
      civilian->berry=-1;
      civilian->tileid=tileidv[rand()%sizeof(tileidv)];
      return 0;
    }
  }
  return -1;
}

/* Comparator for civilian sort. Render order, ie top to bottom.
 */
 
static int sharing_cmp_civilian(const void *a,const void *b) {
  const struct civilian *A=a,*B=b;
  if (A->y<B->y) return -1;
  if (A->y>B->y) return 1;
  return 0;
}

/* New.
 */
 
static int _sharing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  while (BATTLE->civilianc<CIVILIAN_LIMIT) {
    struct civilian *civ=BATTLE->civilianv+BATTLE->civilianc;
    if (sharing_init_civilian(battle,civ)<0) break;
    BATTLE->civilianc++;
  }
  qsort(BATTLE->civilianv,BATTLE->civilianc,sizeof(struct civilian),sharing_cmp_civilian);
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0;
  }
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If we have a pause, pay it out and stand still.
   */
  if (player->cpupause>0.0) {
    player->cpupause-=elapsed;
    player->indx=0;
    player->indy=0;
    return;
  }

  /* If I don't have a target right now, pick the nearest unberried civilian.
   * But then begin a pause.
   * We'll retain the target selection through the whole pause, increases odds that the human can beat us to it.
   */
  if (!player->cputarget) {
    player->indx=0;
    player->indy=0;
    struct player *other=BATTLE->playerv+(player->who?0:1);
    int bestd2=999999;
    struct civilian *civ=BATTLE->civilianv;
    int i=BATTLE->civilianc;
    for (;i-->0;civ++) {
      if (civ->berry>=0) continue;
      // Don't chase the same civilian as the other CPU player.
      // Without this check, Princess often falls into a trap where she's just tailing the Bear.
      if (civ==other->cputarget) continue;
      int dx=civ->x-(int)player->x;
      int dy=civ->y-(int)player->y;
      int d2=dx*dx+dy*dy;
      if (d2<bestd2) {
        player->cputarget=civ;
        bestd2=d2;
      }
    }
    if (!player->cputarget) { // Shouldn't be possible but if so, just spin.
      player->cpupause=999.999;
      return;
    }
    player->cpudx=player->cputarget->x-player->x;
    player->cpudy=player->cputarget->y-player->y;
    double n=(rand()&0xffff)/65535.0;
    player->cpupause=player->cpupauselo*(1.0-n)+player->cpupausehi*n;
    return;
  }
  
  /* Inputs stay on until we cross the target in that direction.
   */
  if (player->cpudx<0) {
    if (player->x<=player->cputarget->x) player->indx=0;
    else player->indx=-1;
  } else if (player->cpudx>0) {
    if (player->x>=player->cputarget->x) player->indx=0;
    else player->indx=1;
  } else player->indx=0;
  if (player->cpudy<0) {
    if (player->y<=player->cputarget->y) player->indy=0;
    else player->indy=-1;
  } else if (player->cpudy>0) {
    if (player->y>=player->cputarget->y) player->indy=0;
    else player->indy=1;
  } else player->indy=0;
  
  /* If we're very close to the target, drop it.
   * We don't check whether a berry was actually delivered.
   * That gives humans a mild edge: CPU players may continue a journey to the next civilian when that one is already lost.
   */
  int dx=player->cputarget->x-(int)player->x;
  int dy=player->cputarget->y-(int)player->y;
  if (dx*dx+dy*dy<=50) {
    player->cputarget=0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if ((player->animclock-=elapsed)<=0.0) {
    player->animclock+=0.150;
    if (++(player->animframe)>=2) player->animframe=0;
  }
  
  if (player->indx||player->indy) {
    player->x+=player->indx*player->walkspeed*elapsed;
    player->y+=player->indy*player->walkspeed*elapsed;
    const double margin=6.0;
    if (player->x<margin) player->x=margin;
    else if (player->x>FBW-margin) player->x=FBW-margin;
    if (player->y<margin) player->y=margin;
    else if (player->y>FBH-margin) player->y=FBH-margin;
    if (player->indx>0) player->xform=0;
    else if (player->indx<0) player->xform=EGG_XFORM_XREV;
    
    // Deliver a berry?
    const int radius=10;
    const int r2=radius*radius;
    struct civilian *civ=BATTLE->civilianv;
    int i=BATTLE->civilianc;
    for (;i-->0;civ++) {
      if (civ->berry>=0) continue;
      int dx=civ->x-(int)player->x;
      int dy=civ->y-(int)player->y;
      if (dx*dx+dy*dy<r2) {
        player->score++;
        civ->berry=player->who;
        bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
      }
    }
  }
}

/* Update.
 */
 
static void _sharing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  /* Battle ends when one player has given more than half of the possible, or everybody has a berry.
   * Ties are deliberately possible, in the spirit of sharing.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    int threshold=(BATTLE->civilianc>>1)+1;
    if ((l->score>=threshold)||(r->score>=threshold)||(l->score+r->score>=BATTLE->civilianc)) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=(int)player->y;
  uint8_t tileid=player->tileid+player->animframe;
  graf_tile(g_graf,x,y,tileid,player->xform);
}

/* Render.
 */
 
static void _sharing_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(g_graf,RID_image_meadow_sprites);
  
  /* Draw civilians in order, but interleave players at the right point.
   */
  struct player *playerv[2];
  if (l->y<r->y) {
    playerv[0]=l;
    playerv[1]=r;
  } else {
    playerv[0]=r;
    playerv[1]=l;
  }
  int pdone=0;
  const struct civilian *civilian=BATTLE->civilianv;
  int i=BATTLE->civilianc;
  for (;i-->0;civilian++) {
    if ((pdone<2)&&(playerv[pdone]->y<=civilian->y)) {
      player_render(battle,playerv[pdone++]);
    }
    graf_tile(g_graf,civilian->x,civilian->y,civilian->tileid,civilian->xform);
    if (civilian->berry>=0) {
      int bx=civilian->x;
      if (civilian->xform) bx-=4; else bx+=4;
      graf_tile(g_graf,bx,civilian->y,0xf3,civilian->xform);
    }
  }
  for (;pdone<2;pdone++) player_render(battle,playerv[pdone]);
  
  /* Scoreboard.
   */
  graf_tile(g_graf,FBW>>1,6,0xf5,0);
  const int xspacing=9;
  int x=(FBW>>1)-((BATTLE->civilianc*xspacing)>>1)+(xspacing>>1);
  for (i=0;i<BATTLE->civilianc;i++,x+=xspacing) {
    uint32_t color=0x808080ff;
    if (i<l->score) color=l->color;
    else if (i>=BATTLE->civilianc-r->score) color=r->color;
    graf_fancy(g_graf,x,6,0xf4,0,0,NS_sys_tilesize,0,color);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_sharing={
  .name="sharing",
  .objlen=sizeof(struct battle_sharing),
  .id=125,
  .strix_name=413,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad,
  .imageid_default=RID_image_meadow,
  .del=_sharing_del,
  .init=_sharing_init,
  .update=_sharing_update,
  .render=_sharing_render,
};

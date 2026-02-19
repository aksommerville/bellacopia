/* battle_stacking.c
 * L/R to walk and catch falling pancakes on your two plates. Fall if too lopsided.
 */

#include "game/bellacopia.h"

#define HEADY 108
#define PLATEY 120
#define LOSTY 130
#define GROUNDY 140
#define CAKE_LIMIT 64
#define CAKE_TIME 0.400
#define GRAVITY_MIN 70.0
#define GRAVITY_MAX 90.0
#define WALKSPEED_WORST 70.0
#define WALKSPEED_BEST 100.0

struct battle_stacking {
  struct battle hdr;
  int done; // Nonzero if we've stopped spawning.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid; // 1x2 tiles. +0x10=feet
    uint8_t xform;
    double x; // Framebuffer pixels.
    int indx;
    double walkspeed;
    int fall; // Nonzero if fallen. -1,1=left,right
    struct arm {
      uint8_t tileid;
      uint8_t xform;
      int cakec;
    } armv[2]; // [l,r], and they don't swap when you change direction or anything.
    int cakec; // Cakes on the noggin, purely decorative.
    int headreqc; // Depending on skill, CPU player will catch a certain number of cakes on her head deliberately.
  } playerv[2];
  
  struct cake {
    double x,y;
    double dx,dy;
    double ddy;
    int landed; // Cakes on the ground stay live but only for decorative purposes, and get reused if needed. Those collected by a player are removed.
  } cakev[CAKE_LIMIT];
  int cakec;
  double cakeclock;
  int rainside;
};

#define BATTLE ((struct battle_stacking*)battle)

/* Delete.
 */
 
static void _stacking_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW/3;
  } else { // Right.
    player->who=1;
    player->x=(FBW*2)/3;
    player->xform=EGG_XFORM_XREV;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    if (player->skill<0.750) {
      player->headreqc=(int)((0.750-player->skill)*4.0+1.0);
    }
  }
  switch (face) {
    case NS_face_monster: {
        player->tileid=0x70;
      } break;
    case NS_face_dot: {
        player->tileid=0x30;
      } break;
    case NS_face_princess: {
        player->tileid=0x50;
      } break;
  }
  player->walkspeed=WALKSPEED_WORST*(1.0-player->skill)+WALKSPEED_BEST*player->skill;
}

/* New.
 */
 
static int _stacking_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->rainside=rand()&1;
  return 0;
}

/* Spawn a new cake.
 */
 
static struct cake *spawn_cake(struct battle *battle) {
  struct cake *cake=0;
  if (BATTLE->cakec<CAKE_LIMIT) {
    cake=BATTLE->cakev+BATTLE->cakec++;
  } else {
    struct cake *q=BATTLE->cakev;
    int i=CAKE_LIMIT;
    for (;i-->0;q++) {
      if (q->landed) {
        cake=q;
        break;
      }
    }
  }
  if (!cake) return 0;
  
  // Alternate falling on the left and right sides, for fairness.
  cake->x=rand()%((FBW>>1)-10);
  if (BATTLE->rainside^=1) {
    cake->x+=FBW>>1;
  } else {
    cake->x+=10;
  }
  cake->y=-8.0;
  cake->landed=0;
  cake->dy=GRAVITY_MIN+((rand()&0xffff)*(GRAVITY_MAX-GRAVITY_MIN))/65535.0;
  cake->dx=0.0;
  cake->ddy=0.0;
  return cake;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Count my armcakes. If there's a discrepancy of 2 or more, don't try to fill the fuller arm.
   */
  int lc=player->armv[0].cakec;
  int rc=player->armv[1].cakec;
  int lok=1,rok=1;
  if (lc>=rc+2) lok=0;
  else if (rc>=lc+2) rok=0;
  double lx=player->x-NS_sys_tilesize*1.5;
  double rx=player->x+NS_sys_tilesize*1.5;
  double y=GROUNDY-NS_sys_tilesize;
  
  struct player *other=BATTLE->playerv;
  if (player==other) other++;
  const double conflict_margin=NS_sys_tilesize*2.0;
  
  /* Check each falling cake.
   * Find the one with the shortest square distance to an available arm.
   */
  struct cake *nearest=0;
  double nearestd2,nearestdx;
  struct cake *cake=BATTLE->cakev;
  int i=BATTLE->cakec;
  for (;i-->0;cake++) {
    if (cake->landed) continue;
    if (cake->y>LOSTY) continue;
    
    // If it's unreachable due to being blocked by the other player, ignore it.
    if ((other->x<player->x)&&(cake->x<player->x)&&(cake->x<other->x+conflict_margin)) continue;
    if ((other->x>player->x)&&(cake->x>player->x)&&(cake->x>other->x-conflict_margin)) continue;
    
    double dy=cake->y-y;
    if (lok) {
      double dx=cake->x-lx;
      double d2=dx*dx+dy*dy;
      if (!nearest||(d2<nearestd2)) {
        nearest=cake;
        nearestd2=d2;
        nearestdx=dx;
      }
    }
    if (rok) { // yes check both arms when both eligible
      double dx=cake->x-rx;
      double d2=dx*dx+dy*dy;
      if (!nearest||(d2<nearestd2)) {
        nearest=cake;
        nearestd2=d2;
        nearestdx=dx;
      }
    }
  }
  
  /* No candidate cake? Chill.
   */
  if (!nearest) {
    player->indx=0;
    return;
  }
  
  /* Catch a fixed count on my head before playing for real.
   */
  if (player->cakec<player->headreqc) {
    if (nearest->x<player->x-2.0) player->indx=-1;
    else if (nearest->x>player->x+2.0) player->indx=1;
    else player->indx=0;
    return;
  }
  
  /* Get under it.
   */
  if (nearestdx<-2.0) player->indx=-1;
  else if (nearestdx>2.0) player->indx=1;
  else player->indx=0;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (player->indx) {
    player->xform=(player->indx<0)?EGG_XFORM_XREV:0;
    struct player *other=BATTLE->playerv;
    if (player==other) other++;
    double xlo=0.0,xhi=FBW;
    if (other->x<player->x) xlo=other->x+NS_sys_tilesize*4.0;
    else xhi=other->x-NS_sys_tilesize*4.0;
    player->x+=player->indx*player->walkspeed*elapsed;
    if (player->x<xlo) player->x=xlo;
    else if (player->x>xhi) player->x=xhi;
  }
  
  // Once any arm pile reaches 4 pancakes, we stop spawning them. The head pile can go as high as you want.
  if ((player->armv[0].cakec>=4)||(player->armv[1].cakec>=4)) BATTLE->done=1;
  // Or if one arm is at least 3 deeper than the other, lose.
  if (player->armv[0].cakec>=player->armv[1].cakec+3) {
    player->fall=-1;
  } else if (player->armv[1].cakec>=player->armv[0].cakec+3) {
    player->fall=1;
  }
  if (player->fall) {
    BATTLE->done=1;
    int cakec=player->cakec+player->armv[0].cakec+player->armv[1].cakec;
    while (cakec-->0) {
      struct cake *cake=spawn_cake(battle);
      cake->x=player->x;
      cake->y=GROUNDY-NS_sys_tilesize;
      cake->dx=(((rand()&0xffff)-0x8000)*80.0)/65535.0;
      cake->dy=-70.0;
      cake->ddy=200.0;
    }
  }
}

/* If this cake connects horizontally with either player's head, commit it and return nonzero.
 */
 
static int check_heads(struct battle *battle,struct cake *cake) {
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->fall) continue;
    double dx=cake->x-player->x;
    if ((dx>=-8.0)&&(dx<8.0)) {
      player->cakec++;
      bm_sound(RID_sound_collect);
      return 1;
    }
  }
  return 0;
}
 
/* If this cake connects horizontally with any plate, commit it adn return nonzero.
 * Cakes rest 22 pixels from player->x.
 */
 
static int check_plates(struct battle *battle,struct cake *cake) {
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->fall) continue;
    double dx=cake->x-player->x;
    if ((dx>=-28.0)&&(dx<=-16.0)) {
      player->armv[0].cakec++;
      bm_sound(RID_sound_collect);
      return 1;
    }
    if ((dx>=16.0)&&(dx<=28.0)) {
      player->armv[1].cakec++;
      bm_sound(RID_sound_collect);
      return 1;
    }
  }
  return 0;
}

/* Update.
 */
 
static void _stacking_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->fall) continue;
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  int rainc=0;
  struct cake *cake=BATTLE->cakev+BATTLE->cakec-1;
  for (i=BATTLE->cakec;i-->0;cake--) {
    if (cake->landed) continue;
    rainc++;
    if ((cake->ddy>0.0)&&(cake->dy<GRAVITY_MAX)) {
      cake->dy+=cake->ddy*elapsed;
    }
    cake->x+=cake->dx*elapsed;
    cake->y+=cake->dy*elapsed;
    int rm=0;
    if (cake->y>=GROUNDY) {
      cake->landed=1;
      cake->y=GROUNDY-1+(rand()%3);
    } else if (cake->y>=LOSTY) {
    } else if (cake->y>=PLATEY) {
      rm=check_plates(battle,cake);
    } else if (cake->y>=HEADY) {
      rm=check_heads(battle,cake);
    }
    if (rm) {
      BATTLE->cakec--;
      memmove(cake,cake+1,sizeof(struct cake)*(BATTLE->cakec-i));
    }
  }
  
  if (!BATTLE->done) {
    if ((BATTLE->cakeclock-=elapsed)<=0.0) {
      BATTLE->cakeclock+=CAKE_TIME;
      spawn_cake(battle);
    }
  } else if (!rainc) {
    // Done spawning, and the last cakes have landed. End the game.
    struct player *l=BATTLE->playerv;
    struct player *r=BATTLE->playerv+1;
    int lc=l->armv[0].cakec+l->armv[1].cakec;
    int rc=r->armv[0].cakec+r->armv[1].cakec;
    // If they both fell, it's a tie, regardless of how many they collected.
    if (l->fall&&r->fall) battle->outcome=0;
    // Or if just one fell, that one loses.
    else if (l->fall) battle->outcome=-1;
    else if (r->fall) battle->outcome=1;
    // Normal play: Most pancakes in the arms wins.
    else if (lc>rc) battle->outcome=1;
    else if (lc<rc) battle->outcome=-1;
    // Could break ties with the head pile, but I'm deliberately not doing that. We use the CPU player's head for nerfing.
    // And finally, ties are entirely possible.
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int ht=NS_sys_tilesize>>1;
  int y=GROUNDY-NS_sys_tilesize-(NS_sys_tilesize>>1)+1; // Middle of head.
  int x=(int)player->x;
  
  // Fallen is a different thing.
  if (player->fall<0) {
    y=GROUNDY-6;
    graf_tile(&g.graf,x-ht,y,player->tileid,EGG_XFORM_SWAP|EGG_XFORM_XREV);
    graf_tile(&g.graf,x+ht,y,player->tileid+0x10,EGG_XFORM_SWAP|EGG_XFORM_XREV);
    return;
  }
  if (player->fall>0) {
    y=GROUNDY-6;
    graf_tile(&g.graf,x-ht,y,player->tileid+0x10,EGG_XFORM_SWAP|EGG_XFORM_YREV|EGG_XFORM_XREV);
    graf_tile(&g.graf,x+ht,y,player->tileid,EGG_XFORM_SWAP|EGG_XFORM_YREV|EGG_XFORM_XREV);
    return;
  }
  
  // If one stack is 2 cakes deeper than the other, shout a warning.
  int lc=player->armv[0].cakec;
  int rc=player->armv[1].cakec;
  if (lc>=rc+2) graf_tile(&g.graf,x-NS_sys_tilesize,y-ht,0xb8,EGG_XFORM_XREV);
  else if (rc>=lc+2) graf_tile(&g.graf,x+NS_sys_tilesize,y-ht,0xb8,0);
  
  // Arms first.
  int ly=y+ht+lc;
  int ry=y+ht+rc;
  graf_tile(&g.graf,x+ht,ry,player->tileid+1,0);
  graf_tile(&g.graf,x+ht+NS_sys_tilesize,ry,player->tileid+2,0);
  graf_tile(&g.graf,x-ht,ly,player->tileid+1,EGG_XFORM_XREV);
  graf_tile(&g.graf,x-ht-NS_sys_tilesize,ly,player->tileid+2,EGG_XFORM_XREV);
  
  // Then trunk.
  graf_tile(&g.graf,x,y,player->tileid,player->xform);
  graf_tile(&g.graf,x,y+NS_sys_tilesize,player->tileid+0x10,player->xform);
  
  // Then noggin-cakes.
  int i=player->cakec;
  while (i-->0) {
    graf_tile(&g.graf,x,y,0xb7,player->xform);
    y-=2;
  }
  
  // Then plate-cakes.
  for (i=player->armv[0].cakec;i-->0;) {
    graf_tile(&g.graf,x-ht-NS_sys_tilesize+2,ly,0xb6,0);
    ly-=2;
  }
  for (i=player->armv[1].cakec;i-->0;) {
    graf_tile(&g.graf,x+ht+NS_sys_tilesize-2,ry,0xb6,0);
    ry-=2;
  }
}

/* Render.
 */
 
static void _stacking_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,GROUNDY,0x80a0c0ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,FBH-GROUNDY,0x106020ff);
  graf_fill_rect(&g.graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(&g.graf,RID_image_battle_fractia);
  
  struct cake *cake=BATTLE->cakev;
  int i=BATTLE->cakec;
  for (;i-->0;cake++) {
    uint8_t tileid=0xfd;
    if (cake->landed) tileid=0xb6;
    graf_tile(&g.graf,(int)cake->x,(int)cake->y,tileid,0);
  }
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_stacking={
  .name="stacking",
  .objlen=sizeof(struct battle_stacking),
  .strix_name=152,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_stacking_del,
  .init=_stacking_init,
  .update=_stacking_update,
  .render=_stacking_render,
};

/* battle_lawnmowing.c
 * Moving constantly, turn to cut the grass and don't touch cells you've already visited.
 * The CPU player is awful, especially towards the end when the grass gets sparse.
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

#define SPRITEID_LEFT 1
#define SPRITEID_RIGHT 2

#define SPEED_LO 4.0 /* m/s */
#define SPEED_HI 8.0
#define SLIDE_SPEED 1.0 /* m/s */

struct battle_lawnmowing {
  struct battle hdr;
  struct batsup_world *world;
  double animclock;
  int animframe;
  int grassc;
  double timer;
};

#define BATTLE ((struct battle_lawnmowing*)battle)

/* Player sprite.
 */
 
struct sprite_player {
  struct batsup_sprite hdr;
  int human; // 0,1,2
  double skill;
  double animclock;
  int animframe;
  uint32_t color;
  int qx,qy; // Quantized effective position.
  int facedx,facedy;
  double speed;
  int score;
  // CPU only:
  int decisionx,decisiony; // Last quantized position examined by AI.
  double celltime; // How long have we been at (qx,qy)? Gathered for all players, but only CPU uses.
};

static void player_update_man(struct batsup_sprite *sprite,double elapsed) {
  struct battle *battle=sprite->world->battle;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  switch (g.input[SPRITE->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_LEFT: SPRITE->facedx=-1; SPRITE->facedy=0; break;
    case EGG_BTN_RIGHT: SPRITE->facedx=1; SPRITE->facedy=0; break;
    case EGG_BTN_UP: SPRITE->facedx=0; SPRITE->facedy=-1; break;
    case EGG_BTN_DOWN: SPRITE->facedx=0; SPRITE->facedy=1; break;
  }
}

static int player_weigh_move(struct batsup_sprite *sprite,int dx,int dy) {
  struct battle *battle=sprite->world->battle;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  int x=SPRITE->qx+dx;
  int y=SPRITE->qy+dy;
  
  // If it's OOB the answer is No. (this won't actually happen; caller filters OOBs out).
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return 0;
  
  // If the cell is solid, the answer is No.
  uint8_t tileid=BATTLE->world->map->v[y*NS_sys_mapw+x];
  switch (BATTLE->world->map->physics[tileid]) {
    case NS_physics_solid: return 0;
  }
  // We don't care about other solid sprites; they won't stay long.
  
  // Bias toward tall grass over cells already cut.
  int weight=1;
  if ((tileid>=0x08)&&(tileid<0x10)) weight+=5;
  
  // Bias toward the direction we're already facing.
  if ((dx==SPRITE->facedx)&&(dy==SPRITE->facedy)) weight+=5;
  
  return weight;
}

static void player_update_cpu(struct batsup_sprite *sprite,double elapsed) {
  struct battle *battle=sprite->world->battle;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  
  /* If we spend too long at one cell, it must be that the other player is blocking us.
   * Panic, and pick a new move at random.
   */
  if (SPRITE->celltime>=1.000) {
    SPRITE->celltime=0.0;
    SPRITE->facedx*=-1;
    SPRITE->facedy*=-1;
    return;
  }
  
  // If my q position is outside the map, do nothing and wait for it to catch up. This will happen on the first update.
  if ((SPRITE->qx<0)||(SPRITE->qy<0)||(SPRITE->qx>=NS_sys_mapw)||(SPRITE->qy>=NS_sys_maph)) return;
  
  /* We'll only make decisions when we're within a certain distance of a cell's center.
   * And then we use (decisionx,decisiony) to track it, and only decide a move once per visit.
   */
  if ((SPRITE->qx==SPRITE->decisionx)&&(SPRITE->qy==SPRITE->decisiony)) return;
  const double radius=0.200;
  double targetx=SPRITE->qx+0.5;
  double targety=SPRITE->qy+0.5;
  double dx=sprite->x-targetx;
  double dy=sprite->y-targety;
  if ((dx<-radius)||(dx>radius)||(dy<-radius)||(dy>radius)) return;
  
  /* OK, we're near the center of a fresh cell.
   * Gather the 4 possible moves, and select one from weighted random.
   */
  SPRITE->decisionx=SPRITE->qx;
  SPRITE->decisiony=SPRITE->qy;
  struct candidate {
    int dx,dy,weight;
  } candidatev[4];
  int candidatec=0;
  if (SPRITE->qx>0) candidatev[candidatec++]=(struct candidate){-1,0};
  if (SPRITE->qx<NS_sys_mapw-1) candidatev[candidatec++]=(struct candidate){1,0};
  if (SPRITE->qy>0) candidatev[candidatec++]=(struct candidate){0,1};
  if (SPRITE->qy<NS_sys_maph-1) candidatev[candidatec++]=(struct candidate){0,-1};
  struct candidate *candidate=candidatev;
  int i=candidatec,wsum=0;
  for (;i-->0;candidate++) {
    candidate->weight=player_weigh_move(sprite,candidate->dx,candidate->dy);
    wsum+=candidate->weight;
  }
  if (wsum<1) return;
  int choice=rand()%wsum;
  for (candidate=candidatev,i=candidatec;i-->0;candidate++) {
    choice-=candidate->weight;
    if (choice<0) {
      SPRITE->facedx=candidate->dx;
      SPRITE->facedy=candidate->dy;
      return;
    }
  }
}

static void player_update(struct batsup_sprite *sprite,double elapsed) {
  struct battle *battle=sprite->world->battle;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  
  // Animate always.
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.080;
    if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
  }
  
  // After game-over, that's it.
  if (battle->outcome>-2) return;
  
  // Invoke controller.
  if (SPRITE->human) player_update_man(sprite,elapsed);
  else player_update_cpu(sprite,elapsed);
  
  // Move always, according to (facedx,facedy).
  batsup_sprite_move(sprite,SPRITE->facedx*SPRITE->speed*elapsed,SPRITE->facedy*SPRITE->speed*elapsed);
  
  // Slide toward the cell's center on the off axis.
  if (SPRITE->facedx) {
    double target=SPRITE->qy+0.5;
    if (sprite->y<target) {
      batsup_sprite_move(sprite,0.0,SLIDE_SPEED*elapsed);
      if (sprite->y>=target) sprite->y=target;
    } else if (sprite->y>target) {
      batsup_sprite_move(sprite,0.0,-SLIDE_SPEED*elapsed);
      if (sprite->y<=target) sprite->y=target;
    }
  } else {
    double target=SPRITE->qx+0.5;
    if (sprite->x<target) {
      batsup_sprite_move(sprite,SLIDE_SPEED*elapsed,0.0);
      if (sprite->x>=target) sprite->x=target;
    } else if (sprite->x>target) {
      batsup_sprite_move(sprite,-SLIDE_SPEED*elapsed,0.0);
      if (sprite->x<=target) sprite->x=target;
    }
  }
  
  // Check the new quantized position.
  int nqx=(int)sprite->x;
  int nqy=(int)sprite->y;
  if ((nqx!=SPRITE->qx)||(nqy!=SPRITE->qy)) {
    SPRITE->celltime=0.0;
    SPRITE->qx=nqx;
    SPRITE->qy=nqy;
    if ((nqx>=0)&&(nqy>=0)&&(nqx<NS_sys_mapw)&&(nqy<NS_sys_maph)) {
      uint8_t tileid=BATTLE->world->map->v[nqy*NS_sys_mapw+nqx];
      if ((tileid>=0x08)&&(tileid<0x10)) { // Tall grass.
        BATTLE->world->map->v[nqy*NS_sys_mapw+nqx]=rand()&7; // Cut grass.
        SPRITE->score++;
        bm_sound_pan(RID_sound_collect,(sprite->id==SPRITEID_RIGHT)?0.250:-0.250);
      }
    }
  } else {
    SPRITE->celltime+=elapsed;
  }
}

static void player_render(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  uint8_t tileid=0x10+SPRITE->animframe;
  
  /* Natural orientation is right.
   * Only use same-chirality transforms (ie even number of bits); the animation produces a rotation.
   */
  uint8_t xform=0;
  if (SPRITE->facedx<0) xform=EGG_XFORM_XREV|EGG_XFORM_YREV;
  else if (SPRITE->facedx>0) xform=0;
  else if (SPRITE->facedy<0) xform=EGG_XFORM_SWAP|EGG_XFORM_XREV;
  else xform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
  
  graf_fancy(&g.graf,dstx,dsty,tileid,xform,0,NS_sys_tilesize,0,SPRITE->color);
}

static struct batsup_sprite *player_init(struct battle *battle,int id,int ctl,int face,double skill) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,id,sizeof(struct sprite_player));
  if (!sprite) return 0;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  sprite->update=player_update;
  sprite->render=player_render;
  sprite->solid=1;
  
  SPRITE->qy=NS_sys_maph>>1;
  if (id==SPRITEID_LEFT) {
    SPRITE->qx=NS_sys_mapw>>2;
    SPRITE->facedx=1;
  } else {
    SPRITE->qx=(NS_sys_mapw*3)>>2;
    SPRITE->facedx=-1;
  }
  sprite->x=SPRITE->qx+0.5;
  sprite->y=SPRITE->qy+0.5;
  SPRITE->qx=SPRITE->qy=-1; // Actually, start the quantized position invalid so we react to the initial cell.
  SPRITE->human=ctl;
  SPRITE->skill=skill;
  SPRITE->speed=SPEED_LO*(1.0-SPRITE->skill)+SPEED_HI*SPRITE->skill;
  
  switch (face) {
    case NS_face_dot: {
        SPRITE->color=0x411775ff;
      } break;
    case NS_face_princess: {
        SPRITE->color=0x0d3ac1ff;
      } break;
    case NS_face_monster: default: {
        SPRITE->color=0xbf8b55ff;
      } break;
  }
  
  return sprite;
}

/* Delete.
 */
 
static void _lawnmowing_del(struct battle *battle) {
  batsup_world_del(BATTLE->world);
}

/* Generate the initial map.
 */
 
static void lawnmowing_generate_map(struct battle *battle) {
  uint8_t *v=BATTLE->world->map->v;
  uint8_t *dsta,*dstb;
  int i;
  
  // Start with tall grass everywhere.
  memset(v,0x08,NS_sys_mapw*NS_sys_maph);
  
  // Walls around the edge.
  for (i=NS_sys_mapw-2,dsta=v+1,dstb=v+(NS_sys_maph-1)*NS_sys_mapw+1;i-->0;dsta++,dstb++) {
    *dsta=0x1b;
    *dstb=0x3b;
  }
  for (i=NS_sys_maph-2,dsta=v+NS_sys_mapw,dstb=v+NS_sys_mapw*2-1;i-->0;dsta+=NS_sys_mapw,dstb+=NS_sys_mapw) {
    *dsta=0x2a;
    *dstb=0x2c;
  }
  v[0]=0x1a;
  v[NS_sys_mapw-1]=0x1c;
  v[(NS_sys_maph-1)*NS_sys_mapw]=0x3a;
  v[NS_sys_mapw*NS_sys_maph-1]=0x3c;
  
  // Count the grass. It's constant, it's 180, but do it dynamically in case we change the generator above.
  BATTLE->grassc=0;
  for (i=NS_sys_mapw*NS_sys_maph,dsta=v;i-->0;dsta++) {
    if ((*dsta>=0x08)&&(*dsta<0x10)) BATTLE->grassc++;
  }
}

/* New.
 */
 
static int _lawnmowing_init(struct battle *battle) {
  if (!(BATTLE->world=batsup_world_new(battle,0))) return -1;
  if (batsup_world_set_image(BATTLE->world,RID_image_battle_labor)<0) return -1;
  double lskill,rskill;
  battle_normalize_bias(&lskill,&rskill,battle);
  player_init(battle,SPRITEID_LEFT,battle->args.lctl,battle->args.lface,lskill);
  player_init(battle,SPRITEID_RIGHT,battle->args.rctl,battle->args.rface,rskill);
  lawnmowing_generate_map(battle);
  BATTLE->timer=20.0;
  return 0;
}

/* Replace each uncut grass tile with the current animation frame.
 */
 
static void lawnmowing_animate_grass(struct battle *battle) {
  uint8_t tileid=0x08+BATTLE->animframe;
  uint8_t *v=BATTLE->world->map->v;
  int i=NS_sys_mapw*NS_sys_maph;
  for (;i-->0;v++) {
    if ((*v>=0x08)&&(*v<0x10)) *v=tileid;
  }
}

/* Update.
 */
 
static void _lawnmowing_update(struct battle *battle,double elapsed) {
  batsup_world_update(BATTLE->world,elapsed);
  if ((BATTLE->animclock-=elapsed)<=0.0) {
    BATTLE->animclock+=0.200;
    if (++(BATTLE->animframe)>=8) BATTLE->animframe=0;
    lawnmowing_animate_grass(battle);
  }
  
  // If the timer expires, whoever's ahead wins. Ties are possible.
  if (battle->outcome==-2) {
    if ((BATTLE->timer-=elapsed)<=0.0) {
      struct batsup_sprite *l=batsup_sprite_by_id(BATTLE->world,SPRITEID_LEFT);
      struct batsup_sprite *r=batsup_sprite_by_id(BATTLE->world,SPRITEID_RIGHT);
      if (!l||!r) { // Can't happen, but if a sprite goes missing call it a tie.
        battle->outcome=0;
      } else {
        struct sprite_player *L=(struct sprite_player*)l;
        struct sprite_player *R=(struct sprite_player*)r;
        if (L->score>R->score) battle->outcome=1;
        else if (L->score<R->score) battle->outcome=-1;
        else battle->outcome=0;
      }
    }
  }
  
  // If all the grass is mowed, game is over.
  if (battle->outcome==-2) {
    struct batsup_sprite *l=batsup_sprite_by_id(BATTLE->world,SPRITEID_LEFT);
    struct batsup_sprite *r=batsup_sprite_by_id(BATTLE->world,SPRITEID_RIGHT);
    if (!l||!r) { // Can't happen, but if a sprite goes missing call it a tie.
      battle->outcome=0;
    } else {
      struct sprite_player *L=(struct sprite_player*)l;
      struct sprite_player *R=(struct sprite_player*)r;
      if (L->score+R->score>=BATTLE->grassc) {
        if (L->score>R->score) battle->outcome=1;
        else if (L->score<R->score) battle->outcome=-1;
        else battle->outcome=0;
      }
    }
  }
}

/* Render.
 */
 
static void lawnmowing_render_score(struct battle *battle,int spriteid) {
  struct batsup_sprite *sprite=batsup_sprite_by_id(BATTLE->world,spriteid);
  if (!sprite) return;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  int score=SPRITE->score;
  if (score>999) score=999;
  if (score<0) score=0;
  int scx,scy;
  batsup_sprite_render_position(&scx,&scy,sprite);
  scy-=NS_sys_tilesize;
  if (score>=100) scx-=8;
  else if (score>=10) scx-=4;
  if (score>=100) { graf_fancy(&g.graf,scx,scy,0x20+score/100,0,0,NS_sys_tilesize,0,0x808080ff); scx+=8; }
  if (score>=10) { graf_fancy(&g.graf,scx,scy,0x20+(score/10)%10,0,0,NS_sys_tilesize,0,0x808080ff); scx+=8; }
  graf_fancy(&g.graf,scx,scy,0x20+score%10,0,0,NS_sys_tilesize,0,0x808080ff);
}
 
static void _lawnmowing_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
  
  /* Render scores above each player.
   * We don't do this during player_render(), because they'd overlap the sprites.
   * I guess they should be distinct sprites but meh.
   */
  lawnmowing_render_score(battle,SPRITEID_LEFT);
  lawnmowing_render_score(battle,SPRITEID_RIGHT);
  
  /* Draw the clock, centered near the top.
   */
  int sec=(int)(BATTLE->timer+0.999);
  if (sec<0) sec=0;
  if (sec>=10) graf_fancy(&g.graf,(FBW>>1)-4,10,0x20+(sec/10)%10,0,0,NS_sys_tilesize,0,0x808080ff);
  graf_fancy(&g.graf,(FBW>>1)+4,10,0x20+sec%10,0,0,NS_sys_tilesize,0,0x808080ff);
}

/* Type definition.
 */
 
const struct battle_type battle_type_lawnmowing={
  .name="lawnmowing",
  .objlen=sizeof(struct battle_lawnmowing),
  .strix_name=172,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_lawnmowing_del,
  .init=_lawnmowing_init,
  .update=_lawnmowing_update,
  .render=_lawnmowing_render,
};

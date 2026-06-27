/* battle_snowboard.c
 */

#include "game/bellacopia.h"

#define TREE_LIMIT 128
#define TREE_RADIUS 6.0
#define PLAYER_RADIUS 6.0
#define COURSE_W 150.0
#define COURSE_H 800.0

struct battle_snowboard {
  struct battle hdr;
  double termclock;
  
  struct tree {
    double x,y; // Course pixels.
    uint8_t tileid;
    uint8_t xform;
  } treev[TREE_LIMIT];
  int treec;
  int midtreec; // Count of trees in the middle, ie not counting the walls. Optimization for CPU controller.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    
    // Controllers set.
    int turn; // -1,0,1
    
    // CPU controller.
    double antijitter;
    
    // Universal state.
    double x,y; // Pixels relative to the course. (y) can be read as "progress".
    double runtime; // s
    double gravity; // px/s**2
    double hurtclock;
    double hurtdx,hurtdy; // Unit vector; speed depends on clock.
    
    // Constants per skill.
    double turn_speed; // px/s
    double gravity_high; // px/s**2, freefall rate
    double gravity_low; // px/s**2, brake rate
    double gravity_high_limit; // px/s, limit during freefall
    double gravity_low_limit; // px/s, limit during brake, only applies when increasing acceleration
    double hurt_time; // s
    
  } playerv[2];
};

#define BATTLE ((struct battle_snowboard*)battle)

/* Delete.
 */
 
static void _snowboard_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=-20.0;
  } else { // Right.
    player->who=1;
    player->x=20.0;
  }
  
  player->turn_speed=40.0*(1.0-player->skill)+60.0*player->skill;
  player->gravity_high=200.0;
  player->gravity_low=100.0;
  player->gravity_high_limit=130.0*(1.0-player->skill)+190.0*player->skill;
  player->gravity_low_limit=30.0*(1.0-player->skill)+40.0*player->skill;
  player->hurt_time=0.750*(1.0-player->skill)+0.250*player->skill;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->gravity_high_limit*=0.800;
    player->gravity_low_limit*=0.800;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xe0f6f1ff;
        player->tileid=0x43;
      } break;
    case NS_face_dot: {
        player->color=0xa668f3ff; // 0x411775ff Brighter than normal Dot, to contrast with the black line.
        player->tileid=0x40;
      } break;
    case NS_face_princess: {
        player->color=0x5d83f4ff; // 0x0d3ac1ff Same problem as Dot.
        player->tileid=0x46;
      } break;
  }
}

/* Add tree.
 */
 
static struct tree *snowboard_add_tree(struct battle *battle,double x,double y) {
  if (BATTLE->treec>=TREE_LIMIT) {
    fprintf(stderr,"Too many trees\n");
    return 0;
  }
  struct tree *tree=BATTLE->treev+BATTLE->treec++;
  tree->x=x;
  tree->y=y;
  // 3 tile options, and can flop, so 6.
  switch (rand()%6) {
    case 0: tree->tileid=0x49; tree->xform=0; break;
    case 1: tree->tileid=0x4a; tree->xform=0; break;
    case 2: tree->tileid=0x4b; tree->xform=0; break;
    case 3: tree->tileid=0x49; tree->xform=EGG_XFORM_XREV; break;
    case 4: tree->tileid=0x4a; tree->xform=EGG_XFORM_XREV; break;
    case 5: tree->tileid=0x4b; tree->xform=EGG_XFORM_XREV; break;
  }
  return tree;
}

/* Find a position for an obstacle tree that isn't very close to any existing tree.
 * <0 if we fail to.
 */
 
static int snowboard_freeish_tree_position(double *_x,double *_y,struct battle *battle) {
  const double xlo=COURSE_W*-0.5+30.0;
  const double xhi=COURSE_W* 0.5-30.0;
  const double ylo=40.0;
  const double yhi=COURSE_H-40.0;
  const double threshold=40.0; // Leave enough room to get between any two trees. 40 is plenty.
  const double threshold2=threshold*threshold;
  int panic=100;
  while (panic-->0) {
    double x=(rand()&0xffff)/65535.0;
    x=xlo*(1.0-x)+xhi*x;
    double y=(rand()&0xffff)/65535.0;
    y=ylo*(1.0-y)+yhi*y;
    int ok=1,i=BATTLE->treec;
    const struct tree *tree=BATTLE->treev;
    for (;i-->0;tree++) {
      double dx=tree->x-x;
      double dy=tree->y-y;
      double d2=dx*dx+dy*dy;
      if (d2<threshold2) {
        ok=0;
        break;
      }
    }
    if (ok) {
      *_x=x;
      *_y=y;
      return 1;
    }
  }
  return -1;
}

/* New.
 */
 
static int _snowboard_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  // And I keep getting this backward: At HIGH skill, the player is favored to win. High bias means high skill for the CPU and low skill for Dot.
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  BATTLE->termclock=20.0; // It's usually over in under 10 seconds. If we run so long, assume someone is stuck.
  
  /* Place a fixed set of obstacle trees.
   * Ensure none is very close to any other.
   * Do these before the edge wall, so we're not checking against fifty edge trees every time.
   * Also record the count so CPU controllers can examine just these, and not the walls.
   */
  int treei=15;
  while (treei-->0) {
    double x,y;
    if (snowboard_freeish_tree_position(&x,&y,battle)>=0) {
      snowboard_add_tree(battle,x,y);
    }
  }
  BATTLE->midtreec=BATTLE->treec;
  
  /* Make a solid column of trees on the horizontal edges of the course.
   * No need to block the space above or below the course.
   */
  const double dy=TREE_RADIUS*2.5;
  const double lx=COURSE_W*-0.5;
  const double rx=COURSE_W*0.5;
  double y=0.0;
  for (;y<COURSE_H;y+=dy) {
    snowboard_add_tree(battle,lx+(rand()&7)-3.0,y);
    snowboard_add_tree(battle,rx+(rand()&7)-3.0,y);
  }
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->turn=-1; break;
    case EGG_BTN_RIGHT: player->turn=1; break;
    default: player->turn=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_cpu_decide(struct player *player,int turn) {
  if (turn==player->turn) return;
  player->antijitter=0.250;
  player->turn=turn;
}
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* If our jitter clock is set, run it down and maintain current control.
   */
  if (player->antijitter>0.0) {
    player->antijitter-=elapsed;
    return;
  }

  /* If we're close to the walls, steer inward regardless of anything else.
   * We're not going to examine wall trees for the general case.
   */
  const double lx=COURSE_W*-0.5+20.0;
  const double rx=COURSE_W* 0.5-20.0;
  if (player->x<lx) {
    player_cpu_decide(player,1);
    return;
  }
  if (player->x>rx) {
    player_cpu_decide(player,-1);
    return;
  }
  
  /* Find the next tree below me and if it's close, steer to avoid.
   */
  const struct tree *next=0;
  const struct tree *tree=BATTLE->treev;
  int i=BATTLE->midtreec;
  for (;i-->0;tree++) {
    if (tree->y<=player->y) continue;
    double dx=player->x-tree->x;
    if ((dx<-20.0)||(dx>20.0)) continue;
    double dy=tree->y-player->y;
    if (dy>60.0) continue; // Far enough away, don't accodate it yet.
    if (!next||(tree->y<next->y)) next=tree;
  }
  if (next) {
    double dx=player->x-next->x;
    // Trees very close to the edge, steer inward, regardless of relative positions.
    if (next->x<lx+20.0) player_cpu_decide(player,1);
    else if (next->x>rx-20.0) player_cpu_decide(player,-1);
    // Otherwise, steer the shorter way.
    else if (dx<0.0) player_cpu_decide(player,-1);
    else player_cpu_decide(player,1);
    return;
  }
  
  /* Nothing? Cool, floor it.
   */
  player_cpu_decide(player,0);
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If I've finished, stop turning and reduce gravity to zero.
   * There won't be anything to collide against below the finish line so don't worry about collisions.
   * (runtime) stops counting when finished.
   * Same treatment if the battle's outcome is established.
   */
  if ((player->y>=COURSE_H)||(battle->outcome>-2)) {
    player->turn=0;
    player->hurtclock=0.0;
    if ((player->gravity-=400.0*elapsed)>0.0) {
      player->y+=player->gravity*elapsed;
    }
    return;
  }
  player->runtime+=elapsed;
  
  /* If we're hurt, tick that clock down and apply extra motion.
   * It doesn't impact any other motion, just gets added on top.
   */
  if (player->hurtclock>0.0) {
    if ((player->hurtclock-=elapsed)>0.0) {
      const double maxspeed=200.0;
      double speed=(maxspeed*player->hurtclock)/player->hurt_time;
      player->x+=speed*elapsed*player->hurtdx;
      player->y+=speed*elapsed*player->hurtdy;
    }
  }
  
  /* Move horizontally if requested.
   */
  if (player->turn) {
    player->x+=player->turn_speed*elapsed*player->turn;
  }
  
  /* Nudge gravity toward its limit.
   */
  if (player->turn) {
    if (player->gravity>player->gravity_low_limit) {
      if ((player->gravity-=player->gravity_low*elapsed)<=player->gravity_low_limit) {
        player->gravity=player->gravity_low_limit;
      }
    } else if (player->gravity<player->gravity_low_limit) {
      if ((player->gravity+=player->gravity_low*elapsed)>=player->gravity_low_limit) {
        player->gravity=player->gravity_low_limit;
      }
    }
  } else {
    if ((player->gravity+=player->gravity_high*elapsed)>=player->gravity_high_limit) {
      player->gravity=player->gravity_high_limit;
    }
  }
  
  /* Apply gravity.
   */
  player->y+=player->gravity*elapsed;
  
  /* Detect and resolve collisions.
   */
  const double hitdistance=TREE_RADIUS+PLAYER_RADIUS;
  const double hitdistance2=hitdistance*hitdistance;
  const struct tree *tree=BATTLE->treev;
  int i=BATTLE->treec;
  for (;i-->0;tree++) {
    double dy=player->y-tree->y;
    if ((dy<-hitdistance)||(dy>hitdistance)) continue;
    double dx=player->x-tree->x;
    if ((dx<-hitdistance)||(dx>hitdistance)) continue;
    double d2=dx*dx+dy*dy;
    if (d2>=hitdistance2) continue;
    double distance=sqrt(d2);
    double pen=hitdistance-distance;
    
    // Unit vector running from tree toward player.
    double nx=dx/distance;
    double ny=dy/distance;
    
    // Force player to slightly outside the collision zone, along that vector.
    // The extra fuzz is to ensure we don't get a repeat of the same collision next frame. Probly not necessary.
    player->x=tree->x+nx*hitdistance*1.050;
    player->y=tree->y+ny*hitdistance*1.050;
    
    // In the very likely case that our normal points upward, reset gravity to zero.
    // Don't touch gravity if the normal points down (ie head against root) -- let savvy players exploit that for damage boosting.
    if (ny<0.0) {
      player->gravity=0.0;
    }
    
    // Register the damage.
    player->hurtclock=player->hurt_time;
    player->hurtdx=nx;
    player->hurtdy=ny;
    bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
    
    // Limit one collision per frame.
    break;
  }
}

/* Update.
 */
 
static void _snowboard_update(struct battle *battle,double elapsed) {
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  /* If (termclock) expires, end the game wherever it is right now.
   * Ties are possible but extremely unlikely.
   */
  if (battle->outcome==-2) {
    if ((BATTLE->termclock-=elapsed)<=0.0) {
      if (l->runtime<r->runtime) battle->outcome=1;
      else if (l->runtime>r->runtime) battle->outcome=-1;
      else if (l->y>r->y) battle->outcome=1;
      else if (l->y<r->y) battle->outcome=-1;
      else battle->outcome=0;
    // Otherwise the game ends when both players reach the finish line.
    } else if ((l->y>=COURSE_H)&&(r->y>=COURSE_H)) {
      if (l->runtime<r->runtime) battle->outcome=1;
      else if (l->runtime>r->runtime) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Helper to render one tile in a scene.
 */
 
static inline void snowboard_tile(
  struct battle *battle,
  int camx,int camy,int camw,int camh,
  int scrollx,int scrolly,
  double worldx,double worldy,
  uint8_t tileid,uint8_t xform
) {
  const int ht=NS_sys_tilesize>>1;
  int dstx=(int)worldx-scrollx;
  if ((dstx<-ht)||(dstx>=camw+ht)) return;
  int dsty=(int)worldy-scrolly;
  if ((dsty<-ht)||(dsty>=camh>ht)) return;
  dstx+=camx;
  dsty+=camy;
  graf_tile(&g.graf,dstx,dsty,tileid,xform);
}

#define TILE(worldx,worldy,tileid,xform) snowboard_tile(battle,camx,camy,camw,camh,scrollx,scrolly,worldx,worldy,tileid,xform);

/* Render one full scene, ie half of the screen.
 * (cam*) are the output position in framebuffer pixels.
 * The bars come later, not our problem.
 * Background is initially snowed out.
 * Use only graf_tile.
 */
 
static void snowboard_render_scene(
  struct battle *battle,
  struct player *player,
  int camx,int camy,int camw,int camh
) {

  // Determine scroll. Player should stay centered and high.
  int scrollx=(int)player->x-(camw>>1);
  int scrolly=(int)(player->y-camh*0.333);
  
  // Finish line.
  int finy=(int)COURSE_H-scrolly;
  if ((finy>=-(NS_sys_tilesize>>1))&&(finy<camh+(NS_sys_tilesize>>1))) {
    finy+=camy;
    int dstx=camx-NS_sys_tilesize;
    dstx-=scrollx%NS_sys_tilesize;
    for (;dstx<=camx+camw+(NS_sys_tilesize>>1);dstx+=NS_sys_tilesize) {
      if (dstx>=camx-(NS_sys_tilesize>>1)) graf_tile(&g.graf,dstx,finy,0x4c,0);
    }
  }
  
  // Trees.
  const struct tree *tree=BATTLE->treev;
  int i=BATTLE->treec;
  for (;i-->0;tree++) {
    TILE(tree->x,tree->y,tree->tileid,tree->xform);
  }
  
  // Opponent.
  {
    struct player *other=(player==BATTLE->playerv)?(BATTLE->playerv+1):(BATTLE->playerv+0);
    if (other->hurtclock>0.0) {
      int alpha=0x40+(int)((other->hurtclock*128.0)/other->hurt_time);
      graf_set_tint(&g.graf,0xff000000|alpha);
    }
    uint8_t tileid=other->tileid;
    if (other->turn>0) tileid+=1;
    else if (other->turn<0) tileid+=2;
    TILE(other->x,other->y,tileid,0);
    if (other->hurtclock>0.0) graf_set_tint(&g.graf,0);
  }

  // Hero.
  if (player->hurtclock>0.0) {
    int alpha=0x40+(int)((player->hurtclock*128.0)/player->hurt_time);
    graf_set_tint(&g.graf,0xff000000|alpha);
  }
  uint8_t tileid=player->tileid;
  if (player->turn>0) tileid+=1;
  else if (player->turn<0) tileid+=2;
  TILE(player->x,player->y,tileid,0)
  if (player->hurtclock>0.0) graf_set_tint(&g.graf,0);
}

#undef TILE

/* Render one progress bar.
 * Background has already been blacked out, so just draw the indicator.
 */
 
static void snowboard_render_bar(
  struct battle *battle,
  struct player *player,
  int dstx,int dsty,int dstw,int dsth
) {
  const int margin=1;
  int thumbw=dstw-(margin<<1); // Also thumb height
  if (thumbw<1) return;
  int availh=dsth-(margin<<1)-thumbw;
  if (availh<0) return;
  int offset=(int)((player->y*availh)/COURSE_H);
  if (offset<0) offset=0;
  else if (offset>availh) offset=availh;
  graf_fill_rect(&g.graf,dstx+margin,dsty+margin+offset,thumbw,thumbw,player->color);
}

/* Render runtime for one scene.
 */
 
static void snowboard_render_runtime(
  struct battle *battle,
  struct player *player,
  int camx,int camy,int camw,int camh
) {
  int dstx=camx+(camw>>1)-28;
  int dsty=camy+camh-12;
  int ms=(int)(player->runtime*1000.0);
  if (ms<0) ms=0;
  int sec=ms/1000; ms%=1000;
  int min=sec/60; sec%=60; // Can't be more than zero but we're being correct about it.
  if (min>9) {
    min=9;
    sec=99;
    ms=999;
  }
  graf_tile(&g.graf,dstx,dsty,'0'+min,0); dstx+=8;
  graf_tile(&g.graf,dstx,dsty,':',0); dstx+=8;
  graf_tile(&g.graf,dstx,dsty,'0'+sec/10,0); dstx+=8;
  graf_tile(&g.graf,dstx,dsty,'0'+sec%10,0); dstx+=8;
  graf_tile(&g.graf,dstx,dsty,'.',0); dstx+=8;
  graf_tile(&g.graf,dstx,dsty,'0'+ms/100,0); dstx+=8;
  graf_tile(&g.graf,dstx,dsty,'0'+(ms/10)%10,0); dstx+=8;
  graf_tile(&g.graf,dstx,dsty,'0'+ms%10,0);
}

/* Render.
 */
 
static void _snowboard_render(struct battle *battle) {

  graf_fill_rect(&g.graf,0,0,FBW,FBH,0xffffffff);
  
  /* Player scenes.
   * Two bars must be at least a tile, ie min 8 here.
   * We use the bars to cover errant tile edges, since we can't clip the camera.
   */
  const int barw=8;
  const int camw=(FBW>>1)-barw;
  graf_set_image(&g.graf,RID_image_icepalace_sprites);
  snowboard_render_scene(battle,BATTLE->playerv+0,0,0,camw,FBH);
  snowboard_render_scene(battle,BATTLE->playerv+1,camw+(barw<<1),0,camw,FBH);
  
  /* Vertical progress bars.
   * These show progress and separate the cameras, but they also secretly serve to cover errant tile edges.
   */
  graf_set_input(&g.graf,0);
  graf_fill_rect(&g.graf,camw,0,barw<<1,FBH,0x000000ff);
  snowboard_render_bar(battle,BATTLE->playerv+0,camw,0,barw,FBH);
  snowboard_render_bar(battle,BATTLE->playerv+1,camw+barw,0,barw,FBH);
  
  /* Runtime at the bottom of each scene.
   */
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_set_tint(&g.graf,0x000000ff);
  snowboard_render_runtime(battle,BATTLE->playerv+0,0,0,camw,FBH);
  snowboard_render_runtime(battle,BATTLE->playerv+1,camw+(barw<<1),0,camw,FBH);
  graf_set_tint(&g.graf,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_snowboard={
  .name="snowboard",
  .objlen=sizeof(struct battle_snowboard),
  .id=NS_battle_snowboard,
  .strix_name=231,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_snowboard_del,
  .init=_snowboard_init,
  .update=_snowboard_update,
  .render=_snowboard_render,
};

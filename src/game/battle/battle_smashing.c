/* battle_smashing.c
 * A to jump. Smash the melons as they roll by.
 */

#include "game/bellacopia.h"

#define GROUNDY 140
#define GRAVITY_ACCEL 300.0
#define GRAVITY_LIMIT 200.0
#define THROW_MIN  3 /* How long before the CPU player makes a mistake. 0x6d strikes him on the fifth, 0x6e and above will not strike him. */
#define THROW_MAX 10 /* Greater than SCORE_MAX, ie at most difficulties, the CPU player won't miss. */
#define SCORE_MAX 5
#define PARTICLE_LIMIT 256 /* For particles in flight. Once bound, they are part of the terrain texture. */
#define PARTICLEC_LO 60 /* How many particles per vegetable. */
#define PARTICLEC_HI 100

#define POSE_IDLE 0
#define POSE_LOSE 1
#define POSE_JUMP 2
#define POSE_FALL 3
#define POSE_COUNT 4

struct battle_smashing {
  struct battle hdr;
  int choice;
  int bgtexid,bgw,bgh;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t xform;
    double x,y; // Framebuffer pixels. (x) is constant. Bottom middle of sprite.
    struct pose {
      int srcx,srcy;
    } posev[POSE_COUNT];
    int posep;
    int input; // Boolean, you just get one button.
    int pvinput;
    int seated;
    int jumping;
    double jumpforce; // px/s positive
    double jumpforce_initial; // px/s positive
    double jumpforce_decel; // px/s**2 positive
    double gravity; // px/s**2 positive
    int score;
    uint32_t color;
    // For CPU players:
    int throw; // Counts down until the one we're going to miss. Only relevant at low difficulties.
    int vegid;
    double leadlo,leadhi; // Timing range per difficulty, constant. This is how much time we waste per vegetable.
    double falldx,jumpdx; // Selected timing for the current vegetable. Phrased in terms of distance to vegetable.
  } playerv[2];
  
  /* There's two possible vegetables, one for each player.
   * [0] has (dx>0) and [1] has (dx<0).
   */
  struct vegetable {
    double x,y;
    double dx;
    double t;
    double dt;
    uint8_t tileid;
    int still;
    int falling;
    int vegid; // Positive and unique, or zero if this vegetable not in use.
    uint32_t color;
  } vegetablev[2];
  int vegid_next;
  
  struct particle {
    int defunct;
    double x,y;
    double dx,dy;
    uint32_t color;
    double shloopclock;
  } particlev[PARTICLE_LIMIT];
  int particlec;
};

#define BATTLE ((struct battle_smashing*)battle)

/* Delete.
 */
 
static void _smashing_del(struct battle *battle) {
  egg_texture_del(BATTLE->bgtexid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=GROUNDY;
  player->pvinput=1; // If it's on initially, wait for it to go off.
  player->seated=1;
  player->jumpforce_initial=250.0;
  player->jumpforce_decel=400.0;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.5-40.0;
    player->xform=EGG_XFORM_XREV; // Face outward. Natural orientation is right.
  } else { // Right.
    player->who=1;
    player->x=FBW*0.5+40.0;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->throw=(int)((1.0-player->skill)*THROW_MIN+player->skill*THROW_MAX);
    if (player->throw<1) player->throw=1;
    player->leadhi=(1.0-player->skill)*0.250;
    player->leadlo=player->leadhi*0.500;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x546a35ff;
        player->posev[POSE_IDLE]=(struct pose){  0,160};
        player->posev[POSE_LOSE]=(struct pose){ 48,160};
        player->posev[POSE_JUMP]=(struct pose){ 96,160};
        player->posev[POSE_FALL]=(struct pose){144,160};
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->posev[POSE_IDLE]=(struct pose){  0, 64};
        player->posev[POSE_LOSE]=(struct pose){ 48, 64};
        player->posev[POSE_JUMP]=(struct pose){  0,208};
        player->posev[POSE_FALL]=(struct pose){ 48,208};
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->posev[POSE_IDLE]=(struct pose){  0,112};
        player->posev[POSE_LOSE]=(struct pose){ 48,112};
        player->posev[POSE_JUMP]=(struct pose){ 96,208};
        player->posev[POSE_FALL]=(struct pose){144,208};
      } break;
  }
}

/* Generate the initial background texture.
 */
 
static int smashing_generate_bgtex(struct battle *battle) {
  BATTLE->bgw=FBW;
  BATTLE->bgh=FBH-GROUNDY;
  BATTLE->bgtexid=egg_texture_new();
  if (egg_texture_load_raw(BATTLE->bgtexid,BATTLE->bgw,BATTLE->bgh,BATTLE->bgw<<2,0,0)<0) return -1;
  egg_texture_clear(BATTLE->bgtexid);
  
  int cliffl=(FBW>>1)-20;
  int cliffr=(FBW>>1)+20;
  uint32_t floorcolor=0x006020ff;
  
  graf_set_output(&g.graf,BATTLE->bgtexid);
  graf_fill_rect(&g.graf,0,0,cliffl,BATTLE->bgh,floorcolor);
  graf_fill_rect(&g.graf,cliffr,0,BATTLE->bgw-cliffr,BATTLE->bgh,floorcolor);
  graf_fill_rect(&g.graf,0,0,cliffl,1,0x000000ff);
  graf_fill_rect(&g.graf,cliffr,0,BATTLE->bgw-cliffr,1,0x000000ff);
  graf_fill_rect(&g.graf,cliffl,0,1,BATTLE->bgh,0x000000ff);
  graf_fill_rect(&g.graf,cliffr,0,1,BATTLE->bgh,0x000000ff);
  graf_set_output(&g.graf,1);
  
  return 0;
}

/* Set one pixel in the background texture.
 */
 
static void smashing_add_bgpixel(struct battle *battle,int x,int y,uint32_t color) {
  y-=GROUNDY;
  if ((x<0)||(x>=BATTLE->bgw)) return;
  if ((y<0)||(y>=BATTLE->bgh)) return;
  graf_set_output(&g.graf,BATTLE->bgtexid);
  graf_set_input(&g.graf,0);
  graf_point(&g.graf,x,y,color&0xffffff7f);
  graf_set_output(&g.graf,1);
}

/* New.
 */
 
static int _smashing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->vegid_next=1;
  if (smashing_generate_bgtex(battle)<0) return -1;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  player->input=(input&EGG_BTN_SOUTH)?1:0;
}

/* CPU player is targetting a new vegetable.
 * Establish (jumpdx,falldx), the critical parameters for his timing.
 */
 
static void player_prepare_jump(struct battle *battle,struct player *player,struct vegetable *veg) {

  // If throwing, we don't need parameters. They won't be used.
  if (player->throw<=0) return;
  
  /* Select (veg)'s position when we intend to strike it. Relative to our center, like the thresholds.
   * This is constant per (skill) but we recalculate every time. Can randomize if we like.
   * We compute against a narrower radius than player_update_common(), to avoid the ugly edges.
   */
  const double radius=12.0;
  double strikex_best,strikex_worst;
  if (player->who) {
    strikex_best=radius;
    strikex_worst=-radius;
  } else {
    strikex_best=-radius;
    strikex_worst=radius;
  }
  double strikex=strikex_best*player->skill+strikex_worst*(1.0-player->skill);
  
  /* Select the desired height of the jump.
   * This is decorative; the only thing that matters is when we land.
   * It's useful as a signal to the human, how smart this CPU player really is.
   */
  const double hlo=   30.0;
  const double hhi=  100.0;
  const double hfuzz= 20.0;
  double n=(rand()&0xffff)/65535.0;
  double h=player->skill*hlo+(1.0-player->skill)*hhi+n*hfuzz;
  
  /* How long does it take to reach this height, and how long to return from it?
   * Determine these iteratively, rather than working out the exact calculus. That's not really something I can be trusted with.
   */
  const double interval=0.016666;
  const double contacth=10.0;
  double upt=0.0;
  double qh=0.0;
  double jumpforce=player->jumpforce_initial;
  while (qh<h) {
    upt+=interval;
    jumpforce-=player->jumpforce_decel*interval;
    if (jumpforce<=0.0) break; // Too high! Let whatever happens happen.
    qh+=jumpforce*interval;
  }
  double downt=0.0;
  double gravity=0.0;
  while (qh>contacth) {
    downt+=interval;
    gravity+=GRAVITY_ACCEL*interval;
    if (gravity>=GRAVITY_LIMIT) gravity=GRAVITY_LIMIT;
    qh-=gravity*interval;
  }
  
  /* Now it's a simple matter of turning those up and down times into distances from (strikex).
   */
  double vegspeed=veg->dx;
  if (vegspeed<0.0) vegspeed=-vegspeed;
  player->falldx=vegspeed*downt+strikex;
  player->jumpdx=vegspeed*(downt+upt)+strikex;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Find the next vegetable.
   * If it's not spawned yet, go idle.
   */
  struct vegetable *veg=BATTLE->vegetablev+player->who;
  if (!veg->vegid) {
    player->input=0;
    return;
  }
  
  /* If this is not the vegetable we saw last time, tick (throw).
   * And if (throw) reaches zero, we become noop, let the next one hit us.
   * Also, establish our timing for this jump.
   */
  if (veg->vegid!=player->vegid) {
    player->vegid=veg->vegid;
    player->throw--;
    player_prepare_jump(battle,player,veg);
  }
  if (player->throw<=0) {
    player->input=0;
    return;
  }
  
  /* Get positive distance to vegetable and check it against our thresholds.
   */
  double distance;
  if (player->who) distance=veg->x-player->x;
  else distance=player->x-veg->x;
  if (distance<player->falldx) player->input=0;
  else if (distance<player->jumpdx) player->input=1;
  else player->input=0;
}

/* Return a color near the input, randomly off a little.
 */
 
static uint32_t fuzz_rgba(uint32_t rgba) {
  int r=(rgba>>24)&0xff;
  int g=(rgba>>16)&0xff;
  int b=(rgba>> 8)&0xff;
  int a=(rgba    )&0xff;
  r+=rand()%32-16; if (r<0) r=0; else if (r>0xff) r=0xff;
  g+=rand()%32-16; if (g<0) g=0; else if (g>0xff) g=0xff;
  b+=rand()%32-16; if (b<0) b=0; else if (b>0xff) b=0xff;
  return (r<<24)|(g<<16)|(b<<8)|a;
}

/* Create a bunch of random particles for a given vegetable.
 */
 
static void smashing_generate_particles(struct battle *battle,struct vegetable *veg) {

  // How many?
  int availc=PARTICLE_LIMIT-BATTLE->particlec;
  if (availc<=0) return;
  if (availc>PARTICLEC_HI) availc=PARTICLEC_HI;
  int c;
  if (availc<=PARTICLEC_LO) c=availc;
  else c=PARTICLEC_LO+rand()%(availc-PARTICLEC_LO+1);
  
  // Make them.
  const double horzrange=50.0; // px/s, signed.
  const double vertlo=-100.0; // px/s
  const double verthi=10.0; // ps/s. Verts are asymmetric because pointing down would terminate really fast.
  struct particle *particle=BATTLE->particlev+BATTLE->particlec;
  BATTLE->particlec+=c;
  for (;c-->0;particle++) {
    particle->defunct=0;
    particle->x=veg->x+(rand()%12)-6;
    particle->y=veg->y+(rand()%12)-6;
    particle->dx=(((rand()&0xffff)-32768)*horzrange)/32768.0;
    particle->dy=vertlo+((rand()&0xffff)*(verthi-vertlo))/65535.0;
    particle->color=fuzz_rgba(veg->color);
    particle->shloopclock=0.0;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  int interactive=(battle->outcome==-2);
  if (!interactive) player->input=0;
  
  /* Begin a jump, cut the jump short, or update input state.
   */
  if (player->input&&!player->pvinput) {
    player->pvinput=1;
    if (player->seated) { // Begin jump.
      bm_sound_pan(RID_sound_jump,player->who?0.250:-0.250);
      player->jumping=1;
      player->seated=0;
      player->gravity=0.0;
      player->jumpforce=player->jumpforce_initial;
    }
  } else if (!player->input) {
    player->pvinput=0;
    if (player->jumping) {
      player->jumping=0;
    }
  }
  
  /* Apply jump or gravity.
   */
  if (player->jumping) {
    if ((player->jumpforce-=player->jumpforce_decel*elapsed)<=0.0) {
      // Crested, restart gravity.
      player->jumping=0;
    } else {
      // Jump ongoing.
      player->y-=player->jumpforce*elapsed;
    }
  } else if (!player->seated) {
    if ((player->gravity+=GRAVITY_ACCEL*elapsed)>=GRAVITY_LIMIT) {
      player->gravity=GRAVITY_LIMIT;
    }
    player->y+=player->gravity*elapsed;
    if (player->y>=GROUNDY) {
      // Landed.
      player->y=GROUNDY;
      player->seated=1;
      player->gravity=0.0;
    }
  }
  
  /* Check vegetables.
   * Note that we check *both* vegetables.
   * See vegetable_update(); a wee change there can allow veg to attack the other guy from behind.
   * But I'm not using that feature, after a little consideration.
   */
  if ((battle->outcome==-2)&&(player->y>GROUNDY-15.0)) {
    const double radius=15.0;
    struct vegetable *veg=BATTLE->vegetablev;
    int i=2;
    for (;i-->0;veg++) {
      if (!veg->vegid) continue;
      double dx=veg->x-player->x;
      if ((dx<-radius)||(dx>radius)) continue;
      if (player->seated||player->jumping) {
        bm_sound_pan(RID_sound_ouch,player->who?0.250:-0.250);
        battle->outcome=player->who?1:-1;
        veg->still=1;
        break;
      } else {
        bm_sound_pan(RID_sound_pop,player->who?0.250:-0.250);
        veg->vegid=0;
        player->score++;
        smashing_generate_particles(battle,veg);
      }
    }
  }
  
  /* Set the appropriate face.
   */
  if (battle->outcome>-2) {
    if (player->who) {
      player->posep=(battle->outcome==1)?POSE_LOSE:POSE_IDLE;
    } else {
      player->posep=(battle->outcome==-1)?POSE_LOSE:POSE_IDLE;
    }
  } else if (player->jumping) player->posep=POSE_JUMP;
  else if (player->seated) player->posep=POSE_IDLE;
  else player->posep=POSE_FALL;
}

/* Update one vegetable.
 */
 
static void vegetable_update(struct battle *battle,struct vegetable *veg,double elapsed) {
  if (veg->still||!veg->vegid) return;
  
  if (veg->falling) {
    veg->y+=100.0*elapsed;
    if (veg->y>FBH+10.0) veg->vegid=0;
    return;
  }
  
  veg->x+=veg->dx*elapsed;
  
  /* Eliminate when we reach the horizontal midpoint.
   * It's fun to play without this, because you can let one thru in the hopes it will strike your opponent from behind.
   * But our CPU player isn't built for it. I don't really like the extra complexity.
   */
  if (((veg->dx>0.0)&&(veg->x>FBW*0.475))||((veg->dx<0.0)&&(veg->x<FBW*0.525))) {
    struct player *player=BATTLE->playerv;
    if (veg->dx<0.0) player+=1;
    if (player->score>0) player->score--;
    veg->falling=1;
    return;
  }
  
  veg->t+=veg->dt*elapsed;
  while (veg->t<-M_PI) veg->t+=M_PI*2.0;
  while (veg->t>M_PI) veg->t-=M_PI*2.0;
}

/* Generate the next vegetable.
 */
 
static void smashing_generate_vegetable(struct battle *battle,int who) {
  if ((who<0)||(who>1)) return;
  struct vegetable *veg=BATTLE->vegetablev+who;
  if (veg->vegid) return;
  veg->vegid=BATTLE->vegid_next++;
  veg->dx=80.0;
  veg->dt=5.0;
  veg->still=0;
  veg->falling=0;
  if (who) {
    veg->x=FBW+NS_sys_tilesize;
    veg->dx=-veg->dx;
    veg->dt=-veg->dt;
  } else {
    veg->x=-NS_sys_tilesize;
  }
  veg->y=GROUNDY-7.0;
  switch (rand()%6) {
    case 0: veg->tileid=0xdc; veg->color=0xb61c32ff; break; // tomato
    case 1: veg->tileid=0xdd; veg->color=0xc97de8ff; break; // plum
    case 2: veg->tileid=0xec; veg->color=0xc4762fff; break; // pumpkin
    case 3: veg->tileid=0xed; veg->color=0x998273ff; break; // acorn
    case 4: veg->tileid=0xfc; veg->color=0xe51485ff; break; // watermelon
    case 5: veg->tileid=0xfd; veg->color=0xf97205ff; break; // cantaloupe
  }
}

/* Update one particle.
 */
 
static void particle_update(struct battle *battle,struct particle *particle,double elapsed) {

  /* After we've hit a wall, we continue applying (dy) until a counter expires, then commit it.
   * Intent is to prevent all the splatter from landing right on the edge pixel.
   */
  if (particle->shloopclock>0.0) {
    if ((particle->shloopclock-=elapsed)<=0.0) {
      smashing_add_bgpixel(battle,(int)particle->x,(int)particle->y,particle->color);
      particle->defunct=1;
    }
    particle->y+=particle->dy*elapsed;
    return;
  }

  particle->dy+=GRAVITY_ACCEL*elapsed;
  particle->x+=particle->dx*elapsed;
  particle->y+=particle->dy*elapsed;
  
  // Check for binding.
  if (particle->y<GROUNDY) {
    // Too high.
  } else if ((particle->x>=(FBW>>1)-20)&&(particle->x<(FBW>>1)+20)) {
    // In the hole.
  } else {
    // Colliding.
    particle->dx=0.0;
    particle->dy=5.0;
    particle->shloopclock=0.001+((rand()&0xffff)*0.750)/65535.0;
  }
  
  if ((particle->x<0.0)&&(particle->dx<0.0)) particle->defunct=1;
  if ((particle->x>FBW)&&(particle->dx>0.0)) particle->defunct=1;
  if (particle->y>FBH) particle->defunct=1;
}

/* Update.
 */
 
static void _smashing_update(struct battle *battle,double elapsed) {

  /* Generate a vegetable?
   */
  if (battle->outcome==-2) {
    if (!BATTLE->vegetablev[0].vegid) smashing_generate_vegetable(battle,0);
    if (!BATTLE->vegetablev[1].vegid) smashing_generate_vegetable(battle,1);
  }
  
  /* Update vegetables.
   */
  struct vegetable *veg=BATTLE->vegetablev;
  int i=2;
  for (;i-->0;veg++) {
    vegetable_update(battle,veg,elapsed);
  }
  
  /* Update particles, then drop defunct.
   */
  struct particle *particle=BATTLE->particlev;
  for (i=BATTLE->particlec;i-->0;particle++) {
    particle_update(battle,particle,elapsed);
  }
  for (particle=BATTLE->particlev+BATTLE->particlec-1,i=BATTLE->particlec;i-->0;particle--) {
    if (!particle->defunct) continue;
    BATTLE->particlec--;
    memmove(particle,particle+1,sizeof(struct particle)*(BATTLE->particlec-i));
  }
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  for (i=2;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Check for completion.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->score>=SCORE_MAX) {
      if (r->score>=SCORE_MAX) battle->outcome=0; // Shouldn't be possible, except mathematically.
      else battle->outcome=1;
    } else if (r->score>=SCORE_MAX) {
      battle->outcome=-1;
    }
  }
}

/* Render bits.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const struct pose *pose=player->posev+player->posep;
  int dstx=(int)player->x-24;
  int dsty=(int)player->y-48;
  graf_decal_xform(&g.graf,dstx,dsty,pose->srcx,pose->srcy,48,48,player->xform);
}

static void scoreboard_render(struct battle *battle,struct player *player) {
  int x=(FBW>>1);
  if (player->who) x+=10;
  else x-=10;
  int y=GROUNDY-40;
  int i=0;
  for (;i<SCORE_MAX;i++,y-=12) {
    uint32_t color=(i<player->score)?player->color:0x808080ff;
    graf_fancy(&g.graf,x,y,0xde,0,0,NS_sys_tilesize,0,color);
  }
}

static void vegetable_render(struct battle *battle,struct vegetable *veg) {
  if (!veg->vegid) return;
  int x=(int)veg->x;
  int y=(int)veg->y;
  int8_t rot=(int8_t)((veg->t*128.0)/M_PI);
  graf_fancy(&g.graf,x,y,veg->tileid,0,rot,NS_sys_tilesize,0,0x808080ff);
}

/* Render.
 */
 
static void _smashing_render(struct battle *battle) {

  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x80b0d0ff);
  graf_set_input(&g.graf,BATTLE->bgtexid);
  graf_decal(&g.graf,0,FBH-BATTLE->bgh,0,0,BATTLE->bgw,BATTLE->bgh);
  graf_set_image(&g.graf,RID_image_battle_labyrinth2);
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  scoreboard_render(battle,BATTLE->playerv+0);
  scoreboard_render(battle,BATTLE->playerv+1);
  
  struct vegetable *veg=BATTLE->vegetablev;
  int i=2;
  for (;i-->0;veg++) vegetable_render(battle,veg);
  
  graf_set_input(&g.graf,0);
  struct particle *particle=BATTLE->particlev;
  for (i=BATTLE->particlec;i-->0;particle++) {
    if (particle->defunct) continue;
    graf_point(&g.graf,(int)particle->x,(int)particle->y,particle->color);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_smashing={
  .name="smashing",
  .objlen=sizeof(struct battle_smashing),
  .strix_name=174,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_smashing_del,
  .init=_smashing_init,
  .update=_smashing_update,
  .render=_smashing_render,
};

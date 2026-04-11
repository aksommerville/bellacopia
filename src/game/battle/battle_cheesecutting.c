/* battle_cheesecutting.c
 * Drop your guillotine when the flying cheese is under it.
 */

#include "game/bellacopia.h"

#define CHEESE_LIMIT 32
#define SCORE_LIMIT 5 /* Best 3 of 5. */
#define SPLATTER_TIME 0.500
#define PLAN_LIMIT 10

struct battle_cheesecutting {
  struct battle hdr;
  int next_target;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // 2 tiles high, and two first (idle first, then engaged).
    int input; // Nonzero to drop the blade when available. The only thing that man and cpu players set.
    int blackout;
    double droppage; // 0..1 = top..chop
    double droppaged;
    int score;
    // Constant per skill:
    double drop_rate; // Positive.
    double lift_rate; // Negative.
    // CPU player only:
    int plan[PLAN_LIMIT]; // 1=strike, 0=miss. At low skill, it's also possible to miss for other reasons.
    int planp;
  } playerv[2];
  
  struct cheese {
    double x,y;
    double animclock;
    int animframe;
    double dx,dy;
    int defunct;
    struct player *target; // WEAK, OPTIONAL. Only one cheese at a time should have it set.
    int attack_stage; // Only relevant when (target) set. Zeroes when (target) assigned.
    double splatter; // If >0, we're dead and just wrapping up visually.
  } cheesev[CHEESE_LIMIT];
  int cheesec;
};

#define BATTLE ((struct battle_cheesecutting*)battle)

/* Delete.
 */
 
static void _cheesecutting_del(struct battle *battle) {
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
    memset(player->plan,0xff,sizeof(player->plan));
    int missc=(1.0-player->skill)*PLAN_LIMIT;
    if (missc>0) {
      if (missc>=PLAN_LIMIT) missc=PLAN_LIMIT-1;
      while (missc-->0) {
        int panic=100;
        while (panic-->0) {
          int p=rand()%PLAN_LIMIT;
          if (!player->plan[p]) continue;
          player->plan[p]=0;
          break;
        }
      }
    }
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x060718ff;
        player->tileid=0xe4;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0xa4;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xc4;
      } break;
  }
  // 4,-1 feels pretty good. 8,-3 is close to immediate, a good upper limit. 2,-0.5 is tolerable but leaning toward painful.
  const double droplo= 2.000;
  const double drophi= 8.000;
  const double liftlo=-0.500;
  const double lifthi=-3.000;
  player->drop_rate=droplo*(1.0-player->skill)+drophi*player->skill;
  player->lift_rate=liftlo*(1.0-player->skill)+lifthi*player->skill;
  player->blackout=1;
}

/* Spawn a cheese.
 */
 
static struct cheese *spawn_cheese(struct battle *battle) {
  if (BATTLE->cheesec>=CHEESE_LIMIT) return 0;
  struct cheese *cheese=BATTLE->cheesev+BATTLE->cheesec++;
  cheese->x=rand()%FBW;
  cheese->y=rand()%80;
  cheese->dx=((rand()&0xffff)*40.0)/65535.0;
  cheese->dx+=30.0;
  if (rand()&1) cheese->dx=-cheese->dx;
  cheese->dy=(((rand()&0xffff)-32768)*40.0)/65535.0;
  cheese->animclock=((rand()&0xffff)*0.100)/65535.0;
  cheese->animframe=rand()%6;
  cheese->defunct=0;
  cheese->target=0;
  cheese->attack_stage=0;
  cheese->splatter=0.0;
  return cheese;
}

/* New.
 */
 
static int _cheesecutting_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->next_target=rand()&1;
  while (spawn_cheese(battle)) ;
  return 0;
}

/* Look for any cheeses we can cut. Call when the blade reaches bottom.
 */
 
static void check_cut(struct battle *battle,struct player *player) {
  
  /* Check for cheese.
   * It's certainly possible for 2 to be in range, but when that happens we only slice one.
   */
  double focusy=140.0;
  double focusx=(FBW>>1)+(player->who?40:-40);
  struct cheese *cheese=BATTLE->cheesev;
  int i=BATTLE->cheesec;
  for (;i-->0;cheese++) {
    if (cheese->target!=player) continue; // Anything that isn't targetting us can't be anywhere close to the guillohole.
    if (cheese->splatter>0.0) continue;
    double dx=cheese->x-focusx;
    if ((dx<-10.0)||(dx>10.0)) continue;
    double dy=cheese->y-focusy;
    if ((dy<-10.0)||(dy>10.0)) continue;
    bm_sound_pan(RID_sound_slice,player->who?0.250:-0.250);
    cheese->splatter=SPLATTER_TIME;
    player->score++;
    return;
  }
  
  bm_sound_pan(RID_sound_chop,player->who?0.250:-0.250);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(g.input[player->human]&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    if (g.input[player->human]&EGG_BTN_SOUTH) player->input=1;
    else player->input=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Blade in motion, clear input and advance plan.
  if (player->droppage>0.0) {
    if (player->input) {
      player->input=0;
      if (++(player->planp)>=PLAN_LIMIT) player->planp=0;
    }
    return;
  }
  
  /* Find a cheese targetting me and in its charge stage.
   * We cheat a little to detect this, by reading the cheese's internal fields.
   * But it's cool. The things we're examining are also eminently visible to the user, in indirect but obvious ways.
   */
  player->input=0;
  struct cheese *cheese=BATTLE->cheesev;
  int i=BATTLE->cheesec;
  for (;i-->0;cheese++) {
    if (cheese->defunct) continue;
    if (cheese->target!=player) continue;
    if (cheese->attack_stage!=2) continue;
    if (cheese->splatter>0.0) continue;
    
    double drop_time=1.0/player->drop_rate;
    double cheese_speed=90.0;
    if (!player->plan[player->planp]) { // Intentional miss: Grossly overestimate my blade's speed, just like humans do.
      drop_time*=0.250;
    }
    double drop_distance=drop_time*cheese_speed;
    double guillx=(FBW>>1)+(player->who?40:-40);
    double distance;
    if (cheese->dx<0.0) {
      distance=cheese->x-guillx;
    } else {
      distance=guillx-cheese->x;
    }
    if (distance<drop_distance) {
      player->input=1;
      return;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* If the blade is already in motion, it completes its cycle before anything else can happen.
   * This is signalled by droppage>0.
   */
  if (player->droppage>0.0) {
    player->droppage+=player->droppaged*elapsed;
    if (player->droppage<0.0) {
      player->droppage=0.0;
      // Available for the next stroke.
    } else if (player->droppage>1.0) {
      player->droppage=1.0;
      player->droppaged=player->lift_rate;
      check_cut(battle,player);
    }
    return;
  }
  
  /* Not in motion, so if input is nonzero, begin the drop.
   */
  if (player->input) {
    bm_sound_pan(RID_sound_deploy,player->who?0.250:-0.250);
    player->droppage=0.001;
    player->droppaged=player->drop_rate;
  }
}

/* Update cheese.
 */
 
static void cheese_update(struct battle *battle,struct cheese *cheese,double elapsed) {

  if (cheese->splatter>0.0) {
    if ((cheese->splatter-=elapsed)<=0.0) {
      cheese->defunct=1;
    }
    return;
  }

  if ((cheese->animclock-=elapsed)<=0.0) {
    cheese->animclock+=0.100;
    if (++(cheese->animframe)>=6) cheese->animframe=0;
  }
  
  // Most cheeses at most times have no target, and they just fly around up high.
  if (!cheese->target) {
    cheese->x+=cheese->dx*elapsed;
    cheese->y+=cheese->dy*elapsed;
    if ((cheese->x<0.0)&&(cheese->dx<0.0)) cheese->dx*=-1.0;
    else if ((cheese->x>FBW)&&(cheese->dx>0.0)) cheese->dx*=-1.0;
    else if ((cheese->y<0.0)&&(cheese->dy<0.0)) cheese->dy*=-1.0;
    else if ((cheese->y>80.0)&&(cheese->dy>0.0)) cheese->dy*=-1.0;
    
  /* When we have a target:
   *  - First fly toward the middle of the screen.
   *  - Descend to the level of the guilloholes.
   *  - Rush thru it.
   *  - Arc back up to safety.
   * If we make it thru the guillohole and back to safety, we can drop (target).
   */
  } else if (cheese->attack_stage==0) {
    double xlo=(FBW>>1)-10.0;
    double xhi=xlo+20.0;
    if (cheese->x<xlo) {
      if (cheese->dx<0.0) cheese->dx=-cheese->dx;
      cheese->x+=cheese->dx*elapsed;
    } else if (cheese->x>xhi) {
      if (cheese->dx>0.0) cheese->dx=-cheese->dx;
      cheese->x+=cheese->dx*elapsed;
    } else {
      cheese->attack_stage=1;
    }
    
  } else if (cheese->attack_stage==1) {
    double targety=140.0;
    if (cheese->dy<0.0) cheese->dy=-cheese->dy;
    if (cheese->dy<40.0) cheese->dy=40.0; // It's random initially and could be zero, so set a floor.
    cheese->y+=cheese->dy*elapsed;
    if (cheese->y>=targety) cheese->attack_stage=2;
    if (cheese->target==BATTLE->playerv) cheese->dx=-0.001; // just for the transform
    else cheese->dx=0.001;
    
  } else if (cheese->attack_stage==2) {
    double speed=90.0;
    if (cheese->dx<0.0) speed=-speed;
    cheese->x+=speed*elapsed;
    if ((cheese->x<100.0)||(cheese->x>FBW-100.0)) {
      cheese->attack_stage=3;
      cheese->dx=((rand()&0xffff)*40.0)/65535.0;
      cheese->dx+=30.0;
      if (cheese->target==BATTLE->playerv) cheese->dx=-cheese->dx;
      cheese->dy=-cheese->dy;
    }
    
  } else if (cheese->attack_stage==3) {
    cheese->x+=cheese->dx*elapsed;
    cheese->y+=cheese->dy*elapsed;
    if (cheese->y<80.0) cheese->target=0;
  } else {
    cheese->target=0;
  }
}

/* Update.
 */
 
static void _cheesecutting_update(struct battle *battle,double elapsed) {

  /* When expired, keep animating the cheeses, but don't let them target anything, and don't update players.
   */
  if (battle->outcome>-2) {
    struct cheese *cheese=BATTLE->cheesev;
    int i=BATTLE->cheesec;
    for (;i-->0;cheese++) {
      cheese->target=0;
      cheese_update(battle,cheese,elapsed);
    }
    return;
  }
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Update cheeses.
   */
  struct cheese *cheese=BATTLE->cheesev;
  for (i=BATTLE->cheesec;i-->0;cheese++) {
    if (cheese->defunct) continue;
    cheese_update(battle,cheese,elapsed);
  }
  
  /* Drop defunct cheeses, and detect attacker.
   */
  int attackerc=0;
  for (cheese=BATTLE->cheesev+BATTLE->cheesec-1,i=BATTLE->cheesec;i-->0;cheese--) {
    if (cheese->defunct) {
      BATTLE->cheesec--;
      memmove(cheese,cheese+1,sizeof(struct cheese)*(BATTLE->cheesec-i));
    } else if (cheese->target) {
      attackerc++;
    }
  }
  
  /* If we're below the limit, spawn a new one.
   * Just one per frame, since I don't imagine we'll be removing them any faster than that.
   */
  if (BATTLE->cheesec<CHEESE_LIMIT) {
    spawn_cheese(battle);
  }
  
  /* If we're below the attacker target, pick one at random.
   * The target alternates.
   */
  if (BATTLE->cheesec&&(attackerc<3)) {
    struct cheese *attacker=BATTLE->cheesev+rand()%BATTLE->cheesec;
    if (!attacker->target) {
      attacker->target=BATTLE->playerv+BATTLE->next_target;
      attacker->attack_stage=0;
      BATTLE->next_target^=1;
    }
  }

  /* Game ends when somebody reaches 3 points.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int threshold=(SCORE_LIMIT+1)>>1;
  if (l->score>=threshold) {
    if (r->score>=threshold) battle->outcome=0;
    else battle->outcome=1;
  } else if (r->score>=threshold) {
    battle->outcome=-1;
  }
}

/* Render bits.
 */
 
static void player_render_lower(struct battle *battle,struct player *player) {

  int guilx=FBW>>1; // Center of guillotine; we draw it with tiles.
  uint8_t xform=0; // Tiles are oriented for the left side.
  int herox; // Center of hero, also tiles.
  if (player->who) {
    guilx+=40;
    xform=EGG_XFORM_XREV;
    herox=guilx+14;
  } else {
    guilx-=40;
    herox=guilx-14;
  }
  int guily=100;
  int heroy=guily+24;
  
  // Hero.
  uint8_t herotile=player->tileid;
  if ((player->droppage>0.0)&&(player->droppaged>0.0)) herotile+=1;
  graf_tile(&g.graf,herox,heroy,herotile,xform);
  graf_tile(&g.graf,herox,heroy+NS_sys_tilesize,herotile+0x10,xform);
  
  // Back of guillotine.
  graf_tile(&g.graf,guilx,guily+NS_sys_tilesize*0,0x64,xform);
  graf_tile(&g.graf,guilx,guily+NS_sys_tilesize*1,0x74,xform);
  graf_tile(&g.graf,guilx,guily+NS_sys_tilesize*2,0x84,xform);
  graf_tile(&g.graf,guilx,guily+NS_sys_tilesize*3,0x94,xform);
  
  // Blade.
  int bladey=guily+7;
  bladey+=(int)(player->droppage*30.0);
  graf_tile(&g.graf,guilx,bladey,0x66,xform);
}
 
static void player_render_upper(struct battle *battle,struct player *player) {

  int guilx=FBW>>1; // Center of guillotine; we draw it with tiles.
  uint8_t xform=0; // Tiles are oriented for the left side.
  if (player->who) {
    guilx+=40;
    xform=EGG_XFORM_XREV;
  } else {
    guilx-=40;
  }
  int guily=100;
  
  // Front of guillotine. Tiles that line up with the rear.
  graf_tile(&g.graf,guilx,guily+NS_sys_tilesize*0,0x65,xform);
  graf_tile(&g.graf,guilx,guily+NS_sys_tilesize*1,0x75,xform);
  graf_tile(&g.graf,guilx,guily+NS_sys_tilesize*2,0x85,xform);
  graf_tile(&g.graf,guilx,guily+NS_sys_tilesize*3,0x95,xform);
}

static void cheese_render(struct battle *battle,struct cheese *cheese) {
  if (cheese->defunct) return;
  uint8_t tileid=0x67;
  if (cheese->splatter>0.0) {
    int frame=(int)((cheese->splatter*4.0)/SPLATTER_TIME);
    if (frame<0) frame=0;
    else if (frame>3) frame=3;
    tileid=0x6f-frame;
  } else switch (cheese->animframe) {
    case 0: tileid+=0; break;
    case 1: tileid+=1; break;
    case 2: tileid+=2; break;
    case 3: tileid+=3; break;
    case 4: tileid+=2; break;
    case 5: tileid+=1; break;
  }
  graf_tile(&g.graf,(int)cheese->x,(int)cheese->y,tileid,(cheese->dx>0.0)?0:EGG_XFORM_XREV);
}

/* Render.
 */
 
static void _cheesecutting_render(struct battle *battle) {

  /* Background.
   */
  int horizon=140;
  graf_gradient_rect(&g.graf,0,0,FBW,horizon,0x102030ff,0x203040ff,0x60a0c0ff,0x60a0c0ff);
  graf_gradient_rect(&g.graf,0,horizon,FBW,FBH-horizon,0x003000ff,0x003000ff,0x008020ff,0x008020ff);
  
  /* Sprites.
   */
  graf_set_image(&g.graf,RID_image_battle_labor);
  player_render_lower(battle,BATTLE->playerv+0);
  player_render_lower(battle,BATTLE->playerv+1);
  struct cheese *cheese=BATTLE->cheesev;
  int i=BATTLE->cheesec;
  for (;i-->0;cheese++) cheese_render(battle,cheese);
  player_render_upper(battle,BATTLE->playerv+0);
  player_render_upper(battle,BATTLE->playerv+1);
  
  /* Scoreboard.
   */
  int lc=BATTLE->playerv[0].score;
  int rc=BATTLE->playerv[1].score;
  int grayc;
  if (lc+rc>SCORE_LIMIT) { // SCORE_LIMIT must be odd.
    lc=rc=SCORE_LIMIT>>1;
    grayc=1;
  } else {
    grayc=SCORE_LIMIT-rc-lc;
  }
  int scorespacing=NS_sys_tilesize;
  int scorex=(FBW>>1)-(SCORE_LIMIT>>1)*scorespacing;
  int scorey=FBH-10;
  for (i=SCORE_LIMIT;i-->0;scorex+=scorespacing) {
    uint32_t color=0x808080ff;
    if (lc-->0) color=BATTLE->playerv[0].color;
    else if (grayc-->0) color=0x808080ff;
    else color=BATTLE->playerv[1].color;
    graf_fancy(&g.graf,scorex,scorey,0x6b,0,0,NS_sys_tilesize,0,color);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_cheesecutting={
  .name="cheesecutting",
  .objlen=sizeof(struct battle_cheesecutting),
  .strix_name=155,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_cheesecutting_del,
  .init=_cheesecutting_init,
  .update=_cheesecutting_update,
  .render=_cheesecutting_render,
};

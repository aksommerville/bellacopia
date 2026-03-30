/* battle_petrifying.c
 * Look at the knights to turn them into stone.
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

#define SPRITEID_LEFT 1
#define SPRITEID_RIGHT 2

#define PLAYER_SPEED_MIN 4.000
#define PLAYER_SPEED_MAX 8.000
#define SPAWN_LIMIT 8
#define KNIGHT_INTERVAL 1.070 /* Aim to be enharmonic against the animation interval, 0.200 */
#define KNIGHT_SPEED 4.000
#define KNIGHT_TURN_MIN 0.500
#define KNIGHT_TURN_MAX 1.500
#define KNIGHT_BLOCK_TIME 0.333 /* Continue for so long when fully blocked before we accept it, to mitigate jitter. */
#define SPRITE_LIMIT 32 /* Stop spawning when so many afield. In the 60 seconds we're allowed to run, it can get unpleasantly crowded. */
#define PLAY_TIME 20.0 /* Game over when time expires or somebody reaches 9 points. */
#define HIGHLIGHT_LIMIT 8
#define HIGHLIGHT_TIME 0.750

struct battle_petrifying {
  struct battle hdr;
  int choice;
  double lskill,rskill;
  struct batsup_world *world;
  
  struct spawn {
    int x,y;
  } spawnv[SPAWN_LIMIT];
  int spawnc;
  
  int knightc; // How many knights have we spawned total?
  double knightclock;
  
  int scoreboardx,scoreboardy;
  int lscore,rscore;
  double playclock;
  
  struct highlight {
    int ax,ay,bx,by;
    uint32_t rgba;
    double ttl;
  } highlightv[HIGHLIGHT_LIMIT];
  int highlightc;
};

#define BATTLE ((struct battle_petrifying*)battle)

struct sprite_player {
  struct batsup_sprite hdr;
  int who; // 0,1
  int human; // 0=cpu, 1,2=human
  int face;
  int facedx,facedy;
  int walkdx,walkdy;
  uint8_t tileid0;
  double walkspeed; // Constant.
  double animclock;
  int animframe;
  uint32_t color;
  double cpuclock; // On expiry, toggle between moving and holding. Before we start moving, pick a direction.
  double holdmin,holdmax,walkmin,walkmax; // CPU timing.
};

struct sprite_knight {
  struct batsup_sprite hdr;
  uint8_t tileid0;
  int petrified;
  int facedx,facedy; // Also walk direction.
  double animclock;
  int animframe;
  double turnclock;
  double blockclock;
};

/* Delete.
 */
 
static void _petrifying_del(struct battle *battle) {
}

/* Positive distance between (a) and (b), along the line of sight from (a) described by (dx,dy).
 * If negative, (b) is on the wrong side of (a).
 */
 
static double petrifying_cardinal_distance(const struct batsup_sprite *a,const struct batsup_sprite *b,int dx,int dy) {
  if (dx<0) return a->x-b->x;
  if (dx>0) return b->x-a->x;
  if (dy<0) return a->y-b->y;
  return b->y-a->y;
}

/* Update player, generic.
 * The human and CPU controller should call this for each update, with the desired dpad state.
 */
 
static void petrifying_player_dpad(struct batsup_sprite *sprite,double elapsed,int dx,int dy) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  if (sprite->world->battle->outcome>-2) return; // If the first player wins on her turn, skip mine. In case we improbably both score our ninth on the same frame.
  
  /* Input state changed?
   */
  if ((dx!=SPRITE->walkdx)||(dy!=SPRITE->walkdy)) {
    if (dx&&!dy) {
      SPRITE->facedx=dx;
      SPRITE->facedy=0;
    } else if (!dx&&dy) {
      SPRITE->facedx=0;
      SPRITE->facedy=dy;
    } else if (!dx&&!dy) {
      // Changing to still -- don't turn.
    } else if (dx&&!SPRITE->walkdx) {
      // New horizontal motion -- face horizontally.
      SPRITE->facedx=dx;
      SPRITE->facedy=0;
    } else if (dy&&!SPRITE->walkdy) {
      // New vertical motion -- face vertically.
      SPRITE->facedx=0;
      SPRITE->facedy=dy;
    } else if (dx&&SPRITE->facedx&&(dx!=SPRITE->facedx)) {
      // Horizontal crossed zero without touching it -- face horizontally.
      SPRITE->facedx=dx;
      SPRITE->facedy=0;
    } else if (dy&&SPRITE->facedy&&(dy!=SPRITE->facedy)) {
      // Vertical crossed zero without touching it -- face vertically.
      SPRITE->facedx=0;
      SPRITE->facedy=dy;
    }
    SPRITE->walkdx=dx;
    SPRITE->walkdy=dy;
  }
  
  /* Walking?
   */
  if (SPRITE->walkdx||SPRITE->walkdy) {
    if (batsup_sprite_move(sprite,SPRITE->walkdx*SPRITE->walkspeed*elapsed,SPRITE->walkdy*SPRITE->walkspeed*elapsed)) {
      // Moved.
    } else {
      // Fully blocked.
    }
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.200;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
    
  /* Not walking. Reset animation.
   */
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
  
  /* Set approprite tile and transform.
   */
  sprite->tileid=SPRITE->tileid0;
  sprite->xform=0;
  switch (SPRITE->animframe) {
    case 1: sprite->tileid+=1; break;
    case 3: sprite->tileid+=2; break;
  }
  if (SPRITE->facedx<0) {
    sprite->tileid+=6;
    sprite->xform=EGG_XFORM_XREV;
  } else if (SPRITE->facedx>0) {
    sprite->tileid+=6;
  } else if (SPRITE->facedy<0) {
    sprite->tileid+=3;
  }
  
  /* Check for petrification.
   * Only do one per update, and only if our line of sight is uninterrupted.
   * We don't bother checking map cells because it's designed such that there's never a wall they can hide behind.
   */
  const double radius=0.400;
  double pl=sprite->x-radius,pr=sprite->x+radius,pt=sprite->y-radius,pb=sprite->y+radius;
  if (SPRITE->facedx<0) pl=0.0;
  else if (SPRITE->facedx>0) pr=NS_sys_mapw;
  else if (SPRITE->facedy<0) pt=0.0;
  else pb=NS_sys_maph;
  double blockdistance=NS_sys_mapw;
  double victimdistance=blockdistance;
  struct batsup_sprite *victim=0;
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    // Check all solid sprites:
    struct batsup_sprite *other=*otherp;
    if (other==sprite) continue;
    if (other->defunct) continue;
    if (!other->solid) continue;
    
    // Only interested if within line of sight:
    if (other->x<pl) continue;
    if (other->x>pr) continue;
    if (other->y<pt) continue;
    if (other->y>pb) continue;
    
    // If it's petrified or not a knight, it can occlude others. Record the nearest.
    double distance=petrifying_cardinal_distance(sprite,other,SPRITE->facedx,SPRITE->facedy);
    struct sprite_knight *OTHER=(other->tileid>=0xf0)?(struct sprite_knight*)other:0;
    if (!OTHER||OTHER->petrified) {
      if (distance<blockdistance) {
        blockdistance=distance;
      }
      if (distance<victimdistance) {
        victim=0;
      }
      continue;
    }
    if ((distance>=blockdistance)||(distance>=victimdistance)) continue;
    if (!OTHER) continue;
    
    // It's a living knight. Is it looking at me?
    if ((OTHER->facedx!=-SPRITE->facedx)||(OTHER->facedy!=-SPRITE->facedy)) {
      blockdistance=distance;
      if (distance<victimdistance) {
        victim=0;
      }
      continue;
    }
    
    // Record for petrification.
    victim=other;
    victimdistance=distance;
  }
  if (victim&&(victimdistance<blockdistance)) {
    struct sprite_knight *VICTIM=(struct sprite_knight*)victim;
    VICTIM->petrified=1;
    bm_sound(RID_sound_pop);
    struct battle *battle=sprite->world->battle;
    if (SPRITE->who) {
      BATTLE->rscore++;
      if (BATTLE->rscore>=9) battle->outcome=-1;
    } else {
      BATTLE->lscore++;
      if (BATTLE->lscore>=9) battle->outcome=1;
    }
    if (BATTLE->highlightc<HIGHLIGHT_LIMIT) {
      struct highlight *highlight=BATTLE->highlightv+BATTLE->highlightc++;
      const double away=0.400;
      highlight->ax=(int)((sprite->x+SPRITE->facedx*away)*NS_sys_tilesize);
      highlight->ay=(int)((sprite->y+SPRITE->facedy*away)*NS_sys_tilesize)-4;
      highlight->bx=(int)((victim->x+VICTIM->facedx*away)*NS_sys_tilesize);
      highlight->by=(int)((victim->y+VICTIM->facedy*away)*NS_sys_tilesize)-4;
      highlight->rgba=SPRITE->color;
      highlight->ttl=HIGHLIGHT_TIME;
    }
  }
}

/* Update player, human control.
 */
 
static void petrifying_update_player_man(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  int ndx=0,ndy=0;
  switch (g.input[SPRITE->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: ndx=-1; break;
    case EGG_BTN_RIGHT: ndx=1; break;
  }
  switch (g.input[SPRITE->human]&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: ndy=-1; break;
    case EGG_BTN_DOWN: ndy=1; break;
  }
  petrifying_player_dpad(sprite,elapsed,ndx,ndy);
}

/* Direction to the nearest living knight, for CPU decisions.
 * Zero for no opinion.
 * We don't do the exact barrel check, or check for blockages. It's supposed to be imperfect.
 */
 
static uint8_t petrifying_nearest_knight(struct batsup_sprite *sprite) {
  uint8_t bestdir=0;
  double bestdistance=999.999;
  const double radius=2.0; // Must be within this on one axis to qualify.
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other->defunct) continue;
    if (other->tileid<0xf0) continue;
    if (((struct sprite_knight*)other)->petrified) continue;
    double dx=other->x-sprite->x;
    double dy=other->y-sprite->y;
    double adx=(dx<0.0)?-dx:dx;
    double ady=(dy<0.0)?-dy:dy;
    if (adx>ady) { // More horizontal.
      if (ady>=radius) continue; // Too diagonal.
      if (adx>=bestdistance) continue; // Already found a better one.
      bestdistance=adx;
      bestdir=(dx<0.0)?0x10:0x08;
    } else { // More vertical.
      if (adx>=radius) continue; // Too diagonal.
      if (ady>=bestdistance) continue; // Already found a better one.
      bestdistance=ady;
      bestdir=(dy<0.0)?0x40:0x02;
    }
  }
  return bestdir;
}

/* Update player, CPU control.
 */
 
static void petrifying_update_player_cpu(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  int ndx=SPRITE->walkdx,ndy=SPRITE->walkdy;
  
  // Change input state whenever my clock expires.
  if ((SPRITE->cpuclock-=elapsed)<=0.0) {
    // If we were moving, enter a hold. Easy.
    if (ndx||ndy) {
      ndx=ndy=0;
      double n=(rand()&0xffff)/65535.0;
      SPRITE->cpuclock=n*SPRITE->holdmax+(1.0-n)*SPRITE->holdmin;
      
    /* If we were holding, start moving. That means picking a direction first.
     * We turn toward the nearest knight during the hold, so keep whatever we've got there usually.
     * But if we're too far from the center, turn back toward it.
     */
    } else {
      ndx=ndy=0;
      double n=(rand()&0xffff)/65535.0;
      SPRITE->cpuclock=n*SPRITE->walkmax+(1.0-n)*SPRITE->walkmin;
      if (sprite->y<4.0) ndy=1;
      else if (sprite->y>9.0) ndy=-1;
      else if (sprite->x<6.0) ndx=1;
      else if (sprite->x>14.0) ndx=-1;
      else {
        ndx=SPRITE->facedx;
        ndy=SPRITE->facedy;
      }
    }
    
  // But when standing still, turn immediately to face the nearest knight.
  // Don't use the dpad for this, just update (faced) directly.
  } else if (!ndx&&!ndy) {
    switch (petrifying_nearest_knight(sprite)) {
      case 0x40: SPRITE->facedx=0; SPRITE->facedy=-1; break;
      case 0x10: SPRITE->facedx=-1; SPRITE->facedy=0; break;
      case 0x08: SPRITE->facedx=1; SPRITE->facedy=0; break;
      case 0x02: SPRITE->facedx=0; SPRITE->facedy=1; break;
    }
  }

  petrifying_player_dpad(sprite,elapsed,ndx,ndy);
}

/* Choose a new direction for this knight.
 */
 
static void petrifying_knight_random_direction(struct batsup_sprite *sprite) {
  struct sprite_knight *SPRITE=(struct sprite_knight*)sprite;
  
  /* Exclude the current direction, and make the quarter turns more likely than the half turns.
   * No consideration of whether a chosen direction is passable, I don't think we need to care.
   */
  uint8_t current;
  if (SPRITE->facedx<0) current=0x10;
  else if (SPRITE->facedx>0) current=0x08;
  else if (SPRITE->facedy<0) current=0x40;
  else current=0x02;
  uint8_t candidatev[8];
  int candidatec=0;
  if (current!=0x40) candidatev[candidatec++]=0x40;
  if (current!=0x10) candidatev[candidatec++]=0x10;
  if (current!=0x08) candidatev[candidatec++]=0x08;
  if (current!=0x02) candidatev[candidatec++]=0x02;
  if ((current==0x40)||(current==0x02)) {
    candidatev[candidatec++]=0x10;
    candidatev[candidatec++]=0x10;
    candidatev[candidatec++]=0x08;
    candidatev[candidatec++]=0x08;
  } else {
    candidatev[candidatec++]=0x40;
    candidatev[candidatec++]=0x40;
    candidatev[candidatec++]=0x02;
    candidatev[candidatec++]=0x02;
  }
  
  SPRITE->facedx=SPRITE->facedy=0;
  uint8_t ndir=candidatev[rand()%candidatec];
  switch (ndir) {
    case 0x40: SPRITE->facedy=-1; break;
    case 0x10: SPRITE->facedx=-1; break;
    case 0x08: SPRITE->facedx=1; break;
    default: SPRITE->facedy=1; break;
  }
}

/* Nonzero if there's no other sprites uncomfortably close, ie ok to become solid.
 */
 
static int sprite_alone(const struct batsup_sprite *sprite) {
  struct batsup_sprite **otherp=sprite->world->spritev;
  int i=sprite->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    if (other==sprite) continue;
    double dx=sprite->x-other->x;
    if ((dx<-1.0)||(dx>1.0)) continue;
    double dy=sprite->y-other->y;
    if ((dy<-1.0)||(dy>1.0)) continue;
    return 0;
  }
  return 1;
}

/* Update knight.
 */
 
static void petrifying_update_knight(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_knight *SPRITE=(struct sprite_knight*)sprite;
  
  /* If we're petrified, ensure we've got the right tile and do nothing more.
   */
  if (SPRITE->petrified) {
    if (SPRITE->facedx<0) {
      sprite->tileid=SPRITE->tileid0+8;
      sprite->xform=EGG_XFORM_XREV;
    } else if (SPRITE->facedx>0) {
      sprite->tileid=SPRITE->tileid0+8;
      sprite->xform=0;
    } else if (SPRITE->facedy<0) {
      sprite->tileid=SPRITE->tileid0+7;
      sprite->xform=0;
    } else {
      sprite->tileid=SPRITE->tileid0+6;
      sprite->xform=0;
    }
    return;
  }
  
  /* Walk in one direction until we feel like something else or get blocked.
   * Stop cold if battle complete.
   */
  if (sprite->world->battle->outcome==-2) {
    int turn=0;
    if (batsup_sprite_move(sprite,SPRITE->facedx*KNIGHT_SPEED*elapsed,SPRITE->facedy*KNIGHT_SPEED*elapsed)) {
      SPRITE->blockclock=0.0;
    } else if ((SPRITE->blockclock+=elapsed)>=KNIGHT_BLOCK_TIME) {
      turn=1;
    }
    if (SPRITE->turnclock>0.0) {
      if ((SPRITE->turnclock-=elapsed)<=0.0) {
        turn=1;
      }
    }
    if (turn) {
      petrifying_knight_random_direction(sprite);
      double n=(rand()&0xffff)/65535.0;
      SPRITE->turnclock=KNIGHT_TURN_MIN*(1.0-n)+KNIGHT_TURN_MAX*n;
      SPRITE->blockclock=0.0;
    }
  }
  
  /* Become solid once we're fully on screen.
   * This also sets turnclock for the first time.
   */
  if (!sprite->solid) {
    if ((sprite->x>=0.5)&&(sprite->y>=0.5)&&(sprite->x<=NS_sys_mapw-0.5)&&(sprite->y<=NS_sys_maph-0.5)) {
      if (sprite_alone(sprite)) {
        sprite->solid=1;
        double n=(rand()&0xffff)/65535.0;
        SPRITE->turnclock=KNIGHT_TURN_MIN*(1.0-n)+KNIGHT_TURN_MAX*n;
      }
    }
  }
  
  /* Animate and set frame.
   */
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.200;
    if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
  }
  sprite->tileid=SPRITE->tileid0+SPRITE->animframe;
  sprite->xform=0;
  if (SPRITE->facedx<0) { sprite->tileid+=4; sprite->xform=EGG_XFORM_XREV; }
  else if (SPRITE->facedx>0) sprite->tileid+=4;
  else if (SPRITE->facedy<0) sprite->tileid+=2;
}

/* Init player.
 */
 
static int petrifying_spawn_player(struct battle *battle,int col,int row,int who) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,who?SPRITEID_RIGHT:SPRITEID_LEFT,sizeof(struct sprite_player));
  if (!sprite) return -1;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  sprite->x=col+0.5;
  sprite->y=row+0.5;
  sprite->solid=1;
  if (who) {
    SPRITE->who=1;
    SPRITE->human=battle->args.rctl;
    SPRITE->face=battle->args.rface;
    SPRITE->facedx=1;
    SPRITE->walkspeed=PLAYER_SPEED_MIN*(1.0-BATTLE->rskill)+PLAYER_SPEED_MAX*BATTLE->rskill;
  } else {
    SPRITE->who=0;
    SPRITE->human=battle->args.lctl;
    SPRITE->face=battle->args.lface;
    SPRITE->facedx=-1;
    SPRITE->walkspeed=PLAYER_SPEED_MIN*(1.0-BATTLE->lskill)+PLAYER_SPEED_MAX*BATTLE->lskill;
  }
  if (SPRITE->human) {
    sprite->update=petrifying_update_player_man;
  } else {
    sprite->update=petrifying_update_player_cpu;
    double skill=SPRITE->who?BATTLE->rskill:BATTLE->lskill;
    // A more "skilled" CPU player just changes direction more often.
    SPRITE->holdmin=0.500*(1.0-skill)+0.100*skill; // More skill, shorter holds.
    SPRITE->walkmin=0.500*(1.0-skill)+0.100*skill; // More skill, shorter walks.
    SPRITE->holdmax=SPRITE->holdmin*2.0;
    SPRITE->walkmax=SPRITE->walkmin*2.0;
  }
  switch (SPRITE->face) {
    case NS_face_dot: {
        SPRITE->tileid0=0xc0;
        SPRITE->color=0x411775ff;
      } break;
    case NS_face_princess: {
        SPRITE->tileid0=0xd0;
        SPRITE->color=0x0d3ac1ff;
      } break;
    case NS_face_monster: default: {
        SPRITE->tileid0=0xe0;
        SPRITE->color=0x2f725dff;
      } break;
  }
  return 0;
}

/* New.
 */
 
static int _petrifying_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->lskill,&BATTLE->rskill,battle);
  BATTLE->scoreboardx=-1;
  BATTLE->playclock=PLAY_TIME;
  BATTLE->knightc=(rand()&1); // Random starting side.
  
  if (!(BATTLE->world=batsup_world_new(battle,RID_map_petrifying))) return -1;
  
  struct cmdlist_reader reader={.v=BATTLE->world->map->cmd,.c=BATTLE->world->map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_battlemark: {
          int arg=(cmd.arg[2]<<8)|cmd.arg[3];
          switch (arg) {
            case 1: if (petrifying_spawn_player(battle,cmd.arg[0],cmd.arg[1],0)<0) return -1; break;
            case 2: if (petrifying_spawn_player(battle,cmd.arg[0],cmd.arg[1],1)<0) return -1; break;
            case 3: {
                if ((cmd.arg[0]!=0)&&(cmd.arg[1]!=0)&&(cmd.arg[0]!=NS_sys_mapw-1)&&(cmd.arg[1]!=NS_sys_maph-1)) {
                  fprintf(stderr,"%s: Spawn point (%d,%d) in map:%d is not on an edge.\n",__func__,cmd.arg[0],cmd.arg[1],BATTLE->world->map->rid);
                } else if (BATTLE->spawnc>=SPAWN_LIMIT) {
                  fprintf(stderr,"%s: Too many spawn points, limit %d.\n",__func__,SPAWN_LIMIT);
                } else {
                  BATTLE->spawnv[BATTLE->spawnc++]=(struct spawn){cmd.arg[0],cmd.arg[1]};
                }
              } break;
            case 4: {
                BATTLE->scoreboardx=cmd.arg[0];
                BATTLE->scoreboardy=cmd.arg[1];
              } break;
          }
        } break;
    }
  }
  
  return 0;
}

/* Does any sprite exist near this spawn point?
 */
 
static int petrifying_sprite_near_spawn(struct battle *battle,const struct spawn *spawn) {
  const double radius=1.0;
  double l=(double)spawn->x+0.5-radius;
  double t=(double)spawn->y+0.5-radius;
  double r=(double)spawn->x+0.5+radius;
  double b=(double)spawn->y+0.5+radius;
  struct batsup_sprite **spritep=BATTLE->world->spritev;
  int i=BATTLE->world->spritec;
  for (;i-->0;spritep++) {
    struct batsup_sprite *sprite=*spritep;
    if (sprite->defunct) continue;
    if (sprite->x<l) continue;
    if (sprite->x>r) continue;
    if (sprite->y<t) continue;
    if (sprite->y>b) continue;
    return 1;
  }
  return 0;
}

/* Spawn a knight.
 */
 
static void petrifying_spawn_knight(struct battle *battle) {

  if (BATTLE->world->spritec>=SPRITE_LIMIT) return;

  /* Advance knightc whether it works or not.
   * We don't really care about the count; we just use this to choose which half of the screen to spawn from.
   */
  BATTLE->knightc++;
  
  /* Don't select from among all spawn points.
   * Alternate left/right in the interest of fairness.
   * Reject any candidates with any sprite nearby.
   * Even non-solid ones, that would likely be a knight that hasn't got on-screen yet.
   */
  struct spawn *candidatev[SPAWN_LIMIT];
  int candidatec=0;
  struct spawn *spawn=BATTLE->spawnv;
  int i=BATTLE->spawnc;
  if (BATTLE->knightc&1) {
    for (;i-->0;spawn++) {
      if (spawn->x<NS_sys_mapw>>1) continue;
      if (petrifying_sprite_near_spawn(battle,spawn)) continue;
      candidatev[candidatec++]=spawn;
    }
  } else {
    for (;i-->0;spawn++) {
      if (spawn->x>=NS_sys_mapw>>1) continue;
      if (petrifying_sprite_near_spawn(battle,spawn)) continue;
      candidatev[candidatec++]=spawn;
    }
  }
  if (candidatec<1) {
    //fprintf(stderr,"%s: No spawn candidates on %s side.\n",__func__,(BATTLE->knightc&1)?"right":"left");
    return;
  }
  int candidatep=rand()%candidatec;
  spawn=candidatev[candidatep];
  
  /* Make the sprite.
   * Don't make him solid yet. Must get on screen first, because the screen edges are solid.
   */
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,0,sizeof(struct sprite_knight));
  if (!sprite) return;
  struct sprite_knight *SPRITE=(struct sprite_knight*)sprite;
  SPRITE->tileid0=0xf0;
  sprite->update=petrifying_update_knight;
  
  /* The actual start position is one meter offscreen.
   * All spawn points are required to be on an edge; we asserted that at init.
   */
  sprite->x=spawn->x+0.5;
  sprite->y=spawn->y+0.5;
  if (spawn->x<=0) { // Left edge.
    sprite->x-=1.0;
    SPRITE->facedx=1;
  } else if (spawn->x>=NS_sys_mapw-1) { // Right edge.
    sprite->x+=1.0;
    SPRITE->facedx=-1;
  } else if (spawn->y<=0) { // Top edge.
    sprite->y-=1.0;
    SPRITE->facedy=1;
  } else { // Bottom edge.
    sprite->y+=1.0;
    SPRITE->facedy=-1;
  }
}

/* Update.
 */
 
static void _petrifying_update(struct battle *battle,double elapsed) {

  // Update highlights.
  int i=BATTLE->highlightc;
  struct highlight *highlight=BATTLE->highlightv+i-1;
  for (;i-->0;highlight--) {
    if ((highlight->ttl-=elapsed)<=0.0) {
      BATTLE->highlightc--;
      memmove(highlight,highlight+1,sizeof(struct highlight)*(BATTLE->highlightc-i));
    }
  }

  /* Time up?
   * We do continue updating after completion, so be sure to check for that.
   */
  if (battle->outcome==-2) {
    if ((BATTLE->playclock-=elapsed)<=0.0) {
      if (BATTLE->lscore>BATTLE->rscore) battle->outcome=1;
      else if (BATTLE->lscore<BATTLE->rscore) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
  
  /* Spawn a knight when our clock rolls over.
   * Do this before the generic update, so new knights get one update before their first render.
   */
  if (battle->outcome==-2) {
    if ((BATTLE->knightclock-=elapsed)<=0.0) {
      BATTLE->knightclock+=KNIGHT_INTERVAL;
      petrifying_spawn_knight(battle);
    }
  }
  
  batsup_world_update(BATTLE->world,elapsed);
}

/* Render.
 */
 
static void _petrifying_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
  
  if (BATTLE->scoreboardx>=0) {
    graf_set_image(&g.graf,RID_image_battle_labyrinth3);
    uint8_t tile0=0x35; // Left score.
    uint8_t tile1=0x45; // Time tens.
    uint8_t tile2=0x45; // Time ones.
    uint8_t tile3=0x35; // Right score.
    if (BATTLE->lscore>=9) tile0+=9; else tile0+=BATTLE->lscore;
    if (BATTLE->rscore>=9) tile3+=9; else tile3+=BATTLE->rscore;
    int sec=(int)(BATTLE->playclock+0.999); // Round up. Game ends when it strikes zero.
    if (sec<0) sec=0; else if (sec>99) sec=99;
    if (sec>=10) tile1+=sec/10; else tile1=0;
    tile2+=sec%10;
    int dstx=((FBW>>1)-((NS_sys_tilesize*NS_sys_mapw)>>1))+(NS_sys_tilesize>>1);
    int dsty=((FBH>>1)-((NS_sys_tilesize*NS_sys_maph)>>1))+(NS_sys_tilesize>>1);
    dstx+=BATTLE->scoreboardx*NS_sys_tilesize;
    dsty+=BATTLE->scoreboardy*NS_sys_tilesize;
    graf_tile(&g.graf,dstx,dsty,tile0,0);
    if (tile1) graf_tile(&g.graf,dstx,dsty,tile1,0);
    graf_tile(&g.graf,dstx+ 4,dsty,tile2,0);
    graf_tile(&g.graf,dstx+17,dsty,tile3,0);
  }
  
  graf_set_input(&g.graf,0);
  struct highlight *highlight=BATTLE->highlightv;
  int i=BATTLE->highlightc;
  for (;i-->0;highlight++) {
    uint32_t rgba=highlight->rgba&0xffffff00;
    int alpha=(int)((highlight->ttl*255.0)/HIGHLIGHT_TIME);
    if (alpha<0) alpha=0; else if (alpha>0xff) alpha=0xff;
    rgba|=alpha;
    graf_line(&g.graf,highlight->ax,highlight->ay,rgba,highlight->bx,highlight->by,rgba);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_petrifying={
  .name="petrifying",
  .objlen=sizeof(struct battle_petrifying),
  .strix_name=176,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .del=_petrifying_del,
  .init=_petrifying_init,
  .update=_petrifying_update,
  .render=_petrifying_render,
};

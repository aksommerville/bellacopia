/* battle_pushing.c
 * Push a statue on to the target. Think Sokoban, but there probably won't be any obstacles.
 *
 * Our sprites are not solid. Physics are tied closely to the game model, so they happen at grid level, not in continuous space.
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

#define SPRITEID_LEFT 1
#define SPRITEID_RIGHT 2
#define SPRITEID_LSTATUE 3
#define SPRITEID_RSTATUE 4

#define DISTANCE_EASIEST 3
#define DISTANCE_HARDEST 8 /* 11 is exactly the distance to the far corner, can't go higher. */
#define STATUE_SPEED 8.0 /* Should be faster than players. */
#define VICTORY_DELAY 0.500
#define CPU_PENALTY 0.500 /* CPU players are always substantially slower than humans. */
#define TIME_LIMIT 15.0 /* We can be rendered unsolvable, and sometimes CPU players get stuck. So, aggressive timeout. */

struct battle_pushing {
  struct battle hdr;
  struct batsup_world *world;
  int lx,ly,rx,ry; // Goal positions, which are also start positions.
  double timer;
};

#define BATTLE ((struct battle_pushing*)battle)

struct sprite_player {
  struct batsup_sprite hdr;
  int who; // 0,1
  int human; // 0,1,2
  int face;
  uint8_t tileid0;
  int cx,cy; // Canonical position.
  double skill;
  int indx,indy;
  int pvindx,pvindy;
  int facedx,facedy;
  int walking;
  int pushing;
  double animclock;
  int animframe;
  double walkspeed;
  double pushdelay;
  double pushclock;
  double waitclock;
  double reconsider;
};

struct sprite_statue {
  struct batsup_sprite hdr;
  int cx,cy; // Canonical position.
  double stilltime;
};

/* Delete.
 */
 
static void _pushing_del(struct battle *battle) {
  batsup_world_del(BATTLE->world);
}

/* Is the given cell ok to walk onto?
 */
 
static void pushing_get_sprite_position(int *x,int *y,const struct batsup_sprite *sprite) {
  *x=*y=-1;
  if (!sprite) return;
  switch (sprite->id) {
    case SPRITEID_LEFT:
    case SPRITEID_RIGHT: {
        *x=((struct sprite_player*)sprite)->cx;
        *y=((struct sprite_player*)sprite)->cy;
      } break;
    case SPRITEID_LSTATUE:
    case SPRITEID_RSTATUE: {
        *x=((struct sprite_statue*)sprite)->cx;
        *y=((struct sprite_statue*)sprite)->cy;
      } break;
  }
}
 
static int pushing_move_ok(struct battle *battle,struct batsup_sprite *sprite,int x,int y) {
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return 0;
  uint8_t tileid=BATTLE->world->map->v[y*NS_sys_mapw+x];
  uint8_t physics=BATTLE->world->map->physics[tileid];
  if (physics!=NS_physics_vacant) return 0;
  struct batsup_sprite **otherp=BATTLE->world->spritev;
  int i=BATTLE->world->spritec;
  for (;i-->0;otherp++) {
    struct batsup_sprite *other=*otherp;
    int ox,oy;
    pushing_get_sprite_position(&ox,&oy,other);
    if ((ox==x)&&(oy==y)) return 0;
  }
  return 1;
}

/* For a hero at (x,y), can he push something in the direction of (dx,dy)?
 * Effect the movement and return nonzero if so.
 */
 
static int pushing_deliver_push(struct battle *battle,int x,int y,int dx,int dy) {
  int pumpkinx=x+dx;
  int pumpkiny=y+dy;
  if ((pumpkinx<0)||(pumpkinx>=NS_sys_mapw)) return 0;
  if ((pumpkiny<0)||(pumpkiny>=NS_sys_maph)) return 0;
  struct batsup_sprite *pumpkin=0; // statue only
  struct sprite_statue *PUMPKIN=0;
  struct batsup_sprite **spritep=BATTLE->world->spritev;
  int i=BATTLE->world->spritec;
  for (;i-->0;spritep++) {
    struct batsup_sprite *sprite=*spritep;
    if ((sprite->id!=SPRITEID_LSTATUE)&&(sprite->id!=SPRITEID_RSTATUE)) continue;
    PUMPKIN=(struct sprite_statue*)sprite;
    if ((PUMPKIN->cx!=pumpkinx)||(PUMPKIN->cy!=pumpkiny)) {
      PUMPKIN=0;
      continue;
    }
    pumpkin=sprite;
    break;
  }
  if (!pumpkin) return 0;
  if (!pushing_move_ok(battle,pumpkin,pumpkinx+dx,pumpkiny+dy)) return 0;
  PUMPKIN->cx+=dx;
  PUMPKIN->cy+=dy;
  PUMPKIN->stilltime=0.0;
  bm_sound(RID_sound_push);
  return 1;
}

/* Update player, either control mode.
 */
 
static void pushing_update_player_common(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  struct battle *battle=sprite->world->battle;
  
  /* Game over, we do continue updating. But just set a neutral face and don't move.
   */
  if (battle->outcome>-2) {
    SPRITE->walking=0;
    SPRITE->pushing=0;
    return;
  }
  
  /* If input changed, update our face immediately.
   */
  if (SPRITE->indx!=SPRITE->pvindx) {
    if (SPRITE->indx) {
      SPRITE->facedx=SPRITE->indx;
      SPRITE->facedy=0;
    } else if (SPRITE->indy) {
      SPRITE->facedx=0;
      SPRITE->facedy=SPRITE->indy;
    }
    SPRITE->pvindx=SPRITE->indx;
  }
  if (SPRITE->indy!=SPRITE->pvindy) {
    if (SPRITE->indy) {
      SPRITE->facedx=0;
      SPRITE->facedy=SPRITE->indy;
    } else if (SPRITE->indx) {
      SPRITE->facedx=SPRITE->indx;
      SPRITE->facedy=0;
    }
    SPRITE->pvindy=SPRITE->indy;
  }
  
  /* If we're walking, approach the target.
   */
  if (SPRITE->walking) {
    double tx=SPRITE->cx+0.5;
    double ty=SPRITE->cy+0.5;
    int xok=1,yok=1;
    if (sprite->x<tx) {
      if ((sprite->x+=SPRITE->walkspeed*elapsed)>=tx) sprite->x=tx;
      else xok=0;
    } else if (sprite->x>tx) {
      if ((sprite->x-=SPRITE->walkspeed*elapsed)<=tx) sprite->x=tx;
      else xok=0;
    }
    if (sprite->y<ty) {
      if ((sprite->y+=SPRITE->walkspeed*elapsed)>=ty) sprite->y=ty;
      else yok=0;
    } else if (sprite->y>ty) {
      if ((sprite->y-=SPRITE->walkspeed*elapsed)<=ty) sprite->y=ty;
      else yok=0;
    }
    // Reached the target?
    if (xok&&yok) {
      SPRITE->walking=0;
    }
  }
  
  /* Not walking, maybe newly reached the target, proceed to the next cell if input matches face.
   */
  if (!SPRITE->walking&&(SPRITE->indx||SPRITE->indy)) {
    if ((SPRITE->indx==SPRITE->facedx)&&(SPRITE->indy==SPRITE->facedy)) {
      if (pushing_move_ok(battle,sprite,SPRITE->cx+SPRITE->indx,SPRITE->cy+SPRITE->indy)) {
        SPRITE->walking=1;
        SPRITE->cx+=SPRITE->indx;
        SPRITE->cy+=SPRITE->indy;
      } else if (!SPRITE->pushing) {
        SPRITE->pushing=1;
        SPRITE->pushclock=SPRITE->pushdelay;
      }
    }
  }
  
  /* Drop pushing if inputs zero.
   */
  if (SPRITE->pushing&&!SPRITE->indx&&!SPRITE->indy) {
    SPRITE->pushing=0;
  }
  
  /* Advance pushclock if pushing.
   * When it expires, try to deliver the push.
   */
  if (SPRITE->pushing) {
    if ((SPRITE->pushclock-=elapsed)<=0.0) {
      if (pushing_deliver_push(battle,SPRITE->cx,SPRITE->cy,SPRITE->facedx,SPRITE->facedy)) {
        SPRITE->pushing=0;
      }
    }
  }
  
  /* Animate if walking.
   */
  if (SPRITE->walking) {
    if ((SPRITE->animclock-=elapsed)<=0.0) {
      SPRITE->animclock+=0.200;
      if (++(SPRITE->animframe)>=4) SPRITE->animframe=0;
    }
  } else {
    SPRITE->animclock=0.0;
    SPRITE->animframe=0;
  }
}

/* Update player, human.
 * Just update (indx,indy) and call pushing_update_player_common().
 */
 
static void pushing_update_player_man(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  struct battle *battle=sprite->world->battle;
  switch (g.input[SPRITE->human]&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: SPRITE->indx=-1; break;
    case EGG_BTN_RIGHT: SPRITE->indx=1; break;
    default: SPRITE->indx=0; break;
  }
  switch (g.input[SPRITE->human]&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: SPRITE->indy=-1; break;
    case EGG_BTN_DOWN: SPRITE->indy=1; break;
    default: SPRITE->indy=0; break;
  }
  pushing_update_player_common(sprite,elapsed);
}

/* Update player, CPU.
 */
 
struct option {
  int dx,dy;
  int weight;
};

static int pushing_weigh_option(
  struct option *option,
  struct battle *battle,
  int spritex,int spritey,int statuex,int statuey,int targetx,int targety
) {
  // Idle always weighs zero.
  if (!option->dx&&!option->dy) return 0;
  int nx=spritex+option->dx;
  int ny=spritey+option->dy;
  // It won't be possible to reach the edges, but if we do, they're negative.
  if ((nx<0)||(ny<0)||(nx>=NS_sys_mapw)||(ny>=NS_sys_maph)) return -1;
  // Likewise if it's impassable via the map.
  if (BATTLE->world->map->physics[BATTLE->world->map->v[ny*NS_sys_mapw+nx]]==NS_physics_solid) return -1;
  // If the new position coincides with the statue, it's either very good or very bad.
  // Good if it moves the statue toward the goal.
  if ((nx==statuex)&&(ny==statuey)) {
    //TODO A more robust implementation would check whether the statue is able to move that way.
    if (option->dx<0) return (statuex>targetx)?100:-100;
    if (option->dx>0) return (statuex<targetx)?100:-100;
    if (option->dy<0) return (statuey>targety)?100:-100;
    return (statuey<targety)?100:-100;
  }
  // Other player on that cell, it's negative. Do allow pushing the wrong statue.
  struct batsup_sprite **spritep=BATTLE->world->spritev;
  int i=BATTLE->world->spritec;
  for (;i-->0;spritep++) {
    struct batsup_sprite *sprite=*spritep;
    int sx,sy;
    pushing_get_sprite_position(&sx,&sy,sprite);
    if ((sx==nx)&&(sy==ny)) {
      if ((sprite->id==SPRITEID_LEFT)||(sprite->id==SPRITEID_RIGHT)) return -1;
    }
  }
  // Identify the point outside the statue by one meter. That's where we want to be.
  int outsidex=statuex;
  int outsidey=statuey;
  if (statuex<targetx) outsidex--;
  else if (statuex>targetx) outsidex++;
  if (statuey<targety) outsidey--;
  else if (statuey>targety) outsidey++;
  // If it points away from the outside point, it's valid but undesirable.
  if ((option->dx<0)&&(spritex<outsidex)) return 1;
  if ((option->dx>0)&&(spritex>outsidex)) return 1;
  if ((option->dy<0)&&(spritey<outsidey)) return 1;
  if ((option->dy>0)&&(spritey>outsidey)) return 1;
  // Otherwise, there are a few positions outside of the statue that we want to reach.
  int xdistance=nx-outsidex;
  int ydistance=ny-outsidey;
  if (xdistance<0) xdistance=-xdistance;
  if (ydistance<0) ydistance=-ydistance;
  return (NS_sys_mapw-xdistance)+(NS_sys_maph-ydistance);
}
 
static void pushing_update_player_cpu(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  struct battle *battle=sprite->world->battle;
  
  /* If (reconsider) expires, drop whatever we're doing.
   */
  if ((SPRITE->reconsider>0.0)&&((SPRITE->reconsider-=elapsed)<=0.0)) {
    SPRITE->indx=0;
    SPRITE->indy=0;
    pushing_update_player_common(sprite,elapsed);
    return;
  }
  
  /* If our waitclock is set, something went wrong and we're killing time until the opponent puts us out of our misery.
   */
  if (SPRITE->waitclock>0.0) {
    SPRITE->waitclock-=elapsed;
    SPRITE->indx=0;
    SPRITE->indy=0;
  
  /* If we are currently pushing, keep on keeping on, until the statue moves.
   * Or until the wall moves, if we messed something up.
   */
  } else if (SPRITE->pushing) {
  
  /* If we are currently walking, set dpad zero and wait to land at the next cell.
   */
  } else if (SPRITE->walking) {
    SPRITE->indx=0;
    SPRITE->indy=0;
  
  /* Choose our next play.
   */
  } else {
    SPRITE->indx=0;
    SPRITE->indy=0;
    SPRITE->reconsider=1.500; // No matter what, after so long we'll stop trying. eg pushing statue into a wall.
    
    // Take quantized positions of ourself, the statue, and the target.
    int spritex=(int)sprite->x;
    int spritey=(int)sprite->y;
    int targetx,targety,statuex,statuey;
    struct batsup_sprite *statue=0;
    if (SPRITE->who) {
      targetx=BATTLE->rx;
      targety=BATTLE->ry;
      statue=batsup_sprite_by_id(BATTLE->world,SPRITEID_RSTATUE);
    } else {
      targetx=BATTLE->lx;
      targety=BATTLE->ly;
      statue=batsup_sprite_by_id(BATTLE->world,SPRITEID_LSTATUE);
    }
    if (!statue) {
      statuex=targetx;
      statuey=targety;
    } else {
      statuex=(int)statue->x;
      statuey=(int)statue->y;
    }
    //fprintf(stderr,"%s: Next step. player=%d,%d statue=%d,%d target=%d,%d\n",__func__,spritex,spritey,statuex,statuey,targetx,targety);
    
    /* There's four things we might be able to do, the four cardinal directions.
     * Record each option and give it a weight.
     * So any deleterious option should be negative.
     */
    struct option optionv[4];
    int optionc=0;
    optionv[optionc++]=(struct option){-1,0,0};
    optionv[optionc++]=(struct option){1,0,0};
    optionv[optionc++]=(struct option){0,-1,0};
    optionv[optionc++]=(struct option){0,1,0};
    struct option *option=optionv;
    int i=optionc;
    for (;i-->0;option++) {
      option->weight=pushing_weigh_option(option,battle,spritex,spritey,statuex,statuey,targetx,targety);
      //fprintf(stderr,"...option %+d,%+d, weight=%d\n",option->dx,option->dy,option->weight);
    }
    
    /* Eliminate options with negative weight.
     * Identify the lightest positive weight.
     */
    int wmin=INT_MAX;
    for (i=optionc,option=optionv+optionc-1;i-->0;option--) {
      if (option->weight<=0) {
        optionc--;
        memmove(option,option+1,sizeof(struct option)*(optionc-i));
      } else if (option->weight<wmin) {
        wmin=option->weight;
      }
    }
    
    /* If there's no positive options, set our waitclock.
     */
    if (wmin>=INT_MAX) {
      SPRITE->waitclock=1.000;
      
    } else {
    
      /* Reduce weights such that the lightest becomes 1, then square it.
       * This serves to accentuate the more likely options.
       * Collect the new sum.
       */
      //fprintf(stderr,"  options after adjustment:\n");
      int wsum=0;
      wmin--;
      for (option=optionv,i=optionc;i-->0;option++) {
        option->weight-=wmin;
        option->weight*=option->weight;
        wsum+=option->weight;
        //fprintf(stderr,"    %+d,%+d weight=%d\n",option->dx,option->dy,option->weight);
      }
      
      /* Pick an option at random based on their weights.
       * "Push statue toward the goal" weighs a lot more than any regular walking option,
       * so it should be very rare for the golem to abandon a statue once started.
       * But possible! And I like the noisiness of that.
       */
      int choice=rand()%wsum;
      for (option=optionv,i=optionc;i-->0;option++) {
        choice-=option->weight;
        if (choice<0) {
          SPRITE->indx=option->dx;
          SPRITE->indy=option->dy;
          break;
        }
      }
    }
  }
  
  pushing_update_player_common(sprite,elapsed);
}

/* Render player.
 */
 
static void pushing_render_player(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  struct battle *battle=sprite->world->battle;
  uint8_t tileid=sprite->tileid,xform=0;
  if (SPRITE->pushing) {
    tileid+=0x30;
  } else if (SPRITE->walking) {
    switch (SPRITE->animframe) {
      case 1: tileid+=0x10; break;
      case 3: tileid+=0x20; break;
    }
  }
  if (SPRITE->facedx<0) { tileid+=2; xform=EGG_XFORM_XREV; }
  else if (SPRITE->facedx>0) tileid+=2;
  else if (SPRITE->facedy<0) tileid+=1;
  graf_tile(&g.graf,dstx,dsty,tileid,xform);
}

/* Update statue.
 */
 
static void pushing_update_statue(struct batsup_sprite *sprite,double elapsed) {
  struct sprite_statue *SPRITE=(struct sprite_statue*)sprite;
  struct battle *battle=sprite->world->battle;
  double targetx=SPRITE->cx+0.5;
  double targety=SPRITE->cy+0.5;
  int moved=0;
  if (sprite->x<targetx) {
    if ((sprite->x+=STATUE_SPEED*elapsed)>=targetx) sprite->x=targetx; else moved=1;
  } else if (sprite->x>targetx) {
    if ((sprite->x-=STATUE_SPEED*elapsed)<=targetx) sprite->x=targetx; else moved=1;
  }
  if (sprite->y<targety) {
    if ((sprite->y+=STATUE_SPEED*elapsed)>=targety) sprite->y=targety; else moved=1;
  } else if (sprite->y>targety) {
    if ((sprite->y-=STATUE_SPEED*elapsed)<=targety) sprite->y=targety; else moved=1;
  }
  if (moved) {
    SPRITE->stilltime=0.0;
  } else {
    SPRITE->stilltime+=elapsed;
  }
}

/* Render statue.
 */
 
static void pushing_render_statue(struct batsup_sprite *sprite,int dstx,int dsty) {
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  struct battle *battle=sprite->world->battle;
  graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
  graf_tile(&g.graf,dstx,dsty-NS_sys_tilesize,sprite->tileid-0x10,sprite->xform);
}

/* Spawn player.
 */
 
static struct batsup_sprite *pushing_spawn_player(struct battle *battle,int x,int y,int spriteid,double skill) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,spriteid,sizeof(struct sprite_player));
  if (!sprite) return 0;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  sprite->render=pushing_render_player;
  sprite->x=x+0.5;
  sprite->y=y+0.5;
  SPRITE->cx=x;
  SPRITE->cy=y;
  SPRITE->skill=skill;
  SPRITE->walkspeed=6.0; //TODO per skill?
  SPRITE->pushdelay=0.125; // TODO per skill?
  if (spriteid==SPRITEID_LEFT) {
    SPRITE->who=0;
    SPRITE->human=battle->args.lctl;
    SPRITE->face=battle->args.lface;
  } else {
    SPRITE->who=1;
    SPRITE->human=battle->args.rctl;
    SPRITE->face=battle->args.rface;
  }
  if (SPRITE->human) {
    sprite->update=pushing_update_player_man;
  } else {
    sprite->update=pushing_update_player_cpu;
    SPRITE->walkspeed*=CPU_PENALTY;
  }
  switch (SPRITE->face) {
    case NS_face_dot: {
        sprite->tileid=0x80;
      } break;
    case NS_face_princess: {
        sprite->tileid=0x83;
      } break;
    case NS_face_monster: default: {
        sprite->tileid=0x86;
      } break;
  }
  return sprite;
}

/* Spawn a statue. We pick its location.
 */
 
static int vacant(struct battle *battle,int x,int y) {
  if (x<0) return 0;
  if (y<0) return 0;
  if (x>=NS_sys_mapw) return 0;
  if (y>=NS_sys_maph) return 0;
  uint8_t tileid=BATTLE->world->map->v[y*NS_sys_mapw+x];
  if (tileid) return 0;
  return 1;
}

static int spritefree(struct battle *battle,int x,int y) {
  struct batsup_sprite **spritep=BATTLE->world->spritev;
  int i=BATTLE->world->spritec;
  for (;i-->0;spritep++) {
    struct batsup_sprite *sprite=*spritep;
    int sx=(int)sprite->x;
    if (sx!=x) continue;
    int sy=(int)sprite->y;
    if (sy!=y) continue;
    return 0;
  }
  return 1;
}
 
static struct batsup_sprite *pushing_spawn_statue(struct battle *battle,struct batsup_sprite *player) {
  if (!player) return 0;
  struct sprite_player *PLAYER=(struct sprite_player*)player;
  
  /* Place it at a distance proportionate to player's skill.
   */
  int distance=(int)(DISTANCE_EASIEST*PLAYER->skill+DISTANCE_HARDEST*(1.0-PLAYER->skill));
  if (distance<DISTANCE_EASIEST) distance=DISTANCE_EASIEST;
  else if (distance>DISTANCE_HARDEST) distance=DISTANCE_HARDEST;
  int x0=(int)player->x;
  int y0=(int)player->y;
  
  /* Cells at a given Manhattan distance describe a diamond.
   * Iterate those and build up a list of candidate positions.
   * There will never be more than 12 candidates, let's say 16 in case I'm wrong.
   * To simplify plan generation, I'm also forbidding cardinals.
   */
  #define CANDIDATE_LIMIT 16
  struct candidate { int x,y; } candidatev[CANDIDATE_LIMIT];
  int candidatec=0;
  #define CHECKCELL(_x,_y) { \
    int x=(_x),y=(_y); \
    if (candidatec>=CANDIDATE_LIMIT) break; \
    if ( \
      vacant(battle,x,y)&& \
      vacant(battle,x-1,y)&& \
      vacant(battle,x+1,y)&& \
      vacant(battle,x,y-1)&& \
      vacant(battle,x,y+1)&& \
      spritefree(battle,x,y) \
    ) { \
      candidatev[candidatec++]=(struct candidate){x,y}; \
    } \
  }
  int dx=-distance;
  for (;dx<=distance;dx++) {
    int adx=(dx<0)?-dx:dx;
    int ady=distance-adx;
    if (!adx||!ady) continue; // Must be at least a little bit diagonal.
    CHECKCELL(x0+dx,y0-ady)
    if (ady) CHECKCELL(x0+dx,y0+ady)
  }
  #undef CHECKCELL
  #undef CANDIDATE_LIMIT
  if (!candidatec) {
    fprintf(stderr,"%s: Panic! No statue candidate positions found for player at %d,%d\n",__func__,x0,y0);
    return 0;
  }
  int candidatep=rand()%candidatec;
  int x=candidatev[candidatep].x;
  int y=candidatev[candidatep].y;
  
  /* Spawn it.
   */
  int spriteid=(player->id==SPRITEID_LEFT)?SPRITEID_LSTATUE:SPRITEID_RSTATUE;
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,spriteid,sizeof(struct sprite_statue));
  if (!sprite) return 0;
  struct sprite_statue *SPRITE=(struct sprite_statue*)sprite;
  sprite->update=pushing_update_statue;
  sprite->render=pushing_render_statue;
  sprite->x=x+0.5;
  sprite->y=y+0.5;
  SPRITE->cx=x;
  SPRITE->cy=y;
  switch (PLAYER->face) {
    case NS_face_dot: {
        sprite->tileid=0x74;
      } break;
    case NS_face_princess: {
        sprite->tileid=0x75;
      } break;
    case NS_face_monster: default: {
        sprite->tileid=0x76;
      } break;
  }
  sprite->xform=(rand()&1)?EGG_XFORM_XREV:0;
  
  return sprite;
}

/* New.
 */
 
static int _pushing_init(struct battle *battle) {
  double lskill,rskill;
  battle_normalize_bias(&lskill,&rskill,battle);
  if (!(BATTLE->world=batsup_world_new(battle,RID_map_pushing))) return -1;
  struct cmdlist_reader reader={.v=BATTLE->world->map->cmd,.c=BATTLE->world->map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_battlemark) {
      int x=cmd.arg[0];
      int y=cmd.arg[1];
      switch ((cmd.arg[2]<<8)|cmd.arg[3]) {
        case SPRITEID_LEFT: if (!(pushing_spawn_player(battle,x,y,SPRITEID_LEFT,lskill))) return -1; BATTLE->lx=x; BATTLE->ly=y; break;
        case SPRITEID_RIGHT: if (!(pushing_spawn_player(battle,x,y,SPRITEID_RIGHT,rskill))) return -1; BATTLE->rx=x; BATTLE->ry=y; break;
      }
    }
  }
  struct batsup_sprite *playerl=0,*playerr=0,*statuel=0,*statuer=0;
  if (!(statuel=pushing_spawn_statue(battle,playerl=batsup_sprite_by_id(BATTLE->world,SPRITEID_LEFT)))) return -1;
  if (!(statuer=pushing_spawn_statue(battle,playerr=batsup_sprite_by_id(BATTLE->world,SPRITEID_RIGHT)))) return -1;
  if (!playerl||!playerr||!statuel||!statuer) {
    fprintf(stderr,"%s: Should have ended up with 4 sprites, but have %d.\n",__func__,BATTLE->world->spritec);
    return -1;
  }
  BATTLE->timer=TIME_LIMIT;
  return 0;
}

/* Update.
 */
 
static void _pushing_update(struct battle *battle,double elapsed) {
  
  /* Do continue updating sprites after finish.
   * If a statue was moving, it's weird for it to just stop cold.
   */
  batsup_world_update(BATTLE->world,elapsed);
  
  if (battle->outcome>-2) return;
  
  if ((BATTLE->timer-=elapsed)<=0.0) {
    battle->outcome=0;
    return;
  }
  
  /* Victory when a statue is on its goal and its (stilltime) crosses some short threshold.
   * If they're both on goal at that crossing, it's a tie.
   */
  struct batsup_sprite *lstatue=batsup_sprite_by_id(BATTLE->world,SPRITEID_LSTATUE);
  struct batsup_sprite *rstatue=batsup_sprite_by_id(BATTLE->world,SPRITEID_RSTATUE);
  if (!lstatue||!rstatue) { // Error. Call it a tie.
    battle->outcome=0;
  } else {
    struct sprite_statue *LSTATUE=(struct sprite_statue*)lstatue;
    struct sprite_statue *RSTATUE=(struct sprite_statue*)rstatue;
    int longoal=0,rongoal=0;
    if ((LSTATUE->cx==BATTLE->lx)&&(LSTATUE->cy==BATTLE->ly)) longoal=1;
    if ((RSTATUE->cx==BATTLE->rx)&&(RSTATUE->cy==BATTLE->ry)) rongoal=1;
    if (longoal&&(LSTATUE->stilltime>=VICTORY_DELAY)) {
      battle->outcome=rongoal?0:1;
    } else if (rongoal&&(RSTATUE->stilltime>=VICTORY_DELAY)) {
      battle->outcome=longoal?0:-1;
    }
  }
}

/* Render.
 */
 
static void _pushing_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
  
  /* Timer.
   */
  int sec=(int)(BATTLE->timer+0.999);
  if (sec>99) sec=99; else if (sec<0) sec=0;
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_tile(&g.graf,(FBW>>1)-4,6,'0'+sec/10,0);
  graf_tile(&g.graf,(FBW>>1)+4,6,'0'+sec%10,0);
}

/* Type definition.
 */
 
const struct battle_type battle_type_pushing={
  .name="pushing",
  .objlen=sizeof(struct battle_pushing),
  .strix_name=175,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_pushing_del,
  .init=_pushing_init,
  .update=_pushing_update,
  .render=_pushing_render,
};

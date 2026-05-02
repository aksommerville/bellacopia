/* battle_plumbing.c
 * Mini Pipe Dream.
 */

#include "game/bellacopia.h"

#define COLC 8
#define ROWC 8

#define INTAKE_LIMIT 8
#define UPNEXT_SIZE 8
#define UPNEXT_SLIDE_TIME 0.400
#define DELETE_COOLDOWN 0.250
#define OUTPUTY_MIN 2

#define CELLBIT_MAIN 0x10 /* Water flowing thru the main pipe. Horizontal, for the crosspipe. */
#define CELLBIT_AUX  0x20 /* '' vertical, crosspipe only. */

struct battle_plumbing {
  struct battle hdr;
  double wateranimclock;
  int wateranimframe;
  
  uint8_t map[COLC*ROWC]; // Low 4 are (EGG_BTN_LEFT,RIGHT,UP,DOWN). High 4 are CELLBIT_*.
  
  struct intake {
    int x;
    int flow;
  } intakev[INTAKE_LIMIT];
  int intakec;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint8_t tileid;
    int x,y; // 0..COLC,ROWC-1
    uint32_t color;
    double animclock;
    int animframe;
    uint8_t upnext[UPNEXT_SIZE];
    int upnextp;
    double upnext_slide; // s, counts down
    double delete; // 0..1 = not deleting .. win
    double delete_decay; // hz
    double delete_step; // 0..1 per stroke
    double delete_cooldown; // Counts down after a successful deletion; no keystrokes during this window.
    int outputy; // 0..ROWC-1
    int flow;
    // For CPU players only:
    double cpumovelo,cpumovehi; // Period range for regular movement.
    double cpuwrenchlo,cpuwrenchhi; // Period range for deletion game.
    int cpuwrench; // Nonzero if we are playing the deletion game.
    double cpuclock;
    int cpux,cpuy; // Chosen cell for the current piece. <0 if we need to examine.
    struct intake *cpuintake; // WEAK, points into (intakev). Null initially.
  } playerv[2];
};

#define BATTLE ((struct battle_plumbing*)battle)

/* Delete.
 */
 
static void _plumbing_del(struct battle *battle) {
}

/* Random piece.
 */
 
static uint8_t random_piece() {
  // Not all 16 combinations are valid, in fact there are 7.
  switch (rand()%7) {
    case 0: return EGG_BTN_UP|EGG_BTN_DOWN;
    case 1: return EGG_BTN_LEFT|EGG_BTN_RIGHT;
    case 2: return EGG_BTN_UP|EGG_BTN_RIGHT;
    case 3: return EGG_BTN_RIGHT|EGG_BTN_DOWN;
    case 4: return EGG_BTN_DOWN|EGG_BTN_LEFT;
    case 5: return EGG_BTN_LEFT|EGG_BTN_UP;
    case 6: return EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN;
  }
  return 0;
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=COLC>>2;
    player->y=ROWC>>1;
  } else { // Right.
    player->who=1;
    player->x=(COLC*3)>>2;
    player->y=ROWC>>1;
    player->animframe=2; // Put them a half-turn out of phase, in case they overlap.
  }
  player->delete_decay=2.000*(1.0-player->skill)+1.000*player->skill;
  player->delete_step=0.250; // ''
  if (player->human=human) { // Human.
    player->delete_cooldown=DELETE_COOLDOWN; // To nix initial input.
  } else { // CPU.
    player->cpumovehi=0.900*(1.0-player->skill)+0.400*player->skill;
    player->cpumovelo=player->cpumovehi*0.5;
    player->cpuwrenchhi=0.300*(1.0-player->skill)+0.200*player->skill;
    player->cpuwrenchlo=player->cpuwrenchhi*0.5;
    player->cpux=player->cpuy=-1; // Choose at the first update.
    player->cpuclock=player->cpumovehi; // Start by delaying our longest interval.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x784d21ff;
        player->tileid=0x8b;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x6b;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x7b;
      } break;
  }
  player->outputy=OUTPUTY_MIN+(int)((1.0-player->skill)*(ROWC-1-OUTPUTY_MIN));
  if (player->outputy<OUTPUTY_MIN) player->outputy=OUTPUTY_MIN;
  else if (player->outputy>=ROWC-1) player->outputy=ROWC-2;
  // Fill upnext with random pieces.
  uint8_t *dst=player->upnext;
  int i=UPNEXT_SIZE;
  for (;i-->0;dst++) *dst=random_piece();
}

/* New.
 */
 
static int _plumbing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  // We could have more than 2 intakes, or adjust their positions. I think constant is ok.
  BATTLE->intakev[BATTLE->intakec++]=(struct intake){2,0};
  BATTLE->intakev[BATTLE->intakec++]=(struct intake){5,0};
  
  return 0;
}

/* Trace from a given cell until the pipe breaks or reaches an intake.
 * If it reaches an intake, return that.
 * We'll crudely guard against loops, but in truth, you should only call if you already know it's connected to the wall.
 * (x) must be 0 or (COLC-1) so we know which direction to start.
 */
 
static struct intake *plumbing_cell_find_intake(struct battle *battle,int x,int y) {
  uint8_t poison=(x?EGG_BTN_RIGHT:EGG_BTN_LEFT); // At each cell, there's one direction we can't go; the way we came from.
  int panic=COLC*ROWC;
  while (panic-->0) {
  
    if (y<0) { // Struck the ceiling. Is it an intake?
      struct intake *intake=BATTLE->intakev;
      int i=BATTLE->intakec;
      for (;i-->0;intake++) {
        if (intake->x==x) return intake;
      }
      return 0; // Nope it's just the ceiling.
    }
    if ((x<0)||(x>=COLC)||(y>=COLC)) return 0; // Struck a wall.
    
    uint8_t pipe=BATTLE->map[y*COLC+x];
    if (!(pipe&poison)) return 0; // There's a pipe here but it doesn't touch the prior cell.
    uint8_t nextdir=(pipe&15)&~poison;
    if (!nextdir) return 0; // Struck empty space.
    
    switch (nextdir) {
      // If (nextdir) is a single bit, it's the straight or elbow pipe. We know where to go next.
      case EGG_BTN_UP: poison=EGG_BTN_DOWN; y--; break;
      case EGG_BTN_DOWN: poison=EGG_BTN_UP; y++; break;
      case EGG_BTN_LEFT: poison=EGG_BTN_RIGHT; x--; break;
      case EGG_BTN_RIGHT: poison=EGG_BTN_LEFT; x++; break;
      // Otherwise it must be 3 bits and we travel to the one opposite the unset.
      case EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP: poison=EGG_BTN_DOWN; y--; break;
      case EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_DOWN: poison=EGG_BTN_UP; y++; break;
      case EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT: poison=EGG_BTN_RIGHT; x--; break;
      case EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_RIGHT: poison=EGG_BTN_LEFT; x++; break;
      // Nothing else should be possible. Abort.
      default: return 0;
    }
  }
  return 0;
}

/* Fill a pipe with water.
 * Similar idea to plumbing_cell_find_intake(), we're going to walk the pipe from here to intake.
 * Difference is we're filling it in as we go.
 */
 
static void plumbing_flow_pipes(struct battle *battle,int x,int y) {
  uint8_t poison=(x?EGG_BTN_RIGHT:EGG_BTN_LEFT);
  int panic=COLC*ROWC;
  while (panic-->0) {
  
    if ((x<0)||(x>=COLC)||(y<0)||(y>=COLC)) return;
    uint8_t pipe=BATTLE->map[y*COLC+x];
    if (!(pipe&poison)) return;
    uint8_t nextdir=(pipe&15)&~poison;
    if (!nextdir) return;
    
    switch (nextdir) {
      // If (nextdir) is a single bit, it's the straight or elbow pipe. We know where to go next.
      case EGG_BTN_UP: BATTLE->map[y*COLC+x]|=CELLBIT_MAIN; poison=EGG_BTN_DOWN; y--; break;
      case EGG_BTN_DOWN: BATTLE->map[y*COLC+x]|=CELLBIT_MAIN; poison=EGG_BTN_UP; y++; break;
      case EGG_BTN_LEFT: BATTLE->map[y*COLC+x]|=CELLBIT_MAIN; poison=EGG_BTN_RIGHT; x--; break;
      case EGG_BTN_RIGHT: BATTLE->map[y*COLC+x]|=CELLBIT_MAIN; poison=EGG_BTN_LEFT; x++; break;
      // Otherwise it must be 3 bits and we travel to the one opposite the unset.
      case EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP: BATTLE->map[y*COLC+x]|=CELLBIT_AUX; poison=EGG_BTN_DOWN; y--; break;
      case EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_DOWN: BATTLE->map[y*COLC+x]|=CELLBIT_AUX; poison=EGG_BTN_UP; y++; break;
      case EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT: BATTLE->map[y*COLC+x]|=CELLBIT_MAIN; poison=EGG_BTN_RIGHT; x--; break;
      case EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_RIGHT: BATTLE->map[y*COLC+x]|=CELLBIT_MAIN; poison=EGG_BTN_LEFT; x++; break;
      // Nothing else should be possible. Abort.
      default: return;
    }
  }
}

/* Check for completed pipes.
 * We scan the whole field for both players, regardless of whatever actually changed.
 * Ties actually are possible here: If the last piece played is a crosspipe that finished both constructions.
 */
 
static void plumbing_check_completion(struct battle *battle) {
  struct player *player=BATTLE->playerv;
  int pi=2;
  for (;pi-->0;player++) {
    
    /* Start at the cell immediately inside the player's output.
     * Verify it's connected to the wall.
     */
    int x=player->who?(COLC-1):0;
    int y=player->outputy;
    uint8_t pipe=BATTLE->map[y*COLC+x];
    if (player->who) {
      if (!(pipe&EGG_BTN_RIGHT)) continue;
    } else {
      if (!(pipe&EGG_BTN_LEFT)) continue;
    }
    
    /* Does it connect to any intake?
     */
    struct intake *intake=plumbing_cell_find_intake(battle,x,y);
    if (!intake) continue;
    
    /* Cool, fill it in.
     */
    player->flow=1;
    intake->flow=1;
    plumbing_flow_pipes(battle,x,y);
  }
  if (BATTLE->playerv[0].flow) {
    if (BATTLE->playerv[1].flow) battle->outcome=0; // impressive!
    else battle->outcome=1;
  } else if (BATTLE->playerv[1].flow) {
    battle->outcome=-1;
  }
}

/* Activate player.
 * If the cell is vacant, commit my piece to it.
 * Otherwise, pump the deletion game.
 */
 
static void player_press(struct battle *battle,struct player *player) {
  if ((player->x<0)||(player->x>=COLC)||(player->y<0)||(player->y>=ROWC)) return;
  int p=player->y*COLC+player->x;
  if (BATTLE->map[p]) {
    player->delete+=player->delete_step;
    if (player->delete>=1.0) {
      // Success! Pass to the replace clause below.
      player->delete=0.0;
      player->delete_cooldown=DELETE_COOLDOWN;
    } else {
      // Deleting...
      return;
    }
  }
  bm_sound_pan(RID_sound_deploy,player->who?0.250:-0.250);
  BATTLE->map[p]=player->upnext[player->upnextp];
  player->upnext[player->upnextp++]=random_piece();
  if (player->upnextp>=UPNEXT_SIZE) player->upnextp=0;
  player->upnext_slide=UPNEXT_SLIDE_TIME;
  plumbing_check_completion(battle);
}

/* Move cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {
  player->x+=dx;
  if (player->x<0) player->x=COLC-1;
  else if (player->x>=COLC) player->x=0;
  player->y+=dy;
  if (player->y<0) player->y=ROWC-1;
  else if (player->y>=ROWC) player->y=0;
  bm_sound_pan(RID_sound_uimotion,player->who?0.250:-0.250);
  // Deletion game nixes when you move.
  player->delete=0.0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  int press=(g.input[player->human]&~g.pvinput[player->human])&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_SOUTH);
  if ((press&EGG_BTN_SOUTH)&&(player->delete_cooldown<=0.0)) {
    press&=~EGG_BTN_SOUTH;
    player_press(battle,player);
  }
  switch (press) {
    case EGG_BTN_LEFT: player_move(battle,player,-1,0); break;
    case EGG_BTN_RIGHT: player_move(battle,player,1,0); break;
    case EGG_BTN_UP: player_move(battle,player,0,-1); break;
    case EGG_BTN_DOWN: player_move(battle,player,0,1); break;
  }
}

/* Choose cell for CPU player.
 * We populate (cpux,cpuy) and return >=0 on success, which should always be successful.
 * I don't think errors are possible, but it's <0 if we fail to choose a cell.
 * This is the essence of the CPU controller. player_update_cpu() is largely obvious boilerplate.
 * (cpuintake) must be present.
 * We just record the position; we don't distinguish whether it's on-path, discard, delete, etc even though we use those facts during the operation.
 */
 
struct coord {
  int x,y;
};
 
static int player_cpu_choose_cell(struct battle *battle,struct player *player) {
  uint8_t pipe=player->upnext[player->upnextp];
  int i,x,y;
  
  /* Since it's going to come up a lot, summarize our path's geometry.
   * It's just a horizontal line, vertical line, and their joint.
   * The horizontal line may run left or right, depending on (player)'s position.
   */
  int pathx=player->cpuintake->x; // Horizontal position of the vertical bit.
  int pathy=player->outputy; // Vertical position of the horizontal bit.
  int horzx,horzw; // Range of the horizontal bit. May be empty.
  if (player->who) {
    horzx=pathx+1;
    horzw=COLC-pathx-1;
  } else {
    horzx=0;
    horzw=pathx;
  }
  
  /* Find the nearest place on our path that we can place it.
   * Manhattan distance is correct for this: That's exactly the step count.
   * On-path cells with an invalid pipe already there do count, but they are less preferable than empties.
   * If there's garbage on the path we delete it, but we place free ones first.
   */
  int bestx=-1,besty=-1,bestscore=999;
  #define CHECKCELL(_x,_y,oka,okb) { \
    int cx=(_x),cy=(_y); \
    uint8_t cell=BATTLE->map[cy*COLC+cx]; \
    if ((cell!=(oka))&&(cell!=(okb))) { \
      int dx=player->x-cx; if (dx<0) dx=-dx; \
      int dy=player->y-cy; if (dy<0) dy=-dy; \
      int score=dx+dy; \
      if (cell) score=COLC+ROWC; /* Penalty if we need to delete something. */ \
      if (score<bestscore) { \
        bestx=cx; \
        besty=cy; \
        bestscore=score; \
      } \
    } \
  }
  switch (pipe&15) {
    case EGG_BTN_UP|EGG_BTN_DOWN: {
        for (y=pathy;y-->0;) CHECKCELL(pathx,y,EGG_BTN_UP|EGG_BTN_DOWN,EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT)
      } break;
    case EGG_BTN_LEFT|EGG_BTN_RIGHT: {
        for (i=horzw;i-->0;) CHECKCELL(horzx+i,pathy,EGG_BTN_LEFT|EGG_BTN_RIGHT,EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT)
      } break;
    case EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT: {
        // We use the crosspipe as either horizontal or vertical.
        for (y=pathy;y-->0;) CHECKCELL(pathx,y,EGG_BTN_UP|EGG_BTN_DOWN,EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT)
        for (i=horzw;i-->0;) CHECKCELL(horzx+i,pathy,EGG_BTN_LEFT|EGG_BTN_RIGHT,EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT)
      } break;
    // Elbows, we only want the upward ones, and one horizontal orientation depending on our side.
    case EGG_BTN_UP|EGG_BTN_LEFT: {
        if (!player->who) {
          CHECKCELL(pathx,pathy,EGG_BTN_UP|EGG_BTN_LEFT,EGG_BTN_UP|EGG_BTN_LEFT)
        }
      } break;
    case EGG_BTN_UP|EGG_BTN_RIGHT: {
        if (player->who) {
          CHECKCELL(pathx,pathy,EGG_BTN_UP|EGG_BTN_RIGHT,EGG_BTN_UP|EGG_BTN_RIGHT)
        }
      } break;
  }
  #undef CHECKCELL
  if ((bestx>=0)&&(besty>=0)) {
    player->cpux=bestx;
    player->cpuy=besty;
    return 0;
  }
  
  /* We need to discard this piece.
   * It's a bit ham-fisted maybe, but for simplicity's sake let's examine the whole map.
   * Prefer empty cells, even very far away, over deleting anything.
   */
  bestscore=999;
  const uint8_t *map=BATTLE->map;
  for (y=0;y<ROWC;y++) {
    for (x=0;x<COLC;x++,map++) {
      if ((x==pathx)&&(y<=pathy)) continue; // On our path, skip it. (vertical leg and joint)
      if ((y==pathy)&&(x>=horzx)&&(x<horzx+horzw)) continue; // On our path, horizontal leg.
      int dx=player->x-x; if (dx<0) dx=-dx;
      int dy=player->y-y; if (dy<0) dy=-dy;
      int score=dx+dy;
      if (*map) score+=COLC+ROWC;
      if (score<bestscore) {
        bestx=x;
        besty=y;
        bestscore=score;
      }
    }
  }
  player->cpux=bestx;
  player->cpuy=besty;
  return 0;
}

/* Update CPU player.
 * Don't overthink it:
 *  - Build a pipeline with a single elbow, to the nearest intake.
 *  - Put each incoming piece in the nearest valid position. (or nearest vacant non-path, for unneeded pieces).
 *  - If there isn't a valid position for it, wrench the nearest non-path cell.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // All activity is regulated in time.
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  
  // If we haven't selected an intake, do it now. Fail to find one? Abort.
  if (!player->cpuintake) {
    struct intake *intake=BATTLE->intakev;
    int i=BATTLE->intakec;
    for (;i-->0;intake++) {
      if (!player->cpuintake) {
        player->cpuintake=intake;
      } else if (player->who&&(intake->x>player->cpuintake->x)) {
        player->cpuintake=intake;
      } else if (!player->who&&(intake->x<player->cpuintake->x)) {
        player->cpuintake=intake;
      }
    }
    if (!player->cpuintake) {
      player->cpuclock=30.0;
      return;
    }
  }
  
  // Are we playing the deletion game?
  if (player->cpuwrench) {
    double nexttime=(rand()&0xffff)/65535.0;
    nexttime=player->cpuwrenchlo*(1.0-nexttime)+player->cpuwrenchhi*nexttime;
    player->cpuclock+=nexttime;
    player_press(battle,player);
    if (player->delete_cooldown>0.0) {
      player->cpuwrench=0;
      player->cpux=player->cpuy=-1;
    }
    return;
  }
  
  // Choose a cell if we haven't got one.
  if ((player->cpux<0)||(player->cpux>=COLC)||(player->cpuy<0)||(player->cpuy>=ROWC)) {
    if (player_cpu_choose_cell(battle,player)<0) {
      player->cpuclock=30.0;
      return;
    }
  }
  
  // If we're not on the target cell, proceed toward it. Choose an axis randomly at each step.
  struct candidate { int dx,dy; } candidatev[2];
  int candidatec=0;
       if (player->x<player->cpux) candidatev[candidatec++]=(struct candidate){1,0};
  else if (player->x>player->cpux) candidatev[candidatec++]=(struct candidate){-1,0};
       if (player->y<player->cpuy) candidatev[candidatec++]=(struct candidate){0,1};
  else if (player->y>player->cpuy) candidatev[candidatec++]=(struct candidate){0,-1};
  if (candidatec) {
    int p=rand()%candidatec;
    player_move(battle,player,candidatev[p].dx,candidatev[p].dy);
    double nexttime=(rand()&0xffff)/65535.0;
    nexttime=player->cpumovelo*(1.0-nexttime)+player->cpumovehi*nexttime;
    player->cpuclock+=nexttime;
    return;
  }
  
  // If this cell has something on it, begin the deletion game.
  if (BATTLE->map[player->y*COLC+player->x]&15) {
    player->cpuwrench=1;
    double nexttime=(rand()&0xffff)/65535.0;
    nexttime=player->cpuwrenchlo*(1.0-nexttime)+player->cpuwrenchhi*nexttime;
    player->cpuclock+=nexttime;
    player->cpux=player->cpuy=-1;
    return;
  }
  
  // Place my pipe.
  player_press(battle,player);
  player->cpux=player->cpuy=-1;
  double nexttime=(rand()&0xffff)/65535.0;
  nexttime=player->cpumovelo*(1.0-nexttime)+player->cpumovehi*nexttime;
  player->cpuclock+=nexttime;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if ((player->animclock-=elapsed)<=0.0) {
    player->animclock+=0.120;
    if (++(player->animframe)>=4) player->animframe=0;
  }
  if ((player->upnext_slide-=elapsed)<=0.0) player->upnext_slide=0.0;
  if ((player->delete-=player->delete_decay*elapsed)<=0.0) player->delete=0.0;
  if ((player->delete_cooldown-=elapsed)<=0.0) player->delete_cooldown=0.0;
}

/* Update.
 */
 
static void _plumbing_update(struct battle *battle,double elapsed) {

  if ((BATTLE->wateranimclock-=elapsed)<=0.0) {
    BATTLE->wateranimclock+=0.150;
    if (++(BATTLE->wateranimframe)>=4) BATTLE->wateranimframe=0;
  }

  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
    if (battle->outcome>-2) break; // If we establish completion, don't let the next guy try.
  }

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Tile and xform for a pipe cell.
 */
 
static void tile_for_cell(uint8_t *tileid,uint8_t *xform,uint8_t cell) {
  /* The pipe tiles are drawn such that reversing is OK.
   */
  *tileid=*xform=0;
  if (cell==(EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT|CELLBIT_MAIN|CELLBIT_AUX)) {
    *tileid=0x7a;
  } else if (cell==(EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT|CELLBIT_MAIN)) {
    *tileid=0x78;
  } else if (cell==(EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT|CELLBIT_AUX)) {
    *tileid=0x79;
  } else if (cell&CELLBIT_MAIN) {
    switch (cell&15) {
      case EGG_BTN_UP|EGG_BTN_DOWN: *tileid=0x76; *xform=0; return;
      case EGG_BTN_LEFT|EGG_BTN_RIGHT: *tileid=0x76; *xform=EGG_XFORM_SWAP; return;
      case EGG_BTN_DOWN|EGG_BTN_RIGHT: *tileid=0x77; *xform=0; return;
      case EGG_BTN_UP|EGG_BTN_RIGHT: *tileid=0x77; *xform=EGG_XFORM_YREV; return;
      case EGG_BTN_DOWN|EGG_BTN_LEFT: *tileid=0x77; *xform=EGG_XFORM_XREV; return;
      case EGG_BTN_UP|EGG_BTN_LEFT: *tileid=0x77; *xform=EGG_XFORM_XREV|EGG_XFORM_YREV; return;
    }
  } else {
    switch (cell&15) {
      case EGG_BTN_UP|EGG_BTN_DOWN: *tileid=0x73; *xform=0; return;
      case EGG_BTN_LEFT|EGG_BTN_RIGHT: *tileid=0x73; *xform=EGG_XFORM_SWAP; return;
      case EGG_BTN_DOWN|EGG_BTN_RIGHT: *tileid=0x74; *xform=0; return;
      case EGG_BTN_UP|EGG_BTN_RIGHT: *tileid=0x74; *xform=EGG_XFORM_YREV; return;
      case EGG_BTN_DOWN|EGG_BTN_LEFT: *tileid=0x74; *xform=EGG_XFORM_XREV; return;
      case EGG_BTN_UP|EGG_BTN_LEFT: *tileid=0x74; *xform=EGG_XFORM_XREV|EGG_XFORM_YREV; return;
      case EGG_BTN_UP|EGG_BTN_DOWN|EGG_BTN_LEFT|EGG_BTN_RIGHT: *tileid=0x75; *xform=0; return;
    }
  }
}

/* Render one player's bits.
 */
 
static void player_render(struct battle *battle,struct player *player,int fldx,int fldy) {
  
  /* Cursor.
   */
  graf_fancy(&g.graf,
    fldx+NS_sys_tilesize*player->x+(NS_sys_tilesize>>1),
    fldy+NS_sys_tilesize*player->y+(NS_sys_tilesize>>1),
    0x63+player->animframe,0,0,NS_sys_tilesize,0,player->color
  );
  
  /* Output spigot and sprite waiting for her glass of water.
   */
  uint8_t spigottile=0x85;
  if (player->flow) spigottile+=1;
  if (player->who) spigottile+=2;
  int spx;
  if (player->who) spx=fldx+COLC*NS_sys_tilesize+(NS_sys_tilesize>>1);
  else spx=fldx-(NS_sys_tilesize>>1);
  int spy=fldy+player->outputy*NS_sys_tilesize+(NS_sys_tilesize>>1);
  graf_tile(&g.graf,spx,spy,spigottile,0);
  if (player->who) spx-=3; else spx+=3;
  spy+=8;
  graf_tile(&g.graf,spx,spy,player->tileid+(player->flow?2:1),player->who?EGG_XFORM_XREV:0);
  if (player->who) spx+=NS_sys_tilesize; else spx-=NS_sys_tilesize;
  graf_tile(&g.graf,spx,spy,player->tileid,player->who?EGG_XFORM_XREV:0);
  
  /* Up next.
   * The tile at (upnextp) is the next one to go out, and they proceed upward, wrapping around.
   */
  int unx,undx;
  int slided=(int)((player->upnext_slide*NS_sys_tilesize)/UPNEXT_SLIDE_TIME);
  if (player->who) {
    undx=NS_sys_tilesize+2;
    unx=fldx+COLC*NS_sys_tilesize+NS_sys_tilesize+slided;
  } else {
    undx=-NS_sys_tilesize-2;
    unx=fldx-NS_sys_tilesize-slided;
  }
  int i=UPNEXT_SIZE,p=player->upnextp;
  for (;i-->0;p++,unx+=undx) {
    if (p>=UPNEXT_SIZE) p=0;
    uint8_t tileid,xform;
    tile_for_cell(&tileid,&xform,player->upnext[p]);
    graf_tile(&g.graf,unx,fldy+NS_sys_tilesize,tileid,xform);
  }
  
  /* Deletion game.
   */
  if (player->delete>0.0) {
    int midx=fldx+NS_sys_tilesize*player->x+(NS_sys_tilesize>>1);
    int midy=fldy+NS_sys_tilesize*player->y-(NS_sys_tilesize>>1);
    graf_tile(&g.graf,midx-(NS_sys_tilesize>>1),midy,0x8e,0);
    graf_tile(&g.graf,midx+(NS_sys_tilesize>>1),midy,0x8f,0);
    int8_t rot=(int8_t)((player->delete-0.5)*128.0);
    graf_fancy(&g.graf,midx,midy+4,0x7e,0,rot,NS_sys_tilesize,0,player->color);
  }
}

/* Render.
 */
 
static void _plumbing_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  graf_set_image(&g.graf,RID_image_battle_fractia2);
  
  /* Take some measurements and draw the main grid.
   */
  int fldw=COLC*NS_sys_tilesize;
  int fldh=ROWC*NS_sys_tilesize;
  int fldx=(FBW>>1)-(fldw>>1);
  int fldy=(FBH>>1)-(fldh>>1);
  int x0=fldx+(NS_sys_tilesize>>1);
  int y=fldy+(NS_sys_tilesize>>1);
  int yi=ROWC;
  const uint8_t *src=BATTLE->map;
  for (;yi-->0;y+=NS_sys_tilesize) {
    int x=x0,xi=COLC;
    for (;xi-->0;x+=NS_sys_tilesize,src++) {
      graf_tile(&g.graf,x,y,0x71,0);
      if (*src) {
        uint8_t tileid,xform;
        tile_for_cell(&tileid,&xform,*src);
        graf_tile(&g.graf,x,y,tileid,xform);
      }
    }
  }
  
  /* Border around the main grid.
   */
  for (yi=ROWC,y=fldy+(NS_sys_tilesize>>1);yi-->0;y+=NS_sys_tilesize) {
    graf_tile(&g.graf,fldx-(NS_sys_tilesize>>1),y,0x70,0);
    graf_tile(&g.graf,fldx+COLC*NS_sys_tilesize+(NS_sys_tilesize>>1),y,0x72,0);
  }
  int x=x0,xi=COLC;
  for (;xi-->0;x+=NS_sys_tilesize) {
    graf_tile(&g.graf,x,fldy-(NS_sys_tilesize>>1),0x61,0);
    graf_tile(&g.graf,x,fldy+ROWC*NS_sys_tilesize+(NS_sys_tilesize>>1),0x81,0);
  }
  graf_tile(&g.graf,fldx-(NS_sys_tilesize>>1),fldy-(NS_sys_tilesize>>1),0x60,0);
  graf_tile(&g.graf,fldx+COLC*NS_sys_tilesize+(NS_sys_tilesize>>1),fldy-(NS_sys_tilesize>>1),0x62,0);
  graf_tile(&g.graf,fldx-(NS_sys_tilesize>>1),fldy+ROWC*NS_sys_tilesize+(NS_sys_tilesize>>1),0x80,0);
  graf_tile(&g.graf,fldx+COLC*NS_sys_tilesize+(NS_sys_tilesize>>1),fldy+ROWC*NS_sys_tilesize+(NS_sys_tilesize>>1),0x82,0);
  
  /* Cistern's lower edge extending out from the border's top.
   */
  y=fldy-(NS_sys_tilesize>>1);
  int xl=fldx-NS_sys_tilesize-(NS_sys_tilesize>>1);
  int xr=fldx+(COLC+1)*NS_sys_tilesize+(NS_sys_tilesize>>1);
  for (;(xl>0)||(xr<FBW);xl-=NS_sys_tilesize,xr+=NS_sys_tilesize) {
    graf_tile(&g.graf,xl,y,0x61,0);
    graf_tile(&g.graf,xr,y,0x61,0);
  }
  
  /* Animated water at the cistern's top.
   * Doesn't matter whether the horizontal edges line up.
   */
  uint8_t tileid=0x67+BATTLE->wateranimframe;
  y-=NS_sys_tilesize;
  for (x=NS_sys_tilesize>>1;x<=FBW;x+=NS_sys_tilesize) {
    graf_tile(&g.graf,x,y,tileid,0);
  }
  
  /* Intakes along the grid's top edge.
   */
  struct intake *intake=BATTLE->intakev;
  int i=BATTLE->intakec;
  for (;i-->0;intake++) {
    uint8_t tileid=intake->flow?0x84:0x83;
    graf_tile(&g.graf,fldx+(NS_sys_tilesize>>1)+intake->x*NS_sys_tilesize,fldy-(NS_sys_tilesize>>1),tileid,0);
  }
  
  /* Players and their associated bits.
   */
  struct player *player=BATTLE->playerv;
  for (i=2;i-->0;player++) {
    player_render(battle,player,fldx,fldy);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_plumbing={
  .name="plumbing",
  .objlen=sizeof(struct battle_plumbing),
  .strix_name=169,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .no_timeout=1, // CPU players will always eventually finish. Allow infinite time in 2-player mode in case they want to just mess around.
  .del=_plumbing_del,
  .init=_plumbing_init,
  .update=_plumbing_update,
  .render=_plumbing_render,
};

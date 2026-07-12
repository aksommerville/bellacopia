/* battle_tempting.c
 */

#include "game/bellacopia.h"

#define ROCKC 7
#define SHIP_LIMIT 10
#define PLAN_LIMIT 20 /* There's time for about 12 notes. No problem to go over (or under for that matter, it loops) */
#define NOTE_LIMIT 8
#define SHIP_ANIM_PERIOD 1.000
#define SINK_TIME 1.000

struct battle_tempting {
  struct battle hdr;
  
  double helloclock;
  uint16_t plan[PLAN_LIMIT];
  int planc;
  int planp;
  double planclock; // Counts down from (beattime).
  double beattime;
  int sangl,sangr; // Nonzero if we heard from a given player this beat. They press something else, it's a penalty.
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    int x,y; // Midpoint (2x2 tiles) in framebuffer pixels.
    double animclock;
    int animframe;
    int singing; // For rendering and temptation efficacy. Stays on until you make a mistake.
    double notedplo,notedphi; // Allowed frequencies of the notes' path fuzzing, hertz.
    double notemaglo,notemaghi; // '' magnitudes, pixels.
    double temptr2; // Squared radius from each note, the temptation range.
    int score;
    
    // Decorative rocks encircling the player's big rock.
    struct rock {
      int x,y; // Relative to player.
      uint8_t tileid;
      uint8_t xform;
    } rockv[ROCKC];
    
    // CPU
    double pauselo,pausehi; // Delay before singing.
    double pause;
    uint8_t mistakev[PLAN_LIMIT]; // Parallel to (plan), nonzero if we should make a mistake on that beat.
    
  } playerv[2];
    
  // Ships sailing around.
  struct ship {
    int defunct;
    double sinking; // If nonzero, counts up to SINK_TIME.
    double x,y; // Framebuffer pixels.
    double t; // Zero is up.
    double speed;
    double finishx; // Random but close to FBW/2. So they don't all pile up.
    struct player *temptress; // Null if untempted.
    double animclock; // Animation for heart above the ship, when tempted. Clock only, counts up to SHIP_ANIM_PERIOD and repeats.
  } shipv[SHIP_LIMIT];
  int shipc;
  double spawnclock;
  int spawnside; // Alternate left/right for spawning. It's a substantial advantage to the player on that side.
  
  // Notes floating from player. Tempt a ship if they cross paths.
  struct note {
    double x,y; // Actual location in framebuffer pixels (derived).
    double refx,refy; // Linear travel position.
    double dx,dy;
    struct player *player;
    int defunct;
    double nx,ny; // Unit vector perpendicular to travel, for sinification.
    double p; // Sine phase.
    double dp; // Sine rate, hz.
    double mag; // Sine magnitude, pixels.
    double animclock;
    int animframe;
  } notev[NOTE_LIMIT];
  int notec;
};

#define BATTLE ((struct battle_tempting*)battle)

/* Delete.
 */
 
static void _tempting_del(struct battle *battle) {
  egg_play_song(3,0,0,0.0,0.0);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->xform=0;
    player->x=FBW/7;
  } else { // Right.
    player->who=1;
    player->xform=EGG_XFORM_XREV;
    player->x=(FBW*6)/7;
  }
  player->y=40;
  
  player->notedplo=5.0*(1.0-player->skill)+10.0*player->skill;
  player->notedphi=player->notedplo*1.500;
  player->notemaglo=1.0*(1.0-player->skill)+12.0*player->skill;
  player->notemaghi=player->notemaglo*1.500;
  double temptr=20.0*(1.0-player->skill)+25.0*player->skill;
  player->temptr2=temptr*temptr;
    
  if (player->human=human) { // Human.
  } else { // CPU.
    player->pausehi=0.300; // Maybe apply skill? It doesn't really matter anymore.
    player->pauselo=player->pausehi*0.5;
    player->pause=player->pausehi;
    
    // Select mistakes.
    int mistakeclo=0;
    int mistakechi=(int)(PLAN_LIMIT*0.600);
    int mistakec=(int)(mistakeclo*player->skill+mistakechi*(1.0-player->skill));
    if (mistakec<0) mistakec=0; else if (mistakec>=PLAN_LIMIT) mistakec=PLAN_LIMIT-1;
    int candidatev[PLAN_LIMIT];
    int i=PLAN_LIMIT; while (i-->0) candidatev[i]=i;
    int candidatec=PLAN_LIMIT;
    for (i=mistakec;i-->0;) {
      int p=rand()%candidatec;
      player->mistakev[candidatev[p]]=1;
      candidatec--;
      memmove(candidatev+p,candidatev+p+1,sizeof(int)*(candidatec-p));
    }
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x109644ff;
        player->tileid=0x86;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x46;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x66;
      } break;
  }
  
  const double rockradiusx=30.0;
  const double rockradiusy=17.0;
  struct rock *rock=player->rockv;
  int i=ROCKC;
  for (;i-->0;rock++) {
    double rx=sin((i*M_PI*2.0)/ROCKC)*rockradiusx;
    double ry=-cos((i*M_PI*2.0)/ROCKC)*rockradiusy;
    rock->x=lround(rx);
    rock->y=lround(ry)+5; // Cheat the y down a little; player images are bottom-heavy.
    rock->tileid=0x4c+(rand()%6)*16;
    rock->xform=(rand()&1)?EGG_XFORM_XREV:0;
  }
}

/* New.
 */
 
static int _tempting_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  // or in simpler cases: BATTLE->difficulty=battle_scalar_difficulty(battle);
  // And I keep getting this backward: At HIGH skill, the player is favored to win. High bias means high skill for the CPU and low skill for Dot.
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  /* Prepare the plan.
   */
  while (BATTLE->planc<PLAN_LIMIT) {
    uint16_t btnid;
    switch (rand()%6) {
      case 0: btnid=EGG_BTN_LEFT; break;
      case 1: btnid=EGG_BTN_RIGHT; break;
      case 2: btnid=EGG_BTN_UP; break;
      case 3: btnid=EGG_BTN_DOWN; break;
      case 4: btnid=EGG_BTN_SOUTH; break;
      case 5: btnid=EGG_BTN_WEST; break;
    }
    BATTLE->plan[BATTLE->planc++]=btnid;
  }
  BATTLE->beattime=1.250;
  BATTLE->planclock=BATTLE->beattime;
  
  BATTLE->helloclock=1.000;
  BATTLE->playclock=15.0;
  
  /* The song has four channels:
   *  0: L good
   *  1: L bad
   *  2: R good
   *  3: R bad
   */
  egg_play_song(3,RID_song_tempting,1,0.500,0.0);
  
  return 0;
}

/* Spawn a note.
 */
 
static struct note *temptation_spawn_note(struct battle *battle,struct player *player) {
  if (BATTLE->notec>=NOTE_LIMIT) return 0;
  struct note *note=BATTLE->notev+BATTLE->notec++;
  note->player=player;
  note->defunct=0;
  note->refx=note->x=player->x;
  note->refy=note->y=player->y;
  
  // Go to a random position on the far half of the screen.
  double targetx=rand()%(FBW>>2);
  if (player->who) targetx+=FBW>>2;
  else targetx+=(FBW*3)>>2;
  double targety=FBH;

  double dx=targetx-note->x;
  double dy=targety-note->y;
  double dist=sqrt(dx*dx+dy*dy);
  const double speed=100.0;
  note->dx=(dx*speed)/dist;
  note->dy=(dy*speed)/dist;
  note->nx=dy/dist;
  note->ny=-dx/dist;
  note->p=0.0;
  double n=(rand()&0xffff)/65535.0;
  note->dp=n*player->notedplo+(1.0-n)*player->notedphi;
  n=(rand()&0xffff)/65535.0;
  note->mag=n*player->notemaglo+(1.0-n)*player->notemaghi;
  return note;
}

/* Sing a note.
 */
 
static void player_sing(struct battle *battle,struct player *player,uint16_t btnid) {

  // Check for illegal extra notes.
  if ((player->who&&BATTLE->sangr)||(!player->who&&BATTLE->sangl)) {
    player->singing=0;
    uint8_t noteid=0x3d; // C#4
    egg_song_event_note_once(3,(player->who<<1)+1,noteid,0x40,500);
    return;
  }

  if (btnid==BATTLE->plan[BATTLE->planp]) {
    /* Correct note.
     * Our background music is all in G minor.
     * Pick something from the complementary major chord (A#).
     */
    uint8_t noteid;
    switch (rand()%4) {
      case 0: noteid=0x46; break; // A#4
      case 1: noteid=0x4a; break; // D5
      case 2: noteid=0x4d; break; // F5
      case 3: noteid=0x52; break; // A#5
    }
    egg_song_event_note_once(3,(player->who<<1),noteid,0x40,500);
    temptation_spawn_note(battle,player);
    player->singing=1;
    player->animclock=0.0;
    player->animframe=0;
    
  } else {
    // Incorrect note.
    player->singing=0;
    uint8_t noteid=0x3d; // C#4
    egg_song_event_note_once(3,(player->who<<1)+1,noteid,0x40,500);
  }
  
  if (player->who) BATTLE->sangr=1;
  else BATTLE->sangl=1;
}

static void player_didnt_sing(struct battle *battle,struct player *player) {
  if (player->singing) {
    player->singing=0;
    uint8_t noteid=0x3d; // C#4
    egg_song_event_note_once(3,(player->who<<1)+1,noteid,0x40,500);
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  const uint16_t notes[]={EGG_BTN_LEFT,EGG_BTN_RIGHT,EGG_BTN_UP,EGG_BTN_DOWN,EGG_BTN_SOUTH,EGG_BTN_WEST};
  const uint16_t *note=notes;
  int i=sizeof(notes)/sizeof(notes[0]);
  for (;i-->0;note++) {
    if ((input&(*note))&&!(pvinput&(*note))) player_sing(battle,player,*note);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  if ((player->who&&BATTLE->sangr)||(!player->who&&BATTLE->sangl)) {
    // Already sang. We could insert false notes if needed to nerf it. But not doing that yet.
    return;
  }
  
  // Run down my pause clock.
  if (player->pause>0.0) {
    player->pause-=elapsed;
    return;
  }
  double n=(rand()&0xffff)/65535.0;
  player->pause=n*player->pauselo+(1.0-n)*player->pausehi;
  
  int correct=!player->mistakev[BATTLE->planp];
  if (correct) {
    player_sing(battle,player,BATTLE->plan[BATTLE->planp]);
  } else {
    uint16_t btnid;
    for (;;) {
      switch (rand()%6) {
        case 0: btnid=EGG_BTN_LEFT; break;
        case 1: btnid=EGG_BTN_RIGHT; break;
        case 2: btnid=EGG_BTN_UP; break;
        case 3: btnid=EGG_BTN_DOWN; break;
        case 4: btnid=EGG_BTN_SOUTH; break;
        case 5: btnid=EGG_BTN_WEST; break;
      }
      if (btnid==BATTLE->plan[BATTLE->planp]) continue; // oops
      break;
    }
    player_sing(battle,player,btnid);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if ((player->animclock-=elapsed)<=0.0) {
    player->animclock+=0.250;
    if (++(player->animframe)>=2) player->animframe=0;
  }
}

/* Update ship.
 */
 
static void ship_update(struct battle *battle,struct ship *ship,double elapsed) {

  /* If we're sinking, tick that off until finished, then defunct.
   */
  if (ship->sinking>0.0) {
    ship->temptress=0;
    if ((ship->sinking+=elapsed)>=SINK_TIME) ship->defunct=1;
    return;
  }

  /* Animate if we're in love.
   * Or drop the temptation if she stopped singing.
   */
  if (ship->temptress&&!ship->temptress->singing) {
    ship->temptress=0;
  }
  if (ship->temptress) {
    if ((ship->animclock+=elapsed)>=SHIP_ANIM_PERIOD) ship->animclock=0.0;
  } else {
    ship->animclock=0.0;
  }

  /* Pick a target to steer toward.
   */
  const double finishy=BATTLE->playerv[0].y;
  double targetx,targety=finishy;
  if (ship->y<=finishy) { // If we made it parallel to the sirens and haven't died yet, steer straight onward to the top.
    targetx=ship->x;
    targety=-32.0;
  } else if (ship->temptress) {
    targetx=ship->temptress->x;
    targety=ship->temptress->y;
  } else {
    targetx=ship->finishx;
  }
  
  /* And what does that target mean? Left or right.
   */
  int steer=0; // -1,0,1
  double targett=atan2(targetx-ship->x,ship->y-targety);
  double dt=targett-ship->t;
  while (dt<=-M_PI) dt+=M_PI*2.0;
  while (dt>=M_PI) dt-=M_PI*2.0;
       if (dt<-0.010) steer=-1;
  else if (dt> 0.010) steer=1;

  /* Apply steering.
   */
  if (steer) {
    const double steerrate=2.000; // rad/sec
    ship->t+=steerrate*steer*elapsed;
    if (ship->t<-M_PI) ship->t+=2.0*M_PI;
    else if (ship->t>M_PI) ship->t-=2.0*M_PI;
  }
  
  /* Apply velocity.
   */
  ship->x+=sin(ship->t)*ship->speed*elapsed;
  ship->y-=cos(ship->t)*ship->speed*elapsed;
  
  /* Dash upon the rocks if too close to a siren, or escape if over the top edge.
   * Rocks are only effective while temptation is active.
   * So if you hit a foul note just before the crash, the ship might magically sail over your rocks. Sucks to suck.
   */
  if (ship->y<-10.0) {
    ship->defunct=1;
  } else if (battle->outcome>-2) {
    ship->temptress=0;
  } else if (ship->temptress) {
    double dx=ship->temptress->x-ship->x;
    double dy=ship->temptress->y-ship->y;
    const double crashradius2=40.0*40.0;
    if (dx*dx+dy*dy<crashradius2) {
      ship->sinking=0.001;
      ship->temptress->score++;
      bm_sound_pan(RID_sound_bombblow,ship->temptress->who?PLAYER_PAN:-PLAYER_PAN);
    }
  }
}

/* Update note.
 */
 
static void note_update(struct battle *battle,struct note *note,double elapsed) {

  // Animate.
  if ((note->animclock-=elapsed)<=0.0) {
    note->animclock+=0.150;
    if (++(note->animframe)>=4) note->animframe=0;
  }

  // Reference position advances linearly.
  note->refx+=note->dx*elapsed;
  note->refy+=note->dy*elapsed;
  
  // Advance a sine wave and combine with reference for the final position.
  note->p+=note->dp*elapsed;
  if (note->p>M_PI) note->p-=M_PI*2.0;
  double off=sin(note->p)*note->mag;
  note->x=note->refx+note->nx*off;
  note->y=note->refy+note->ny*off;
  
  // If we leave the screen, we're done.
  const double margin=8.0;
  if ((note->x<-margin)||(note->x>FBW+margin)||(note->y<-margin)||(note->y>FBH+margin)) {
    note->defunct=1;
    return;
  }
  
  // Check for nearby ships and tempt them.
  // Don't bother if our siren isn't singing; they'd just toggle back off next update.
  if ((battle->outcome==-2)&&note->player->singing) {
    struct ship *ship=BATTLE->shipv;
    int i=BATTLE->shipc;
    for (;i-->0;ship++) {
      if (ship->defunct||(ship->sinking>0.0)) continue;
      if (ship->temptress) continue;
      double dx=ship->x-note->x;
      double dy=ship->y-note->y;
      double d2=dx*dx+dy*dy;
      if (d2>note->player->temptr2) continue;
      bm_sound_pan(RID_sound_collect,note->player->who?PLAYER_PAN:-PLAYER_PAN);
      ship->temptress=note->player;
    }
  }
}

/* Finish a beat.
 */
 
static void tempting_finish_beat(struct battle *battle) {
  
  /* Lightly penalize any player that didn't sing.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (!BATTLE->sangl) player_didnt_sing(battle,l);
  if (!BATTLE->sangr) player_didnt_sing(battle,r);
  
  /* Skip to the next beat and reset trackers.
   * There should be more plan that needed, but if we go over, start from the top again.
   */
  if (++(BATTLE->planp)>=BATTLE->planc) BATTLE->planp=0;
  BATTLE->sangl=0;
  BATTLE->sangr=0;
}

/* Update.
 */
 
static void _tempting_update(struct battle *battle,double elapsed) {
  
  // Tick the hello clock if it hasn't expired yet, or advance plan. Neither if game over.
  if (battle->outcome>-2) {
  } else if (BATTLE->helloclock>0.0) {
    BATTLE->helloclock-=elapsed;
  } else if ((BATTLE->planclock-=elapsed)<=0.0) {
    BATTLE->planclock+=BATTLE->beattime;
    tempting_finish_beat(battle);
  }
  
  // Spawn a ship at regular intervals.
  if (battle->outcome==-2) {
    if ((BATTLE->spawnclock-=elapsed)<0.0) {
      BATTLE->spawnclock+=1.500;
      if (BATTLE->shipc<SHIP_LIMIT) {
        struct ship *ship=BATTLE->shipv+BATTLE->shipc++;
        ship->defunct=0;
        double offset=((rand()&0xffff)*FBW*0.333)/65535;
        if (BATTLE->spawnside^=1) ship->x=FBW*0.5+offset;
        else ship->x=FBW*0.5-offset;
        ship->y=FBH+12;
        ship->t=0.0;
        ship->speed=40.0;
        ship->finishx=(FBW>>1)+((rand()&0xffff)*20.0)/65535.0-10.0;
        ship->temptress=0;
        ship->sinking=0.0;
      }
    }
  }
  
  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (battle->outcome==-2) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
  
  // Update ships.
  struct ship *ship=BATTLE->shipv+BATTLE->shipc-1;
  for (i=BATTLE->shipc;i-->0;ship--) {
    ship_update(battle,ship,elapsed);
    if (ship->defunct) {
      BATTLE->shipc--;
      memmove(ship,ship+1,sizeof(struct ship)*(BATTLE->shipc-i));
    }
  }
  
  // Update notes.
  struct note *note=BATTLE->notev+BATTLE->notec-1;
  for (i=BATTLE->notec;i-->0;note--) {
    note_update(battle,note,elapsed);
    if (note->defunct) {
      BATTLE->notec--;
      memmove(note,note+1,sizeof(struct note)*(BATTLE->notec-i));
    }
  }
  
  // Game ends when the clock runs out. Ties are perfectly normal.
  if (battle->outcome==-2) {
    if ((BATTLE->playclock-=elapsed)<=0.0) {
      struct player *l=BATTLE->playerv;
      struct player *r=l+1;
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  graf_set_image(&g.graf,RID_image_battle_sea);

  // First draw the rocks with (y<0).
  struct rock *rock=player->rockv;
  int i=ROCKC;
  for (;i-->0;rock++) {
    if (rock->y>=0) continue;
    graf_tile(&g.graf,player->x+rock->x,player->y+rock->y,rock->tileid,rock->xform);
  }

  // Then the player on her big rock.
  uint8_t tileid=player->tileid;
  if (player->singing) {
    if (player->animframe) tileid+=4; else tileid+=2;
  }
  int dstx=player->x-NS_sys_tilesize;
  int dsty=player->y-NS_sys_tilesize;
  int srcx=(tileid&15)*NS_sys_tilesize;
  int srcy=(tileid>>4)*NS_sys_tilesize;
  graf_decal_xform(&g.graf,dstx,dsty,srcx,srcy,NS_sys_tilesize*2,NS_sys_tilesize*2,player->xform);

  // Then the rocks in front.
  for (rock=player->rockv,i=ROCKC;i-->0;rock++) {
    if (rock->y<0) continue;
    graf_tile(&g.graf,player->x+rock->x,player->y+rock->y,rock->tileid,rock->xform);
  }
}

/* Render ship.
 */
 
static void ship_render(struct battle *battle,struct ship *ship) {
  int x=(int)ship->x;
  int y=(int)ship->y;
  
  // Sinking is a whole different thing.
  if (ship->sinking>0.0) {
    int exposure=(int)((1.0-ship->sinking/SINK_TIME)*NS_sys_tilesize);
    if (exposure>0) {
      if (exposure>NS_sys_tilesize) exposure=NS_sys_tilesize;
      graf_decal(&g.graf,
        x-(NS_sys_tilesize>>1),
        y-(NS_sys_tilesize>>1)+NS_sys_tilesize-exposure,
        NS_sys_tilesize*14,
        NS_sys_tilesize*3,
        NS_sys_tilesize,exposure
      );
      graf_tile(&g.graf,x,y,0x2e,0);
    }
    return;
  }
  
  uint8_t rot=(int8_t)((ship->t*128.0)/M_PI);
  uint8_t tileid=0x0f;
  int i=10; for (;i-->0;tileid+=0x10,y-=1) {
    graf_fancy(&g.graf,x,y,tileid,0,rot,NS_sys_tilesize,0,0x808080ff);
  }
  
  if (ship->temptress) {
    int hearty=y-(int)((ship->animclock*20.0)/SHIP_ANIM_PERIOD);
    uint8_t tileid=0x7e;
    int frame=(int)((ship->animclock*5.0)/SHIP_ANIM_PERIOD);
    switch (frame) {
      case 1: tileid+=0x10; break;
      case 2: tileid+=0x20; break;
      case 3: tileid+=0x10; break;
    }
    graf_fancy(&g.graf,x,hearty,tileid,0,0,NS_sys_tilesize,0,ship->temptress->color);
  }
}

/* Render note.
 */
 
static void note_render(struct battle *battle,struct note *note) {
  int x=(int)note->x;
  int y=(int)note->y;
  uint8_t tileid=0x4e;
  switch (note->animframe) {
    case 1: tileid+=0x10; break;
    case 2: tileid+=0x20; break;
    case 3: tileid+=0x10; break;
  }
  graf_fancy(&g.graf,x,y,tileid,0,0,NS_sys_tilesize,0,note->player->color);
}

/* Render.
 */
 
static void _tempting_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x4b74a5ff);
  graf_set_image(&g.graf,RID_image_battle_sea);
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  struct ship *ship=BATTLE->shipv;
  int i=BATTLE->shipc;
  graf_set_image(&g.graf,RID_image_battle_sea);
  graf_set_filter(&g.graf,0); // debatable
  for (;i-->0;ship++) ship_render(battle,ship);
  graf_set_filter(&g.graf,0);
  
  struct note *note=BATTLE->notev;
  for (i=BATTLE->notec;i-->0;note++) note_render(battle,note);
  
  // Current note.
  if (battle->outcome==-2) {
    int alpha=(BATTLE->planclock*255.0)/BATTLE->beattime;
    if (alpha>0) {
      if (alpha>0xff) alpha=0xff;
      graf_set_alpha(&g.graf,alpha);
      uint8_t tileid;
      switch (BATTLE->plan[BATTLE->planp]) {
        case EGG_BTN_LEFT: tileid=0x4d; break;
        case EGG_BTN_RIGHT: tileid=0x5d; break;
        case EGG_BTN_UP: tileid=0x6d; break;
        case EGG_BTN_DOWN: tileid=0x7d; break;
        case EGG_BTN_SOUTH: tileid=0x8d; break;
        case EGG_BTN_WEST: tileid=0x9d; break;
      }
      graf_tile(&g.graf,FBW>>1,22,tileid,0);
      graf_set_alpha(&g.graf,0xff);
    }
  }
  
  /* Clock and scores.
   */
  graf_set_image(&g.graf,RID_image_fonttiles);
  if (BATTLE->playclock>0.0) {
    int s=(int)(BATTLE->playclock+0.999);
    if (s<1) s=1; else if (s>99) s=99;
    if (s>=10) graf_tile(&g.graf,(FBW>>1)-4,8,'0'+s/10,0);
    graf_tile(&g.graf,(FBW>>1)+4,8,'0'+s%10,0);
  }
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->score>=10) {
    graf_tile(&g.graf,(FBW>>1)-40,8,'0'+(l->score/10)%10,0);
    graf_tile(&g.graf,(FBW>>1)-32,8,'0'+l->score%10,0);
  } else {
    graf_tile(&g.graf,(FBW>>1)-36,8,'0'+l->score%10,0);
  }
  if (r->score>=10) {
    graf_tile(&g.graf,(FBW>>1)+32,8,'0'+(r->score/10)%10,0);
    graf_tile(&g.graf,(FBW>>1)+40,8,'0'+r->score%10,0);
  } else {
    graf_tile(&g.graf,(FBW>>1)+36,8,'0'+r->score%10,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_tempting={
  .name="tempting",
  .objlen=sizeof(struct battle_tempting),
  .id=NS_battle_tempting,
  .strix_name=278,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_dpad_ab,
  .del=_tempting_del,
  .init=_tempting_init,
  .update=_tempting_update,
  .render=_tempting_render,
};

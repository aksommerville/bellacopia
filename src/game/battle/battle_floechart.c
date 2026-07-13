/* battle_floechart.c
 */

#include "game/bellacopia.h"

#define BGCOLC NS_sys_mapw
#define BGROWC (NS_sys_maph-3)
#define CHARTCOLC 7 /* Must be less than BGCOLC. This is also how many options the players have. */
#define CHARTROWC 5 /* No more than 7, ie BGROWC-2. */
#define HOP_TIME 0.300
#define LOOP_REPORT_STEPS 10
#define CPUXV_LIMIT 6

struct battle_floechart {
  struct battle hdr;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    int col,row; // row<0 before selection
    double hopclock;
    int done;
    int loop; // Counts up each loop step. We don't show the error state immediately, let the user notice too.
    double drownclock; // Counts up after drowning.
    
    double cpuclock;
    double cputhinktime; // Between all movements.
    int cpuxv[CPUXV_LIMIT]; // Visit all of these before committing, commit at the last one.
    int cpuxc;
    int cpuxp;
    int cpudone;
  } playerv[2];
  
  // Background and floe chart tiles are kept in ready lists.
  struct egg_render_tile bgvtxv[BGCOLC*BGROWC];
  struct egg_render_tile chartvtxv[CHARTCOLC*CHARTROWC];
  
  // The floe chart has additional per-cell data too.
  struct floe {
    double animclock; // Toggle between the high and low tiles each period. low is row 8, high is row 9.
    int dx,dy; // Direction my arrow points.
    uint8_t visit; // 1=left, 2=right. For detecting loops during the final animation.
    uint8_t correct; // Nonzero if this is the correct path. There's just one.
  } floev[CHARTCOLC*CHARTROWC];
  int correctin; // Correct column on top, for CPU's reference.
  int correctout; // Correct column on bottom, for rendering.
};

#define BATTLE ((struct battle_floechart*)battle)

/* Delete.
 */
 
static void _floechart_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->col=0;
  } else { // Right.
    player->who=1;
    player->col=CHARTCOLC-1;
  }
  player->row=-1;
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cputhinktime=0.400*player->skill+0.600*(1.0-player->skill);
    player->cpuclock=player->cputhinktime+1.000+2.000*(1.0-player->skill);
  
    /* CPU player decides its entire program of play up front, right here.
     * (cpuxv) is a list of columns that we will visit, and when we fall off the list, we commit at that last column.
     */
    player->cpuxc=(int)((1.0-player->skill)*CPUXV_LIMIT);
    if (player->cpuxc<2) player->cpuxc=2;
    else if (player->cpuxc>CPUXV_LIMIT) player->cpuxc=CPUXV_LIMIT;
    int i=player->cpuxc-1;
    
    // One all-important choice: Which column goes in the end? The right one or a wrong one?
    int rightodds=(int)((1.0-player->skill)*33.0+player->skill*100.0);
    if (rand()%100<rightodds) {
      player->cpuxv[i]=BATTLE->correctin;
    } else {
      do {
        player->cpuxv[i]=rand()%CHARTCOLC;
      } while (player->cpuxv[i]==BATTLE->correctin);
    }
    
    // And the rest are random.
    while (i-->0) player->cpuxv[i]=rand()%CHARTCOLC;
  
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xd4cfbbff;
        player->tileid=0xa3;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x83;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x93;
      } break;
  }
}

/* New.
 */
 
static int _floechart_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  BATTLE->playclock=30.0;
  
  /* Prepare the background vertex list.
   * The ground is a flat color, but this background goes on top of it with water and the edges.
   */
  int x,i;
  int y=FBH-BGROWC*NS_sys_tilesize;
  struct egg_render_tile *vtx=BATTLE->bgvtxv;
  for (i=BGCOLC,x=NS_sys_tilesize>>1;i-->0;x+=NS_sys_tilesize,vtx++) {
    vtx->x=x;
    vtx->y=y;
    vtx->tileid=0x80;
    vtx->xform=0;
  }
  y+=NS_sys_tilesize;
  for (i=BGROWC-2;i-->0;y+=NS_sys_tilesize) {
    int xi=BGCOLC;
    for (x=NS_sys_tilesize>>1;xi-->0;x+=NS_sys_tilesize,vtx++) {
      vtx->x=x;
      vtx->y=y;
      vtx->tileid=0x90;
      vtx->xform=0;
    }
  }
  for (i=BGCOLC,x=NS_sys_tilesize>>1;i-->0;x+=NS_sys_tilesize,vtx++) {
    vtx->x=x;
    vtx->y=y;
    vtx->tileid=0xa0;
    vtx->xform=0;
  }
  
  /* Prepare the floe chart vertex list.
   * This is a smaller grid centered within the background.
   * Its cell stride is greater than the tilesize.
   */
  int chartcolw=NS_sys_tilesize+4;
  int chartrowh=NS_sys_tilesize+4;
  int chartx0=(FBW>>1)-((chartcolw*CHARTCOLC)>>1)+(NS_sys_tilesize>>1);
  int ylo=BATTLE->bgvtxv[0].y;
  int yhi=BATTLE->bgvtxv[BGCOLC*BGROWC-1].y;
  int charty=((ylo+yhi)>>1)-((chartrowh*CHARTROWC)>>1)+(NS_sys_tilesize>>1);
  vtx=BATTLE->chartvtxv;
  for (i=CHARTROWC;i-->0;charty+=chartrowh) {
    int xi=CHARTCOLC;
    int chartx=chartx0;
    for (;xi-->0;chartx+=chartcolw,vtx++) {
      int bits=rand();
      vtx->x=chartx;
      vtx->y=charty;
      vtx->tileid=0x81+(bits&1)+(bits&0x10);
      vtx->xform=0;
      if (bits&2) vtx->x--;
      else if (bits&4) vtx->x++;
      if (bits&8) vtx->y--;
      else if (bits&32) vtx->y++;
    }
  }
  
  /* Initialize the chart.
   */
  int panic=100;
 _try_again_:;
  struct floe *floe=BATTLE->floev;
  for (i=CHARTCOLC*CHARTROWC;i-->0;floe++) {
    floe->animclock=((rand()&0xffff)*0.500)/65535.0;
    floe->dx=floe->dy=0;
    floe->visit=0;
    floe->correct=0;
  }
  
  /* Pick a random starting column.
   * Then drunk-walk from there to the bottom.
   * Do not visit the top row more than once, and do not revisit any cell.
   * We're only populating (floev) in this pass, and only its correct path.
   */
  BATTLE->correctin=rand()%CHARTCOLC;
  int px=BATTLE->correctin;
  int py=0;
  while (py<CHARTROWC) {
    floe=BATTLE->floev+py*CHARTCOLC+px;
    floe->correct=1;
    struct candidate { int x,y,dx,dy; } candidatev[8];
    int candidatec=0;
    int dx=-1; for (;dx<=1;dx++) {
      int dy=-1; for (;dy<=1;dy++) {
        if (!dx&&!dy) continue;
        int qx=px+dx;
        int qy=py+dy;
        if ((qx<0)||(qx>=CHARTCOLC)) continue;
        if (qy<1) continue; // (qy>=CHARTROWC) is allowed.
        if ((qy>=CHARTROWC)||!BATTLE->floev[qy*CHARTCOLC+qx].correct) {
          candidatev[candidatec++]=(struct candidate){qx,qy,dx,dy};
        }
      }
    }
    // The drunk walk can easily paint itself into a corner, in which case we start over.
    if (!candidatec) {
      if (panic--<0) return -1;
      goto _try_again_;
    }
    struct candidate *choice=candidatev+rand()%candidatec;
    floe->dx=choice->dx;
    floe->dy=choice->dy;
    px+=choice->dx;
    py+=choice->dy;
  }
  BATTLE->correctout=px;
  
  /* Then every floe that isn't assigned yet, give it a random direction.
   * But do not point toward anything marked correct. If we did, there would be multiple correct answers.
   * It's fine to point off the edges, create non-correct loops, whatever.
   */
  floe=BATTLE->floev;
  for (py=0;py<CHARTROWC;py++) {
    for (px=0;px<CHARTCOLC;px++,floe++) {
      if (floe->correct) continue; // Already been here.
      int panic=50;
      while (panic-->0) {
        floe->dx=(rand()%3)-1;
        floe->dy=(rand()%3)-1;
        if (!floe->dx&&!floe->dy) continue;
        int qx=px+floe->dx;
        int qy=py+floe->dy;
        if ((qx>=0)&&(qx<CHARTCOLC)&&(qy>=0)&&(qy<CHARTROWC)) {
          if (BATTLE->floev[qy*CHARTCOLC+qx].correct) continue;
        }
        break;
      }
    }
  }
  
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Move player horizontally.
 */
 
static void player_move(struct battle *battle,struct player *player,int d) {
  int ncol=player->col+d;
  if ((ncol<0)||(ncol>=CHARTCOLC)) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  player->col=ncol;
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Commit player.
 */
 
static void player_commit(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->row=0;
  player->hopclock=HOP_TIME;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1);
  else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1);
  else if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_commit(battle,player);
  else if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_commit(battle,player);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  player->cpuclock+=player->cputhinktime;
  
  if (player->cpuxp>=player->cpuxc) {
    if (player->cpudone) { // A wee mitigation for when he chooses a floe that loops back to the start shore. Step right and try again.
      player->cpudone=0;
      player_move(battle,player,1);
      return;
    }
    player->cpudone=1;
    player_commit(battle,player);
    return;
  }
  
  int dstx=player->cpuxv[player->cpuxp];
  if (dstx<player->col) player_move(battle,player,-1);
  else if (dstx>player->col) player_move(battle,player,1);
  else player->cpuxp++;
}

/* Update player after commit.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  // Reached the end or drowned already?
  if (player->row>=CHARTROWC) return;
  if ((player->col<0)||(player->col>=CHARTCOLC)) {
    player->drownclock+=elapsed;
    return;
  }
  if (player->loop>=LOOP_REPORT_STEPS) {
    player->done=1;
    return;
  }
  
  // Tick down hopclock for each step.
  if ((player->hopclock-=elapsed)>0.0) return;
  player->hopclock+=HOP_TIME;
  
  // Go with the floe.
  struct floe *floe=BATTLE->floev+player->row*CHARTCOLC+player->col;
  player->col+=floe->dx;
  player->row+=floe->dy;
  
  // It's possible to get hopped back to the starting shore. I dunno, I guess let it roll? She becomes interactive again.
  // But if it was a diagonal, ensure we're still in bounds.
  if (player->row<0) {
    bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
    if (player->col<0) player->col=0;
    else if (player->col>=CHARTCOLC) player->col=CHARTCOLC-1;
    return;
  }
  
  // Make a different sound when landing at the far shore or drowning.
  if (player->row>=CHARTROWC) {
    player->done=1;
    if (player->col==BATTLE->correctout) {
      bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
    } else {
      bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
  } else if ((player->col<0)||(player->col>=CHARTCOLC)) {
    player->done=1;
    bm_sound_pan(RID_sound_splash,player->who?PLAYER_PAN:-PLAYER_PAN);
  } else {
    bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
    uint8_t mask=player->who?2:1;
    struct floe *floe=BATTLE->floev+player->row*CHARTCOLC+player->col;
    if (floe->visit&mask) player->loop++;
    else floe->visit|=mask;
  }
}

/* Update.
 */
 
static void _floechart_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->row<0) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    } else {
      player_update_common(battle,player,elapsed);
    }
  }
  
  struct floe *floe=BATTLE->floev;
  for (i=CHARTCOLC*CHARTROWC;i-->0;floe++) {
    if ((floe->animclock-=elapsed)<=0.0) {
      floe->animclock+=0.200+((rand()&0xffff)*0.300)/65535.0;
      int p=floe-BATTLE->floev;
      struct egg_render_tile *vtx=BATTLE->chartvtxv+p;
      vtx->tileid^=0x10;
    }
  }
  
  /* Game ends when a player reaches the goal or time runs out.
   * Or if both players are marked done, it's a tie. eg both drowned or in a loop.
   */
  BATTLE->playclock-=elapsed;
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->done&&(l->row==CHARTROWC)&&(l->col==BATTLE->correctout)) {
    if (r->done&&(r->row==CHARTROWC)&&(r->col==BATTLE->correctout)) battle->outcome=0; // Both correct and at the same time. Narrowly possible.
    else battle->outcome=1;
  } else if (r->done&&(r->row==CHARTROWC)&&(r->col==BATTLE->correctout)) battle->outcome=-1;
  else if (BATTLE->playclock<=0.0) battle->outcome=0;
  else if (l->done&&r->done) battle->outcome=0;
}

/* Render player.
 */

static void player_render(struct battle *battle,struct player *player) {

  // Align horizontally to the chart. Nudge left or right always per side.
  int x,drown=0;
  if (player->col<0) { drown=1; x=BATTLE->chartvtxv[0].x-20; }
  else if (player->col>=CHARTCOLC) { drown=1; x=BATTLE->chartvtxv[CHARTCOLC-1].x+20; }
  else x=BATTLE->chartvtxv[player->col].x;
  if (player->who) x+=3;
  else x-=3;
  
  int y;
  uint8_t tileid=player->tileid;
  if (drown) {
    if (player->drownclock>0.500) return;
    else if (player->drownclock>0.333) tileid=0xa4;
    else if (player->drownclock>0.166) tileid=0x94;
    else tileid=0x84;
  }
    
  // Vertical might be in the floe chart or on either shore.
  if (player->row<0) { // On the top shore, choosing column.
    y=BATTLE->bgvtxv[0].y-NS_sys_tilesize;
  } else if (player->row>=CHARTROWC) { // On the bottom shore, end of game.
    y=BATTLE->bgvtxv[BGCOLC*BGROWC-1].y;
  } else { // In the floe chart.
    struct egg_render_tile *vtx=BATTLE->chartvtxv+player->row*CHARTCOLC;
    if ((player->col>=0)&&(player->col<CHARTCOLC)) vtx+=player->col;
    y=vtx->y-2;
    if ((player->col>=0)&&(player->col<CHARTCOLC)&&(vtx->tileid&0x10)) y--;
  }
  
  graf_tile(&g.graf,x,y,tileid,0);
  if (player->loop>=LOOP_REPORT_STEPS) {
    graf_tile(&g.graf,x,y-13,0x71,0);
  }
}

/* Render.
 */
 
static void _floechart_render(struct battle *battle) {

  // Background and ice floes, mostly prepared batches.
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(&g.graf,RID_image_battle_tundra);
  graf_tile_batch(&g.graf,BATTLE->bgvtxv,BGCOLC*BGROWC);
  
  /* Originally there was a third batch for arrows on the floes.
   * That proved a challenge to read and not much fun.
   * So instead, we draw dark blue lines on the water. Much clearer!
   */
  {
    graf_set_input(&g.graf,0);
    struct egg_render_tile *vtx=BATTLE->chartvtxv;
    struct floe *floe=BATTLE->floev;
    int i=CHARTCOLC*CHARTROWC;
    for (;i-->0;vtx++,floe++) {
      graf_line(&g.graf,vtx->x,vtx->y,0x0020a0ff,vtx->x+floe->dx*20,vtx->y+floe->dy*20,0x0020a0ff);
    }
  }
  
  graf_set_image(&g.graf,RID_image_battle_tundra);
  graf_tile_batch(&g.graf,BATTLE->chartvtxv,CHARTCOLC*CHARTROWC);
  
  // Ice cream cone below the correct output.
  struct egg_render_tile *vtx=BATTLE->chartvtxv+CHARTCOLC*(CHARTROWC-1)+BATTLE->correctout;
  graf_tile(&g.graf,vtx->x,vtx->y+20,0x70,0);
  
  // Players.
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // Clock on top.
  if (BATTLE->playclock>0.0) {
    int s=(int)(BATTLE->playclock+0.999);
    if (s<1) s=1; else if (s>99) s=99;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_set_tint(&g.graf,battle->ctab[BATTLE_COLOR_GROUND_TEXT]);
    if (s>=10) graf_tile(&g.graf,(FBW>>1)-4,10,'0'+s/10,0);
    graf_tile(&g.graf,(FBW>>1)+4,10,'0'+s%10,0);
    graf_set_tint(&g.graf,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_floechart={
  .name="floechart",
  .objlen=sizeof(struct battle_floechart),
  .id=NS_battle_floechart,
  .strix_name=280,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_horz_a,
  .del=_floechart_del,
  .init=_floechart_init,
  .update=_floechart_update,
  .render=_floechart_render,
};

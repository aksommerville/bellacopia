/* battle_scubatuba.c
 */

#include "game/bellacopia.h"

// How far the player can dive, in framebuffer pixels.
#define TOP_LIMIT 30.0
#define BOTTOM_LIMIT 135.0

#define BUBBLE_LIMIT 8

struct battle_scubatuba {
  struct battle hdr;
  double refanimclock;
  int refanimframe;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t xform;
    double x,y; // Framebuffer pixels.
    double kick; // 0..1 = together..apart
    int inkick; // -1,0,1 = in,neutral,out
    uint8_t inblow; // 0x40,0x10,0x08,0x02
    int blackout; // Humans only.
    int blowblackout; // All players.
    double uprate; // px/s, must be negative
    double downrate; // px/s, must be positive
    double kickrate; // hz
    double blowrate; // hz
    double lossrate; // hz, negative
    double scorescale; // 1..9
    double bubble; // 0..1 = none..ready
    double quality; // 0..1, grows with (bubble) and always less
    int score; // Sum of quality for completed bubbles, scaled and quantized.
    uint8_t noteid;
    int rptscore; // The last scored bubble, for display.
    double rptscoreclock;
    double rptscorey;
    int cpuychoice; // -1,0,1
    double cpublowclock;
    double cpublowmin,cpublowmax;
  } playerv[2];
  
  struct bubble {
    double x,y;
    double dx,dy;
  } bubblev[BUBBLE_LIMIT];
  int bubblec;
};

#define BATTLE ((struct battle_scubatuba*)battle)

/* Delete.
 */
 
static void _scubatuba_del(struct battle *battle) {
  egg_play_song(3,0,0,0.0,0.0);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.333;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.666;
    player->xform=EGG_XFORM_XREV;
  }
  player->y=FBH*0.500;
  player->kick=0.5;
  player->uprate=-30.0*(1.0-player->skill)-40.0*player->skill;
  player->downrate=40.0*(1.0-player->skill)+30.0*player->skill;
  player->blowrate=0.800*(1.0-player->skill)+1.200*player->skill;
  player->lossrate=-2.000*(1.0-player->skill)-1.0*player->skill;
  player->scorescale=4.0*(1.0-player->skill)+9.0*player->skill;
  player->kickrate=3.000*(1.0-player->skill)+4.000*player->skill;
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpublowmin=1.500*(1.0-player->skill)+1.200*player->skill;
    player->cpublowmax=player->cpublowmin*2.0;
    double n=(rand()&0xffff)/65535.0;
    player->cpublowclock=player->cpublowmin*(1.0-n)+player->cpublowmax*n;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x92a8b5ff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
}

/* New.
 */
 
static int _scubatuba_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  egg_play_song(3,RID_song_scubatuba,1,0.800,0.0);
  BATTLE->playclock=15.0;
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&(EGG_BTN_SOUTH|EGG_BTN_WEST))) player->blackout=0;
  } else switch (input&(EGG_BTN_SOUTH|EGG_BTN_WEST)) {
    case EGG_BTN_SOUTH: player->inkick=1; break;
    case EGG_BTN_WEST: player->inkick=-1; break;
    default: player->inkick=0; break;
  }
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_LEFT: player->inblow=0x10; break;
    case EGG_BTN_RIGHT: player->inblow=0x08; break;
    case EGG_BTN_UP: player->inblow=0x40; break;
    case EGG_BTN_DOWN: player->inblow=0x02; break;
    default: player->inblow=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Kick or sink?
   */
  switch (player->cpuychoice) {
    case -1: if (player->y<60.0) player->cpuychoice=1; break;
    case 0: player->cpuychoice=1; break;
    case 1: if (player->y>100.0) player->cpuychoice=-1; break;
  }
  if (player->cpuychoice<0) {
    if ((player->kick<=0.0)&&(player->inkick<0)) player->inkick=1;
    else if ((player->kick>=1.0)&&(player->inkick>0)) player->inkick=-1;
    else if (!player->inkick) player->inkick=(player->kick>=0.5)?-1:1;
  } else {
    player->inkick=0;
  }
  
  /* Blow?
   */
  player->cpublowclock-=elapsed;
  if (player->cpublowclock<=0.0) {
    double n=(rand()&0xffff)/65535.0;
    player->cpublowclock=player->cpublowmin*(1.0-n)+player->cpublowmax*n;
    switch (rand()&3) {
      case 0: player->inblow=0x40; break;
      case 1: player->inblow=0x10; break;
      case 2: player->inblow=0x08; break;
      case 3: player->inblow=0x02; break;
    }
  } else if (player->cpublowclock<0.250) {
    player->inblow=0;
  }
}

/* Bubble is ready. Score it, create a sprite for it, and reset the player.
 */
 
static void player_commit_bubble(struct battle *battle,struct player *player) {

  // Choose a score for this bubble.
  int score=lround(player->quality*player->scorescale);
  if (score<1) score=1; else if (score>9) score=9;
  
  // Arrange to report it.
  player->rptscore=score;
  player->rptscoreclock=0.750;
  player->rptscorey=player->y-20.0;

  // Update total score, and reset player's bubble state.
  player->score+=score;
  player->bubble=0.0;
  player->quality=0.0;
  player->blowblackout=1;
  
  // Floating bubble sprite if we have room for one.
  if (BATTLE->bubblec<BUBBLE_LIMIT) {
    struct bubble *bubble=BATTLE->bubblev+BATTLE->bubblec++;
    bubble->x=player->x;
    if (player->xform) bubble->x-=16.0;
    else bubble->x+=16.0;
    bubble->y=player->y-8.0;
    double n=(rand()&0xffff)/65535.0;
    bubble->dx=-10.0*(1.0-n)+10.0*n;
    n=(rand()&0xffff)/65535.0;
    bubble->dy=-30.0*(1.0-n)-20.0*n;
  }
}

/* Momentary quality 0..1 based on player's vertical position.
 */
 
static double player_get_quality(const struct battle *battle,const struct player *player) {
  double dy=player->y-FBH*0.5;
  if (dy<0.0) dy=-dy;
  double q=1.0-dy/40.0;
  if (q<=0.0) return 0.0;
  if (q>=1.0) return 1.0;
  return q;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Once outcome is established, nix all input.
   */
  if (battle->outcome>-2) {
    player->inkick=0;
    player->inblow=0;
  }
  
  /* If a score is being reported, tick it down.
   */
  if (player->rptscore) {
    if ((player->rptscoreclock-=elapsed)<=0.0) {
      player->rptscore=0;
    } else {
      player->rptscorey-=20.0*elapsed;
    }
  }

  /* Kick?
   */
  int kicking=0;
  if (player->inkick) {
    player->kick+=player->inkick*elapsed*player->kickrate;
    if (player->kick<0.0) player->kick=0.0;
    else if (player->kick>1.0) player->kick=1.0;
    else kicking=1;
  }
  if (kicking) {
    player->y+=player->uprate*elapsed;
    if (player->y<TOP_LIMIT) player->y=TOP_LIMIT;
  } else {
    player->y+=player->downrate*elapsed;
    if (player->y>BOTTOM_LIMIT) player->y=BOTTOM_LIMIT;
  }
  
  /* Blow?
   */
  if (player->blowblackout) {
    if (!player->inblow) player->blowblackout=0;
  } else if (player->inblow) {
    player->bubble+=player->blowrate*elapsed;
    player->quality+=player->blowrate*elapsed*player_get_quality(battle,player);
    if (player->bubble>=1.0) {
      player_commit_bubble(battle,player);
    }
  } else {
    player->bubble+=player->lossrate*elapsed;
    if (player->bubble<=0.0) {
      player->bubble=0.0;
      player->quality=0.0;
    } else {
      // (quality) drops at (lossrate) too, which is faster than its relative growth -- a penalty for changing your tune mid-bubble.
      player->quality+=player->lossrate*elapsed;
      if (player->quality<=0.0) player->quality=0.0;
    }
  }
  
  /* Note?
   */
  uint8_t noteid=0;
  if (player->bubble>0.0) switch (player->inblow) {
    case 0x02: noteid=0x2e; break;
    case 0x08: noteid=0x32; break;
    case 0x10: noteid=0x35; break;
    case 0x40: noteid=0x3a; break;
  }
  if (noteid!=player->noteid) {
    if (player->noteid) egg_song_event_note_off(3,player->who,player->noteid);
    player->noteid=noteid;
    if (player->noteid) egg_song_event_note_on(3,player->who,player->noteid,0x40);
  }
}

/* Update.
 */
 
static void _scubatuba_update(struct battle *battle,double elapsed) {

  if ((BATTLE->refanimclock-=elapsed)<=0.0) {
    BATTLE->refanimclock+=0.150;
    if (++(BATTLE->refanimframe)>=4) BATTLE->refanimframe=0;
  }
  
  struct bubble *bubble=BATTLE->bubblev+BATTLE->bubblec-1;
  int i=BATTLE->bubblec;
  for (;i-->0;bubble--) {
    bubble->x+=bubble->dx*elapsed;
    bubble->y+=bubble->dy*elapsed;
    if (bubble->y<TOP_LIMIT) {
      BATTLE->bubblec--;
      memmove(bubble,bubble+1,sizeof(struct bubble)*(BATTLE->bubblec-i));
    }
  }
  
  struct player *player=BATTLE->playerv;
  for (i=2;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game ends when time runs out.
   * Ties are entirely possible.
   */
  if ((battle->outcome==-2)&&((BATTLE->playclock-=elapsed)<=0.0)) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int ht=NS_sys_tilesize>>1;
  int midx=(int)player->x;
  int midy=(int)player->y;
  int x0,x1;
  if (player->xform) {
    x0=midx+ht;
    x1=midx-ht;
  } else {
    x0=midx-ht;
    x1=midx+ht;
  }
  int y0=midy-ht;
  int y1=midy+ht;
  int legd=lround(player->kick*4.0);
  int legy=y1+4;
  uint8_t tileid=0x14;
  if (player->bubble>0.0) tileid+=2;
  graf_fancy(&g.graf,midx-legd,legy,0x34,EGG_XFORM_XREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,midx+legd,legy,0x34,0,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,x0,y0,tileid+0x00,player->xform,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,x1,y0,tileid+0x01,player->xform,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,x0,y1,tileid+0x10,player->xform,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,x1,y1,tileid+0x11,player->xform,0,NS_sys_tilesize,0,player->color);
  if (player->bubble>0.0) {
    int bubx=x1;
    if (player->xform) bubx-=8;
    else bubx+=8;
    int buby=y0;
    uint8_t bubtileid=0x35;
         if (player->bubble>=0.800) bubtileid+=5;
    else if (player->bubble>=0.600) bubtileid+=4;
    else if (player->bubble>=0.450) bubtileid+=3;
    else if (player->bubble>=0.300) bubtileid+=2;
    else if (player->bubble>=0.150) bubtileid+=1;
    graf_fancy(&g.graf,bubx,buby,bubtileid,0,0,NS_sys_tilesize,0,player->color);
  }
}

/* Render 1..3 digit unsigned integer.
 */
 
static void render_int(int x,int y,int v) {
  if (v<0) v=0; else if (v>999) v=999;
  if (v>=100) {
    graf_tile(&g.graf,x-8,y,'0'+v/100,0);
    graf_tile(&g.graf,x,y,'0'+(v/10)%10,0);
    graf_tile(&g.graf,x+8,y,'0'+v%10,0);
  } else if (v>=10) {
    graf_tile(&g.graf,x-4,y,'0'+v/10,0);
    graf_tile(&g.graf,x+4,y,'0'+v%10,0);
  } else {
    graf_tile(&g.graf,x,y,'0'+v,0);
  }
}

/* Render.
 */
 
static void _scubatuba_render(struct battle *battle) {

  // Start blue.
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x0020e0ff);
  graf_set_image(&g.graf,RID_image_battle_sea);
  
  // Decorative sand and waves at top and bottom.
  int srcx=NS_sys_tilesize*8;
  int srcy=NS_sys_tilesize*1;
  int srcw=NS_sys_tilesize*3;
  int srch=NS_sys_tilesize*2;
  int dstx=0;
  int dsty=FBH-srch;
  for (;dstx<FBW;dstx+=srcw) graf_decal(&g.graf,dstx,dsty,srcx,srcy,srcw,srch);
  dstx=0; // Could animate waves if we feel like it, just cheat (dstx) negative here.
  dsty=0;
  srcx=NS_sys_tilesize*11;
  srcy=NS_sys_tilesize*1;
  srcw=NS_sys_tilesize*3;
  srch=NS_sys_tilesize*2;
  for (;dstx<FBW;dstx+=srcw) graf_decal(&g.graf,dstx,dsty,srcx,srcy,srcw,srch);
  
  // Decorative referee fish.
  uint8_t reftileid=0x3b;
  switch (BATTLE->refanimframe) {
    case 1: case 3: reftileid+=1; break;
    case 2: reftileid+=2; break;
  }
  graf_fancy(&g.graf,FBW>>1, 35,reftileid,0,0,NS_sys_tilesize,0,0xff0000ff);
  graf_fancy(&g.graf,FBW>>1, 85,reftileid,0,0,NS_sys_tilesize,0,0x00c000ff);
  graf_fancy(&g.graf,FBW>>1,135,reftileid,0,0,NS_sys_tilesize,0,0xff0000ff);
  
  // Decorative floating bubbles.
  struct bubble *bubble=BATTLE->bubblev;
  int i=BATTLE->bubblec;
  for (;i-->0;bubble++) graf_fancy(&g.graf,(int)bubble->x,(int)bubble->y,0x3a,0,0,NS_sys_tilesize,0,0x808080ff);
  
  // Players.
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  player_render(battle,l);
  player_render(battle,r);
  
  // Score toasts, a separate pass because they use a different texture.
  graf_set_image(&g.graf,RID_image_tinyfonttiles);
  if (l->rptscore) graf_tile(&g.graf,(int)l->x,(int)l->rptscorey,'0'+l->rptscore,0);
  if (r->rptscore) graf_tile(&g.graf,(int)r->x,(int)r->rptscorey,'0'+r->rptscore,0);
  
  // Clock and scores at the bottom.
  int scorey=FBH-15;
  graf_set_image(&g.graf,RID_image_fonttiles);
  if (BATTLE->playclock>0.0) render_int(FBW>>1,scorey,(int)(BATTLE->playclock+0.999));
  render_int(FBW/3,scorey,l->score);
  render_int((FBW*2)/3,scorey,r->score);
}

/* Type definition.
 */
 
const struct battle_type battle_type_scubatuba={
  .name="scubatuba",
  .objlen=sizeof(struct battle_scubatuba),
  .id=NS_battle_scubatuba,
  .strix_name=301,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_dpad_ab_alternate,
  .imageid_default=0,
  .del=_scubatuba_del,
  .init=_scubatuba_init,
  .update=_scubatuba_update,
  .render=_scubatuba_render,
};

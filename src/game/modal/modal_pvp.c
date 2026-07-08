/* modal_pvp.c
 * Launches random battles for one or two players.
 * This is a quickie for early demos, I don't expect to keep it forever.
 */

#include "game/bellacopia.h"

#define BATTLE_LIMIT 128

#define STAGE_CHOOSE_PLAYERS 1
#define STAGE_REPORT 2 /* Between battles. */
#define STAGE_FINAL 3 /* After one player reaches 3 wins. */

struct modal_pvp {
  struct modal hdr;
  const struct battle_type *p1v[BATTLE_LIMIT];
  const struct battle_type *p2v[BATTLE_LIMIT];
  int p1c,p2c;
  int p1p,p2p;
  
  int lbltexid,lblw,lblh;
  
  int stage;
  int playerc; // 1,2
  int lwinc,rwinc;
  int outcome;
};

#define MODAL ((struct modal_pvp*)modal)

/* Cleanup.
 */
 
static void _pvp_del(struct modal *modal) {
  egg_texture_del(MODAL->lbltexid);
}

/* Init.
 */
 
static int _pvp_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1;
  
  /* List all of the available battle types, separately for 1 and 2 players.
   * NB: They must be ID'd contiguously from 1.
   */
  int id=1;
  for (;;id++) {
    const struct battle_type *type=battle_type_by_id(id);
    if (!type) break;
    
    // Pick off some oddballs.
    if (id==NS_battle_placeholder) continue; // Not a real battle but for technical reasons it must look like one.
    if (id==NS_battle_chess) { // 2p mode exists but it is real Chess, not something we'd present in a minigames contest. 1p is a regular battle.
      if (MODAL->p1c<BATTLE_LIMIT) MODAL->p1v[MODAL->p1c++]=type;
      continue;
    }
    if (id==NS_battle_seamonster) continue; // Not a real battle.
    if (id==NS_battle_election) continue; // Could do, but it only feels right in the context of the game.
    if (id==NS_battle_greenfish) continue; // Not really a battle (tho technically we could)
    if (id==NS_battle_bluefish) continue;
    if (id==NS_battle_redfish) continue;
    //TODO If I don't finish any outstanding placeholder in time, pick it off here: sumohorse fencing jeter homerunderby dissection stenography sorting
    
    // Everything that survived the filter above has a 1-player mode.
    if (MODAL->p1c<BATTLE_LIMIT) MODAL->p1v[MODAL->p1c++]=type;
    
    // And most have a 2-player mode, it's flagged on the type.
    if (type->support_pvp) {
      if (MODAL->p2c<BATTLE_LIMIT) MODAL->p2v[MODAL->p2c++]=type;
    }
  }
  if (!MODAL->p1c||!MODAL->p2c) {
    fprintf(stderr,"%s: Failed to find battles.\n",__func__);
    return -1;
  }
  MODAL->p1p=MODAL->p1c; // Invalid position to force a shuffle the first time we start.
  MODAL->p2p=MODAL->p2c;
  
  bm_song_gently(RID_song_death_rattle);
  
  MODAL->stage=STAGE_CHOOSE_PLAYERS;
  MODAL->playerc=1;
  
  return 0;
}

/* Focus.
 */
 
static void _pvp_focus(struct modal *modal,int focus) {
}

/* Notify.
 */
 
static void _pvp_notify(struct modal *modal,int k,int v) {
  if (k==EGG_PREF_LANG) {
  }
}

/* Shuffle one list of battle types.
 * No need to get sophisticated about this. Just pick two random slots and swap them, and repeat.
 */
 
static void pvp_shuffle(const struct battle_type **v,int c) {
  if (c<2) return;
  int i=300;
  while (i-->0) {
    int ap=rand()%c;
    int bp=rand()%c;
    const struct battle_type *tmp=v[ap];
    v[ap]=v[bp];
    v[bp]=tmp;
  }
}

/* Battle ends.
 */

static void pvp_cb_battle(struct modal *battle_modal,int outcome,void *userdata) {
  struct modal *modal=userdata;
  if (outcome<0) MODAL->rwinc++;
  else if (outcome>0) MODAL->lwinc++;
  MODAL->outcome=outcome;
  
  if ((MODAL->rwinc>=3)||(MODAL->lwinc>=3)) {
    MODAL->stage=STAGE_FINAL;
    int strix;
    if (MODAL->lwinc>=3) strix=218; // "Dot wins"
    else if (MODAL->playerc==2) strix=219; // "Princess wins"
    else strix=220; // "Monsters win"
    egg_texture_del(MODAL->lbltexid);
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_battle,strix);
    MODAL->lbltexid=font_render_to_texture(0,g.font,src,srcc,FBW,font_get_line_height(g.font),0xffffffff);
    egg_texture_get_size(&MODAL->lblw,&MODAL->lblh,MODAL->lbltexid);
    fprintf(stderr,"%d %s %.*s\n",(int)egg_time_real(),__func__,srcc,src);
    
  } else {
    MODAL->stage=STAGE_REPORT;
  }
}

/* Begin game.
 */
 
static void pvp_begin_game(struct modal *modal) {

  /* Grab the next battle type off the appropriate list.
   */
  const struct battle_type *type=0;
  if (MODAL->playerc==1) {
    if (MODAL->p1p>=MODAL->p1c) {
      pvp_shuffle(MODAL->p1v,MODAL->p1c);
      MODAL->p1p=0;
    }
    type=MODAL->p1v[MODAL->p1p++];
  } else if (MODAL->playerc==2) {
    if (MODAL->p2p>=MODAL->p2c) {
      pvp_shuffle(MODAL->p2v,MODAL->p2c);
      MODAL->p2p=0;
    }
    type=MODAL->p2v[MODAL->p2p++];
  }
  if (!type) return;
  
  /* Prep args for a battle modal.
   */
  struct modal_args_battle args={
    .battle=type->id,
    .args={
      .difficulty=0x80, // TODO Either user-configurable or auto adjust based on outcome of each round.
      .bias=0x80,
      .lface=NS_face_dot,
      .lctl=1,
      .no_store=1, // Important!
    },
    .cb=pvp_cb_battle,
    .userdata=modal,
    .nameless_prompt=1,
  };
  switch (MODAL->playerc) {
    case 1: {
        args.args.rface=NS_face_monster;
        args.args.rctl=0;
      } break;
    case 2: {
        args.args.rface=NS_face_princess;
        args.args.rctl=2;
      } break;
  }
  
  struct modal *battle_modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
  if (!battle_modal) return;
}

/* Update, choosing players.
 */
 
static void pvp_set_playerc(struct modal *modal,int playerc) {
  if (playerc==MODAL->playerc) {
    bm_sound(RID_sound_reject);
  } else {
    bm_sound(RID_sound_uimotion);
    MODAL->playerc=playerc;
  }
}
 
static void pvp_update_choose_players(struct modal *modal,double elapsed) {

  // WEST to dismiss, presumably returning to Hello.
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    bm_song_gently(RID_song_break_soil);
    bm_sound(RID_sound_uicancel);
    modal->defunct=1;
    return;
  }
  
  // LEFT or RIGHT to change player count.
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) pvp_set_playerc(modal,1);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) pvp_set_playerc(modal,2);
  
  // SOUTH to proceed.
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    fprintf(stderr,"%d pvp start playerc=%d\n",(int)egg_time_real(),MODAL->playerc);
    bm_sound(RID_sound_uiactivate);
    MODAL->lwinc=0;
    MODAL->rwinc=0;
    pvp_begin_game(modal);
  }
}

/* Update, between rounds.
 */
 
static void pvp_update_report(struct modal *modal,double elapsed) {

  // WEST to dismiss, presumably returning to Hello.
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    bm_song_gently(RID_song_break_soil);
    bm_sound(RID_sound_uicancel);
    modal->defunct=1;
    return;
  }
  
  // SOUTH to proceed.
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    bm_sound(RID_sound_uiactivate);
    pvp_begin_game(modal);
  }
}

/* Update, with winner established.
 */
 
static void pvp_update_final(struct modal *modal,double elapsed) {

  // WEST to dismiss, presumably returning to Hello.
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    bm_song_gently(RID_song_break_soil);
    bm_sound(RID_sound_uicancel);
    modal->defunct=1;
    return;
  }
  
  // SOUTH to return to the beginning.
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    bm_sound(RID_sound_uiactivate);
    MODAL->stage=STAGE_CHOOSE_PLAYERS;
  }
}

/* Update.
 */
 
static void _pvp_update(struct modal *modal,double elapsed) {
  switch (MODAL->stage) {
    case STAGE_CHOOSE_PLAYERS: pvp_update_choose_players(modal,elapsed); break;
    case STAGE_REPORT: pvp_update_report(modal,elapsed); break;
    case STAGE_FINAL: pvp_update_final(modal,elapsed); break;
  }
}

/* Render, choosing players.
 */
 
static void pvp_render_choose_players(struct modal *modal) {
  const int panelw=NS_sys_tilesize<<2;
  const int panelh=NS_sys_tilesize<<2;
  const int spacing=8;
  int x0=(FBW>>1)-((panelw+panelh+spacing)>>1);
  int y=(FBH>>1)-(panelh>>1);
  graf_set_image(&g.graf,RID_image_arcade);
  if (MODAL->playerc==1) {
    graf_decal(&g.graf,x0,y,0,0,panelw,panelh);
    graf_decal(&g.graf,x0+panelw+spacing,y,panelw,panelh,panelw,panelh);
  } else {
    graf_decal(&g.graf,x0,y,panelw,0,panelw,panelh);
    graf_decal(&g.graf,x0+panelw+spacing,y,0,panelh,panelw,panelh);
  }
}

/* Render scoreboard as a single row.
 */
 
static void pvp_render_scoreboard(struct modal *modal,int y) {
  graf_set_image(&g.graf,RID_image_arcade);
  const int spacing=4;
  const int totalw=NS_sys_tilesize*5+spacing*4;
  int x=(FBW>>1)-(totalw>>1)+(NS_sys_tilesize>>1);
  int i=0;
  for (;i<5;i++,x+=spacing+NS_sys_tilesize) {
    uint8_t tileid=0x28;
    if (i<MODAL->lwinc) tileid+=1;
    else if (i>=5-MODAL->rwinc) {
      if (MODAL->playerc==2) tileid+=2;
      else tileid+=3;
    }
    graf_tile(&g.graf,x,y,tileid,0);
  }
}

/* Render, between rounds.
 */
 
static void pvp_render_report(struct modal *modal) {
  pvp_render_scoreboard(modal,FBH>>1);
}

/* Render, game over.
 */
 
static void pvp_render_final(struct modal *modal) {
  int picx=(FBW>>1)-NS_sys_tilesize;
  int picy=(FBH>>1)-NS_sys_tilesize;
  pvp_render_scoreboard(modal,60);
  int picsrcx;
  if (MODAL->lwinc>=3) picsrcx=128;
  else if (MODAL->playerc==2) picsrcx=160;
  else picsrcx=192;
  graf_decal(&g.graf,picx,picy,picsrcx,0,NS_sys_tilesize*2,NS_sys_tilesize*2);
  graf_set_input(&g.graf,MODAL->lbltexid);
  graf_decal(&g.graf,(FBW>>1)-(MODAL->lblw>>1),picy+NS_sys_tilesize*2,0,0,MODAL->lblw,MODAL->lblh);
}

/* Render, dispatch.
 */
 
static void _pvp_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x185e27ff);
  switch (MODAL->stage) {
    case STAGE_CHOOSE_PLAYERS: pvp_render_choose_players(modal); break;
    case STAGE_REPORT: pvp_render_report(modal); break;
    case STAGE_FINAL: pvp_render_final(modal); break;
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_pvp={
  .name="pvp",
  .objlen=sizeof(struct modal_pvp),
  .del=_pvp_del,
  .init=_pvp_init,
  .focus=_pvp_focus,
  .notify=_pvp_notify,
  .update=_pvp_update,
  .render=_pvp_render,
};

#include "bellacopia.h"

struct g g={0};
extern struct font *g_font; // battle_glue.c
extern int g_framec;

/* Quit.
 */

void egg_client_quit(int status) {
  if (g.store.loaded) g.store.dirty=1; // Since the clocks don't dirty on ticks, dirty it now.
  store_save_if_dirty("save",4,1);
}

/* Init.
 */
 
int egg_client_init() {

  int fbw=0,fbh=0;
  egg_texture_get_size(&fbw,&fbh,1);
  if ((fbw!=FBW)||(fbh!=FBH)) {
    fprintf(stderr,"Expected %dx%d framebuffer (bellacopia.h), but got %dx%d (metadata)\n",FBW,FBH,fbw,fbh);
    return -1;
  }
  
  srand_auto();

  if (res_init()<0) return -1;
  if (game_init_targets()<0) return -1;
  
  if (!(g.font=font_new())) return -1;
  g_font=g.font;
  if (font_add_image(g.font,RID_image_font9_0020,0x0020)) return -1;
  
  return 0;
}

/* Notification.
 */
 
void egg_client_notify(int k,int v) {
  struct modal **p=g.modalv;
  int i=g.modalc;
  for (;i-->0;p++) {
    struct modal *modal=*p;
    if (modal->defunct) continue;
    if (modal->type->notify) modal->type->notify(modal,k,v);
  }
}

/* Update.
 */
 
void egg_client_update(double elapsed) {

  /* Record the prior input state and capture next.
   * Also some light universal mangling of input.
   */
  memcpy(g.pvinput,g.input,sizeof(g.input));
  egg_input_get_all(g.input,sizeof(g.input)/sizeof(g.input[0]));
  modal_pickside_apply(g.input);
  int *v=g.input;
  int i=sizeof(g.input)/sizeof(g.input[0]);
  for (;i-->0;v++) {
    // (L1,R1)==(L2,R2). Might come up if you've accepted default mapping and the guesser guessed wrong. We never use the 2's.
    if ((*v)&(EGG_BTN_L1|EGG_BTN_L2)) (*v)|=(EGG_BTN_L1|EGG_BTN_L2);
    if ((*v)&(EGG_BTN_R1|EGG_BTN_R2)) (*v)|=(EGG_BTN_R1|EGG_BTN_R2);
    // Also alias EAST to WEST -- we're not going to use EAST.
    if ((*v)&EGG_BTN_EAST) (*v)|=EGG_BTN_WEST;
  }
  
  modals_update(elapsed);
  modals_drop_defunct();
  
  if (!g.modalc) {
    if (!modal_spawn(&modal_type_hello,0,0)) {
      egg_terminate(1);
      return;
    }
  }
  
  store_save_if_dirty("save",4,0);
}

/* XXX Show current play time in the corner, while I time a few things.
 */
 
static void XXX_render_play_time() {
  if (!modal_get_topmost(&modal_type_story)) return;
  int s=(int)(store_get_clock(NS_clock_playtime)+store_get_clock(NS_clock_battletime)+store_get_clock(NS_clock_pausetime));
  int m=s/60; s%=60;
  if (m>99) s=m=99;
  int x=8,y=FBH-8;
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_tile(&g.graf,x,y,'0'+m/10,0); x+=8;
  graf_tile(&g.graf,x,y,'0'+m%10,0); x+=8;
  graf_tile(&g.graf,x,y,':',0); x+=8;
  graf_tile(&g.graf,x,y,'0'+s/10,0); x+=8;
  graf_tile(&g.graf,x,y,'0'+s%10,0); x+=8;
}

/* Render.
 */
 
void egg_client_render() {
  g.framec++;
  g_framec=g.framec;
  graf_reset(&g.graf);
  modals_render();
  //XXX_render_play_time();
  graf_flush(&g.graf);
}

/* Audio.
 */
 
void bm_song_force(int rid) {
  if (rid==g.song_playing) return;
  g.song_playing=rid;
  egg_play_song(1,rid,1,0.4,0.0);
}

void bm_song_gently(int rid) {
  // Was thinking of a cross-fade or something but meh. This is good.
  bm_song_force(rid);
}

void bm_song_poke() {
  int phonograph=store_get_fld16(NS_fld16_phonograph);
  if (phonograph) {
    // Phonograph overrides all.
    bm_song_gently(phonograph);
  } else if (g.songid_requested) {
    // Maps with explicit song.
    bm_song_gently(g.songid_requested);
  } else {
    // Outerworld.
    bm_song_gently(bm_song_for_outerworld());
  }
}

void bm_sound_pan(int rid,double pan) {
  const double BLACKOUT_INTERVAL=0.050;
  double now=egg_time_real();
  struct sound_blackout *oldest=0;
  struct sound_blackout *blackout=g.sound_blackoutv;
  int i=g.sound_blackoutc;
  for (;i-->0;blackout++) {
    if (blackout->rid==rid) {
      if (now-blackout->when<BLACKOUT_INTERVAL) return;
      blackout->when=now;
      egg_play_sound(rid,1.0,pan);
      return;
    }
    if (!oldest||(blackout->when<oldest->when)) oldest=blackout;
  }
  if (oldest&&(now-oldest->when>BLACKOUT_INTERVAL)) {
    oldest->rid=rid;
    oldest->when=now;
    egg_play_sound(rid,1.0,pan);
    return;
  }
  if (g.sound_blackoutc>=SOUND_BLACKOUT_LIMIT) return;
  blackout=g.sound_blackoutv+g.sound_blackoutc++;
  blackout->rid=rid;
  blackout->when=now;
  egg_play_sound(rid,1.0,pan);
}

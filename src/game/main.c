#include "bellacopia.h"

struct g g={0};

/* Quit.
 */

void egg_client_quit(int status) {
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
  
  if (!(g.font=font_new())) return -1;
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

  memcpy(g.pvinput,g.input,sizeof(g.input));
  egg_input_get_all(g.input,sizeof(g.input)/sizeof(g.input[0]));
  
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

/* Render.
 */
 
void egg_client_render() {
  graf_reset(&g.graf);
  modals_render();
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
  bm_song_force(rid);//TODO gentle song change
}

void bm_sound(int rid) {
  //TODO sound_blackout
  egg_play_sound(rid,1.0,0.0);
}

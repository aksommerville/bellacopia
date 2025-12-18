#include "game.h"

/* Minor errata.
 */
 
struct g g={0};

void egg_client_quit(int status) {
}

void egg_client_notify(int k,int v) {
}

/* Init.
 */
 
int egg_client_init() {

  int fbw=0,fbh=0;
  egg_texture_get_size(&fbw,&fbh,1);
  if ((fbw!=FBW)||(fbh!=FBH)) {
    fprintf(stderr,"Actual fb size %dx%d (per metadata) does not match expectation %dx%d (per game.h)\n",fbw,fbh,FBW,FBH);
    return -1;
  }
  
  if (res_init()<0) return -1;
  
  return 0;
}

/* Maintenance.
 */
 
void egg_client_update(double elapsed) {
  memcpy(g.pvinput,g.input,sizeof(g.input));
  egg_input_get_all(g.input,3);
  modal_update_all(elapsed);
  modal_drop_defunct();
  if (!g.modalc) {
    if (!modal_new_hello()) egg_terminate(1);
  }
}

void egg_client_render() {
  graf_reset(&g.graf);
  modal_render_all();
  graf_flush(&g.graf);
}

/* Audio.
 */
 
void bm_song(int rid,int repeat) {
  if (rid==g.song_playing) return;
  g.song_playing=rid;
  egg_play_song(1,rid,repeat,0.5f,0.0f);
}

void bm_sound(int rid,float pan) {
  if (rid<1) return;
  const double SOUND_BLACKOUT_TIME=0.075; // Below 50ms there can be trouble.
  double now=egg_time_real();
  struct sound_blackout *q=g.sound_blackoutv;
  struct sound_blackout *bo=q;
  int i=SOUND_BLACKOUT_LIMIT;
  for (;i-->0;q++) {
    if (q->rid==rid) {
      double elapsed=now-q->time;
      if (elapsed<SOUND_BLACKOUT_TIME) return;
      bo=q;
    } else if (q->time<bo->time) {
      bo=q;
    }
  }
  bo->time=now;
  bo->rid=rid;
  egg_play_sound(rid,1.0f,pan);
}

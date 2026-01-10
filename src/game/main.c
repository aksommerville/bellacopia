#include "game.h"

/* Minor errata.
 */
 
struct g g={0};

void egg_client_quit(int status) {
  store_save_if_dirty();
  store_jigsaw_save_if_dirty(1);
  inventory_save_if_dirty();
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
  
  if (!(g.font=font_new())) return -1;
  if (font_add_image(g.font,RID_image_font9_0020,0x0020)<0) return -1;
  
  srand_auto();
  
  store_load();
  store_jigsaw_load();
  
  if (0) { //XXX Starup hacks.
    // Acquiring inventory here will work, but will also clear any saved inventory and present an unexpected modal initially.
    //inventory_acquire(NS_itemid_candy,10);
    store_set(NS_fld_gold,10,123);
    store_set(NS_fld_greenfish,7,1);
    store_set(NS_fld_bluefish,7,2);
    store_set(NS_fld_redfish,7,73);
  }
  
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
    if (!modal_new_hello()) {
      egg_terminate(1);
      return;
    }
  }
  store_save_if_dirty();
  store_jigsaw_save_if_dirty(0);
  inventory_save_if_dirty();
}

void egg_client_render() {
  g.framec++;
  graf_reset(&g.graf);
  modal_render_all();
  graf_flush(&g.graf);
  
  // Maybe remove this in production. Track texture evictions on consecutive frames.
  // If it happens a lot, we need to increase graf's cache size.
  if (g.graf.texevictc!=g.pvtexevictc) {
    g.pvtexevictc=g.graf.texevictc;
    g.texchurnc++;
    if ((g.texchurnc==5)||!(g.texchurnc%100)) fprintf(stderr,"WARNING: Texture cache churn detected. %d consecutive frames with eviction.\n",g.texchurnc);
  } else {
    g.texchurnc=0;
  }
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

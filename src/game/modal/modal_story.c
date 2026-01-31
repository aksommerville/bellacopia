/* modal_story.c
 * We serve as the main controller for the outer world, also coordinating the larger session.
 * Everything useful should happen in our subordinates; keep this one as clean as possible.
 */

#include "game/bellacopia.h"

struct modal_story {
  struct modal hdr;
  int map_listener;
  int cell_listener;
};

#define MODAL ((struct modal_story*)modal)

/* Cleanup.
 */
 
static void _story_del(struct modal *modal) {
  camera_unlisten(MODAL->map_listener);
  camera_unlisten(MODAL->cell_listener);
}

/* Map exposure callbacks.
 */
 
static void story_cb_map_exposure(struct map *map,int focus,void *userdata) {
  struct modal *modal=userdata;
  switch (focus) {
    // If we need them: -1 is losing focus, and 0 is going out of view.
    case 1: game_welcome_map(map); break;
    case 2: game_focus_map(map); break;
  }
}

static void story_cb_cell_exposure(int x,int y,int w,int h,void *userdata) {
  struct modal *modal=userdata;
  spawner_expose(x,y,w,h);
}

/* Init.
 */
 
static int _story_init(struct modal *modal,const void *arg,int argc) {
  modal->opaque=1;
  modal->interactive=1;
  
  int use_save=1;
  if (arg&&(argc==sizeof(struct modal_args_story))) {
    const struct modal_args_story *ARG=arg;
    use_save=ARG->use_save;
  }
  if (game_reset(use_save)<0) return -1;
  
  if ((MODAL->map_listener=camera_listen_map(story_cb_map_exposure,modal))<0) return -1;
  if ((MODAL->cell_listener=camera_listen_cell(story_cb_cell_exposure,modal))<0) return -1;
  
  int mapid,col,row;
  if (maps_get_start_position(&mapid,&col,&row)<0) return -1;
  camera_cut(mapid,col,row,NS_transition_cut);
  
  return 0;
}

/* Focus.
 */
 
static void _story_focus(struct modal *modal,int focus) {
}

/* Update.
 */
 
static void _story_update(struct modal *modal,double elapsed) {

  // AUX1 to pause.
  if ((g.input[0]&EGG_BTN_AUX1)&&!(g.pvinput[0]&EGG_BTN_AUX1)) {
    modal_spawn(&modal_type_pause,0,0);
    return;
  }
  
  // Tick playtime.
  double *playtime=store_require_clock(NS_clock_playtime);
  if (playtime) (*playtime)+=elapsed;
  
  // Update sprites, then kill those that have run out of funk.
  int i=GRP(update)->sprc;
  while (i-->0) {
    struct sprite *sprite=GRP(update)->sprv[i];
    if (sprite->defunct) continue;
    if (sprite->type->update) sprite->type->update(sprite,elapsed);
  }
  sprite_group_kill_all(GRP(deathrow));
  
  // Other updatey things.
  game_update(elapsed);
  feet_update();
  camera_update(elapsed);
  
  if (g.gameover) {
    modal->defunct=1;
  }
}

/* Overlay with HP and gold.
 * TODO It's a fair bit of work to render and won't change often. Cache this into a temporary texture and redraw that on changes.
 */
 
static void story_render_overlay(struct modal *modal) {
  int hp=store_get_fld16(NS_fld16_hp);
  int hpmax=store_get_fld16(NS_fld16_hpmax);
  int gold=store_get_fld16(NS_fld16_gold);
  int goldmax=store_get_fld16(NS_fld16_goldmax);
  graf_set_image(&g.graf,RID_image_pause);
  
  graf_tile(&g.graf,9,9,0x17,0);
  const struct invstore *invstore=g.store.invstorev;
  int itemid=invstore->itemid;
  if (itemid) {
    const struct item_detail *detail=item_detail_for_itemid(itemid);
    if (detail) {
      uint8_t tileid=detail->tileid;
      switch (itemid) {
        case NS_itemid_potion: switch (invstore->quantity) {
            case 0: tileid=0x4a; break;
            // 1 is the default
            case 2: tileid=0x4b; break;
            case 3: tileid=0x4c; break;
          } break;
      }
      if (tileid) graf_tile(&g.graf,9,9,tileid,0);
    }
  }
  
  int x=23,y=5;
  int i=0;
  for (;i<hp;i++,x+=8) graf_tile(&g.graf,x,y,0x28,0);
  for (;i<hpmax;i++,x+=8) graf_tile(&g.graf,x,y,0x27,0);
  
  uint32_t color;
  if (gold<=0) color=0xb0b0b0ff;
  else if (gold>=goldmax) color=0x00ff00ff;
  else color=0xf08040ff;
  int digitc=(gold>=10000)?5:(gold>=1000)?4:(gold>=100)?3:(gold>=10)?2:1;
  x=22;
  y=14;
  graf_tile(&g.graf,x,y,0x29,0);
  y=13;
  x+=7+4*(digitc-1);
  for (i=digitc;i-->0;x-=4,gold/=10) graf_fancy(&g.graf,x,y,0x40+gold%10,0,0,NS_sys_tilesize,0,color);
  
  // Equipped item quantity goes last, so it can share a batch with the gold count.
  if (itemid&&invstore->limit) {
    int v=invstore->quantity;
    uint32_t color;
    if (v<=0) color=0xb0b0b0ff;
    else if (v>=invstore->limit) color=0x00ff00ff;
    else color=0xf08040ff;
    int digitc=(v>=100)?3:(v>=10)?2:1;
    x=14;
    y=21;
    for (i=digitc;i-->0;x-=4,v/=10) graf_fancy(&g.graf,x,y,0x40+v%10,0,0,NS_sys_tilesize,0,color);
  }
}

/* Render.
 */
 
static void _story_render(struct modal *modal) {
  camera_render();
  story_render_overlay(modal);
}

/* Type definition.
 */
 
const struct modal_type modal_type_story={
  .name="story",
  .objlen=sizeof(struct modal_story),
  .del=_story_del,
  .init=_story_init,
  .focus=_story_focus,
  .update=_story_update,
  .render=_story_render,
};

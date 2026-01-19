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
  //fprintf(stderr,"%s %d,%d,%d,%d\n",__func__,x,y,w,h);
  //TODO Call out for possible rsprite.
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
  
  if (use_save) {
    if (store_load("save",4)<0) return -1;
  } else {
    if (store_clear()<0) return -1;
  }
  
  sprites_reset();
  camera_reset();
  feet_reset();
  
  if ((MODAL->map_listener=camera_listen_map(story_cb_map_exposure,modal))<0) return -1;
  if ((MODAL->cell_listener=camera_listen_cell(story_cb_cell_exposure,modal))<0) return -1;
  
  int mapid,col,row;
  if (maps_get_start_position(&mapid,&col,&row)<0) return -1;
  camera_cut(mapid,col,row);
  
  return 0;
}

/* Focus.
 */
 
static void _story_focus(struct modal *modal,int focus) {
}

/* Update.
 */
 
static void _story_update(struct modal *modal,double elapsed) {
  
  // Update sprites, then kill those that have run out of funk.
  int i=GRP(update)->sprc;
  while (i-->0) {
    struct sprite *sprite=GRP(update)->sprv[i];
    if (sprite->defunct) continue;
    if (sprite->type->update) sprite->type->update(sprite,elapsed);
  }
  sprite_group_kill_all(GRP(deathrow));
  
  // Other updatey things.
  feet_update();
  camera_update(elapsed);
}

/* Render.
 */
 
static void _story_render(struct modal *modal) {
  camera_render();
  //TODO Overlay. That's not camera's problem, it's ours.
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

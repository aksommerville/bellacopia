#include "game/game.h"
#include "game/world/map.h"

struct modal_world {
  struct modal hdr;
  int x,y,z;//XXX testing maps
  struct map *map;
};

#define MODAL ((struct modal_world*)modal)

/* Cleanup.
 */
 
static void _world_del(struct modal *modal) {
}

/* XXX load map
 */
 
static void world_load_map(struct modal *modal) {
  fprintf(stderr,"%s %d,%d,%d\n",__func__,MODAL->x,MODAL->y,MODAL->z);
  MODAL->map=map_by_position(MODAL->x,MODAL->y,MODAL->z);
}

/* Init.
 */
 
static int _world_init(struct modal *modal) {
  modal->opaque=1;
  modal->interactive=1;
  MODAL->x=0;
  MODAL->y=0;
  MODAL->z=1;
  world_load_map(modal);
  return 0;
}

/* XXX Move to another map
 */
 
static void world_move_XXX(struct modal *modal,int dx,int dy) {
  MODAL->x+=dx;
  MODAL->y+=dy;
  world_load_map(modal);
}

/* Update.
 */
 
static void _world_update(struct modal *modal,double elapsed) {
  //XXX on dpad strokes, move to the next map
  #define BTN(tag) ((g.input[0]&EGG_BTN_##tag)&&!(g.pvinput[0]&EGG_BTN_##tag))
       if (BTN(LEFT)) world_move_XXX(modal,-1,0);
  else if (BTN(RIGHT)) world_move_XXX(modal,1,0);
  else if (BTN(UP)) world_move_XXX(modal,0,-1);
  else if (BTN(DOWN)) world_move_XXX(modal,0,1);
  #undef BTN
}

/* Render.
 */
 
static void _world_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x604010ff);
  
  //XXX
  if (MODAL->map) {
    graf_set_image(&g.graf,RID_image_meadow);
    const uint8_t *p=MODAL->map->v;
    int yi=NS_sys_maph;
    int dsty=NS_sys_tilesize>>1;
    for (;yi-->0;dsty+=NS_sys_tilesize) {
      int xi=NS_sys_mapw;
      int dstx=NS_sys_tilesize>>1;
      for (;xi-->0;dstx+=NS_sys_tilesize,p++) {
        graf_tile(&g.graf,dstx,dsty,*p,0);
      }
    }
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_world={
  .name="world",
  .objlen=sizeof(struct modal_world),
  .del=_world_del,
  .init=_world_init,
  .update=_world_update,
  .render=_world_render,
};

/* Public ctor.
 */
 
struct modal *modal_new_world() {
  struct modal *modal=modal_new(&modal_type_world);
  if (!modal) return 0;
  if (modal_push(modal)<0) {
    modal_del(modal);
    return 0;
  }
  //TODO further init?
  return modal;
}

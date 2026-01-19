#include "game/game.h"
#include "game/world/map.h"
#include "game/world/camera.h"
#include "game/sprite/sprite.h"

struct modal_world {
  struct modal hdr;
};

#define MODAL ((struct modal_world*)modal)

/* Cleanup.
 */
 
static void _world_del(struct modal *modal) {
}

/* Init.
 */
 
static int _world_init(struct modal *modal) {
  modal->opaque=1;
  modal->interactive=1;
  inventory_reset();
  if (camera_reset(RID_map_start)<0) return -1;
  return 0;
}

/* Update.
 */
 
static void _world_update(struct modal *modal,double elapsed) {

  if (g.bugspray>0.0) g.bugspray-=elapsed;

  sprites_update(elapsed);
  camera_update(elapsed);
  
  if ((g.input[1]&EGG_BTN_AUX1)&&!(g.pvinput[1]&EGG_BTN_AUX1)) {
    modal_new_pause();
  }
  
  if (g.deferred_battle.battletype) {
    bm_begin_battle(g.deferred_battle.battletype,g.deferred_battle.playerc,g.deferred_battle.handicap,g.deferred_battle.cb,g.deferred_battle.userdata);
    memset(&g.deferred_battle,0,sizeof(g.deferred_battle));
  }
}

/* Unsigned decimal integer, with tiles.
 */
 
static void world_render_decuint(int x,int y,int v) {
  if (v<0) v=0;
  else if (v>999999) v=999999;
  if (v>=100000) { graf_tile(&g.graf,x,y,0x40+(v/100000),0); x+=4; }
  if (v>= 10000) { graf_tile(&g.graf,x,y,0x40+(v/ 10000)%10,0); x+=4; }
  if (v>=  1000) { graf_tile(&g.graf,x,y,0x40+(v/  1000)%10,0); x+=4; }
  if (v>=   100) { graf_tile(&g.graf,x,y,0x40+(v/   100)%10,0); x+=4; }
  if (v>=    10) { graf_tile(&g.graf,x,y,0x40+(v/    10)%10,0); x+=4; }
  graf_tile(&g.graf,x,y,0x40+(v%10),0);
}

/* Render.
 */
 
static void _world_render(struct modal *modal) {

  // Camera does most of it: Terrain, sprites, weather.
  camera_render();
  
  /* We do the artificial overlay on top of that: HP, gold, equipped item, ...
   */
  int hp=store_get(NS_fld_hp,4);
  int hpmax=store_get(NS_fld_hpmax,4);
  int gold=store_get(NS_fld_gold,10);
  int greenfish=store_get(NS_fld_greenfish,7);
  int bluefish=store_get(NS_fld_bluefish,7);
  int redfish=store_get(NS_fld_redfish,7);
  int itemtile=tileid_for_item(g.equipped.itemid,g.equipped.quantity);
  graf_set_image(&g.graf,RID_image_pause);
  graf_tile(&g.graf,10,10,0x17,0);
  if (itemtile) {
    graf_tile(&g.graf,10,10,itemtile,0);
    if (g.equipped.limit) {
      world_render_decuint(21,14,g.equipped.quantity);
    }
  }
  int y=23;
  int x=6;
  int i=0;
  for (;i<hp;i++,x+=8) graf_tile(&g.graf,x,y,0x28,0);
  for (;i<hpmax;i++,x+=8) graf_tile(&g.graf,x,y,0x27,0);
  y+=8;
  if (gold) {
    graf_tile(&g.graf,6,y,0x29,0);
    world_render_decuint(13,y,gold);
    y+=8;
  }
  if (greenfish) {
    graf_tile(&g.graf,6,y,0x2a,0);
    world_render_decuint(13,y,greenfish);
    y+=8;
  }
  if (bluefish) {
    graf_tile(&g.graf,6,y,0x2b,0);
    world_render_decuint(13,y,bluefish);
    y+=8;
  }
  if (redfish) {
    graf_tile(&g.graf,6,y,0x2c,0);
    world_render_decuint(13,y,redfish);
    y+=8;
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

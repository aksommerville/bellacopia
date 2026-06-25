#include "game/bellacopia.h"

#define STATE_EMPTY    1 /* Ice cube. Dragon hasn't come here yet. */
#define STATE_OCCUPIED 2 /* Ice cube with a dragon on top. Ready for combat. */
#define STATE_MELTED   3 /* Just a puddle. */

struct sprite_icedragon {
  struct sprite hdr;
  int seq; // 1..6
  int state;
  int left; // We don't transform, usually. But there are separate tiles in OCCUPIED stage for facing left and right.
  int store_listener;
};

#define SPRITE ((struct sprite_icedragon*)sprite)

/* Cleanup.
 */
 
static void _icedragon_del(struct sprite *sprite) {
  store_unlisten(SPRITE->store_listener);
}

/* State changes, driven by our (seq) and global (iceseq).
 */

static void icedragon_update_state(struct sprite *sprite,int iceseq) {
  int nstate=0;
  if ((SPRITE->seq<1)||(SPRITE->seq>6)) {
    nstate=STATE_MELTED;
  } else {
    int iceseq=store_get_fld16(NS_fld16_iceseq);
    if (SPRITE->seq<=iceseq) { // Got me already.
      nstate=STATE_MELTED;
    } else if (SPRITE->seq==iceseq+1) { // I'm next.
      nstate=STATE_OCCUPIED;
    } else { // Ask again later.
      nstate=STATE_EMPTY;
    }
  }
  if (nstate==SPRITE->state) return;
  
  // Exit state.
  switch (SPRITE->state) {
    case STATE_MELTED: { // We don't normally exit MELTED state, but let's be consistent.
        sprite->layer=100;
        sprite_group_add(GRP(solid),sprite);
        sprite_group_add(GRP(moveable),sprite);
      } break;
  }
  
  SPRITE->state=nstate;
  
  // Enter state.
  switch (SPRITE->state) {
    case STATE_MELTED: {
        SPRITE->state=STATE_MELTED;
        sprite->layer=20;
        sprite_group_remove(GRP(solid),sprite);
        sprite_group_remove(GRP(moveable),sprite);
      } break;
  }
}

/* Store listener.
 */
 
static void icedragon_cb_store(char type,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if ((type=='6')&&(id==NS_fld16_iceseq)) {
    icedragon_update_state(sprite,value);
  }
}

/* Init.
 */
 
static int _icedragon_init(struct sprite *sprite) {
  SPRITE->seq=sprite->arg[0];
  SPRITE->store_listener=store_listen('6',icedragon_cb_store,sprite);
  icedragon_update_state(sprite,store_get_fld16(NS_fld16_iceseq));
  return 0;
}

/* Update.
 */
 
static void _icedragon_update(struct sprite *sprite,double elapsed) {
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    double dx=hero->x-sprite->x;
    if (dx<=-0.750) SPRITE->left=1;
    else if (dx>=0.750) SPRITE->left=0;
  }
}

/* Dot has defeated us in combat.
 */
 
static void icedragon_dot_wins(struct sprite *sprite) {

  store_set_fld16(NS_fld16_iceseq,SPRITE->seq);
  // Our state change will happen automatically due to that fld16 change.
  
  if (SPRITE->seq==6) {
    // I was the last Ice Dragon. Should there be some special fanfare?
  } else {
    // Show a decorative me, flying toward the next instance.
    struct sprite *inter=sprite_spawn(sprite->x,sprite->y,0,0,0,&sprite_type_icedragon_inter,0,0);
    fprintf(stderr,"%s: inter=%p\n",__func__,inter);
  }
}

/* Collide.
 */
 
static void _icedragon_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->state!=STATE_OCCUPIED) return;
  //TODO Enter battle.
  icedragon_dot_wins(sprite);
}

/* Render.
 */
 
static void _icedragon_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  switch (SPRITE->state) {
    case STATE_EMPTY: {
        graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
      } break;
    case STATE_OCCUPIED: if (SPRITE->left) {
        graf_tile(&g.graf,x,y,sprite->tileid+0x02,sprite->xform);
        graf_tile(&g.graf,x,y-NS_sys_tilesize,sprite->tileid+0xf2,sprite->xform);
      } else {
        graf_tile(&g.graf,x,y,sprite->tileid+0x01,sprite->xform);
        graf_tile(&g.graf,x,y-NS_sys_tilesize,sprite->tileid+0xf1,sprite->xform);
      } break;
    case STATE_MELTED: {
        graf_tile(&g.graf,x,y,sprite->tileid-0x10,sprite->xform);
      } break;
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_icedragon={
  .name="icedragon",
  .objlen=sizeof(struct sprite_icedragon),
  .del=_icedragon_del,
  .init=_icedragon_init,
  .update=_icedragon_update,
  .collide=_icedragon_collide,
  .render=_icedragon_render,
};

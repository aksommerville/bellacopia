#include "game/bellacopia.h"

#define STATE_EMPTY    0 /* Ice cube. Dragon hasn't come here yet. */
#define STATE_OCCUPIED 1 /* Ice cube with a dragon on top. Ready for combat. */
#define STATE_MELTED   2 /* Just a puddle. */

struct sprite_icedragon {
  struct sprite hdr;
  int seq; // 1..6
  int state;
  int left; // We don't transform, usually. But there are separate tiles in OCCUPIED stage for facing left and right.
};

#define SPRITE ((struct sprite_icedragon*)sprite)

/* Do all the extra stuff that puts us in MELTED state.
 */
 
static void icedragon_set_melted(struct sprite *sprite) {
  SPRITE->state=STATE_MELTED;
  sprite->layer=20;
  sprite_group_remove(GRP(solid),sprite);
  sprite_group_remove(GRP(moveable),sprite);
}

/* Init.
 */
 
static int _icedragon_init(struct sprite *sprite) {
  SPRITE->seq=sprite->arg[0];
  
  if ((SPRITE->seq<1)||(SPRITE->seq>6)) {
    SPRITE->state=STATE_MELTED;
  } else {
    int iceseq=store_get_fld16(NS_fld16_iceseq);
    if (SPRITE->seq<=iceseq) { // Got me already.
      SPRITE->state=STATE_MELTED;
    } else if (SPRITE->seq==iceseq+1) { // I'm next.
      SPRITE->state=STATE_OCCUPIED;
    } else { // Ask again later.
      SPRITE->state=STATE_EMPTY;
    }
  }
  
  if (SPRITE->state==STATE_MELTED) {
    icedragon_set_melted(sprite);
  }
  
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

/* Collide.
 */
 
static void _icedragon_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->state!=STATE_OCCUPIED) return;
  //TODO Enter battle. If Dot wins, update iceseq like this, and either animate me flying to the next spot or do whatever it is we do at the end.
  // Is it possible for the next instance of this sprite to be in scope when I change?
  // If so, it might not be a big deal. Just like, he's going there but he hasn't arrived yet.
  fprintf(stderr,"%s %s\n",__func__,other?other->type->name:"null");
  int iceseq=store_get_fld16(NS_fld16_iceseq);
  iceseq=SPRITE->seq;
  store_set_fld16(NS_fld16_iceseq,iceseq);
  icedragon_set_melted(sprite);
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
  .init=_icedragon_init,
  .update=_icedragon_update,
  .collide=_icedragon_collide,
  .render=_icedragon_render,
};

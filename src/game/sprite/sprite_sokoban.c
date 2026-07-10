/* sprite_sokoban.c
 * Controller of the diegetic sokoban game.
 * The blocks are regular 'pushable' but we look for a specific rid (sokoblock).
 */
 
#include "game/bellacopia.h"

#define POLL_INTERVAL 0.250 /* No sense polling the boxes every frame. */

struct sprite_sokoban {
  struct sprite hdr;
  int x,y,w,h; // Target field in plane meters. We must be instantiated at the top left corner.
  int fldid;
  int finished;
  double pollclock;
  uint8_t tileid_off;
};

#define SPRITE ((struct sprite_sokoban*)sprite)

static void _sokoban_del(struct sprite *sprite) {
}

static int _sokoban_init(struct sprite *sprite) {
  SPRITE->x=(int)sprite->x;
  SPRITE->y=(int)sprite->y;
  SPRITE->w=sprite->arg[0];
  SPRITE->h=sprite->arg[1];
  if ((SPRITE->w<1)||(SPRITE->h<1)) {
    fprintf(stderr,"%s: Invalid dimensions\n",__func__);
    return -1;
  }
  SPRITE->fldid=(sprite->arg[2]<<8)|sprite->arg[3];
  if (store_get_fld(SPRITE->fldid)) {
    SPRITE->finished=1;
  }
  
  // And just to be super correct about it: Read the sokoblock's resource to find (tileid_off).
  const void *blockv;
  int blockc=res_get(&blockv,EGG_TID_sprite,RID_sprite_sokoblock);
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,blockv,blockc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      if (cmd.opcode==CMD_sprite_tile) {
        SPRITE->tileid_off=cmd.arg[0];
        break;
      }
    }
  }
  
  return 0;
}

static void sokoban_poll(struct sprite *sprite) {
  int onc=0,offc=0;
  struct sprite **otherp=GRP(moveable)->sprv;
  int otheri=GRP(moveable)->sprc;
  for (;otheri-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->rid!=RID_sprite_sokoblock) continue;
    int dx=(int)other->x-SPRITE->x;
    int dy=(int)other->y-SPRITE->y;
    if ((dx<0)||(dx>=SPRITE->w)||(dy<0)||(dy>=SPRITE->h)) {
      offc++;
      other->tileid=SPRITE->tileid_off;
    } else {
      onc++;
      other->tileid=SPRITE->tileid_off+1;
    }
  }
  if (onc>=SPRITE->w*SPRITE->h) { // Done!
    bm_sound(RID_sound_secret);
    SPRITE->finished=1;
    store_set_fld(SPRITE->fldid,1);
    g.camera.mapsdirty=1;
  }
}

static void _sokoban_update(struct sprite *sprite,double elapsed) {
  if (!SPRITE->finished) {
    if (SPRITE->pollclock>0.0) {
      SPRITE->pollclock-=elapsed;
    } else {
      SPRITE->pollclock+=POLL_INTERVAL;
      sokoban_poll(sprite);
    }
  }
}

static void _sokoban_render(struct sprite *sprite,int x,int y) {
  // dummy, just in case i accidentally put it in the visible group.
}

const struct sprite_type sprite_type_sokoban={
  .name="sokoban",
  .objlen=sizeof(struct sprite_sokoban),
  .del=_sokoban_del,
  .init=_sokoban_init,
  .update=_sokoban_update,
  .render=_sokoban_render,
};

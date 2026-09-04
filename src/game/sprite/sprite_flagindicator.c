/* sprite_flagindicator.c
 * Tile changes according to a flag.
 */
 
#include "game/bellacopia.h"

struct sprite_flagindicator {
  struct sprite hdr;
  int fldid;
  int delta;
  uint8_t tileid0;
  int store_listener;
};

#define SPRITE ((struct sprite_flagindicator*)sprite)

static void _flagindicator_del(struct sprite *sprite) {
  store_unlisten(SPRITE->store_listener);
}

static void flagindicator_cb_store(char type,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if ((type=='f')&&(id==SPRITE->fldid)) {
    sprite->tileid=SPRITE->tileid0+(value?SPRITE->delta:0);
  }
}

static int _flagindicator_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->fldid=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->delta=sprite->arg[2];
  SPRITE->store_listener=store_listen('f',flagindicator_cb_store,sprite);
  flagindicator_cb_store('f',SPRITE->fldid,store_get_fld(SPRITE->fldid),sprite);
  return 0;
}

const struct sprite_type sprite_type_flagindicator={
  .name="flagindicator",
  .objlen=sizeof(struct sprite_flagindicator),
  .del=_flagindicator_del,
  .init=_flagindicator_init,
};

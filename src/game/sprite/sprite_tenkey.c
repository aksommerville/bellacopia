/* sprite_tenkey.c
 * Displays some fld16 in 3 unsigned digits.
 * Non-interactive. You probably also want a "bump" command on the same cell.
 */
 
#include "game/bellacopia.h"

struct sprite_tenkey {
  struct sprite hdr;
  int fld16;
  int value;
  int store_listener;
};

#define SPRITE ((struct sprite_tenkey*)sprite)

static void _tenkey_del(struct sprite *sprite) {
  store_unlisten(SPRITE->store_listener);
}

static void _tenkey_cb_store(char type,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if ((type=='6')&&(id==SPRITE->fld16)) {
    SPRITE->value=value;
  }
}

static int _tenkey_init(struct sprite *sprite) {
  SPRITE->fld16=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->value=store_get_fld16(SPRITE->fld16);
  SPRITE->store_listener=store_listen('6',_tenkey_cb_store,sprite);
  return 0;
}

static void _tenkey_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x,y,sprite->tileid,0);
  // Digits are spaced horizontally by 4 pixels, and the most significant is aligned with the base tile.
  if (SPRITE->value>=100) graf_tile(&g.graf,x,y,sprite->tileid+1+(SPRITE->value/100)%10,0);
  if (SPRITE->value>=10) graf_tile(&g.graf,x+4,y,sprite->tileid+1+(SPRITE->value/10)%10,0);
  graf_tile(&g.graf,x+8,y,sprite->tileid+1+SPRITE->value%10,0);
}

const struct sprite_type sprite_type_tenkey={
  .name="tenkey",
  .objlen=sizeof(struct sprite_tenkey),
  .del=_tenkey_del,
  .init=_tenkey_init,
  .render=_tenkey_render,
};

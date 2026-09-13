/* sprite_letfloor_ref.c
 * Displays a 4-digit password.
 */
 
#include "game/bellacopia.h"

struct sprite_letfloor_ref {
  struct sprite hdr;
  int strix; // strings:item
  int fldid; // Set when the password was entered and door is open.
  const char *text;
  int textc;
};

#define SPRITE ((struct sprite_letfloor_ref*)sprite)

/* Init.
 */
 
static int _letfloor_ref_init(struct sprite *sprite) {
  SPRITE->strix=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->fldid=(sprite->arg[2]<<8)|sprite->arg[3];
  SPRITE->textc=text_get_string(&SPRITE->text,RID_strings_item,SPRITE->strix);
  return 0;
}

/* Render.
 */
 
static void _letfloor_ref_render(struct sprite *sprite,int dstx,int dsty) {
  // This seems a bit crude, I thought I'd be writing a proper thing after. But it works ok.
  graf_set_image(&g.graf,RID_image_fonttiles);
  const char *v=SPRITE->text;
  int i=SPRITE->textc;
  for (;i-->0;v++,dstx+=8) graf_tile(&g.graf,dstx,dsty,*v,0);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_letfloor_ref={
  .name="letfloor_ref",
  .objlen=sizeof(struct sprite_letfloor_ref),
  .init=_letfloor_ref_init,
  .render=_letfloor_ref_render,
};

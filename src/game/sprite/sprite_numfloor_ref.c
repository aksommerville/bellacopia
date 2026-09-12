/* sprite_numfloor_ref.c
 * Displays a 4-digit password.
 */
 
#include "game/bellacopia.h"

struct sprite_numfloor_ref {
  struct sprite hdr;
  int fld16id; // Contains my password. I'm responsible for generating it.
  int fldid; // Set when the password was entered and door is open.
  int password; // Value under (fld16id). I'm the only one allowed to change it.
};

#define SPRITE ((struct sprite_numfloor_ref*)sprite)

/* Generic password utilities.
 */
 
static int numfloor_valid_password(int v) {
  // Must be in 1..9999 with no duplicate digits.
  if ((v<1)||(v>9999)) return 0;
  int k=v/1000;
  int h=(v/100)%10;
  int t=(v/10)%10;
  int o=v%10;
  if (k==h) return 0;
  if (k==t) return 0;
  if (k==o) return 0;
  if (h==t) return 0;
  if (h==o) return 0;
  if (t==o) return 0;
  return 1;
}

static int numfloor_generate_password() {
  uint8_t dv[10]={0,1,2,3,4,5,6,7,8,9};
  int dc=10;
  int base=1;
  int v=0;
  for (;base<10000;base*=10) {
    int p=rand()%dc;
    v+=dv[p]*base;
    dc--;
    memmove(dv+p,dv+p+1,dc-p);
  }
  return v;
}

/* Init.
 */
 
static int _numfloor_ref_init(struct sprite *sprite) {
  SPRITE->fld16id=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->fldid=(sprite->arg[2]<<8)|sprite->arg[3];
  SPRITE->password=store_get_fld16(SPRITE->fld16id);
  
  // Generate password if needed.
  if (!numfloor_valid_password(SPRITE->password)) {
    SPRITE->password=numfloor_generate_password();
    store_set_fld16(SPRITE->fld16id,SPRITE->password);
  }

  return 0;
}

/* Render.
 */
 
static void _numfloor_ref_render(struct sprite *sprite,int dstx,int dsty) {
  // Four tiles spaced 5 pixels apart; first aligns to my natural position.
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,dstx,dsty,sprite->tileid+(SPRITE->password/1000)%10,0); dstx+=5;
  graf_tile(&g.graf,dstx,dsty,sprite->tileid+(SPRITE->password/100)%10,0); dstx+=5;
  graf_tile(&g.graf,dstx,dsty,sprite->tileid+(SPRITE->password/10)%10,0); dstx+=5;
  graf_tile(&g.graf,dstx,dsty,sprite->tileid+SPRITE->password%10,0);
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_numfloor_ref={
  .name="numfloor_ref",
  .objlen=sizeof(struct sprite_numfloor_ref),
  .init=_numfloor_ref_init,
  .render=_numfloor_ref_render,
};

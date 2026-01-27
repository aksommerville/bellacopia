/* sprite_toast.c
 * Quick little message that pops up above the hero's head and disappears fast.
 */
 
#include "game/bellacopia.h"

#define TTL 0.500
#define FADETIME 0.250
#define SPEED 1.000

struct sprite_toast {
  struct sprite hdr;
  int texid,w,h;
  double ttl;
};

#define SPRITE ((struct sprite_toast*)sprite)

static void _toast_del(struct sprite *sprite) {
  egg_texture_del(SPRITE->texid);
}

static int _toast_init(struct sprite *sprite) {
  sprite->layer=200;
  SPRITE->ttl=TTL;
  sprite_group_add(GRP(visible),sprite);
  sprite_group_add(GRP(update),sprite);
  return 0;
}

static void _toast_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->ttl-=elapsed)<=0.0) {
    sprite_kill_soon(sprite);
  }
  sprite->y-=SPEED*elapsed;
}

static void _toast_render(struct sprite *sprite,int x,int y) {
  graf_set_input(&g.graf,SPRITE->texid);
  if (SPRITE->ttl<FADETIME) {
    int alpha=(int)((SPRITE->ttl*255.0)/FADETIME);
    if (alpha<1) return;
    if (alpha<0xff) graf_set_alpha(&g.graf,alpha);
  }
  graf_decal(&g.graf,x-(SPRITE->w>>1),y-(SPRITE->h>>1),0,0,SPRITE->w,SPRITE->h);
  graf_set_alpha(&g.graf,0xff);
}

const struct sprite_type sprite_type_toast={
  .name="toast",
  .objlen=sizeof(struct sprite_toast),
  .del=_toast_del,
  .init=_toast_init,
  .update=_toast_update,
  .render=_toast_render,
};

int sprite_toast_set_text(struct sprite *sprite,const char *src,int srcc) {
  if (!sprite||(sprite->type!=&sprite_type_toast)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  egg_texture_del(SPRITE->texid);
  SPRITE->w=SPRITE->h=0;
  SPRITE->texid=font_render_to_texture(0,g.font,src,srcc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&SPRITE->w,&SPRITE->h,SPRITE->texid);
  return 0;
}

struct sprite *sprite_toast_get_any() {
  struct sprite **p=GRP(update)->sprv;
  int i=GRP(update)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    if (sprite->type==&sprite_type_toast) return sprite;
  }
  return 0;
}

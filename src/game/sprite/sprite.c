#include "game/game.h"
#include "sprite.h"

/* Globals.
 */
 
static struct {
  struct sprite **v;
  int c,a;
  struct sprite *hero; // WEAK, managed at new/del
} sprites={0};

/* Delete.
 */
 
static void sprite_del(struct sprite *sprite) {
  if (!sprite) return;
  if (sprite==sprites.hero) sprites.hero=0;
  if (sprite->type->del) sprite->type->del(sprite);
  free(sprite);
}

/* Spawn.
 */

struct sprite *sprite_spawn(
  double x,double y,
  int rid,
  const uint8_t *arg,
  const struct sprite_type *type,
  const void *serial,int serialc
) {
  
  // (rid) but no (serial), fetch the resource.
  if (rid&&!serialc) serialc=res_get(&serial,EGG_TID_sprite,rid);
  
  // (serial) but no (type), extract the type.
  if (serialc&&!type) type=sprite_type_from_serial(serial,serialc);
  
  // And if we still don't have a type, panic.
  if (!type) return 0;
  
  // Allocate and set the obvious things.
  struct sprite *sprite=calloc(1,type->objlen);
  if (!sprite) return 0;
  sprite->type=type;
  sprite->x=x;
  sprite->y=y;
  sprite->rid=rid;
  sprite->serial=serial;
  sprite->serialc=serialc;
  sprite->arg=arg?arg:(const uint8_t*)"\0\0\0\0";
  sprite->physics=(1<<NS_physics_solid)|(1<<NS_physics_water)|(1<<NS_physics_hole)|(1<<NS_physics_cliff)|(1<<NS_physics_hookable);
  sprite->hbl=sprite->hbt=-0.5;
  sprite->hbr=sprite->hbb=0.5;
  
  // If we have serial, apply standard commands.
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,serial,serialc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_sprite_solid: sprite->solid=1; break;
        case CMD_sprite_image: sprite->imageid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
        case CMD_sprite_tile: sprite->tileid=cmd.arg[0]; sprite->xform=cmd.arg[1]; break;
        case CMD_sprite_layer: sprite->layer=(int16_t)((cmd.arg[0]<<8)|cmd.arg[1]); break;
        case CMD_sprite_physics: sprite->physics=(cmd.arg[0]<<24)|(cmd.arg[1]<<16)|(cmd.arg[2]<<8)|cmd.arg[3]; break;
        case CMD_sprite_hitbox: {
            sprite->hbl=(double)(int8_t)cmd.arg[0]/(double)NS_sys_tilesize;
            sprite->hbr=(double)(int8_t)cmd.arg[1]/(double)NS_sys_tilesize;
            sprite->hbt=(double)(int8_t)cmd.arg[2]/(double)NS_sys_tilesize;
            sprite->hbb=(double)(int8_t)cmd.arg[3]/(double)NS_sys_tilesize;
          } break;
      }
    }
  }
  
  // Let type initialize it.
  if (type->init&&((type->init(sprite)<0)||sprite->defunct)) {
    sprite_del(sprite);
    return 0;
  }
  
  // Add to the global list.
  if (sprites.c>=sprites.a) {
    int na=sprites.a+64;
    if (na>INT_MAX/sizeof(void*)) { sprite_del(sprite); return 0; }
    void *nv=realloc(sprites.v,sizeof(void*)*na);
    if (!nv) { sprite_del(sprite); return 0; }
    sprites.v=nv;
    sprites.a=na;
  }  
  sprites.v[sprites.c++]=sprite;
  
  if (type==&sprite_type_hero) sprites.hero=sprite;
  
  return sprite;
}

/* Look up live sprites.
 */

int sprite_is_resident(const struct sprite *sprite) {
  if (!sprite) return 0;
  struct sprite **p=sprites.v;
  int i=sprites.c;
  for (;i-->0;p++) if (*p==sprite) return 1;
  return 0;
}
 
struct sprite *sprites_get_hero() {
  return sprites.hero;
}

struct sprite *sprite_by_arg(const void *arg) {
  struct sprite **p=sprites.v;
  int i=sprites.c;
  for (;i-->0;p++) {
    if ((*p)->arg==arg) return *p;
  }
  return 0;
}

struct sprite *sprite_by_type(const struct sprite_type *type) {
  struct sprite **p=sprites.v;
  int i=sprites.c;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->type==type) return sprite;
  }
  return 0;
}

int sprites_get_all(struct sprite ***dstpp) {
  *dstpp=sprites.v;
  return sprites.c;
}

/* Update all.
 */

void sprites_update(double elapsed) {
  int i=sprites.c;
  while (i-->0) {
    struct sprite *sprite=sprites.v[i];
    if (sprite->defunct) continue;
    if (sprite->type->update) sprite->type->update(sprite,elapsed);
  }
  struct sprite **p=sprites.v+sprites.c-1;
  for (i=sprites.c;i-->0;p--) {
    struct sprite *sprite=*p;
    if (!sprite->defunct) continue;
    sprites.c--;
    memmove(p,p+1,sizeof(void*)*(sprites.c-i));
    sprite_del(sprite);
  }
}

/* Two passes of a cocktail sort.
 */
 
static int sprites_rendercmp(const struct sprite *a,const struct sprite *b) {
  if (a->layer<b->layer) return -1;
  if (a->layer>b->layer) return 1;
  if (a->y<b->y) return -1;
  if (a->y>b->y) return 1;
  return 0;
}
 
static void sprites_sort_partial() {
  if (sprites.c<2) return;
  {
    int i=1,done=1;
    for (;i<sprites.c;i++) {
      struct sprite *a=sprites.v[i-1];
      struct sprite *b=sprites.v[i];
      if (sprites_rendercmp(a,b)>0) {
        done=0;
        sprites.v[i-1]=b;
        sprites.v[i]=a;
      }
    }
    if (done) return;
  }
  int i=sprites.c-1;
  while (i-->0) {
    struct sprite *a=sprites.v[i];
    struct sprite *b=sprites.v[i+1];
    if (sprites_rendercmp(a,b)>0) {
      sprites.v[i]=b;
      sprites.v[i+1]=a;
    }
  }
}

/* Render all.
 */
 
void sprites_render(int scrollx,int scrolly) {
  sprites_sort_partial();
  struct sprite **p=sprites.v;
  int i=sprites.c;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    int dstx=lround(sprite->x*NS_sys_tilesize)-scrollx;
    int dsty=lround(sprite->y*NS_sys_tilesize)-scrolly;
    //TODO Cull if far off?
    if (sprite->type->render) {
      sprite->type->render(sprite,dstx,dsty);
    } else {
      graf_set_image(&g.graf,sprite->imageid);
      graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
    }
  }
}

/* Clear global list.
 */

void sprites_clear() {
  while (sprites.c>0) {
    sprites.c--;
    sprite_del(sprites.v[sprites.c]);
  }
}

/* Type by id.
 */

const struct sprite_type *sprite_type_by_id(int sprtype) {
  switch (sprtype) {
    #define _(tag) case NS_sprtype_##tag: return &sprite_type_##tag;
    FOR_EACH_SPRTYPE
    #undef _
  }
  return 0;
}

/* Type from full resource.
 */
 
const struct sprite_type *sprite_type_from_serial(const void *src,int srcc) {
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,src,srcc)<0) return 0;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_sprite_type) return sprite_type_by_id((cmd.arg[0]<<8)|cmd.arg[1]);
  }
  return 0;
}

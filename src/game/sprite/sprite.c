#include "game/bellacopia.h"

static const uint8_t default_sprite_arg[4]={0};

/* Reset globals.
 */

void sprites_reset() {
  struct sprite_group *group=g.sprites.grpv;
  int i=32;
  for (;i-->0;group++) sprite_group_clear(group);
  
  // Most groups default to MODE_ADDR, zero. Pick off the others.
  g.sprites.grpv[NS_sprgrp_visible].mode=SPRITE_GROUP_MODE_RENDER;
}

/* Find sprite by arg.
 */
 
struct sprite *find_sprite_by_arg(const void *arg) {
  struct sprite **p=GRP(keepalive)->sprv;
  int i=GRP(keepalive)->sprc;
  for (;i-->0;p++) {
    if ((*p)->arg==arg) {
      if ((*p)->defunct) continue;
      return *p;
    }
  }
  return 0;
}

/* Delete sprite.
 */
 
void sprite_del(struct sprite *sprite) {
  if (!sprite) return;
  if (sprite->refc-->1) return;
  if (sprite->type->del) sprite->type->del(sprite);
  if (sprite->grpc) fprintf(stderr,"!!! WARNING !!! Deleting sprite %p with grpc==%d\n",sprite,sprite->grpc);
  if (sprite->grpv) free(sprite->grpv);
  free(sprite);
}

/* Apply generic commands to new sprite.
 */
 
static int sprite_apply_generic_commands(struct sprite *sprite) {
  if (!sprite->cmdc) return 0;
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,sprite->cmd,sprite->cmdc)<0) return -1;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_sprite_image: sprite->imageid=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_sprite_tile: sprite->tileid=cmd.arg[0]; sprite->xform=cmd.arg[1]; break;
      case CMD_sprite_layer: sprite->layer=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_sprite_physics: sprite->physics=(cmd.arg[0]<<24)|(cmd.arg[1]<<16)|(cmd.arg[2]<<8)|cmd.arg[3]; break;
      case CMD_sprite_hitbox: {
          sprite->hbl=(double)(int8_t)cmd.arg[0]/(double)NS_sys_tilesize;
          sprite->hbr=(double)(int8_t)cmd.arg[1]/(double)NS_sys_tilesize;
          sprite->hbt=(double)(int8_t)cmd.arg[2]/(double)NS_sys_tilesize;
          sprite->hbb=(double)(int8_t)cmd.arg[3]/(double)NS_sys_tilesize;
        } break;
      case CMD_sprite_groups: {
          uint32_t mask=(cmd.arg[0]<<24)|(cmd.arg[1]<<16)|(cmd.arg[2]<<8)|cmd.arg[3];
          uint32_t bit=1;
          int i=0;
          for (;i<32;i++,bit<<=1) {
            if (mask&bit) {
              if (sprite_group_add(g.sprites.grpv+i,sprite)<0) return -1;
            }
          }
        } break;
    }
  }
  return 0;
}

static int sprtype_from_res(const void *src,int srcc) {
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,src,srcc)<0) return NS_sprtype_dummy;
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_sprite_type) {
      return (cmd.arg[0]<<8)|cmd.arg[1];
    }
  }
  return NS_sprtype_dummy;
}

/* New sprite.
 */
 
struct sprite *sprite_new(
  double x,double y,
  int rid,
  const void *arg,int argc,
  const struct sprite_type *type,
  const void *cmd,int cmdc
) {
  //fprintf(stderr,"%s(%f,%f,rid=%d,argc=%d,type=%s,cmdc=%d)\n",__func__,x,y,rid,argc,type?type->name:"NULL",cmdc);
  if (!arg||!argc) {
    arg=default_sprite_arg;
    argc=4;
  } else if (argc<4) {
    return 0;
  }
  
  if (!cmdc) {
    if (rid) {
      cmdc=res_get(&cmd,EGG_TID_sprite,rid);
    } else {
      cmd=0;
      cmdc=0;
    }
  }
  if (!type) {
    int sprtype=sprtype_from_res(cmd,cmdc);
    if (!(type=sprite_type_by_id(sprtype))) return 0;
  }
  
  struct sprite *sprite=calloc(1,type->objlen);
  if (!sprite) return 0;
  
  sprite->type=type;
  sprite->refc=1;
  sprite->rid=rid;
  sprite->arg=arg;
  sprite->argc=argc;
  sprite->cmd=cmd;
  sprite->cmdc=cmdc;
  sprite->x=x;
  sprite->y=y;
  sprite->z=g.camera.z;
  sprite->layer=100;
  sprite->hbl=sprite->hbt=-0.5;
  sprite->hbr=sprite->hbb=0.5;
  
  if (sprite_group_add(GRP(keepalive),sprite)<0) {
    sprite_del(sprite);
    return 0;
  }
  
  if (sprite_apply_generic_commands(sprite)<0) {
    sprite_kill(sprite);
    sprite_del(sprite);
    return 0;
  }
  
  if (type->init&&((type->init(sprite)<0)||sprite_group_has(GRP(deathrow),sprite))) {
    sprite_kill(sprite);
    sprite_del(sprite);
    return 0;
  }
  
  return sprite;
}
  
/* Retain sprite.
 */
 
int sprite_ref(struct sprite *sprite) {
  if (!sprite) return -1;
  if (sprite->refc<1) return -1;
  if (sprite->refc>=INT_MAX) return -1;
  sprite->refc++;
  return 0;
}

/* Spawn sprite.
 */

struct sprite *sprite_spawn(
  double x,double y,
  int rid,
  const void *arg,int argc,
  const struct sprite_type *type,
  const void *cmd,int cmdc
) {
  struct sprite *sprite=sprite_new(x,y,rid,arg,argc,type,cmd,cmdc);
  if (!sprite) return 0;
  sprite_del(sprite);
  return sprite;
}

/* Defunct sprite.
 */

void sprite_kill_soon(struct sprite *sprite) {
  if (!sprite) return;
  sprite->defunct=1;
  sprite_group_add(GRP(deathrow),sprite);
}

/* Sprite type by id.
 */

const struct sprite_type *sprite_type_by_id(int sprtype) {
  switch (sprtype) {
    #define _(tag) case NS_sprtype_##tag: return &sprite_type_##tag;
    FOR_EACH_sprtype
    #undef _
  }
  return 0;
}

/* Delete group.
 */

void sprite_group_del(struct sprite_group *group) {
  if (!group) return;
  if (!group->refc) return; // Immortal.
  if (group->refc-->1) return;
  if (group->sprc) fprintf(stderr,"!!! WARNING !!! Deleting group %p with sprc==%d\n",group,group->sprc);
  if (group->sprv) free(group->sprv);
  free(group);
}

/* New group.
 */
 
struct sprite_group *sprite_group_new() {
  struct sprite_group *group=calloc(1,sizeof(struct sprite_group));
  if (!group) return 0;
  group->refc=1;
  return group;
}

/* Retain group.
 */
 
int sprite_group_ref(struct sprite_group *group) {
  if (!group) return -1;
  if (!group->refc) return 0; // Immortal.
  if (group->refc<0) return -1;
  if (group->refc>=INT_MAX) return -1;
  group->refc++;
  return 0;
}

/* List primitives, in both group and sprite.
 */
 
static int sprite_grpv_search(const struct sprite *sprite,const struct sprite_group *group) {
  int lo=0,hi=sprite->grpc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct sprite_group *q=sprite->grpv[ck];
         if (q<group) hi=ck;
    else if (q>group) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int group_sprv_search(const struct sprite_group *group,const struct sprite *sprite) {
  switch (group->mode) {
    case SPRITE_GROUP_MODE_ADDR: {
        int lo=0,hi=group->sprc;
        while (lo<hi) {
          int ck=(lo+hi)>>1;
          const struct sprite *q=group->sprv[ck];
               if (q<sprite) hi=ck;
          else if (q>sprite) lo=ck+1;
          else return ck;
        }
        return -lo-1;
      }
    case SPRITE_GROUP_MODE_RENDER: {
        // It's a brute search, but also try to capture the ideal insertion point.
        int i=0,before=-1;
        for (;i<group->sprc;i++) {
          const struct sprite *q=group->sprv[i];
          if (q==sprite) return i;
          if (before<0) {
            if (q->layer>sprite->layer) before=i;
            else if ((q->layer==sprite->layer)&&(q->y>sprite->y)) before=i;
          }
        }
        if (before<0) return -group->sprc-1;
        return -before-1;
      }
    default: {
        int i=group->sprc;
        struct sprite **p=group->sprv+i-1;
        for (;i-->0;p--) {
          if (*p==sprite) return i;
        }
        return -group->sprc-1;
      }
  }
  return -1;
}

static int sprite_grpv_insert(struct sprite *sprite,int p,struct sprite_group *group) {
  if ((p<0)||(p>sprite->grpc)) return -1;
  if (sprite->grpc>=sprite->grpa) {
    int na=sprite->grpa+8;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(sprite->grpv,sizeof(void*)*na);
    if (!nv) return -1;
    sprite->grpv=nv;
    sprite->grpa=na;
  }
  if (sprite_group_ref(group)<0) return -1;
  memmove(sprite->grpv+p+1,sprite->grpv+p,sizeof(void*)*(sprite->grpc-p));
  sprite->grpv[p]=group;
  sprite->grpc++;
  return 0;
}

static int group_sprv_insert(struct sprite_group *group,int p,struct sprite *sprite) {
  if ((p<0)||(p>group->sprc)) return -1;
  if (group->sprc>=group->spra) {
    int na=group->spra+16;
    if (na>INT_MAX/sizeof(void*)) return -1;
    void *nv=realloc(group->sprv,sizeof(void*)*na);
    if (!nv) return -1;
    group->sprv=nv;
    group->spra=na;
  }
  if (sprite_ref(sprite)<0) return -1;
  memmove(group->sprv+p+1,group->sprv+p,sizeof(void*)*(group->sprc-p));
  group->sprv[p]=sprite;
  group->sprc++;
  return 0;
}

static void sprite_grpv_remove(struct sprite *sprite,int p) {
  if ((p<0)||(p>=sprite->grpc)) return;
  struct sprite_group *group=sprite->grpv[p];
  sprite->grpc--;
  memmove(sprite->grpv+p,sprite->grpv+p+1,sizeof(void*)*(sprite->grpc-p));
  sprite_group_del(group);
}

static void group_sprv_remove(struct sprite_group *group,int p) {
  if ((p<0)||(p>=group->sprc)) return;
  struct sprite *sprite=group->sprv[p];
  group->sprc--;
  memmove(group->sprv+p,group->sprv+p+1,sizeof(void*)*(group->sprc-p));
  sprite_del(sprite);
}

/* Test membership.
 */

int sprite_group_has(const struct sprite_group *group,const struct sprite *sprite) {
  if (!group||!sprite) return 0;
  return (sprite_grpv_search(sprite,group)>=0)?1:0;
}

/* Add sprite to group, public.
 */

int sprite_group_add(struct sprite_group *group,struct sprite *sprite) {
  if (!group||!sprite) return -1;
  
  // SINGLE mode is kind of a different deal.
  if (group->mode==SPRITE_GROUP_MODE_SINGLE) {
    if ((group->sprc>=1)&&(group->sprv[0]==sprite)) return 0;
    sprite_group_clear(group);
    int grpp=sprite_grpv_search(sprite,group);
    if (grpp>=0) return -1; // Lists inconsistent!
    grpp=-grpp-1;
    if (group_sprv_insert(group,0,sprite)<0) return -1;
    if (sprite_grpv_insert(sprite,grpp,group)<0) {
      group_sprv_remove(group,0);
      return -1;
    }
    return 1;
  }
  
  int grpp=sprite_grpv_search(sprite,group);
  if (grpp>=0) { // Already added. For EXPLICIT mode, shuffle sprite to the back of group.
    if (group->mode==SPRITE_GROUP_MODE_EXPLICIT) {
      int sprp=group_sprv_search(group,sprite);
      if (sprp<0) return -1; // Lists inconsistent!
      if (sprp>=group->sprc-1) return 0; // Already at the end.
      memmove(group->sprv+sprp,group->sprv+sprp+1,sizeof(void*)*(group->sprc-sprp-1));
      group->sprv[group->sprc-1]=sprite;
      return 1;
    }
    return 0;
  }
  int sprp=group_sprv_search(group,sprite);
  if (sprp>=0) return -1; // Lists inconsistent!
  grpp=-grpp-1;
  sprp=-sprp-1;
  
  if (sprite_grpv_insert(sprite,grpp,group)<0) return -1;
  if (group_sprv_insert(group,sprp,sprite)<0) {
    sprite_grpv_remove(sprite,grpp);
    return -1;
  }
  
  // Update magic markers.
  if (group==GRP(solid)) sprite->solid=1;
  else if (group==GRP(floating)) sprite->floating=1;
  
  return 1;
}

/* Remove sprite from group, public.
 */
 
int sprite_group_remove(struct sprite_group *group,struct sprite *sprite) {
  if (!group||!sprite) return -1;
  int grpp=sprite_grpv_search(sprite,group);
  if (grpp<0) return 0; // Not added.
  int sprp=group_sprv_search(group,sprite);
  if (sprp<0) return -1; // Lists inconsistent!
  
  // The removal might delete one or both of these objects.
  // So retain the group first, and do the sprite's list before the group's list.
  if (sprite_group_ref(group)<0) return -1;
  sprite_grpv_remove(sprite,grpp);
  group_sprv_remove(group,sprp);
  sprite_group_del(group);
  
  // Update magic markers.
  if (group==GRP(solid)) sprite->solid=0;
  else if (group==GRP(floating)) sprite->floating=0;
  
  return 1;
}

/* Clear group.
 */

void sprite_group_clear(struct sprite_group *group) {
  if (!group) return;
  if (!group->sprc) return;
  if (sprite_group_ref(group)<0) return;
  while (group->sprc>0) {
    group->sprc--;
    struct sprite *sprite=group->sprv[group->sprc];
    int grpp=sprite_grpv_search(sprite,group);
    if (grpp>=0) sprite_grpv_remove(sprite,grpp);
    sprite_del(sprite);
  }
  sprite_group_del(group);
}

/* Clear sprite's groups.
 */
 
void sprite_kill(struct sprite *sprite) {
  if (sprite_ref(sprite)<0) return;
  while (sprite->grpc>0) {
    sprite->grpc--;
    struct sprite_group *group=sprite->grpv[sprite->grpc];
    int sprp=group_sprv_search(group,sprite);
    if (sprp>=0) group_sprv_remove(group,sprp);
    sprite_group_del(group);
  }
  sprite_del(sprite);
}

/* Kill all sprites in group.
 */
 
void sprite_group_kill_all(struct sprite_group *group) {
  if (!group) return;
  if (!group->sprc) return;
  if (sprite_group_ref(group)<0) return;
  while (group->sprc>0) {
    group->sprc--;
    struct sprite *sprite=group->sprv[group->sprc];
    sprite_kill(sprite); // Will try to remove from this group and miss.
    sprite_del(sprite);
  }
  sprite_group_del(group);
}

/* Partial render sort.
 * Non-render modes are noop.
 */
 
static int sprite_rendercmp(const struct sprite *a,const struct sprite *b) {
  if (a->layer<b->layer) return -1;
  if (a->layer>b->layer) return 1;
  if (a->y<b->y) return -1;
  if (a->y>b->y) return 1;
  return 0;
}
 
void sprite_group_sort_partial(struct sprite_group *group) {
  if (group->sprc<2) return;
  if (group->mode==SPRITE_GROUP_MODE_RENDER) {
    int i=1,done=1;
    for (;i<group->sprc;i++) {
      struct sprite *a=group->sprv[i-1];
      struct sprite *b=group->sprv[i];
      if (sprite_rendercmp(a,b)>0) {
        group->sprv[i-1]=b;
        group->sprv[i]=a;
        done=0;
      }
    }
    if (done) return;
    for (i=group->sprc;i-->1;) {
      struct sprite *a=group->sprv[i-1];
      struct sprite *b=group->sprv[i];
      if (sprite_rendercmp(a,b)>0) {
        group->sprv[i-1]=b;
        group->sprv[i]=a;
      }
    }
  }
}

#include "game/bellacopia.h"
#include "game/race/race.h"

#define COOLDOWN 0.500

struct sprite_npc {
  struct sprite hdr;
  int activity;
  int activity_arg;
  int noxform;
  double cooldown;
  int store_listener;
  uint8_t tileid0;
};

#define SPRITE ((struct sprite_npc*)sprite)

/* Delete.
 */
 
static void _npc_del(struct sprite *sprite) {
  store_unlisten(SPRITE->store_listener);
}

/* Prepare Moon Song.
 * Set her tile if won, and if not, install a listener in case it changes.
 * Our nature is that we will always be present when that field gets set.
 */
 
static void npc_moonsong_cb(char type,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if ((type=='f')&&value&&(id==race_fld_by_id(SPRITE->activity_arg))) {
    sprite->tileid=SPRITE->tileid0+1;
  }
}
 
static void npc_prepare_moonsong(struct sprite *sprite) {
  int fld=race_fld_by_id(SPRITE->activity_arg);
  if (!fld) return;
  if (store_get_fld(fld)) {
    sprite->tileid=SPRITE->tileid0+1;
  } else {
    SPRITE->store_listener=store_listen('f',npc_moonsong_cb,sprite);
  }
}

/* Mr and Mrs Rabbit.
 * Similar to Moon Song, we're very likely to be present when our flag changes.
 * Can't imagine why a user would do this, but they can undo and turn the flag off, be ready for it.
 */
 
static void npc_mr_mrs_rabbit_reunited(struct sprite *sprite) {
  if (sprite->tileid==SPRITE->tileid0) {
    sprite->tileid=SPRITE->tileid0+0x10;
    if (SPRITE->activity_arg==2) { // mrs
      sprite->x-=0.5;
      struct sprite *heart=sprite_spawn(sprite->x-0.5,sprite->y-1.0,RID_sprite_heart_ornament,0,0,0,0,0);
    } else { // mr
      sprite->x+=0.5;
    }
  }
}

static void npc_mr_mrs_rabbit_tragically_torn_asunder(struct sprite *sprite) {
  if (sprite->tileid==SPRITE->tileid0+0x10) {
    sprite->tileid=SPRITE->tileid0;
    if (SPRITE->activity_arg==2) {
      sprite->x+=0.5;
      // Remove the heart ornament if we find one.
      struct sprite **otherp=GRP(visible)->sprv;
      int otheri=GRP(visible)->sprc;
      for (;otheri-->0;otherp++) {
        struct sprite *other=*otherp;
        if (other->rid==RID_sprite_heart_ornament) {
          sprite_kill_soon(other);
        }
      }
    } else {
      sprite->x-=0.5;
    }
  }
}
 
static void npc_mr_mrs_rabbit_cb(char type,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if ((type=='f')&&(id==NS_fld_surveyor_complete)) {
    if (value) npc_mr_mrs_rabbit_reunited(sprite);
    else npc_mr_mrs_rabbit_tragically_torn_asunder(sprite);
  }
}

static void npc_prepare_mr_mrs_rabbit(struct sprite *sprite) {
  if (store_get_fld(NS_fld_surveyor_complete)) {
    npc_mr_mrs_rabbit_reunited(sprite);
  } else {
    SPRITE->store_listener=store_listen('f',npc_mr_mrs_rabbit_cb,sprite);
  }
}

/* Init.
 */
 
static int _npc_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->activity=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->activity_arg=(sprite->arg[2]<<8)|sprite->arg[3];
  if (game_activity_sprite_should_abort(SPRITE->activity,SPRITE->activity_arg,sprite->type)) return -1;
  
  /* Certain activities imply tileid+1 when some flag is set, or a similar change.
   */
  switch (SPRITE->activity) {
    case NS_activity_logproblem1: if (store_get_fld(NS_fld_mayor)) sprite->tileid+=1; break;
    case NS_activity_logproblem2: if (store_get_fld(NS_fld_mayor)) sprite->tileid+=1; break;
    case NS_activity_moonsong: npc_prepare_moonsong(sprite); break;
    case NS_activity_mr_mrs_rabbit: npc_prepare_mr_mrs_rabbit(sprite); break;
  }
  
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,sprite->cmd,sprite->cmdc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_sprite_noxform: SPRITE->noxform=1; break;
      }
    }
  }
  
  return 0;
}

/* Update.
 */
 
static void _npc_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->cooldown>0.0) {
    SPRITE->cooldown-=elapsed;
  }
  if (!SPRITE->noxform) {
    if (GRP(hero)->sprc>0) {
      struct sprite *hero=GRP(hero)->sprv[0];
      double dx=hero->x-sprite->x;
      if (dx<-0.5) sprite->xform=EGG_XFORM_XREV;
      else if (dx>0.5) sprite->xform=0;
    }
  }
}

/* Collide.
 */
 
static void _npc_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->cooldown>0.0) return;
  if (other->type==&sprite_type_hero) {
    game_begin_activity(SPRITE->activity,SPRITE->activity_arg,sprite);
    SPRITE->cooldown=COOLDOWN;
    sprite_hero_unanimate(other);
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_npc={
  .name="npc",
  .objlen=sizeof(struct sprite_npc),
  .del=_npc_del,
  .init=_npc_init,
  .update=_npc_update,
  .collide=_npc_collide,
};

/* Public accessors.
 */
 
int sprite_npc_get_activity(const struct sprite *sprite) {
  if (!sprite||(sprite->type!=&sprite_type_npc)) return 0;
  return SPRITE->activity;
}

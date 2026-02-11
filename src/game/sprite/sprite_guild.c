#include "game/bellacopia.h"

struct sprite_guild {
  struct sprite hdr;
  uint8_t tileid0;
  int battle;
  int name_strix; // RID_strings_battle
  int fld;
  double cooldown;
};

#define SPRITE ((struct sprite_guild*)sprite)

/* Init.
 */
 
static int _guild_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,sprite->cmd,sprite->cmdc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_sprite_guild: {
            SPRITE->battle=(cmd.arg[0]<<8)|cmd.arg[1];
            SPRITE->name_strix=(cmd.arg[2]<<8)|cmd.arg[3];
            SPRITE->fld=(cmd.arg[4]<<8)|cmd.arg[5];
          } break;
      }
    }
  }
  if (store_get_fld(SPRITE->fld)) sprite->tileid=SPRITE->tileid0+1;
  
  return 0;
}

/* Update.
 */
 
static void _guild_update(struct sprite *sprite,double elapsed) {
  // Face the hero. Our natural orientation is right.
  if (GRP(hero)->sprc>0) {
    struct sprite *hero=GRP(hero)->sprv[0];
    const double margin=0.25;
    double dx=hero->x-sprite->x;
    if (dx>margin) sprite->xform=0;
    else if (dx<-margin) sprite->xform=EGG_XFORM_XREV;
  }
  if (SPRITE->cooldown>0.0) {
    SPRITE->cooldown-=elapsed;
  }
}

/* Are all guild members satisfied?
 * We don't bother confirming they belong to the same guild, since all sprites at one time will.
 */
 
static int all_guild_satisfied() {
  struct sprite **otherp=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->type!=&sprite_type_guild) continue;
    struct sprite_guild *OTHER=(struct sprite_guild*)other;
    if (other->tileid==OTHER->tileid0) return 0;
  }
  return 1;
}

/* Battle callback.
 */
 
static void guild_cb_battle(struct modal *modal,int outcome,void *userdata) {
  struct sprite *sprite=userdata;
  if (outcome>0) {
    sprite->tileid=SPRITE->tileid0+1;
    if (!store_get_fld(SPRITE->fld)&&all_guild_satisfied()) {
      store_set_fld(SPRITE->fld,1);
      modal_battle_add_consequence(modal,NS_itemid_text,113);
    }
  } else if (outcome<0) {
    sprite->tileid=SPRITE->tileid0;
    game_hurt_hero();
    modal_battle_add_consequence(modal,NS_itemid_heart,-1);
  }
}

/* Collide.
 */
 
static void _guild_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->cooldown>0.0) return;
  struct modal_args_battle args={
    .battle=SPRITE->battle,
    .args={
      .difficulty=0x80,
      .bias=0x80,
      .rctl=0,
      .rface=NS_face_monster,
    },
    .userdata=sprite,
    .right_name=SPRITE->name_strix,
  };

  if (other->type==&sprite_type_hero) {
    args.args.lctl=1;
    args.args.lface=NS_face_dot;
    //TODO difficulty and bias
    args.cb=guild_cb_battle;
  } else {
    return;
  }

  struct modal *modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
  if (!modal) return;
  SPRITE->cooldown=0.500;
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_guild={
  .name="guild",
  .objlen=sizeof(struct sprite_guild),
  .init=_guild_init,
  .update=_guild_update,
  .collide=_guild_collide,
};

#include "game/bellacopia.h"

struct sprite_guild {
  struct sprite hdr;
  uint8_t tileid0;
  int battle;
  int name_strix; // RID_strings_battle
  int fld;
  double cooldown;
  uint8_t pvhero;
  int await_hero_orientation_change;
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
  int election_season=(store_get_fld(NS_fld_election_start)&&!store_get_fld(NS_fld_mayor));
  if (election_season&&store_get_fld(SPRITE->fld)) sprite->tileid=SPRITE->tileid0+1;
  
  return 0;
}

/* Update.
 */
 
static void _guild_update(struct sprite *sprite,double elapsed) {
  uint8_t horient=0;
  // Face the hero. Our natural orientation is right.
  if (GRP(hero)->sprc>0) {
    struct sprite *hero=GRP(hero)->sprv[0];
    const double margin=0.25;
    double dx=hero->x-sprite->x;
    if (dx>margin) sprite->xform=0;
    else if (dx<-margin) sprite->xform=EGG_XFORM_XREV;
    horient=sprite_hero_get_facedir(hero);
  }
  if (horient!=SPRITE->pvhero) {
    SPRITE->pvhero=horient;
    SPRITE->await_hero_orientation_change=0;
  }
  if (SPRITE->cooldown>0.0) {
    SPRITE->cooldown-=elapsed;
  }
}

/* There's a special NPC "guildguard" who appears in front of the door after you win the first guild battle.
 * She quietly lets herself out after you win the endorsement.
 * This is just a little "are you sure?" to let them know the guild will reset if they leave partway thru.
 */

// Args will be reused. It's cool, there's only one guild active at a time.
static uint8_t guild_guard_args[4]={
  NS_activity_guildguard>>8,NS_activity_guildguard&0xff,
  0,0,
};
 
static void guild_drop_guard() {
  struct sprite *sprite=find_sprite_by_arg(guild_guard_args);
  if (sprite) {
    sprite_kill_soon(sprite);
  }
}

static void guild_require_guard() {

  // Already got? We will be called redundantly.
  struct sprite *sprite=find_sprite_by_arg(guild_guard_args);
  if (sprite) return;
  
  // Find a home. There should be one door on this map leading to the parent map. We want to be one meter north of that.
  double x=-1.0,y=-1.0;
  struct map *map=g.camera.map;
  if (!map) return;
  if (!map->parent) return; // Parent required.
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_door) {
      int dstmapid=(cmd.arg[2]<<8)|cmd.arg[3];
      if (dstmapid!=map->parent) continue;
      x=cmd.arg[0]+0.5;
      y=cmd.arg[1]-0.5;
      break;
    }
  }
  if (x<0.0) return; // Failed to locate door.
  x+=map->lng*NS_sys_mapw;
  y+=map->lat*NS_sys_maph;
  
  // Spawn.
  sprite=sprite_spawn(x,y,RID_sprite_guildguard,guild_guard_args,sizeof(guild_guard_args),0,0,0);
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
    int election_season=(store_get_fld(NS_fld_election_start)&&!store_get_fld(NS_fld_mayor));
    if (election_season) {
      sprite->tileid=SPRITE->tileid0+1;
    }
    if (!election_season) {
      //TODO Some other prize when it's not election season.
    } else if (store_get_fld(SPRITE->fld)) {
      // Replaying after winning the endorsement. Sure, Dot, you do you.
    } else if (all_guild_satisfied()) {
      // Last of the guild. Wrap it up.
      store_set_fld(SPRITE->fld,1);
      modal_battle_add_consequence(modal,NS_itemid_text,113);
      guild_drop_guard();
    } else {
      // Guild progress.
      guild_require_guard();
    }
    
  } else if (outcome<0) {
    sprite->tileid=SPRITE->tileid0;
    modal_battle_add_consequence(modal,NS_itemid_heart,-1);
  }
}

static void guild_cb_final(struct modal *modal,int outcome,void *userdata) {
  if (outcome<0) {
    game_hurt_hero();
  }
}

/* Collide.
 */
 
static void _guild_collide(struct sprite *sprite,struct sprite *other) {

  /* It's easy to reenter a guild contest by accident, and that can be a real bummer if you just spent some effort to win it.
   * So we do an extra-strength cooldown: Dot's orientation has to change before we can trigger again.
   */
  if (SPRITE->cooldown>0.0) return;
  if (SPRITE->await_hero_orientation_change) return;
  
  struct modal_args_battle args={
    .battle=SPRITE->battle,
    .args={
      .difficulty=0x80,
      .bias=bm_battle_bias(SPRITE->battle),
      .rctl=0,
      .rface=NS_face_monster,
    },
    .userdata=sprite,
    .right_name=SPRITE->name_strix,
  };

  if (other->type==&sprite_type_hero) {
    args.args.lctl=1;
    args.args.lface=NS_face_dot;
    args.args.bias=bm_battle_bias(SPRITE->battle);
    args.cb=guild_cb_battle;
    args.cb_final=guild_cb_final;
  } else {
    return;
  }

  struct modal *modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
  if (!modal) return;
  SPRITE->cooldown=0.500;
  SPRITE->await_hero_orientation_change=1;
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

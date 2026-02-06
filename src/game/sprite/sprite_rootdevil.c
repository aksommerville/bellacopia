#include "game/bellacopia.h"

struct sprite_rootdevil {
  struct sprite hdr;
  uint8_t tileid0;
  int fld;
  double cooldown;
  double animclock;
  int animframe;
};

#define SPRITE ((struct sprite_rootdevil*)sprite)

static int _rootdevil_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->fld=(sprite->arg[0]<<8)|sprite->arg[1];
  if (store_get_fld(SPRITE->fld)) return -1;
  return 0;
}

static void _rootdevil_update(struct sprite *sprite,double elapsed) {
  if (SPRITE->cooldown>0.0) {
    SPRITE->cooldown-=elapsed;
  }
  if ((SPRITE->animclock-=elapsed)<=0.0) {
    SPRITE->animclock+=0.250;
    if (++(SPRITE->animframe)>=2) SPRITE->animframe=0;
    sprite->tileid=SPRITE->tileid0+SPRITE->animframe;
  }
}

static void rootdevil_cb_battle(struct modal *modal,int outcome,void *userdata) {
  struct sprite *sprite=userdata;
  if (outcome>0) {
    store_set_fld(SPRITE->fld,1);
    sprite_kill_soon(sprite);
  } else if (outcome<0) {
    game_hurt_hero();
    modal_battle_add_consequence(modal,NS_itemid_heart,-1);
  }
}

// We don't care what order you fight the root devils, their difficulty increases with each one you defeat.
static uint8_t rootdevil_choose_bias() {
  int winc=0;
  if (store_get_fld(NS_fld_root1)) winc++;
  if (store_get_fld(NS_fld_root2)) winc++;
  if (store_get_fld(NS_fld_root3)) winc++;
  if (store_get_fld(NS_fld_root4)) winc++;
  if (store_get_fld(NS_fld_root5)) winc++;
  if (store_get_fld(NS_fld_root6)) winc++;
  if (store_get_fld(NS_fld_root7)) winc++;
  switch (winc) {
    case 0: return 0x10;
    case 1: return 0x30;
    case 2: return 0x50;
    case 3: return 0x70;
    case 4: return 0x90;
    case 5: return 0xb0;
    case 6: return 0xd0;
    case 7: return 0xf0; // Where did you find a root devil, if you've already killed seven?
  }
  return 0x80;
}

static void _rootdevil_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->cooldown>0.0) return;
  if (other->type==&sprite_type_hero) {
    struct modal_args_battle args={
      .battle=NS_battle_strangling,
      .args={
        .difficulty=0x80,
        .bias=rootdevil_choose_bias(),
        .lctl=1,
        .lface=NS_face_dot,
        .rctl=0,
        .rface=NS_face_monster,
      },
      .cb=rootdevil_cb_battle,
      .userdata=sprite,
      .right_name=18, // "Root Devil"
    };
    struct modal *modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
    if (!modal) return;
    SPRITE->cooldown=0.500;
  }
}

const struct sprite_type sprite_type_rootdevil={
  .name="rootdevil",
  .objlen=sizeof(struct sprite_rootdevil),
  .init=_rootdevil_init,
  .update=_rootdevil_update,
  .collide=_rootdevil_collide,
};

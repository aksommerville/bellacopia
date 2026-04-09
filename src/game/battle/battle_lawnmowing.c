/* battle_lawnmowing.c
 * Moving constantly, turn to cut the grass and don't touch cells you've already visited.
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

#define SPRITEID_LEFT 1
#define SPRITEID_RIGHT 2

struct battle_lawnmowing {
  struct battle hdr;
  struct batsup_world *world;
  double animclock;
  int animframe;
};

#define BATTLE ((struct battle_lawnmowing*)battle)

/* Player sprite.
 */
 
struct sprite_player {
  struct batsup_sprite hdr;
  int human; // 0,1,2
  double skill;
};

static void player_update(struct batsup_sprite *sprite,double elapsed) {
  struct battle *battle=sprite->world->battle;
  //TODO
}

static struct batsup_sprite *player_init(struct battle *battle,int id,int ctl,int face,double skill) {
  struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,id,sizeof(struct sprite_player));
  if (!sprite) return 0;
  struct sprite_player *SPRITE=(struct sprite_player*)sprite;
  
  sprite->y=(NS_sys_maph>>1)+0.5;
  if (id==SPRITEID_LEFT) {
    sprite->x=(NS_sys_mapw>>2)+0.5;
  } else {
    sprite->x=((NS_sys_mapw*3)>>2)+0.5;
  }
  SPRITE->human=ctl;
  SPRITE->skill=skill;
  
  switch (face) {
    case NS_face_dot: {
        //TODO
      } break;
    case NS_face_princess: {
      } break;
    case NS_face_monster: default: {
      } break;
  }
  
  return sprite;
}

/* Delete.
 */
 
static void _lawnmowing_del(struct battle *battle) {
  batsup_world_del(BATTLE->world);
}

/* Generate the initial map.
 */
 
static void lawnmowing_generate_map(struct battle *battle) {
  uint8_t *dst=BATTLE->world->map->v;
  int i=NS_sys_mapw*NS_sys_maph;
  for (;i-->0;dst++) {
    int choice=rand()%15;
    if (choice<8) *dst=choice; // static grass
    else *dst=0x08; // for mowing
  }
  //TODO
}

/* New.
 */
 
static int _lawnmowing_init(struct battle *battle) {
  if (!(BATTLE->world=batsup_world_new(battle,0))) return -1;
  if (batsup_world_set_image(BATTLE->world,RID_image_battle_labor)<0) return -1;
  double lskill,rskill;
  battle_normalize_bias(&lskill,&rskill,battle);
  player_init(battle,SPRITEID_LEFT,battle->args.lctl,battle->args.lface,lskill);
  player_init(battle,SPRITEID_RIGHT,battle->args.rctl,battle->args.rface,rskill);
  lawnmowing_generate_map(battle);
  return 0;
}

/* Replace each uncut grass tile with the current animation frame.
 */
 
static void lawnmowing_animate_grass(struct battle *battle) {
  uint8_t tileid=0x08+BATTLE->animframe;
  uint8_t *v=BATTLE->world->map->v;
  int i=NS_sys_mapw*NS_sys_maph;
  for (;i-->0;v++) {
    if ((*v>=0x08)&&(*v<0x10)) *v=tileid;
  }
}

/* Update.
 */
 
static void _lawnmowing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  batsup_world_update(BATTLE->world,elapsed);
  if ((BATTLE->animclock-=elapsed)<=0.0) {
    BATTLE->animclock+=0.200;
    if (++(BATTLE->animframe)>=8) BATTLE->animframe=0;
    lawnmowing_animate_grass(battle);
  }

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render.
 */
 
static void _lawnmowing_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
}

/* Type definition.
 */
 
const struct battle_type battle_type_lawnmowing={
  .name="lawnmowing",
  .objlen=sizeof(struct battle_lawnmowing),
  .strix_name=172,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_lawnmowing_del,
  .init=_lawnmowing_init,
  .update=_lawnmowing_update,
  .render=_lawnmowing_render,
};

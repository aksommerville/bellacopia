/* battle_mindcontrol.c
 * Alternate A/B in the right rhythm to maintain psychic control. While control established, dpad moves your victim.
 * Push a piece of candy thru the hazards.
 */

#include "game/bellacopia.h"
#include "game/batsup/batsup_world.h"

/* Our sprite ids are also arguments to "battlemark" poi in the map.
 */
#define SPRITEID_LMAN 1
#define SPRITEID_RMAN 2
#define SPRITEID_LCAT 3
#define SPRITEID_RCAT 4
#define BATTLEMARK_TREE 5 /* POI must describe a rectangle. */

struct battle_mindcontrol {
  struct battle hdr;
  struct batsup_world *world;
  double treex,treey,treew,treeh;
};

#define BATTLE ((struct battle_mindcontrol*)battle)

struct sprite_man {
  struct batsup_sprite hdr;
  int human; // player id or zero for cpu control
};

struct sprite_cat {
  struct batsup_sprite hdr;
};

/* Delete.
 */
 
static void _mindcontrol_del(struct battle *battle) {
}

/* New.
 */
 
static int _mindcontrol_init(struct battle *battle) {
  //TODO difficulty
  
  if (!(BATTLE->world=batsup_world_new(RID_map_mindcontrol))) return -1;
  
  struct cmdlist_reader reader={.v=BATTLE->world->map->cmd,.c=BATTLE->world->map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_battlemark: {
          int arg=(cmd.arg[2]<<8)|cmd.arg[3];
          switch (arg) {
          
            case SPRITEID_LMAN: {
                struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_LMAN,sizeof(struct sprite_man));
                if (!sprite) return -1;
                struct sprite_man *SPRITE=(struct sprite_man*)sprite;
                sprite->x=cmd.arg[0]+0.5;
                sprite->y=cmd.arg[1]+0.5;
                sprite->tileid=0x80; //TODO 0x80=Dot, 0x90=Princess, 0xa0=Nyarlathotep
              } break;

            case SPRITEID_RMAN: {
                struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_RMAN,sizeof(struct sprite_man));
                if (!sprite) return -1;
                struct sprite_man *SPRITE=(struct sprite_man*)sprite;
                sprite->x=cmd.arg[0]+0.5;
                sprite->y=cmd.arg[1]+0.5;
                sprite->tileid=0xa0; //TODO 0x80=Dot, 0x90=Princess, 0xa0=Nyarlathotep
                sprite->xform=EGG_XFORM_XREV;
              } break;

            case SPRITEID_LCAT: {
                struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_LCAT,sizeof(struct sprite_cat));
                if (!sprite) return -1;
                struct sprite_cat *SPRITE=(struct sprite_cat*)sprite;
                sprite->x=cmd.arg[0]+0.5;
                sprite->y=cmd.arg[1]+0.5;
                sprite->tileid=0xb0;
              } break;

            case SPRITEID_RCAT: {
                struct batsup_sprite *sprite=batsup_sprite_spawn(BATTLE->world,SPRITEID_RCAT,sizeof(struct sprite_cat));
                if (!sprite) return -1;
                struct sprite_cat *SPRITE=(struct sprite_cat*)sprite;
                sprite->x=cmd.arg[0]+0.5;
                sprite->y=cmd.arg[1]+0.5;
                sprite->tileid=0xb0;
                sprite->xform=EGG_XFORM_XREV;
              } break;

            case BATTLEMARK_TREE: {
                if (BATTLE->treew<0.5) {
                  BATTLE->treex=cmd.arg[0];
                  BATTLE->treey=cmd.arg[1];
                  BATTLE->treew=1.0;
                  BATTLE->treeh=1.0;
                } else {
                  double nv=cmd.arg[0];
                  if (nv<BATTLE->treex) {
                    BATTLE->treew+=BATTLE->treex-nv;
                    BATTLE->treex=nv;
                  }
                  nv+=1.0;
                  if (nv>BATTLE->treex+BATTLE->treew) {
                    BATTLE->treew=nv-BATTLE->treex;
                  }
                  nv=cmd.arg[1];
                  if (nv<BATTLE->treey) {
                    BATTLE->treeh+=BATTLE->treey-nv;
                    BATTLE->treey=nv;
                  }
                  nv+=1.0;
                  if (nv>BATTLE->treey+BATTLE->treeh) {
                    BATTLE->treeh=nv-BATTLE->treey;
                  }
                }
              } break;
          }
        } break;
    }
  }

  return 0;
}

/* Update.
 */
 
static void _mindcontrol_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  batsup_world_update(BATTLE->world,elapsed);

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render.
 */
 
static void _mindcontrol_render(struct battle *battle) {
  batsup_world_render(BATTLE->world);
}

/* Type definition.
 */
 
const struct battle_type battle_type_mindcontrol={
  .name="mindcontrol",
  .objlen=sizeof(struct battle_mindcontrol),
  .strix_name=177,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_mindcontrol_del,
  .init=_mindcontrol_init,
  .update=_mindcontrol_update,
  .render=_mindcontrol_render,
};

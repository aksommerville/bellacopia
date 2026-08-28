/* sprite_ornament.c
 * Simple single-tile animation.
 */
 
#include "game/bellacopia.h"

struct sprite_ornament {
  struct sprite hdr;
  uint8_t tileid0;
  int framec;
  double interval;
  double clock;
  int frame;
};

#define SPRITE ((struct sprite_ornament*)sprite)

static int _ornament_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->framec=1;
  SPRITE->interval=0.250;
  
  struct cmdlist_reader reader;
  if (sprite_reader_init(&reader,sprite->cmd,sprite->cmdc)>=0) {
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_sprite_ornament: {
            SPRITE->framec=cmd.arg[0];
            if (SPRITE->framec<1) SPRITE->framec=1;
            SPRITE->interval=(cmd.arg[1]*4)/1000.0;
            if (SPRITE->interval<0.020) SPRITE->interval=0.020;
          } break;
      }
    }
  }
  
  SPRITE->clock=SPRITE->interval;
  SPRITE->frame=0;
  return 0;
}

static void _ornament_update(struct sprite *sprite,double elapsed) {
  if ((SPRITE->clock-=elapsed)<=0.0) {
    SPRITE->clock+=SPRITE->interval;
    if (++(SPRITE->frame)>=SPRITE->framec) {
      SPRITE->frame=0;
    }
    sprite->tileid=SPRITE->tileid0+SPRITE->frame;
  }
}

const struct sprite_type sprite_type_ornament={
  .name="ornament",
  .objlen=sizeof(struct sprite_ornament),
  .init=_ornament_init,
  .update=_ornament_update,
};

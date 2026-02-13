/* sprite_statuemaze.c
 * Passage that locks if you walk the statue maze incorrectly.
 * We also take responsibility for monitoring progress through the maze.
 * Our partner, sprite_statue, is a dummy.
 *
 * Rules:
 *  - 0x18 Mermaid: Can't step on a horizontal neighbor.
 *  - 0x19 Bear: Can't step on any neighbor.
 *  - 0x1a Minotaur: Can't step below.
 *  - 0x1b Wyvern: Can't step above.
 *  - 0x1c Penguin: Noop.
 */
 
#include "game/bellacopia.h"

// Cells in (SPRITE->grid).
#define GRID_NOOP    0
#define GRID_SOLID   1
#define GRID_BREAK   2
#define GRID_TRIGGER 3
#define GRID_RESET   4
#define GRID_STATUE  5

struct sprite_statuemaze {
  struct sprite hdr;
  uint8_t tileid0;
  double gridx,gridy;
  uint8_t grid[NS_sys_mapw*NS_sys_maph];
  int herox,heroy; // Recent position, in my grid space.
  int failed;
};

#define SPRITE ((struct sprite_statuemaze*)sprite)

static void statuemaze_add_statue(struct sprite *sprite,int x,int y,int tileid) {
  // Statues are not permitted on the edges -- a statue always has four cardinal neighbors on this map.
  if ((x<1)||(x>=NS_sys_mapw-1)||(y<1)||(y>=NS_sys_maph-1)) return;
  uint8_t *p=SPRITE->grid+y*NS_sys_mapw+x;
  *p=GRID_STATUE;
  switch (tileid) {
    case 0x18: { // Mermaid.
        p[-1]=GRID_BREAK;
        p[1]=GRID_BREAK;
      } break;
    case 0x19: { // Bear.
        p[-NS_sys_mapw]=GRID_BREAK;
        p[-1]=GRID_BREAK;
        p[1]=GRID_BREAK;
        p[NS_sys_mapw]=GRID_BREAK;
      } break;
    case 0x1a: { // Minotaur.
        p[NS_sys_mapw]=GRID_BREAK;
      } break;
    case 0x1b: { // Wyvern.
        p[-NS_sys_mapw]=GRID_BREAK;
      } break;
    case 0x1c: { // Penguin.
        // all good, buddy
      } break;
  }
}

static int _statuemaze_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->herox=-1;
  SPRITE->heroy=-1;
  
  // Scan the map to identify solid cells and statues.
  struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  SPRITE->gridx=map->lng*NS_sys_mapw;
  SPRITE->gridy=map->lat*NS_sys_maph;
  const uint8_t *src=map->v;
  uint8_t *dst=SPRITE->grid;
  int i=NS_sys_mapw*NS_sys_maph;
  for (;i-->0;src++,dst++) {
    switch (map->physics[*src]) {
      case NS_physics_solid:
      case NS_physics_grabbable:
      case NS_physics_vanishable: *dst=GRID_SOLID; break;
    }
  }
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_sprite: {
          int x=cmd.arg[0];
          int y=cmd.arg[1];
          if ((x>=NS_sys_mapw)||(y>=NS_sys_maph)) break;
          int rid=(cmd.arg[2]<<8)|cmd.arg[3];
          const void *spriteserial=0;
          int spriteserialc=res_get(&spriteserial,EGG_TID_sprite,rid);
          uint8_t stileid=0,isstatue=0;
          struct cmdlist_reader sreader;
          if (sprite_reader_init(&sreader,spriteserial,spriteserialc)>=0) {
            struct cmdlist_entry scmd;
            while (cmdlist_reader_next(&scmd,&sreader)>0) {
              if (scmd.opcode==CMD_sprite_type) {
                int sprtype=(scmd.arg[0]<<8)|scmd.arg[1];
                if (sprtype==NS_sprtype_statue) isstatue=1;
              } else if (scmd.opcode==CMD_sprite_tile) {
                stileid=scmd.arg[0];
              }
            }
          }
          if (!isstatue) break;
          statuemaze_add_statue(sprite,x,y,stileid);
        } break;
    }
  }
  
  /* Statues must all be on the same horizontal side of me.
   */
  int x=(int)sprite->x-map->lng*NS_sys_mapw;
  int y=(int)sprite->y-map->lat*NS_sys_maph;
  if ((x<0)||(y<0)||(x>=NS_sys_mapw)||(y>=NS_sys_maph)) return -1;
  int lc=0,rc=0,xlo=NS_sys_mapw,xhi=0;
  const uint8_t *v=SPRITE->grid;
  int sty=0;
  for (;sty<NS_sys_maph;sty++) {
    int stx=0;
    for (;stx<NS_sys_mapw;stx++,v++) {
      if (*v==GRID_STATUE) {
        if (stx<x) lc++;
        else if (stx>x) rc++;
        else {
          fprintf(stderr,"%s: Statue not permitted in same column as door.\n",__func__);
          return -1;
        }
        if (stx<xlo) xlo=stx;
        if (stx>xhi) xhi=stx;
      }
    }
  }
  if (lc&&rc) {
    fprintf(stderr,"%s: Statues must all be on the same side of the door, horizontally.\n",__func__);
    return -1;
  }
  if (!lc&&!rc) {
    fprintf(stderr,"%s: No statues.\n",__func__);
    return -1;
  }
  
  /* Make a full column of TRIGGER inward of me, and a similar column of RESET on the far side fo the statues.
   * There has to be a gap column between the statues and the RESETS, so make sure they have two columns free.
   */
  if (lc) {
    if (xlo<2) {
      fprintf(stderr,"%s: Statues need more left clearance.\n",__func__);
      return -1;
    }
  }
  if (rc) {
    if (xhi>=NS_sys_mapw-2) {
      fprintf(stderr,"%s: Statues need more right clearance.\n",__func__);
      return -1;
    }
  }
  int trigx,resetx;
  if (lc) {
    trigx=x-1;
    resetx=xlo-2;
  } else {
    trigx=x+1;
    resetx=xhi+2;
  }
  int qy=0;
  for (;qy<NS_sys_maph;qy++) {
    uint8_t *p;
    p=SPRITE->grid+qy*NS_sys_mapw+trigx;
    if (*p==GRID_NOOP) *p=GRID_TRIGGER;
    p=SPRITE->grid+qy*NS_sys_mapw+resetx;
    if (*p==GRID_NOOP) *p=GRID_RESET;
  }
  
  return 0;
}

static void statuemaze_trigger(struct sprite *sprite) {
  if (SPRITE->failed&&(sprite->tileid==SPRITE->tileid0)) {
    bm_sound(RID_sound_reject);
    sprite->tileid=SPRITE->tileid0+1;
    sprite_group_add(GRP(solid),sprite);
  }
}

static void statuemaze_reset(struct sprite *sprite) {
  SPRITE->failed=0;
  if (sprite->tileid==SPRITE->tileid0+1) {
    bm_sound(RID_sound_affirmative);
    sprite->tileid=SPRITE->tileid0;
    sprite_group_remove(GRP(solid),sprite);
  }
}

static void statuemaze_qmotion(struct sprite *sprite,uint8_t grid) {
  switch (grid) {
    case GRID_BREAK: SPRITE->failed=1; break;
    case GRID_RESET: statuemaze_reset(sprite); break;
    case GRID_TRIGGER: statuemaze_trigger(sprite); break;
  }
}

static void _statuemaze_update(struct sprite *sprite,double elapsed) {
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    if (hero->z==sprite->z) {
      int herox=(int)(hero->x-SPRITE->gridx);
      int heroy=(int)(hero->y-SPRITE->gridy);
      if ((herox!=SPRITE->herox)||(heroy!=SPRITE->heroy)) {
        SPRITE->herox=herox;
        SPRITE->heroy=heroy;
        if ((herox>=0)&&(herox<NS_sys_mapw)&&(heroy>=0)&&(heroy<NS_sys_maph)) {
          statuemaze_qmotion(sprite,SPRITE->grid[heroy*NS_sys_mapw+herox]);
        }
      }
    }
  }
}

const struct sprite_type sprite_type_statuemaze={
  .name="statuemaze",
  .objlen=sizeof(struct sprite_statuemaze),
  .init=_statuemaze_init,
  .update=_statuemaze_update,
};

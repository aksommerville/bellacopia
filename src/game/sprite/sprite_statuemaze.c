/* sprite_statuemaze.c
 * Passage that locks if you walk the statue maze incorrectly.
 * We also take responsibility for monitoring progress through the maze.
 * Our partner, sprite_statue, is a dummy.
 * The composition of the maze is managed separately by src/game/statuemaze.c
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
  int orx,ory; // Local map coords for top-left of the grid, where the first statue goes.
  int herox,heroy; // Recent position, in my grid space.
  int failed;
};

#define SPRITE ((struct sprite_statuemaze*)sprite)

static void statuemaze_drop_statues() {
  struct sprite **p=GRP(solid)->sprv;
  int i=GRP(solid)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    switch (sprite->rid) {
      case RID_sprite_statue_mermaid:
      case RID_sprite_statue_bear:
      case RID_sprite_statue_minotaur:
      case RID_sprite_statue_wyvern:
      case RID_sprite_statue_penguin:
        sprite_kill_soon(sprite);
    }
  }
}

static int _statuemaze_init(struct sprite *sprite) {
  SPRITE->tileid0=sprite->tileid;
  SPRITE->herox=-1;
  SPRITE->heroy=-1;
  
  /* Scan the map to identify solid cells and statues.
   * We'll fill (SPRITE->grid).
   * Don't need to know which statue is which, yet.
   */
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
      case CMD_map_statuemaze: {
          SPRITE->orx=cmd.arg[0];
          SPRITE->ory=cmd.arg[1];
        } break;
    }
  }
  if ((SPRITE->orx<2)||(SPRITE->ory<1)||(SPRITE->orx+STATUEMAZE_COLC*2+1>=NS_sys_mapw)||(SPRITE->ory+STATUEMAZE_ROWC*2>=NS_sys_maph)) {
    fprintf(stderr,"%s:%d: Statue maze origin (%d,%d) will not fit.\n",__FILE__,__LINE__,SPRITE->orx,SPRITE->ory);
    return -1;
  }
  
  /* Trigger and reset columns are just left and right of the active zone.
   */
  int trigx=SPRITE->orx-2;
  int resetx=SPRITE->orx+STATUEMAZE_COLC*2;
  if ((trigx<0)||(resetx>=NS_sys_mapw)) return -1;
  for (i=NS_sys_maph;i-->0;) {
    SPRITE->grid[i*NS_sys_mapw+trigx]=GRID_TRIGGER;
    SPRITE->grid[i*NS_sys_mapw+resetx]=GRID_RESET;
  }
  
  /* Get the maze's model.
   * If it hasn't been initialized yet, that will happen now.
   */
  uint8_t model[STATUEMAZE_COLC*STATUEMAZE_ROWC]={0};
  statuemaze_get_grid(model);
  
  /* If any statue sprites already exist, kill them.
   * We could reuse them, but meh, that would be complicated.
   * This can and will happen -- walk a little right of our room, we get dropped before the statues.
   */
  statuemaze_drop_statues();
  
  /* Read the model.
   * For each cell, we produce a sprite, and set 0..4 cells to BREAK.
   */
  int my=0,gy=SPRITE->ory;
  uint8_t *mp=model;
  uint8_t *grow=SPRITE->grid+gy*NS_sys_mapw;
  for (;my<STATUEMAZE_ROWC;my++,gy+=2,grow+=NS_sys_mapw*2) {
    int mx=0,gx=SPRITE->orx;
    uint8_t *gp=grow+gx;
    for (;mx<STATUEMAZE_COLC;mx++,mp++,gx+=2,gp+=2) {
      if ((*mp)&STATUEMAZE_N) gp[-NS_sys_mapw]=GRID_BREAK;
      if ((*mp)&STATUEMAZE_S) gp[NS_sys_mapw]=GRID_BREAK;
      if ((*mp)&STATUEMAZE_W) gp[-1]=GRID_BREAK;
      if ((*mp)&STATUEMAZE_E) gp[1]=GRID_BREAK;
      int spriteid=0;
      switch (STATUEMAZE_APPEARANCE(*mp)) {
        case STATUEMAZE_APPEARANCE_MERMAID: spriteid=RID_sprite_statue_mermaid; break;
        case STATUEMAZE_APPEARANCE_BEAR: spriteid=RID_sprite_statue_bear; break;
        case STATUEMAZE_APPEARANCE_MINOTAUR: spriteid=RID_sprite_statue_minotaur; break;
        case STATUEMAZE_APPEARANCE_WYVERN: spriteid=RID_sprite_statue_wyvern; break;
        case STATUEMAZE_APPEARANCE_PENGUIN: spriteid=RID_sprite_statue_penguin; break;
        default: {
            fprintf(stderr,
              "%s:%d:WARNING: Unexpected appearance %d for statuemaze cell %d,%d\n",
              __FILE__,__LINE__,STATUEMAZE_APPEARANCE(*mp),mx,my
            );
          }
      }
      if (spriteid) {
        struct sprite *statue=sprite_spawn(map->lng*NS_sys_mapw+gx+0.5,map->lat*NS_sys_maph+gy+0.5,spriteid,0,0,0,0,0);
      }
    }
  }
  
  /* XXX Very temporary!
   * Alter the map to expose BREAK cells.
   */
  if (0) {
    uint8_t *mdst=map->v;
    const uint8_t *msrc=SPRITE->grid;
    int i=NS_sys_mapw*NS_sys_maph;
    for (;i-->0;mdst++,msrc++) {
      if (*msrc==GRID_BREAK) *mdst=0x06;
      else if (*mdst==0x06) *mdst=0x00;
    }
    g.camera.mapsdirty=1;
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

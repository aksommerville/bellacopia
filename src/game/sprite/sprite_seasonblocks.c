/* sprite_seasonblocks.c
 * We're not a visible sprite; just controller for the 4 season blocks that we spawn.
 */
 
#include "game/bellacopia.h"

#define ROLE_HINT 1
#define ROLE_PUZZLE 2

struct sprite_seasonblocks {
  struct sprite hdr;
  int role;
  int seed; // Not really a seed; it's the actual placement of the 4 blocks.
  int solution[9]; // LRTB, sprite rid or zero.
  int col,row; // Position of the solution field's top left corner in plane meters. I start in the center, so this is (-1,-1) from me.
  int solved;
};

#define SPRITE ((struct sprite_seasonblocks*)sprite)

/* Cleanup.
 */
 
static void _seasonblocks_del(struct sprite *sprite) {
}

/* Spawn a block.
 */
 
static int seasonblocks_spawn(struct sprite *sprite,int subx,int suby,int rid) {
  struct sprite *block=sprite_spawn(SPRITE->col+subx+0.5,SPRITE->row+suby+0.5,rid,0,0,0,0,0);
  if (!block) return -1;
  return 0;
}

/* Init.
 */
 
static int _seasonblocks_init(struct sprite *sprite) {
  SPRITE->role=sprite->arg[0];
  SPRITE->col=(int)sprite->x-1;
  SPRITE->row=(int)sprite->y-1;
  SPRITE->solved=store_get_fld(NS_fld_seasonblocks);
  
  /* Regardless of our role, if the puzzle isn't initialized, do that now.
   */
  SPRITE->seed=store_get_fld16(NS_fld16_seasonblocks_seed);
  if (!SPRITE->seed) {
    int range=9*8*7*6;
    SPRITE->seed=1+rand()%range;
    store_set_fld16(NS_fld16_seasonblocks_seed,SPRITE->seed);
  }
  
  /* Expand the seed into a more digestible solution.
   */
  int n=SPRITE->seed-1;
  uint8_t available[9]={0,1,2,3,4,5,6,7,8};
  int availablec=9;
  #define NEXT(rid) { \
    int avp=n%availablec; \
    n/=availablec; \
    SPRITE->solution[available[avp]]=rid; \
    availablec--; \
    memmove(available+avp,available+avp+1,availablec-avp); \
  }
  NEXT(RID_sprite_season_winter)
  NEXT(RID_sprite_season_spring)
  NEXT(RID_sprite_season_summer)
  NEXT(RID_sprite_season_autumn)
  #undef NEXT
  
  /* If we're the hint, or the puzzle is already solved, spawn blocks in the solution positions.
   */
  if ((SPRITE->role==ROLE_HINT)||SPRITE->solved) {
    const int *rid=SPRITE->solution;
    int suby=0; for (;suby<3;suby++) {
      int subx=0; for (;subx<3;subx++,rid++) {
        if (!*rid) continue;
        if (seasonblocks_spawn(sprite,subx,suby,*rid)<0) return -1;
      }
    }
  /* Or if we're the unsolved puzzle, put the four blocks in constant OOB positions.
   */
  } else {
    if (seasonblocks_spawn(sprite,-1,-1,RID_sprite_season_winter)<0) return -1;
    if (seasonblocks_spawn(sprite, 3,-1,RID_sprite_season_spring)<0) return -1;
    if (seasonblocks_spawn(sprite, 3, 3,RID_sprite_season_summer)<0) return -1;
    if (seasonblocks_spawn(sprite,-1, 3,RID_sprite_season_autumn)<0) return -1;
  }
  
  return 0;
}

/* Update.
 */
 
static void _seasonblocks_update(struct sprite *sprite,double elapsed) {
  // Only need to update if we're the puzzle role and unsolved.
  if (SPRITE->solved) return;
  if (SPRITE->role!=ROLE_PUZZLE) return;
  
  // The blocks are all in "hookpull" group, which should be pretty light otherwise.
  int matchc=0;
  struct sprite **blockp=GRP(hookpull)->sprv;
  int i=GRP(hookpull)->sprc;
  for (;i-->0;blockp++) {
    struct sprite *block=*blockp;
    if (!block->rid) continue;
    int qx=(int)block->x-SPRITE->col;
    if ((qx<0)||(qx>=3)) continue; // continue, not break: We don't know yet whether this block is actually part of the puzzle.
    int qy=(int)block->y-SPRITE->row;
    if ((qy<0)||(qy>=3)) continue;
    int sp=qy*3+qx;
    if (SPRITE->solution[sp]==block->rid) {
      matchc++;
    } else {
      // Something is in my bounds and it's not in the solution place. We're not solved.
      matchc=0;
      break;
    }
  }
  
  // If we got 4 matches, we're solved, hooray!
  if (matchc==4) {
    bm_sound(RID_sound_secret);
    store_set_fld(NS_fld_seasonblocks,1);
    g.camera.mapsdirty=1;
    SPRITE->solved=1;
    // They can keep moving blocks around after solving, no worries, it won't reset the solved state or anything.
  }
}

/* Render.
 */
 
static void _seasonblocks_render(struct sprite *sprite,int x,int y) {
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_seasonblocks={
  .name="seasonblocks",
  .objlen=sizeof(struct sprite_seasonblocks),
  .del=_seasonblocks_del,
  .init=_seasonblocks_init,
  .update=_seasonblocks_update,
  .render=_seasonblocks_render,
};

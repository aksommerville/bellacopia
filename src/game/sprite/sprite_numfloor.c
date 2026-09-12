/* sprite_numfloor.c
 * 4x4 grid that when you step on a tile, it gets the next digit.
 * After you've placed '9', the next step clears everything and places '0'.
 * We coordinate with numfloor_ref to trigger flags.
 * Our position must be the top-left corner of that 4x4 grid.
 */
 
#include "game/bellacopia.h"

#define DOOR_LIMIT 8

struct sprite_numfloor {
  struct sprite hdr;
  int qx,qy; // Top left corner in plane meters.
  struct door {
    int fld16id;
    int fldid;
    int password; // Value under (fld16id). If zero, we haven't got it yet. Lazy.
  } doorv[DOOR_LIMIT];
  int doorc;
  int hx,hy; // Recent quantized hero position.
  uint8_t state[16]; // 4x4 LRTB of tileid. Zero for none.
  int next; // 0..9, next digit we'll place. If 0, we'll clear the state first.
};

#define SPRITE ((struct sprite_numfloor*)sprite)

/* Init.
 */
 
static int _numfloor_init(struct sprite *sprite) {

  SPRITE->qx=(int)sprite->x;
  SPRITE->qy=(int)sprite->y;
  SPRITE->hx=SPRITE->hy=-1;

  /* Read the map to identify my associated passwords.
   * numfloor and numfloor_ref in a group must all be on the same map.
   */
  const struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_sprite: {
          int rid=(cmd.arg[2]<<8)|cmd.arg[3];
          if (rid==RID_sprite_numfloor_ref) {
            int fld16id=(cmd.arg[4]<<8)|cmd.arg[5];
            int fldid=(cmd.arg[6]<<8)|cmd.arg[7];
            if (SPRITE->doorc>DOOR_LIMIT) {
              fprintf(stderr,"Too many doors for numfloor, limit %d.\n",DOOR_LIMIT);
            } else {
              SPRITE->doorv[SPRITE->doorc++]=(struct door){fld16id,fldid,0};
            }
          }
        } break;
    }
  }
  
  return 0;
}

/* There are four positions where a password can be encoded.
 * Since there's only ten digits, only two positions can be full at a time, but just let that happen naturally.
 * This gets called on every state change (but not initially), and it sets every known output field.
 * Returns nonzero if we played a sound.
 */
 
static int numfloor_check_completion(struct sprite *sprite) {
  if (!SPRITE->doorc) return 0;
  int result=0;

  // Collect the four-digit passwords currently present. (can't actually be more than two, and zero is perfectly normal).
  int passv[4];
  int passc=0;
  const uint8_t *src=SPRITE->state;
  int i=4;
  for (;i-->0;src+=4) {
    if (!src[0]||!src[1]||!src[2]||!src[3]) continue; // Row has at least one blank.
    passv[passc++]=(
      (src[0]-sprite->tileid)*1000+
      (src[1]-sprite->tileid)*100+
      (src[2]-sprite->tileid)*10+
      (src[3]-sprite->tileid)
    );
  }
  
  // Visit each door. Load its password if we haven't yet, and compare to the visible passwords.
  // Set each door's flag every time. If one was held open from a previous session, the first step closes it.
  struct door *door=SPRITE->doorv;
  for (i=SPRITE->doorc;i-->0;door++) {
    if (!door->password) door->password=store_get_fld16(door->fld16id);
    int present=0;
    const int *pp=passv;
    int pi=passc;
    for (;pi-->0;pp++) if (*pp==door->password) {
      present=1;
      break;
    }
    int pv=store_get_fld(door->fldid);
    if (present&&!pv) {
      result=1;
      store_set_fld(door->fldid,1);
      g.camera.mapsdirty=1;
    } else if (!present&&pv) {
      store_set_fld(door->fldid,0);
      g.camera.mapsdirty=1;
    }
  }
  if (result) bm_sound(RID_sound_treasure);
  return result;
}

/* We got stepped on.
 * (rx,ry) is the relative cell position 0..3, no oobs please.
 */
 
static void numfloor_visit_cell(struct sprite *sprite,int rx,int ry) {
  int p=ry*4+rx;
  if ((p<0)||(p>=sizeof(SPRITE->state))) return; // oy, we said no oobs!
  if (SPRITE->state[p]) return; // Restepping on a visited cell has no effect.
  if (!SPRITE->next) memset(SPRITE->state,0,sizeof(SPRITE->state)); // Before placing zero, clear the state.
  SPRITE->state[p]=sprite->tileid+SPRITE->next;
  int digit=SPRITE->next;
  if (++(SPRITE->next)>=10) SPRITE->next=0;
  if (GRP(hero)->sprc>=1) { // Make a toast with the newly-placed digit, same idea as minesweep.
    struct sprite *hero=GRP(hero)->sprv[0];
    struct sprite *toast=sprite_spawn(hero->x,hero->y-0.5,0,0,0,&sprite_type_toast,0,0);
    if (toast) {
      char ch='0'+digit;
      sprite_toast_set_text(toast,&ch,1);
    }
  }
  if (!numfloor_check_completion(sprite)) {
    bm_sound(RID_sound_treadle);
  }
}

/* Update.
 */
 
static void _numfloor_update(struct sprite *sprite,double elapsed) {
  int hx=-1,hy=-1;
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    if (sprite_hero_is_grounded(hero)) {
      hx=(int)hero->x;
      hy=(int)hero->y;
    }
  }
  if ((hx!=SPRITE->hx)||(hy!=SPRITE->hy)) {
    SPRITE->hx=hx;
    SPRITE->hy=hy;
    int rx=hx-SPRITE->qx;
    int ry=hy-SPRITE->qy;
    if ((rx>=0)&&(rx<4)&&(ry>=0)&&(ry<4)) {
      numfloor_visit_cell(sprite,rx,ry);
    }
  }
}

/* Render.
 */
 
static void _numfloor_render(struct sprite *sprite,int dstx,int dsty) {
  graf_set_image(&g.graf,sprite->imageid);
  int dstx0=dstx;
  int dsty0=dsty;
  int yi=4;
  const uint8_t *tileid=SPRITE->state;
  for (;yi-->0;dsty+=NS_sys_tilesize) {
    int xi=4;
    for (dstx=dstx0;xi-->0;dstx+=NS_sys_tilesize,tileid++) {
      if (!*tileid) continue;
      graf_tile(&g.graf,dstx,dsty,*tileid,0);
    }
  }
  
  // While Dot is standing on us, highlight the focussed cell. Same as her shovel.
  int rx=SPRITE->hx-SPRITE->qx;
  int ry=SPRITE->hy-SPRITE->qy;
  if ((rx>=0)&&(rx<4)&&(ry>=0)&&(ry<4)) {
    int hlx=dstx0+rx*NS_sys_tilesize;
    int hly=dsty0+ry*NS_sys_tilesize;
    uint8_t xform=0;
    switch ((g.framec/10)&3) {
      case 1: xform=EGG_XFORM_SWAP|EGG_XFORM_XREV; break;
      case 2: xform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
      case 3: xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
    }
    graf_set_image(&g.graf,RID_image_hero);
    graf_tile(&g.graf,hlx,hly,0x00,xform);
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_numfloor={
  .name="numfloor",
  .objlen=sizeof(struct sprite_numfloor),
  .init=_numfloor_init,
  .update=_numfloor_update,
  .render=_numfloor_render,
};

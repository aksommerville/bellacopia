/* sprite_letfloor.c
 * Same as "numfloor" but instead of '0'..'9', we emit the name of your equipped item.
 */
 
#include "game/bellacopia.h"

#define DOOR_LIMIT 8
#define PASSWORD_SIZE 4 /* not negotiable */

struct sprite_letfloor {
  struct sprite hdr;
  int qx,qy; // Top left corner in plane meters.
  struct door {
    int strix; // strings:item
    int fldid;
    char text[PASSWORD_SIZE];
  } doorv[DOOR_LIMIT];
  int doorc;
  int hx,hy; // Recent quantized hero position.
  char state[16]; // 4x4 LRTB of tileid (ascii). Zero for none.
  
  int equipped; // itemid. Compare to (g.store.invstorev[0].itemid).
  const char *eqname; // Name of equipped item. Not normalized, we do that on the fly.
  int eqnamec;
  int eqnamep;
};

#define SPRITE ((struct sprite_letfloor*)sprite)

/* Init.
 */
 
static int _letfloor_init(struct sprite *sprite) {

  SPRITE->qx=(int)sprite->x;
  SPRITE->qy=(int)sprite->y;
  SPRITE->hx=SPRITE->hy=-1;

  /* Read the map to identify my associated passwords.
   * letfloor and letfloor_ref in a group must all be on the same map.
   */
  const struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (!map) return -1;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_sprite: {
          int rid=(cmd.arg[2]<<8)|cmd.arg[3];
          if (rid==RID_sprite_letfloor_ref) {
            int strix=(cmd.arg[4]<<8)|cmd.arg[5];
            int fldid=(cmd.arg[6]<<8)|cmd.arg[7];
            if (SPRITE->doorc>DOOR_LIMIT) {
              fprintf(stderr,"Too many doors for letfloor, limit %d.\n",DOOR_LIMIT);
            } else {
              struct door *door=SPRITE->doorv+SPRITE->doorc++;
              door->strix=strix;
              door->fldid=fldid;
              const char *src;
              int srcc=text_get_string(&src,RID_strings_item,strix);
              if (srcc!=PASSWORD_SIZE) {
                fprintf(stderr,"strings:item:%d: Expected length %d, found %d\n",strix,PASSWORD_SIZE,srcc);
                return -1;
              }
              memcpy(door->text,src,PASSWORD_SIZE);
            }
          }
        } break;
    }
  }
  
  return 0;
}

/* Is this password visible, in one of the four active positions?
 */
 
static int letfloor_password_present(struct sprite *sprite,const char *src) {
  int statep=0;
  for (;statep<sizeof(SPRITE->state);statep+=PASSWORD_SIZE) {
    if (!memcmp(SPRITE->state+statep,src,PASSWORD_SIZE)) return 1;
  }
  return 0;
}

/* There are four positions where a password can be encoded.
 * This gets called on every state change (but not initially), and it sets every known output field.
 * Returns nonzero if we played a sound.
 */
 
static int letfloor_check_completion(struct sprite *sprite) {
  if (!SPRITE->doorc) return 0;
  int result=0,i;
  struct door *door=SPRITE->doorv;
  for (i=SPRITE->doorc;i-->0;door++) {
    int present=letfloor_password_present(sprite,door->text);
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

/* Refresh (equipped) et al per current state.
 * Fails if we don't have a valid name.
 */
 
static int letfloor_require_eqname(struct sprite *sprite) {
  
  // Has the equipped item changed?
  if (SPRITE->equipped!=g.store.invstorev[0].itemid) {
    SPRITE->equipped=g.store.invstorev[0].itemid;
    SPRITE->eqnamec=0;
    SPRITE->eqnamep=0;
    const struct item_detail *detail=item_detail_for_itemid(SPRITE->equipped);
    if (detail) {
      SPRITE->eqnamec=text_get_string(&SPRITE->eqname,RID_strings_item,detail->strix_name);
      // First character must be a letter.
      if ((SPRITE->eqnamec>=1)&&(
        ((SPRITE->eqname[0]>='A')&&(SPRITE->eqname[0]<='Z'))||
        ((SPRITE->eqname[0]>='a')&&(SPRITE->eqname[0]<='z'))
      )) {
        // ok
      } else {
        fprintf(stderr,"letfloor: invalid name for item %d\n",SPRITE->equipped);
        SPRITE->eqnamec=0;
      }
    }
  }
  
  // Pass if our name contains a letter.
  const char *v=SPRITE->eqname;
  int i=SPRITE->eqnamec;
  for (;i-->0;v++) {
    if ((*v>='a')&&(*v<='z')) return 0;
    if ((*v>='A')&&(*v<='Z')) return 0;
  }
  return -1;
}

/* We got stepped on.
 * (rx,ry) is the relative cell position 0..3, no oobs please.
 */
 
static void letfloor_visit_cell(struct sprite *sprite,int rx,int ry) {
  int p=ry*4+rx;
  if ((p<0)||(p>=sizeof(SPRITE->state))) return; // oy, we said no oobs!
  
  // In general, you can re-step on cells already visited and it's noop.
  // But if the entire field is full, reset. (this situation was not possible for numfloor, but easy to reach for us).
  if (SPRITE->state[p]) {
    const char *v=SPRITE->state;
    int i=sizeof(SPRITE->state);
    for (;i-->0;v++) if (!*v) return;
    // it's full!
    memset(SPRITE->state,0,sizeof(SPRITE->state));
  }
  
  if (letfloor_require_eqname(sprite)<0) return; // eg nothing equipped. Noop.
  
  // Advance (eqnamep) until it reaches a letter.
  // If we wrap around, clear the state.
  int panic=SPRITE->eqnamec+1;
  for (;;) {
    if (panic--<0) return;
    if (SPRITE->eqnamep>=SPRITE->eqnamec) {
      SPRITE->eqnamep=0;
      memset(SPRITE->state,0,sizeof(SPRITE->state));
    }
    char ch=SPRITE->eqname[SPRITE->eqnamep];
    if ((ch>='a')&&(ch<='z')) break;
    if ((ch>='A')&&(ch<='Z')) break;
    SPRITE->eqnamep++;
  }
  
  char ch=SPRITE->eqname[SPRITE->eqnamep];
  if ((ch>='a')&&(ch<='z')) ch-=0x20;
  SPRITE->state[p]=ch;
  SPRITE->eqnamep++;
  if (GRP(hero)->sprc>=1) { // Make a toast with the newly-placed digit, same idea as minesweep.
    struct sprite *hero=GRP(hero)->sprv[0];
    struct sprite *toast=sprite_spawn(hero->x,hero->y-0.5,0,0,0,&sprite_type_toast,0,0);
    if (toast) sprite_toast_set_text(toast,&ch,1);
  }
  if (!letfloor_check_completion(sprite)) {
    bm_sound(RID_sound_treadle);
  }
}

/* Update.
 */
 
static void _letfloor_update(struct sprite *sprite,double elapsed) {
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
      letfloor_visit_cell(sprite,rx,ry);
    }
  }
}

/* Render.
 */
 
static void _letfloor_render(struct sprite *sprite,int dstx,int dsty) {
  int dstx0=dstx;
  int dsty0=dsty;
  
  // Depressed tile at any nonzero cell. Vacant tiles, let the map handle it.
  graf_set_image(&g.graf,sprite->imageid);
  int yi=4;
  const char *tileid=SPRITE->state;
  for (;yi-->0;dsty+=NS_sys_tilesize) {
    int xi=4;
    for (dstx=dstx0;xi-->0;dstx+=NS_sys_tilesize,tileid++) {
      if (!*tileid) continue;
      graf_tile(&g.graf,dstx,dsty,sprite->tileid,0);
    }
  }
  
  // Letters on depressed tiles.
  graf_set_image(&g.graf,RID_image_fonttiles);
  for (yi=4,tileid=SPRITE->state,dsty=dsty0;yi-->0;dsty+=NS_sys_tilesize) {
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
 
const struct sprite_type sprite_type_letfloor={
  .name="letfloor",
  .objlen=sizeof(struct sprite_letfloor),
  .init=_letfloor_init,
  .update=_letfloor_update,
  .render=_letfloor_render,
};

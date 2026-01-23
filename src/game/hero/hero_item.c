#include "hero_internal.h"

/* Broom.
 */
 
static int broom_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_broom;
  return 1;
}

static void broom_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
}

static void broom_update(struct sprite *sprite,double elapsed) {
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    broom_end(sprite);
    return;
  }
  //TODO broom
}

/* Divining Rod.
 * Unlike most updating items, our update is called passively, doesn't need to be "in use", just equipped.
 */
 
static int divining_begin(struct sprite *sprite) {
  SPRITE->divining_alert_clock=0.500;
  int soundid=RID_sound_negatory;

  /* Prepare the alerts with quantized positions and the negative tile.
   */
  struct divining_alert *alert=SPRITE->divining_alertv;
  int dy=-1; for (;dy<=1;dy++) {
    int dx=-1; for (;dx<=1;dx++,alert++) {
      alert->tileid=0x68;
      alert->x=(SPRITE->qx+dx)*NS_sys_tilesize+(NS_sys_tilesize>>1);
      alert->y=(SPRITE->qy+dy)*NS_sys_tilesize+(NS_sys_tilesize>>1);
    }
  }
  
  /* Check for roots in cells adjacent to the quantized cell and flip positive where we find one.
   * It's a pain in the butt because we need neighbor cells, which might be on neighbor *maps*.
   */
  int mxa=(SPRITE->qx-1)/NS_sys_mapw;
  int mxz=(SPRITE->qx+1)/NS_sys_mapw;
  int mya=(SPRITE->qy-1)/NS_sys_maph;
  int myz=(SPRITE->qy+1)/NS_sys_maph;
  int my=mya; for (;my<=myz;my++) {
    int mx=mxa; for (;mx<=mxz;mx++) {
      const struct map *map=map_by_position(mx,my,sprite->z);
      if (!map) continue;
      int x0=map->lng*NS_sys_mapw;
      int y0=map->lat*NS_sys_maph;
      struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
      struct cmdlist_entry cmd;
      while (cmdlist_reader_next(&cmd,&reader)) {
        if (cmd.opcode==CMD_map_root) {
          int rx=cmd.arg[0]+x0;
          int ry=cmd.arg[1]+y0;
          if (rx<SPRITE->qx-1) continue;
          if (rx>SPRITE->qx+1) continue;
          if (ry<SPRITE->qy-1) continue;
          if (ry>SPRITE->qy+1) continue;
          int fld=(cmd.arg[2]<<8)|cmd.arg[3];
          if (!store_get_fld(fld)) {
            SPRITE->divining_alertv[
              (ry-SPRITE->qy+1)*3+(rx-SPRITE->qx+1)
            ].tileid=0x69;
            soundid=RID_sound_affirmative;
          }
        }
      }
    }
  }
  bm_sound(soundid);
  return 1;
}

static void divining_update(struct sprite *sprite,double elapsed) {
  int qx=(int)sprite->x;
  int qy=(int)sprite->y;
  if ((qx==SPRITE->qx)&&(qy==SPRITE->qy)) return;
  SPRITE->qx=qx;
  SPRITE->qy=qy;
  SPRITE->root=0;
  const struct map *map=map_by_sprite_position(sprite->x,sprite->y,sprite->z);
  if (map) {
    qx-=map->lng*NS_sys_mapw;
    qy-=map->lat*NS_sys_maph;
    struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)) {
      if (cmd.opcode==CMD_map_root) {
        if (cmd.arg[0]!=qx) continue;
        if (cmd.arg[1]!=qy) continue;
        int fld=(cmd.arg[2]<<8)|cmd.arg[3];
        if (!store_get_fld(fld)) {
          SPRITE->root=1;
          return;
        }
      }
    }
  }
}

/* Match.
 */
 
static int match_begin(struct sprite *sprite) {
  if (g.store.invstorev[0].quantity<1) return 0;
  g.store.invstorev[0].quantity--;
  bm_sound(RID_sound_match);
  SPRITE->matchclock+=5.000;
  sprite_group_add(GRP(light),sprite);
  return 1;
}

/* Wand.
 */
 
static int wand_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_wand;
  SPRITE->spellc=0;
  SPRITE->wanddir='.';
  return 1;
}

static void wand_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
  if (game_cast_spell(SPRITE->spell,SPRITE->spellc)) {
  } else {
    bm_sound(RID_sound_reject);
  }
}

static void wand_update(struct sprite *sprite,double elapsed) {
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    wand_end(sprite);
    return;
  }
  uint8_t ndir=0;
  switch (g.input[0]&(EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_LEFT: ndir='L'; break;
    case EGG_BTN_RIGHT: ndir='R'; break;
    case EGG_BTN_UP: ndir='U'; break;
    case EGG_BTN_DOWN: ndir='D'; break;
  }
  if (ndir==SPRITE->wanddir) return;
  if (SPRITE->wanddir=ndir) {
    if (SPRITE->spellc>=SPELL_LIMIT) {
      memmove(SPRITE->spell,SPRITE->spell+1,SPELL_LIMIT-1);
      SPRITE->spell[0]='.';
      SPRITE->spell[SPELL_LIMIT-1]=ndir;
    } else {
      SPRITE->spell[SPRITE->spellc++]=ndir;
    }
    bm_sound(RID_sound_wandstroke);
  } else {
    bm_sound(RID_sound_wandunstroke);
  }
}

/* Fishpole.
 */
 
static int fishpole_begin(struct sprite *sprite) {
  //TODO verify position
  SPRITE->itemid_in_progress=NS_itemid_fishpole;
  return 1;
}

static void fishpole_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
}

static void fishpole_update(struct sprite *sprite,double elapsed) {
  //TODO Releasing SOUTH doesn't end the mode, it's on a clock. Fudged out for now.
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    fishpole_end(sprite);
    return;
  }
}

/* Bug Spray.
 */
 
static int bugspray_begin(struct sprite *sprite) {
  if (g.store.invstorev[0].quantity<1) return 0;
  g.store.invstorev[0].quantity--;
  g.store.dirty=1;
  g.bugspray+=5.000;
  bm_sound(RID_sound_bugspray);
  return 1;
}

/* Potion.
 */
 
static int potion_begin(struct sprite *sprite) {
  if (g.store.invstorev[0].quantity<1) return 0;
  // Forbid quaffture if HP full, it must be an accident.
  if (store_get_fld16(NS_fld16_hp)>=store_get_fld16(NS_fld16_hpmax)) return 0;
  SPRITE->itemid_in_progress=NS_itemid_potion;
  SPRITE->potionclock=1.000;
  bm_sound(RID_sound_glug);
  return 1;
}

static void potion_update(struct sprite *sprite,double elapsed) {
  double pvclock=SPRITE->potionclock;
  if ((SPRITE->potionclock-=elapsed)<=0.0) {
    SPRITE->itemid_in_progress=0;
  } else if ((SPRITE->potionclock<0.500)&&(pvclock>=0.500)) {
    bm_sound(RID_sound_glug2);
    // Don't assume that potion is still armed; the user could have paused while animating.
    struct invstore *invstore=store_get_itemid(NS_itemid_potion);
    if (invstore&&(invstore->quantity>0)) {
      invstore->quantity--;
      g.store.dirty=1;
    }
    // If the item disappeared or zeroed out, heal anyway i guess? We made the sound.
    store_set_fld16(NS_fld16_hp,store_get_fld16(NS_fld16_hpmax));
  }
}

/* Hookshot.
 */
 
static int hookshot_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_hookshot;
  bm_sound(RID_sound_hookshot_begin);
  return 1;
}

static void hookshot_update(struct sprite *sprite,double elapsed) {
  //TODO Multiple stages, pretty complicated. For now, just exit the state when button released.
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    SPRITE->itemid_in_progress=0;
  }
}

/* Candy.
 */
 
static int candy_begin(struct sprite *sprite) {
  if (g.store.invstorev[0].quantity<1) return 0;
  double x=sprite->x,y=sprite->y;
  if (SPRITE->facedx<0) x-=1.0;
  else if (SPRITE->facedx>0) x+=1.0;
  else if (SPRITE->facedy<0) y-=1.0;
  else y+=1.0;
  struct sprite *candy=sprite_spawn(x,y,RID_sprite_candy,0,0,0,0,0);
  if (!candy) return 0;
  g.store.invstorev[0].quantity--;
  g.store.dirty=1;
  bm_sound(RID_sound_deploy);
  return 1;
}

/* Vanishing cream.
 */
 
static int vanishing_begin(struct sprite *sprite) {
  if (g.store.invstorev[0].quantity<1) return 0;
  g.store.invstorev[0].quantity--;
  g.store.dirty=1;
  g.vanishing+=5.000;
  bm_sound(RID_sound_vanishing);
  return 1;
}

/* Bell.
 */
 
static int bell_begin(struct sprite *sprite) {
  bm_sound(RID_sound_bell);
  //TODO Visual feedback.
  //TODO Some kind of global alert. Monsters should notice, maybe other things happen.
  return 1;
}

/* Compass.
 * We get passive updates like Divining Rod.
 */
 
static int cb_compass(int optionid,void *userdata) {
  if (optionid<1) return 0;
  struct sprite *sprite=userdata;
  store_set_fld16(NS_fld16_compassoption,optionid);
  const char *name=0; int namec=text_get_string(&name,RID_strings_item,optionid);
  SPRITE->compassz=-1; // Refresh lazily at next update.
  return 1;
}
 
static int compass_begin(struct sprite *sprite) {
  struct modal_args_dialogue args={
    .rid=RID_strings_item,
    .strix=43,
    .speaker=sprite,
    .cb=cb_compass,
    .userdata=sprite,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return 0;
  int strixv[10];
  int strixc=game_list_targets(strixv,sizeof(strixv)/sizeof(int));
  int i=0; for (;i<strixc;i++) {
    modal_dialogue_add_option_string(modal,RID_strings_item,strixv[i]);
  }
  modal_dialogue_set_default(modal,store_get_fld16(NS_fld16_compassoption));
  return 1;
}

static void compass_update(struct sprite *sprite,double elapsed) {
  
  /* Do we need to refresh?
   */
  if (SPRITE->compassz!=sprite->z) {
    SPRITE->compassz=sprite->z;
    int srcx=(int)sprite->x,srcy=(int)sprite->y,dstx=-1,dsty=-1;
    int compass=store_get_fld16(NS_fld16_compassoption);
    if (!compass) compass=NS_compass_auto;
    if (game_get_target_position(&dstx,&dsty,srcx,srcy,sprite->z,compass)<0) {
      SPRITE->compassx=SPRITE->compassy=-1.0;
      return;
    }
    SPRITE->compassx=dstx+0.5;
    SPRITE->compassy=dsty+0.5;
  }
  if (SPRITE->compassx<0.0) return;
  
  /* Update angle.
   */
  double dx=SPRITE->compassx-sprite->x;
  double dy=SPRITE->compassy-sprite->y;
  SPRITE->compasst=atan2(dx,dy);
  // At first I was thinking of spinning and slowing down near the target, a la Full Moon and Arrautza.
  // But I kind of like this simplistic locked-on approach.
  // Anyhoo, we can change minds at any time in the future. (compasst) should be the after-animation position, where it should render.
}

/* Update items, main entry point.
 */
 
void hero_item_update(struct sprite *sprite,double elapsed) {

  /* If an item is being used, it has full control.
   */
  if (SPRITE->itemid_in_progress) {
    switch (SPRITE->itemid_in_progress) {
      case NS_itemid_broom: broom_update(sprite,elapsed); break;
      case NS_itemid_wand: wand_update(sprite,elapsed); break;
      case NS_itemid_fishpole: fishpole_update(sprite,elapsed); break;
      case NS_itemid_hookshot: hookshot_update(sprite,elapsed); break;
      case NS_itemid_potion: potion_update(sprite,elapsed); break;
      default: fprintf(stderr,"%s:%d:ERROR: Item %d is in progress but has no update handler.\n",__FILE__,__LINE__,SPRITE->itemid_in_progress);
    }
    return;
  }
  
  /* Divining Rod and Compass get updated passively just by virtue of being equipped.
   */
  switch (g.store.invstorev[0].itemid) {
    case NS_itemid_divining: divining_update(sprite,elapsed); break;
    case NS_itemid_compass: compass_update(sprite,elapsed); break;
  }
  
  /* Poll input.
   * Call out when SOUTH is pressed. Handler may set itemid_in_progress.
   */
  if (SPRITE->item_blackout) {
    if (g.input[0]&EGG_BTN_SOUTH) return;
    SPRITE->item_blackout=0;
  }
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    int result=0;
    switch (g.store.invstorev[0].itemid) {
      case NS_itemid_broom: result=broom_begin(sprite); break;
      case NS_itemid_divining: result=divining_begin(sprite); break;
      case NS_itemid_match: result=match_begin(sprite); break;
      case NS_itemid_wand: result=wand_begin(sprite); break;
      case NS_itemid_fishpole: result=fishpole_begin(sprite); break;
      case NS_itemid_bugspray: result=bugspray_begin(sprite); break;
      case NS_itemid_potion: result=potion_begin(sprite); break;
      case NS_itemid_hookshot: result=hookshot_begin(sprite); break;
      case NS_itemid_candy: result=candy_begin(sprite); break;
      case NS_itemid_vanishing: result=vanishing_begin(sprite); break;
      case NS_itemid_compass: result=compass_begin(sprite); break;
      case NS_itemid_bell: result=bell_begin(sprite); break;
    }
    if (!result) {
      bm_sound(RID_sound_reject);
    }
  }
}

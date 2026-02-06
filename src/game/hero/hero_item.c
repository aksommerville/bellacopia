#include "hero_internal.h"

/* Broom.
 */
 
static int broom_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_broom;
  sprite->physics&=~((1<<NS_physics_water)|(1<<NS_physics_hole));
  sprite_group_add(GRP(floating),sprite);
  return 1;
}

static void broom_end(struct sprite *sprite) {
  sprite->physics|=((1<<NS_physics_water)|(1<<NS_physics_hole));
  if (!sprite_test_position(sprite)) {
    // Try quantizing to the cell. If it's still a collision, restore and prevent the unbrooming.
    double x0=sprite->x,y0=sprite->y;
    sprite->x=(int)sprite->x+0.5;
    sprite->y=(int)sprite->y+0.5;
    if (!sprite_test_position(sprite)) {
      sprite->physics&=~((1<<NS_physics_water)|(1<<NS_physics_hole));
      sprite->x=x0;
      sprite->y=y0;
      return;
    }
  }
  SPRITE->itemid_in_progress=0;
  sprite_group_remove(GRP(floating),sprite);
}

static void broom_update(struct sprite *sprite,double elapsed) {
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    broom_end(sprite);
    return;
  }
  if (SPRITE->hurt>0.0) {
    broom_end(sprite);
    return;
  }
  // Was thinking of doing the Thirty-Seconds-Apothecary-style relative steering, but now having second thoughts.
  // The simpler Full Moon and Dead Weight style broom is much easier to do, and also easier for the user to understand.
  // We did the relative broom here in Mark 1. It was awkward switching between relative and absolute control.
  // So for now, hero_item.c only manages toggling the broom state on and off. Motion is just like walking, in hero_motion.c.
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
  SPRITE->matchclock+=8.000;
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
  // Injury cancels the activity.
  if (SPRITE->hurt>0.0) {
    SPRITE->itemid_in_progress=0;
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
 
#define FISH_TIME_MIN 1.000
#define FISH_TIME_MAX 10.000
 
static int fishpole_begin(struct sprite *sprite) {
  double qx=sprite->x+SPRITE->facedx*0.75;
  double qy=sprite->y+SPRITE->facedy*0.75;
  struct map *map=map_by_sprite_position(qx,qy,sprite->z);
  if (!map) return 0;
  int col=((int)qx)%NS_sys_mapw;
  int row=((int)qy)%NS_sys_maph;
  if ((col<0)||(row<0)) return 0;
  uint8_t physics=map->physics[map->v[row*NS_sys_mapw+col]];
  if (physics!=NS_physics_water) return 0;
  bm_sound(RID_sound_fishpole_begin);
  SPRITE->itemid_in_progress=NS_itemid_fishpole;
  SPRITE->fish=0;
  
  const double timerange=FISH_TIME_MAX-FISH_TIME_MIN;
  double norm=(rand()&0x7fff)/32768.0;
  norm=norm*norm*norm; // Raise balanced random to the 3rd power, makes short times way likelier than long.
  SPRITE->fishclock=FISH_TIME_MIN+norm*timerange;
  
  return 1;
}

static void fishpole_end(struct sprite *sprite) {
  SPRITE->itemid_in_progress=0;
}

static void fishpole_cb_battle(struct modal *modal,int outcome,void *userdata) {
  struct sprite *sprite=userdata;
  if (outcome>0) {
    game_get_item(SPRITE->fish,1);
    modal_battle_add_consequence(modal,SPRITE->fish,1);
  }
  // No consequences for losing, just you don't catch the fish.
}

static void fishpole_update(struct sprite *sprite,double elapsed) {

  // Before the fish appears, you can drop SOUTH to cancel.
  if (!SPRITE->fish) {
    if (!(g.input[0]&EGG_BTN_SOUTH)) {
      fishpole_end(sprite);
      return;
    }
  }
  
  // Injury aborts the activity, even if the fish is in flight.
  // What happens to that fish will remain one of life's mysteries.
  if (SPRITE->hurt>0.0) {
    SPRITE->itemid_in_progress=0;
    return;
  }
  
  if ((SPRITE->fishclock-=elapsed)<=0.0) {
    int battle=0;
    switch (SPRITE->fish) {
      case NS_itemid_greenfish: battle=NS_battle_greenfish; break;
      case NS_itemid_bluefish: battle=NS_battle_bluefish; break;
      case NS_itemid_redfish: battle=NS_battle_redfish; break;
    }
    if (battle) {
      struct modal_args_battle args={
        .battle=battle,
        .players=NS_players_man_cpu,
        .handicap=0x80,//TODO how to decide?
        .cb=fishpole_cb_battle,
        .userdata=sprite,
        .skip_prompt=1,
      };
      struct modal *modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
      if (!modal) return;
      SPRITE->itemid_in_progress=0;
    } else {
      if (SPRITE->fish=game_choose_fish(sprite->x,sprite->y,sprite->z)) {
        bm_sound(RID_sound_fishpole_catch);
        SPRITE->fishclock=FISH_FLY_TIME;
      } else {
        bm_sound(RID_sound_fishpole_wompwomp);
        SPRITE->itemid_in_progress=0;
      }
    }
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
 
#define HOOKSTAGE_GO     1
#define HOOKSTAGE_RETURN 2
#define HOOKSTAGE_PULL   3

#define HOOK_SPEED_GO     10.0
#define HOOK_SPEED_RETURN 10.0
#define HOOK_SPEED_PULL   10.0

#define HOOK_DISTANCE_MAX 7.0

#define HOOK_TICK_TIME 0.100
 
static int hookshot_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_hookshot;
  bm_sound(RID_sound_hookshot_begin);
  SPRITE->hookstage=HOOKSTAGE_GO;
  SPRITE->hookdistance=0.5;
  SPRITE->hookclock=HOOK_TICK_TIME; // not zero; we just played hookshot_begin
  SPRITE->safex=sprite->x;
  SPRITE->safey=sprite->y;
  return 1;
}

static void hookshot_end_pull(struct sprite *sprite) {
  sprite->physics|=((1<<NS_physics_water)|(1<<NS_physics_hole));
  SPRITE->itemid_in_progress=0;
  // If after restoring hole collisions, we're still good then we're still good. Done.
  if (sprite_test_position(sprite)) return;
  // Try forcing to the center of my cell.
  sprite->x=(int)sprite->x+0.5;
  sprite->y=(int)sprite->y+0.5;
  if (sprite_test_position(sprite)) return;
  // Emergency. Return to where we first hooked from.
  // TODO Could be trouble if a solid sprite moved into that space during the flight. Should we account for that?
  sprite->x=SPRITE->safex;
  sprite->y=SPRITE->safey;
}

static void hookshot_check_grabbage(struct sprite *sprite) {

  // Position is a little tweaky based on face direction.
  double hx=sprite->x+SPRITE->hookdistance*SPRITE->facedx;
  double hy=sprite->y+SPRITE->hookdistance*SPRITE->facedy;
  if (SPRITE->facedx) hy+=0.25;
  else if (SPRITE->facedy<0) hx+=0.25;
  else hx-=0.25;

  // If there's a grabbable sprite, grab it and return.
  struct sprite **otherp=GRP(grabbable)->sprv;
  int i=GRP(grabbable)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other==sprite) continue; // is the hero grabbable? she shouldn't be
    if (hx>=other->x+other->hbr) continue;
    if (hx<=other->x+other->hbl) continue;
    if (hy>=other->y+other->hbb) continue;
    if (hy<=other->y+other->hbt) continue;
    if (!SPRITE->pumpkin) {
      if (!(SPRITE->pumpkin=sprite_group_new())) return;
      SPRITE->pumpkin->mode=SPRITE_GROUP_MODE_SINGLE;
    }
    if (sprite_group_add(SPRITE->pumpkin,other)<0) return;
    SPRITE->pumpkin_physics=other->physics;
    other->physics&=~((1<<NS_physics_water)|(1<<NS_physics_hole));
    bm_sound(RID_sound_hookshot_grab);
    SPRITE->hookstage=HOOKSTAGE_RETURN;
    return;
  }
  
  // Check the single cell. If grabbable, enter PULL or if solid enter RETURN.
  // Shouldn't mean a thing, just noting: the hook and the hero might not be on the same map.
  int ihx=(int)hx;
  int ihy=(int)hy;
  if ((ihx>=0)&&(ihy>=0)) {
    struct map *map=map_by_sprite_position(hx,hy,sprite->z);
    if (map) {
      int col=ihx%NS_sys_mapw;
      int row=ihy%NS_sys_maph;
      uint8_t physics=map->physics[map->v[row*NS_sys_mapw+col]];
      if (physics==NS_physics_grabbable) {
        bm_sound(RID_sound_hookshot_grab);
        SPRITE->hookstage=HOOKSTAGE_PULL;
        sprite->physics&=~((1<<NS_physics_water)|(1<<NS_physics_hole));
        return;
      } else if ((physics==NS_physics_solid)||(physics==NS_physics_vanishable)) {
        bm_sound(RID_sound_hookshot_reject);
        SPRITE->hookstage=HOOKSTAGE_RETURN;
        return;
      }
    }
  }
  
  // Reject if we touch a solid sprite.
  // This comes after the map check, because we want to check all grabbables first.
  for (otherp=GRP(solid)->sprv,i=GRP(solid)->sprc;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other==sprite) continue;
    if (hx>=other->x+other->hbr) continue;
    if (hx<=other->x+other->hbl) continue;
    if (hy>=other->y+other->hbb) continue;
    if (hy<=other->y+other->hbt) continue;
    bm_sound(RID_sound_hookshot_reject);
    SPRITE->hookstage=HOOKSTAGE_RETURN;
    return;
  }
}

static void hookshot_update(struct sprite *sprite,double elapsed) {

  // If injured while hookshotting, generally we just stop.
  // Use hookshot_end_pull from HOOKSTAGE_PULL because we do want its position corrections.
  // If you get corrected to the launch site, and it no longer collides with the assailant, you still get injured.
  if (SPRITE->hurt>0.0) {
    if (SPRITE->pumpkin) sprite_group_clear(SPRITE->pumpkin);
    if (SPRITE->hookstage==HOOKSTAGE_PULL) hookshot_end_pull(sprite);
    else SPRITE->itemid_in_progress=0;
    return;
  }

  switch (SPRITE->hookstage) {
  
    case HOOKSTAGE_GO: {
        if ((SPRITE->hookclock-=elapsed)<=0.0) {
          SPRITE->hookclock+=HOOK_TICK_TIME;
          bm_sound(RID_sound_hookshot_tick);
        }
        // Releasing SOUTH puts us in RETURN stage.
        if (!(g.input[0]&EGG_BTN_SOUTH)) {
          SPRITE->hookstage=HOOKSTAGE_RETURN;
          break;
        }
        // Throw the hook a bit further, and if it breaches the limit, enter RETURN.
        SPRITE->hookdistance+=HOOK_SPEED_GO*elapsed;
        if (SPRITE->hookdistance>HOOK_DISTANCE_MAX) {
          SPRITE->hookstage=HOOKSTAGE_RETURN;
          break;
        }
        hookshot_check_grabbage(sprite);
      } break;
      
    case HOOKSTAGE_RETURN: {
        if ((SPRITE->hookclock-=elapsed)<=0.0) {
          SPRITE->hookclock+=HOOK_TICK_TIME;
          bm_sound(RID_sound_hookshot_untick);
        }
        if (SPRITE->pumpkin&&SPRITE->pumpkin->sprc) {
          struct sprite *pumpkin=SPRITE->pumpkin->sprv[0];
          if (!sprite_move(pumpkin,-SPRITE->facedx*HOOK_SPEED_RETURN*elapsed,-SPRITE->facedy*HOOK_SPEED_RETURN*elapsed)) {
            // oops it hit a wall or something.
            pumpkin->physics=SPRITE->pumpkin_physics;
            sprite_group_clear(SPRITE->pumpkin);
            //TODO We probably need to check for dropping into water etc, as we do at the end of PULL.
          }
        }
        if ((SPRITE->hookdistance-=HOOK_SPEED_RETURN*elapsed)<=0.0) {
          if (SPRITE->pumpkin&&SPRITE->pumpkin->sprc) {
            struct sprite *pumpkin=SPRITE->pumpkin->sprv[0];
            pumpkin->physics=SPRITE->pumpkin_physics;
            sprite_group_clear(SPRITE->pumpkin);
          }
          SPRITE->itemid_in_progress=0;
        }
      } break;
      
    case HOOKSTAGE_PULL: {
        if ((SPRITE->hookclock-=elapsed)<=0.0) {
          SPRITE->hookclock+=HOOK_TICK_TIME;
          bm_sound(RID_sound_hookshot_untick);
        }
        // OK to abort along the way.
        if (!(g.input[0]&EGG_BTN_SOUTH)) {
          hookshot_end_pull(sprite);
          return;
        }
        // Move the hero toward the hookshot, then drop actual movement from (hookdistance).
        // If she stops altogether, abort the activity.
        double x0=sprite->x,y0=sprite->y;
        if (sprite_move(sprite,SPRITE->facedx*HOOK_SPEED_PULL*elapsed,SPRITE->facedy*HOOK_SPEED_PULL*elapsed)) {
          double actual=(sprite->x-x0)+(sprite->y-y0);
          if (actual>0.0) actual=-actual;
          if ((SPRITE->hookdistance+=actual)<=0.0) {
            hookshot_end_pull(sprite);
          }
        } else {
          hookshot_end_pull(sprite);
        }
      } break;
      
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
  sprite->physics&=~(1<<NS_physics_vanishable);
  bm_sound(RID_sound_vanishing);
  return 1;
}

// Public, called from game_update().
void hero_unvanish(struct sprite *sprite) {
  int pvphysics=sprite->physics;
  sprite->physics|=(1<<NS_physics_vanishable);
  if (!sprite_test_position(sprite)) {
    sprite->physics=pvphysics;
  }
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
  int strixc=game_list_targets(strixv,sizeof(strixv)/sizeof(int),TARGET_MODE_AVAILABLE);
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

/* Barrel Hat.
 */
 
static int barrelhat_begin(struct sprite *sprite) {
  
  // First find the map and confirm the cell is grabbable; all barrels are.
  const struct map *map=map_by_sprite_position(sprite->x+SPRITE->facedx,sprite->y+SPRITE->facedy,sprite->z);
  if (!map||!map->rid) return 0;
  int col=(int)sprite->x+SPRITE->facedx-map->lng*NS_sys_mapw;
  int row=(int)sprite->y+SPRITE->facedy-map->lat*NS_sys_maph;
  if ((col<0)||(row<0)||(col>=NS_sys_mapw)||(row>=NS_sys_maph)) return 0;
  if (map->physics[map->v[row*NS_sys_mapw+col]]!=NS_physics_grabbable) return 0;
  
  // There must be both 'switchable' and 'barrelhat' commands at that cell.
  int isbarrelhat=0,fld=0;
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_barrelhat: {
          if ((cmd.arg[0]==col)&&(cmd.arg[1]==row)) isbarrelhat=1;
        } break;
      case CMD_map_switchable: {
          if ((cmd.arg[0]==col)&&(cmd.arg[1]==row)) {
            fld=(cmd.arg[2]<<8)|cmd.arg[3];
          }
        } break;
    }
  }
  if (!isbarrelhat||!fld) return 0;
  
  // Is it hatted already?
  if (store_get_fld(fld)) return 0;
  
  // Remove from inventory, set the fld, and play a sound.
  g.store.invstorev[0].itemid=0;
  store_set_fld(fld,1);
  g.camera.mapsdirty=1;
  bm_sound(RID_sound_presto);
  return 1;
}

/* Telescope.
 */
 
#define TELESCOPE_AWAY_SPEED 250.0 /* px/s (NB not meters) */
 
static int telescope_begin(struct sprite *sprite) {
  SPRITE->itemid_in_progress=NS_itemid_telescope;
  return 1;
}

static void telescope_update(struct sprite *sprite,double elapsed) {
  if (!(g.input[0]&EGG_BTN_SOUTH)) {
    g.camera.teledx=0.0;
    g.camera.teledy=0.0;
    g.camera.cut=1;
    SPRITE->itemid_in_progress=0;
  } else {
    g.camera.teledx+=TELESCOPE_AWAY_SPEED*elapsed*SPRITE->facedx;
    g.camera.teledy+=TELESCOPE_AWAY_SPEED*elapsed*SPRITE->facedy;
  }
}

/* Shovel. Update is passive.
 */
 
static void shovel_update(struct sprite *sprite,double elapsed) {
  SPRITE->qx=(int)(sprite->x+SPRITE->facedx*0.750);
  SPRITE->qy=(int)(sprite->y+SPRITE->facedy*0.750);
}
 
static int shovel_begin(struct sprite *sprite) {

  // Confirm map cell is vacant or safe.
  struct map *map=map_by_sprite_position(SPRITE->qx,SPRITE->qy,sprite->z);
  if (!map) return 0;
  int col=SPRITE->qx%NS_sys_mapw;
  int row=SPRITE->qy%NS_sys_maph;
  uint8_t physics=map->physics[map->v[row*NS_sys_mapw+col]];
  switch (physics) {
    case NS_physics_vacant:
    case NS_physics_safe:
      break;
    default: return 0;
  }
  
  // Check whether there's already a hole there.
  double x=SPRITE->qx+0.5;
  double y=SPRITE->qy+0.5;
  struct sprite **otherp=GRP(visible)->sprv;
  int i=GRP(visible)->sprc;
  for (;i-->0;otherp++) {
    struct sprite *other=*otherp;
    if (other->defunct) continue;
    if (other->rid!=RID_sprite_hole) continue;
    if (other->x<x-0.1) continue;
    if (other->x>x+0.1) continue;
    if (other->y<y-0.1) continue;
    if (other->y>y+0.1) continue;
    return 0; // Already dug.
  }
  
  // Do it.
  struct sprite *hole=sprite_spawn(x,y,RID_sprite_hole,0,0,0,0,0);
  if (!hole) return 0;
  //TODO digging animation
  
  // Scan map's commands for anything buried here.
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_buriedtreasure: {
          if (cmd.arg[0]!=col) continue;
          if (cmd.arg[1]!=row) continue;
          int fld=(cmd.arg[2]<<8)|cmd.arg[3];
          if (store_get_fld(fld)) continue;
          store_set_fld(fld,1);
          int itemid=(cmd.arg[4]<<8)|cmd.arg[5];
          int quantity=cmd.arg[6];
          game_get_item(itemid,quantity);
          return 1;
        }
    }
  }
  
  // Nothing buried here, so just make the dig sound and we're done.
  bm_sound(RID_sound_dig);
  return 1;
}

/* Magnifier. Updates passively.
 * Copies most of Divining Rod's logic, and borrows some of the same plumbing.
 */
 
static void magnifier_update(struct sprite *sprite,double elapsed) {
  int qx=(int)sprite->x;
  int qy=(int)sprite->y;
  if ((qx==SPRITE->qx)&&(qy==SPRITE->qy)) return;
  SPRITE->qx=qx;
  SPRITE->qy=qy;
}

static double nearest_secret_distance2(const struct secret *v,int c,double x,double y) {
  double bestd2=999.999;
  for (;c-->0;v++) {
    double dx=v->x-x;
    double dy=v->y-y;
    double d2=dx*dx+dy*dy;
    if (d2<bestd2) bestd2=d2;
  }
  return bestd2;
}
 
static int magnifier_begin(struct sprite *sprite) {
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
  
  /* Ask map store to list nearby secrets.
   * If there's none, we can wrap up.
   */
  #define SECRETS_LIMIT 8
  struct secret secretv[SECRETS_LIMIT];
  int secretc=game_find_secrets(secretv,SECRETS_LIMIT,sprite->x,sprite->y,sprite->z,8.0);
  #undef SECRETS_LIMIT
  if (secretc<1) {
    bm_sound(RID_sound_negatory);
    return 1;
  }
  
  /* For each cell in (divining_alertv), find the distance to the nearest secret,
   * and that determines the tile we'll display.
   * If we show anything other than the default 0x68, we'll play the affirmative sound.
   */
  for (alert=SPRITE->divining_alertv,dy=-1;dy<=1;dy++) {
    int dx=-1; for (;dx<=1;dx++,alert++) {
      double alertx=SPRITE->qx+dx+0.5;
      double alerty=SPRITE->qy+dy+0.5;
      double distance=nearest_secret_distance2(secretv,secretc,alertx,alerty);
           if (distance<0.5*0.5) { soundid=RID_sound_affirmative; alert->tileid=0x01; }
      else if (distance<1.5*1.5) { soundid=RID_sound_affirmative; alert->tileid=0x02; }
      else if (distance<2.5*2.5) { soundid=RID_sound_affirmative; alert->tileid=0x03; }
      else if (distance<3.5*3.5) { soundid=RID_sound_affirmative; alert->tileid=0x04; }
      else if (distance<4.5*4.5) { soundid=RID_sound_affirmative; alert->tileid=0x05; }
      else if (distance<5.5*5.5) { soundid=RID_sound_affirmative; alert->tileid=0x06; }
      else if (distance<6.5*6.5) { soundid=RID_sound_affirmative; alert->tileid=0x07; }
    }
  }
  bm_sound(soundid);
  return 1;
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
      case NS_itemid_telescope: telescope_update(sprite,elapsed); break;
      default: fprintf(stderr,"%s:%d:ERROR: Item %d is in progress but has no update handler.\n",__FILE__,__LINE__,SPRITE->itemid_in_progress);
    }
    return;
  }
  
  /* Some items get updated passively just by virtue of being equipped.
   */
  switch (g.store.invstorev[0].itemid) {
    case NS_itemid_divining: divining_update(sprite,elapsed); break;
    case NS_itemid_compass: compass_update(sprite,elapsed); break;
    case NS_itemid_shovel: shovel_update(sprite,elapsed); break;
    case NS_itemid_magnifier: magnifier_update(sprite,elapsed); break;
  }
  
  // No starting items if injured.
  if (SPRITE->hurt>0.0) return;
  
  /* Poll input.
   * Call out when SOUTH is pressed. Handler may set itemid_in_progress.
   */
  if (SPRITE->item_blackout) {
    if (g.input[0]&EGG_BTN_SOUTH) return;
    SPRITE->item_blackout=0;
  }
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    SPRITE->blocked=0;
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
      case NS_itemid_barrelhat: result=barrelhat_begin(sprite); break;
      case NS_itemid_telescope: result=telescope_begin(sprite); break;
      case NS_itemid_shovel: result=shovel_begin(sprite); break;
      case NS_itemid_magnifier: result=magnifier_begin(sprite); break;
    }
    if (!result) {
      bm_sound(RID_sound_reject);
    } else {
      cryptmsg_notify_item(g.store.invstorev[0].itemid);
    }
  }
}

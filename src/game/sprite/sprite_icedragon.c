#include "game/bellacopia.h"

#define STATE_EMPTY    1 /* Ice cube. Dragon hasn't come here yet. */
#define STATE_OCCUPIED 2 /* Ice cube with a dragon on top. Ready for combat. */
#define STATE_MELTED   3 /* Just a puddle. */

static const int icedragon_schedule[]={
  NS_battle_skijump,
  NS_battle_curling,
  NS_battle_figureskating,
  NS_battle_bobsleigh,
  NS_battle_snowboard,
  NS_battle_hockey,
};

struct sprite_icedragon {
  struct sprite hdr;
  int seq; // 1..6
  int state;
  int left; // We don't transform, usually. But there are separate tiles in OCCUPIED stage for facing left and right.
  int store_listener;
};

#define SPRITE ((struct sprite_icedragon*)sprite)

/* Cleanup.
 */
 
static void _icedragon_del(struct sprite *sprite) {
  store_unlisten(SPRITE->store_listener);
}

/* State changes, driven by our (seq) and global (iceseq).
 */

static void icedragon_update_state(struct sprite *sprite,int iceseq) {
  int nstate=0;
  if ((SPRITE->seq<1)||(SPRITE->seq>6)) {
    nstate=STATE_MELTED;
  } else {
    int iceseq=store_get_fld16(NS_fld16_iceseq);
    if ((SPRITE->seq==1)&&(iceseq>=6)) { // First instance can replay after you've beaten them all.
      nstate=STATE_OCCUPIED;
    } else if (SPRITE->seq<=iceseq) { // Got me already.
      nstate=STATE_MELTED;
    } else if (SPRITE->seq==iceseq+1) { // I'm next.
      nstate=STATE_OCCUPIED;
    } else { // Ask again later.
      nstate=STATE_EMPTY;
    }
  }
  if (nstate==SPRITE->state) return;
  
  // Exit state.
  switch (SPRITE->state) {
    case STATE_MELTED: { // We don't normally exit MELTED state, but let's be consistent.
        sprite->layer=100;
        sprite_group_add(GRP(solid),sprite);
        sprite_group_add(GRP(moveable),sprite);
      } break;
  }
  
  SPRITE->state=nstate;
  
  // Enter state.
  switch (SPRITE->state) {
    case STATE_MELTED: {
        SPRITE->state=STATE_MELTED;
        sprite->layer=20;
        sprite_group_remove(GRP(solid),sprite);
        sprite_group_remove(GRP(moveable),sprite);
      } break;
  }
}

/* Store listener.
 */
 
static void icedragon_cb_store(char type,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if ((type=='6')&&(id==NS_fld16_iceseq)) {
    icedragon_update_state(sprite,value);
  }
}

/* Init.
 */
 
static int _icedragon_init(struct sprite *sprite) {
  SPRITE->seq=sprite->arg[0];
  SPRITE->store_listener=store_listen('6',icedragon_cb_store,sprite);
  icedragon_update_state(sprite,store_get_fld16(NS_fld16_iceseq));
  return 0;
}

/* Update.
 */
 
static void _icedragon_update(struct sprite *sprite,double elapsed) {
  if (GRP(hero)->sprc>=1) {
    struct sprite *hero=GRP(hero)->sprv[0];
    double dx=hero->x-sprite->x;
    if (dx<=-0.750) SPRITE->left=1;
    else if (dx>=0.750) SPRITE->left=0;
  }
}

/* Dot has defeated us in combat.
 */
 
static void icedragon_dot_wins(struct sprite *sprite) {

  int iceseq=store_get_fld16(NS_fld16_iceseq);
  if (iceseq>=6) {
    // Replay. Don't change any state or create the fly-away decoration.
    return;
  }
  store_set_fld16(NS_fld16_iceseq,SPRITE->seq);
  // Our state change will happen automatically due to that fld16 change.
  
  if (SPRITE->seq==6) {
    // I was the last Ice Dragon. Should there be some special fanfare?
  } else {
    // Show a decorative me, flying toward the next instance.
    struct sprite *inter=sprite_spawn(sprite->x,sprite->y,0,0,0,&sprite_type_icedragon_inter,0,0);
  }
}

/* Battle callback.
 */
 
static void icedragon_cb_battle(struct modal *modal,int outcome,void *userdata) {
  struct sprite *sprite=userdata;
  if (outcome>0) {
    /* If we're in replay mode, earn a coin or whatever like usual.
     * The main-sequence victories are their own reward, nothing else.
     */
    int iceseq=store_get_fld16(NS_fld16_iceseq);
    if (iceseq>=6) {
      struct prize prizev[8];
      int prizec=game_get_prizes(prizev,8,modal_battle_get_battle(modal)->type->id,sprite->arg);
      struct battle *battle=modal_battle_get_battle(modal);
      if (battle&&battle->type->get_prizes) {
        prizec+=battle->type->get_prizes(prizev+prizec,8-prizec,battle);
      }
      struct prize *prize=prizev;
      for (;prizec-->0;prize++) {
        game_get_item(prize->itemid,prize->quantity);
        modal_battle_add_consequence(modal,prize->itemid,prize->quantity);
      }
    }
    icedragon_dot_wins(sprite);
  } else if (outcome<0) {
    modal_battle_add_consequence(modal,NS_itemid_heart,-1);
  }
}

static void icedragon_cb_final(struct modal *modal,int outcome,void *userdata) {
  if (outcome<0) {
    game_hurt_hero();
  }
}

/* Begin battle.
 */
 
static void icedragon_begin_battle(struct sprite *sprite,int battleid) {
  if (!battleid) return;
  const struct battle_type *type=battle_type_by_id(battleid);
  if (!type) return;
  
  /* Enter battle.
   */
  struct modal_args_battle args={
    .battle=battleid,
    .args={
      .difficulty=0x80,
      .bias=bm_battle_bias(battleid),
      .lctl=1,
      .lface=NS_face_dot,
      .rctl=0,
      .rface=NS_face_monster,
    },
    .userdata=sprite,
    .right_name=226,
    .cb=icedragon_cb_battle,
    .cb_final=icedragon_cb_final,
  };
  struct modal *modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
  if (!modal) return;
}

/* Replay proposal modal.
 */
 
static int icedragon_cb_replay(int optionid,void *userdata) {
  if (optionid==234) return 0;
  // Otherwise, optionid should be a battleid.
  icedragon_begin_battle(userdata,optionid);
  return 0;
}
 
static void icedragon_propose_replay(struct sprite *sprite) {
  struct modal_args_dialogue args={
    .rid=RID_strings_battle,
    .strix=233,
    .speaker=sprite,
    .cb=icedragon_cb_replay,
    .userdata=sprite,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_battle,234);
  int i=0;
  int c=sizeof(icedragon_schedule)/sizeof(icedragon_schedule[0]);
  for (;i<c;i++) {
    int battleid=icedragon_schedule[i];
    const struct battle_type *type=battle_type_by_id(battleid);
    if (!type) continue;
    modal_dialogue_add_option_string_id(modal,RID_strings_battle,type->strix_name,battleid);
  }
}

/* Collide.
 */
 
static void _icedragon_collide(struct sprite *sprite,struct sprite *other) {
  if (SPRITE->state!=STATE_OCCUPIED) return;
  
  /* Schedule of battles, or call out for the replay modal.
   */
  int battleid=0;
  int iceseq=store_get_fld16(NS_fld16_iceseq);
  if (iceseq>=6) {
    icedragon_propose_replay(sprite);
    return;
  }
  int optionp=SPRITE->seq-1;
  int optionc=sizeof(icedragon_schedule)/sizeof(icedragon_schedule[0]);
  if ((optionp<0)||(optionp>=optionc)) return;
  icedragon_begin_battle(sprite,icedragon_schedule[optionp]);
}

/* Render.
 */
 
static void _icedragon_render(struct sprite *sprite,int x,int y) {
  graf_set_image(&g.graf,sprite->imageid);
  switch (SPRITE->state) {
    case STATE_EMPTY: {
        graf_tile(&g.graf,x,y,sprite->tileid,sprite->xform);
      } break;
    case STATE_OCCUPIED: if (SPRITE->left) {
        graf_tile(&g.graf,x,y,sprite->tileid+0x02,sprite->xform);
        graf_tile(&g.graf,x,y-NS_sys_tilesize,sprite->tileid+0xf2,sprite->xform);
      } else {
        graf_tile(&g.graf,x,y,sprite->tileid+0x01,sprite->xform);
        graf_tile(&g.graf,x,y-NS_sys_tilesize,sprite->tileid+0xf1,sprite->xform);
      } break;
    case STATE_MELTED: {
        graf_tile(&g.graf,x,y,sprite->tileid-0x10,sprite->xform);
      } break;
  }
}

/* Type definition.
 */
 
const struct sprite_type sprite_type_icedragon={
  .name="icedragon",
  .objlen=sizeof(struct sprite_icedragon),
  .del=_icedragon_del,
  .init=_icedragon_init,
  .update=_icedragon_update,
  .collide=_icedragon_collide,
  .render=_icedragon_render,
};

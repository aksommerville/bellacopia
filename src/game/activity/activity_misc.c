#include "activity_internal.h"
#include "game/race/race.h"

/* Phonograph.
 */
 
static int cb_phonograph(int rid,void *userdata) {
  store_set_fld16(NS_fld16_phonograph,rid);
  if (!g.song_override_outerworld&&map_is_outerworld(g.camera.map)) {
    bm_song_gently(bm_song_for_outerworld());
  }
  return 1;
}
 
void begin_phonograph() {
  struct modal_args_dialogue args={
    .rid=RID_strings_item,
    .strix=85,
    .cb=cb_phonograph,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  struct song_name_and_rid songv[10];
  int songc=bm_get_available_songs(songv,10);
  if (songc<0) songc=0; else if (songc>10) songc=10;
  struct song_name_and_rid *song=songv;
  for (;songc-->0;song++) {
    modal_dialogue_add_option_string_id(modal,RID_strings_item,song->name,song->rid);
  }
  modal_dialogue_set_default(modal,g.song_playing);
}

/* Bus stop.
 * This is just when you look at a fixed bus stop, it doesn't do anything but talk.
 */
 
static const struct busstop_metadata {
  int busstop;
  int strix; // RID_strings_item
} busstop_metadatav[]={
  {NS_busstop_cheapside,     94},
  {NS_busstop_fractia,       95},
  {NS_busstop_temple,        96},
  {NS_busstop_castle,        97},
  {NS_busstop_magneticnorth, 98},
  {NS_busstop_botire,        99},
};
 
void begin_busstop(int busstop) {
  int strix=0;
  const struct busstop_metadata *metadata=busstop_metadatav;
  int i=sizeof(busstop_metadatav)/sizeof(struct busstop_metadata);
  for (;i-->0;metadata++) {
    if (metadata->busstop!=busstop) continue;
    strix=metadata->strix;
    break;
  }
  if (!strix) {
    fprintf(stderr,"%s:%d: No metadata for busstop %d.\n",__FILE__,__LINE__,busstop);
    return;
  }
  struct modal_args_dialogue args={
    .rid=RID_strings_item,
    .strix=strix,
  };
  modal_spawn(&modal_type_dialogue,&args,sizeof(args));
}

int busstop_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(busstop_metadatav)/sizeof(struct busstop_metadata);
  if (p>=c) return 0;
  return busstop_metadatav[p].busstop;
}

int busstop_name_by_index(int p) {
  if (p<0) return 0;
  int c=sizeof(busstop_metadatav)/sizeof(struct busstop_metadata);
  if (p>=c) return 0;
  return busstop_metadatav[p].strix;
}

/* Pause, while race running.
 * This isn't involved enough to deserve its own modal, like regular pause.
 */
 
static int pauserace_cb(int optionid,void *userdata) {
  if (optionid==141) {
    race_end();
  }
  return 0;
}
 
void begin_pauserace() {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=139,
    .cb=pauserace_cb,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,140);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,141);
}

/* "Reset puzzle". Creating for diegetic minesweeper, but intending to be usable generically.
 */
 
static int reset_puzzle_cb(int optionid,void *userdata) {
  if (optionid!=4) return 0;
  int fldid=(int)(uintptr_t)userdata;
  store_set_fld(fldid,0);
  store_broadcast('s',NS_signal_reset_puzzle,fldid);
  return 0;
}
 
void begin_reset_puzzle(struct sprite *initiator,int fldid) {
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=151,
    .cb=reset_puzzle_cb,
    .userdata=(void*)(uintptr_t)fldid,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
}

/* Generic battle-as-activity.
 * Works basically the same as sprite_monster.
 */
 
static void cb_activity_battle(struct modal *modal,int outcome,void *userdata) {
  struct battle *battle=modal_battle_get_battle(modal);
  if (!battle) return;
  if (outcome>0) {
    struct prize prizev[8];
    int prizec=game_get_prizes(prizev,8,battle->type->id,0);
    if (battle&&battle->type->get_prizes) {
      prizec+=battle->type->get_prizes(prizev+prizec,8-prizec,battle);
    }
    struct prize *prize=prizev;
    for (;prizec-->0;prize++) {
      game_get_item(prize->itemid,prize->quantity);
      modal_battle_add_consequence(modal,prize->itemid,prize->quantity);
    }
  } else if (outcome<0) {
    // Don't actually hurt the hero until cb_final. Report it first. If it's her last heart, game_hurt_hero would trigger the gameover modal.
    modal_battle_add_consequence(modal,NS_itemid_heart,-1);
  }
}
 
static void cb_activity_battle_final(struct modal *modal,int outcome,void *userdata) {
  if (outcome<0) {
    game_hurt_hero();
  }
}
 
void begin_battle(struct sprite *sprite,int battleid) {
  struct modal_args_battle args={
    .battle=battleid,
    .args={
      .difficulty=0x80,
      .bias=0x80,
      .lctl=1,
      .rctl=0,
      .lface=NS_face_dot,
      .rface=NS_face_monster,
      .imageid=0, // Ask the camera.
      .no_store=0,
    },
    .cb=cb_activity_battle,
    .cb_final=cb_activity_battle_final,
    .userdata=sprite,
    .left_name=0, // See lctl.
    .right_name=0, // Will fill in below if available.
    .skip_prompt=0,
    .nameless_prompt=0,
    .skip_outtro=0,
  };
  
  /* If a sprite was provided, scan it for a "monster" command, to get the proper name and fallback battleid.
   */
  if (sprite) {
    struct cmdlist_reader reader;
    if (sprite_reader_init(&reader,sprite->cmd,sprite->cmdc)>=0) {
      struct cmdlist_entry cmd;
      while (cmdlist_reader_next(&cmd,&reader)>0) {
        switch (cmd.opcode) {
          case CMD_sprite_monster: {
              if (!args.battle) { // Our caller overrides (battleid), but if zero we can take it from the sprite.
                args.battle=(cmd.arg[0]<<8)|cmd.arg[1];
              }
              args.right_name=(cmd.arg[4]<<8)|cmd.arg[5];
            } break;
        }
      }
    }
  }
  // If we still don't have a battleid, abort.
  if (!args.battle) return;
  
  struct modal *modal=modal_spawn(&modal_type_battle,&args,sizeof(args));
  if (!modal) return;
}

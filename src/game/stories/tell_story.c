#include "game/bellacopia.h"

/* If there's a tree near the hero, return its field id. Zero if none.
 */
 
static int stories_get_tree_field() {

  // Get the hero and the map she's on. Short range, so we'll only examine the one map.
  // Keep trees off the very edge.
  if (GRP(hero)->sprc<1) return 0;
  struct sprite *hero=GRP(hero)->sprv[0];
  struct map *map=map_by_sprite_position(hero->x,hero->y,hero->z);
  if (!map) return 0;
  int xa=(int)hero->x-map->lng*NS_sys_mapw-1;
  int ya=(int)hero->y-map->lat*NS_sys_maph-1;
  int xz=xa+2;
  int yz=ya+2;
  
  // Find a bump command with the tree activity. Its arg is the field.
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode!=CMD_map_bump) continue;
    if ((cmd.arg[0]<xa)||(cmd.arg[0]>xz)) continue;
    if ((cmd.arg[1]<ya)||(cmd.arg[1]>yz)) continue;
    int activity=(cmd.arg[2]<<8)|cmd.arg[3];
    if (activity!=NS_activity_tree) continue;
    return (cmd.arg[4]<<8)|cmd.arg[5];
  }
  return 0;
}

/* Tell story, main entry point.
 */
 
static void cb_story_reject(void *userdata) {
  modal_dialogue_simple(RID_strings_dialogue,93);
}

static void cb_story_accept(void *userdata) {
  modal_dialogue_simple(RID_strings_dialogue,92);
}

void game_tell_story(const struct story *story) {
  if (!story) return;
  int postaction=0; // What to do after the cutscene: -1,0,1=reject,nothing,accept
  int treefld=stories_get_tree_field();
  fprintf(stderr,"%s:%d:%s:TODO: Tell story (strix %d), tree?%d\n",__FILE__,__LINE__,__func__,story->strix_title,treefld);
  
  // If this tree is already satisfied, pretend it's not there.
  if (treefld&&store_get_fld(treefld)) treefld=0;
  
  // If there's no tree (or satisfied), we're just playing the cutscene. All good.
  if (!treefld) {
  
  // If the story has been told already, we'll do a gentle rejection after.
  } else if (store_get_fld(story->fld_told)) {
    postaction=-1;
    
  // Complete both tree and story, and remember to give some feedback after the cutscene.
  } else {
    postaction=1;
    store_set_fld(treefld,1);
    store_set_fld(story->fld_told,1);
    g.camera.mapsdirty=1;
  }
  
  // Prepare the cutscene's postaction, then kick it off.
  struct modal_args_cutscene args={
    .strix_title=story->strix_title,
    .cb=0,
    .userdata=0,
  };
  if (postaction<0) args.cb=cb_story_reject;
  else if (postaction>0) args.cb=cb_story_accept;
  struct modal *modal=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
  
  // If we don't have a postaction, that's all. (If the pause modal was open, it stays open).
  if (!postaction) return;
  
  // Find the pause modal and defunct it. No need to be graceful about this; it will silently die while the cutscene modal is occluding it.
  int i=g.modalc;
  while (i-->0) {
    struct modal *modal=g.modalv[i];
    if (modal->type==&modal_type_pause) modal->defunct=1;
  }
}

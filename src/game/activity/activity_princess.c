/* activity_princess.c
 * Activities related to rescuing the Princess and decrypting the gobins' puzzles.
 */
 
#include "activity_internal.h"

/* King: Behooves you to rescue the Princess, or recompenses you for doing so.
 */
 
static int king_cb_win(int optionid,void *userdata) {
  store_set_fld(NS_fld_rescued_princess,1);
  int goldmax=store_get_fld16(NS_fld16_goldmax);
  goldmax+=100;
  store_set_fld16(NS_fld16_goldmax,goldmax);
  store_set_fld16(NS_fld16_gold,goldmax); // Also fill er up, as long as we're in there.
  bm_sound(RID_sound_treasure);
  return 1;
}

// null and (*near) nonzero if the princess exists but out of range.
static struct sprite *find_princess(struct sprite *king,int *near) {
  struct sprite **spritep=GRP(monsterlike)->sprv;
  int i=GRP(monsterlike)->sprc;
  for (;i-->0;spritep++) {
    struct sprite *sprite=*spritep;
    if (sprite->defunct) continue;
    if (sprite->type!=&sprite_type_princess) continue;
    *near=1;
    double dx=sprite->x-king->x;
    double dy=sprite->y-king->y;
    double d2=dx*dx+dy*dy;
    double tolerance=5.0;
    tolerance*=tolerance;
    if (d2>tolerance) return 0;
    return sprite;
  }
  return 0;
}
 
void begin_king(struct sprite *sprite) {
  if (store_get_fld(NS_fld_rescued_princess)) {
    begin_dialogue(32,sprite); // "Thanks again!"
  } else {
    int near=0;
    struct sprite *princess=find_princess(sprite,&near);
    if (princess) {
      sprite_kill_soon(princess);
      struct modal *modal=begin_dialogue(20,sprite); // "Hey thanks!"
      if (!modal) return;
      modal_dialogue_set_callback(modal,king_cb_win,sprite);
    } else if (near) {
      begin_dialogue(42,sprite); // "Where is she?"
    } else {
      begin_dialogue(33,sprite); // "Oh no Princess is kidnapped!"
    }
  }
}

/* Jaildoor: Unlock if we have the key.
 */
 
void begin_jaildoor() {
  if (store_get_fld(NS_fld_jailopen)) return;
  if (store_get_fld(NS_fld_jailkey)) {
    store_set_fld(NS_fld_jailopen,1);
    g.camera.mapsdirty=1;
    bm_sound(RID_sound_presto);
  }
}

/* Kidnap: The big cutscene where Dot gets kidnapped by goblins.
 */
 
static int kidnap_cb(int optionid,void *userdata) {
  game_warp(RID_map_jail,NS_transition_fadeblack);
  return 0;
}
 
void begin_kidnap(struct sprite *sprite) {
  fprintf(stderr,"TODO %s:%d:%s\n",__FILE__,__LINE__,__func__);
  struct modal_args_dialogue args={
    .text="TODO: Cutscene of Dot falling into a trap, then the goblins drag her off.",
    .textc=-1,
    .cb=kidnap_cb,
    .userdata=sprite,
  };
  modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  store_set_fld(NS_fld_kidnapped,1);
}

/* Escape: Nothing noticeable to the user, but as you leave the cave we set this flag.
 * It's so we know whether to spawn you next in jail or at home.
 * A crafty user might try to get out of jail by quitting and restarting the game.
 */
 
void begin_escape() {
  store_set_fld(NS_fld_escaped,1);
}

/* Cryptmsg: One of the seven obelisks giving encrypted clues in the goblins' cave.
 */
 
void begin_cryptmsg(int which) {
  
  /* If we're in a dark room -- they all are -- require a nearby light source.
   * Camera figures out its lights on the fly and doesn't leave any useful residue for us.
   * Cryptmsg 3, north center of Skull Lake, is 103 m**2, and to my eye should be just barely outside.
   * Perfect! Let's call the threshold 100.
   */
  if (g.camera.darkness>0.0) {
    int lit=0;
    if (GRP(hero)->sprc>=1) {
      struct sprite *hero=GRP(hero)->sprv[0];
      struct sprite *nearlight=0;
      double neard2=999.999;
      struct sprite **qp=GRP(light)->sprv;
      int i=GRP(light)->sprc;
      for (;i-->0;qp++) {
        struct sprite *q=*qp;
        if (q->defunct) continue;
        double dx=q->x-hero->x;
        double dy=q->y-hero->y;
        double d2=dx*dx+dy*dy;
        if (d2<neard2) {
          nearlight=q;
          neard2=d2;
          if (d2<1.0) break;
        }
      }
      if (neard2<100.0) lit=1;
    }
    if (!lit) {
      begin_dialogue(45,0); // "Too dark"
      return;
    }
  }
  
  char msg[256];
  int msgc=cryptmsg_get(msg,sizeof(msg),which);
  if ((msgc<1)||(msgc>sizeof(msg))) return;
  
  struct modal_args_cryptmsg args={
    .src=msg,
    .srcc=msgc,
  };
  struct modal *modal=modal_spawn(&modal_type_cryptmsg,&args,sizeof(args));
}

/* Linguist: Offer help with translating Old Goblish, or show the things already purchased.
 * There's a special modal for the whole interaction; all we do is spawn it.
 */
 
void begin_linguist(struct sprite *sprite) {
  struct modal_args_linguist args={
    .speaker=sprite,
  };
  struct modal *modal=modal_spawn(&modal_type_linguist,&args,sizeof(args));
}

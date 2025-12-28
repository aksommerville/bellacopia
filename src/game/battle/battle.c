#include "game/game.h"

/* Begin battle, given resource id.
 */
 
int bm_begin_battle(
  int battletype,int playerc,int handicap,
  void (*cb)(void *userdata,int outcome),
  void *userdata
) {
  const struct battle_type *type=battle_type_by_id(battletype);
  if (!type) {
    fprintf(stderr,"battletype %d not found\n",battletype);
    return -1;
  }
  if ((playerc==1)&&(type->flags&BATTLE_FLAG_ONEPLAYER)) ;
  else if ((playerc==2)&&(type->flags&BATTLE_FLAG_TWOPLAYER)) ;
  else {
    fprintf(stderr,"battle '%s' does not support %d-player mode.\n",type->name,playerc);
    return -1;
  }
  struct modal *modal=modal_new_battle(type,playerc,handicap);
  if (!modal) return -1;
  modal_battle_set_callback(modal,cb,userdata);
  return 0;
}

/* Register request for deferred battle.
 */
 
void bm_begin_battle_soon(
  int battletype,int playerc,int handicap,
  void (*cb)(void *userdata,int outcome),
  void *userdata
) {
  g.deferred_battle.battletype=battletype;
  g.deferred_battle.playerc=playerc;
  g.deferred_battle.handicap=handicap;
  g.deferred_battle.cb=cb;
  g.deferred_battle.userdata=userdata;
}

/* Type by id.
 */
 
const struct battle_type *battle_type_by_id(int battletype) {
  switch (battletype) {
    #define _(tag) case NS_battletype_##tag: return &battle_type_##tag;
    FOR_EACH_BATTLETYPE
    #undef _
  }
  return 0;
}

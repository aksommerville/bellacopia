#include "game/bellacopia.h"

struct battle_boomerang {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
};

#define CTX ((struct battle_boomerang*)ctx)

/* Delete.
 */
 
static void _boomerang_del(void *ctx) {
  free(ctx);
}

/* New.
 */
 
static void *_boomerang_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_boomerang));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  //TODO
  return ctx;
}

/* Update.
 */
 
static void _boomerang_update(void *ctx,double elapsed) {
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    if (CTX->cb_end) CTX->cb_end(1,CTX->userdata);
    return;
  }
  //TODO
}

/* Render.
 */
 
static void _boomerang_render(void *ctx) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x804020ff);//TODO
}

/* Type definition.
 */
 
const struct battle_type battle_type_boomerang={
  .name="boomerang",
  .strix_name=11,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_cpu_cpu)|(1<<NS_players_cpu_man)|(1<<NS_players_man_cpu)|(1<<NS_players_man_man),
  .del=_boomerang_del,
  .init=_boomerang_init,
  .update=_boomerang_update,
  .render=_boomerang_render,
};

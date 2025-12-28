#include "game/game.h"

struct battle_fishing {
  int playerc;
  int handicap;
  void (*cb_finish)(void *userdata,int outcome);
  void *userdata;
};

#define CTX ((struct battle_fishing*)ctx)

/* Delete.
 */
 
static void _fishing_del(void *ctx) {
  free(ctx);
}

/* Init.
 */
 
static void *_fishing_init(
  int playerc,int handicap,
  void (*cb_finish)(void *userdata,int outcome),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_fishing));
  if (!ctx) return 0;
  CTX->playerc=playerc;
  CTX->handicap=handicap;
  CTX->cb_finish=cb_finish;
  CTX->userdata=userdata;
  return ctx;
}

/* Update.
 */
 
static void _fishing_update(void *ctx,double elapsed) {
  if (CTX->cb_finish) {
    if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
      fprintf(stderr,"%s: SOUTH: Let's say the hero wins.\n",__FILE__);
      CTX->cb_finish(CTX->userdata,1);
      CTX->cb_finish=0;
    } else if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
      fprintf(stderr,"%s: WEST: Let's say the foe wins.\n",__FILE__);
      CTX->cb_finish(CTX->userdata,-1);
      CTX->cb_finish=0;
    } else if ((g.input[0]&EGG_BTN_EAST)&&!(g.pvinput[0]&EGG_BTN_EAST)) {
      fprintf(stderr,"%s: EAST: Let's say it's a draw.\n",__FILE__);
      CTX->cb_finish(CTX->userdata,0);
      CTX->cb_finish=0;
    }
  }
}

/* Render.
 */
 
static void _fishing_render(void *ctx) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x0000ffff);
}

/* Type definition.
 */
 
const struct battle_type battle_type_fishing={
  .name="fishing",
  .battletype=NS_battletype_fishing,
  .flags=BATTLE_FLAG_ONEPLAYER|BATTLE_FLAG_TWOPLAYER,
  .foe_name_strix=50,
  .battle_name_strix=51,
  .del=_fishing_del,
  .init=_fishing_init,
  .update=_fishing_update,
  .render=_fishing_render,
};

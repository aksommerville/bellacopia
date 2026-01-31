/* battle_erudition.c
 */

#include "game/bellacopia.h"

struct battle_erudition {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  int choice;
};

#define CTX ((struct battle_erudition*)ctx)

/* Delete.
 */
 
static void _erudition_del(void *ctx) {
  free(ctx);
}

/* New.
 */
 
static void *_erudition_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_erudition));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  return ctx;
}

/* Update.
 */
 
static void _erudition_update(void *ctx,double elapsed) {
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) { if (--(CTX->choice)<-1) CTX->choice=1; }
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) { if (++(CTX->choice)>1) CTX->choice=-1; }
  if (CTX->cb_end&&(g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    bm_sound(RID_sound_uiactivate);
    CTX->cb_end(CTX->choice,CTX->userdata);
    CTX->cb_end=0;
  }
}

/* Render.
 */
 
static void _erudition_render(void *ctx) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  int y=FBH/3;
  int boxh=20;
  int boxw=20;
  int y1=y+boxh+1;
  int xv[3]={
    (FBW>>2)-(boxw>>1),
    (FBW>>1)-(boxw>>1),
    ((FBW*3)>>2)-(boxw>>1),
  };
  graf_fill_rect(&g.graf,xv[0],y,boxw,boxh,0xff0000ff);
  graf_fill_rect(&g.graf,xv[1],y,boxw,boxh,0x404040ff);
  graf_fill_rect(&g.graf,xv[2],y,boxw,boxh,0x00ff00ff);
  switch (CTX->choice) {
    case -1: graf_fill_rect(&g.graf,xv[0],y1,boxw,boxh,0xffffffff); break;
    case  0: graf_fill_rect(&g.graf,xv[1],y1,boxw,boxh,0xffffffff); break;
    case  1: graf_fill_rect(&g.graf,xv[2],y1,boxw,boxh,0xffffffff); break;
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_erudition={//TODO
  .name="erudition",
  .strix_name=55,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_cpu_cpu)|(1<<NS_players_cpu_man)|(1<<NS_players_man_cpu)|(1<<NS_players_man_man),
  .del=_erudition_del,
  .init=_erudition_init,
  .update=_erudition_update,
  .render=_erudition_render,
};

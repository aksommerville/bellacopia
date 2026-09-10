/* modal_pickside.c
 * Optional swapping of input [1] and [2], such that [1] is Left and [2] Right.
 */

#include "game/bellacopia.h"

/* Private globals.
 */
 
#define PICKSIDE_STATE_UNSET 0
#define PICKSIDE_STATE_NATURAL 1
#define PICKSIDE_STATE_SWAP 2
static int pickside_state=PICKSIDE_STATE_UNSET;

/* Modal instance.
 */
 
struct modal_pickside {
  struct modal hdr;
  int blackout;
  int prompttexid,promptw,prompth;
  struct player {
    int who; // 1,2
    int side; // 0,1,2 = center,left,right
    uint16_t state;
    int y;
    int confirmed;
  } playerv[2];
};

#define MODAL ((struct modal_pickside*)modal)

/* Cleanup.
 */
 
static void _pickside_del(struct modal *modal) {
  egg_texture_del(MODAL->prompttexid);
}

/* Rebuild prompt.
 */
 
static void pickside_rebuild_prompt(struct modal *modal) {
  const char *src;
  int srcc=text_get_string(&src,1,49);
  MODAL->prompttexid=font_render_to_texture(MODAL->prompttexid,g.font,src,srcc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&MODAL->promptw,&MODAL->prompth,MODAL->prompttexid);
}

/* Init.
 */
 
static int _pickside_init(struct modal *modal,const void *arg,int arglen) {
  modal->opaque=1;
  modal->interactive=1;
  modal->blotter=0;
  modal->stay_on_top=1;
  MODAL->blackout=0;
  
  MODAL->playerv[0].who=1;
  MODAL->playerv[0].side=0;
  MODAL->playerv[0].state=0;
  MODAL->playerv[0].y=30;
  
  MODAL->playerv[1].who=2;
  MODAL->playerv[1].side=0;
  MODAL->playerv[1].state=0;
  MODAL->playerv[1].y=100;
  
  pickside_rebuild_prompt(modal);
  return 0;
}

/* Move player.
 */
 
static void pickside_move(struct modal *modal,struct player *player,int d) {
  if (player->confirmed) {
    bm_sound_pan(RID_sound_reject,(player->side==2)?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  if (d<0) player->side=1;
  else player->side=2;
  bm_sound_pan(RID_sound_uimotion,(player->side==2)?PLAYER_PAN:-PLAYER_PAN);
}

/* Confirm for one player.
 */
 
static void pickside_confirm(struct modal *modal,struct player *player) {
  if (!player->side) {
    bm_sound(RID_sound_reject);
    return;
  }
  player->confirmed=1;
  struct player *l=MODAL->playerv;
  struct player *r=l+1;
  if (l->confirmed&&r->confirmed&&(l->side==r->side)) {
    // Don't play the happy activation when they choose the same side; we're not happy about it.
    bm_sound_pan(RID_sound_reject,(player->side==2)?PLAYER_PAN:-PLAYER_PAN);
    player->confirmed=0;
  } else {
    bm_sound_pan(RID_sound_uiactivate,(player->side==2)?PLAYER_PAN:-PLAYER_PAN);
  }
}

/* Rescind confirmation for one player.
 */
 
static void pickside_cancel(struct modal *modal,struct player *player) {
  if (!player->side||!player->confirmed) return;
  player->confirmed=0;
  bm_sound_pan(RID_sound_uicancel,(player->side==2)?PLAYER_PAN:-PLAYER_PAN);
}

/* Update.
 */
 
static void _pickside_update(struct modal *modal,double elapsed) {
  if (MODAL->blackout) {
    const uint16_t buttons=
      EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN|
      EGG_BTN_SOUTH|EGG_BTN_WEST;
    if (!(g.input[0]&buttons)) MODAL->blackout=0;
  } else {
    struct player *player=MODAL->playerv;
    int i=2;
    for (;i-->0;player++) {
      player->state=g.input[player->who];
      if ((g.input[player->who]&EGG_BTN_LEFT)&&!(g.pvinput[player->who]&EGG_BTN_LEFT)) pickside_move(modal,player,-1);
      else if ((g.input[player->who]&EGG_BTN_RIGHT)&&!(g.pvinput[player->who]&EGG_BTN_RIGHT)) pickside_move(modal,player,1);
      else if ((g.input[player->who]&EGG_BTN_SOUTH)&&!(g.pvinput[player->who]&EGG_BTN_SOUTH)) pickside_confirm(modal,player);
      else if ((g.input[player->who]&EGG_BTN_WEST)&&!(g.pvinput[player->who]&EGG_BTN_WEST)) pickside_cancel(modal,player);
    }
  }
  struct player *l=MODAL->playerv;
  struct player *r=l+1;
  if (l->confirmed&&r->confirmed&&l->side&&r->side&&(l->side!=r->side)) {
    if ((l->side==1)&&(r->side==2)) {
      pickside_state=PICKSIDE_STATE_NATURAL;
      modal->defunct=1;
    } else if ((l->side==2)&&(r->side==1)) {
      pickside_state=PICKSIDE_STATE_SWAP;
      modal->defunct=1;
    }
  }
}

/* Notify.
 */
 
static void _pickside_notify(struct modal *modal,int k,int v) {
  switch (k) {
    case EGG_PREF_LANG: pickside_rebuild_prompt(modal); break;
  }
}

/* Render a player.
 */
 
static void pickside_render_player(struct modal *modal,struct player *player) {
  const int inputw=NS_sys_tilesize*7;
  const int inputh=NS_sys_tilesize*4;
  int inputdstx;
  int inputdsty=player->y;
  switch (player->side) {
    case 0: inputdstx=FBW>>1; break;
    case 1: inputdstx=FBW/3; break;
    case 2: inputdstx=(FBW*2)/3; break;
    default: return;
  }
  inputdstx-=inputw>>1;
  if (player->confirmed) graf_set_alpha(&g.graf,0x40);
  graf_decal(&g.graf,inputdstx,inputdsty,0,0,NS_sys_tilesize*7,NS_sys_tilesize*4);
  
  if (player->state&&!player->confirmed) {
    const int left_buttons[]={EGG_BTN_LEFT,EGG_BTN_RIGHT,EGG_BTN_UP,EGG_BTN_DOWN,EGG_BTN_L1};
    const int right_buttons[]={EGG_BTN_SOUTH,EGG_BTN_WEST,EGG_BTN_EAST,EGG_BTN_NORTH,EGG_BTN_R1};
    int inputsrcy=NS_sys_tilesize*4;
    int inputsrcx,btnc,i;
    for (inputsrcx=0,btnc=sizeof(left_buttons)/sizeof(int),i=0;i<btnc;i++,inputsrcx+=NS_sys_tilesize*3) {
      if (player->state&left_buttons[i]) graf_decal(&g.graf,inputdstx,inputdsty,inputsrcx,inputsrcy,NS_sys_tilesize*3,NS_sys_tilesize*4);
    }
    inputsrcy+=NS_sys_tilesize*4;
    for (inputsrcx=0,btnc=sizeof(right_buttons)/sizeof(int),i=0;i<btnc;i++,inputsrcx+=NS_sys_tilesize*3) {
      if (player->state&right_buttons[i]) graf_decal(&g.graf,inputdstx+NS_sys_tilesize*4,inputdsty,inputsrcx,inputsrcy,NS_sys_tilesize*3,NS_sys_tilesize*4);
    }
  }
    
  if (player->confirmed) graf_set_alpha(&g.graf,0xff);
}

/* Render.
 */
 
static void _pickside_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  graf_set_input(&g.graf,MODAL->prompttexid);
  graf_decal(&g.graf,(FBW>>1)-(MODAL->promptw>>1),10,0,0,MODAL->promptw,MODAL->prompth);
  graf_set_image(&g.graf,RID_image_input);
  pickside_render_player(modal,MODAL->playerv+0);
  pickside_render_player(modal,MODAL->playerv+1);
}

/* Type definition.
 */
 
const struct modal_type modal_type_pickside={
  .name="pickside",
  .objlen=sizeof(struct modal_pickside),
  .del=_pickside_del,
  .init=_pickside_init,
  .update=_pickside_update,
  .notify=_pickside_notify,
  .render=_pickside_render,
};

/* Global public functions.
 */
 
int modal_pickside_require() {
  if (pickside_state!=PICKSIDE_STATE_UNSET) return 0;
  struct modal *modal=modal_spawn(&modal_type_pickside,0,0);
  if (!modal) return 0;
  return 1;
}

void modal_pickside_apply(int *inputv/*3*/) {
  if (pickside_state==PICKSIDE_STATE_SWAP) {
    int tmp=inputv[1];
    inputv[1]=inputv[2];
    inputv[2]=tmp;
  }
}

/* modal_raceconfig.c
 * A menu that configures Broom Race Mode.
 */
 
#include "game/bellacopia.h"
#include "game/race/race.h"

#define RACE_LIMIT 8

struct modal_raceconfig {
  struct modal hdr;
  int playerc;
  struct raceopt {
    int raceid;
    int strix_title;
    int texid;
    int x,y,w,h;
  } raceoptv[RACE_LIMIT];
  int raceoptc;
  int raceoptp;
};

#define MODAL ((struct modal_raceconfig*)modal)

/* Delete.
 */
 
static void _raceconfig_del(struct modal *modal) {
  while (MODAL->raceoptc>0) {
    MODAL->raceoptc--;
    egg_texture_del(MODAL->raceoptv[MODAL->raceoptc].texid);
  }
}

/* Init.
 */
 
static int _raceconfig_init(struct modal *modal,const void *args,int argslen) {
  modal->opaque=1;
  modal->interactive=1;
  MODAL->playerc=1;
  MODAL->raceoptp=0;
  
  /* Races must be ID'd consecutively from 1.
   * Race rid matches strix in strings:arcade.
   */
  int y=80;
  int raceid=1;
  for (;MODAL->raceoptc<RACE_LIMIT;raceid++) {
    if (!race_fld_by_id(raceid)) break;
    struct raceopt *raceopt=MODAL->raceoptv+MODAL->raceoptc++;
    raceopt->raceid=raceid;
    raceopt->strix_title=raceid;
    const char *src;
    int srcc=text_get_string(&src,RID_strings_arcade,raceid);
    raceopt->texid=font_render_to_texture(0,g.font,src,srcc,FBW,FBH,0xffffffff);
    egg_texture_get_size(&raceopt->w,&raceopt->h,raceopt->texid);
    raceopt->x=120;
    raceopt->y=y;
    y+=raceopt->h+1;
  }
  
  return 0;
}

/* Dismiss.
 */
 
static void raceconfig_dismiss(struct modal *modal) {
  bm_sound(RID_sound_uicancel);
  modal->defunct=1;
}

/* Activate.
 */
 
static void raceconfig_activate(struct modal *modal) {
  fprintf(stderr,"%d %s\n",(int)egg_time_real(),__func__);
  struct modal_args_broomrace args={
    //.playerc=1,//XXX
    //.playerc=2,
    //.raceid=RID_race_round_the_meadow,
    //.raceid=RID_race_downstairs_lake,
    //.raceid=RID_race_across_the_tundra,
    //.raceid=RID_race_seaside_circle,
    //.raceid=RID_race_desert_run,
    //.raceid=RID_race_undernorth,
    .playerc=MODAL->playerc,
    .raceid=MODAL->raceoptv[MODAL->raceoptp].raceid,
  };
  struct modal *race=modal_spawn(&modal_type_broomrace,&args,sizeof(args));
  if (!race) {
    bm_sound(RID_sound_reject);
    return;
  }
}

/* Change player count.
 */
 
static void raceconfig_move_playerc(struct modal *modal,int d) {
  int nc=MODAL->playerc+d;
  if ((nc<1)||(nc>2)) return;
  MODAL->playerc=nc;
  bm_sound(RID_sound_uimotion);
}

/* Change race.
 */
 
static void raceconfig_move_raceid(struct modal *modal,int d) {
  int np=MODAL->raceoptp+d;
  if ((np<0)||(np>=MODAL->raceoptc)) return;
  MODAL->raceoptp=np;
  bm_sound(RID_sound_uimotion);
}

/* Update.
 */
 
static void _raceconfig_update(struct modal *modal,double elapsed) {
  if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) raceconfig_move_playerc(modal,-1);
  if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) raceconfig_move_playerc(modal,1);
  if ((g.input[0]&EGG_BTN_UP)&&!(g.pvinput[0]&EGG_BTN_UP)) raceconfig_move_raceid(modal,-1);
  if ((g.input[0]&EGG_BTN_DOWN)&&!(g.pvinput[0]&EGG_BTN_DOWN)) raceconfig_move_raceid(modal,1);
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) raceconfig_dismiss(modal);
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) raceconfig_activate(modal);
}

/* Focus.
 */
 
static void _raceconfig_focus(struct modal *modal,int focus) {
  if (focus) {
    bm_song_force(RID_song_break_soil);
  }
}

/* Render.
 */
 
static void _raceconfig_render(struct modal *modal) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x185e27ff);
  graf_set_image(&g.graf,RID_image_arcade);
  
  /* Top row indicates (playerc) with big decals.
   */
  int y=10;
  graf_decal(&g.graf,(FBW>>1)-64,y,(MODAL->playerc==1)?0:64,128,64,64);
  graf_decal(&g.graf,FBW>>1,y,(MODAL->playerc==2)?0:64,192,64,64);
  
  /* Text labels for (raceid).
   */
  struct raceopt *raceopt=MODAL->raceoptv;
  int i=0;
  for (;i<MODAL->raceoptc;i++,raceopt++) {
    if (i==MODAL->raceoptp) {
      graf_fill_rect(&g.graf,raceopt->x-1,raceopt->y-1,raceopt->w+2,raceopt->h+1,0x102040ff);
    }
    graf_set_input(&g.graf,raceopt->texid);
    graf_decal(&g.graf,raceopt->x,raceopt->y,0,0,raceopt->w,raceopt->h);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_raceconfig={
  .name="raceconfig",
  .objlen=sizeof(struct modal_raceconfig),
  .del=_raceconfig_del,
  .init=_raceconfig_init,
  .update=_raceconfig_update,
  .focus=_raceconfig_focus,
  .render=_raceconfig_render,
};

#include "game/bellacopia.h"
#include "vellum.h"

#define GLYPHSIZE 8
#define COLC 26 /* 36 fits neatly. 26 should be the most we'll need, in English. */
#define ROWC 7 /* TODO 18 fits neatly. Add things to the report until it fills the page. */
//TODO Effect the horizontal centering dynamically. Once we allow multiple languages, we can't know the width limit statically.

struct vellum_stats {
  struct vellum hdr;
  char text[COLC*ROWC];
};

#define VELLUM ((struct vellum_stats*)vellum)

/* Delete.
 */
 
static void _stats_del(struct vellum *vellum) {
}

/* Focus.
 */
 
static void _stats_focus(struct vellum *vellum,int focus) {
}

/* Rewrite bits of the report into (VELLUM->text).
 * The individual bits take a buffer COLC long and overwrite the whole thing.
 */
  
static void stats_write_progress(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  int pct=0;//TODO Total completion 0..100.
  struct text_insertion insv[]={
    {.mode='i',.i=pct},
  };
  insv[0].mode='s'; insv[0].s.v="TODO"; insv[0].s.c=4;//XXX
  text_format_res(dst,COLC,1,17,insv,sizeof(insv)/sizeof(insv[0]));
}

static void stats_write_time(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  double fs=store_get_clock(NS_clock_playtime)+store_get_clock(NS_clock_battletime)+store_get_clock(NS_clock_pausetime);
  int ms=(int)(fs*1000.0);
  int seconds=ms/1000; ms%=1000;
  int minutes=seconds/60; seconds%=60;
  int hours=minutes/60; minutes%=60;
  if (hours>999) {
    hours=ms=999;
    minutes=seconds=99;
  }
  char tmp[]={
    '0'+hours/100,
    '0'+(hours/10)%10,
    '0'+hours%10,
    ':',
    '0'+minutes/10,
    '0'+minutes%10,
    ':',
    '0'+seconds/10,
    '0'+seconds%10,
    '.',
    '0'+ms/100,
    '0'+(ms/10)%10,
    '0'+ms%10,
  };
  // Trim zeroes off the front down to minute ones digit.
  int lopc;
  if (hours>=100) lopc=0;
  else if (hours>=10) lopc=1;
  else if (hours>=1) lopc=2;
  else if (minutes>=10) lopc=4;
  else lopc=5;
  struct text_insertion insv[]={
    {.mode='s',.s={.v=tmp+lopc,.c=sizeof(tmp)-lopc}},
  };
  text_format_res(dst,COLC,1,18,insv,sizeof(insv)/sizeof(insv[0]));
}

static void stats_write_flowers(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  int flowerc=0;
  if (store_get_fld(NS_fld_root1)) flowerc++;
  if (store_get_fld(NS_fld_root2)) flowerc++;
  if (store_get_fld(NS_fld_root3)) flowerc++;
  if (store_get_fld(NS_fld_root4)) flowerc++;
  if (store_get_fld(NS_fld_root5)) flowerc++;
  if (store_get_fld(NS_fld_root6)) flowerc++;
  if (store_get_fld(NS_fld_root7)) flowerc++;
  struct text_insertion insv[]={
    {.mode='i',.i=flowerc},
  };
  text_format_res(dst,COLC,1,19,insv,sizeof(insv)/sizeof(insv[0]));
}

static void stats_write_sidequests(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  int completec=0;//TODO
  int totalc=0;//TODO
  struct text_insertion insv[]={
    {.mode='i',.i=completec},
    {.mode='i',.i=totalc},
  };
  insv[0].mode='s'; insv[0].s.v="TODO"; insv[0].s.c=4;//XXX
  text_format_res(dst,COLC,1,20,insv,sizeof(insv)/sizeof(insv[0]));
}

static void stats_write_items(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  int itemc=0;
  const struct invstore *invstore=g.store.invstorev;
  int i=INVSTORE_SIZE;
  for (;i-->0;invstore++) {
    if (invstore->itemid) itemc++;
  }
  struct text_insertion insv[]={
    {.mode='i',.i=itemc},
    {.mode='i',.i=INVSTORE_SIZE},
  };
  text_format_res(dst,COLC,1,21,insv,sizeof(insv)/sizeof(insv[0]));
}

static void stats_write_maps(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  // I assume we'll be counting entire puzzles, not pieces. (completec) I guess should be when you both have every piece, and the lot are assembled.
  // This is surprisingly difficult to know.
  int completec=0;//TODO
  int totalc=0;//TODO
  struct text_insertion insv[]={
    {.mode='i',.i=completec},
    {.mode='i',.i=totalc},
  };
  insv[0].mode='s'; insv[0].s.v="TODO"; insv[0].s.c=4;//XXX
  text_format_res(dst,COLC,1,22,insv,sizeof(insv)/sizeof(insv[0]));
}
  
static void stats_rewrite_all(struct vellum *vellum) {
  memset(VELLUM->text,' ',sizeof(VELLUM->text));
  stats_write_progress  (vellum,VELLUM->text+ 0*COLC);
  stats_write_time      (vellum,VELLUM->text+ 1*COLC);
  stats_write_flowers   (vellum,VELLUM->text+ 2*COLC);
  stats_write_sidequests(vellum,VELLUM->text+ 3*COLC);
  stats_write_items     (vellum,VELLUM->text+ 4*COLC);
  stats_write_maps      (vellum,VELLUM->text+ 5*COLC);
}

/* Update animation only.
 */
 
static void _stats_updatebg(struct vellum *vellum,double elapsed) {
}

/* Update.
 */
 
static void _stats_update(struct vellum *vellum,double elapsed) {
  _stats_updatebg(vellum,elapsed);
  // pausetime keeps ticking while we're open. Update it constantly.
  stats_write_time(vellum,VELLUM->text+1*COLC);
}

/* Language change.
 */
 
static void _stats_langchanged(struct vellum *vellum,int lang) {
  stats_rewrite_all(vellum);
}

/* Render.
 */
 
static void _stats_render(struct vellum *vellum,int x,int y,int w,int h) {
  graf_set_image(&g.graf,RID_image_fonttiles);
  graf_set_tint(&g.graf,0x000000ff);
  const char *src=VELLUM->text;
  int fullw=COLC*GLYPHSIZE;
  int fullh=ROWC*GLYPHSIZE;
  int tx0=x+(w>>1)-(fullw>>1)+(GLYPHSIZE>>1);
  int ty=y+(h>>1)-(fullh>>1)+(GLYPHSIZE>>1),yi=ROWC;
  for (;yi-->0;ty+=GLYPHSIZE) {
    int tx=tx0,xi=COLC;
    for (;xi-->0;tx+=GLYPHSIZE,src++) {
      if ((*src<=0x20)||(*src>=0x7f)) continue;
      graf_tile(&g.graf,tx,ty,*src,0);
    }
  }
  graf_set_tint(&g.graf,0);
}

/* New.
 */
 
struct vellum *vellum_new_stats(struct modal *parent) {
  struct vellum *vellum=calloc(1,sizeof(struct vellum_stats));
  if (!vellum) return 0;
  vellum->parent=parent;
  vellum->lblstrix=15;
  vellum->del=_stats_del;
  vellum->focus=_stats_focus;
  vellum->updatebg=_stats_updatebg;
  vellum->update=_stats_update;
  vellum->render=_stats_render;
  vellum->langchanged=_stats_langchanged;
  
  stats_rewrite_all(vellum);
  
  return vellum;
}

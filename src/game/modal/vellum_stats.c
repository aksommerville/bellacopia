#include "game/bellacopia.h"
#include "vellum.h"

#define GLYPHSIZE 8
#define COLC 36 /* 36 fits neatly. 26 should be the most we'll need, in English. */
#define ROWC 6 /* TODO 18 fits neatly. Add things to the report until it fills the page. */

struct vellum_stats {
  struct vellum hdr;
  char text[COLC*ROWC];
  int colonp;
};

#define VELLUM ((struct vellum_stats*)vellum)

/* Delete.
 */
 
static void _stats_del(struct vellum *vellum) {
}

/* Take measurements for per-line alignment.
 */
 
static void stats_measure_alignment(struct vellum *vellum) {
  // Read all the format strings and find the longest key (measuring to colon).
  VELLUM->colonp=0;
  int i=17; for (;i<=22;i++) {
    const char *src=0;
    int srcc=text_get_string(&src,1,i);
    int srcp=0;
    for (;srcp<srcc;srcp++) {
      if (src[srcp]==':') {
        if (srcp>VELLUM->colonp) VELLUM->colonp=srcp;
        break;
      }
    }
  }
  
  // Assume 11 bytes after the key, and center that longest line.
  int linelen=VELLUM->colonp+11;
  int extra=(COLC>>1)-(linelen>>1);
  if (extra>0) VELLUM->colonp+=extra;
}

/* Line up colons, based on a measurement we took at load.
 */
 
static void stats_align(char *line,struct vellum *vellum) {
  int linep=0,colonp=-1;
  for (;linep<COLC;linep++) {
    if (line[linep]==':') {
      colonp=linep;
      break;
    }
  }
  if (colonp<0) return;
  int shift=VELLUM->colonp-colonp;
  if (shift<=0) return;
  int available=0;
  while ((available<COLC)&&((unsigned char)line[COLC-available-1]<=0x20)) available++;
  if (available<=0) return;
  if (shift>available) shift=available;
  memmove(line+shift,line,COLC-shift);
  memset(line,' ',shift);
}

/* Rewrite bits of the report into (VELLUM->text).
 * The individual bits take a buffer COLC long and overwrite the whole thing.
 */
  
static void stats_write_progress(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  int pct=game_get_completion();
  struct text_insertion insv[]={
    {.mode='i',.i=pct},
  };
  text_format_res(dst,COLC,1,17,insv,sizeof(insv)/sizeof(insv[0]));
  stats_align(dst,vellum);
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
  stats_align(dst,vellum);
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
  stats_align(dst,vellum);
}

static void stats_write_sidequests(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  int completec,totalc;
  game_get_sidequests(&completec,&totalc);
  struct text_insertion insv[]={
    {.mode='i',.i=completec},
    {.mode='i',.i=totalc},
  };
  text_format_res(dst,COLC,1,20,insv,sizeof(insv)/sizeof(insv[0]));
  stats_align(dst,vellum);
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
  stats_align(dst,vellum);
}

static void stats_write_maps(struct vellum *vellum,char *dst) {
  memset(dst,' ',COLC);
  struct jigstore_progress progress;
  jigstore_progress_tabulate(&progress);
  int v=progress.piecec_got+progress.finished;
  int c=progress.piecec_total+1;
  int pct;
  if (c<1) pct=0;
  else if (v<1) pct=0;
  else if (v>=c) pct=100;
  else {
    pct=(v*100)/c;
    if (pct<1) pct=1;
    else if (pct>99) pct=99;
  }
  struct text_insertion insv[]={
    {.mode='i',.i=pct},
  };
  text_format_res(dst,COLC,1,22,insv,sizeof(insv)/sizeof(insv[0]));
  stats_align(dst,vellum);
}

static void stats_rewrite_all_except_maps(struct vellum *vellum) {
  stats_write_progress  (vellum,VELLUM->text+ 0*COLC);
  stats_write_time      (vellum,VELLUM->text+ 1*COLC);
  stats_write_flowers   (vellum,VELLUM->text+ 2*COLC);
  stats_write_sidequests(vellum,VELLUM->text+ 3*COLC);
  stats_write_items     (vellum,VELLUM->text+ 4*COLC);
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

/* Focus.
 */
 
static void _stats_focus(struct vellum *vellum,int focus) {
  if (focus) {
    // We have to assume that jigsaw progress could have changed, since their assembly happens in pause modal too.
    stats_write_maps(vellum,VELLUM->text+ 5*COLC);
  }
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
  stats_measure_alignment(vellum);
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
  
  stats_measure_alignment(vellum);
  memset(VELLUM->text,' ',sizeof(VELLUM->text));
  stats_rewrite_all_except_maps(vellum); // maps are expensive, and we'll rewrite them at focus
  
  return vellum;
}

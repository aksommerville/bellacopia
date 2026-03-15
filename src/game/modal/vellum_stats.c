#include "game/bellacopia.h"
#include "vellum.h"

#define COMPLETABLE_LIMIT 16

struct vellum_stats {
  struct vellum hdr;
  int pct;
  struct completable total;
  struct completable completablev[COMPLETABLE_LIMIT];
  int completablec;
  int texid,texw,texh;
  int store_listener;
  int dirty;
  
  // Positions relative to (tex) for dynamic values:
  int clockx,clocky;
  int flowerx,flowery;
  int clocktex,clockw,clockh;
  int clocksec;
  int flowerv[7];
};

#define VELLUM ((struct vellum_stats*)vellum)

/* Delete.
 */
 
static void _stats_del(struct vellum *vellum) {
  egg_texture_del(VELLUM->texid);
  egg_texture_del(VELLUM->clocktex);
  store_unlisten(VELLUM->store_listener);
}

/* Focus.
 */
 
static void _stats_focus(struct vellum *vellum,int focus) {
  if (focus) {
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
}

/* Language change.
 */
 
static void _stats_langchanged(struct vellum *vellum,int lang) {
  VELLUM->dirty=1;
}

/* Store changed.
 */
 
static void _stats_store_changed(char type,int id,int value,void *userdata) {
  struct vellum *vellum=userdata;
  switch (type) {
    case 'f':
    case 'j':
    case 'i':
      VELLUM->dirty=1;
      break;
  }
}

/* Format time in seconds for display.
 */
 
static int stats_format_time(char *dst,int dsta,int sec) {
  if (!dst||(dsta<1)) return 0;
  if (sec<0) sec=0;
  int min=sec/60; sec%=60;
  int hour=min/60; min%=60;
  if (hour>999) {
    hour=999;
    min=99;
    sec=99;
  }
  int dstc=0;
  if (hour>=100) { if (dstc<dsta) dst[dstc]='0'+hour/100; dstc++; }
  if (hour>=10) { if (dstc<dsta) dst[dstc]='0'+(hour/10)%10; dstc++; }
  if (hour>=1) {
    if (dstc<dsta) dst[dstc]='0'+hour%10; dstc++;
    if (dstc<dsta) dst[dstc]=':'; dstc++;
  }
  if ((hour>0)||(min>=10)) { if (dstc<dsta) dst[dstc]='0'+min/10; dstc++; }
  // Minutes ones and lower print always.
  if (dstc<dsta) dst[dstc]='0'+min%10; dstc++;
  if (dstc<dsta) dst[dstc]=':'; dstc++;
  if (dstc<dsta) dst[dstc]='0'+sec/10; dstc++;
  if (dstc<dsta) dst[dstc]='0'+sec%10; dstc++;
  return dstc;
}

/* Fill rectangle.
 */
 
static void stats_fill_rect(uint32_t *dst,int dstw,int dsth,int x,int y,int w,int h,uint32_t rgba) {
  rgba=(rgba>>24)|((rgba&0xff0000)>>8)|((rgba&0xff00)<<8)|(rgba<<24);
  if (x<0) { w+=x; x=0; }
  if (y<0) { h+=y; y=0; }
  if (x>dstw-w) w=dstw-x;
  if (y>dsth-h) h=dsth-y;
  if ((w<1)||(h<1)) return;
  dst+=y*dstw+x;
  for (;h-->0;dst+=dstw) {
    uint32_t *dstp=dst;
    int xi=w;
    for (;xi-->0;dstp++) *dstp=rgba;
  }
}

/* Render one line of the report.
 */
 
static void stats_render_completable(uint32_t *dst,int y,struct vellum *vellum,const struct completable *comp,int sepx) {
  const char *k=0;
  int kc=text_get_string(&k,1,comp->strix);
  int kw=font_measure_string(g.font,k,kc);
  int dstx=sepx-kw;
  if (dstx<0) dstx=0;
  uint32_t kcolor=0x000000ff;
  if (comp->numer<=0) kcolor=0x606060ff;
  else if (comp->numer>=comp->denom) kcolor=0x008000ff;
  font_render(
    dst+y*VELLUM->texw+dstx,VELLUM->texw,VELLUM->texh-y,VELLUM->texw<<2,
    g.font,k,kc,kcolor
  );
  switch (comp->strix) {
  
    /* Flowers: Record position so we can render live with animated flower icons.
     */
    case 30: {
        int dstx=sepx;
        dstx+=font_render(
          dst+y*VELLUM->texw+dstx,VELLUM->texw-dstx,VELLUM->texh-y,VELLUM->texw<<2,
          g.font,": ",2,0x000000ff
        );
        VELLUM->flowerx=dstx;
        VELLUM->flowery=y;
      } break;
      
    /* Completion: Progress bar and percentage.
     */
    case 31: {
        int boxh=font_get_line_height(g.font);
        int boxw=VELLUM->texw-sepx-5;
        int barw=comp->denom?((comp->numer*boxw)/comp->denom):0;
        stats_fill_rect(dst,VELLUM->texw,VELLUM->texh,sepx+5,y-1,boxw,boxh,0xa08060ff);
        stats_fill_rect(dst,VELLUM->texw,VELLUM->texh,sepx+5,y-1,barw,boxh,0x000000ff);
        char pcttext[8];
        int pcttextc=snprintf(pcttext,sizeof(pcttext),"%d%%",VELLUM->pct);
        if ((pcttextc<0)||(pcttextc>sizeof(pcttext))) pcttextc=0;
        font_render(
          dst+y*VELLUM->texw+sepx+7,VELLUM->texw-sepx-7,VELLUM->texh-y,VELLUM->texw<<2,
          g.font,pcttext,pcttextc,0xffff00ff
        );
      } break;
      
    /* Play time: Record position. We'll render it as a separate texture, so as not to redraw this whole report every second.
     */
    case 32: {
        int dstx=sepx;
        dstx+=font_render(
          dst+y*VELLUM->texw+dstx,VELLUM->texw-dstx,VELLUM->texh-y,VELLUM->texw<<2,
          g.font,": ",2,0x000000ff
        );
        VELLUM->clockx=dstx;
        VELLUM->clocky=y;
      } break;
      
    /* Maps: Show as a percentage.
     */
    case 35: {
        int pct=0;
        if (comp->numer&&comp->denom) {
          pct=(comp->numer*100)/comp->denom;
          if (comp->numer>=comp->denom) pct=100;
          else if (pct<1) pct=1;
          else if (pct>99) pct=99;
        }
        char msg[256];
        int msgc=snprintf(msg,sizeof(msg),": %d%%\n",pct);
        if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
        font_render(
          dst+y*VELLUM->texw+sepx,VELLUM->texw-sepx,VELLUM->texh-y,VELLUM->texw<<2,
          g.font,msg,msgc,0x000000ff
        );
      } break;
  
    /* Everything else: KEY: NUMER/DENOM
     */
    default: {
        char msg[256];
        int msgc=snprintf(msg,sizeof(msg),": %d/%d\n",comp->numer,comp->denom);
        if ((msgc<0)||(msgc>=sizeof(msg))) msgc=0;
        font_render(
          dst+y*VELLUM->texw+sepx,VELLUM->texw-sepx,VELLUM->texh-y,VELLUM->texw<<2,
          g.font,msg,msgc,0x000000ff
        );
      }
  }
}

/* Render report.
 * (dst) has size (VELLUM->texw,VELLUM->texh) and is initially zero.
 * (VELLUM->completablev,total,pct) must be populated first.
 * We'll replace (clockx,clocky,flowerx,flowery) along the way.
 */
 
static void stats_render_report(uint32_t *dst,struct vellum *vellum,int sepx) {
  VELLUM->clockx=VELLUM->clocky=-1;
  VELLUM->flowerx=VELLUM->flowery=-1;
  int lineh=font_get_line_height(g.font);
  int y=2;
  stats_render_completable(dst,y,vellum,&VELLUM->total,sepx); y+=lineh;
  struct completable playtime={.strix=32,.numer=1,.denom=2};
  stats_render_completable(dst,y,vellum,&playtime,sepx); y+=lineh;
  const struct completable *comp=VELLUM->completablev;
  int i=VELLUM->completablec;
  for (;i-->0;comp++,y+=lineh) {
    stats_render_completable(dst,y,vellum,comp,sepx);
  }
}

/* Rendered width of a string resource.
 */
 
static int stats_string_width(int strix) {
  const char *src;
  int srcc=text_get_string(&src,1,strix);
  if (srcc<1) return 0;
  return font_measure_string(g.font,src,srcc);
}

/* Generate report.
 */
 
static void stats_generate_report(struct vellum *vellum) {

  // Collect flowers a la carte. Completables only describes them as a total count.
  VELLUM->flowerv[0]=store_get_fld(NS_fld_root1);
  VELLUM->flowerv[1]=store_get_fld(NS_fld_root2);
  VELLUM->flowerv[2]=store_get_fld(NS_fld_root3);
  VELLUM->flowerv[3]=store_get_fld(NS_fld_root4);
  VELLUM->flowerv[4]=store_get_fld(NS_fld_root5);
  VELLUM->flowerv[5]=store_get_fld(NS_fld_root6);
  VELLUM->flowerv[6]=store_get_fld(NS_fld_root7);

  // Collect the essential stats.
  VELLUM->completablec=game_get_completables(VELLUM->completablev,COMPLETABLE_LIMIT);
  if ((VELLUM->completablec<0)||(VELLUM->completablec>COMPLETABLE_LIMIT)) VELLUM->completablec=0;
  VELLUM->pct=completables_total(&VELLUM->total,VELLUM->completablev,VELLUM->completablec);
  VELLUM->total.strix=31;
  
  // Allocate a buffer to render to, client-side.
  VELLUM->texw=250;
  VELLUM->texh=100;
  int sepx=VELLUM->texw>>1;
  uint32_t *rgba=calloc(VELLUM->texw*4,VELLUM->texh);
  if (!rgba) return;
  stats_render_report(rgba,vellum,sepx);
  
  if (!VELLUM->texid) VELLUM->texid=egg_texture_new();
  egg_texture_load_raw(VELLUM->texid,VELLUM->texw,VELLUM->texh,VELLUM->texw<<2,rgba,VELLUM->texw*VELLUM->texh*4);
  
  free(rgba);
}

/* Render.
 */
 
static void _stats_render(struct vellum *vellum,int x,int y,int w,int h) {
  if (VELLUM->dirty) {
    VELLUM->dirty=0;
    stats_generate_report(vellum);
  }
  
  // If the clock reached a new second, redraw it.
  int sec=(int)(store_get_clock(NS_clock_playtime)+store_get_clock(NS_clock_battletime)+store_get_clock(NS_clock_pausetime));
  if (sec!=VELLUM->clocksec) {
    VELLUM->clocksec=sec;
    char tmp[16];
    int tmpc=stats_format_time(tmp,sizeof(tmp),sec);
    font_render_to_texture(VELLUM->clocktex,g.font,tmp,tmpc,FBW,FBH,0x000000ff);
    egg_texture_get_size(&VELLUM->clockw,&VELLUM->clockh,VELLUM->clocktex);
  }
  
  int dstx=x+(w>>1)-(VELLUM->texw>>1);
  int dsty=y+(h>>1)-(VELLUM->texh>>1);
  graf_set_input(&g.graf,VELLUM->texid);
  graf_decal(&g.graf,dstx,dsty,0,0,VELLUM->texw,VELLUM->texh);
  
  // Play time. Keeps running while paused.
  graf_set_input(&g.graf,VELLUM->clocktex);
  graf_decal(&g.graf,dstx+VELLUM->clockx,dsty+VELLUM->clocky,0,0,VELLUM->clockw,VELLUM->clockh);
  
  // Flowers.
  graf_set_image(&g.graf,RID_image_pause);
  int flx=dstx+VELLUM->flowerx+2;
  int fly=dsty+VELLUM->flowery+4;
  int i=0; for (;i<7;i++,flx+=10) {
    graf_tile(&g.graf,flx,fly,0xa6+i+(VELLUM->flowerv[i]?0x10:0),0);
  }
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
  
  VELLUM->store_listener=store_listen(0,_stats_store_changed,vellum);
  VELLUM->dirty=1;
  VELLUM->clocktex=egg_texture_new();
  VELLUM->clocksec=-1;
  
  return vellum;
}

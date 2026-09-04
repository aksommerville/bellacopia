#include "game/bellacopia.h"
#include "game/race/race.h"
#include "multicamera.h"

/* Private globals.
 */
 
#define VIEW_LIMIT 4
 
static struct {
  struct multicamera_view viewv[VIEW_LIMIT];
  int viewc; // Zero if not initialized.
  int texid,texw,texh;
} multicamera={0};

/* Cleanup.
 */
 
static void multicamera_view_cleanup(struct multicamera_view *view) {
  sprite_group_cleanup(&view->group,1);
}

void multicamera_quit() {
  while (multicamera.viewc>0) {
    multicamera.viewc--;
    multicamera_view_cleanup(multicamera.viewv+multicamera.viewc);
  }
  egg_texture_del(multicamera.texid);
  memset(&multicamera,0,sizeof(multicamera));
}

/* Bounds.
 */
 
static void multicamera_bounds(int p,int x,int y,int w,int h) {
  struct multicamera_view *view=multicamera.viewv+p;
  view->dstx=x;
  view->dsty=y;
  view->dstw=w;
  view->dsth=h;
}

/* Init.
 */

int multicamera_init(int viewc) {
  if ((viewc<1)||(viewc>VIEW_LIMIT)) return -1;
  if (multicamera.viewc) return -1;
  multicamera.viewc=viewc;
  
  // Establish view output bounds.
  switch (viewc) {
    case 1: {
        multicamera_bounds(0,0,0,FBW,FBH);
      } break;
    case 2: {
        multicamera_bounds(0,0,0,(FBW>>1)-1,FBH);
        multicamera_bounds(1,(FBW>>1),0,FBW>>1,FBH);
      } break;
    case 3: {
        multicamera_bounds(0,0,0,(FBW>>1)-1,FBH);
        multicamera_bounds(1,(FBW>>1),0,FBW>>1,(FBH>>1)-1);
        multicamera_bounds(2,(FBW>>1),FBH>>1,FBW>>1,FBH>>1);
      } break;
    case 4: {
        multicamera_bounds(0,0,0,(FBW>>1)-1,(FBH>>1)-1);
        multicamera_bounds(1,(FBW>>1),0,FBW>>1,(FBH>>1)-1);
        multicamera_bounds(2,0,FBH>>1,(FBW>>1)-1,FBH>>1);
        multicamera_bounds(3,(FBW>>1),FBH>>1,FBW>>1,FBH>>1);
      } break;
  }
  
  // Allocate a scratch texture as least as large as every view, on each axis.
  struct multicamera_view *view=multicamera.viewv;
  int i=multicamera.viewc;
  for (;i-->0;view++) {
    if (view->dstw>multicamera.texw) multicamera.texw=view->dstw;
    if (view->dsth>multicamera.texh) multicamera.texh=view->dsth;
  }
  multicamera.texid=egg_texture_new();
  egg_texture_load_raw(multicamera.texid,multicamera.texw,multicamera.texh,multicamera.texw<<2,0,0);
  
  // Find the focus sprite for each view.
  struct sprite **spritep=GRP(visible)->sprv;
  for (i=GRP(visible)->sprc;i-->0;spritep++) {
    struct sprite *sprite=*spritep;
    if (sprite->type!=&sprite_type_racer) continue;
    int human=sprite_racer_is_human(sprite);
    if ((human>=1)&&(human<=viewc)) {
      sprite_group_add(&multicamera.viewv[human-1].group,sprite);
    }
  }
  
  return 0;
}

/* Trivial accessors.
 */

struct multicamera_view *multicamera_get_view(int p) {
  if ((p<0)||(p>=multicamera.viewc)) return 0;
  return multicamera.viewv+p;
}

/* Update.
 */

void multicamera_update(double elapsed) {
  struct multicamera_view *view=multicamera.viewv;
  int i=multicamera.viewc;
  for (;i-->0;view++) {
    if (view->group.sprc<1) continue; // No sprite, just leave it wherever it is.
    struct sprite *sprite=view->group.sprv[0];
    view->z=sprite->z;
    view->x=(int)(sprite->x*NS_sys_tilesize)-(view->dstw>>1);
    view->y=(int)(sprite->y*NS_sys_tilesize)-(view->dsth>>1);
    if (!view->plane||(view->plane->z!=view->z)) {
      if (!(view->plane=plane_by_position(view->z))) continue;
    }
    int xlimit=view->plane->w*NS_sys_mapw*NS_sys_tilesize-view->dstw;
    int ylimit=view->plane->h*NS_sys_maph*NS_sys_tilesize-view->dsth;
    if (view->x<0) view->x=0; else if (view->x>xlimit) view->x=xlimit;
    if (view->y<0) view->y=0; else if (view->y>ylimit) view->y=ylimit;
  }
}

/* Render one map for a view.
 * (cola,colz,rowa,rowz) are the full bounds requested, in plane meters.
 * We'll clip that down.
 */
 
static void multicamera_render_map(struct multicamera_view *view,const struct map *map,int cola,int colz,int rowa,int rowz) {

  /* Shift bounds so they're local to this map, then clip.
   */
  cola-=map->lng*NS_sys_mapw;
  colz-=map->lng*NS_sys_mapw;
  rowa-=map->lat*NS_sys_maph;
  rowz-=map->lat*NS_sys_maph;
  if (cola<0) cola=0;
  if (rowa<0) rowa=0;
  if (colz>=NS_sys_mapw) colz=NS_sys_mapw-1;
  if (rowz>=NS_sys_maph) rowz=NS_sys_maph-1;
  
  graf_set_image(&g.graf,map->imageid);
  
  /* Iterate cells.
   */
  int dstx0=(cola+map->lng*NS_sys_mapw)*NS_sys_tilesize+(NS_sys_tilesize>>1)-view->x;
  int dsty=(rowa+map->lat*NS_sys_maph)*NS_sys_tilesize+(NS_sys_tilesize>>1)-view->y;
  const uint8_t *cellrow=map->v+rowa*NS_sys_mapw+cola;
  int row=rowa;
  for (;row<=rowz;row++,cellrow+=NS_sys_mapw,dsty+=NS_sys_tilesize) {
    const uint8_t *cell=cellrow;
    int col=cola;
    int dstx=dstx0;
    for (;col<=colz;col++,cell++,dstx+=NS_sys_tilesize) {
      graf_tile(&g.graf,dstx,dsty,*cell,0);
    }
  }
}

/* Render maps for one view.
 * Fills (multicamera.texid) up to (view->dstw,h).
 * Requires that view bounds be smaller than the plane, and inside it.
 * (multicamera_update enforces that).
 */
 
static void multicamera_render_maps(struct multicamera_view *view) {

  /* Plane not loaded? Black it and get out.
   * This should never happen.
   */
  if (!view->plane) {
    graf_fill_rect(&g.graf,0,0,view->dstw,view->dsth,0x000000ff);
    return;
  }

  /* Get the quantized coverage bounds in plane meters.
   */
  int cola=view->x/NS_sys_tilesize;
  int rowa=view->y/NS_sys_tilesize;
  int colz=(view->x+view->dstw-1)/NS_sys_tilesize;
  int rowz=(view->y+view->dsth-1)/NS_sys_tilesize;
  if (cola<0) cola=0;
  if (rowa<0) rowa=0;
  int xlimit=view->plane->w*NS_sys_mapw;
  int ylimit=view->plane->h*NS_sys_maph;
  if (colz>=xlimit) colz=xlimit-1;
  if (rowz>=ylimit) rowz=ylimit-1;
  
  /* Quantize one step further, to maps.
   */
  int mxa=cola/NS_sys_mapw;
  int mxz=colz/NS_sys_mapw;
  int mya=rowa/NS_sys_maph;
  int myz=rowz/NS_sys_maph;
  
  /* Iterate maps.
   */
  int my=mya;
  const struct map *maprow=view->plane->v+mya*view->plane->w+mxa;
  for (;my<=myz;my++,maprow+=view->plane->w) {
    int mx=mxa;
    const struct map *map=maprow;
    for (;mx<=mxz;mx++,map++) {
      multicamera_render_map(view,map,cola,colz,rowa,rowz);
    }
  }
}

/* Render sprites for one view.
 */
 
static void multicamera_render_sprites(struct multicamera_view *view) {
  struct sprite **p=GRP(visible)->sprv;
  int i=GRP(visible)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    int dstx=(int)(sprite->x*NS_sys_tilesize)-view->x;
    int dsty=(int)(sprite->y*NS_sys_tilesize)-view->y;
    if (sprite->type->render) {
      sprite->type->render(sprite,dstx,dsty);
    } else {
      graf_set_image(&g.graf,sprite->imageid);
      graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
    }
  }
}

/* Render one view.
 */
 
static void multicamera_view_render(struct multicamera_view *view) {
  graf_set_output(&g.graf,multicamera.texid);
  multicamera_render_maps(view);
  struct sprite *racer=0;
  if (view->group.sprc>=1) racer=view->group.sprv[0];
  race_render_checkpoints(view->x,view->y,racer); // TODO conditionalize, if multicamera is ever used outside broom races.
  multicamera_render_sprites(view);
  graf_set_output(&g.graf,1);
  graf_set_input(&g.graf,multicamera.texid);
  graf_decal(&g.graf,view->dstx,view->dsty,0,0,view->dstw,view->dsth);
}

/* Render all.
 */

void multicamera_render() {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x000000ff);
  struct multicamera_view *view=multicamera.viewv;
  int i=multicamera.viewc;
  for (;i-->0;view++) multicamera_view_render(view);
}

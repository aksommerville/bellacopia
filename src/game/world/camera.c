#include "game/game.h"
#include "game/sprite/sprite.h"
#include "map.h"
#include "camera.h"

/* Private globals.
 */
 
static struct {
  int mx,my,mz; // Map coords for the one currently focussed.
  int px,py,pw,ph,poobx,pooby; // Detail for the current plane.
  const struct map *mapv; // WEAK, current plane.
  double fx,fy; // Focus point in plane meters.
  int vx,vy; // Top left of visible area, in plane pixels.
} camera={0};

/* Adjust plane coordinates re oob, the same way the map loader does.
 */
 
static int camera_adjust_plane(int n,int p,int c,int oob) {
  if (c<1) return n;
  n-=p;
  switch (oob) {
    case NS_mapoob_loop: while (n<0) n+=c; n%=c; break;
    case NS_mapoob_repeat: if (n<0) n=0; else if (n>=c) n=c-1; break;
    case NS_mapoob_farloop: {
        int twice=c<<1;
        int half=c>>1;
        int adj=n+half;
        while (adj<0) adj+=twice;
        adj%=twice;
        n=adj-half;
        if (n<0) n=0; else if (n>=c) n=c-1;
      } break;
  }
  return n+p;
}
 
static int camera_adjust_plane_x(int x) {
  return camera_adjust_plane(x,camera.px,camera.pw,camera.poobx);
}

static int camera_adjust_plane_y(int y) {
  return camera_adjust_plane(y,camera.py,camera.ph,camera.pooby);
}

/* Similar to "adjust_plane", but return map's top left corner in framebuffer pixels.
 * Beware there are some subtle differences from map coords.
 */
 
static int camera_map_dstx(int mx) {
  return mx*NS_sys_mapw*NS_sys_tilesize-camera.vx;
}

static int camera_map_dsty(int my) {
  return my*NS_sys_maph*NS_sys_tilesize-camera.vy;
}

/* New map enters focus.
 */
 
static void camera_new_map(int x,int y) {
  const struct map *map=0;
  int nx=camera_adjust_plane_x(x);
  int ny=camera_adjust_plane_y(y);
  if ((nx>=camera.px)&&(ny>=camera.py)&&(nx<camera.px+camera.pw)&&(ny<camera.py+camera.ph)) {
    map=camera.mapv+(ny-camera.py)*camera.pw+(nx-camera.px);
  }
  fprintf(stderr,"%s %d,%d (map:%d)\n",__func__,x,y,map?map->rid:0);
  camera.mx=x;
  camera.my=y;
  
  /* Run the map's commands.
   */
  if (map) {
    struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        //TODO CMD_map_door probably wants to be indexed, and there will be others like that.
        case CMD_map_sprite: {
            double sx=cmd.arg[0]+0.5+(double)(x*NS_sys_mapw);
            double sy=cmd.arg[1]+0.5+(double)(y*NS_sys_maph);
            int rid=(cmd.arg[2]<<8)|cmd.arg[3];
            uint32_t arg=(cmd.arg[4]<<24)|(cmd.arg[5]<<16)|(cmd.arg[6]<<8)|cmd.arg[7];
            if (rid==RID_sprite_hero) {
              if (sprites_get_hero()) continue;
            } //TODO Similarly, we need a generic check for "does the sprite from this spawn point already exist?"
            struct sprite *sprite=sprite_spawn(sx,sy,rid,arg,0,0,0);
          } break;
      }
    }
  }
}

/* Reset.
 */
 
int camera_reset(int mapid) {
  fprintf(stderr,"%s map:%d\n",__func__,mapid);
  if (mapid<1) return -1;
  struct map *map=map_by_id(mapid);
  if (!map) {
    fprintf(stderr,"map:%d not found\n",mapid);
    return -1;
  }
  camera.mx=map->x;
  camera.my=map->y;
  camera.mz=map->z;
  if (!(camera.mapv=maps_get_plane(
    &camera.px,&camera.py,
    &camera.pw,&camera.ph,
    &camera.poobx,&camera.pooby,
    camera.mz
  ))) return -1;
  camera.fx=(double)(map->x*NS_sys_mapw)+NS_sys_mapw*0.5;
  camera.fy=(double)(map->y*NS_sys_maph)+NS_sys_maph*0.5;
  sprites_clear();
  camera_new_map(camera.mx,camera.my);
  camera_update(0.0);
  return 0;
}

/* Update.
 */
 
void camera_update(double elapsed) {

  // If we don't have sensible plane bounds, get out.
  if ((camera.pw<1)||(camera.ph<1)) return;

  // Acquire focus point. If there's no hero, stay put and get out.
  struct sprite *hero=sprites_get_hero();
  if (!hero) return;
  camera.fx=hero->x;
  camera.fy=hero->y;
  
  /* If the OOB mode is "null", clamp focus.
   * Also, with null OOB and just one map along that axis, force focus to center.
   * (actually that happens implicitly)
   */
  if (camera.poobx==NS_mapoob_null) {
    double lo=camera.px*NS_sys_mapw+NS_sys_mapw*0.5;
    double hi=(camera.px+camera.pw)*NS_sys_mapw-NS_sys_mapw*0.5;
    if (camera.fx<lo) camera.fx=lo;
    else if (camera.fx>hi) camera.fx=hi;
  }
  if (camera.pooby==NS_mapoob_null) {
    double lo=camera.py*NS_sys_maph+NS_sys_maph*0.5;
    double hi=(camera.py+camera.ph)*NS_sys_maph-NS_sys_maph*0.5;
    if (camera.fy<lo) camera.fy=lo;
    else if (camera.fy>hi) camera.fy=hi;
  }
  
  /* Quantize (fx,fy) ie the hero's position, to form our position in plane coords.
   * If that changed since last update, reassess sprites and music and such.
   */
  int mx=(int)camera.fx/NS_sys_mapw;
  int my=(int)camera.fy/NS_sys_maph;
  if (camera.fx<0.0) mx--; // We want floor, not trunc.
  if (camera.fy<0.0) my--;
  //fprintf(stderr,"%s f=%.03f,%.03f m=%d,%d\n",__func__,camera.fx,camera.fy,mx,my);
  if ((mx!=camera.mx)||(my!=camera.my)) {
    camera_new_map(mx,my);
  }
  
  // And finally, establish (vx,vy).
  camera.vx=(int)(camera.fx*NS_sys_tilesize-(FBW>>1));
  camera.vy=(int)(camera.fy*NS_sys_tilesize-(FBH>>1));
}

/* Render one map. XXX TEMP
 */
 
static void camera_render_map(int dstx,int dsty,const struct map *map) {
  graf_set_image(&g.graf,map->imageid);
  int y=dsty+(NS_sys_tilesize>>1);
  const uint8_t *p=map->v;
  int yi=NS_sys_maph;
  for (;yi-->0;y+=NS_sys_tilesize) {
    int x=dstx+(NS_sys_tilesize>>1);
    int xi=NS_sys_mapw;
    for (;xi-->0;x+=NS_sys_tilesize,p++) {
      graf_tile(&g.graf,x,y,*p,0);
    }
  }
}

/* Render.
 */
 
void camera_render() {

  /* TODO Maintain a background buffer of 2x2 maps.
   * Due to the general complexity of this camera, I'm doing everything the ridiculously expensive way first, just to keep things straight.
   * It is unforgivably inefficient to draw every map from scratch every time.
   * ...well... Now that I'm actually observing it, there isn't much CPU load visible.
   * Do change this, but there's no urgency.
   */
   
  // Visit all 9 candidate maps. 5 of them will not be visible, we'll check before any heavy lifting.
  const int mapw=NS_sys_mapw*NS_sys_tilesize; // Be careful -- maps are not spaced by (FBW,FBH)
  const int maph=NS_sys_maph*NS_sys_tilesize;
  int rx=-1; for (;rx<=1;rx++) {
    int mx=camera_adjust_plane_x(camera.mx+rx);
    if ((mx<camera.px)||(mx>=camera.px+camera.pw)) continue;
    int dstx=camera_map_dstx(camera.mx+rx);
    if (dstx>=mapw) continue;
    if (dstx<=-mapw) continue;
    int ry=-1; for (;ry<=1;ry++) {
      int my=camera_adjust_plane_y(camera.my+ry);
      if ((my<camera.py)||(my>=camera.py+camera.ph)) continue;
      int dsty=camera_map_dsty(camera.my+ry);
      if (dsty>=maph) continue;
      if (dsty<=-maph) continue;
      const struct map *map=camera.mapv+(my-camera.py)*camera.pw+(mx-camera.px);
      if (map) camera_render_map(dstx,dsty,map);
      else graf_fill_rect(&g.graf,dstx,dsty,NS_sys_mapw*NS_sys_tilesize,NS_sys_maph*NS_sys_tilesize,0x000000ff);
    }
  }
  
  // Render sprites.
  sprites_render(camera.vx,camera.vy);
}

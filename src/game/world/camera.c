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
  
  /* (poiv) gets wiped out and rebuilt on every map transition.
   * It contains selected map commands.
   * Sort by (x,y). Plane meters.
   * Tracked for the focus map and its neighbors. Roughly the same scope as sprites.
   */
  struct poi {
    int x,y;
    uint8_t opcode;
    const uint8_t *arg; // Straight off the map command, usually includes (col,row) again.
  } *poiv;
  int poic,poia;
  
  int doormapid,doorx,doory; // Deferred door entry.
  int cut;
  
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

/* Is this sprite near enough to our focus that we should not respawn it?
 * May be null or defunct.
 */
 
static int camera_sprite_near(const struct sprite *sprite) {
  if (!sprite||sprite->defunct) return 0;
  // Take the axiswise distance from focus and quantize to meters.
  int dx=(int)(sprite->x-camera.fx);
  int dy=(int)(sprite->y-camera.fy);
  // And let's say a full map length on either axis is too far. (note this means half a screen from the edge, we're comparing to the center).
  if (dx<-NS_sys_mapw) return 0;
  if (dx>NS_sys_mapw) return 0;
  if (dy<-NS_sys_maph) return 0;
  if (dy>NS_sys_maph) return 0;
  return 1;
}

/* Defunct any sprites too far away.
 */
 
static void camera_defunct_far_sprites() {
  struct sprite **p=0;
  int i=sprites_get_all(&p);
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue;
    if (camera_sprite_near(sprite)) continue;
    sprite->defunct=1;
  }
}

/* POI list.
 */
 
// Always >=0, always the first if multiple.
static int camera_poiv_search(int x,int y) {
  int lo=0,hi=camera.poic;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct poi *poi=camera.poiv+ck;
         if (x<poi->x) hi=ck;
    else if (x>poi->x) lo=ck+1;
    else if (y<poi->y) hi=ck;
    else if (y>poi->y) lo=ck+1;
    else {
      while (ck>lo) {
        poi--;
        if (poi->x!=x) break;
        if (poi->y!=y) break;
        ck--;
      }
      return ck;
    }
  }
  return lo;
}
 
static int camera_add_poi(int x,int y,uint8_t opcode,const uint8_t *arg) {
  if (camera.poic>=camera.poia) {
    int na=camera.poia+32;
    if (na>INT_MAX/sizeof(struct poi)) return -1;
    void *nv=realloc(camera.poiv,sizeof(struct poi)*na);
    if (!nv) return -1;
    camera.poiv=nv;
    camera.poia=na;
  }
  int p=camera_poiv_search(x,y);
  struct poi *poi=camera.poiv+p;
  memmove(poi+1,poi,sizeof(struct poi)*(camera.poic-p));
  camera.poic++;
  poi->x=x;
  poi->y=y;
  poi->opcode=opcode;
  poi->arg=arg;
  return 0;
}

/* Spawn sprites and poi for a map. It's either the focus map or a neighbor.
 */
 
static void camera_spawn_sprites(const struct map *map,int mx,int my) {
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
    
      case CMD_map_sprite: {
          double sx=cmd.arg[0]+0.5+(double)(mx*NS_sys_mapw);
          double sy=cmd.arg[1]+0.5+(double)(my*NS_sys_maph);
          int rid=(cmd.arg[2]<<8)|cmd.arg[3];
          if (!camera.cut) { // During scroll-initiated spawns, don't spawn if already visible.
            int spx=(int)(sx*NS_sys_tilesize);
            int spy=(int)(sy*NS_sys_tilesize);
            if ((spx>=camera.vx)&&(spy>=camera.vy)&&(spx<camera.vx+FBW)&&(spy<camera.vy+FBH)) {
              break;
            }
          }
          const uint8_t *arg=cmd.arg+4;
          // All hero spawn points are equivalent; they're the one hero sprite. Otherwise check by spawn arg.
          struct sprite *existing=0;
          if (rid==RID_sprite_hero) existing=sprites_get_hero();
          else existing=sprite_by_arg(arg);
          if (camera_sprite_near(existing)) {
            // The guy from this spawn point was instantiated previously and remains in view, or close to. Keep it and do nothing.
          } else {
            // Either we don't have this guy yet, or he's far away.
            if (existing) {
              existing->defunct=1;
            }
            struct sprite *sprite=sprite_spawn(sx,sy,rid,arg,0,0,0);
            if (sprite) {
              sprite->z=camera.mz;
            }
          }
        } break;
      
      // POI commands with position in first two bytes (should be all of them).
      case CMD_map_door: {
          camera_add_poi(mx*NS_sys_mapw+cmd.arg[0],my*NS_sys_maph+cmd.arg[1],cmd.opcode,cmd.arg);
        } break;
    }
  }
}

/* If there isn't currently a polefairy among the sprites, spawn one.
 * (dy) is <0 for the Borealic Fairy and >0 for the Australic Fairy.
 */
 
static void camera_maybe_polefairy(int dy) {
  if (sprite_by_type(&sprite_type_polefairy)) return;
  
  // They spawn pretty far ahead in the pole direction.
  // That has a nice side effect, that you won't see them if travelling homeward.
  double x=camera.fx;
  double y=camera.fy;
  double tx=camera.fx;
  double ty=camera.fy;
  if (dy<0) { // Borealic Fairy enters from the left and biases a little up.
    x-=NS_sys_mapw*0.666;
    tx-=NS_sys_mapw*0.125;
    y-=20.0;
    ty-=18.0;
  } else { // Australic Fairy enters from the right and biases a little down.
    x+=NS_sys_mapw*0.666;
    tx+=NS_sys_mapw*0.125;
    y+=20.0;
    ty+=18.0;
  }
  
  struct sprite *sprite=sprite_spawn(x,y,RID_sprite_polefairy,0,&sprite_type_polefairy,0,0);
  if (!sprite) return;
  sprite->z=camera.mz;
  sprite_polefairy_set_target(sprite,tx,ty);
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
  camera.mx=x;
  camera.my=y;
  
  /* Do things specific to the focus map.
   * Song. Maybe others.
   */
  if (map) {
    if (map->songid>=0) {
      //TODO Instead of jumping right to the new song, can we do a crossfade, and retain the playhead of the outgoing one?
      bm_song(map->songid,1);
    }
  }
  
  /* On the occasion of map changes, we reassess sprites for this map and the eight neighbors.
   * This same pass will rebuild (poiv).
   */
  camera.poic=0;
  camera_defunct_far_sprites();
  int rx=-1; for (;rx<=1;rx++) {
    int adjx=camera_adjust_plane_x(camera.mx+rx);
    if ((adjx<camera.px)||(adjx>=camera.px+camera.pw)) continue;
    int ry=-1; for (;ry<=1;ry++) {
      int adjy=camera_adjust_plane_y(camera.my+ry);
      if ((adjy<camera.py)||(adjy>=camera.py+camera.ph)) continue;
      const struct map *smap=camera.mapv+(adjy-camera.py)*camera.pw+(adjx-camera.px);
      if (!smap->cmdc) continue;
      camera_spawn_sprites(smap,camera.mx+rx,camera.my+ry);
    }
  }
  
  /* Should we spawn a polefairy?
   */
  if (camera.pooby==NS_mapoob_repeat) {
    int dy;
    // Thresholds of -3 and +2 cause it to occur first around the fourth instance of the edge screen.
    if ((dy=y-camera.py)<=-3) {
      camera_maybe_polefairy(-1);
    } else if ((dy=y-camera.ph-camera.py)>=2) {
      camera_maybe_polefairy(1);
    }
  }
}

/* After travelling thru a door, move hero to the provided cell.
 */
 
static void camera_force_hero(int x,int y) {
  struct sprite *hero=sprites_get_hero();
  if (hero) {
    hero->x=x+0.5;
    hero->y=y+0.5;
    sprite_hero_ackpos(hero);
  } else {
    hero=sprite_spawn(x+0.5,y+0.5,RID_sprite_hero,0,0,0,0);
  }
}

/* Reset.
 */
 
int camera_reset(int mapid) {
  if (mapid<1) return -1;
  struct map *map=map_by_id(mapid);
  if (!map) {
    fprintf(stderr,"map:%d not found\n",mapid);
    return -1;
  }
  int was_door=camera.doormapid;
  int herox=map->x*NS_sys_mapw+camera.doorx;
  int heroy=map->y*NS_sys_maph+camera.doory;
  camera.doormapid=camera.doorx=camera.doory=0;
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
  camera.cut=1;
  camera_new_map(camera.mx,camera.my);
  if (was_door) camera_force_hero(herox,heroy);
  camera_update(0.0);
  camera.cut=0;
  return 0;
}

/* Update.
 */
 
void camera_update(double elapsed) {

  // Is there a door transition pending?
  if (camera.doormapid) {
    camera_reset(camera.doormapid);
  }

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
  if ((mx!=camera.mx)||(my!=camera.my)) {
    camera_new_map(mx,my);
  }
  
  // And finally, establish (vx,vy).
  camera.vx=lround(camera.fx*NS_sys_tilesize-(FBW>>1));
  camera.vy=lround(camera.fy*NS_sys_tilesize-(FBH>>1));
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

/* Trivial accessors.
 */
 
int camera_get_z() {
  return camera.mz;
}

int camera_for_each_poi(int x,int y,int (*cb)(uint8_t opcode,const uint8_t *arg,void *userdata),void *userdata) {
  int p=camera_poiv_search(x,y);
  const struct poi *poi=camera.poiv+p;
  int i=camera.poic-p;
  for (;i-->0;poi++) {
    if (poi->x!=x) break;
    if (poi->y!=y) break;
    int err=cb(poi->opcode,poi->arg,userdata);
    if (err) return err;
  }
  return 0;
}

void camera_enter_door(int mapid,int col,int row) {
//TODO Fade to black?
  camera.doormapid=mapid;
  camera.doorx=col;
  camera.doory=row;
}

void camera_warp_home_soon() {
  int col=NS_sys_mapw>>1;
  int row=NS_sys_maph>>1;
  struct map *map=map_by_id(RID_map_start);
  if (map) {
    struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      if (cmd.opcode==CMD_map_sprite) {
        int rid=(cmd.arg[2]<<8)|cmd.arg[3];
        if (rid==RID_sprite_hero) {
          col=cmd.arg[0];
          row=cmd.arg[1];
          break;
        }
      }
    }
  }
  camera_enter_door(RID_map_start,col,row);
}

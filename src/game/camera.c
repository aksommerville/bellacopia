#include "game/bellacopia.h"

#define DARK_UP_SPEED   2.000
#define DARK_DOWN_SPEED 5.000

/* Hard reset.
 */
 
void camera_reset() {
  g.camera.listenerid_next=1;
  g.camera.map_listenerc=0;
  g.camera.cell_listenerc=0;
  struct scope *scope=g.camera.scopev;
  int i=CAMERA_SCOPE_LIMIT;
  for (;i-->0;scope++) {
    scope->map=0;
  }
  g.camera.edgel=-1;
  g.camera.edger=-1;
  g.camera.edget=-1;
  g.camera.edgeb=-1;
  g.camera.cut=1;
  g.camera.lock=0;
  g.camera.listenlock=0;
  g.camera.darkness=0.0;
  g.camera.teledx=0.0;
  g.camera.teledy=0.0;
  g.camera.transition=0;
  g.camera.transition_time=0.0;
}

/* Listeners.
 */

void camera_unlisten(int listenerid) {
  // It's ok to unlisten even while locked; listeners might remove themselves.
  // Listeners are asked not to remove any other listeners during their callback. (that would be kind of a weird thing to do anyway).
  int i=g.camera.map_listenerc;
  struct map_listener *ml=g.camera.map_listenerv+i-1;
  for (;i-->0;ml--) {
    if (ml->listenerid==listenerid) {
      g.camera.map_listenerc--;
      memmove(ml,ml+1,sizeof(struct map_listener)*(g.camera.map_listenerc-i));
      return;
    }
  }
  struct cell_listener *cl=g.camera.cell_listenerv+g.camera.cell_listenerc-1;
  for (i=g.camera.cell_listenerc;i-->0;cl++) {
    if (cl->listenerid==listenerid) {
      g.camera.cell_listenerc--;
      memmove(cl,cl+1,sizeof(struct cell_listener)*(g.camera.cell_listenerc-i));
      return;
    }
  }
}

int camera_listen_map(void (*cb)(struct map *map,int focus,void *userdata),void *userdata) {
  if (g.camera.listenlock) return -1;
  if (g.camera.map_listenerc>=g.camera.map_listenera) {
    int na=g.camera.map_listenera+8;
    if (na>INT_MAX/sizeof(struct map_listener)) return -1;
    void *nv=realloc(g.camera.map_listenerv,sizeof(struct map_listener)*na);
    if (!nv) return -1;
    g.camera.map_listenerv=nv;
    g.camera.map_listenera=na;
  }
  if (g.camera.listenerid_next<1) g.camera.listenerid_next=1;
  struct map_listener *ml=g.camera.map_listenerv+g.camera.map_listenerc++;
  ml->cb=cb;
  ml->userdata=userdata;
  ml->listenerid=g.camera.listenerid_next++;
  return ml->listenerid;
}

int camera_listen_cell(void (*cb)(int x,int y,int w,int h,void *userdata),void *userdata) {
  if (g.camera.listenlock) return -1;
  if (g.camera.cell_listenerc>=g.camera.cell_listenera) {
    int na=g.camera.cell_listenera+8;
    if (na>INT_MAX/sizeof(struct cell_listener)) return -1;
    void *nv=realloc(g.camera.cell_listenerv,sizeof(struct cell_listener)*na);
    if (!nv) return -1;
    g.camera.cell_listenerv=nv;
    g.camera.cell_listenera=na;
  }
  if (g.camera.listenerid_next<1) g.camera.listenerid_next=1;
  struct cell_listener *cl=g.camera.cell_listenerv+g.camera.cell_listenerc++;
  cl->cb=cb;
  cl->userdata=userdata;
  cl->listenerid=g.camera.listenerid_next++;
  return cl->listenerid;
}

/* Broadcast to listeners.
 */
 
static void camera_broadcast_map(struct map *map,int focus) {
  if (g.camera.listenlock) return;
  g.camera.listenlock=1;
  int i=g.camera.map_listenerc;
  struct map_listener *ml=g.camera.map_listenerv+i-1;
  for (;i-->0;ml--) ml->cb(map,focus,ml->userdata);
  g.camera.listenlock=0;
}

static void camera_broadcast_cell(int x,int y,int w,int h) {
  if (g.camera.listenlock) return;
  g.camera.listenlock=1;
  int i=g.camera.cell_listenerc;
  struct cell_listener *cl=g.camera.cell_listenerv+i-1;
  for (;i-->0;cl--) cl->cb(x,y,w,h,cl->userdata);
  g.camera.listenlock=0;
}

/* Create texture if needed, and render this scope's map to it.
 * (loading) nonzero if the map is entering scope fresh. Zero if it's a rerender on the fly.
 */
 
static void camera_render_scope(struct scope *scope,int loading) {
  int w=NS_sys_mapw*NS_sys_tilesize;
  int h=NS_sys_maph*NS_sys_tilesize;
  if (scope->texid<1) {
    if ((scope->texid=egg_texture_new())<1) return;
    egg_texture_load_raw(scope->texid,w,h,w<<2,0,0);
  }
  graf_set_output(&g.graf,scope->texid);
  if (scope->map&&scope->map->imageid) {
  
    const char *debugmsg=0;
    int debugmsgc=0;
    struct cmdlist_reader reader={.v=scope->map->cmd,.c=scope->map->cmdc};
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_map_switchable:
        case CMD_map_stompbox:
        case CMD_map_treadle: {
            int x=cmd.arg[0],y=cmd.arg[1];
            if ((x<NS_sys_mapw)&&(y<NS_sys_maph)) {
              int p=y*NS_sys_mapw+x;
              if (store_get_fld((cmd.arg[2]<<8)|cmd.arg[3])) {
                scope->map->v[p]=scope->map->rov[p]+1;
              } else {
                scope->map->v[p]=scope->map->rov[p];
              }
            }
          } break;
        case CMD_map_debugmsg: debugmsg=(char*)cmd.arg; debugmsgc=cmd.argc; break;
      }
    }
  
    graf_set_image(&g.graf,scope->map->imageid);
    int dsty=NS_sys_tilesize>>1;
    const uint8_t *src=scope->map->v;
    int yi=NS_sys_maph;
    for (;yi-->0;dsty+=NS_sys_tilesize) {
      int dstx=NS_sys_tilesize>>1;
      int xi=NS_sys_mapw;
      for (;xi-->0;dstx+=NS_sys_tilesize,src++) {
        graf_tile(&g.graf,dstx,dsty,*src,0);
      }
    }
    
    if (debugmsgc) {
      int texid=font_render_to_texture(0,g.font,debugmsg,debugmsgc,w,h,0xffffffff);
      if (texid>0) {
        int texw=0,texh=0;
        egg_texture_get_size(&texw,&texh,texid);
        graf_set_input(&g.graf,texid);
        graf_decal(&g.graf,(w>>1)-(texw>>1),(h>>1)-(texh>>1),0,0,texw,texh);
        graf_flush(&g.graf);
        egg_texture_del(texid);
      }
    }
    
  } else { // No map, we shouldn't have been called. But black it out just to be safe.
    graf_fill_rect(&g.graf,0,0,w,h,0x000000ff);
  }
  graf_set_output(&g.graf,1);
}

/* If this map is not already in our scope, add it, render it, and broadcast.
 */
 
static int camera_maybe_add_scope(struct map *map) {
  struct scope *empty=0;
  struct scope *scope=g.camera.scopev;
  int i=CAMERA_SCOPE_LIMIT;
  for (;i-->0;scope++) {
    if (scope->map==map) return 0;
    if (!scope->map) empty=scope;
  }
  if (!empty) return 0; // Absent but no room for it! This shouldn't be possible.
  empty->map=map;
  camera_render_scope(empty,1);
  camera_broadcast_map(map,1);
  return 1;
}

/* Calculate the edge columns and rows from scratch based on (rx,ry).
 */
 
static void camera_calculate_edges(int *l,int *r,int *t,int *b) {
  *l=g.camera.rx/NS_sys_tilesize-1;
  *t=g.camera.ry/NS_sys_tilesize-1;
  *r=(g.camera.rx+FBW-1)/NS_sys_tilesize+1;
  *b=(g.camera.ry+FBH-1)/NS_sys_tilesize+1;
}

/* Check for any new exposures.
 * (ox,oy) are the previous values of (rx,ry).
 */
 
static void camera_check_exposures(int ox,int oy) {
  struct plane *plane=plane_by_position(g.camera.z);
  if (!plane) return;
  if ((plane->w<1)||(plane->h<1)) return;

  /* Compute coverage as lng,lat. Clamp to plane.
   * Abort if we end up empty, or more than 4 visible maps (that's not possible).
   */
  const int mapw=NS_sys_mapw*NS_sys_tilesize;
  const int maph=NS_sys_maph*NS_sys_tilesize;
  int lnga=g.camera.rx/mapw;
  int lngz=(g.camera.rx+FBW-1)/mapw;
  int lata=g.camera.ry/maph;
  int latz=(g.camera.ry+FBH-1)/maph;
  if (lnga<0) lnga=0;
  if (lngz>=plane->w) lngz=plane->w-1;
  if (lata<0) lata=0;
  if (latz>=plane->h) latz=plane->h-1;
  int viewcolc=lngz-lnga+1;
  int viewrowc=latz-lata+1;
  if ((viewcolc<1)||(viewcolc>2)) return;
  if ((viewrowc<1)||(viewrowc>2)) return;
  
  /* If anything is in (scopev) that's no longer in the covered range, drop it and broadcast.
   * And if it was the primary, report loss of primary first, to keep those calls nested neatly.
   */
  struct scope *scope=g.camera.scopev;
  int i=CAMERA_SCOPE_LIMIT,okc=0;
  for (;i-->0;scope++) {
    struct map *map=scope->map;
    if (!map) continue;
    if (
      (map->z!=g.camera.z)||
      (map->lng<lnga)||
      (map->lng>lngz)||
      (map->lat<lata)||
      (map->lat>latz)
    ) {
      scope->map=0;
      if (map==g.camera.map) {
        g.camera.map=0;
        camera_broadcast_map(map,-1);
      }
      camera_broadcast_map(map,0);
    } else {
      okc++;
    }
  }
  
  /* If any map exists in the plane, within our view, and not registered in scope, register it.
   * As a wee optimization, we've counted the in-scope maps and can compare to expectation.
   */
  if (okc<viewcolc*viewrowc) {
    int lat=lata; for (;lat<=latz;lat++) {
      int lng=lnga; for (;lng<=lngz;lng++) {
        struct map *map=plane->v+lat*plane->w+lng;
        camera_maybe_add_scope(map);
      }
    }
  }
  
  /* Validate primary focus.
   */
  int x=g.camera.rx+(FBW>>1);
  int y=g.camera.ry+(FBH>>1);
  int flng=x/mapw;
  int flat=y/maph;
  struct map *nfocus=0;
  if ((flng>=0)&&(flat>=0)&&(flng<plane->w)&&(flat<plane->h)) {
    nfocus=plane->v+flat*plane->w+flng;
  }
  if (nfocus!=g.camera.map) {
    struct map *ofocus=g.camera.map;
    if (ofocus) {
      g.camera.map=0;
      camera_broadcast_map(ofocus,-1);
    }
    if (g.camera.map=nfocus) {
      camera_broadcast_map(nfocus,2);
    }
  }
  
  /* Check for cell exposures.
   */
  int nl,nr,nt,nb;
  camera_calculate_edges(&nl,&nr,&nt,&nb);
  if (nl!=g.camera.edgel) {
    g.camera.edgel=nl;
    camera_broadcast_cell(nl,nt,1,nb-nt+1);
  }
  if (nr!=g.camera.edger) {
    g.camera.edger=nr;
    camera_broadcast_cell(nr,nt,1,nb-nt+1);
  }
  if (nt!=g.camera.edget) {
    g.camera.edget=nt;
    camera_broadcast_cell(nl,nt,nr-nl+1,1);
  }
  if (nb!=g.camera.edgeb) {
    g.camera.edgeb=nb;
    camera_broadcast_cell(nl,nb,nr-nl+1,1);
  }
}

/* Move to a map on the singleton plane zero.
 * Replaces (fx,fy,scopev,map).
 */
 
static void camera_singleton_map(struct map *map) {
  if (!map) return;
  g.camera.fx=NS_sys_mapw*(map->lng+0.5);
  g.camera.fy=NS_sys_maph*(map->lat+0.5);
  
  // Drop everything from (scopev).
  struct scope *scope=g.camera.scopev;
  int i=CAMERA_SCOPE_LIMIT;
  for (;i-->0;scope++) {
    if (!scope->map) continue;
    if (scope->map==g.camera.map) {
      g.camera.map=0;
      camera_broadcast_map(scope->map,-1);
    }
    struct map *outgoing=scope->map;
    scope->map=0;
    camera_broadcast_map(outgoing,0);
  }
  
  // If we still have a focus, drop it.
  if (g.camera.map) {
    struct map *outgoing=g.camera.map;
    g.camera.map=0;
    camera_broadcast_map(outgoing,-1);
  }
  
  // Add incoming map to (scopev), then principal.
  g.camera.z=map->z;
  g.camera.scopev[0].map=map;
  camera_broadcast_map(map,1);
  g.camera.map=map;
  camera_broadcast_map(map,2);
  g.camera.mapsdirty=1;
}

/* Render current scene to use as the still "from" image of a transition.
 */
 
static void camera_acquire_transition_bits() {
  if (g.camera.transition_texid<1) {
    g.camera.transition_texid=egg_texture_new();
    egg_texture_load_raw(g.camera.transition_texid,FBW,FBH,FBW<<2,0,0);
  }
  camera_render_pretransition(g.camera.transition_texid);
}

/* Update.
 */

void camera_update(double elapsed) {

  /* If a door transition was scheduled, effect it now.
   * Modelwise, we're at the destination map directly a transition begins.
   * Only reason we don't do this during camera_cut() is we want to be outside the sprites' update cycle.
   */
  if (g.camera.door_map) {
    if (g.camera.transition) {
      camera_acquire_transition_bits();
    }
    if (g.camera.door_map->z) {
      g.camera.fx=g.camera.door_x+0.5;
      g.camera.fy=g.camera.door_y+0.5;
      g.camera.z=g.camera.door_map->z;
    } else {
      camera_singleton_map(g.camera.door_map);
    }
    g.camera.cut=1;
    g.camera.lock=0;
    g.camera.door_map=0;
    g.camera.transition_ready=1;
  }
  
  /* If a transition is in progress, advance it.
   */
  if (g.camera.transition_clock<g.camera.transition_time) {
    g.camera.transition_clock+=elapsed;
    if (g.camera.transition_clock>=g.camera.transition_time) {
      g.camera.transition=0;
      g.camera.transition_clock=0.0;
      g.camera.transition_time=0.0;
    }
  }
  
  /* Find the ideal unclamped position.
   * This is generally the hero.
   * If we don't have a hero, it's our most recent position.
   * Includes telescope offset.
   */
  double idealx=g.camera.fx;
  double idealy=g.camera.fy;
  if (g.camera.z==0) {
    // Plane zero consists of unrelated singleton maps.
    // Set the correct position when entering the plane, and we'll never move.
  } else {
    if (GRP(hero)->sprc>=1) {
      struct sprite *hero=GRP(hero)->sprv[0];
      if (hero->z==g.camera.z) { // Just to be sure.
        idealx=hero->x;
        idealy=hero->y;
      }
    }
  }
  idealx+=g.camera.teledx;
  idealy+=g.camera.teledy;
  
  /* Advance (fx,fy) toward (idealx,idealy) at a global speed limit.
   * If we're within a pixel of it, just snap. (bear in mind this is running always).
   * Ditto in cut and lock cases.
   */
  int wascut=g.camera.cut;
  if (g.camera.cut) {
    g.camera.cut=0;
    g.camera.fx=idealx;
    g.camera.fy=idealy;
    g.camera.lock=1;
  } else {
    double dx=idealx-g.camera.fx;
    double dy=idealy-g.camera.fy;
    double d2=dx*dx+dy*dy;
    if (g.camera.lock&&(d2<=CAMERA_LOCK_DISTANCE*CAMERA_LOCK_DISTANCE)) {
      g.camera.fx=idealx;
      g.camera.fy=idealy;
    } else if (d2<=1.0/(double)(NS_sys_tilesize*NS_sys_tilesize)) {
      g.camera.fx=idealx;
      g.camera.fy=idealy;
      g.camera.lock=1;
    } else {
      g.camera.lock=0;
      double speed=CAMERA_PAN_SPEED*elapsed;
      double d=sqrt(d2);
      if (dx<0.0) {
        if ((g.camera.fx+=(dx*speed)/d)<idealx) g.camera.fx=idealx;
      } else if (dx>0.0) {
        if ((g.camera.fx+=(dx*speed)/d)>idealx) g.camera.fx=idealx;
      }
      if (dy<0.0) {
        if ((g.camera.fy+=(dy*speed)/d)<idealy) g.camera.fy=idealy;
      } else if (dy>0.0) {
        if ((g.camera.fy+=(dy*speed)/d)>idealy) g.camera.fy=idealy;
      }
    }
  }
  
  /* Calculate new render position in plane pixels, from (fx,fy).
   * Clamp these to the plane.
   * Or if we're on plane zero, compute them trivially.
   */
  int rx,ry;
  if (g.camera.z==0) {
    struct map *map=g.camera.scopev[0].map;
    if (map) {
      rx=map->lng*NS_sys_mapw*NS_sys_tilesize;
      ry=map->lat*NS_sys_maph*NS_sys_tilesize;
      rx+=((NS_sys_mapw*NS_sys_tilesize)>>1)-(FBW>>1);
      ry+=((NS_sys_maph*NS_sys_tilesize)>>1)-(FBH>>1);
    } else {
      rx=g.camera.rx;
      ry=g.camera.ry;
    }
  } else {
    rx=(int)(g.camera.fx*NS_sys_tilesize)-(FBW>>1);
    ry=(int)(g.camera.fy*NS_sys_tilesize)-(FBH>>1);
    int planew,planeh;
    struct plane *plane=plane_by_position(g.camera.z);
    if (plane) {
      planew=plane->w*NS_sys_mapw*NS_sys_tilesize;
      planeh=plane->h*NS_sys_maph*NS_sys_tilesize;
    } else {
      planew=NS_sys_mapw*NS_sys_tilesize;
      planeh=NS_sys_maph*NS_sys_tilesize;
    }
    if (rx<0) rx=0; else if (rx>planew-FBW) rx=planew-FBW;
    if (ry<0) ry=0; else if (ry>planeh-FBH) ry=planeh-FBH;
  }
  int ox=g.camera.rx;
  int oy=g.camera.ry;
  g.camera.rx=rx;
  g.camera.ry=ry;
  camera_check_exposures(ox,oy);
  
  /* During a spotlight transition, dynamically update the To focus point.
   */
  if (g.camera.transition==NS_transition_spotlight) {
    if (GRP(hero)->sprc>=1) {
      struct sprite *sprite=GRP(hero)->sprv[0];
      g.camera.tox=(int)(sprite->x*NS_sys_tilesize)-g.camera.rx;
      g.camera.toy=(int)(sprite->y*NS_sys_tilesize)-g.camera.ry;
    }
  }
  
  /* Update darkness.
   */
  if (g.camera.map) {
    if (g.camera.map->dark) {
      if (wascut) g.camera.darkness=1.0;
      else if ((g.camera.darkness+=DARK_UP_SPEED*elapsed)>1.0) g.camera.darkness=1.0;
    } else {
      if (wascut) g.camera.darkness=0.0;
      else if ((g.camera.darkness-=DARK_DOWN_SPEED*elapsed)<0.0) g.camera.darkness=0.0;
    }
  }
}

/* Dark overlay sprites.
 * When Dot walks in a dark room there may be sprites above her.
 * But we yoink her and rerender after, because she is supposed to remain visible above the darkness.
 * So what about the overlay bit that partially occludes her?
 * We'll rerender every sprite above layer 100, with a global tint.
 * WARNING: This will go bad if the sprites override graf's tint.
 */
 
static void camera_render_dark_overlay_sprites(int alpha) {
  struct sprite **spritev=GRP(visible)->sprv;
  int spritec=GRP(visible)->sprc;
  int spritep=spritec;
  while ((spritep>0)&&(spritev[spritep-1]->layer>100)) spritep--;
  if (spritep>=spritec) return;
  graf_set_tint(&g.graf,0x00000000|alpha);
  for (;spritep<spritec;spritep++) {
    struct sprite *sprite=spritev[spritep];
    int dstx=(int)(sprite->x*NS_sys_tilesize)-g.camera.rx;
    int dsty=(int)(sprite->y*NS_sys_tilesize)-g.camera.ry;
    if (sprite->type->render) {
      sprite->type->render(sprite,dstx,dsty);
    } else {
      graf_set_image(&g.graf,sprite->imageid);
      graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
    }
  }
  graf_set_tint(&g.graf,0);
}

/* Darkness and light.
 */
 
static void camera_render_darkness() {
  if (g.camera.darkness>0.0) {
    double darkness=g.camera.darkness;
    if (g.flash>0.0) {
      if ((darkness-=g.flash)<0.0) darkness=0.0;
    }
    double nlightness=0.0;
    if (GRP(light)->sprc&&(darkness>0.0)) {
      // g.camera.fx,fy is unclamped; we want a clamped version:
      double fx=(double)(g.camera.rx+(FBW>>1))/(double)NS_sys_tilesize;
      double fy=(double)(g.camera.ry+(FBH>>1))/(double)NS_sys_tilesize;
      double d2max=NS_sys_mapw*0.5;
      d2max*=d2max;
      struct sprite **p=GRP(light)->sprv;
      int i=GRP(light)->sprc;
      for (;i-->0;p++) {
        struct sprite *sprite=*p;
        double dx=sprite->x-fx;
        double dy=sprite->y-fy;
        double d2=dx*dx+dy*dy;
        double r=1.0-d2/d2max;
        r*=0.5; // Tone it down a bit. Manmade light sources never get even close to sunlight.
        if (r<=0.0) continue;
        nlightness+=r;
      }
      // A little periodic up and down too.
      if (nlightness>0.0) {
        nlightness+=sin(((g.framec%50)*M_PI)/50.0)*0.050;
      }
      if (nlightness>1.0) nlightness=1.0;
      else if (nlightness<0.0) nlightness=0.0;
    }
    const double syncspeed=0.050;
    if (g.camera.lightness<nlightness) {
      if ((g.camera.lightness+=syncspeed)>nlightness) g.camera.lightness=nlightness;
    } else if (g.camera.lightness>nlightness) {
      if ((g.camera.lightness-=syncspeed)<nlightness) g.camera.lightness=nlightness;
    }
    if ((darkness-=g.camera.lightness)<0.0) darkness=0.0;
    int alpha=(int)(darkness*255.0);
    if (alpha>0xff) alpha=0xff;
    graf_fill_rect(&g.graf,0,0,FBW,FBH,0x00000000|alpha);
    // Hero renders on top of the darkness.
    if (GRP(hero)->sprc>=1) {
      struct sprite *sprite=GRP(hero)->sprv[0];
      int dstx=(int)(sprite->x*NS_sys_tilesize)-g.camera.rx;
      int dsty=(int)(sprite->y*NS_sys_tilesize)-g.camera.ry;
      if (sprite->type->render) {
        sprite->type->render(sprite,dstx,dsty);
      } else {
        graf_set_image(&g.graf,sprite->imageid);
        graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
      }
      camera_render_dark_overlay_sprites(alpha);
    }
  
  /* Flashing in the light.
   */
  } else if (g.flash>0.0) {
    int alpha=(int)((g.flash*128.0)/FLASH_TIME);
    if (alpha>0) graf_fill_rect(&g.graf,0,0,FBW,FBH,0xfff08000|alpha);
  }
}

/* Render.
 */

void camera_render_pretransition(int dsttexid) {
  
  /* Rerender scoped maps if dirty.
   */
  if (g.camera.mapsdirty) {
    g.camera.mapsdirty=0;
    struct scope *scope=g.camera.scopev;
    int i=CAMERA_SCOPE_LIMIT;
    for (;i-->0;scope++) {
      if (!scope->map) continue;
      camera_render_scope(scope,0);
    }
  }
  
  graf_set_output(&g.graf,dsttexid);
  
  /* Copy all scopes to the main output.
   * We take it on faith that these will cover the whole framebuffer opaquely.
   * Effort is made elsewhere to ensure that none is necessary here.
   */
  struct scope *scope=g.camera.scopev;
  int i=CAMERA_SCOPE_LIMIT;
  for (;i-->0;scope++) {
    if (!scope->map) continue;
    int dstx=scope->map->lng*NS_sys_mapw*NS_sys_tilesize-g.camera.rx;
    int dsty=scope->map->lat*NS_sys_maph*NS_sys_tilesize-g.camera.ry;
    graf_set_input(&g.graf,scope->texid);
    graf_decal(&g.graf,dstx,dsty,0,0,NS_sys_mapw*NS_sys_tilesize,NS_sys_maph*NS_sys_tilesize);
  }
  
  /* Render sprites.
   */
  sprite_group_sort_partial(GRP(visible));
  struct sprite **p=GRP(visible)->sprv;
  for (i=GRP(visible)->sprc;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->defunct) continue; // Defuncts should have been killed by this point, but let's be safe.
    // We'll draw the hero special on top of the darkness.
    // Note that this will also put her on top of anything with a higher layer. Hoping that layering won't get used much.
    if ((sprite->type==&sprite_type_hero)&&(g.camera.darkness>0.0)) continue;
    int dstx=(int)(sprite->x*NS_sys_tilesize)-g.camera.rx;
    int dsty=(int)(sprite->y*NS_sys_tilesize)-g.camera.ry;
    // Could cull offscreen sprites here but I'm not sure it's worth the risk. We don't have the entire planeful or anything.
    if (sprite->type->render) {
      sprite->type->render(sprite,dstx,dsty);
    } else {
      graf_set_image(&g.graf,sprite->imageid);
      graf_tile(&g.graf,dstx,dsty,sprite->tileid,sprite->xform);
    }
  }
  
  /* Darkness and flash.
   */
  if ((g.camera.darkness>0.0)||(g.flash>0.0)) {
    camera_render_darkness();
  }
  
  //TODO Weather.
  
  graf_set_output(&g.graf,1);
}

/* Spotlight transition.
 */
 
static void camera_render_spotlight(double t) {
  
  // Pick the focus point and rephrase (t) as 0..1 = collapsed..expanded.
  int fx,fy,stage;
  if (t<0.5) { // Show From bits with the frame collapsing into the From focus point.
    t=1.0-t*2.0;
    fx=g.camera.fromx;
    fy=g.camera.fromy;
    stage=0;
  } else { // Show To bits with the frame expanding from the To focus point.
    t=(t-0.5)*2.0;
    fx=g.camera.tox;
    fy=g.camera.toy;
    stage=1;
  }
  
  // Slide focus point to the screen's center according to (t).
  int midx=FBW>>1,midy=FBH>>1;
  fx=(int)(fx*(1.0-t)+midx*t);
  fy=(int)(fy*(1.0-t)+midy*t);
  
  // Select bounds. The width and height we slide up to should be about sqrt(2) times the framebuffer size.
  const double ROOT2=1.4142135623730951;
  int maxw=(int)(FBW*ROOT2);
  int maxh=(int)(FBH*ROOT2);
  int cpw=(int)(maxw*t);
  int cph=(int)(maxh*t);
  int cpx=fx-(cpw>>1); //if (cpx<0) cpx=0; else if (cpx>FBW-cpw) cpx=FBW-cpw;
  int cpy=fy-(cph>>1); //if (cpy<0) cpy=0; else if (cpy>FBH-cph) cpy=FBH-cph;
  
  /* In the From stage, black out framebuffer, then copy the focussed bounds from (transition_texid).
   * In the To stage, render the whole scene, then black out the rectangles outside the focus.
   */
  const uint32_t bgcolor=0x000000ff;
  if (stage) {
    camera_render_pretransition(1);
    graf_fill_rect(&g.graf,0,0,cpx,FBH,bgcolor);
    graf_fill_rect(&g.graf,cpx+cpw,0,FBW,FBH,bgcolor);
    graf_fill_rect(&g.graf,cpx,0,cpw,cpy,bgcolor);
    graf_fill_rect(&g.graf,cpx,cpy+cph,cpw,FBH,bgcolor);
  } else {
    graf_fill_rect(&g.graf,0,0,FBW,FBH,bgcolor);
    graf_set_input(&g.graf,g.camera.transition_texid);
    graf_decal(&g.graf,cpx,cpy,cpx,cpy,cpw,cph);
  }
  
  /* Stretch a fuzzy circle cutout with black corners over the focus bounds.
   * I said "circle", but in truth it has a wide aspect. Shouldn't really matter.
   */
  const int srcx=1,srcy=161,srcw=94,srch=46;
  graf_set_image(&g.graf,RID_image_pause);
  graf_set_filter(&g.graf,1);
  graf_triangle_strip_tex_begin(&g.graf,
    cpx    ,cpy    ,srcx     ,srcy,
    cpx+cpw,cpy    ,srcx+srcw,srcy,
    cpx    ,cpy+cph,srcx     ,srcy+srch
  );
  graf_triangle_strip_tex_more(&g.graf,
    cpx+cpw,cpy+cph,srcx+srcw,srcy+srch
  );
  graf_set_filter(&g.graf,0);
}

/* Crossfade transition.
 */
 
static void camera_render_crossfade(double t) {
  camera_render_pretransition(1);
  int alpha=(int)((1.0-t)*255.0);
  if (alpha<1) return;
  if (alpha>0xff) alpha=0xff;
  graf_set_alpha(&g.graf,alpha);
  graf_set_input(&g.graf,g.camera.transition_texid);
  graf_decal(&g.graf,0,0,0,0,FBW,FBH);
  graf_set_alpha(&g.graf,0xff);
}

/* Fade to color transition.
 */
 
static void camera_render_fadevia(double t,uint32_t rgba) {
  // Draw the scene and rephrase (t) as 0..1 = transparent..black.
  if (t<0.5) {
    graf_set_input(&g.graf,g.camera.transition_texid);
    graf_decal(&g.graf,0,0,0,0,FBW,FBH);
    t*=2.0;
  } else {
    camera_render_pretransition(1);
    t=(1.0-t)*2.0;
  }
  // Draw the blackout on top.
  int alpha=(int)(t*255.0);
  if (alpha<1) return;
  if (alpha>0xff) alpha=0xff;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,(rgba&0xffffff00)|alpha);
}

/* Render, main.
 */
 
void camera_render() {

  /* No transition requested or texture invalid, do the normal render.
   */
  if (!g.camera.transition||!g.camera.transition_ready||(g.camera.transition_texid<1)||(g.camera.transition_time<=0.0)) {
    camera_render_pretransition(1);
    return;
  }
  
  /* Normalize transition progress, 0..1 = from..to
   * If it's at or beyond either boundary, take a shortcut.
   */
  double t=g.camera.transition_clock/g.camera.transition_time;
  if (t<=0.0) {
    graf_set_input(&g.graf,g.camera.transition_texid);
    graf_decal(&g.graf,0,0,0,0,FBW,FBH);
    return;
  }
  if (t>=1.0) {
    camera_render_pretransition(1);
    return;
  }
  
  /* Dispatch per transition type.
   * Anything handled should return. If we fall thru, we'll render the default.
   */
  switch (g.camera.transition) {
    case NS_transition_spotlight: camera_render_spotlight(t); return;
    case NS_transition_crossfade: camera_render_crossfade(t); return;
    case NS_transition_fadeblack: camera_render_fadevia(t,0x000000ff); return;
  }
  camera_render_pretransition(1);
}

/* Schedule door transition.
 */

void camera_cut(int mapid,int subcol,int subrow,int transition) {
  struct map *map=map_by_id(mapid);
  if (!map) return;
  g.camera.door_map=map;
  g.camera.door_x=map->lng*NS_sys_mapw+subcol;
  g.camera.door_y=map->lat*NS_sys_maph+subrow;
  g.camera.cut=1;
  g.camera.lock=0;
  g.camera.transition_ready=0;

  g.camera.transition_clock=0.0;
  g.camera.transition_time=CAMERA_TRANSITION_TIME;
  switch (g.camera.transition=transition) {
  
    // Typical transition are good to go.
    case NS_transition_crossfade:
    case NS_transition_fadeblack:
      break;
    
    // Spotlight needs to sample the hero position before and after.
    // We don't know the after point yet, so use the screen's center and trust it will be set properly after the first update.
    // (subcol,subrow) aren't super helpful for determining the after point, since we'd have to account for clamping.
    case NS_transition_spotlight: {
        g.camera.fromx=g.camera.tox=FBW>>1;
        g.camera.tox=g.camera.toy=FBH>>1;
        if (GRP(hero)->sprc>=1) {
          struct sprite *sprite=GRP(hero)->sprv[0];
          g.camera.fromx=(int)(sprite->x*NS_sys_tilesize)-g.camera.rx;
          g.camera.fromy=(int)(sprite->y*NS_sys_tilesize)-g.camera.ry;
        }
      } break;
    
    // Unknown transition, cut instead.
    case NS_transition_cut:
    default: {
        g.camera.transition=NS_transition_cut;
        g.camera.transition_time=0.0;
      }
  }
}

/* Describe transition.
 */
 
int camera_describe_transition() {
  if (!g.camera.transition) return 0;
  if (g.camera.transition_clock>=g.camera.transition_time) return 0;
  // Call out transitions with an occlusive front half.
  if (g.camera.transition_clock<g.camera.transition_time*0.5) {
    switch (g.camera.transition) {
      case NS_transition_spotlight:
      case NS_transition_fadeblack:
        return 2;
    }
  }
  // Everything else, assume we're partially occluded.
  return 1;
}

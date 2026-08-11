#include "game/bellacopia.h"
#include "vellum.h"
#include "jigsaw.h"

static int maps_check_assembly(const struct plane *plane);

// We have room in the closet for 5 puzzles. Try to design the world to need 5 exactly.
static struct puzzle {
  uint8_t z;
  int srcx; // image:pause, srcy is 80, size 48x30
  int present; // MUTABLE.
  int all_collected; // MUTABLE.
  int all_assembled; // MUTABLE.
} puzzlev[]={
  {NS_plane_outerworld,0},
  {NS_plane_tunnel1,48},
  {NS_plane_caves1,96},
  {NS_plane_labyrinth1,144},
  {NS_plane_icepalace,192},
};

// And ok, other components might need that puzzle list too.
int get_puzzle_planes(int *v,int a) {
  int c=sizeof(puzzlev)/sizeof(puzzlev[0]);
  if (c>a) c=a;
  if (c<=0) return 0;
  const struct puzzle *puzzle=puzzlev;
  int i=c;
  for (;i-->0;puzzle++,v++) *v=puzzle->z;
  return c;
}

void vellum_map_reset_cache() {
  struct puzzle *puzzle=puzzlev;
  int i=sizeof(puzzlev)/sizeof(struct puzzle);
  for (;i-->0;puzzle++) {
    puzzle->present=0; // Resets on load anyway, but just for consistency's sake.
    puzzle->all_collected=0;
    puzzle->all_assembled=0;
  }
}

struct vellum_map {
  struct vellum hdr;
  int puzzlep;
  struct jigsaw jigsaw;
  int fldx,fldy; // Track the position of the field at render, so it stays fresh even when we move around.
};

#define VELLUM ((struct vellum_map*)vellum)

/* Delete.
 */
 
static void _map_del(struct vellum *vellum) {
  jigsaw_cleanup(&VELLUM->jigsaw);
}

/* Focus.
 */
 
static void _map_focus(struct vellum *vellum,int focus) {
  if (focus) {
    egg_input_set_mode(EGG_INPUT_MODE_MOUSE);
  } else {
    egg_input_set_mode(EGG_INPUT_MODE_GAMEPAD);
  }
}

/* Switch to another puzzle.
 */
 
static void map_change_puzzle(struct vellum *vellum,int p) {
  if (p<0) return;
  int c=sizeof(puzzlev)/sizeof(puzzlev[0]);
  if (p>=c) return;
  VELLUM->puzzlep=p;
  jigsaw_cleanup(&VELLUM->jigsaw);
  jigsaw_require(&VELLUM->jigsaw,puzzlev[VELLUM->puzzlep].z);
}

/* Mouse down. If we're pointing at some ancillary chrome, like the closet or tabs, do it and return nonzero.
 * Return zero to pass the event on to jigsaw.
 */
 
static int map_check_outer_click(struct vellum *vellum) {
  int x,y;
  if (!egg_input_get_mouse(&x,&y)) return 0;
  if ((x>=VELLUM->fldx)&&(y>=VELLUM->fldy)) return 0; // In the field.
  if (y>=VELLUM->fldy) { // In the closet.
    if (x>=VELLUM->fldx-48) {
      int p=(y-VELLUM->fldy)/30;
      if (p>=0) {
        int i=0,realp=-1;
        int c=sizeof(puzzlev)/sizeof(puzzlev[0]);
        const struct puzzle *puzzle=puzzlev;
        for (;i<c;i++,puzzle++) {
          if (!puzzle->present) continue;
          if (!p--) {
            realp=i;
            break;
          }
        }
        if ((realp>=0)&&(realp!=VELLUM->puzzlep)) {
          map_change_puzzle(vellum,realp);
        }
      }
    }
  } else { // In the tabs.
    modal_pause_click_tabs(vellum->parent,x,y);
  }
  return 1;
}

/* Update animation only.
 */
 
static void _map_updatebg(struct vellum *vellum,double elapsed) {
}

/* A piece was newly connected. Did it finish assembly?
 */
 
static void map_maybe_assembled(struct vellum *vellum) {
  if (VELLUM->puzzlep<0) return;
  struct puzzle *puzzle=puzzlev+VELLUM->puzzlep;
  if (!puzzle->all_collected) return; // Can't finish assembly if we don't have all the pieces.
  if (puzzle->all_assembled) return; // Huh. How did they make a new connection then?
  puzzle->all_assembled=maps_check_assembly(plane_by_position(puzzle->z));
}

/* Update.
 */
 
static void _map_update(struct vellum *vellum,double elapsed) {
  _map_updatebg(vellum,elapsed);
  
  // Note that we use [0] and not [1] like modal_pause: We want Egg's mouse inputs.
  if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) {
    if (!map_check_outer_click(vellum)) jigsaw_grab(&VELLUM->jigsaw);
  } else if (!(g.input[0]&EGG_BTN_SOUTH)&&(g.pvinput[0]&EGG_BTN_SOUTH)) {
    if (jigsaw_release(&VELLUM->jigsaw)) map_maybe_assembled(vellum);
  }
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    if (jigsaw_rotate(&VELLUM->jigsaw)) map_maybe_assembled(vellum);
  }
  
  int x,y;
  if (egg_input_get_mouse(&x,&y)) jigsaw_motion(&VELLUM->jigsaw,x,y);
}

/* Language change.
 */
 
static void _map_langchanged(struct vellum *vellum,int lang) {
}

/* Render the playing field.
 */
 
static void map_render_field(struct vellum *vellum,int x,int y,int w,int h) {
  graf_fill_rect(&g.graf,x,y,w,h,0x00000020);
  jigsaw_set_bounds(&VELLUM->jigsaw,x,y,w,h);
  jigsaw_render(&VELLUM->jigsaw);
}

/* Render the closet: Column on the left showing the available maps.
 */
 
static void map_render_closet(struct vellum *vellum,int x,int y,int w,int h) {
  const int boxw=NS_sys_tilesize*3;
  const int boxh=NS_sys_tilesize*2-2;
  const struct puzzle *puzzle=puzzlev;
  const int puzzlec=sizeof(puzzlev)/sizeof(puzzlev[0]);
  int dsty=y+2;
  int dstx=x+(w>>1)-(boxw>>1);
  int i=0;
  graf_set_image(&g.graf,RID_image_pause);
  for (;i<puzzlec;i++,puzzle++) {
    if (!puzzle->present) continue;
    int srcy=(i==VELLUM->puzzlep)?112:80;
    graf_decal(&g.graf,dstx,dsty,puzzle->srcx,srcy,boxw,boxh);
    if (puzzle->all_assembled) {
      graf_decal(&g.graf,dstx-8,dsty+(boxh>>1)-8,176,0,16,16);
    } else if (puzzle->all_collected) {
      graf_decal(&g.graf,dstx-8,dsty+(boxh>>1)-8,192,0,16,16);
    }
    dsty+=boxh;
  }
}

/* Render.
 */
 
static void _map_render(struct vellum *vellum,int x,int y,int w,int h) {
  int fldy=(h>>1)-(JIGSAW_FLDH>>1);
  int fldx=x+w-fldy-JIGSAW_FLDW;
  fldy+=y;
  map_render_field(vellum,fldx,fldy,JIGSAW_FLDW,JIGSAW_FLDH);
  map_render_closet(vellum,x,y,fldx-x,h);
  VELLUM->fldx=fldx;
  VELLUM->fldy=fldy;
  
  /* We keep Egg in mouse mode, so render a cursor.
   */
  int cursorx=0,cursory=0;
  if (egg_input_get_mouse(&cursorx,&cursory)) {
    graf_set_image(&g.graf,RID_image_pause);
    uint8_t tileid=jigsaw_is_grabbed(&VELLUM->jigsaw)?0x16:0x15;
    graf_tile(&g.graf,cursorx,cursory,tileid,0);
  }
}

/* Scan jigstore to determine whether all pieces of this plane are assembled.
 * Since this is expensive, it's only called when the plane is fully collected, and never again once it goes true.
 */
 
static int maps_check_assembly(const struct plane *plane) {
  if (!plane) return 0;
  int gotc=0;
  int x0,y0; // Undefined until (gotc) true.
  const struct jigstore *jigstore=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;jigstore++) {
    const struct map *map=map_by_id(jigstore->mapid);
    if (!map) continue;
    if (map->z!=plane->z) continue;
    // Puzzles are only considered complete if oriented properly (spares us from needing to compare their orientations).
    if (jigstore->xform) return 0;
    // Don't need to search the plane for it -- maps live inside their plane, arranged LRTB.
    int p=map-plane->v;
    if ((p<0)||(p>=plane->w*plane->h)) continue;
    int col=p%plane->w;
    int row=p/plane->w;
    if (!gotc) { // First map defines the origin.
      x0=jigstore->x-col*NS_sys_mapw;
      y0=jigstore->y-row*NS_sys_maph;
    } else { // All others compare to it.
      if (jigstore->x-col*NS_sys_mapw!=x0) return 0;
      if (jigstore->y-row*NS_sys_maph!=y0) return 0;
    }
    gotc++;
  }
  if (gotc!=plane->w*plane->h) return 0;
  return 1;
}

/* New.
 */
 
struct vellum *vellum_new_map(struct modal *parent) {
  struct vellum *vellum=calloc(1,sizeof(struct vellum_map));
  if (!vellum) return 0;
  vellum->parent=parent;
  vellum->lblstrix=14;
  vellum->del=_map_del;
  vellum->focus=_map_focus;
  vellum->updatebg=_map_updatebg;
  vellum->update=_map_update;
  vellum->render=_map_render;
  vellum->langchanged=_map_langchanged;
  
  /* Set (present) in all puzzles if at least one jigpiece exists for it.
   * And (all_collected,all_assembled) as warranted.
   * Note that we don't need to set these false; once they go true, they're true until end of session.
   */
  #define ZLIMIT 10 /* Greater than the highest (z) value for any puzzle. */
  int present[ZLIMIT]={0}; // Count of jigpieces by plane.
  const struct jigstore *jigstore=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;jigstore++) {
    const struct map *map=map_by_id(jigstore->mapid);
    if (!map) continue;
    if (map->parent&&!(map=map_by_id(map->parent))) continue;
    if (map->z<ZLIMIT) present[map->z]++;
  }
  struct puzzle *puzzle=puzzlev;
  for (i=sizeof(puzzlev)/sizeof(puzzlev[0]);i-->0;puzzle++) {
    puzzle->present=present[puzzle->z]?1:0;
    if (puzzle->present) {
      struct plane *plane=plane_by_position(puzzle->z);
      if (present[puzzle->z]>=plane->w*plane->h) {
        puzzle->all_collected=1;
        if (!puzzle->all_assembled) puzzle->all_assembled=maps_check_assembly(plane);
      }
    }
  }
  #undef ZLIMIT
  
  /* Focus the puzzle where Dot currently is, or its parent.
   * All maps should either belong to a puzzle or have a parent that does.
   * But if we fail in that, no one gets hurt.
   */
  struct map *map=g.camera.map;
  if (map&&map->parent) map=map_by_id(map->parent);
  if (map) {
    int puzzlec=sizeof(puzzlev)/sizeof(puzzlev[0]);
    for (i=0,puzzle=puzzlev;i<puzzlec;i++,puzzle++) {
      if (puzzle->z==map->z) {
        VELLUM->puzzlep=i;
        break;
      }
    }
  }
  
  if (jigsaw_require(&VELLUM->jigsaw,puzzlev[VELLUM->puzzlep].z)<0) {
    vellum_del(vellum);
    return 0;
  }
  
  return vellum;
}

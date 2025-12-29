#include "game/game.h"

#define ARRIVE_SPEED  2.000 /* hz */
#define DISMISS_SPEED 3.000 /* hz */
#define PAGE_SPEED    4.000 /* hz */
#define INV_CURSOR_SPEED 6.000 /* hz */

#define INV_COLC 5
#define INV_ROWC 5

/* Each page is a little smaller than the framebuffer, enough to show the edges of our neighbor pages.
 * Pages' borders are baked into this; they tile at exactly PAGE_W.
 * Must be multiples of 16.
 */
#define PAGE_W 304
#define PAGE_H 160

#define PAGE_INVENTORY 0
#define PAGE_ACHIEVEMENTS 1
#define PAGE_MAP 2
#define PAGE_SYSTEM 3
#define PAGE_COUNT 4

struct modal_pause {
  struct modal hdr;
  int arrival; // <0,0,>0 = dismissing,running,arriving
  double arrivitude; // 0..1 = ready..offscreen
  int pagep; // Updates immediately on page transitions.
  double page_transition; // Moves toward zero. <0 if camera is panning left, >0 if panning right.
  double cursorclock; // For shared animated cursors.
  int cursorframe; // 0..3
  
  // For the Inventory page.
  struct {
    int cfromx,cfromy; // Old cursor position in cells.
    int ctox,ctoy; // True cursor position in cells.
    double cclock; // Counts down while animating (from) to (to). 1..0, not necessarily seconds.
  } inv;
};

#define MODAL ((struct modal_pause*)modal)

/* Cleanup.
 */
 
static void _pause_del(struct modal *modal) {
}

/* Init.
 */
 
static int _pause_init(struct modal *modal) {
  modal->opaque=0;
  modal->interactive=1;
  MODAL->arrival=1;
  MODAL->arrivitude=1.0;
  //TODO Remember the last page we had open, make that part of the persistent storage.
  
  MODAL->inv.ctox=MODAL->inv.cfromx=INV_COLC>>1;
  MODAL->inv.ctoy=MODAL->inv.cfromy=INV_ROWC>>1;
  
  return 0;
}

/* Dismiss.
 */
 
static void pause_dismiss(struct modal *modal) {
  bm_sound(RID_sound_uicancel,0.0);
  MODAL->arrival=-1;
}

/* Swap equipped item with inventory.
 */
 
static void pause_equip_inventory(struct modal *modal) {
  if ((MODAL->inv.ctox<0)||(MODAL->inv.ctox>=INV_COLC)) return;
  if ((MODAL->inv.ctoy<0)||(MODAL->inv.ctoy>=INV_ROWC)) return;
  int invp=MODAL->inv.ctoy*INV_COLC+MODAL->inv.ctox;
  bm_sound(RID_sound_uiactivate,0.0);
  fprintf(stderr,"%s:%d:TODO: Swap equipped item with inventory slot %d/%d\n",__FILE__,__LINE__,invp,INV_COLC*INV_ROWC);
}

/* Activate.
 */
 
static void pause_activate(struct modal *modal) {
  switch (MODAL->pagep) {
    case PAGE_INVENTORY: pause_equip_inventory(modal); break;
    case PAGE_ACHIEVEMENTS: break;//TODO
    case PAGE_MAP: break;//TODO
    case PAGE_SYSTEM: break;//TODO
  }
}

/* Cancel.
 */
 
static void pause_cancel(struct modal *modal) {
  //TODO
}

/* Page left or right.
 */
 
static void pause_page(struct modal *modal,int d) {
  bm_sound(RID_sound_uipage,(d<0)?-0.5:0.5);
  MODAL->pagep+=d;
  if (MODAL->pagep<0) MODAL->pagep=PAGE_COUNT-1;
  else if (MODAL->pagep>=PAGE_COUNT) MODAL->pagep=0;
  MODAL->page_transition+=d;
  
  // If there's a cursor on the new page, position it at the leading edge.
  switch (MODAL->pagep) {
    case PAGE_INVENTORY: {
        MODAL->inv.ctox=MODAL->inv.cfromx=(d<0)?(INV_COLC-1):0;
        MODAL->inv.cclock=0.0;
      } break;
  }
}

/* Regular motion.
 * If cursor leaves the screen horizontally, we should call pause_page().
 */
 
static void pause_motion(struct modal *modal,int dx,int dy) {
  switch (MODAL->pagep) {
  
    case PAGE_INVENTORY: {
        MODAL->inv.cclock=1.0;
        MODAL->inv.cfromx=MODAL->inv.ctox;
        MODAL->inv.cfromy=MODAL->inv.ctoy;
        MODAL->inv.ctox+=dx;
        if (MODAL->inv.ctox<0) {
          MODAL->inv.ctox=0;
          pause_page(modal,-1);
          return;
        }
        if (MODAL->inv.ctox>=INV_COLC) {
          MODAL->inv.ctox=INV_COLC-1;
          pause_page(modal,1);
          return;
        }
        MODAL->inv.ctoy+=dy;
        if (MODAL->inv.ctoy<0) MODAL->inv.ctoy=INV_ROWC-1;
        else if (MODAL->inv.ctoy>=INV_ROWC) MODAL->inv.ctoy=0;
        bm_sound(RID_sound_uimotion,0.0);
      } break;
  }
}

/* Update.
 */
 
static void _pause_update(struct modal *modal,double elapsed) {

  /* If we are arriving or dismissing, update arrivitude.
   * Proceed to normal interaction during arrival, but not dismissal.
   */
  if (MODAL->arrival>0) {
    if ((MODAL->arrivitude-=elapsed*ARRIVE_SPEED)<=0.0) {
      MODAL->arrivitude=0.0;
      MODAL->arrival=0;
    }
  } else if (MODAL->arrival<0) {
    if ((MODAL->arrivitude+=elapsed*DISMISS_SPEED)>=1.0) {
      modal->defunct=1;
    }
    return; // No interaction permitted during dismissal.
  }
  
  /* Apply page transition.
   */
  if (MODAL->page_transition<0.0) {
    if ((MODAL->page_transition+=elapsed*PAGE_SPEED)>=0.0) {
      MODAL->page_transition=0.0;
    }
  } else if (MODAL->page_transition>0.0) {
    if ((MODAL->page_transition-=elapsed*PAGE_SPEED)<=0.0) {
      MODAL->page_transition=0.0;
    }
  }
  
  /* Miscellaneous clocks.
   */
  if (MODAL->inv.cclock>0.0) MODAL->inv.cclock-=elapsed*INV_CURSOR_SPEED;
  if ((MODAL->cursorclock-=elapsed)<0.0) {
    MODAL->cursorclock+=0.200;
    if (++(MODAL->cursorframe)>=4) MODAL->cursorframe=0;
  }

  // Poll input.
  if (g.input[1]!=g.pvinput[1]) {
    if ((g.input[1]&EGG_BTN_SOUTH)&&!(g.pvinput[1]&EGG_BTN_SOUTH)) { pause_activate(modal); return; }
    if ((g.input[1]&EGG_BTN_WEST)&&!(g.pvinput[1]&EGG_BTN_WEST)) { pause_cancel(modal); return; }
    if ((g.input[1]&EGG_BTN_AUX1)&&!(g.pvinput[1]&EGG_BTN_AUX1)) { pause_dismiss(modal); return; }
    if ((g.input[1]&EGG_BTN_LEFT)&&!(g.pvinput[1]&EGG_BTN_LEFT)) pause_motion(modal,-1,0);
    if ((g.input[1]&EGG_BTN_RIGHT)&&!(g.pvinput[1]&EGG_BTN_RIGHT)) pause_motion(modal,1,0);
    if ((g.input[1]&EGG_BTN_UP)&&!(g.pvinput[1]&EGG_BTN_UP)) pause_motion(modal,0,-1);
    if ((g.input[1]&EGG_BTN_DOWN)&&!(g.pvinput[1]&EGG_BTN_DOWN)) pause_motion(modal,0,1);
    if ((g.input[1]&EGG_BTN_L1)&&!(g.pvinput[1]&EGG_BTN_L1)) pause_page(modal,-1);
    if ((g.input[1]&EGG_BTN_R1)&&!(g.pvinput[1]&EGG_BTN_R1)) pause_page(modal,1);
  }
}

/* Helper: Render an animated square cursor with a focus roughly the size of a tile, 16x16.
 */
 
static void pause_render_tile_cursor(struct modal *modal,int x,int y) {
  const int halftile=8;
  graf_set_image(&g.graf,RID_image_pause);
  uint8_t tileid=0x03+MODAL->cursorframe;
  graf_tile(&g.graf,x-halftile,y-halftile,tileid,EGG_XFORM_SWAP|EGG_XFORM_YREV);
  graf_tile(&g.graf,x+halftile,y-halftile,tileid,EGG_XFORM_XREV|EGG_XFORM_YREV);
  graf_tile(&g.graf,x-halftile,y+halftile,tileid,0);
  graf_tile(&g.graf,x+halftile,y+halftile,tileid,EGG_XFORM_SWAP|EGG_XFORM_XREV);
}

/* Helper: Render an unsigned decimal integer at the lower-right corner of some box.
 */
 
static void pause_render_quantity(struct modal *modal,int x,int y,int w,int h,int n,uint32_t rgba) {
  if (n<0) n=0;
  x+=w-3;
  y+=h-4;
  graf_set_image(&g.graf,RID_image_pause);
  graf_fancy(&g.graf,x,y,0x40+n%10,0,0,16,0,rgba);
  for (;;) {
    n/=10;
    if (!n) return;
    x-=4;
    graf_fancy(&g.graf,x,y,0x40+n%10,0,0,16,0,rgba);
  }
}

/* Render content for the inventory page.
 */
 
static void pause_render_page_inventory(struct modal *modal,int x,int y) {
  const int tilesize=16;
  const int colw=20;
  const int rowh=20;
  const int fldw=colw*INV_COLC;
  const int fldh=rowh*INV_ROWC;
  int fldx=x+(PAGE_W>>1)-((fldw+40)>>1); // Center with respect to the equipped-item hand too.
  int fldy=y+(PAGE_H>>1)-(fldh>>1);
  
  // Field background.
  const uint32_t fldcolor=0xbfa97cff;
  graf_fill_rect(&g.graf,fldx,fldy,fldw,fldh,fldcolor);
  graf_set_image(&g.graf,RID_image_pause);
  
  // Cursor.
  int cursorx=fldx+MODAL->inv.ctox*colw+(colw>>1);
  int cursory=fldy+MODAL->inv.ctoy*rowh+(rowh>>1);
  if (MODAL->inv.cclock>0.0) {
    int bx=fldx+MODAL->inv.cfromx*colw+(colw>>1);
    int by=fldy+MODAL->inv.cfromy*rowh+(rowh>>1);
    cursorx=(int)((double)cursorx*(1.0-MODAL->inv.cclock)+(double)bx*MODAL->inv.cclock);
    cursory=(int)((double)cursory*(1.0-MODAL->inv.cclock)+(double)by*MODAL->inv.cclock);
  }
  pause_render_tile_cursor(modal,cursorx,cursory);
  
  // Inventory contents.
  int celly=fldy+(rowh>>1);
  int row=0;
  for (;row<INV_ROWC;row++,celly+=rowh) {
    int cellx=fldx+(colw>>1);
    int col=0;
    for (;col<INV_COLC;col++,cellx+=colw) {
      graf_tile(&g.graf,cellx,celly,0x30+(row*INV_COLC+col)%9,0);//TODO inventory
    }
  }
  
  // Equipped item, with Dot's hand behind it.
  int handx=fldx+fldw+20;
  int handy=fldy+20;
  graf_tile(&g.graf,handx-(tilesize>>1),handy-(tilesize>>1),0x13,0);
  graf_tile(&g.graf,handx+(tilesize>>1),handy-(tilesize>>1),0x14,0);
  graf_tile(&g.graf,handx-(tilesize>>1),handy+(tilesize>>1),0x23,0);
  graf_tile(&g.graf,handx+(tilesize>>1),handy+(tilesize>>1),0x24,0);
  graf_tile(&g.graf,handx,handy,0x30,0);//TODO equipped item
  
  // Quantity for inventory content, where warranted.
  #define QTY(col,row,qty,limit) { \
    uint32_t rgba; \
    if ((qty)<=0) rgba=0x808080ff; \
    else if ((qty)>=(limit)) rgba=0x00ff00ff; \
    else rgba=0x00c080ff; \
    pause_render_quantity(modal,fldx+col*colw,fldy+row*rowh,colw,rowh,qty,rgba); \
  }
  QTY(0,0,0,1)//TODO inventory quantities
  QTY(2,1,9,9)
  QTY(3,1,10,20)
  QTY(4,4,99,99)
  QTY(0,4,12345,12346)
  #undef QTY
}

/* Render content for the achievements page.
 */
 
static void pause_render_page_achievements(struct modal *modal,int x,int y) {
  //TODO
}

/* Render content for the map page.
 */
 
static void pause_render_page_map(struct modal *modal,int x,int y) {
  //TODO
}

/* Render content for the system page.
 */
 
static void pause_render_page_system(struct modal *modal,int x,int y) {
  //TODO
}

/* Render one page.
 * (pagep) may be OOB, we mod it into range.
 * It's normal to draw a page just for a sliver of its horizontal edge; we'll be smart about that.
 * (x,y) are the top-left corner of output in framebuffer pixels; output range is (PAGE_W,PAGE_H).
 */
 
static void pause_render_page(struct modal *modal,int pagep,int x,int y) {
  const int tilesize=16;
  while (pagep<0) pagep+=PAGE_COUNT;
  pagep%=PAGE_COUNT;
  
  /* Fill the background grid.
   * Clamp the horizontal aggressively. We'll routinely be asked to draw way more than needed along the horizontal.
   * Could do the same on the vertical, but I don't think that comes up enough to matter.
   */
  const int colc=(PAGE_W/tilesize);
  const int rowc=(PAGE_H/tilesize);
  int x0=x+(tilesize>>1);
  int y0=y+(tilesize>>1);
  graf_set_image(&g.graf,RID_image_pause);
  int cola=0,colz=colc-1;
  int rowa=0,rowz=rowc-1;
  int xa=x0;
  while ((cola<=colz)&&(xa<-tilesize)) { cola++; xa+=tilesize; }
  int xz=x0+colz*tilesize;
  while ((cola<=colz)&&(xz>=FBW+tilesize)) { colz--; xz-=tilesize; }
  if (cola<=colz) {
    if (!cola) { // Left edge.
      graf_tile(&g.graf,x0,y0,0x00,0);
      int dsty=y0+tilesize;
      int i=rowc-2;
      for (;i-->0;dsty+=tilesize) graf_tile(&g.graf,x0,dsty,0x10,0);
      graf_tile(&g.graf,x0,dsty,0x20,0);
    }
    if (colz==colc-1) { // Right edge.
      graf_tile(&g.graf,xz,y0,0x02,0);
      int dsty=y0+tilesize;
      int i=rowc-2;
      for (;i-->0;dsty+=tilesize) graf_tile(&g.graf,xz,dsty,0x12,0);
      graf_tile(&g.graf,xz,dsty,0x22,0);
    }
    // Middle...
    int mcola=cola; if (!mcola) mcola=1;
    int mcolz=colz; if (mcolz==colc-1) mcolz--;
    int col=mcola;
    int dstx=x0+mcola*tilesize;
    for (;col<=mcolz;col++,dstx+=tilesize) {
      graf_tile(&g.graf,dstx,y0,0x01,0);
      int dsty=y0+tilesize;
      int yi=rowc-2;
      for (;yi-->0;dsty+=tilesize) graf_tile(&g.graf,dstx,dsty,0x11,0);
      graf_tile(&g.graf,dstx,dsty,0x21,0);
    }
  }
  
  /* Pages must endeavour to avoid hard work if only eg 8 horizontal pixels are visible; that will happen a lot.
   */
  switch (pagep) {
    case PAGE_INVENTORY: pause_render_page_inventory(modal,x,y); break;
    case PAGE_ACHIEVEMENTS: pause_render_page_achievements(modal,x,y); break;
    case PAGE_MAP: pause_render_page_map(modal,x,y); break;
    case PAGE_SYSTEM: pause_render_page_system(modal,x,y); break;
  }
}

/* Render.
 */
 
static void _pause_render(struct modal *modal) {

  /* Determine vertical origin, same for all pages.
   * This is the arrive and dismiss sliding motion.
   */
  const int y0=(FBH-PAGE_H)>>1;
  const int yrange=FBH-y0;
  int y=y0;
  if (MODAL->arrivitude>0.0) {
    y=y0+(int)(MODAL->arrivitude*yrange);
  }
  
  /* Determine horizontal position of the focussed page and draw it.
   */
  int x0=(FBW-PAGE_W)>>1;
  x0+=(int)(MODAL->page_transition*PAGE_W);
  pause_render_page(modal,MODAL->pagep,x0,y);
  
  /* Draw neighbors left and right until we go offscreen.
   * Typically there's one neighbor in each direction but during a transition it could be zero, or two, or maybe more.
   */
  int x=x0;
  int pagep=MODAL->pagep;
  for (;;) {
    x-=PAGE_W;
    if (x<=-PAGE_W) break;
    pagep--;
    pause_render_page(modal,pagep,x,y);
  }
  for (x=x0,pagep=MODAL->pagep;;) {
    x+=PAGE_W;
    if (x>=FBW) break;
    pagep++;
    pause_render_page(modal,pagep,x,y);
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_pause={
  .name="pause",
  .objlen=sizeof(struct modal_pause),
  .del=_pause_del,
  .init=_pause_init,
  .update=_pause_update,
  .render=_pause_render,
};

/* Public ctor.
 */
 
struct modal *modal_new_pause() {
  struct modal *modal=modal_new(&modal_type_pause);
  if (!modal) return 0;
  if (modal_push(modal)<0) {
    modal_del(modal);
    return 0;
  }
  //TODO further init?
  return modal;
}

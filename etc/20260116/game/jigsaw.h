/* jigsaw.h
 * Our world maps are little interactive jigsaw puzzles.
 */
 
#ifndef JIGSAW_H
#define JIGSAW_H

struct map;

// All private.
struct jigsaw {
  int ox,oy,ow,oh; // Outer bounds, as delivered to jigsaw_set_bounds. Never more than 256 per axis. (framebuffer pixels)
  int mx,my; // Mouse position in framebuffer pixels.
  int texid; // Tilesheet.
  int texid_outline; // Also tilesheet.
  struct map *mapv;
  int mapstride;
  int colc,rowc; // Logical size of (mapv). (colc) may be less than (mapstride).
  int fx,fy; // Focus map, where the hero is. OOB if hero not found.
  struct jigpiece {
    int x,y;
    uint8_t tileid;
    uint8_t xform;
    uint8_t clusterid; // Pieces with the same nonzero clusterid must move together.
    uint8_t indicator; // Tileid in RID_image_pause if nonzero.
  } *jigpiecev;
  int jigpiecec;
  struct jigpiece *hover; // WEAK
  struct jigpiece *grab; // WEAK
  int cheerclock; // frames (we don't have a regular update)
  uint8_t cheercluster;
  int blinkclock;
};

void jigsaw_cleanup(struct jigsaw *jigsaw);
int jigsaw_require(struct jigsaw *jigsaw);

void jigsaw_set_bounds(struct jigsaw *jigsaw,int x,int y,int w,int h);
void jigsaw_render(struct jigsaw *jigsaw);

int jigsaw_is_grabbed(const struct jigsaw *jigsaw);

void jigsaw_grab(struct jigsaw *jigsaw);
void jigsaw_release(struct jigsaw *jigsaw);
void jigsaw_rotate(struct jigsaw *jigsaw);
void jigsaw_motion(struct jigsaw *jigsaw,int x,int y);

#endif

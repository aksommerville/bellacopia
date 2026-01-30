// 2026-01-30: Quick hack to generate mountain tiles. Might need this again.
 
static struct egg_render_tile tilev[NS_sys_mapw*NS_sys_maph*2];
static int tilec=0;
static int texid=0;
 
void egg_client_render() { // XXX Highly temporary: Generate my mountain tiles.

  // Generate the tiles.
  if (!tilec) {
    const int halftile=NS_sys_tilesize>>1;
    int y=halftile;
    int yi=NS_sys_maph-1; // maph is slightly larger than the framebuffer. We need a crisp bottom, and some dead space below is fine.
    for (;yi-->0;) {
      // One row of column-aligned border-friendly tiles 0x76..0x7e.
      // Also include the oddball tiles 0x6a..0x6c. These should only be interior, but it's cool, we aren't going to keep the whole thing.
      int x=halftile;
      int xi=NS_sys_mapw;
      for (;xi-->0;x+=NS_sys_tilesize) {
        struct egg_render_tile *tile=tilev+tilec++;
        if (rand()%10) { // Border-friendly.
          tile->tileid=0x76+rand()%9;
        } else { // Oddball.
          tile->tileid=0x6a+rand()%3;
        }
        tile->x=x;
        tile->y=y;
        tile->xform=0;
      }
      // Stop here at the bottom row.
      if (!yi) break;
      // Then a row offset by a half-tile, using the mergeable tiles 0x66..0x69.
      y+=halftile;
      for (xi=NS_sys_mapw-1,x=NS_sys_tilesize;xi-->0;x+=NS_sys_tilesize) {
        struct egg_render_tile *tile=tilev+tilec++;
        tile->tileid=0x66+rand()%4;
        tile->x=x;
        tile->y=y;
        tile->xform=0;
      }
      y+=halftile;
    }
  }
  
  // Load the texture.
  if (!texid) egg_texture_load_image(texid=egg_texture_new(),RID_image_mountains);

  // Draw it.
  struct egg_render_uniform un={
    .dsttexid=1,
    .srctexid=texid,
    .mode=EGG_RENDER_TILE,
    .alpha=0xff,
  };
  egg_render(&un,tilev,sizeof(tilev[0])*tilec);
}

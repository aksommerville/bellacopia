#include "bellacopia.h"

/* Initialize if we haven't yet.
 */
 
void surveyor_require() {

  /* If any of the key fields is nonzero, we're initialized.
   * Zero is not legal for any of them.
   * They all get set together, so we only need to check one.
   */
  if (store_get_fld16(NS_fld16_surveyor_a)) return;
  
  /* Collect candidate cells by scanning the underworld plane.
   * In the very non-final maps I have today, there are 1697 candidate cells.
   * Expect something in that ballpark in real life too.
   * That's a bit much for recording all candidates, so instead we'll count, choose, then iterate again to find them.
   * All physically vacant cells are candidates. Safe are not.
   * Ensure that the maps in play do not have any decorative vacant cells that would be damaged by replacing with the surveyor markers.
   */
  int aokc=0,bokc=0,cokc=0;
  struct plane *plane=plane_by_position(NS_plane_tunnel1);
  if (!plane) {
    fprintf(stderr,"%s:PANIC: tunnel1 not found\n",__func__);
    return;
  }
  struct map *map=plane->v;
  int i=plane->w*plane->h;
  for (;i-->0;map++) {
    if (!map->surveyor) continue;
    int okc=0;
    const uint8_t *p=map->v;
    int pi=NS_sys_mapw*NS_sys_maph;
    for (;pi-->0;p++) {
      if (map->physics[*p]==NS_physics_vacant) {
        okc++;
      }
    }
    switch (map->surveyor) {
      case NS_fld16_surveyor_a: aokc+=okc; break;
      case NS_fld16_surveyor_b: bokc+=okc; break;
      case NS_fld16_surveyor_c: cokc+=okc; break;
    }
  }
  if ((aokc<1)||(bokc<1)||(cokc<1)) {
    fprintf(stderr,"%s:PANIC: Candidate counts (a,b,c): %d,%d,%d\n",__func__,aokc,bokc,cokc);
    return;
  }
  
  /* Pick one of each at random, then iterate again to resolve the exact locations.
   * We can stop when all three are written to the store (they're all northish in the plane, and we iterate LRTB. it will terminate early).
   */
  int achoice=rand()%aokc;
  int bchoice=rand()%bokc;
  int cchoice=rand()%cokc;
  int donec=0;
  for (map=plane->v,i=plane->w*plane->h;i-->0;map++) {
    int *choice;
    switch (map->surveyor) {
      case NS_fld16_surveyor_a: choice=&achoice; break;
      case NS_fld16_surveyor_b: choice=&bchoice; break;
      case NS_fld16_surveyor_c: choice=&cchoice; break;
      default: continue;
    }
    const uint8_t *p=map->v;
    int y=0;
    for (;y<NS_sys_maph;y++) {
      int x=0;
      for (;x<NS_sys_mapw;x++,p++) {
        if (map->physics[*p]==NS_physics_vacant) {
          if ((*choice)--==0) {
            store_set_fld16(map->surveyor,((x+map->lng*NS_sys_mapw)<<8)|(y+map->lat*NS_sys_maph));
            if (++donec>=3) return;
          }
        }
      }
    }
  }
}

/* Place remote tile in map if it needs one.
 */
 
void surveyor_apply_map(struct map *map) {
  if (!map->surveyor) return;
  surveyor_require();
  int v=store_get_fld16(map->surveyor);
  int planex=v>>8;
  int planey=v&0xff;
  int col=planex-map->lng*NS_sys_mapw;
  int row=planey-map->lat*NS_sys_maph;
  if ((col<0)||(row<0)||(col>=NS_sys_mapw)||(row>=NS_sys_maph)) return; // It's not on this map.
  uint8_t *p=map->v+row*NS_sys_mapw+col;
  switch (map->surveyor) {
    case NS_fld16_surveyor_a: *p=0x86; break;
    case NS_fld16_surveyor_b: *p=0x87; break;
    case NS_fld16_surveyor_c: *p=0x88; break;
  }
}

/* Examine the globals and update state if it got solved.
 */
 
int surveyor_check() {
  surveyor_require();
  const uint8_t tileid0=0x86; // The "A" tile in image:caves. "B" and "C" are immediately after.
  
  /* Get the actual locations of the three remotes.
   */
  int v;
  v=store_get_fld16(NS_fld16_surveyor_a);
  int ax=v>>8,ay=v&0xff;
  v=store_get_fld16(NS_fld16_surveyor_b);
  int bx=v>>8,by=v&0xff;
  v=store_get_fld16(NS_fld16_surveyor_c);
  int cx=v>>8,cy=v&0xff;
  
  /* Confirm that the currently-focussed map is the local-points map, and get the three locals.
   */
  int arx=0,ary=0,brx=0,bry=0,crx=0,cry=0;
  const struct map *map=g.camera.map;
  if (map) {
    const uint8_t *p=map->v;
    int y=0; for (;y<NS_sys_maph;y++) {
      int x=0; for (;x<NS_sys_mapw;x++,p++) {
        if (*p==tileid0) { arx=x; ary=y; }
        else if (*p==tileid0+1) { brx=x; bry=y; }
        else if (*p==tileid0+2) { crx=x; cry=y; }
      }
    }
  }
  if (!arx||!brx||!crx) {
    fprintf(stderr,"%s:%d: Expected 0x%02x et al on map:%d, didn't find.\n",__FILE__,__LINE__,tileid0,map?map->rid:0);
    // Poison the reference values with an impossible position. But not so big that squaring it overflows int.
    arx=ary=brx=bry=crx=cry=9999;
  }
  arx+=map->lng*NS_sys_mapw;
  ary+=map->lat*NS_sys_maph;
  brx+=map->lng*NS_sys_mapw;
  bry+=map->lat*NS_sys_maph;
  crx+=map->lng*NS_sys_mapw;
  cry+=map->lat*NS_sys_maph;
  
  /* Take the true distance for all three pairs, rounded to int.
   */
  int ad,bd,cd;
  #define SURVEY(letter) { \
    double dx=(letter##x-letter##rx); \
    double dy=(letter##y-letter##ry); \
    double distance=sqrt(dx*dx+dy*dy); \
    letter##d=lround(distance); \
    fprintf(stderr,#letter" ref=%d,%d remote=%d,%d distance=%d guess=%d\n",letter##rx,letter##ry,letter##x,letter##y,letter##d,store_get_fld16(NS_fld16_surveyor_##letter##_guess)); \
  }
  SURVEY(a)
  SURVEY(b)
  SURVEY(c)
  #undef SURVEY
  
  /* Compare guesses to true distances, with some integer tolerance.
   */
  const int tolerance=1;
  int ok=1;
  int d;
  d=store_get_fld16(NS_fld16_surveyor_a_guess)-ad;
  if ((d<-tolerance)||(d>tolerance)) ok=0;
  d=store_get_fld16(NS_fld16_surveyor_b_guess)-bd;
  if ((d<-tolerance)||(d>tolerance)) ok=0;
  d=store_get_fld16(NS_fld16_surveyor_c_guess)-cd;
  if ((d<-tolerance)||(d>tolerance)) ok=0;
  
  /* And finally, did our "ok" state change?
   */
  int pvok=store_get_fld(NS_fld_surveyor_complete);
  if (ok&&!pvok) { // Solved!
    bm_sound(RID_sound_secret);
    store_set_fld(NS_fld_surveyor_complete,1);
    g.camera.mapsdirty=1;
    return 2;
  } else if (ok) { // Still solved!
    return 1;
  } else if (!pvok) { // Still unsolved.
    return 0;
  } else { // Was solved, but user, what on earth are you doing?
    store_set_fld(NS_fld_surveyor_complete,0);
    g.camera.mapsdirty=1;
    return -1;
  }
  return 0;
}

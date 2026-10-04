#include "savetest.h"

struct g g={0};

/* A saved game with no progress and like 5 seconds on the clock.
 */
static const char *old_format_fresh="AAAFACAAAAAAAAADAADAAAABjAAAiLAAAflALb4H";

/* 2026-10-03: A natural 100% completion.
 * Picture appending this whole thing to a URL. It's absurd.
 */
static const char *old_format_100pct=
  "A0AqAFEuAa4f8/H+/hf//////29//f////////////////GYD4P/8/+/3/9doBAAAAAHA"
  "AHABtAGPAAQAACAASAAAApXBv+AAAPSdAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABZjBYBB"
  "Q/Bj2BXqBhrAAAAAGHsHJIpMAgABdABFABpAAAAGsBg/CEbCUJAQXuRAFOm3AIoNEAPR1"
  "ZAeMFrADIMQANINwAQ+MQAO+NwBA0NwBg0MQAS+KwJBLtYAFIKwAHSKwB3IHwA1SHwAzS"
  "JQB1IJQIZLqYILLo4INVo4IXVqYCdcHwBCqNwBiqMQBsqKwBcqJQBe0KwAU0JQBy+JQB4"
  "+HwAq0HwAs0GQB6+GQCjcMQAJSMQClcNwC5SPQC3IPQDHIQwCy0PQAu0EwAw0DQAoqGQA"
  "WqHwBagHwBugJQBqgKwCwqPQC0+PQBkgMQBEgNwAigEwAggDQAkqDQA++DQA9IDQB/IEw"
  "A7SDQA5SEwA3SGQCbcGQB9IGQCDcDQCZcEwCHmEwCJmGQCfcJQCNmJQCPmKwChcKwDE+Q"
  "wJg3wYJ23x4JetwYJcjwYJaZwYHaPwYHcPx4J8Zx4J6jx4J4tx4DAqQwDC0QwCugPQC+g"
  "QwCsWPQC6MQwC8WQwDJSQwCpcQwCXmQwCVmPQCTmNwCFmDQCA+EwAmqEwAYgGQAaWEwAe"
  "WDQAcMDQBUMEwBSMGQBQMHwBOMJQBMMKwBwWJQBoWKwBYWHwGJYvIGZOvIGHYtoGdOsIF"
  "/OpIGBYpIGDYqoGFYsIGfOqoGjEsIGbOtoGXEvIGlEtoGm6toGU6vIGwwtoGSwvIGQmvI"
  "GOmtoGMmsIGuwsIGKmqoF2mpIF4wpIGq6qoF66pIGswqoGo6sIF9EpIGhEqoBWWGQKI/N"
  "IKK/LoKM/KIKi1LoKmrNIKk1NIKorLoKqrKIEjVJAEhLJAEfBJAERBKgEJLMAEPLKgELV"
  "MAD/VNgD9LNgD1BNgD23NgD4tNgD6jNgEAjMAEWjKgEYjJAEatJAEc3JAES3KgEE3MAEU"
  "tKgECtMAENVKgCRmMQBGWNwBIMNwBKMMQHWPtYI2ZtYIkZr4Iotr4IetqYIc3qYImjr4I"
  "gjqYICjo4IAZo4IiZqYH+ZnYDQZl4Hejl4Hgtl4H6tnYIEto4H8jnYHUPr4HSPqYDWPo4"
  "DUPnYDSPl4I4jtYI6ttYI83tYJS3u4JUtu4JWju4JYZu4HYPu4JRBu4JPLu4JnVwYJvfx"
  "4JpfwYJrpwYJtpx4JNVu4JDVtYJFftYJxVx4JzLx4JlLwYJjBwYJ1Bx4CncPQJLfu4JJp"
  "u4JHptYI1pr4ITpqYIRpo4HvpnYHxfnYI/BtYItBr4Iq3r4IG3o4H43nYIvLr4IxVr4Iz"
  "fr4IVfqYIPfo4HzVnYH1LnYH3BnYIJBo4IbBqYALSNwCLmHwBmWMQCqMPQHCjkYHEtkYH"
  "G3kYHJBkYDOZkYDMPkYHLLkYHi3l4HNVkYHnLl4HlBl4HpVl4HPfkYHRpkYHtpl4KU1KI"
  "KW1IoKYrIoKarHIKerFoKg1FoKc1HIK5JIoK7JHIK9JFoLBTFoK/THILDTIoKtJKIKvJL"
  "oK3TKIKzTLoK1TNIKxJNIKO/IoKQ/HIKS/FoEHBMAEq3HgEstHgEujHgEwjGAFGjEgEyt"
  "GAEpBHgFEtEgFC3EgE3BGAFBBEgE5LGAE/LEgE03GAEnLHgElVHgE9VEgE7VGAHrfl4IA"
  "AAFAAAHwoKDAoKBwoKCgoKDQAAJQAAAwAABgAAAgAACwAAJwAABGMiGQoEJAAAIQAACAM"
  "DJgAAGAAAIgAAAQAACQAAIwAAFwAABQAArTqZt";
  
/* The 100% clear scenario, but all maps toggled off and on, so they aren't connected anymore.
 * This should be the worst-case scenario for the new format. (worst realistic case at least).
 */
static const char *worst_case_scenario=
  "A0AqAFEuAa4f8/H+/hf//////29//f////////////////GYD4P/8/+/3/9doBAAAAAHAAHAB5AGPAAQAACAASAAAApXBv+AAAPSdAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
  "BZjBYBBQ/Bj2BXqBhrAAAAAGHsHJIpMAgABdABFABpAAAAGsBg/CEbCUJAQd3HAFPuDAJC3EAPR1ZAeMFrAcUo4AfQrtAhcDAAkLvwAxwwWA+PKYA86joA67qdCD0QoCEqo"
  "YCGGh1CYMJVA5WDrB/JozCBchNAvSvLAmQEgAiuS2Aa3joBUnlQBSPN4BWMGrAY3PQAo0tbAtLGzB6kxoB9lwNA2ertCaQMVCJCmYCKINACdYNAA0qOLB2qp7B5MH+ArGAL"
  "AXKvQBbiyeBZinoBQ+CgBPlsQBw9JIBujjWBc+gNAVwquBzCEbB10QmAy1u1Ce6BuCNeqWCPrAzChou2AGMslAEtFmASkJDBewFLBsENIBqSimBpNiFBMyCLBKcnwBmQmOB"
  "lmLWBi6w1BhAsYAQim2ACvq4AJTyYCiHIVCQVlICS8lVCkeNlALZMrAMOStAP0O1BA3JmBDtjmBFtxTBGkGtBJfu+CrmGbCsXx7CuBylCwjTDCzSiGC0LL4C2nnzC5AlQCm"
  "6vTCUUMACW1LzCoiQjDIYDGDGosuDCbMuDE1xGDAuGrC+hjDC8KjzC6jRlDNoQrDPbplHCfF1HFxjLHHEvdHIhiVHKMDTHMDxwHO5MIHQawOHteA+Hq/uTHo/OQHmQJIHkz"
  "owHjDL1HgDLYHehh+DRFtDDSKQYDUkuAH/FLFH9KglH7eQIH4BvVH2CgTH0+yVHyepFHxbPrHuCCoIQADGIOrEdIMhFYIKKO1IIULrIHaQdIDaJOIEoG1IAzPeDX1KlHTEm"
  "gIi+KdIhjKjIedoGIdlKuIbADOIYKDjIWDxIIVRBGITOn4I0tAVIyGCuIw4uNIvdD2ItGmLIqRJzIoRKeInyK+IlNQdHVcsYHZfjFHWmhYI2djoI5uRrI6ZF1I9wHWI+XPL"
  "JAEMWJCaFLJFaM9JHMgoJIVqoJLtDIJMZrYJO6nFJQ/NWJTGqNJVbrDJW6KmJZJl+HaJRuJaBBDJcOCGJfgkmJhKx1JiDGeJlXy9Jm0J4JpRy2JrXjVJsMPFJviBjJxzAbJ"
  "ytDLJ1Uk2J2rDwJ5qSLJ7mStJ88jGHdERjFG3utFFcrGFDXEAFAyBwE/Gp9E8tKOE7WSzE5imeE29p1E0ZJ7Ey6hFExhotEujMoEs0tAErVLGEo1o+EmryrEkCt2Ejyr1Eg"
  "TmOEfUnDEdZxOEanBrEYuBFEWbMDEVhPIESjyLERtL2EPmguENkrAELequEIJl2EGTBIEFUrLECZpLEBaC7D7moOD5qKdD2RHOD1RJwD9WJ9D+TDYGAAqFF/rGQF8XqbF7s"
  "LrF4XlIF2RxVGKoCgGtZKFGrEC9GgSPAGfFSoGDJDLGEQJYGdCD2GjVKOGoLtTGuQAeGMTrlGOkI2GxCBYGmMjeGkAS+GaKp1GG3oQGIKCNGYPxLGWzFFGVUkFGSKGIGRik"
  "WKezP1KgHNAKTsSOK8oyoLAljLK/rN1K60rmKQxR7KcXRmKbhN1KYTv+KXNCVKPEQLK4Jy+LCJGOK2yk1KtXBmKMQOrKVEnTKqfu4KoyM2KjzQbKKbw2KvTi9Ky6N2K0aLt"
  "KwYGOKJewQKlGhwKmXOmAgAAFAAAHwoKDAoKBwoKCgoKDQAAJQAAAwAABgAAIAAACwAAJwAABGMiGQoEJAAAIQAACAMDJgAAGAAAIgAAAQAACQAAIwAAFwAABQAAlxtgQ";
  
/* Compare two jigstore lists.
 * We require exactly the same content in both, but the orders are allowed to be mixed up.
 */
 
const struct jigstore *jigstorev_search(const struct jigstore *j,int c,int mapid) {
  for (;c-->0;j++) if (j->mapid==mapid) return j;
  return 0;
}
 
static int store_jigstorev_equivalent(const struct store *a,const struct store *b) {
  if (a->jigstorec!=b->jigstorec) {
    fprintf(stderr,"jigstore length mismatch %d => %d\n",a->jigstorec,b->jigstorec);
    return 0;
  }
  // Call it ok if we find all of (a)'s in (b).
  const struct jigstore *aj=a->jigstorev;
  int i=a->jigstorec;
  for (;i-->0;aj++) {
    if (!jigstorev_search(b->jigstorev,b->jigstorec,aj->mapid)) return 0;
  }
  // And one more thing, just to be certain: Confirm all mapid in (a) are unique.
  for (i=a->jigstorec;i-->0;) {
    if (jigstorev_search(a->jigstorev,i,a->jigstorev[i].mapid)) return 0;
  }
  return 1;
}
  
/* Compare two live stores. Fail if not identical.
 */
 
static int compare_stores(const struct store *pv,const struct store *nx) {

  if (pv->fldc!=nx->fldc) {
    fprintf(stderr,"fldv length mismatch %d => %d\n",pv->fldc,nx->fldc);
    return -1;
  }
  if (memcmp(pv->fldv,nx->fldv,pv->fldc)) {
    fprintf(stderr,"fldv content mismatch\n");
    const uint8_t *v; int i;
    fprintf(stderr,"pv:"); for (v=pv->fldv,i=pv->fldc;i-->0;v++) fprintf(stderr," %02x",*v); fprintf(stderr,"\n");
    fprintf(stderr,"nx:"); for (v=nx->fldv,i=nx->fldc;i-->0;v++) fprintf(stderr," %02x",*v); fprintf(stderr,"\n");
    return -1;
  }
  
  if (pv->fld16c!=nx->fld16c) {
    fprintf(stderr,"fld16v length mismatch %d => %d\n",pv->fld16c,nx->fld16c);
    return -1;
  }
  if (memcmp(pv->fld16v,nx->fld16v,pv->fld16c*2)) {
    fprintf(stderr,"fld16v content mismatch\n");
    const uint16_t *v; int i;
    fprintf(stderr,"pv:"); for (v=pv->fld16v,i=pv->fld16c;i-->0;v++) fprintf(stderr," %04x",*v); fprintf(stderr,"\n");
    fprintf(stderr,"nx:"); for (v=nx->fld16v,i=nx->fld16c;i-->0;v++) fprintf(stderr," %04x",*v); fprintf(stderr,"\n");
    return -1;
  }
  
  if (pv->clockc!=nx->clockc) {
    fprintf(stderr,"clockv length mismatch %d => %d\n",pv->clockc,nx->clockc);
    return -1;
  }
  if (memcmp(pv->clockv,nx->clockv,pv->clockc*sizeof(double))) {
    fprintf(stderr,"clockv content mismatch\n");
    return -1;
  }
  
  if (!store_jigstorev_equivalent(pv,nx)) {
    fprintf(stderr,"jigstorev mismatch\n");
    return -1;
  }
  
  if (memcmp(pv->invstorev,nx->invstorev,sizeof(pv->invstorev))) {
    fprintf(stderr,"invstorev content mismatch\n");
    const struct invstore *v; int i;
    fprintf(stderr,"pv: "); for (v=pv->invstorev,i=INVSTORE_SIZE;i-->0;v++) fprintf(stderr," %02x,%02x,%02x",v->itemid,v->limit,v->quantity); fprintf(stderr,"\n");
    fprintf(stderr,"nx: "); for (v=nx->invstorev,i=INVSTORE_SIZE;i-->0;v++) fprintf(stderr," %02x,%02x,%02x",v->itemid,v->limit,v->quantity); fprintf(stderr,"\n");
    return -1;
  }
  
  return 0;
}

/* Decode fldv.
 */
 
// We write out the bits, but we don't change (store->fldc). Caller should do that once at the end.
static int store_add_fldv_run(struct store *store,int fldid,int v,int runlen) {
  
  // Overestimate the size of the addition a little, grow enough buffer.
  int reqbytes=(fldid+runlen+7)>>3;
  if (reqbytes>store->flda) {
    int na=(reqbytes+64)&~63;
    uint8_t *nv=realloc(store->fldv,na);
    if (!nv) return -1;
    memset(nv+store->flda,0,na-store->flda); // Initialize new memory to zero.
    store->fldv=nv;
    store->flda=na;
  }
  
  // If the value is zero, great, we're done.
  if (!v) return 0;
  
  // Write out bits. One at a time, to preserve my sanity. Could be done more efficiently.
  while (runlen-->0) {
    store->fldv[fldid>>3]|=(1<<(fldid&7));
    fldid++;
  }
  
  return 0;
}
 
static int decode_new_fldv(struct store *store,const char *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  int v=0,fldid=0;
  for (;srcc-->0;src++) {
    int intake=store_decode_base64_digit(*src);
    if (intake<0) return -1;
    int runlen=intake&7;
    if (store_add_fldv_run(store,fldid,v,runlen)<0) return -1;
    fldid+=runlen;
    if (runlen!=7) v^=1;
    runlen=intake>>3;
    if (store_add_fldv_run(store,fldid,v,runlen)<0) return -1;
    fldid+=runlen;
    if (runlen!=7) v^=1;
  }
  store->fldc=(fldid+7)>>3;
  return 0;
}

/* Decode fld16v.
 */
 
static int decode_new_fld16v(struct store *store,const char *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  int fld16id=0,srcp=0;
  while (srcp<srcc) {
  
    if (store->fld16c>=store->fld16a) {
      int na=store->fld16a+64;
      if (na>INT_MAX/sizeof(uint16_t)) return -1;
      void *nv=realloc(store->fld16v,na*sizeof(uint16_t));
      if (!nv) return -1;
      store->fld16v=nv;
      store->fld16a=na;
    }
    
    int intake=store_decode_base64_digit(src[srcp++]);
    if (intake<0) return -1;
    int v=intake&0x1f;
    if (intake&0x20) {
      if (srcp>=srcc) return -1;
      intake=store_decode_base64_digit(src[srcp++]);
      if (intake<0) return -1;
      v<<=5;
      v|=intake&0x1f;
      if (intake&0x20) {
        if (srcp>=srcc) return -1;
        intake=store_decode_base64_digit(src[srcp++]);
        if (intake<0) return -1;
        v<<=6;
        v|=intake;
      }
    }
    
    store->fld16v[store->fld16c++]=v;
  }
  return 0;
}

/* Decode clockv.
 */
 
static int decode_new_clockv(struct store *store,const char *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  
  // Fixed length units, so allocate in advance.
  store->clockc=srcc/5;
  if (store->clockc>store->clocka) {
    void *nv=realloc(store->clockv,sizeof(double)*store->clockc);
    if (!nv) { store->clockc=0; return -1; }
    store->clockv=nv;
    store->clocka=store->clockc;
  }
  
  int i=0;
  for (;i<store->clockc;i++,src+=5) {
    int ms=store_decode_30bit(src,5);
    if (ms<0) return -1;
    store->clockv[i]=(double)ms/1000.0;
  }
  return 0;
}

/* Decode jigstorev.
 */
 
static int store_append_jigstore(struct store *store,int mapid,int x,int y,int xform) {
  //fprintf(stderr,"%s mapid=%d @%d,%d xform=%d\n",__func__,mapid,x,y,xform);
  if ((mapid<1)||(mapid>0xffff)) return -1;
  if ((x<0)||(x>0xff)) return -1;
  if ((y<0)||(y>0xff)) return -1;
  if (store->jigstorec>=store->jigstorea) {
    int na=store->jigstorea+64;
    if (na>INT_MAX/sizeof(struct jigstore)) return -1;
    void *nv=realloc(store->jigstorev,sizeof(struct jigstore)*na);
    if (!nv) return -1;
    store->jigstorev=nv;
    store->jigstorea=na;
  }
  struct jigstore *jigstore=store->jigstorev+store->jigstorec++;
  jigstore->mapid=mapid;
  jigstore->x=x;
  jigstore->y=y;
  jigstore->xform=xform;
  return 0;
}

// Fill (nx,ny) with the position of (nmapid), known from position of (mapid).
static int store_infer_jigstore_position(
  int *nx,int *ny,
  const struct store *store,
  const struct plane *plane,int lng,int lat,
  int mapid,int x,int y,int xform,
  int nmapid
) {
  int nlng,nlat;
  if (!plane_contains_mapid(&nlng,&nlat,plane,nmapid)) return -1; // Can't infer position of something on a different plane!
  int dlng=nlng-lng;
  int dlat=nlat-lat;
  switch (xform) {
    case 0: { // Natural orientation.
        *nx=x+dlng*MAPW;
        *ny=y+dlat*MAPH;
      } return 0;
    case 6: { // Clockwise.
        *nx=x-dlat*MAPH;
        *ny=y+dlng*MAPW;
      } return 0;
    case 3: { // Half turn.
        *nx=x-dlng*MAPW;
        *ny=y-dlat*MAPH;
      } return 0;
    case 5: { // Deasil.
        *nx=x+dlat*MAPH;
        *ny=y-dlng*MAPW;
      } return 0;
  }
  return -1;
}
 
static int decode_new_jigstorev(struct store *store,const char *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  /* jigstorev: Five encoded bytes each, split big-endianly: 11 mapid, 8 x, 8 y, 3 xform.
   * Our addition: If (mapid) is zero, the remainder discusses maps relative to the previous entry.
   *   16:zero, 7:sequentialc, 7:namedc
   * Then for (sequentialc), add pieces at the expected places with ids increasing from this one.
   * Then for (namedc), read two bytes each for (mapid: 0..4095) and place relative to this one.
   */
  int pvmapid=0,pvx,pvy,pvxform;
  int srcp=0;
  while (srcp<srcc) {
  
    int intake=store_decode_30bit(src+srcp,srcc-srcp);
    if (intake<0) return -1;
    srcp+=5;
    int mapid=intake>>19;
    int x=(intake>>11)&0xff;
    int y=(intake>>3)&0xff;
    int xform=intake&7;
    
    if (mapid) { // Single piece.
      if (store_append_jigstore(store,mapid,x,y,xform)<0) return -1;
      pvmapid=mapid;
      pvx=x;
      pvy=y;
      pvxform=pvxform;
      continue;
    }
    
    // Repetition from the previous piece.
    if (!pvmapid) return -1; // Can't start with a "repeat previous"!
    mapid=pvmapid;
    if (intake&~0x3fff) return -1; // Top 16 bits must all be zero.
    int sequentialc=(intake>>7)&0x7f;
    int namedc=intake&0x7f;
    int lng,lat;
    const struct plane *plane=plane_for_mapid(&lng,&lat,mapid);
    //fprintf(stderr,"jigstore repeat sequentialc=%d namedc=%d plane=%p coord=%d,%d\n",sequentialc,namedc,plane,lng,lat);
    if (!plane) return -1;
    
    while (sequentialc-->0) {
      pvmapid++;
      int nx,ny;
      if (store_infer_jigstore_position(&nx,&ny,store,plane,lng,lat,mapid,pvx,pvy,pvxform,pvmapid)<0) return -1;
      if (store_append_jigstore(store,pvmapid,nx,ny,xform)<0) return -1;
    }
    
    while (namedc-->0) {
      int nmapid=store_decode_12bit(src+srcp,srcc-srcp);
      if (nmapid<0) return -1;
      srcp+=2;
      int nx,ny;
      if (store_infer_jigstore_position(&nx,&ny,store,plane,lng,lat,mapid,pvx,pvy,pvxform,nmapid)<0) return -1;
      if (store_append_jigstore(store,nmapid,nx,ny,xform)<0) return -1;
    }
    
    pvmapid=0; // One repeat per explicit.
  }
  return 0;
}

/* Decode invstorev.
 */
 
static int decode_new_invstorev(struct store *store,const char *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  /* Read one base64 character. If it's under 63, it's itemid, and limit and quantity are zero.
   * If it's 63 exactly, read four more characters, producing 3 bytes, just as before.
   */
  struct invstore *dst=store->invstorev;
  memset(dst,0,sizeof(struct invstore)*INVSTORE_SIZE);
  int i=INVSTORE_SIZE;
  int srcp=0;
  for (;i-->0;dst++) {
    if (srcp>=srcc) break;
    int v=store_decode_base64_digit(src[srcp++]);
    if (v<0) return -1;
    if (v==63) { // Long form.
      v=store_decode_24bit(src+srcp,srcc-srcp);
      if (v<0) return -1;
      srcp+=4;
      dst->itemid=v>>16;
      dst->limit=v>>8;
      dst->quantity=v;
    } else { // Short form.
      dst->itemid=v;
    }
  }
  return 0;
}
  
/* Decode new format.
 */
 
static int decode_new(struct store *store,const char *src,int srcc) {
  fprintf(stderr,"%s srcc=%d\n",__func__,srcc);
  
  /* Must begin with a signature "/".
   */
  if (!src||(srcc<1)||(src[0]!='/')) return -1;
  int srcp=1;
  
  /* Followed by 5 heaps which all have a 2-character length prefix in bytes.
   */
  #define HEAP(name) { \
    int len=store_decode_12bit(src+srcp,srcc-srcp); \
    if (len<0) return -1; \
    srcp+=2; \
    if (srcp>srcc-len) return -1; \
    if (decode_new_##name(store,src+srcp,len)<0) return -1; \
    srcp+=len; \
  }
  HEAP(fldv)
  HEAP(fld16v)
  HEAP(clockv)
  HEAP(jigstorev)
  HEAP(invstorev)
  #undef HEAP
  
  /* Followed by a checksum against the prior encoded content.
   */
  if (srcp!=srcc-5) return -1;
  int expect=store_decode_30bit(src+srcp,srcc-srcp);
  int actual=store_checksum(src,srcp);
  if (expect!=actual) {
    fprintf(stderr,"%s: checksum mismatch. expect=0x%08x actual=0x%08x\n",__func__,expect,actual);
    return -1;
  }
  
  fprintf(stderr,"...%s ok\n",__func__);
  return 0;
}

/* Encode new format.
 */
 
static int encode_new(char *dst,int dsta,const struct store *store) {
  fprintf(stderr,"%s\n",__func__);
  
  int dstc=0;
  #define APPEND(v) { \
    int _v=(v); \
    if ((_v<0)||(_v>63)) return -1; \
    if (dstc<dsta) dst[dstc]=store_base64_alphabet[_v]; \
    dstc++; \
  }
  
  /* Start with "/" to identify the new format.
   * If that appeared in an old-format store, there would be at least 4032 bytes of field (24192 fields).
   * We'll surely never reach that many; there's 302 today and we're more than half done.
   */
  APPEND(63)
  
  /* Then two bytes for the encoded length of (fldv).
   * Fill in after encoding.
   */
  int fldvlenp=dstc;
  APPEND(0)
  APPEND(0)
  
  /* (fldv) encodes as a fairly simple RLE strategy.
   * Stream bits little-endianly from (fldv). The first bit must be zero.
   * Encode only run lengths: We already know the value.
   * Run lengths encode in 3-bit units, so two units per encoded character.
   * After a run of 7, do not toggle the expected value.
   * So we have a worst case of 3/1, and a best case approaching 3/7.
   */
  {
    int dstbuf=0; // 0..63
    int dstshift=0; // 0|3
    const uint8_t *src=store->fldv;
    int srcc=store->fldc;
    uint8_t srcmask=0x01;
    int srcv=0;
    for (;;) {
      // Count the next run, consuming at least one bit unless EOF.
      int runlen=0;
      while (srcc>0) {
        runlen++;
        if (srcmask==0x80) {
          src++;
          srcc--;
          srcmask=0x01;
        } else {
          srcmask<<=1;
        }
        int nv=((*src)&srcmask)?1:0;
        if (nv!=srcv) break;
      }
      if (!runlen) break; // End of input.
      srcv=srcv?0:1;
      // Emit an extender (7) for every seven bits.
      int extc=runlen/7;
      while (extc-->0) {
        dstbuf|=dstshift?0x38:0x07;
        if (dstshift) {
          APPEND(dstbuf)
          dstbuf=0;
          dstshift=0;
        } else {
          dstshift=3;
        }
      }
      // Emit whatever's left in runlen (possibly zero if extended).
      dstbuf|=(runlen%7)<<dstshift;
      if (dstshift) {
        APPEND(dstbuf)
        dstbuf=0;
        dstshift=0;
      } else {
        dstshift=3;
      }
    }
    // And then fill in the length.
    int len=dstc-(fldvlenp+2);
    fprintf(stderr,"fldv len, new format: %d\n",len);
    if ((len<0)||(len>0xfff)) return -1; // oops
    if (fldvlenp<=dsta-2) {
      dst[fldvlenp]=store_base64_alphabet[len>>6];
      dst[fldvlenp+1]=store_base64_alphabet[len&0x3f];
    }
  }
  
  /* Two bytes for the encoded length of (fld16v).
   */
  int fld16vlenp=dstc;
  APPEND(0)
  APPEND(0)
  
  /* (fld16v) encodes kind of like VLQ.
   * Each entry produces 1, 2, or 3 bytes of base64.
   * If the high bit (32) is set, lop that off, shift 5 bytes, and read the next character.
   * But the third character is six bits, not five.
   * Or if it's easier: 0..31 produce 1 byte, 32..1024 produce 2, and 1024..65535 produce 3.
   * Overencoding is possible.
   */
  {
    const uint16_t *src=store->fld16v;
    int i=store->fld16c;
    for (;i-->0;src++) {
      if (*src<0x0020) {
        APPEND(*src)
      } else if (*src<0x0400) {
        APPEND(0x20|((*src)>>5))
        APPEND((*src)&0x1f)
      } else {
        APPEND(0x20|((*src)>>11))
        APPEND(0x20|(((*src)>>6)&0x1f))
        APPEND((*src)&0x3f)
      }
    }
    // And then fill in the length.
    int len=dstc-(fld16vlenp+2);
    fprintf(stderr,"fld16vlen, new format: %d\n",len);
    if ((len<0)||(len>0xfff)) return -1;
    if (fld16vlenp<=dsta-2) {
      dst[fld16vlenp]=store_base64_alphabet[len>>6];
      dst[fld16vlenp+1]=store_base64_alphabet[len&0x3f];
    }
  }
  
  /* (clockv) will encode just as before: 5 bytes of base64 producing a 30-bit count of milliseconds.
   */
  {
    if (store->clockc>=64) return -1;
    int len=store->clockc*5;
    APPEND(len>>6)
    APPEND(len&0x3f)
    const double *src=store->clockv;
    int i=store->clockc;
    for (;i-->0;src++) {
      int v=(int)((*src)*1000.0);
      if (v&~0x3fffffff) v=0x3fffffff;
      APPEND(v>>24)
      APPEND((v>>18)&0x3f)
      APPEND((v>>12)&0x3f)
      APPEND((v>>6)&0x3f)
      APPEND(v&0x3f)
    }
    fprintf(stderr,"clockv doesn't change, still %d bytes\n",store->clockc*5);
  }
  
  /* Two bytes for jigstore length, in bytes.
   */
  int jigstorelenp=dstc;
  APPEND(0)
  APPEND(0)
  
  /* (jigstorev) is the interesting part.
   * It's currently enormous once the map collection gets filled out.
   * 1510 bytes for a full collection (always exactly 1510; 302 maps * 5 bytes each).
   * This is where I expect real savings.
   *
   * jigstorev: Five encoded bytes each, split big-endianly: 11 mapid, 8 x, 8 y, 3 xform.
   * Our addition: If (mapid) is zero, the remainder discusses maps relative to the previous entry.
   *   16:zero, 7:sequentialc, 7:namedc
   * Then for (sequentialc), add pieces at the expected places with ids increasing from this one.
   * Then for (namedc), read two bytes each for (mapid: 0..4095) and place relative to this one.
   *
   * Mind that this may change the order of pieces.
   */
  {
    uint8_t visitv[128]={0}; // Little-endian bits per jigstorev index, which maps have already been emitted.
    if (store->jigstorec>=sizeof(visitv)*8) return -1; // There won't be more than 302. But we can tolerate 1024.
    int i=0;
    const struct jigstore *jigstore=store->jigstorev;
    for (;i<store->jigstorec;i++,jigstore++) {
      if (visitv[i>>3]&(1<<(i&7))) continue; // Already got it from some earlier piece, cool.
      visitv[i>>3]|=1<<(i&7);
      int lng,lat;
      const struct plane *plane=plane_for_mapid(&lng,&lat,jigstore->mapid);
      if (!plane) {
        fprintf(stderr,"Plane not found for map:%d. Save file is corrupt, or tool/maps.c is wrong.\n",jigstore->mapid);
        return -2;
      }
      
      /* Emit (jigstore) in the old format.
       */
      int v=(jigstore->mapid<<19)|(jigstore->x<<11)|(jigstore->y<<3)|jigstore->xform;
      APPEND(v>>24)
      APPEND((v>>18)&0x3f)
      APPEND((v>>12)&0x3f)
      APPEND((v>>6)&0x3f)
      APPEND(v&0x3f)
      
      /* Find every other piece on this plane, with the same xform, whose position is exactly knowable from mine per (lng,lat,xform).
       * Normally that means pieces connected to me, but they don't actually need to be connected. If there's a gap between them, that's fine.
       * For us, this is a ridiculous n-squared-and-then-some algorithm. In game it should be neater because there we can look up maps by id.
       */
      const struct jigstore *alsov[256];
      int alsoc=0;
      const struct jigstore *other=jigstore+1;
      int otheri=i+1;
      for (;otheri<store->jigstorec;otheri++,other++) {
        // First two easy tests:
        if (other->xform!=jigstore->xform) continue;
        if (visitv[otheri>>3]&(1<<(otheri&7))) continue;
        // Take the relative position. If it's not a multiple of the map size, it can't be in the expected position. Mind the xform.
        int dx=other->x-jigstore->x;
        int dy=other->y-jigstore->y;
        if (jigstore->xform&0x04/*SWAP*/) {
          if (dx%MAPH) continue;
          if (dy%MAPW) continue;
          dx/=MAPH;
          dy/=MAPW;
        } else {
          if (dx%MAPW) continue;
          if (dy%MAPH) continue;
          dx/=MAPW;
          dy/=MAPH;
        }
        if (!dx&&!dy) continue; // Also they can't be on top of each other.
        // OK, search for (other)'s expected position.
        int olng,olat;
        const struct plane *oplane=plane_for_mapid(&olng,&olat,other->mapid);
        if (!oplane||(oplane->id!=plane->id)) continue;
        // And where is our jigpiece in latitude and longitude? Mind the xform.
        int ax,ay;
        switch (jigstore->xform) {
          case 0x00: ax=lng+dx; ay=lat+dy; break;
          case 0x06: ax=lat-dx; ay=lng+dy; break;
          case 0x03: ax=lng-dx; ay=lat-dy; break;
          case 0x05: ax=lat+dx; ay=lng-dy; break;
          default: ax=lng; ay=lat;
        }
        if ((olng!=ax)||(olat!=ay)) continue;
        // OK, this one is a candidate to ride along. Not a sure thing yet, tho.
        alsov[alsoc++]=other;
        //visitv[otheri>>3]|=1<<(otheri&7);
      }
      
      /* For those in (alsov), find the sequential ones, count them, and zero them.
       * Then sweep again to count named neighbors.
       * Their order is essentially random, so it's all very sweepy.
       */
      int sequentialc=0;
      int nextid=jigstore->mapid+1;
      for (;;nextid++) {
        int found=0;
        const struct jigstore **q=alsov;
        int qi=alsoc;
        for (;qi-->0;q++) {
          if (*q&&((*q)->mapid==nextid)) {
            int p=(*q)-store->jigstorev;
            if ((p>=0)&&(p<store->jigstorec)) visitv[p>>3]|=1<<(p&7);
            *q=0;
            found=1;
            break;
          }
        }
        if (!found) break;
        sequentialc++;
        if (sequentialc>=0x7f) break; // really shouldn't happen; the largest plane is 100
      }
      int namedc=0;
      const struct jigstore **q=alsov;
      int qi=alsoc;
      qi=0; // XXX At least in the full-map case, it actually works out better to skip named neighbors. Because they tend to be contiguous to each other.
      for (;qi-->0;q++) {
        if (!*q) continue;
        namedc++;
        if (namedc>=0x7f) break;
      }
      if (!sequentialc&&!namedc) continue; // No neighbors. That's fine; jigstore is off on its own.
      
      // Emit a dummy record with mapid zero, and the neighbor counts.
      v=(sequentialc<<7)|namedc;
      APPEND(v>>24)
      APPEND((v>>18)&0x3f)
      APPEND((v>>12)&0x3f)
      APPEND((v>>6)&0x3f)
      APPEND(v&0x3f)
      
      // Then each named neighbor, emit its mapid in two bytes.
      for (q=alsov,qi=alsoc;namedc&&(qi-->0);q++) {
        if (!*q) continue;
        int p=(*q)-store->jigstorev;
         if ((p>=0)&&(p<store->jigstorec)) visitv[p>>3]|=1<<(p&7);
        APPEND((*q)->mapid>>6)
        APPEND((*q)->mapid&0x3f)
        namedc--;
      }
    }
    // Fill in length.
    int len=dstc-(jigstorelenp+2);
    fprintf(stderr,"new format jigstorec (bytes): %d\n",len);
    if ((len<0)||(len>0xfff)) return -1;
    if (jigstorelenp<=dsta-2) {
      dst[jigstorelenp]=store_base64_alphabet[len>>6];
      dst[jigstorelenp+1]=store_base64_alphabet[len&0x3f];
    }
  }
  
  /* Invstore length.
   */
  int invstorelenp=dstc;
  APPEND(0)
  APPEND(0)
  
  /* Invstore.
   * We'll do close to the same thing as before, but optimize for no-quantity items with itemid<63.
   * So, read one base64 character. If it's under 63, it's itemid, and limit and quantity are zero.
   * If it's 63 exactly, read four more characters, producing 3 bytes, just as before.
   */
  {
    int invstorec=INVSTORE_SIZE;
    while (invstorec&&!store->invstorev[invstorec-1].itemid) invstorec--;
    const struct invstore *invstore=store->invstorev;
    int i=invstorec;
    for (;i-->0;invstore++) {
      if ((invstore->itemid<63)&&!invstore->limit&&!invstore->quantity) {
        APPEND(invstore->itemid)
      } else {
        int v=(invstore->itemid<<16)|(invstore->limit<<8)|invstore->quantity;
        APPEND(63)
        APPEND((v>>18)&0x3f)
        APPEND((v>>12)&0x3f)
        APPEND((v>>6)&0x3f)
        APPEND(v&0x3f)
      }
    }
    // Fill in length.
    int len=dstc-(invstorelenp+2);
    if ((len<0)||(len>0xfff)) return -1;
    if (invstorelenp<=dsta-2) {
      dst[invstorelenp]=store_base64_alphabet[len>>6];
      dst[invstorelenp+1]=store_base64_alphabet[len&0x3f];
    }
  }
  
  /* And finally a 30-bit checksum of the content encoded so far.
   */
  int checksum=0;
  if (dstc<=dsta) {
    checksum=store_checksum(dst,dstc);
  }
  APPEND(checksum>>24)
  APPEND((checksum>>18)&0x3f)
  APPEND((checksum>>12)&0x3f)
  APPEND((checksum>>6)&0x3f)
  APPEND(checksum&0x3f)
  
  #undef APPEND
  return dstc;
}

/* Compare jigstore for sort.
 */
 
static int jigstore_cmp(const void *a,const void *b) {
  const struct jigstore *A=a,*B=b;
  return A->mapid-B->mapid;
}

/* Main.
 */
 
int main(int argc,char **argv) {
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) g.exename=argv[0];
  else g.exename="validate";
  
  /* All three of these files get smaller under the new regime.
   * The biggest driver of savings is jigstore, and it works best when the pieces are connected.
   */
  //const char *input=old_format_fresh; // 32<40
  const char *input=old_format_100pct; // 269<1832
  //const char *input=worst_case_scenario; // 1719<1832
  int inputc=0; while (input[inputc]) inputc++;
  
  struct store store={0};
  if (store_decode(&store,input,inputc)<0) {
    fprintf(stderr,"%s: Failed to decode input store.\n",g.exename);
    return 1;
  }
  
  /* A lil experiment: Does it help to sort jigstore by id?
   * ...a little.
   * ...a lot, if we also nix named-neighbors.
   */
  qsort(store.jigstorev,store.jigstorec,sizeof(struct jigstore),jigstore_cmp);
  
  char output[4096];
  int outputc=encode_new(output,sizeof(output),&store);
  if (outputc<0) {
    fprintf(stderr,"%s: Reencode failed\n",g.exename);
    return 1;
  } else if (outputc>sizeof(output)) {
    fprintf(stderr,"%s: Reencoded output way too large (%d>%d)\n",g.exename,outputc,(int)sizeof(output));
    return 1;
  } else if (outputc>=inputc) {
    fprintf(stderr,"%s: Seems to have reencoded ok but no length improvement (%d>=%d)\n",g.exename,outputc,inputc);
  } else {
    fprintf(stderr,"%s: Reencoded with improved length (%d<%d)\n",g.exename,outputc,inputc);
    //fprintf(stderr,"%.*s\n",outputc,output);
  }
  
  struct store restore={0};
  if (decode_new(&restore,output,outputc)<0) {
    fprintf(stderr,"%s: Decode new format failed!\n",g.exename);
    return 1;
  }
  if (compare_stores(&store,&restore)<0) {
    fprintf(stderr,"%s: Stores mismatch after conversion!\n",g.exename);
    return 1;
  }
  fprintf(stderr,"%s: Reencoded store decoded indentical to original -- pass.\n",g.exename);
  
  return 0;
}

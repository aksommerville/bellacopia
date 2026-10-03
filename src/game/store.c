#include "game/bellacopia.h"
#include "game/modal/jigsaw.h"

#define STORE_SAVE_DEBOUNCE 2.000

/* Sanitize a fresh store, after clear or load.
 * Sets a few defaults, forces valid HP, any other non-negotiable state.
 */
 
static int store_sanitize() {

  // (hp,hpmax) can't be less than 3, and (hp) can't exceed (hpmax).
  int hp=store_get_fld16(NS_fld16_hp);
  int hpmax=store_get_fld16(NS_fld16_hpmax);
  if (hpmax<3) store_set_fld16(NS_fld16_hpmax,hpmax=3);
  if (hp<3) store_set_fld16(NS_fld16_hp,hp=3);
  else if (hp>hpmax) store_set_fld16(NS_fld16_hp,hp=hpmax);
  
  // (goldmax) can't be less than 99, and (gold) can't exceed (goldmax).
  int gold=store_get_fld16(NS_fld16_gold);
  int goldmax=store_get_fld16(NS_fld16_goldmax);
  if (goldmax<99) store_set_fld16(NS_fld16_goldmax,goldmax=99);
  if (gold>goldmax) store_set_fld16(NS_fld16_gold,gold=goldmax);

  g.store.dirty=0;
  return 0;
}

/* Clear.
 */
 
int store_clear() {
  //fprintf(stderr,"%s\n",__func__);
  g.store.fldc=0;
  g.store.fld16c=0;
  g.store.clockc=0;
  g.store.jigstorec=0;
  memset(g.store.invstorev,0,sizeof(g.store.invstorev));
  g.store.listenerc=0;
  g.store.listenerid_next=1;
  
  // Those fld16 that hold initial limits must get populated.
  store_set_fld16(NS_fld16_hpmax,3);
  store_set_fld16(NS_fld16_hp,3);
  store_set_fld16(NS_fld16_goldmax,99);
  
  return store_sanitize();
}

/* Grow sections.
 * Caller provides the desired count. (c) is ignored.
 */
 
int store_require_fldv(struct store *store,int totalc) {
  if (totalc<=store->flda) return 0;
  void *nv=realloc(store->fldv,totalc);
  if (!nv) return -1;
  store->fldv=nv;
  store->flda=totalc;
  return 0;
}

int store_require_fld16v(struct store *store,int totalc) {
  if (totalc<=store->fld16a) return 0;
  if (totalc>INT_MAX/2) return -1;
  void *nv=realloc(store->fld16v,totalc<<1);
  if (!nv) return -1;
  store->fld16v=nv;
  store->fld16a=totalc;
  return 0;
}

int store_require_clockv(struct store *store,int totalc) {
  if (totalc<=store->clocka) return 0;
  if (totalc>INT_MAX/sizeof(double)) return -1;
  void *nv=realloc(store->clockv,sizeof(double)*totalc);
  if (!nv) return -1;
  store->clockv=nv;
  store->clocka=totalc;
  return 0;
}

int store_require_jigstorev(struct store *store,int totalc) {
  if (totalc<=store->jigstorea) return 0;
  if (totalc>INT_MAX/sizeof(struct jigstore)) return -1;
  void *nv=realloc(store->jigstorev,sizeof(struct jigstore)*totalc);
  if (!nv) return -1;
  store->jigstorev=nv;
  store->jigstorea=totalc;
  return 0;
}

/* After a successful load, but before general sanitization, scan all maps for POI.
 * Anything driven by a treadle must be forced off.
 * Player could end their session standing on a treadle, and without intervention it would stick on next time.
 */
 
static void store_force_agreement_with_poi() {
  struct map **mapp=g.mapstore.byidv;
  int i=g.mapstore.byidc;
  for (;i-->0;mapp++) {
    struct map *map=*mapp;
    if (!map) continue;
    struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
    struct cmdlist_entry cmd;
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_map_treadle: store_set_fld((cmd.arg[2]<<8)|cmd.arg[3],0); break;
      }
    }
  }
}

/* Check for a saved game arriving as process input.
 */
 
int store_refresh_fromuser() {
  fprintf(stderr,"%s...\n",__func__);
  if (g.store.fromuser) free(g.store.fromuser);
  g.store.fromuser=0;
  g.store.fromuserc=0;
  
  g.store.fromuserc=egg_store_get(0,0,"savedgame",9);
  if (g.store.fromuserc<1) {
    g.store.fromuserc=0;
    fprintf(stderr,"...no savedgame as input, this is normal.\n");
    return 0;
  }
  fprintf(stderr,"...savedgame input exists, %d bytes...\n",g.store.fromuserc);
  
  if (!(g.store.fromuser=malloc(g.store.fromuserc))) {
    g.store.fromuserc=0;
    return -1;
  }
  if (egg_store_get(g.store.fromuser,g.store.fromuserc,"savedgame",9)!=g.store.fromuserc) {
    free(g.store.fromuser);
    g.store.fromuser=0;
    g.store.fromuserc=0;
    return -1;
  }
  egg_store_set("savedgame",9,"",0); // Important to unset it after acquisition, otherwise it saves forever.
  fprintf(stderr,"...got savedgame: %.*s\n",g.store.fromuserc,g.store.fromuser);
  
  return 0;
}

/* Load.
 */
 
static int store_load_fail() {
  fprintf(stderr,"Failed to decode saved game.\n");
  return store_clear();
}

int store_load(const char *k,int kc) {
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  //fprintf(stderr,"%s '%.*s'...\n",__func__,kc,k);
  g.store.loaded=1;
  g.store.listenerc=0;
  g.store.listenerid_next=1;
  
  int srcc=egg_store_get(0,0,k,kc);
  if (srcc<1) return store_clear();
  char *src=malloc(srcc);
  if (!src) return store_load_fail();
  if (egg_store_get(src,srcc,k,kc)!=srcc) {
    free(src);
    return store_load_fail();
  }
  
  int err=store_decode(&g.store,src,srcc);
  free(src);
  if (err<0) return store_load_fail();
  
  store_force_agreement_with_poi();
  return store_sanitize();
}

/* Save.
 */

static int store_save_now(const char *k,int kc) {
  g.store.dirty=0;
  g.store.savedebounce=0.0;
  
  int seriala=2048; // TODO What's a good upper bound for expected output size of encoded saved game?
  char *serial=malloc(seriala);
  if (!serial) return -1;
  int serialc;
  for (;;) {
    serialc=store_encode(serial,seriala,&g.store);
    if (serialc<0) {
      free(serial);
      return -1;
    }
    if (serialc<=seriala) break;
    void *nv=realloc(serial,serialc);
    if (!nv) {
      free(serial);
      return -1;
    }
    serial=nv;
    seriala=serialc;
  }
  
  if (!k) kc=0; else if (kc<0) { kc=0; while (k[kc]) kc++; }
  int err=egg_store_set(k,kc,serial,serialc);
  free(serial);
  if (err<0) {
    fprintf(stderr,"%s: Failed to save store, encoded to %d bytes\n",__func__,serialc);
    return -1;
  } else {
    //fprintf(stderr,"%s: Saved store as '%.*s', %d bytes\n",__func__,kc,k,serialc);
  }
  return 0;
}

/* Save if dirty.
 */

void store_save_if_dirty(const char *k,int kc,int immediate) {
  if (!g.store.dirty) return;
  if (immediate) {
    store_save_now(k,kc);
  } else {
    double now=egg_time_real();
    if (g.store.savedebounce<=0.0) {
      g.store.savedebounce=now;
    } else if (now-g.store.savedebounce>=STORE_SAVE_DEBOUNCE) {
      store_save_now(k,kc);
    }
  }
}

/* Read.
 */

int store_get_fld(int fld) {
  // A few special fields are immutable, whether we have them for real or not:
  switch (fld) {
    case NS_fld_zero: return 0;
    case NS_fld_one: return 1;
    case NS_fld_alsozero: return 0;
  }
  if (fld<0) return 0;
  int p=fld>>3;
  if (p>=g.store.fldc) return 0;
  uint8_t mask=1<<(fld&7);
  return (g.store.fldv[p]&mask)?1:0;
}

int store_get_fld16(int fld16) {
  if ((fld16<0)||(fld16>=g.store.fld16c)) return 0;
  return g.store.fld16v[fld16];
}

double store_get_clock(int clock) {
  if ((clock<0)||(clock>=g.store.clockc)) return 0.0;
  return g.store.clockv[clock];
}

struct jigstore *store_get_jigstore(int mapid) {
  if (mapid&~0x07ff) return 0;
  struct jigstore *jigstore=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;jigstore++) {
    if (jigstore->mapid==mapid) return jigstore;
  }
  return 0;
}

struct invstore *store_get_invstore(int invslot) {
  if ((invslot<0)||(invslot>=INVSTORE_SIZE)) return 0;
  return g.store.invstorev+invslot;
}

struct invstore *store_get_itemid(int itemid) {
  if (itemid&~0xff) return 0;
  struct invstore *invstore=g.store.invstorev;
  int i=INVSTORE_SIZE;
  for (;i-->0;invstore++) {
    if (invstore->itemid==itemid) return invstore;
  }
  return 0;
}

/* Set fields.
 */

int store_set_fld(int fld,int value) {
  if (fld<=NS_fld_alsozero) return 0;
  int p=fld>>3;
  if (p>=g.store.fldc) {
    if (!value) return 0;
    if (store_require_fldv(&g.store,p+1)<0) return -1;
    while (g.store.fldc<=p) g.store.fldv[g.store.fldc++]=0;
  }
  uint8_t mask=1<<(fld&7);
  if (value) {
    if (g.store.fldv[p]&mask) return 0;
    g.store.fldv[p]|=mask;
  } else {
    if (!(g.store.fldv[p]&mask)) return 0;
    g.store.fldv[p]&=~mask;
  }
  g.store.dirty=1;
  store_broadcast('f',fld,value?1:0);
  return 1;
}

int store_set_fld16(int fld,int value) {
  if (fld<0) return -1;
  value&=0xffff;
  if (fld>=g.store.fld16c) {
    if (!value) return 0;
    if (store_require_fld16v(&g.store,fld+1)<0) return -1;
    while (g.store.fld16c<=fld) g.store.fld16v[g.store.fld16c++]=0;
  }
  if (g.store.fld16v[fld]==value) return 0;
  g.store.fld16v[fld]=value;
  g.store.dirty=1;
  store_broadcast('6',fld,value);
  return 1;
}

/* Choose position and xform for new jigstore.
 * It's random but controlled in complicated ways.
 */

static void store_choose_jigstore_position(struct jigstore *jigstore) {
  
  /* (xform) may only be those of even chirality, zero or two bits set.
   * ie those you can rotate to from natural.
   */
  switch (rand()&3) {
    case 0: jigstore->xform=0; break;
    case 1: jigstore->xform=EGG_XFORM_XREV|EGG_XFORM_YREV; break;
    case 2: jigstore->xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
    case 3: jigstore->xform=EGG_XFORM_XREV|EGG_XFORM_SWAP; break;
  }
  
  /* If we have to bail out, eg for the first piece of a puzzle, it goes in the middle.
   */
  jigstore->x=JIGSAW_FLDW>>1;
  jigstore->y=JIGSAW_FLDH>>1;
  
  /* Get the map in question, so we can compare (z).
   */
  const struct map *map=map_by_id(jigstore->mapid);
  if (!map) return;
  
  /* Collect all jigstores on this plane.
   * We only store mapid per jigpiece, so there's a lot of looking-up.
   * Best to front load that effort.
   * No others is perfectly possible, eg the first jigpiece you pick up. In that case, we're already ready.
   */
  const struct jigstore *otherv[100]; // Largest plane is 10x10, we'll keep it that way.
  int otherc=0;
  const struct jigstore *q=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;q++) {
    const struct map *qmap=map_by_id(q->mapid);
    if (!qmap) continue;
    if (qmap->z!=map->z) continue;
    otherv[otherc++]=q;
    if (otherc>=100) break;
  }
  if (!otherc) return;
  
  /* Take a few random guesses at position, and keep whichever yields the most breathing room.
   * I'm not crazy about this algorithm. It tends to favor the edges.
   */
  #define MARGIN 8 /* Try to keep them off the very edge initially. They can overlap the tab bar in awkward ways. */
  int repc=20;
  double bestscore=999999.999;
  while (repc-->0) {
    int x=MARGIN+rand()%(JIGSAW_FLDW-(MARGIN<<1));
    int y=MARGIN+rand()%(JIGSAW_FLDH-(MARGIN<<1));
    double score=0.0;
    const struct jigstore **otherp=otherv;
    int i=otherc;
    for (;i-->0;otherp++) {
      const struct jigstore *other=*otherp;
      int dx=other->x-x;
      int dy=other->y-y;
      if (!dx&&!dy) score+=1.0;
      else score+=1.0/(double)(dx*dx+dy*dy);
    }
    if (score<bestscore) {
      bestscore=score;
      jigstore->x=x;
      jigstore->y=y;
    }
  }
  #undef MARGIN
}

/* Add jigstore.
 */

struct jigstore *store_add_jigstore(int mapid) {
  if (mapid&~0x07ff) return 0;
  struct jigstore *jigstore=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;jigstore++) {
    if (jigstore->mapid==mapid) return jigstore;
  }
  if (store_require_jigstorev(&g.store,g.store.jigstorec+1)<0) return 0;
  jigstore=g.store.jigstorev+g.store.jigstorec;
  jigstore->mapid=mapid;
  store_choose_jigstore_position(jigstore);
  g.store.jigstorec++;
  g.store.dirty=1;
  store_broadcast('j',mapid,0);
  
  /* If we haven't computed the total yet, do that now.
   */
  if (!g.store.jigstore_limit) {
    int planev[8];
    int planec=get_puzzle_planes(planev,8);
    while (planec-->0) {
      const struct plane *plane=plane_by_position(planev[planec]);
      if (!plane) continue;
      g.store.jigstore_limit+=plane->w*plane->h; // Puzzled planes are expected to be rectangular with no holes.
    }
  }
  
  /* If we crossed the limit, set the indicator flag.
   */
  if ((g.store.jigstorec>=g.store.jigstore_limit)&&!store_get_fld(NS_fld_maps_complete)) {
    fprintf(stderr,"%.0f Collected the last puzzle piece.\n",egg_time_real());
    store_set_fld(NS_fld_maps_complete,1);
    struct modal_args_cutscene args={
      .strix_title=10,
      .context=CUTSCENE_CONTEXT_INTERRUPT,
    };
    struct modal *modal=modal_spawn(&modal_type_cutscene,&args,sizeof(args));
  }
  
  return jigstore;
}

/* Add item.
 */

struct invstore *store_add_itemid(int itemid,int quantity) {
  if (itemid&~0xff) return 0;
  struct invstore *blank=0;
  struct invstore *invstore=g.store.invstorev;
  int i=INVSTORE_SIZE;
  for (;i-->0;invstore++) {
    if (invstore->itemid==itemid) {
      if (invstore->limit) {
        int nq=invstore->quantity+quantity;
        if (nq>invstore->limit) invstore->quantity=invstore->limit;
        else invstore->quantity=nq;
        g.store.dirty=1;
        store_broadcast('i',itemid,0);
      }
      return invstore;
    } else if (!invstore->itemid&&!blank) {
      blank=invstore;
    }
  }
  if (!blank) return 0;
  blank->itemid=itemid;
  if (quantity) {
    blank->quantity=quantity;
    const struct item_detail *detail=item_detail_for_itemid(itemid);
    if (detail&&(detail->initial_limit>0)) blank->limit=detail->initial_limit;
    else blank->limit=quantity;
  }
  g.store.dirty=1;
  store_broadcast('i',itemid,0);
  return blank;
}

/* Get clock, growing store as needed.
 */
 
double *store_require_clock(int clock) {
  if (clock<0) return 0;
  if (clock>256) return 0; // Sanity limit.
  if (clock>=g.store.clocka) {
    int na=(clock+16)&~15;
    void *nv=realloc(g.store.clockv,sizeof(double)*na);
    if (!nv) return 0;
    g.store.clockv=nv;
    g.store.clocka=na;
  }
  if (clock>=g.store.clockc) {
    g.store.dirty=1;
    while (clock>=g.store.clockc) g.store.clockv[g.store.clockc++]=0.0;
  }
  return g.store.clockv+clock;
}

/* Map has parent?
 */
 
static int map_has_parent(const struct map *map) {
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    if (cmd.opcode==CMD_map_parent) return 1;
  }
  return 0;
}

/* Measure jigstore progress.
 * We also set (g.jigstate).
 */
 
void jigstore_progress_tabulate(struct jigstore_progress *progress) {
  memset(progress,0,sizeof(struct jigstore_progress));
  g.jigstate=-1;
  /* Iterate planes from mapstore.
   * Iterate maps in each. (mind that index zero isn't necessarily a real map, have to iterate to find the first).
   * If the first map has a parent, abort the plane, move on.
   * First map does not have a parent, increment (planec_total) and start counting maps into (piecec_total).
   * ^ If (plane->full), we can short-circuit this.
   * (piecec_got) is easy, it's just (g.store.jigstorec).
   * If every piece is got, scan jigstore for completion. That's the tricky part.
   */
  struct plane *plane=g.mapstore.planev;
  int planei=g.mapstore.planec;
  for (;planei-->0;plane++) {
    if (!plane->v) continue;
    if (plane->full) {
      if (map_has_parent(plane->v)) continue;
      progress->planec_total++;
      progress->piecec_total+=plane->w*plane->h;
    } else {
      struct map *map=plane->v;
      int mapi=plane->w*plane->h;
      while ((mapi-->0)&&!map->rid) map++;
      if (map_has_parent(map)) continue;
      progress->planec_total++;
      progress->piecec_total++;
      map++;
      for (;mapi-->0;map++) if (map->rid) progress->piecec_total++;
    }
  }
  if (progress->planec_total<1) return;
  if (progress->piecec_total<1) return;
  progress->piecec_got=g.store.jigstorec;
  // If all the pieces are got, ask jigsaw for each plane, whether it's complete.
  // We can stop at the first false.
  if (progress->piecec_got==progress->piecec_total) {
    progress->finished=g.jigstate=1;
    for (plane=g.mapstore.planev,planei=g.mapstore.planec;planei-->0;plane++) {
      if (!plane->v) continue;
      if (plane->full) {
        if (map_has_parent(plane->v)) continue;
      } else {
        struct map *map=plane->v;
        int mapi=plane->w*plane->h;
        while ((mapi-->0)&&!map->rid) map++;
        if (map_has_parent(map)) continue;
      }
      if (!jigsaw_plane_is_complete(plane->z)) {
        progress->finished=0;
        g.jigstate=-1;
        return;
      }
    }
  }
}

/* Jigstore progress, the simpler answer.
 */
 
int jigstore_is_complete() {
  if (!g.jigstate) {
    struct jigstore_progress progress;
    jigstore_progress_tabulate(&progress);
    if (!g.jigstate) { // tabulation should set this, but we can clean up after it if not
      g.jigstate=progress.finished?1:-1;
    }
  }
  if (g.jigstate>0) return 1;
  return 0;
}

/* Nonzero if anything in jigstore is present.
 */
 
int jigstore_has_anything() {
  struct jigstore *jigstore=g.store.jigstorev;
  int i=g.store.jigstorec;
  for (;i-->0;jigstore++) {
    if (jigstore->y==0xff) continue; // Dummy.
    return 1;
  }
  return 0;
}

/* Listeners.
 */
 
int store_listen(char type,void (*cb)(char type,int id,int value,void *userdata),void *userdata) {
  if (!cb) return -1;
  if (g.store.listenerc>=g.store.listenera) {
    int na=g.store.listenera+16;
    if (na>INT_MAX/sizeof(struct store_listener)) return -1;
    void *nv=realloc(g.store.listenerv,sizeof(struct store_listener)*na);
    if (!nv) return -1;
    g.store.listenerv=nv;
    g.store.listenera=na;
  }
  struct store_listener *listener=g.store.listenerv+g.store.listenerc++;
  listener->type=type;
  listener->cb=cb;
  listener->userdata=userdata;
  if (g.store.listenerid_next<1) g.store.listenerid_next=1;
  listener->listenerid=g.store.listenerid_next++;
  return listener->listenerid;
}

void store_unlisten(int listenerid) {
  if (listenerid<1) return;
  int i=g.store.listenerc;
  struct store_listener *listener=g.store.listenerv+i-1;
  for (;i-->0;listener--) {
    if (listener->listenerid!=listenerid) continue;
    g.store.listenerc--;
    memmove(listener,listener+1,sizeof(struct store_listener)*(g.store.listenerc-i));
    return;
  }
}

void store_broadcast(char type,int id,int value) {
  // Don't keep a pointer to (listenerv) across callbacks.
  int i=g.store.listenerc;
  while (i-->0) {
    struct store_listener *listener=g.store.listenerv+i;
    if (listener->type&&(listener->type!=type)) continue;
    listener->cb(type,id,value,listener->userdata);
  }
}

/* store.h
 * Everything gets packed into one field in the save file, but they're split out in memory:
 *  - fld: 1-bit general fields.
 *  - fld16: 16-bit general fields.
 *  - jigsaw: Possession, position, and rotation of each piece, as they appear in the pause modal.
 *  - inventory: Equipped item, and the ones in your backpack.
 *  - clock: Floating-point times. Persist as integer ms.
 */
 
#ifndef STORE_H
#define STORE_H

#define INVSTORE_SIZE 26 /* First is the equipped item, then the 25 in your backpack. */

struct store {
  
  uint8_t *fldv; // Little-endian.
  int fldc,flda; // Bytes (not bits!)
  
  uint16_t *fld16v;
  int fld16c,fld16a; // Words (not bytes!)
  
  double *clockv;
  int clockc,clocka;
  
  struct jigstore {
    uint16_t mapid;
    uint8_t x,y,xform;
  } *jigstorev;
  int jigstorec,jigstorea;
  int jigstore_limit; // Total count of jigstores in the game; determined dynamically the first time we need it.
  
  struct invstore {
    uint8_t itemid; // If zero, the slot is vacant. (limit,quantity) undefined.
    uint8_t limit; // If zero, it's not a counted item, and (quantity) isn't used.
    uint8_t quantity;
  } invstorev[INVSTORE_SIZE];
  
  int dirty; // Outsiders may set, if you change something.
  double savedebounce;
  int loaded;
  
  struct store_listener {
    int listenerid;
    char type; // Zero for everything, or one of: f6ji
    void (*cb)(char type,int id,int value,void *userdata);
    void *userdata;
  } *listenerv;
  int listenerc,listenera;
  int listenerid_next;
  
  /* Present if we got an encoded saved game from argv or query params.
   * store_refresh_fromuser() to reacquire, but that only needs done once.
   */
  char *fromuser;
  int fromuserc;
};

int store_refresh_fromuser();

/* These are both fallible, but missing or invalid data is not an error.
 * The only real error is allocation failure, and should be treated as an emergency.
 * If we report success, the store is initialized and valid.
 * Possibly to the default state, even if you requested a load.
 */
int store_clear();
int store_load(const char *k,int kc);

/* main.c should spam this.
 * Always noop if the store is clean.
 * If (now), we encode and save it immediately.
 * Otherwise there may be some amount of debounce.
 * Trivial things like moving a jigpiece can cause repetitive store changes, so we prefer to space them out a bit.
 */
void store_save_if_dirty(const char *k,int kc,int now);

/* Further details for encode and decode, probably only interesting to store itself.
 */
int store_validate_serial(const char *src,int srcc);
int store_decode(struct store *store,const char *src,int srcc);
int store_encode(char *dst,int dsta,const struct store *store);
int store_require_fldv(struct store *store,int totalc_bytes);
int store_require_fld16v(struct store *store,int totalc);
int store_require_clockv(struct store *store,int totalc);
int store_require_jigstorev(struct store *store,int totalc);

/* Anyone can listen for changes to the store.
 * If you modify the store directly, broadcast it.
 * Do not remove listeners during your callback, except yourself.
 * Clock changes do not broadcast.
 * All listeners are dropped at clear and load.
 *
 * | type | id              | value |
 * +------+-----------------+-------+
 * | (0)  | Subscribes to all changes.
 * | 'f'  | fld             | 0,1   |
 * | '6'  | fld16           | 0..0xffff |
 * | 'j'  | mapid(jigstore) | unused |
 * | 'i'  | itemid          | unused |
 * | 's'  | Signal: Not part of the store. For arbitrary signalling.
 */
int store_listen(char type,void (*cb)(char type,int id,int value,void *userdata),void *userdata);
void store_unlisten(int listenerid);
void store_broadcast(char type,int id,int value);

int store_get_fld(int fld);
int store_get_fld16(int fld16);
double store_get_clock(int clock);
struct jigstore *store_get_jigstore(int mapid);
struct invstore *store_get_invstore(int invslot); // 0=equipped, 1..25=backpack
struct invstore *store_get_itemid(int itemid);

int store_set_fld(int fld,int value); // => nonzero if changed
int store_set_fld16(int fld,int value); // => ''
struct jigstore *store_add_jigstore(int mapid); // => creates if not existing yet
struct invstore *store_add_itemid(int itemid,int quantity); // => creates if not existing yet, or bumps quantity as warranted

/* In general, ticking a clock shouldn't dirty the store.
 * Call this to get the clock, then you tick it directly.
 * It gets saved the next time something else goes dirty.
 * The first access of a clock that didn't exist yet does dirty the store.
 */
double *store_require_clock(int clock);

/* Scan all maps and all jigsaw pieces to measure progress.
 * I wish we could report how many planes are fully complete, but that is an (n*m) problem.
 * Likewise, quantifying intermediate completion of each plane is just too much.
 * We basically just count pieces.
 * If (piecec_got==piecec_total), then we scan jigstore to determine whether they are all connected.
 */
struct jigstore_progress {
  int piecec_got;
  int piecec_total;
  int planec_total;
  int finished;
};
void jigstore_progress_tabulate(struct jigstore_progress *progress);
int jigstore_is_complete();
int jigstore_has_anything();
 
/* Serial format, written out to "save" in the Egg store.
 * Lexically, it's Base64 with no padding.
 * But don't decode the whole thing Base64, it's designed to be accessed piecemeal on the encoded stream.
 * There's a minimum length of 16: One signature byte, 10 bytes of framing, and a 5 byte checksum.
 *
 * Top level structure:
 *  - 1: "/" signature.
 *  - ...: fldv. 2 encoded bytes of length in bytes, followed by content.
 *  - ...: fld16v. ''
 *  - ...: clockv. ''
 *  - ...: jigstorev. ''
 *  - ...: invstorev. ''
 *  - 5: Checksum. Computed on the encoded stream up to this point.
 *
 * fldv:
 * Each encoded character is two run lengths 0..7, little-endianly.
 * The first run has a value of zero (fld 0 must always be unset).
 * Emit so many fields little-endianly.
 * After reading a 7, do not swap the output value.
 * Any other run length, swap the output value each time.
 *
 * fld16v:
 * Each value is stored in 1, 2, or 3 bytes, similar to VLQ.
 * First and second bytes have 5 payload bits at the low end, and if their high bit is set, read another byte.
 * Third byte is 6 bits of payload.
 * The three payload combine big-endianly.
 *
 * clockv:
 * 5 bytes per value, big-endian milliseconds.
 *
 * jigstorev:
 * Read 5 bytes at a time, and split that 30-bit integer big-endianly. One of:
 *   11:mapid 8:x 8:y 3:xform
 *   16:zero 7:sequentialc 7:namedc
 * If (mapid) is zero, it's the second form, and there must have been a command of the first form immediately proceeding.
 * Add (sequentialc) maps with mapid increasing from the reference map, in the inferrable locations.
 * Then read 2-byte mapid (0..4095) for each (namedc) and place them in the inferrable locations.
 * Encoding and decoding requires knowledge of the map set. Specifically, just the location of each mapid in its plane.
 *
 * invstorev:
 * Read one leading byte.
 * If <63, it's (itemid). (limit,quantity) are both zero.
 * If 63 exactly, read another 4 bytes: 0xff0000=itemid 0x00ff00=limit 0x0000ff=quantity.
 */

#endif

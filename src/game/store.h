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
    uint8_t x,y,xform; // (y==0xff) means it isn't got yet.
  } *jigstorev;
  int jigstorec,jigstorea;
  
  struct invstore {
    uint8_t itemid; // If zero, the slot is vacant. (limit,quantity) undefined.
    uint8_t limit; // If zero, it's not a counted item, and (quantity) may be used for something else.
    uint8_t quantity;
  } invstorev[INVSTORE_SIZE];
  
  int dirty; // Outsiders may set, if you change something.
  double savedebounce;
};

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

/* Serial format, written out to "save" in the Egg store.
 * Starts with 10 bytes for the lengths of the individual stores.
 * Each length is 2 Base64 digits, big-endianly:
 *  - fldc (bytes decoded, always a multiple of 3)
 *  - fld16c (fields)
 *  - clockc (fields)
 *  - jigstorec (records)
 *  - invstorec (records). Can't go above 26, but we use 2 bytes like the others, for consistency.
 * Followed by the heaps, Base64, in the same order:
 *  - fldv: Straight Base64, and length must align to a block.
 *  - fld16v: Three encoded bytes each, big-endian, the 2 high bits of each must be zero.
 *  - clockv: Five encoded bytes each, big-endian, ms. Holds about 298 hours each.
 *  - jigstorev: Five encoded bytes each, split big-endianly: 11 mapid, 8 x, 8 y, 3 xform.
 *  - invstorev: Four encoded bytes each: itemid,limit,quantity. ie straight base64 of the whole (invstorev).
 * Followed by a 30-bit checksum, performed on the encoded stream.
 */

#endif

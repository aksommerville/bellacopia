#ifndef SAVETEST_H
#define SAVETEST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "opt/fs/fs.h"
#include "opt/serial/serial.h"

/* Some stuff copied out of the game, lightly massaged to work here.
 ********************************************************************************/

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
};
 
struct store_toc {
  const char *fldv,*fld16v,*clockv,*jigstorev,*invstorev;
  int efldc,efld16c,eclockc,ejigstorec,einvstorec; // Length of encoded heaps in bytes.
  int lfldc,lfld16c,lclockc,ljigstorec,linvstorec; // Logical length, ie count of fields.
  int checksum_expect; // What's encoded in the serial for validation.
  int checksum_actual; // We compute during decode but we don't fail on mismatches.
};

void store_cleanup(struct store *store);
int store_toc_decode(struct store_toc *toc,const char *src,int srcc);
int store_validate_serial(const char *src,int srcc);
int store_decode(struct store *store,const char *src,int srcc);
int store_encode(char *dst,int dsta,const struct store *store);

int store_decode_base64_digit(char src);
extern const char store_base64_alphabet[64];
int store_decode_12bit(const char *src,int srcc); // 2 bytes in
int store_decode_18bit(const char *src,int srcc); // 3 bytes in
int store_decode_24bit(const char *src,int srcc); // 4 bytes in
int store_decode_30bit(const char *src,int srcc); // 5 bytes in
int store_checksum(const char *src,int srcc);

/* Maps info copied from game.
 * In real life, this is acquired dynamically from the resources.
 * Beware that mapid are not contiguous within each plane, they overlap in a few places.
 ***************************************************************************/
 
#define PLANEC 5
#define MAPW 20
#define MAPH 12

extern const struct plane {
  int id; // 0..4, my index here. NB this is not the NS_plane_* used in game.
  int w,h; // In maps.
  const uint16_t *v; // mapid
} planev[PLANEC];

const struct plane *plane_for_mapid(int *lng,int *lat,int mapid);
const int plane_contains_mapid(int *lng,int *lat,const struct plane *plane,int mapid);

/* Tool globals.
 ***************************************************************************/

extern struct g {
  const char *exename;
} g;

#endif

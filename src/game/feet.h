/* feet.h
 * Tracks the quantized position of all solid sprites for the purpose of actuating poi (treadles and such).
 */

#ifndef FEET_H
#define FEET_H

// Global singleton, (g.feet).
struct feet {
  uint8_t z;
  struct foot {
    int x,y;
    struct sprite *sprite; // For tracking during update. Unsafe to use after.
  } *footv;
  int footc,foota;
  struct foot *tmpv;
  int tmpc,tmpa;
  struct poi {
    int x,y;
    uint8_t opcode;
    const uint8_t *arg; // owned by map
    int argc;
  } *poiv; // Sorted by (y,x).
  int poic,poia;
};

void feet_reset();
void feet_update();

#endif

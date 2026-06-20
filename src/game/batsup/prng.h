/* prng.h
 * Use rand() as usual most of the time.
 * If you need a controlled, deterministic PRNG, use this instead.
 */
 
#ifndef PRNG_H
#define PRNG_H

#include <stdint.h>

/* Initialize with any nonzero seed.
 */
struct prng {
  uint32_t v;
};

int prng_next(struct prng *prng);

// Convenience, just prng_next modded by (limit).
int prng_range(struct prng *prng,int limit);

#endif

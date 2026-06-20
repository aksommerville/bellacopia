#include "prng.h"

int prng_next(struct prng *prng) {
  prng->v^=prng->v<<13;
  prng->v^=prng->v>>17;
  prng->v^=prng->v<<5;
  return prng->v&0x7fffffff;
}

int prng_range(struct prng *prng,int limit) {
  if (limit<2) return 0;
  prng->v^=prng->v<<13;
  prng->v^=prng->v>>17;
  prng->v^=prng->v<<5;
  return (prng->v&0x7fffffff)%limit;
}

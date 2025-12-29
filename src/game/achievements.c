#include "game.h"

/* Generate achievements texture.
 */
 
int bm_achievements_generate() {
  return font_render_to_texture(0,g.font,"TODO: This will be the achievements report, eg how long have you been playing, how much stuff have you done...",-1,FBW>>1,FBH>>1,0x000000ff);
}

/* battle.h
 * Generic interface for battles.
 * Usually you'll interact with modal_battle, not directly with this.
 */
 
#ifndef BATTLE_H
#define BATTLE_H

struct battle;
struct battle_type;
struct battle_args;

#define BATTLE_UNIVERSAL_TIMEOUT 60.0

struct battle_args {
  uint8_t difficulty; // Scale the general difficulty, presumably applicable to both players. 0..128..255 = easy..default..hard.
  uint8_t bias; // Give one player an advantage over the other. 0..128..255 = left easy .. neutral .. right easy.
  uint8_t lctl,rctl; // Input source. 0=CPU, or player id.
  uint8_t lface,rface; // Hero appearance. NS_face_*. Might be limited to (0,1,2)=(monster,dot,princess). Best effort.
};

struct battle {
  const struct battle_type *type;
  struct battle_args args; // Copied from request; safe to write.
  int outcome; // -2 until established. Then (-1,0,1) = (right wins, tie, left wins). Play should stop immediately when set.
};

struct battle_type {
  const char *name; // For logging and such; do not display to user.
  int objlen;
  int strix_name; // For display to user.
  int no_article; // Don't use an indefinite article when discussing the game in English.
  int no_contest; // Don't call it "X Contest", just "X".
  int no_timeout; // Suppress default timeout. You must guarantee to terminate when unattended.
  int support_pvp; // Player-vs-player supported. Only ones that shouldn't are the very asymmetric ones where a second player would have nothing to do.
  int support_cvc; // CPU-vs-CPU supported. All battles should do this, I'm not sure what kind of game couldn't be played automatically.
  
  void (*del)(struct battle *battle);
  int (*init)(struct battle *battle);
  
  /* Update and render are both required.
   * Render must overwrite the whole framebuffer.
   * Caller may continue to update and render after you set (outcome); battle should gracefully noop when set.
   */
  void (*update)(struct battle *battle,double elapsed);
  void (*render)(struct battle *battle);
};

const struct battle_type *battle_type_by_id(int battle); // NS_battle_*

void battle_del(struct battle *battle);

struct battle *battle_new(
  const struct battle_type *type,
  const struct battle_args *args
);

/* Produce text for display to the user.
 * Short is eg "Clapping", where long would be "a Clapping contest".
 * Savvy to the global language.
 */
int battle_type_describe_short(char *dst,int dsta,const struct battle_type *type);
int battle_type_describe_long(char *dst,int dsta,const struct battle_type *type);

/* Digest (difficulty,bias) to produce two "skill" values in 0..1.
 * For symmetric games that want to express difficulty as performance by each player.
 */
void battle_normalize_bias(double *lskill,double *rskill,const struct battle *battle);

/* Digest (difficulty,bias) to produce a single "difficulty" value in 0..1.
 * If bias is used, our answer discusses the left player.
 */
double battle_scalar_difficulty(const struct battle *battle);

#endif

#ifndef BATTLE_H
#define BATTLE_H

#define BATTLE_FLAG_ONEPLAYER      0x0001
#define BATTLE_FLAG_TWOPLAYER      0x0002
#define BATTLE_FLAG_AWKWARD_NAME   0x0004 /* If set, the battle's name is its name. Otherwise it's "a YADDAYADDA contest". */

struct battle_type {
  const char *name; // For diagnostics only; user-visible text must come from resources.
  int battletype;
  int flags;
  int foe_name_strix;
  int battle_name_strix;
  
  /* Both required.
   * (init) must return a non-null context object on success. Entirely opaque to owner.
   * (handicap) in 0..128..255.
   * (cb_finish) is required. Implementation must call it with (-1,0,1) when the game is over. (-1=foe wins, 0=draw, 1=hero wins).
   * (del) will only be called if (init) returned non-null, ie we'll never call it with null.
   */
  void *(*init)(
    int playerc,int handicap,
    void (*cb_finish)(void *userdata,int outcome),
    void *userdata
  );
  void (*del)(void *ctx);
  
  /* Both required.
   * (render) is expected to fill the entire framebuffer.
   */
  void (*update)(void *ctx,double elapsed);
  void (*render)(void *ctx);
};

const struct battle_type *battle_type_by_id(int battletype);

#define _(tag) extern const struct battle_type battle_type_##tag;
FOR_EACH_BATTLETYPE
#undef _

#endif

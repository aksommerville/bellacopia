/* sprite_treadlepass.c
 * Invisible controller for a challenge where you must press a bunch of treadles in the right order.
 */
 
#include "game/bellacopia.h"

#define TREADLE_LIMIT 16

struct sprite_treadlepass {
  struct sprite hdr;
  int fldid;
  int treadlec;
  int store_listener;
  int seq; // Position in (order).
  int order[TREADLE_LIMIT]; // Value is (0..treadlec-1), the order the buttons must be pressed. No dups.
};

#define SPRITE ((struct sprite_treadlepass*)sprite)

static void _treadlepass_del(struct sprite *sprite) {
  store_unlisten(SPRITE->store_listener);
}

static void treadlepass_cb_store(char type,int id,int value,void *userdata) {
  struct sprite *sprite=userdata;
  if (type=='f') {
    if (value&&(id>=SPRITE->fldid+1)&&(id<=SPRITE->fldid+SPRITE->treadlec)) {
      if (store_get_fld(SPRITE->fldid)) return; // Already solved, stop doing things.
      
      // Start with (btnid), static per map.
      int btnid=id-SPRITE->fldid-1;
      
      // Find that in (order) and use the (order) position from here on.
      int seq=-1;
      int i=0; for (;i<SPRITE->treadlec;i++) if (SPRITE->order[i]==btnid) {
        seq=i;
        break;
      }
      
      // Is (seq) what we're expecting?
      if (!seq) SPRITE->seq=1; // Touch our first treadle, right or wrong, we expect the second now.
      else if (seq!=SPRITE->seq) SPRITE->seq=0;
      else { // Hit the correct (non-first) treadle.
        SPRITE->seq=seq+1;
        if (SPRITE->seq>=SPRITE->treadlec) {
          SPRITE->seq=0;
          bm_sound(RID_sound_secret);
          store_set_fld(SPRITE->fldid,1);
          g.camera.mapsdirty=1;
        }
      }
    }
  }
}

static int _treadlepass_init(struct sprite *sprite) {
  SPRITE->fldid=(sprite->arg[0]<<8)|sprite->arg[1];
  SPRITE->treadlec=(sprite->arg[2]<<8)|sprite->arg[3];
  if (store_get_fld(SPRITE->fldid)) { // We're already solved. No need to exist.
    return -1;
  }
  if ((SPRITE->treadlec<2)||(SPRITE->treadlec>TREADLE_LIMIT)) {
    fprintf(stderr,"%s: Invalid treadle count %d, must be 2..%d\n",__func__,SPRITE->treadlec,TREADLE_LIMIT);
    return -1;
  }
  
  // Generate a fresh random order for the treadles.
  int available[TREADLE_LIMIT];
  int i=SPRITE->treadlec;
  while (i-->0) available[i]=i;
  int availablec=SPRITE->treadlec;
  for (i=0;i<SPRITE->treadlec;i++) {
    int availablep=rand()%availablec;
    SPRITE->order[i]=available[availablep];
    availablec--;
    memmove(available+availablep,available+availablep+1,sizeof(int)*(availablec-availablep));
  }
  
  SPRITE->store_listener=store_listen('f',treadlepass_cb_store,sprite);
  return 0;
}

static void _treadlepass_update(struct sprite *sprite,double elapsed) {
  // Hm, turns out we don't actually need an update; the store callback is enough.
  // But we do need to stay in GRP(update) in order for game_get_hints_override_position() to find us.
}

static void _treadlepass_render(struct sprite *sprite,int x,int y) {
}

const struct sprite_type sprite_type_treadlepass={
  .name="treadlepass",
  .objlen=sizeof(struct sprite_treadlepass),
  .del=_treadlepass_del,
  .init=_treadlepass_init,
  .update=_treadlepass_update,
  .render=_treadlepass_render,
};

int sprite_treadlepass_hint(int *x,int *y,const struct sprite *sprite,int fld) {
  if (!sprite||(sprite->type!=&sprite_type_treadlepass)) return -1;
  if (fld!=SPRITE->fldid) return -1;
  if ((SPRITE->seq<0)||(SPRITE->seq>=SPRITE->treadlec)) return -1;
  int btnfldid=SPRITE->fldid+1+SPRITE->order[SPRITE->seq];
  uint8_t arg2=btnfldid>>8;
  uint8_t arg3=btnfldid;
  
  /* The "feet" system keeps a neat list of POI currently in scope, that we can query.
   */
  const struct poi *poi=g.feet.poiv;
  int i=g.feet.poic;
  for (;i-->0;poi++) {
    if (poi->opcode!=CMD_map_treadle) continue;
    if (poi->arg[2]!=arg2) continue;
    if (poi->arg[3]!=arg3) continue;
    *x=poi->x;
    *y=poi->y;
    return 0;
  }
  // Ooops treadle not found.
  return -1;
}

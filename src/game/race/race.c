#include "game/bellacopia.h"
#include "game/race/race.h"

/* Hide and neutralize the real-game Dot and Moon sprites.
 */
 
static void race_hide_game_sprites() {
  fprintf(stderr,"%s\n",__func__);
  struct sprite **p=GRP(keepalive)->sprv;
  int i=GRP(keepalive)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->type==&sprite_type_hero) {
      fprintf(stderr,"hiding hero %p\n",sprite);
      sprite_group_remove(GRP(visible),sprite);
      sprite_group_remove(GRP(update),sprite);
      sprite_group_remove(GRP(solid),sprite);
      sprite_group_remove(GRP(hero),sprite);
    } else if ((sprite->type==&sprite_type_npc)&&(sprite_npc_get_activity(sprite)==NS_activity_moonsong)) {
      fprintf(stderr,"hiding moonsong %p\n",sprite);
      sprite_group_remove(GRP(visible),sprite);
      sprite_group_remove(GRP(update),sprite);
      sprite_group_remove(GRP(solid),sprite);
    }
  }
}

static void race_restore_game_sprites() {
  fprintf(stderr,"%s\n",__func__);
  struct sprite **p=GRP(keepalive)->sprv;
  int i=GRP(keepalive)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->type==&sprite_type_hero) {
      fprintf(stderr,"restoring hero %p\n",sprite);
      sprite_group_add(GRP(visible),sprite);
      sprite_group_add(GRP(update),sprite);
      sprite_group_add(GRP(solid),sprite);
      sprite_group_add(GRP(hero),sprite);
    } else if ((sprite->type==&sprite_type_npc)&&(sprite_npc_get_activity(sprite)==NS_activity_moonsong)) {
      fprintf(stderr,"restoring moonsong %p\n",sprite);
      sprite_group_add(GRP(visible),sprite);
      sprite_group_add(GRP(update),sprite);
      sprite_group_add(GRP(solid),sprite);
    } else if (sprite->type==&sprite_type_racer) {
      fprintf(stderr,"killing racer %p\n",sprite);
      sprite_kill_soon(sprite);
    }
  }
}

/* Spawn the racer sprites.
 */
 
static uint8_t racerargs[8]={0};
 
static int race_spawn_sprites(int raceid) {
  double x=g.camera.fx;//TODO per (racerid)
  double y=g.camera.fy;
  racerargs[0]=1; // Dot controller.
  racerargs[1]=NS_face_dot;
  racerargs[2]=0;
  racerargs[3]=0;
  racerargs[4]=0; // Moon controller.
  racerargs[5]=NS_face_moonsong;
  racerargs[6]=0;
  racerargs[7]=0;
  struct sprite *dot=sprite_spawn(x-0.5,y,0,racerargs,4,&sprite_type_racer,0,0);
  struct sprite *moon=sprite_spawn(x+0.5,y,0,racerargs+4,4,&sprite_type_racer,0,0);
  if (!dot||!moon) return -1;
  return 0;
}

/* Begin race.
 */
 
int race_begin(int raceid) {
  fprintf(stderr,"%s %d\n",__func__,raceid);
  race_hide_game_sprites();
  if (race_spawn_sprites(raceid)<0) {
    race_restore_game_sprites();
    return -1;
  }
  g.raceid=raceid;
  return 0;
}

/* End race.
 */
 
void race_end() {
  fprintf(stderr,"%s\n",__func__);
  race_restore_game_sprites();
  g.raceid=0;
}

#include "game/bellacopia.h"
#include "game/race/race.h"

/* Global index of races.
 */
 
#define RACE_RES_LIMIT 16
#define CHECKPOINT_LIMIT 8
#define TRACK_LIMIT 32
 
static struct races {
  int ready;
  struct race {
    int rid;
    int plane;
    int lapc;
    int target; // sec
    int winfld;
    int timefld16;
    uint8_t startdir; // 0x40,0x10,0x08,0x02. Which direction to face initially, determined by relative positions of first two checkpoints.
    struct checkpoint {
      int mapid;
      double x,y; // In plane meters, center of marked tile.
    } checkpointv[CHECKPOINT_LIMIT];
    int checkpointc; // All races have at least two, or we choke at init.
    struct track {
      int mapid;
      double x,y;
    } trackv[TRACK_LIMIT];
    int trackc;
  } racev[RACE_RES_LIMIT];
  int racec;
  struct race *race; // WEAK, points into (racev) when active. Must match (g.raceid).
  double countdown;
  double cooldown;
} races={0};

/* Add one map's commands to a new race.
 * This is called for every map in the plane; most will be noop.
 */
 
static int race_add_map(struct race *race,struct map *map) {
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_map_race: {
          if (cmd.arg[2]!=race->rid) break; // Checkpoint for some other race.
          int seq=cmd.arg[3];
          if ((seq<0)||(seq>=CHECKPOINT_LIMIT)) {
            fprintf(stderr,"race:%d invalid checkpoint sequence %d. Must be in 0..%d (or update CHECKPOINT_LIMIT in %s)\n",race->rid,seq,CHECKPOINT_LIMIT-1,__FILE__);
            return -1;
          }
          if (seq>=race->checkpointc) race->checkpointc=seq+1;
          struct checkpoint *checkpoint=race->checkpointv+seq;
          if (checkpoint->x>0.0) {
            fprintf(stderr,"race:%d duplicate checkpoint sequence %d (map:%d and map:%d)\n",race->rid,seq,checkpoint->mapid,map->rid);
            return -1;
          }
          checkpoint->mapid=map->rid;
          checkpoint->x=map->lng*NS_sys_mapw+cmd.arg[0]+0.5;
          checkpoint->y=map->lat*NS_sys_maph+cmd.arg[1]+0.5;
        } break;
      case CMD_map_track: {
          if (cmd.arg[2]!=race->rid) break; // Checkpoint for some other race.
          int seq=cmd.arg[3];
          if ((seq<0)||(seq>=TRACK_LIMIT)) {
            fprintf(stderr,"race:%d invalid track sequence %d. Must be in 0..%d (or update TRACK_LIMIT in %s)\n",race->rid,seq,TRACK_LIMIT-1,__FILE__);
            return -1;
          }
          if (seq>=race->trackc) race->trackc=seq+1;
          struct track *track=race->trackv+seq;
          if (track->x>0.0) {
            fprintf(stderr,"race:%d duplicate track sequence %d (map:%d and map:%d)\n",race->rid,seq,track->mapid,map->rid);
            return -1;
          }
          track->mapid=map->rid;
          track->x=map->lng*NS_sys_mapw+cmd.arg[0]+0.5;
          track->y=map->lat*NS_sys_maph+cmd.arg[1]+0.5;
        } break;
    }
  }
  return 0;
}

/* Decode and validate one race.
 * Log all errors.
 */
 
static int race_decode(struct race *race,const uint8_t *v,int c) {
  
  // Defaults.
  race->plane=-1;
  race->lapc=1;
  race->target=0;
  race->checkpointc=0;
  race->trackc=0;
  race->winfld=0;
  race->timefld16=0;
  
  // Read commands.
  struct cmdlist_reader reader={.v=v,.c=c};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      case CMD_race_plane: race->plane=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_race_laps: race->lapc=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_race_target: race->target=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_race_win: race->winfld=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_race_time: race->timefld16=(cmd.arg[0]<<8)|cmd.arg[1]; break;
    }
  }
  
  // Find checkpoints in plane.
  struct plane *plane=plane_by_position(race->plane);
  if (!plane) {
    fprintf(stderr,"race:%d: plane %d not found\n",race->rid,race->plane);
    return -1;
  }
  struct map *map=plane->v;
  int i=plane->w*plane->h;
  for (;i-->0;map++) {
    if (race_add_map(race,map)<0) return -1;
  }
  
  /* Validate:
   *  - Must have at least 2 checkpoints.
   *  - Checkpoint sequence must be fully populated. Detect via (x<0.5), that can only happen if skipped.
   *  - Track has exactly the same requirements as checkpoint.
   *  - lapc at least 1.
   * Not validating that checkpoints are on passable terrain or that there exists a path between them. Just don't make tracks like that, m'k?
   * Not validating target time. I think we should allow zero as some special "untimed race", maybe we'll want something like that.
   */
  if (race->lapc<1) {
    fprintf(stderr,"race:%d invalid lapc %d\n",race->rid,race->lapc);
    return -1;
  }
  if (race->checkpointc<2) {
    fprintf(stderr,"race:%d needs at least 2 checkpoints, as CMD_map_race in plane %d\n",race->rid,race->plane);
    return -1;
  }
  struct checkpoint *checkpoint=race->checkpointv;
  int seq=0;
  for (i=race->checkpointc;i-->0;checkpoint++,seq++) {
    if (checkpoint->x<0.5) {
      fprintf(stderr,"race:%d missing checkpoint %d\n",race->rid,seq);
      return -1;
    }
  }
  if (race->trackc<2) {
    fprintf(stderr,"race:%d needs at least 2 track points, as CMD_map_track in plane %d\n",race->rid,race->plane);
    return -1;
  }
  struct track *track=race->trackv;
  for (i=race->trackc,seq=0;i-->0;track++,seq++) {
    if (track->x<0.5) {
      fprintf(stderr,"race:%d missing track point %d\n",race->rid,seq);
      return -1;
    }
  }
  
  /* Set (startdir) per (checkpointv[0,1]).
   */
  double dx=race->checkpointv[1].x-race->checkpointv[0].x;
  double dy=race->checkpointv[1].y-race->checkpointv[0].y;
  double adx=dx; if (adx<0.0) adx=-adx;
  double ady=dy; if (ady<0.0) ady=-ady;
  if (adx>ady) {
    if (dx<0.0) race->startdir=0x10;
    else race->startdir=0x08;
  } else {
    if (dy<0.0) race->startdir=0x40;
    else race->startdir=0x02;
  }
  
  return 0;
}

/* Get a race resource by id.
 * Lazy-loads the store first time you call.
 */
 
static struct race *race_get(int rid) {
  
  // Load store if we haven't yet.
  if (!races.ready) {
    races.ready=1;
    int resp=res_search(EGG_TID_race,0);
    if (resp<0) resp=-resp-1;
    const struct rom_entry *res=g.resv+resp;
    for (;(resp<g.resc)&&(res->tid==EGG_TID_race);resp++,res++) {
      if (races.racec>=RACE_RES_LIMIT) {
        fprintf(stderr,"%s:%d: Too many races, limit %d.\n",__FILE__,__LINE__,RACE_RES_LIMIT);
        break;
      }
      struct race *race=races.racev+races.racec++;
      race->rid=res->rid;
      if (race_decode(race,res->v,res->c)<0) {
        races.ready=-1;
        races.racec=0;
        return 0;
      }
    }
  }
  
  // Sticky error?
  if (races.ready<0) return 0;
  
  /* Find the race in our store.
   */
  struct race *race=0;
  int lo=0,hi=races.racec;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    struct race *q=races.racev+ck;
         if (rid<q->rid) hi=ck;
    else if (rid>q->rid) lo=ck+1;
    else return q;
  }
  return 0;
}

/* Hide and neutralize the real-game Dot and Moon sprites.
 */
 
static void race_hide_game_sprites() {
  struct sprite **p=GRP(keepalive)->sprv;
  int i=GRP(keepalive)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->type==&sprite_type_hero) {
      sprite_group_remove(GRP(visible),sprite);
      sprite_group_remove(GRP(update),sprite);
      sprite_group_remove(GRP(solid),sprite);
      sprite_group_remove(GRP(hero),sprite);
    } else if ((sprite->type==&sprite_type_npc)&&(sprite_npc_get_activity(sprite)==NS_activity_moonsong)) {
      sprite_group_remove(GRP(visible),sprite);
      sprite_group_remove(GRP(update),sprite);
      sprite_group_remove(GRP(solid),sprite);
    }
  }
}

static void race_restore_game_sprites() {
  struct sprite *hero=0;
  struct sprite **p=GRP(keepalive)->sprv;
  int i=GRP(keepalive)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->type==&sprite_type_hero) {
      hero=sprite;
      sprite_group_add(GRP(visible),sprite);
      sprite_group_add(GRP(update),sprite);
      sprite_group_add(GRP(solid),sprite);
      sprite_group_add(GRP(hero),sprite);
    } else if ((sprite->type==&sprite_type_npc)&&(sprite_npc_get_activity(sprite)==NS_activity_moonsong)) {
      sprite_group_add(GRP(visible),sprite);
      sprite_group_add(GRP(update),sprite);
      sprite_group_add(GRP(solid),sprite);
    } else if (sprite->type==&sprite_type_racer) {
      sprite_group_remove(GRP(hero),sprite); // Will happen at kill, but we need it now so the camera knows which hero is real.
      sprite_kill_soon(sprite);
    }
  }
  /* One more thing: Any monsters that wandered near our landing site, zap them.
   * They are probably visible, the user will see this happen. But letting them live is pretty annoying.
   * This is a separate pass because the hero isn't necessarily first in any list.
   */
  if (hero) {
    const double killradius=2.0;
    const double killradius2=killradius*killradius;
    for (p=GRP(update)->sprv,i=GRP(update)->sprc;i-->0;p++) {
      struct sprite *sprite=*p;
      if (sprite->type==&sprite_type_monster) {
        double dx=sprite->x-hero->x;
        if ((dx<-killradius)||(dx>killradius)) continue;
        double dy=sprite->y-hero->y;
        if ((dy<-killradius)||(dy>killradius)) continue;
        double d2=dx*dx+dy*dy;
        if (d2>killradius2) continue;
        sprite_kill_soon(sprite);
      }
    }
  }
}

/* Spawn the racer sprites.
 */
 
static uint8_t racerargs[8]={0};
 
static int race_spawn_sprites(struct race *race) {

  // Start around the first checkpoint. Horizontal orientation, Dot goes on top, and vertical, she goes left.
  double x=race->checkpointv[0].x;
  double y=race->checkpointv[0].y;
  double dotx,doty,moonx,moony;
  if (race->startdir&0x42) {
    dotx=x-0.5;
    moonx=x+0.5;
    doty=moony=y;
  } else {
    dotx=moonx=x;
    doty=y-0.5;
    moony=y+0.5;
  }
  
  racerargs[0]=1; // Dot controller.
  racerargs[1]=NS_face_dot;
  racerargs[2]=race->startdir;
  racerargs[3]=0;
  racerargs[4]=0; // Moon controller.
  racerargs[5]=NS_face_moonsong;
  racerargs[6]=race->startdir;
  racerargs[7]=0;
  struct sprite *dot=sprite_spawn(dotx,doty,0,racerargs,4,&sprite_type_racer,0,0);
  struct sprite *moon=sprite_spawn(moonx,moony,0,racerargs+4,4,&sprite_type_racer,0,0);
  if (!dot||!moon) return -1;
  return 0;
}

/* Begin race.
 */
 
int race_begin(int raceid) {
  struct race *race=race_get(raceid);
  if (!race) {
    fprintf(stderr,"%s: race:%d not found\n",__func__,raceid);
    return -1;
  }
  race_hide_game_sprites();
  races.race=race; // Must set before spawning sprites.
  g.raceid=raceid;
  if (race_spawn_sprites(race)<0) {
    races.race=0;
    g.raceid=0;
    race_restore_game_sprites();
    return -1;
  }
  races.countdown=2.999;
  races.cooldown=0.0;
  bm_song_gently(0);
  return 0;
}

/* End race.
 */

void race_end() {

  // Commit win flag and time, if improved.
  int arg=0;
  if (races.race->winfld||races.race->timefld16) {
    struct race_status status={0};
    race_get_status(&status);
    if (status.lapp>status.lapc) { // Confirm we actually completed this race; we do get called on cancellation too.
      if (!status.opponent_finished||(status.racetime<=status.opponenttime)) {
        arg|=1; // Dot wins. Ties count as winning, they're unlikely enough to not worry about it.
        if (races.race->winfld) store_set_fld(races.race->winfld,1);
      } else {
        arg|=2; // Moon wins.
      }
      if (races.race->timefld16) {
        int time8ms=(int)(status.racetime*(1000.0/8.0));
        if (time8ms>0xffff) time8ms=0xffff;
        if (time8ms>0) {
          int pvtime=store_get_fld16(races.race->timefld16);
          if (!pvtime) arg|=8; // First time.
          if (!pvtime||(time8ms<pvtime)) {
            arg|=4; // New high score (or first play).
            store_set_fld16(races.race->timefld16,time8ms);
          }
        }
      }
    }
  }
  
  race_restore_game_sprites();
  g.raceid=0;
  races.race=0;
  bm_song_gently(bm_song_for_outerworld());
  
  // If we have something to report, trigger it. Important to do this after restoring the sprites; it impacts camera.
  if (arg) {
    game_begin_activity(NS_activity_endrace,arg,0);
  }
}

/* Check completion.
 * Sprites will call this when one racer finishes.
 */
 
void race_check_completion(int stop_soon) {
  if (stop_soon) races.cooldown=5.0; // Dot is done. Give Moon a few seconds, but do stop without her before long.
  struct sprite **p=GRP(update)->sprv;
  int i=GRP(update)->sprc;
  for (;i-->0;p++) {
    struct sprite *sprite=*p;
    if (sprite->type!=&sprite_type_racer) continue;
    if (!sprite_racer_is_finished(sprite)) return;
  }
  races.cooldown=3.0;
}

/* Update.
 */
 
void race_update(double elapsed) {
  if (races.countdown>0.0) {
    races.countdown-=elapsed;
    if (races.countdown<=0.0) {
      bm_song_gently(RID_song_death_rattle);//TODO song for races
    }
  }
  if (races.cooldown>0.0) {
    races.cooldown-=elapsed;
    if (races.cooldown<=0.0) {
      race_end();
    }
  }
}

/* Trivial accessors against metadata.
 */
 
int race_fld_by_id(int raceid) {
  struct race *race=race_get(raceid);
  if (!race) return 0;
  return race->winfld;
}
 
int race_fld_by_index(int p) {
  if (p<0) return 0;
  if (!races.ready) race_get(1); // Force load.
  if (p>=races.racec) return 0;
  return races.racev[p].winfld;
}

int race_fld16_by_index(int p) {
  if (p<0) return 0;
  if (!races.ready) race_get(1); // Force load.
  if (p>=races.racec) return 0;
  return races.racev[p].timefld16;
}

/* Trivial accessors against active race.
 */
 
int race_get_lapc() {
  if (!races.race) return 0;
  return races.race->lapc;
}

int race_get_checkpointc() {
  if (!races.race) return 0;
  return races.race->checkpointc;
}

int race_get_checkpoint(double *x,double *y,int p) {
  if (!races.race) return -1;
  if (p<0) return -1;
  if (p>=races.race->checkpointc) return -1;
  *x=races.race->checkpointv[p].x;
  *y=races.race->checkpointv[p].y;
  return 0;
}

int race_get_trackc() {
  if (!races.race) return 0;
  return races.race->trackc;
}

int race_get_track(double *x,double *y,int p) {
  if (!races.race) return -1;
  if (p<0) return -1;
  if (p>=races.race->trackc) return -1;
  *x=races.race->trackv[p].x;
  *y=races.race->trackv[p].y;
  return 0;
}

int race_get_target_time() {
  if (!races.race) return 0;
  return races.race->target;
}

double race_get_countdown() {
  return races.countdown;
}

/* Get status pertinent to the human racer.
 */
 
void race_get_status(struct race_status *status) {
  if (!races.race) {
    status->lapp=0;
    status->lapc=0;
    status->laptime=0.0;
    status->racetime=0.0;
    status->opponenttime=0.0;
    status->opponent_finished=0;
  } else {
    status->lapc=races.race->lapc;
    struct sprite **p=GRP(update)->sprv;
    int i=GRP(update)->sprc;
    for (;i-->0;p++) {
      struct sprite *sprite=*p;
      if (sprite->type!=&sprite_type_racer) continue;
      if (sprite_racer_is_human(sprite)) {
        status->lapp=sprite_racer_get_lapp(sprite);
        status->laptime=sprite_racer_get_lap_time(sprite);
        status->racetime=sprite_racer_get_race_time(sprite);
      } else {
        status->opponenttime=sprite_racer_get_race_time(sprite);
        status->opponent_finished=(sprite_racer_get_lapp(sprite)>status->lapc);
      }
    }
  }
}

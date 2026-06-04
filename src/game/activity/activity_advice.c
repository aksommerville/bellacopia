#include "activity_internal.h"

/* Crystal Ball.
 */
 
void begin_crystal() {
  modal_spawn(&modal_type_crystal,0,0);
}

/* Get relevant mapid for cartographer.
 * This is a crazy expensive operation if we do it from scratch.
 * So our global (carto) lists all mapid,fldid pairs for all possible secrets.
 */

static void cb_carto_fld(char type,int id,int value,void *userdata);
 
#define CARTO_LIMIT 128
 
static struct carto {
  int ready;
  struct carto_entry {
    int mapid,fldid;
  } entryv[CARTO_LIMIT];
  int entryc;
} carto={0};
 
void cartographer_reset() {
  memset(&carto,0,sizeof(carto));
  store_listen('f',cb_carto_fld,0);
}

static int carto_entry_cmp(const void *a,const void *b) {
  const struct carto_entry *A=a,*B=b;
  if (A->mapid<B->mapid) return -1;
  if (A->mapid>B->mapid) return 1;
  if (A->fldid<B->fldid) return -1;
  if (A->fldid>B->fldid) return 1;
  return 0;
}

static int carto_add_entry(int mapid,int fldid) {
  if (carto.entryc>=CARTO_LIMIT) return -1;
  struct carto_entry *entry=carto.entryv+carto.entryc++;
  entry->mapid=mapid;
  entry->fldid=fldid;
  return 0;
}

static void cartographer_find_entries_for_map(const struct map *map) {
  int mapid=map->parent?map->parent:map->rid;

  /* Barrel hats are two commands: CMD_map_barrelhat and CMD_map_switchable. We need both, and they can be in either order.
   * So we'll stash all the barrelhat commands in the first pass, and do a second pass to find the fldid for them.
   */
  int barrelhatv[16]; // (x<<8)|y
  int barrelhatc=0;
  
  // First pass should catch most things.
  struct cmdlist_reader reader={.v=map->cmd,.c=map->cmdc};
  struct cmdlist_entry cmd;
  while (cmdlist_reader_next(&cmd,&reader)>0) {
    switch (cmd.opcode) {
      // Not going to include zookeeper, tho initially I planned to. It's easier if every secret has a single fldid. (and zookeeper aren't really secret, are they?)
      case CMD_map_barrelhat: if (barrelhatc<16) barrelhatv[barrelhatc++]=(cmd.arg[0]<<8)|cmd.arg[1]; break;
      case CMD_map_compass: {
          int compass=(cmd.arg[2]<<8)|cmd.arg[3];
          int fldid=(cmd.arg[4]<<8)|cmd.arg[5];
          if (fldid&&(
            (compass==NS_compass_heartcontainer)|| // All heart container should be flagged like this.
            (compass==NS_compass_gold) // This is only for golds that don't use buriedtreasure; I think won't be any.
          )) carto_add_entry(mapid,fldid);
        } break;
      case CMD_map_buriedtreasure: carto_add_entry(mapid,(cmd.arg[2]<<8)|cmd.arg[3]); break;
      case CMD_map_bump: {
          int activity=(cmd.arg[2]<<8)|cmd.arg[3];
          int fldid=(cmd.arg[4]<<8)|cmd.arg[5];
          if (activity==NS_activity_tree) {
            carto_add_entry(mapid,fldid);
          }
        } break;
      case CMD_map_sprite: {
          int spriteid=(cmd.arg[2]<<8)|cmd.arg[3];
          if (spriteid==RID_sprite_bridget) {
            int fldid_start=(cmd.arg[6]<<8)|cmd.arg[7];
            int fldid_done=fldid_start+7; // Ensure that the "done" fields are +7 from their "start", I don't want to do this right.
            carto_add_entry(mapid,fldid_done);
          }
        } break;
    }
  }
  
  // Do we need a second pass?
  if (barrelhatc) {
    reader=(struct cmdlist_reader){.v=map->cmd,.c=map->cmdc};
    while (cmdlist_reader_next(&cmd,&reader)>0) {
      switch (cmd.opcode) {
        case CMD_map_switchable: {
            int pos=(cmd.arg[0]<<8)|cmd.arg[1];
            int i=barrelhatc;
            while (i-->0) {
              if (pos==barrelhatv[i]) {
                int fldid=(cmd.arg[2]<<8)|cmd.arg[3];
                carto_add_entry(mapid,fldid);
                break;
              }
            }
          } break;
      }
    }
  }
}

static void carto_require() {
  if (carto.ready) return;
  carto.ready=1;
  struct map **map=g.mapstore.byidv;
  int i=g.mapstore.byidc;
  for (;i-->0;map++) {
    if (!*map) continue;
    cartographer_find_entries_for_map(*map);
  }
  // (byidv) is of course sorted by id already, but we record parent IDs, so our list is not necessarily in order yet.
  qsort(carto.entryv,carto.entryc,sizeof(struct carto_entry),carto_entry_cmp);
}

static void cb_carto_fld(char type,int id,int value,void *userdata) {
  // If a secret field gets set, and it's one of the three currently highlighted, remove that highlight.
  if (type!='f') return;
  if (!value) return;
  if (!g.camera.map) return;
  int mapid=mapid_jigsawable(g.camera.map->rid);
  int fld16=0;
  int mapid1=store_get_fld16(NS_fld16_carto1);
  int mapid2=store_get_fld16(NS_fld16_carto2);
  int mapid3=store_get_fld16(NS_fld16_carto3);
  if (!mapid1&&!mapid2&&!mapid3) return; // Nothing highlighted right now.
       if (mapid==mapid1) fld16=NS_fld16_carto1;
  else if (mapid==mapid2) fld16=NS_fld16_carto2;
  else if (mapid==mapid3) fld16=NS_fld16_carto3;
  else {
    // Alas, it is possible to set secret-bearing flags while standing on a different map. eg if you hookshot Bridget across the river.
    // So when a field doesn't match the easy way, examine all carto entries.
    carto_require();
    const struct carto_entry *entry=carto.entryv;
    int i=carto.entryc;
    for (;i-->0;entry++) {
      if (entry->fldid!=id) continue;
           if (entry->mapid==mapid1) store_set_fld16(NS_fld16_carto1,0);
      else if (entry->mapid==mapid2) store_set_fld16(NS_fld16_carto2,0);
      else if (entry->mapid==mapid3) store_set_fld16(NS_fld16_carto3,0);
      else continue;
      break;
    }
    return;
  }
  // OK, a field was set and we're on one of the highlighted maps. Is this field a secret?
  carto_require();
  int lo=0,hi=carto.entryc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    const struct carto_entry *q=carto.entryv+ck;
         if (mapid<q->mapid) hi=ck;
    else if (mapid>q->mapid) lo=ck+1;
    else if (id<q->fldid) hi=ck;
    else if (id>q->fldid) lo=ck+1;
    else { // Got the secret!
      store_set_fld16(fld16,0);
      return;
    }
  }
}

static int cartographer_get_mapids(int *mapidv,int mapida,int *needmapc) {
  if (needmapc) *needmapc=0;
  if (mapida<1) return 0;
  
  /* Populate the cache if we don't have it yet.
   * This will only happen once per session.
   */
  carto_require();
  
  /* Scan the entire cache each time we talk to the cartographer.
   * Shouldn't be a big deal, performance-wise. The hard part is already done and cached.
   */
  int mapidc=0;
  const struct carto_entry *entry=carto.entryv;
  int i=carto.entryc;
  for (;i-->0;entry++) {
    if (mapidc&&(mapidv[mapidc-1]==entry->mapid)) continue; // List each mapid only once -- important!
    if (store_get_fld(entry->fldid)) continue; // Already got the treasure.
    if (!store_get_jigstore(entry->mapid)) { // Don't have the map.
      if (needmapc) (*needmapc)++;
      continue;
    }
    mapidv[mapidc++]=entry->mapid;
    if (mapidc>=mapida) return mapidc;
  }
  
  return mapidc;
}

/* Cartographer: Offers to mark three secrets on your map, for a price.
 */
 
static struct cartographer_context {
  int price;
  int mapidv[3];
  int mapidc;
} cartographer_context;

static int cb_cartographer(int optionid,void *userdata) {
  if (optionid!=4) return 0;
  
  int gold=store_get_fld16(NS_fld16_gold);
  if (gold<cartographer_context.price) {
    begin_dialogue(2,0);
    return 0;
  }
  gold-=cartographer_context.price;
  store_set_fld16(NS_fld16_gold,gold);
  
  if (cartographer_context.mapidc>=1) store_set_fld16(NS_fld16_carto1,cartographer_context.mapidv[0]);
  if (cartographer_context.mapidc>=2) store_set_fld16(NS_fld16_carto2,cartographer_context.mapidv[1]);
  if (cartographer_context.mapidc>=3) store_set_fld16(NS_fld16_carto3,cartographer_context.mapidv[2]);
  
  begin_dialogue(136,0);
  return 1;
}
 
void begin_cartographer(struct sprite *initiator) {
  
  /* First, if any secret is currently marked, you can have no more.
   * Must clear all three before we give you a new one.
   */
  if (store_get_fld16(NS_fld16_carto1)||store_get_fld16(NS_fld16_carto2)||store_get_fld16(NS_fld16_carto3)) {
    begin_dialogue(133,initiator);
    return;
  }
  
  /* Find all the maps with incomplete secrets, filtering to maps for which you have the jigpiece.
   * Also count maps with incomplete secrets but no jigpiece. Those influence our rejection message.
   */
  #define SECRET_LIMIT 128
  int mapidv[SECRET_LIMIT];
  int needmapc=0;
  int mapidc=cartographer_get_mapids(mapidv,SECRET_LIMIT,&needmapc);
  #undef SECRET_LIMIT
  
  /* If we didn't find any valid candidates, reject with either "no more secrets" or "finish your map".
   */
  if (!mapidc) {
    if (needmapc) {
      begin_dialogue(134,initiator);
    } else {
      begin_dialogue(135,initiator);
    }
    return;
  }
  
  /* Pick up to three secrets at random, and prepare a dialogue box.
   * At this point, we resolve parent references.
   */
  cartographer_context.mapidc=0;
  while ((mapidc>0)&&(cartographer_context.mapidc<3)) {
    int p=rand()%mapidc;
    cartographer_context.mapidv[cartographer_context.mapidc++]=mapid_jigsawable(mapidv[p]);
    mapidc--;
    memmove(mapidv+p,mapidv+p+1,sizeof(int)*(mapidc-p));
  }
  cartographer_context.price=20; // Could reduce if there's fewer than 3, but I'm thinking same price regardless.
  struct text_insertion insv[2]={
    {.mode='i',.i=cartographer_context.mapidc},
    {.mode='i',.i=cartographer_context.price},
  };
  struct modal_args_dialogue args={
    .rid=RID_strings_dialogue,
    .strix=132,
    .insv=insv,
    .insc=2,
    .speaker=initiator,
    .cb=cb_cartographer,
  };
  struct modal *modal=modal_spawn(&modal_type_dialogue,&args,sizeof(args));
  if (!modal) return;
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,4);
  modal_dialogue_add_option_string(modal,RID_strings_dialogue,5);
}

#include "game.h"

/* Encode or decode single entry.
 */
 
static int inventory_decode_1(struct inventory *dst,const char *src/*6*/) {
  uint8_t norm[6];
  uint8_t *ndst=norm;
  int i=6;
  for (;i-->0;ndst++,src++) {
    if ((*src<0x30)||(*src>=0x70)) return -1;
    *ndst=(*src)-0x30;
  }
  dst->itemid=(norm[0]<<2)|(norm[1]>>4);
  dst->limit=((norm[1]&0x0f)<<10)|(norm[2]<<4)|(norm[3]>>2);
  dst->quantity=((norm[3]&0x03)<<12)|(norm[4]<<6)|norm[5];
  return 0;
}

static int inventory_encode_1(char *dst/*6*/,const struct inventory *src) {
  // Force fields to the encodable size, and some further sanitization.
  int itemid=src->itemid;
  if (itemid<0) itemid=0;
  else if (itemid>0xff) itemid=0xff;
  int limit=src->limit;
  if (limit<0) limit=0;
  else if (limit>16383) limit=16383;
  int quantity=src->quantity;
  if (quantity<0) quantity=0;
  else if (quantity>16383) quantity=16383;
  if (!itemid) limit=quantity=0;
  dst[0]=0x30+(itemid>>2);
  dst[1]=0x30+((itemid&0x03)<<4)+(limit>>10);
  dst[2]=0x30+((limit&0x3f0)>>4);
  dst[3]=0x30+((limit&0x0f)<<2)+(quantity>>12);
  dst[4]=0x30+((quantity&0xfc0)>>6);
  dst[5]=0x30+(quantity&0x3f);
  return 6;
}

/* Decode inventory from provided text directly into the globals.
 * Globals must be zeroed initially, and are undefined if we fail.
 */
 
static int inventory_decode(const char *src,int srcc) {
  /* Up to 26 slots. First the equipped item, then the 25 inventory slots.
   * Allow short, as long as it lines up with a slot boundary.
   * For each slot:
   *   - 8 itemid
   *   - 14 limit
   *   - 14 quantity
   *   - = 36 bits.
   * Each slot encodes as 6 bytes of 0x30..0x6f. Looks like Base64 but it's not, not exactly.
   */
  if (srcc%6) return -1;
  int itemc=srcc/6;
  if (itemc<1) return 0;
  if (inventory_decode_1(&g.equipped,src)<0) return -1;
  itemc--;
  src+=6;
  if (itemc>INVENTORY_SIZE) {
    fprintf(stderr,"WARNING: Dropping %d unexpected items from encoded inventory.\n",itemc-INVENTORY_SIZE);
    itemc=INVENTORY_SIZE;
  }
  struct inventory *dst=g.inventoryv;
  for (;itemc-->0;dst++,src+=6) {
    if (inventory_decode_1(dst,src)<0) return -1;
  }
  return 0;
}

/* Reset inventory.
 */
 
void inventory_reset() {
  memset(g.inventoryv,0,sizeof(g.inventoryv));
  memset(&g.equipped,0,sizeof(struct inventory));
  g.inventory_dirty=0;
  
  char serial[1024];
  int serialc=egg_store_get(serial,sizeof(serial),"inventory",9);
  if ((serialc>0)&&(serialc<=sizeof(serial))) {
    if (inventory_decode(serial,serialc)<0) {
      fprintf(stderr,"*** Failed to decode persisted inventory. Resetting to blank. ***\n");
      memset(g.inventoryv,0,sizeof(g.inventoryv));
      memset(&g.equipped,0,sizeof(struct inventory));
    } else {
      fprintf(stderr,"Decoded inventory from %d bytes.\n",serialc);//XXX
    }
  } else {
    fprintf(stderr,"No persisted inventory. Keeping default.\n");//XXX
  }
}

/* Save inventory if dirty.
 */
 
int inventory_save_if_dirty() {
  if (!g.inventory_dirty) return 0;
  g.inventory_dirty=0;
  if (INVENTORY_SIZE!=25) {
    fprintf(stderr,"Please update %s for INVENTORY_SIZE %d\n",__func__,INVENTORY_SIZE);
    return -1;
  }
  char tmp[156]; // 26*6
  char *dst=tmp;
  inventory_encode_1(dst,&g.equipped);
  dst+=6;
  const struct inventory *inventory=g.inventoryv;
  int i=25;
  for (;i-->0;dst+=6,inventory++) {
    inventory_encode_1(dst,inventory);
  }
  return egg_store_set("inventory",9,tmp,sizeof(tmp));
}

/* Tile for item.
 */
 
uint8_t tileid_for_item(int itemid,int quantity) {
  switch (itemid) {
    case NS_itemid_broom: return 0x30;
    case NS_itemid_divining_rod: return 0x31;
    case NS_itemid_hookshot: return 0x32;
    case NS_itemid_fishpole: return 0x33;
    case NS_itemid_match: return 0x34;
    case NS_itemid_bugspray: return 0x35;
    case NS_itemid_candy: return 0x36;
    case NS_itemid_wand: return 0x37;
    case NS_itemid_magnifier: return 0x38;
    case NS_itemid_stick: return 0x39;
  }
  return 0;
}

uint8_t hand_tileid_for_item(int itemid,int quantity) {
  switch (itemid) {
    case NS_itemid_broom: return 0x70;
    case NS_itemid_divining_rod: return 0x71;
    case NS_itemid_hookshot: return 0x72;
    case NS_itemid_fishpole: return 0x73;
    case NS_itemid_match: return quantity?0x74:0;
    case NS_itemid_bugspray: return quantity?0x75:0;
    case NS_itemid_candy: return quantity?0x76:0;
    case NS_itemid_wand: return 0x77;
    case NS_itemid_magnifier: return 0x78;
    case NS_itemid_stick: return 0x79;
  }
  return 0;
}

/* Default limit by itemid.
 * This is the authority for whether items use quantity or not.
 * But it's generally not enforced generically; spawn points need to be careful to correctly supply zero or nonzero quantity per itemid.
 */
 
static int initial_limit_for_itemid(int itemid) {
  switch (itemid) {
    case NS_itemid_match: return 99;
    case NS_itemid_bugspray: return 99;
    case NS_itemid_candy: return 99;
  }
  return 0;
}

/* Strings (in RID_strings_dialogue) by itemid.
 */
 
void strings_for_item(int *strix_name,int *strix_desc,int itemid) {
  switch (itemid) {
    case NS_itemid_broom:        *strix_name= 5; *strix_desc= 6; break;
    case NS_itemid_divining_rod: *strix_name= 7; *strix_desc= 8; break;
    case NS_itemid_hookshot:     *strix_name= 9; *strix_desc=10; break;
    case NS_itemid_fishpole:     *strix_name=11; *strix_desc=12; break;
    case NS_itemid_match:        *strix_name=13; *strix_desc=14; break;
    case NS_itemid_bugspray:     *strix_name=15; *strix_desc=16; break;
    case NS_itemid_candy:        *strix_name=17; *strix_desc=18; break;
    case NS_itemid_wand:         *strix_name=19; *strix_desc=20; break;
    case NS_itemid_magnifier:    *strix_name=21; *strix_desc=22; break;
    case NS_itemid_stick:        *strix_name=23; *strix_desc=24; break;
    default: *strix_name=25; *strix_desc=0; break;
  }
}

/* Test presence in inventory.
 */
 
struct inventory *inventory_search(int itemid) {
  if (itemid<1) return 0;
  if (itemid==g.equipped.itemid) return &g.equipped;
  struct inventory *inventory=g.inventoryv;
  int i=INVENTORY_SIZE;
  for (;i-->0;inventory++) {
    if (inventory->itemid==itemid) return inventory;
  }
  return 0;
}

/* Add to inventory, no side effects.
 */
 
static struct inventory *inventory_add(int itemid,int quantity) {
  struct inventory *inventory=0;
  if (!g.equipped.itemid) {
    inventory=&g.equipped;
  } else {
    struct inventory *q=g.inventoryv;
    int i=INVENTORY_SIZE;
    for (;i-->0;q++) {
      if (!q->itemid) {
        inventory=q;
        break;
      }
    }
  }
  if (!inventory) return 0;
  inventory->itemid=itemid;
  if (inventory->limit=initial_limit_for_itemid(itemid)) {
    if (!quantity) fprintf(stderr,"WARNING: %s itemid=%d, expected a quantity.\n",__func__,itemid);
    if (quantity>inventory->limit) quantity=inventory->limit;
    inventory->quantity=quantity;
  } else {
    // Don't warn about this, it's not important.
    //if (quantity) fprintf(stderr,"WARNING: %s itemid=%d, ignoring unexpected quantity %d.\n",__func__,itemid,quantity);
    inventory->quantity=0;
  }
  return inventory;
}

/* Get item.
 */
 
int inventory_acquire(int itemid,int quantity) {
  struct inventory *inventory=inventory_search(itemid);
  
  // TODO There will likely be items in the future that don't go into inventory. eg keys, things you just Have forever.
  // Those should be managed here.
  
  /* Acquiring for the first time?
   * This gets some extra fanfare.
   */
  if (!inventory) {
    if (!(inventory=inventory_add(itemid,quantity))) return 0;
    g.inventory_dirty=1;
    bm_sound(RID_sound_treasure,0.0);
    int strix_name=0,strix_desc=0;
    strings_for_item(&strix_name,&strix_desc,itemid);
    struct modal *modal=modal_new_dialogue(0,0);
    if (modal) {
      struct text_insertion insv[]={
        {.mode='r',.r={RID_strings_dialogue,strix_name}},
        {.mode='r',.r={RID_strings_dialogue,strix_desc}},
      };
      modal_dialogue_set_fmt(modal,RID_strings_dialogue,4,insv,2);
    }
    return 1;
    
  /* Already had it? If quantity is involved, do that.
   */
  } else {
    if (quantity>0) {
      int nq=inventory->quantity+quantity;
      if (nq>inventory->limit) nq=inventory->limit;
      if (nq!=inventory->quantity) {
        inventory->quantity=nq;
        g.inventory_dirty=1;
        bm_sound(RID_sound_collect,0.0);
        return 1;
      }
    }
  }
  return 0;
}

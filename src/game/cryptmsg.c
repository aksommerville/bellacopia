#include "bellacopia.h"
#include "game/battle/battle.h"

/* Crypto sidequest state.
 * This is all volatile. We rebuild it from a 16-bit seed on demand.
 * That seed is the only thing that persists (and the progress flags obviously).
 */
 
#define CRYPTMSGC 7
#define SPELL_LENGTH 6
 
static struct {
  int seed; // If zero, we are not initialized.
  uint8_t alphabet[26]; // Index is the plaintext a..z; value is 0..25=a..z.
  int boneitem; // NS_itemid_*
  int leafitem; // NS_itemid_*
  int staritem; // NS_itemid-*
  int starbattle; // NS_battle_*
  int msgtmpl[1+CRYPTMSGC]; // strix in strings:dialogue, indexed by (which). [0] unused.
  int sealcv[4]; // Count of feet per seal.
  int itemseal; // (1,2)=(bone,leaf), when an item has been played upon the seal.
  int itemsealc; // How many of them?
  int itemseal_framec; // (g.framec) at the last play.
  char spell[SPELL_LENGTH];
  int translate_time; // If nonzero, global frame count the last time the Spell of Translating was cast.
} cryptmsg={0};

/* Build the initial state if needed.
 */
 
static void cryptmsg_require() {
  if (cryptmsg.seed) return;
  
  // Get seed from the store, or generate if zero.
  if (!(cryptmsg.seed=store_get_fld16(NS_fld16_cryptmsg_seed))) {
    for (;;) {
      cryptmsg.seed=rand()&0xffff;
      if (cryptmsg.seed) break;
    }
    store_set_fld16(NS_fld16_cryptmsg_seed,cryptmsg.seed);
    //fprintf(stderr,"Initializing cryptmsg with seed %d.\n",cryptmsg.seed);
  } else {
    //fprintf(stderr,"Restoring cryptmsg with seed %d.\n",cryptmsg.seed);
  }
  
  // That seed is for a single-use PRNG.
  uint32_t prng=cryptmsg.seed|(cryptmsg.seed<<16);
  #define RAND(range) ({ \
    prng^=prng<<13; \
    prng^=prng>>17; \
    prng^=prng<<5; \
    ((prng&0x7fffffff)%(range)); \
  })
  
  // Generate the alphabet.
  //fprintf(stderr,"Alphabet:\n");
  uint8_t available[26];
  int i=0; for (;i<26;i++) available[i]=i;
  int abc=0;
  for (;abc<26;abc++) {
    int avp=RAND(26-abc);
    cryptmsg.alphabet[abc]=available[avp];
    memmove(available+avp,available+avp+1,(26-abc-1)-avp);
    //fprintf(stderr,"  %d\n",cryptmsg.alphabet[abc]);
  }
  
  // Choose the items that open the Bone Room and Leaf Room.
  uint8_t itemidv[]={
    NS_itemid_match,
    NS_itemid_candy,
    NS_itemid_bugspray,
    NS_itemid_vanishing,
    //NS_itemid_pepper, //TODO pepper isn't implemented yet, but should be a choice here
    // Can't use potion, because it's not possible to use it twice.
  };
  cryptmsg.boneitem=RAND(sizeof(itemidv));
  cryptmsg.leafitem=RAND(sizeof(itemidv)-1);
  if (cryptmsg.leafitem>=cryptmsg.boneitem) cryptmsg.leafitem++;
  cryptmsg.boneitem=itemidv[cryptmsg.boneitem];
  cryptmsg.leafitem=itemidv[cryptmsg.leafitem];
  //fprintf(stderr,"Chose items. bone=%d leaf=%d\n",cryptmsg.boneitem,cryptmsg.leafitem);
  
  // Choose battle and item that open the Star Room.
  uint8_t staritemidv[]={
    NS_itemid_stick,
    NS_itemid_broom,
    NS_itemid_divining,
    NS_itemid_wand,
    NS_itemid_fishpole,
    NS_itemid_bugspray,
    NS_itemid_potion,
    NS_itemid_hookshot,
    NS_itemid_candy,
    //NS_itemid_magnifier, //TODO not placed yet
    NS_itemid_vanishing,
    NS_itemid_compass,
    //NS_itemid_bell, //TODO This is a side quest reward. Not sure it's appropriate to require it here.
    //NS_itemid_telescope, //TODO not placed yet
    // Can't be shovel, since that's what we're guarding.
    // Can't be barrelhat, since that vanishes irreversibly after completing that side quest.
    // Don't include match or pepper, because that is what they would probably have equipped without intervention.
    // And of course, only inventoriables.
  };
  // All the goblin games are candidates, and they all have rsprite by the Star Door.
  // But I'm still on the fence whether to require a specific battle, or just "lose any battle".
  uint8_t starbattlev[]={
    NS_battle_throwing,
    NS_battle_stealing,
    //NS_battle_regex,
    //NS_battle_racketeering,
    //NS_battle_laziness,
    //NS_battle_hiring,
    //NS_battle_gobbling,
    NS_battle_erudition,
    //NS_battle_crying,
    //NS_battle_cobbling,
    NS_battle_apples,
  };
  cryptmsg.staritem=staritemidv[RAND(sizeof(staritemidv))];
  cryptmsg.starbattle=starbattlev[RAND(sizeof(starbattlev))];
  //fprintf(stderr,"Chose star item %d and battle %d.\n",cryptmsg.staritem,cryptmsg.starbattle);
  
  // msgtmpl [1] and [2] are strix 46 and 47. Random order.
  if (RAND(2)) {
    cryptmsg.msgtmpl[1]=46;
    cryptmsg.msgtmpl[2]=47;
  } else {
    cryptmsg.msgtmpl[1]=47;
    cryptmsg.msgtmpl[2]=46;
  }
  
  // msgtmpl [3] thru [7] are for around Skull Lake.
  // One of (48,49), one of (50,51) and two of (53,54,55,56).
  // Always include (52) because it's long, letter-rich, and has two apostrophes for two different contractions.
  uint8_t multistrix[]={53,54,55,56};
  int strixpre[5];
  strixpre[0]=RAND(2)?48:49;
  strixpre[1]=RAND(2)?50:51;
  strixpre[2]=RAND(4);
  strixpre[3]=RAND(3);
  if (strixpre[3]>=strixpre[2]) strixpre[3]++;
  strixpre[2]=multistrix[strixpre[2]];
  strixpre[3]=multistrix[strixpre[3]];
  strixpre[4]=52;
  // Now randomize the order:
  int msgtmplp=3,strixprelen=5;
  for (;msgtmplp<8;msgtmplp++) {
    int choice=RAND(strixprelen);
    cryptmsg.msgtmpl[msgtmplp]=strixpre[choice];
    strixprelen--;
    memmove(strixpre+choice,strixpre+choice+1,sizeof(int)*(strixprelen-choice));
  }
  const int *v=cryptmsg.msgtmpl;
  //fprintf(stderr,"Chose message templates: %d,%d,%d,%d,%d,%d,%d,%d\n",v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]);
  
  // Generate the Spell of Translating.
  // It only takes 12 bits: 2 per stroke, and 6 strokes.
  // But we digest, and force it to contain each direction at least once.
  // That "every direction" rule is important; it's how we ensure that this won't collide with static spells.
  int prespell=RAND(0x1000);
  int count_by_direction[4]={0};
  int shift=0;
  for (;shift<12;shift+=2) {
    int direction=(prespell>>shift)&3;
    count_by_direction[direction]++;
  }
  for (i=0;i<4;i++) {
    if (count_by_direction[i]) continue;
    int j=0;
    for (;j<4;j++) {
      if (count_by_direction[j]<2) continue;
      count_by_direction[j]--;
      count_by_direction[i]++;
      for (shift=0;shift<12;shift+=2) {
        if (((prespell>>shift)&3)==j) {
          prespell=((prespell&~(3<<shift))|(i<<shift));
          break;
        }
      }
    }
  }
  for (i=0,shift=0;i<SPELL_LENGTH;i++,shift+=2) {
    cryptmsg.spell[i]="LRUD"[(prespell>>shift)&3];
  }
  
  #undef RAND
}

/* Apply encryption in place.
 */
 
void cryptmsg_encrypt_in_place(char *v,int c) {
  cryptmsg_require();
  for (;c-->0;v++) {
         if ((*v>=0x41)&&(*v<=0x5a)) *v=0xc1+cryptmsg.alphabet[(*v)-0x41];
    else if ((*v>=0x61)&&(*v<=0x7a)) *v=0xc1+cryptmsg.alphabet[(*v)-0x61];
    // else keep it
  }
}
 
static int cryptmsg_encrypt(char *v,int c) {
  if (store_get_fld(NS_fld_no_encryption)) return c;
  int tclock=g.framec-cryptmsg.translate_time;
  if (cryptmsg.translate_time) {
    cryptmsg.translate_time=0;
    if (tclock<30*60) return c;
  }
  cryptmsg_encrypt_in_place(v,c);
  return c;
}

/* Insert battle name in a message.
 */
 
static int dress_battle(char *dst,int dsta,const char *name,int namec,const struct battle_type *type) {
  int dstc=0;
  if (!namec) return 0;
  if (!type->no_article) {
    //TODO I'm just doing this in English for now. Other languages will need specific support.
    switch (name[0]) {
      case 'a': case 'e': case 'i': case 'o': case 'u':
      case 'A': case 'E': case 'I': case 'O': case 'U': {
          if (dstc<=dsta-3) memcpy(dst+dstc,"an ",3);
          dstc+=3;
        } break;
      default: {
          if (dstc<=dsta-2) memcpy(dst+dstc,"a ",2);
          dstc+=2;
        }
    }
  }
  if (dstc<=dsta-namec) memcpy(dst+dstc,name,namec);
  dstc+=namec;
  if (!type->no_contest) {
    if (dstc<=dsta-8) memcpy(dst+dstc," Contest",8);
    dstc+=8;
  }
  return dstc;
}
 
static int cryptmsg_fmt_battle(char *dst,int dsta,const char *src,int srcc,int battle) {
  char name[256];
  int namec=battle_type_describe_long(name,sizeof(name),battle_type_by_id(battle));
  if ((namec<0)||(namec>sizeof(name))) namec=0;
  struct text_insertion ins={.mode='s',.s={.v=name,.c=namec}};
  return text_format(dst,dsta,src,srcc,&ins,1);
}

/* Insert item name in a message.
 */
 
static int cryptmsg_fmt_item(char *dst,int dsta,const char *src,int srcc,int itemid) {
  const struct item_detail *detail=item_detail_for_itemid(itemid);
  if (detail) {
    struct text_insertion ins={.mode='r',.r={.rid=RID_strings_item,.strix=detail->strix_name}};
    return text_format(dst,dsta,src,srcc,&ins,1);
  }
  if (srcc>dsta) srcc=dsta;
  memcpy(dst,src,srcc);
  return srcc;
}

/* Get an enumerated message in Old Goblish.
 * Public entry point.
 */
 
int cryptmsg_get(char *dst,int dsta,int which) {
  if ((which<1)||(which>CRYPTMSGC)) return 0;
  if (!dst||(dsta<1)) return 0;
  cryptmsg_require();
  int strix=cryptmsg.msgtmpl[which];
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_dialogue,strix);
  if (srcc>dsta) return 0;
  int dstc=srcc;
  switch (strix) {
    // Every message that has an insertion has just one.
    case 46: dstc=cryptmsg_fmt_battle(dst,dsta,src,srcc,cryptmsg.starbattle); break;
    case 47: dstc=cryptmsg_fmt_item(dst,dsta,src,srcc,cryptmsg.staritem); break;
    case 48: case 49: dstc=cryptmsg_fmt_item(dst,dsta,src,srcc,cryptmsg.boneitem); break;
    case 50: case 51: dstc=cryptmsg_fmt_item(dst,dsta,src,srcc,cryptmsg.leafitem); break;
    default: dstc=srcc; memcpy(dst,src,srcc); break;
  }
  return cryptmsg_encrypt(dst,dstc);
}

/* Notify of entering or leaving a seal.
 */
 
void cryptmsg_press_seal(int id) {
  if ((id>=1)&&(id<=3)) {
    cryptmsg.sealcv[id]++;
  }
}

void cryptmsg_release_seal(int id) {
  if ((id>=1)&&(id<=3)) {
    if (cryptmsg.sealcv[id]<1) {
    } else {
      cryptmsg.sealcv[id]--;
    }
  }
}

/* An item has been used.
 * Check the seal sequences.
 */
 
void cryptmsg_notify_item(int itemid) {
  // Don't require just yet. We get called every time any item is used.
  
  int nseal=0; // Nonzero if this is a valid play for the given seal 1,2,3.
  if (cryptmsg.sealcv[1]&&!cryptmsg.sealcv[2]&&!cryptmsg.sealcv[3]) {
    cryptmsg_require();
    if (itemid==cryptmsg.boneitem) nseal=1;
  } else if (!cryptmsg.sealcv[1]&&cryptmsg.sealcv[2]&&!cryptmsg.sealcv[3]) {
    cryptmsg_require();
    if (itemid==cryptmsg.leafitem) nseal=2;
  } else if (!cryptmsg.sealcv[1]&&!cryptmsg.sealcv[2]&&cryptmsg.sealcv[3]) {
    // Star seal doesn't matter; you only have to equip the item here, not use it.
  }
  
  if (!nseal) {
    cryptmsg.itemseal=0;
    cryptmsg.itemsealc=0;
    cryptmsg.itemseal_framec=0;
  } else {
    if (cryptmsg.itemsealc&&(cryptmsg.itemseal!=nseal)) {
      cryptmsg.itemseal=0;
      cryptmsg.itemsealc=0;
      cryptmsg.itemseal_framec=0;
    }
    const int EXPIRY=60*10;
    if (cryptmsg.itemsealc&&(cryptmsg.itemseal==nseal)&&(g.framec<=cryptmsg.itemseal_framec+EXPIRY)) {
      cryptmsg.itemsealc++;
    } else {
      cryptmsg.itemseal=nseal;
      cryptmsg.itemsealc=1;
    }
    cryptmsg.itemseal_framec=g.framec;
  }
  
  if (cryptmsg.itemsealc>=2) {
    switch (cryptmsg.itemseal) {
      case 1: {
          if (store_set_fld(NS_fld_bonedoor,1)) {
            bm_sound(RID_sound_secret);
            g.camera.mapsdirty=1;
          }
        } break;
      case 2: {
          if (store_set_fld(NS_fld_leafdoor,1)) {
            bm_sound(RID_sound_secret);
            g.camera.mapsdirty=1;
          }
        } break;
    }
  }
}

/* Check the star door, after losing a battle.
 */

int cryptmsg_check_star_door(int battle,int itemid) {
  if (!cryptmsg.seed&&!store_get_fld(NS_fld_kidnapped)) return 0; // Don't initialize cryptmsg if you haven't been to the caves yet. (optimization only)
  if (store_get_fld(NS_fld_stardoor)) return 0;
  cryptmsg_require();
  if (battle!=cryptmsg.starbattle) return 0;
  if (itemid!=cryptmsg.staritem) return 0;
  bm_sound(RID_sound_secret);
  store_set_fld(NS_fld_stardoor,1);
  g.camera.mapsdirty=1;
  return 1;
}

/* Get the Spell of Translating.
 */
 
int cryptmsg_get_spell(char *dst,int dsta,int require) {
  if (require) cryptmsg_require();
  if (dsta>=SPELL_LENGTH) memcpy(dst,cryptmsg.spell,SPELL_LENGTH);
  return SPELL_LENGTH;
}

void cryptmsg_translate_next() {
  cryptmsg.translate_time=g.framec;
}

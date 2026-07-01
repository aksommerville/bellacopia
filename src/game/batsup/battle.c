#include "game/bellacopia.h"
#include "egg/egg_language_codes.h"

/* Type by id.
 */

#define _(tag) extern const struct battle_type battle_type_##tag;
FOR_EACH_battle
#undef _

const struct battle_type *battle_type_by_id(int battle) {
  switch (battle) {
    #define _(tag) case NS_battle_##tag: return &battle_type_##tag;
    FOR_EACH_battle
    #undef _
  }
  return 0;
}

/* Delete instance.
 */

void battle_del(struct battle *battle) {
  if (!battle) return;
  if (battle->type->del) battle->type->del(battle);
  free(battle);
}

/* New.
 */

struct battle *battle_new(
  const struct battle_type *type,
  const struct battle_args *args
) {
  if (!type||!args) return 0;
  
  // Validate the control scheme requested in (args).
  if ((args->lctl==1)&&(args->rctl==0)) {
    // Player one vs CPU. All battles must support.
  } else if ((args->lctl==1)&&(args->rctl==2)) {
    if (!type->support_pvp) {
      fprintf(stderr,"Battle type '%s' does not support player-vs-player.\n",type->name);
      return 0;
    }
  } else if ((args->lctl==0)&&(args->rctl==0)) {
    if (!type->support_cvc) {
      fprintf(stderr,"Battle type '%s' does not support CPU-vs-CPU.\n",type->name);
      return 0;
    }
  } else {
    // No particular reason to reject other things, but they don't make sense. eg Player One controlling both, or CPU on the left...
    fprintf(stderr,"Battle type '%s' rejecting unexpected control scheme %d,%d.\n",type->name,args->lctl,args->rctl);
    return 0;
  }
  
  // OK, instantiate.
  struct battle *battle=calloc(1,type->objlen);
  if (!battle) return 0;
  battle->type=type;
  battle->args=*args;
  battle->outcome=-2;
  
  // Prepare the color table automagically.
  if (!battle->args.mapid&&g.camera.map) battle->args.mapid=g.camera.map->rid;
  battle_get_ctab_by_id(battle->ctab,BATTLE_COLOR_COUNT,battle->args.mapid);
  
  if (type->init&&(type->init(battle)<0)) {
    battle_del(battle);
    return 0;
  }
  return battle;
}

/* Language-specific long names.
 * In general: "a NameOfGame Contest".
 * (name) is never empty.
 */
 
static int battle_type_describe_en(char *dst,int dsta,const char *name,int namec,const struct battle_type *type) {
  int dstc=0;
  if (!type->no_article) {
    if (dstc<dsta) dst[dstc]='a'; dstc++;
    switch (name[0]) {
      case 'a': case 'e': case 'i': case 'o': case 'u':
      case 'A': case 'E': case 'I': case 'O': case 'U': {
          if (dstc<dsta) dst[dstc]='n'; dstc++;
        } break;
    }
    if (dstc<dsta) dst[dstc]=' '; dstc++;
  }
  if (dstc<=dsta-namec) memcpy(dst+dstc,name,namec);
  dstc+=namec;
  if (!type->no_contest) {
    if (dstc<=dsta-8) memcpy(dst+dstc," Contest",8);
    dstc+=8;
  }
  return dstc;
}

static int battle_type_describe_fr(char *dst,int dsta,const char *name,int namec,const struct battle_type *type) {
  int dstc=0;
  if (type->no_contest) {
    if (!type->no_article) {
      switch (name[0]) {
        case 'a': case 'e': case 'i': case 'o': case 'u':
        case 'A': case 'E': case 'I': case 'O': case 'U': {
            if (dstc<=dsta-2) memcpy(dst+dstc,"l'",2);
            dstc+=2;
          } break;
        default: {
            if (dstc<=dsta-3) memcpy(dst+dstc,"la ",3);
            dstc+=3;
          }
      }
    }
  } else {
    if (dstc<=dsta-8) memcpy(dst+dstc,"un jeu d",8);
    dstc+=8;
    switch (name[0]) {
      case 'a': case 'e': case 'i': case 'o': case 'u':
      case 'A': case 'E': case 'I': case 'O': case 'U': {
          if (dstc<dsta) dst[dstc]='\''; dstc++;
        } break;
      default: {
          if (dstc<=dsta-2) memcpy(dst+dstc,"e ",2);
          dstc+=2;
        }
    }
  }
  if (dstc<=dsta-namec) memcpy(dst+dstc,name,namec);
  dstc+=namec;
  return dstc;
}

/* Human-friendly text for a battle's name.
 */

int battle_type_describe_short(char *dst,int dsta,const struct battle_type *type) {
  if (!type) return 0;
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_battle,type->strix_name);
  if (srcc<1) return 0;
  if (srcc>dsta) return srcc;
  memcpy(dst,src,srcc);
  return srcc;
}

int battle_type_describe_long(char *dst,int dsta,const struct battle_type *type) {
  if (!type) return 0;
  const char *name=0;
  int namec=text_get_string(&name,RID_strings_battle,type->strix_name);
  if (namec<1) return 0;
  int lang=egg_prefs_get(EGG_PREF_LANG);
  int dstc=0;
  switch (egg_prefs_get(EGG_PREF_LANG)) {
    case EGG_LANG_en: dstc=battle_type_describe_en(dst,dsta,name,namec,type); break;
    case EGG_LANG_fr: dstc=battle_type_describe_fr(dst,dsta,name,namec,type); break;
    //TODO All other supported languages.
  }
  if (dstc>0) return dstc;
  if (namec<=dsta) memcpy(dst,name,namec);
  return namec;
}

/* Digest difficulty and bias.
 */

void battle_normalize_bias(double *lskill,double *rskill,const struct battle *battle) {
  if (battle->args.bias!=0x80) *rskill=(double)battle->args.bias/255.0;
  else *rskill=(double)battle->args.difficulty/256.0;
  *lskill=1.0-(*rskill);
}

double battle_scalar_difficulty(const struct battle *battle) {
  if (battle->args.difficulty!=0x80) return (double)battle->args.difficulty/255.0;
  return (double)battle->args.bias/255.0;
}

/* Color tables.
 * Logically these belong nearer the tilesheet resource, but that would be inconvenient.
 */
 
void battle_get_ctab_by_id(uint32_t *dst,int dsta,int imageid) {
  #define _(tag,rgba) if (dsta>BATTLE_COLOR_##tag) dst[BATTLE_COLOR_##tag]=rgba;
  switch (imageid) {
    // Most "_int" are unlikely to come up in real battles, but should mimic their Ext.
    // Meadow and similar are the default. Call them out explicitly here so we don't log a reminder.
    case RID_image_meadow:
    case RID_image_botire:
    case RID_image_botire_int:
    case RID_image_cheapside:
    case RID_image_cheapside_int:
    case RID_image_battlefield:
    case RID_image_battlefield_int:
    case RID_image_fractia:
    case RID_image_fractia_int:
    case RID_image_mountains:
    case 0: case -1: // These are ok as known unknowns.
      break;
    // Then on with the specialty ctabs...
    case RID_image_caves: {
        _(SKY,   0x785830ff)
        _(GROUND,0x3c2011ff)
      } return;
    case RID_image_tundra:
    case RID_image_tundra_int:
    case RID_image_icepalace: {
        _(SKY,   0x96ceedff)
        _(GROUND,0xffffffff)
      } return;
    case RID_image_desert:
    case RID_image_castle_ext:
    case RID_image_castle_int: {
        _(SKY,   0xa1d5dcff)
        _(GROUND,0xb6963dff)
      } return;
    case RID_image_temple:
    case RID_image_temple_int: {
        _(SKY,   0x5bc3fbff)
        _(GROUND,0x6c5d47ff)
      } return;
    case RID_image_labyrinth: {
        _(SKY,   0x574949ff)
        _(GROUND,0x776a5cff)
      } return;
    default: {
        fprintf(stderr,"Unknown image %d for battle ctab. Using default.\n",imageid);
      } break;
  }
  
  // Default.
  _(SKY,   0x5e9fc7ff)
  _(GROUND,0x126e29ff)
  
  #undef _
}

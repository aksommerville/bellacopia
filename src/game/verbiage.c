#include "game.h"
#include "egg/egg_language_codes.h"

/* Get a strings resource entry and force lowercase.
 * Always returns in 0..dsta.
 */
 
static int verbiage_lowercase_res(char *dst,int dsta,int rid,int strix) {
  const char *src=0;
  int srcc=text_get_string(&src,rid,strix);
  if (srcc<=0) return 0;
  if (srcc>dsta) srcc=dsta;
  int i=srcc;
  while (i-->0) {
    if ((src[i]>='A')&&(src[i]<='Z')) dst[i]=src[i]+0x20;
    else dst[i]=src[i];
  }
  return srcc;
}

/* "a" or "an".
 */
 
static const char *english_article(const char *noun,int nounc,int upper) {
  if (nounc<1) return "";
  switch (noun[0]) {
    case 'a': case 'A':
    case 'e': case 'E':
    case 'i': case 'I':
    case 'o': case 'O':
    case 'u': case 'U':
      return upper?"An":"an";
  }
  return upper?"A":"a";
}

/* verbiage_begin_battle, language-specific variations.
 */
 
static int verbiage_begin_battle_en(char *dst,int dsta,const struct battle_type *type) {
  char name[64];
  int namec=verbiage_lowercase_res(name,sizeof(name),RID_strings_battle,type->battle_name_strix);
  if (type->flags&BATTLE_FLAG_AWKWARD_NAME) {
    struct text_insertion insv[]={
      {.mode='r',.r={RID_strings_battle,type->foe_name_strix}},
      {.mode='s',.s={name,namec}},
    };
    return text_format_res(dst,dsta,RID_strings_battle,2,insv,2);
  } else {
    struct text_insertion insv[]={
      {.mode='r',.r={RID_strings_battle,type->foe_name_strix}},
      {.mode='s',.s={english_article(name,namec,0),-1}},
      {.mode='s',.s={name,namec}},
    };
    return text_format_res(dst,dsta,RID_strings_battle,1,insv,3);
  }
}
 
static int verbiage_begin_battle_fr(char *dst,int dsta,const struct battle_type *type) {
  char name[64];
  int namec=verbiage_lowercase_res(name,sizeof(name),RID_strings_battle,type->battle_name_strix);
  struct text_insertion insv[]={
    {.mode='r',.r={RID_strings_battle,type->foe_name_strix}},
    {.mode='s',.s={name,namec}},
  };
  if (type->flags&BATTLE_FLAG_AWKWARD_NAME) {
    return text_format_res(dst,dsta,RID_strings_battle,2,insv,2);
  } else {
    return text_format_res(dst,dsta,RID_strings_battle,1,insv,2);
  }
}
 
static int verbiage_begin_battle_generic(char *dst,int dsta,const struct battle_type *type) {
  struct text_insertion insv[]={
    {.mode='r',.r={RID_strings_battle,type->foe_name_strix}},
    {.mode='r',.r={RID_strings_battle,type->battle_name_strix}},
  };
  if (type->flags&BATTLE_FLAG_AWKWARD_NAME) {
    return text_format_res(dst,dsta,RID_strings_battle,2,insv,2);
  } else {
    return text_format_res(dst,dsta,RID_strings_battle,1,insv,2);
  }
}

/* "FOE challenges you to a MINIGAME contest!" or similar.
 */
 
int verbiage_begin_battle(char *dst,int dsta,const struct battle_type *type) {
  if (!type) return 0;
  int lang=egg_prefs_get(EGG_PREF_LANG);
  switch (lang) {
    case EGG_LANG_en: return verbiage_begin_battle_en(dst,dsta,type);
    case EGG_LANG_fr: return verbiage_begin_battle_fr(dst,dsta,type);
    default: return verbiage_begin_battle_generic(dst,dsta,type);
  }
  return 0;
}

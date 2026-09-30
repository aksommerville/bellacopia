#include <util/stdlib/egg-stdlib.h>
#include "text.h"

/* Integer as words, support.
 */
 
#define APPEND(str) { const char *_str=(str); while (*_str) { if (dstc<dsta) dst[dstc]=*_str; dstc++; _str++; } }
#define SPACE { if (dstc) APPEND(" ") }

static const char *decade_names[10]={
  "zeroty","tenty",
  "twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety",
};
static const char *digit_names[20]={ // Including teens.
  "zero","one","two","three","four","five","six","seven","eight","nine",
  "ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen","seventeen","eighteen","nineteen",
};

static int int_as_words_ltk(char *dst,int dsta,int v/* 0..999 */) {
  int dstc=0;
  if (v>=100) {
    int sub=v/100;
    v%=100;
    APPEND(digit_names[sub])
    SPACE
    APPEND("hundred")
  }
  
  // In English, 20..99 follow an exploitable pattern. 0..19 are singleton words.
  if (v>=20) {
    int sub=v/10;
    v%=10;
    SPACE
    APPEND(decade_names[sub])
    if (v) {
      APPEND("-")
      APPEND(digit_names[v]);
    }
  } else {
    SPACE
    APPEND(digit_names[v]);
  }
  return dstc;
}

/* Integer as plain words.
 */
 
int int_as_words(char *dst,int dsta,int v) {
  /* TODO Needs an alternate implementation for every language.
   * Hard-coding English at least for now. Not sure we're going to translate at all.
   * (for that matter, hard-coding *American* English. Brits define "billion" differently).
   */
  int dstc=0;
  
  if (v==INT_MIN) v++; // We need negatives to be positable.
  if (v<0) {
    v=-v;
    APPEND("negative")
  }
  
  // 31-bit integers go just a bit into the billions.
  if (v>=1000000000) {
    int sub=v/1000000000;
    v%=1000000000;
    SPACE
    dstc+=int_as_words_ltk(dst+dstc,dsta-dstc,sub);
    SPACE
    APPEND("billion")
  }
  if (v>=1000000) {
    int sub=v/1000000;
    v%=1000000;
    SPACE
    dstc+=int_as_words_ltk(dst+dstc,dsta-dstc,sub);
    SPACE
    APPEND("million")
  }
  if (v>=1000) {
    int sub=v/1000;
    v%=1000;
    SPACE
    dstc+=int_as_words_ltk(dst+dstc,dsta-dstc,sub);
    SPACE
    APPEND("thousand")
  }
  SPACE
  dstc+=int_as_words_ltk(dst+dstc,dsta-dstc,v);
  
  if (dstc<dsta) dst[dstc]=0;
  return dstc;
}

#undef APPEND
#undef SPACE

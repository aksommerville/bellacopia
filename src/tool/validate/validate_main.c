#include "validate_internal.h"

struct g g={0};

int main(int argc,char **argv) {
  if ((argc>=1)&&argv&&argv[0]&&argv[0][0]) g.exename=argv[0];
  else g.exename="validate";
  int passc=0,failc=0;
  #define TRY(fnname) { \
    int err=fnname(); \
    if (err<0) { \
      fprintf(stderr,"FAIL: %s\n",#fnname); \
      failc++; \
    } else { \
      passc++; \
    } \
  }
  
  TRY(validate_letfloor_text)
  
  #undef TRY
  fprintf(stderr,"%s: %d fail, %d pass\n",g.exename,failc,passc);
  return failc?1:0;
}

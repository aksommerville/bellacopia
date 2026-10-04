#include "game/bellacopia.h"
#include "qr.h"
#include "qrcodegen.h"

int qr_generate(int *w,int *h,const char *src,int srcc) {
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  
  int len=qrcodegen_BUFFER_LEN_FOR_VERSION(40);
  if (len<1) return -1;
  if (srcc>len) return -1;
  uint8_t *data=calloc(1,len);
  if (!data) return -1;
  uint8_t *qrcode=calloc(1,len);
  if (!qrcode) { free(data); return -1; }
  memcpy(data,src,srcc);
  
  if (!qrcodegen_encodeBinary(data,srcc,qrcode,qrcodegen_Ecc_LOW,1,40,qrcodegen_Mask_AUTO,1)) {
    free(data);
    free(qrcode);
    return -1;
  }
  free(data);
  
  int size=qrcodegen_getSize(qrcode);
  if ((size<1)||(size>4096)) {
    free(qrcode);
    return -1;
  }
  const int margin=8;
  int dstw=size+margin*2;
  int dsth=size+margin*2;
  int dststride=dstw<<2;
  int dstsize=dststride*dsth;
  uint32_t *dst=malloc(dstsize);
  if (!dst) {
    free(qrcode);
    return -1;
  }
  
  const uint32_t black=0xff000000,white=0xffffffff; // Need to do something if we ever build big-endian.
  uint32_t *p=dst;
  int y=0; for (;y<dsth;y++) {
    int x=0; for (;x<dstw;x++,p++) {
      if ((x<margin)||(x>=dstw-margin)||(y<margin)||(y>=dsth-margin)) {
        *p=white;
      } else if (qrcodegen_getModule(qrcode,x-margin,y-margin)) {
        *p=black;
      } else {
        *p=white;
      }
    }
  }
  free(qrcode);
  
  int texid=egg_texture_new();
  if (texid<0) {
    free(dst);
    return -1;
  }
  if (egg_texture_load_raw(texid,dstw,dsth,dststride,dst,dstsize)<0) {
    free(dst);
    egg_texture_del(texid);
    return -1;
  }
  free(dst);
  
  if (w) *w=dstw;
  if (h) *h=dsth;
  return texid;
}

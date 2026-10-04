/* qr.h
 * Thin wrapper around qrcodegen, for bellacopia.
 */
 
#ifndef QR_H
#define QR_H

/* Returns a new Egg texture and its dimensions.
 */
int qr_generate(int *w,int *h,const char *src,int srcc);

#endif

/* sprite_ticker.c
 * LED text display, text scrolls to the left.
 */
 
#include "game/bellacopia.h"

#define LED_COLC 30
#define LED_ROWC 5

#define SCROLL_INTERVAL 0.100

struct sprite_ticker {
  struct sprite hdr;
  char *text;
  int textc;
  int textw; // Full message width in led cells.
  int textp; // 0..textw
  double textclock;
  struct led {
    uint8_t target; // Alpha, set during update. 0 or 255, tho technically intermediates are allowed.
    uint8_t actual; // Alpha, syncs toward (target) during render.
  } ledv[LED_COLC*LED_ROWC];
};

#define SPRITE ((struct sprite_ticker*)sprite)

static void _ticker_del(struct sprite *sprite) {
  if (SPRITE->text) free(SPRITE->text);
}

static int _ticker_init(struct sprite *sprite) {
  int fld=(sprite->arg[0]<<8)|sprite->arg[1];
  switch (fld) {
    case NS_fld_zoo1_0:
      {
        char tmp[1024];
        int tmpc=zoo_get_ticker_text(tmp,sizeof(tmp),fld);
        if ((tmpc<0)||(tmpc>sizeof(tmp))) tmpc=0;
        sprite_ticker_set_text(sprite,tmp,tmpc);
      } break;
    default: {
        sprite_ticker_set_text(sprite,"sprite_ticker misconfigured",-1);
      }
  }
  return 0;
}

/* Get one bit from a glyph.
 * (x) in 0..2, (y) in 0..4.
 * Returns alpha. 0x00 or 0xff.
 */

// Packed y1 big-endian 3x315.
static const uint8_t alphabits[]={
0x49,0x05,0x68,0x02,0xfb,0xef,0x9c,0xf8,0x54,0x2b,0x5d,0xd2,0x00,0x14,0x91,0x89,
0x29,0x55,0x00,0x2e,0x80,0x03,0x20,0x38,0x00,0x00,0x81,0x50,0x2b,0x6a,0xc9,0x2f,
0x8a,0x9f,0x14,0x75,0xbc,0x9f,0x31,0xca,0x6a,0xb9,0x24,0xaa,0xaa,0x75,0x92,0x10,
0x40,0x21,0x91,0x51,0x11,0xc7,0x11,0x15,0x31,0x41,0x2b,0xe3,0x57,0xdb,0xae,0xb9,
0xc9,0x1e,0xb6,0xef,0x34,0xfe,0x69,0x14,0xb5,0xdb,0xed,0xe9,0x2e,0x49,0xaa,0xdd,
0x6c,0x92,0x7f,0xfd,0xa7,0xff,0x15,0xb5,0x6b,0xa4,0x56,0xf7,0xae,0xb5,0xc4,0x77,
0x49,0x2b,0x6d,0xf6,0xda,0xad,0xff,0xda,0xad,0xb5,0x25,0xca,0x9d,0xa4,0x9e,0x49,
0x61,0x11,0x0a,0x80,0x00,0x03,0x80,
};
 
static uint8_t ticker_glyph_bit(char ch,int x,int y) {
  if ((x<0)||(x>=3)||(y<0)||(y>=5)) return 0;
  if ((ch<=0x20)||(ch>=0x7f)) return 0; // G0 only; and space is blank.
  if (ch>=0x60) ch-=0x20; // Just one case of the alphabet and the high punctuation gets mangled: `{|}~ become @[\]^
  int glyphp=ch-0x21;
  int srcpbit=(5*glyphp+y)*3+x;
  int srcp=srcpbit>>3;
  if ((srcp<0)||(srcp>=sizeof(alphabits))) return 0; // Just to be safe.
  uint8_t srcmask=0x80>>(srcpbit&7);
  return (alphabits[srcp]&srcmask)?0xff:0x00;
}

/* Redraw the whole led grid.
 */
 
static void ticker_update_leds(struct sprite *sprite) {
  struct led *led=SPRITE->ledv;
  int ledy=0,texty=0;
  for (;ledy<LED_ROWC;ledy++,texty++) {
    int ledx=0;
    int textx=SPRITE->textp;
    for (;ledx<LED_COLC;ledx++,textx++,led++) {
      int textp=textx>>2;
      if ((textp<0)||(textp>=SPRITE->textc)) {
        led->target=0;
      } else {
        int subx=textx&3;
        led->target=ticker_glyph_bit(SPRITE->text[textp],subx,texty);
      }
    }
  }
}

/* Update.
 */

static void _ticker_update(struct sprite *sprite,double elapsed) {
  int dirty=0;
  SPRITE->textclock-=elapsed;
  while (SPRITE->textclock<0.0) {
    SPRITE->textclock+=SCROLL_INTERVAL;
    SPRITE->textp++;
    if (SPRITE->textp>=SPRITE->textw) SPRITE->textp=-LED_COLC;
    dirty=1;
  }
  if (dirty) ticker_update_leds(sprite);
}

/* Render.
 */

static void _ticker_render(struct sprite *sprite,int dstx,int dsty) {

  /* First advance LED actuals toward their targets.
   * Close half the distance each frame, minimum one.
   * So they should sync over 8 frames.
   */
  struct led *led=SPRITE->ledv;
  int i=LED_COLC*LED_ROWC;
  for (;i-->0;led++) {
    int d=led->target-led->actual;
    int n=led->actual;
    if (d>0) {
      if ((n+=60)>led->target) n=led->target;
    } else if (d<0) {
      if ((n-=60)<led->target) n=led->target;
    }
    led->actual=n;
  }

  // Background is 3 tiles lined up in 4 slots horizontally.
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,dstx+NS_sys_tilesize*0,dsty,sprite->tileid+0,0);
  graf_tile(&g.graf,dstx+NS_sys_tilesize*1,dsty,sprite->tileid+1,0);
  graf_tile(&g.graf,dstx+NS_sys_tilesize*2,dsty,sprite->tileid+1,0);
  graf_tile(&g.graf,dstx+NS_sys_tilesize*3,dsty,sprite->tileid+2,0);

  /* Foreground are fancies, one per lit led.
   */
  int ledy=dsty-8+5;
  int ledx0=dstx-8+3;
  int yi=LED_ROWC;
  for (led=SPRITE->ledv;yi-->0;ledy+=2) {
    int ledx=ledx0;
    int xi=LED_COLC;
    for (;xi-->0;ledx+=2,led++) {
      if (!led->actual) continue;
      graf_fancy(&g.graf,ledx,ledy,sprite->tileid+3,0,0,NS_sys_tilesize,0,0x80808000|led->actual);
    }
  }
}

/* Type definition and public API.
 */

const struct sprite_type sprite_type_ticker={
  .name="ticker",
  .objlen=sizeof(struct sprite_ticker),
  .del=_ticker_del,
  .init=_ticker_init,
  .update=_ticker_update,
  .render=_ticker_render,
};

int sprite_ticker_set_text(struct sprite *sprite,const char *src,int srcc) {
  if (!sprite||(sprite->type!=&sprite_type_ticker)) return -1;
  if (!src) srcc=0; else if (srcc<0) { srcc=0; while (src[srcc]) srcc++; }
  char *nv=0;
  if (srcc) {
    if (!(nv=malloc(srcc))) return -1;
    memcpy(nv,src,srcc);
  }
  if (SPRITE->text) free(SPRITE->text);
  SPRITE->text=nv;
  SPRITE->textc=srcc;
  SPRITE->textw=SPRITE->textc*4;
  SPRITE->textp=-LED_COLC;
  memset(SPRITE->ledv,0,sizeof(SPRITE->ledv));
  return 0;
}

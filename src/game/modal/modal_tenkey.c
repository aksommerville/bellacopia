/* modal_tenkey.c
 * Generic ten-key numeric input.
 * Writing this for the surveyor contest, but keeping generic.
 */
 
#include "game/bellacopia.h"
#include "modal.h"

struct modal_tenkey {
  struct modal hdr;
  int value;
  void (*cb)(int value,void *userdata);
  void *userdata;
  int selx,sely; // (selx) in 0..2, (sely) in 0..3
  int finger; // Nonzero if button down. Decorative; the button only takes effect once per stroke.
  int virgin; // Next input clears existing value first.
  int blackout;
};

#define MODAL ((struct modal_tenkey*)modal)

/* Cleanup.
 */
 
static void _tenkey_del(struct modal *modal) {
}

/* Init.
 */
 
static int _tenkey_init(struct modal *modal,const void *args,int argslen) {
  modal->interactive=1;
  modal->opaque=0;
  modal->blotter=1;
  MODAL->selx=1;
  MODAL->sely=1;
  MODAL->virgin=1;
  MODAL->blackout=1;
  
  if (args&&(argslen==sizeof(struct modal_args_tenkey))) {
    const struct modal_args_tenkey *ARGS=args;
    MODAL->value=ARGS->value;
    MODAL->cb=ARGS->cb;
    MODAL->userdata=ARGS->userdata;
    if (MODAL->value<0) MODAL->value=0;
    else if (MODAL->value>999) MODAL->value=999;
  }
  
  return 0;
}

/* Submit.
 */
 
static void tenkey_submit(struct modal *modal) {
  bm_sound(RID_sound_collect);
  if (MODAL->cb) {
    MODAL->cb(MODAL->value,MODAL->userdata);
    MODAL->cb=0;
  }
  modal->defunct=1;//TODO animated dismissal
}

/* Backspace.
 */
 
static void tenkey_backspace(struct modal *modal) {
  bm_sound(RID_sound_reject);
  if (MODAL->virgin) {
    MODAL->value=0;
    MODAL->virgin=0;
  } else {
    MODAL->value/=10;
  }
}

/* Digit.
 */
 
static void tenkey_digit(struct modal *modal,int digit) {
  if (MODAL->virgin) {
    MODAL->value=0;
    MODAL->virgin=0;
  }
  MODAL->value*=10;
  MODAL->value+=digit;
  if (MODAL->value>999) MODAL->value=999;
  bm_sound(RID_sound_uiactivate);
}

/* Activate button.
 */
 
static void tenkey_activate(struct modal *modal) {
  if ((MODAL->selx<0)||(MODAL->sely<0)||(MODAL->selx>=3)||(MODAL->sely>=4)) return;
  int btnid=MODAL->sely*3+MODAL->selx; // 1,2,3,4,5,6,7,8,9,DEL,0,ENTER
  switch (btnid) {
    case 0: tenkey_digit(modal,1); break;
    case 1: tenkey_digit(modal,2); break;
    case 2: tenkey_digit(modal,3); break;
    case 3: tenkey_digit(modal,4); break;
    case 4: tenkey_digit(modal,5); break;
    case 5: tenkey_digit(modal,6); break;
    case 6: tenkey_digit(modal,7); break;
    case 7: tenkey_digit(modal,8); break;
    case 8: tenkey_digit(modal,9); break;
    case 9: tenkey_backspace(modal); break;
    case 10: tenkey_digit(modal,0); break;
  }
}

static void tenkey_finish_stroke(struct modal *modal) {
  if ((MODAL->selx<0)||(MODAL->sely<0)||(MODAL->selx>=3)||(MODAL->sely>=4)) return;
  int btnid=MODAL->sely*3+MODAL->selx; // 1,2,3,4,5,6,7,8,9,DEL,0,ENTER
  switch (btnid) {
    case 11: tenkey_submit(modal); break;
  }
}

/* Move cursor.
 */
 
static void tenkey_move(struct modal *modal,int dx,int dy) {
  bm_sound(RID_sound_uimotion);
  MODAL->selx+=dx;
  MODAL->sely+=dy;
  if (MODAL->selx<0) MODAL->selx=2; else if (MODAL->selx>2) MODAL->selx=0;
  if (MODAL->sely<0) MODAL->sely=3; else if (MODAL->sely>3) MODAL->sely=3;
}

/* Update.
 */
 
static void _tenkey_update(struct modal *modal,double elapsed) {
  if (!MODAL->finger) {
    #define DPAD(tag,dx,dy) if ((g.input[0]&EGG_BTN_##tag)&&!(g.pvinput[0]&EGG_BTN_##tag)) tenkey_move(modal,dx,dy);
    DPAD(LEFT,-1,0)
    DPAD(RIGHT,1,0)
    DPAD(UP,0,-1)
    DPAD(DOWN,0,1)
    #undef DPAD
  }
  
  // Activate button is stateful so we can animate appealingly.
  if (MODAL->blackout) {
    if (!(g.input[0]&EGG_BTN_SOUTH)) MODAL->blackout=0;
  } else if (g.input[0]&EGG_BTN_SOUTH) {
    if (!MODAL->finger) {
      MODAL->finger=1;
      tenkey_activate(modal);
    }
  } else if (MODAL->finger) {
    MODAL->finger=0;
    tenkey_finish_stroke(modal);
  }
  
  // WEST to dismiss without committing.
  if ((g.input[0]&EGG_BTN_WEST)&&!(g.pvinput[0]&EGG_BTN_WEST)) {
    modal->defunct=1;
    bm_sound(RID_sound_reject);
  }
}

/* Render.
 */
 
static void _tenkey_render(struct modal *modal) {
  //TODO enter and exit transitions
  
  const int outer_margin=3;
  const int inner_margin=5; // Vertical, between display and keys. Includes one row of padding inside the display.
  const int totalw=outer_margin*2+NS_sys_tilesize*3;
  const int totalh=NS_sys_tilesize*5+outer_margin*2+inner_margin;
  
  // Frame and display background.
  int x0=(FBW>>1)-(totalw>>1);
  int y0=(FBH>>1)-(totalh>>1);
  graf_fill_rect(&g.graf,x0,y0,totalw,totalh,0xc0c0c0ff);
  graf_fill_rect(&g.graf,x0+outer_margin,y0+outer_margin,NS_sys_tilesize*3,NS_sys_tilesize+1,0x002000ff);
  
  // Prepare for griddish tiles.
  int gridx0=x0+outer_margin+(NS_sys_tilesize>>1);
  int displayy=y0+outer_margin+(NS_sys_tilesize>>1);
  int gridy0=displayy+inner_margin+NS_sys_tilesize;
  graf_set_image(&g.graf,RID_image_cave_sprites);
  
  // Display digits.
  if (MODAL->value>=100) graf_tile(&g.graf,gridx0,displayy,0x40+MODAL->value/100,0);
  else graf_tile(&g.graf,gridx0,displayy,0x4a,0);
  if (MODAL->value>=10) graf_tile(&g.graf,gridx0+NS_sys_tilesize,displayy,0x40+(MODAL->value/10)%10,0);
  else graf_tile(&g.graf,gridx0+NS_sys_tilesize,displayy,0x4a,0);
  graf_tile(&g.graf,gridx0+NS_sys_tilesize*2,displayy,0x40+MODAL->value%10,0);
  
  // Digits and such.
  int row=0,y=gridy0,btnid=0;
  for (;row<4;row++,y+=NS_sys_tilesize) {
    int col=0,x=gridx0;
    for (;col<3;col++,x+=NS_sys_tilesize,btnid++) {
      uint8_t tileid=0;
      switch (btnid) {
        case 0: tileid=0x63; break; // 1...
        case 1: tileid=0x64; break;
        case 2: tileid=0x65; break;
        case 3: tileid=0x66; break;
        case 4: tileid=0x67; break;
        case 5: tileid=0x68; break;
        case 6: tileid=0x69; break;
        case 7: tileid=0x6a; break;
        case 8: tileid=0x6b; break;
        case 9: tileid=0x6c; break; // backspace
        case 10: tileid=0x62; break; // zero
        case 11: tileid=0x6d; break; // enter
      }
      int press=0;
      if ((col==MODAL->selx)&&(row==MODAL->sely)&&MODAL->finger) {
        press=1;
        graf_tile(&g.graf,x,y,0x61,0);
      } else {
        graf_tile(&g.graf,x,y,0x60,0);
      }
      if (tileid) {
        graf_tile(&g.graf,x,y+(press?2:0),tileid,0);
      }
      if (press) {
        graf_tile(&g.graf,x,y+2,0x71,0);
      } else if ((col==MODAL->selx)&&(row==MODAL->sely)) {
        graf_tile(&g.graf,x,y,0x70,0);
      }
    }
  }
}

/* Type definition.
 */
 
const struct modal_type modal_type_tenkey={
  .name="tenkey",
  .objlen=sizeof(struct modal_tenkey),
  .del=_tenkey_del,
  .init=_tenkey_init,
  .update=_tenkey_update,
  .render=_tenkey_render,
};

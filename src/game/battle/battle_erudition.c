/* battle_erudition.c
 */

#include "game/bellacopia.h"

#define END_COOLDOWN 1.0
#define PICW 128
#define PICH 96
#define FRAMEW 144
#define FRAMEH 112
#define WORD_LIMIT 32 /* At least as long as the longest critique, across all languages. */

// With these params, I am able to win just barely at 0xf0. At 0xff the best I could do is one keystroke short.
// 0x80 is easy but you do have to keep up.
#define INITIAL_HOLD_SLOW 1.0
#define INITIAL_HOLD_FAST 0.5
#define HOLD_SLOW 0.500
#define HOLD_FAST 0.100

struct battle_erudition {
  struct battle hdr;
  double end_cooldown;
  
  int picsrcx,picsrcy;
  const char *speech_text;
  int speech_textc;
  int speech_texid,speechw,speechh;
  struct word {
    uint16_t x,y; // Position in (speech_texid).
    uint16_t p; // Position in (speech_text).
    uint16_t c; // Length in (speech_text). Printed width is (c*4-1), and height is 5.
  } wordv[WORD_LIMIT];
  int wordc;
  
  struct player {
    int who;
    int human;
    int srcx,srcy; // 32x48, 2 frames
    int wordc;
    int blackout;
    int mouth;
    double holdclock;
    double holdtime;
  } playerv[2];
};

#define BATTLE ((struct battle_erudition*)battle)

/* Delete.
 */
 
static void _erudition_del(struct battle *battle) {
  egg_texture_del(BATTLE->speech_texid);
}

/* Initialize player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
  if (player==BATTLE->playerv+0) { // Left
    player->who=0;
  } else { // Right
    player->who=1;
  }
  if (player->human=human) { // Human
  } else { // CPU
    double skill=(double)battle->args.bias/255.0;
    if (!player->who) skill=1.0-skill;
    player->holdclock=INITIAL_HOLD_SLOW*(1.0-skill)+INITIAL_HOLD_FAST*skill;
    player->holdtime=HOLD_SLOW*(1.0-skill)+HOLD_FAST*skill;
  }
  switch (appearance) {
    case 0: { // Snoblin
        player->srcx=16;
        player->srcy=208;
      } break;
    case 1: { // Dot
        player->srcx=80;
        player->srcy=208;
      } break;
    case 2: { // Princess
        player->srcx=144;
        player->srcy=208;
      } break;
  }
  player->blackout=1;
}

/* Generate the speech image and per-word geometry in it.
 */
 
static void erudition_generate_text(struct battle *battle,int strix) {
  int wlimit=((FBW-FRAMEW)>>1)-4;
  int hlimit=100;
  BATTLE->speech_textc=text_get_string(&BATTLE->speech_text,RID_strings_battle,strix);
  if (BATTLE->speech_textc<1) {
    BATTLE->speech_text="ERROR";
    BATTLE->speech_textc=5;
  }
  
  /* Start by populating (wordv) with the broken words.
   * Once populated, we will know the exact texture bounds.
   * It preserves enough detail to write each word individually, in a later pass.
   */
  BATTLE->wordc=0;
  {
    const char *src=BATTLE->speech_text;
    int srcc=BATTLE->speech_textc;
    int srcp=0;
    int x=0,y=0;
    while (srcp<srcc) {
      if ((unsigned char)src[srcp]<=0x20) {
        srcp++;
        continue;
      }
      const char *sub=src+srcp;
      int subc=0;
      while ((srcp<srcc)&&((unsigned char)src[srcp]>0x20)) { srcp++; subc++; }
      int subw=subc*4-1;
      if (x+subw>wlimit) {
        if (subw>wlimit) { // An individual word is longer than one line. Trim the word.
          subc=wlimit/4;
          subw=subc*4-1;
        }
        if (x+subw>wlimit) {
          x=0;
          y+=6;
        }
      }
      
      struct word *word=BATTLE->wordv+BATTLE->wordc++;
      word->x=x;
      word->y=y;
      word->p=sub-src;
      word->c=subc;
      if (BATTLE->wordc>=WORD_LIMIT) break; // Text too long. Omit some.
      x+=subw;
      x+=4;
    }
  }
  
  /* Determine the texture's bounds from (wordv), and allocate it.
   */
  BATTLE->speechw=0;
  BATTLE->speechh=0;
  {
    struct word *word=BATTLE->wordv;
    int i=BATTLE->wordc;
    for (;i-->0;word++) {
      if (word->y>BATTLE->speechh) BATTLE->speechh=word->y;
      int xz=word->x+word->c*4-1;
      if (xz>BATTLE->speechw) BATTLE->speechw=xz;
    }
    BATTLE->speechh+=5;
    if (BATTLE->speechw<1) BATTLE->speechw=1; // Shouldn't be possible.
    if (!BATTLE->speech_texid) BATTLE->speech_texid=egg_texture_new();
    egg_texture_load_raw(BATTLE->speech_texid,BATTLE->speechw,BATTLE->speechh,BATTLE->speechw<<2,0,0);
    egg_texture_clear(BATTLE->speech_texid);
  }
  
  /* Iterate the words again and this time write them to the new texture.
   * We have a mono G0 font with 3x5 glyphs starting at (208,224).
   */
  {
    graf_set_output(&g.graf,BATTLE->speech_texid);
    graf_set_image(&g.graf,RID_image_battle_erudition);
    struct word *word=BATTLE->wordv;
    int i=BATTLE->wordc;
    for (;i-->0;word++) {
      const char *src=BATTLE->speech_text+word->p;
      int srci=word->c;
      int x=word->x;
      int y=word->y;
      for (;srci-->0;src++,x+=4) {
        int srcx=208+((*src)&15)*3;
        int srcy=224+((*src-0x20)>>4)*5;
        graf_decal(&g.graf,x,y,srcx,srcy,3,5);
      }
    }
    graf_set_output(&g.graf,1);
  }
}

/* New.
 */

static int _erudition_init(struct battle *battle) {
  
  /* Paintings are probably temporary. The current four were copied from Wikipedia:
   * The Art of Painting; by Johannes Vermeer; 1666–1668; oil on canvas; 1.3 × 1.1 m; Kunsthistorisches Museum (Vienna, Austria)
   * A Sunday Afternoon on the Island of La Grande Jatte, 1884–1886, oil on canvas, 207.5 × 308.1 cm, Art Institute of Chicago
   * The Swing; by Jean-Honoré Fragonard; 1767–1768; oil on canvas; Wallace Collection
   * The Garden of Earthly Delights; by Hieronymus Bosch; c. 1504; oil on panel, Museo del Prado
   * TODO
   * As with the criticism text, we need to be very careful to ensure we have the right to use this content. I haven't done so yet.
   * At the limit, we can draw and criticize four paintings ourselves.
   */
  switch (rand()&3) {
    case 0: {
        BATTLE->picsrcx=0;
        BATTLE->picsrcy=0;
        erudition_generate_text(battle,60);
      } break;
    case 1: {
        BATTLE->picsrcx=128;
        BATTLE->picsrcy=0;
        erudition_generate_text(battle,61);
      } break;
    case 2: {
        BATTLE->picsrcx=0;
        BATTLE->picsrcy=96;
        erudition_generate_text(battle,62);
      } break;
    case 3: {
        BATTLE->picsrcx=128;
        BATTLE->picsrcy=96;
        erudition_generate_text(battle,63);
      } break;
  }
  
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Speak the next word.
 */

static void player_speak(struct battle *battle,struct player *player) {
  if (player->wordc>=BATTLE->wordc) {
    battle->outcome=player->who?-1:1;
    BATTLE->end_cooldown=END_COOLDOWN;
    player->mouth=0;
    return;
  }
  player->wordc++;
  bm_sound_pan(RID_sound_chatter,player->who?0.250:-0.250);
  player->blackout=1;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) {
      player->mouth=0;
      player->blackout=0;
    }
  } else if (input&EGG_BTN_SOUTH) {
    player->mouth=1;
    player_speak(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->holdclock-=elapsed)<=0.0) {
    player->holdclock+=player->holdtime;
    player->mouth=1;
    player_speak(battle,player);
  } else if (player->holdclock<0.050) {
    player->mouth=0;
  }
}

/* Update.
 */
 
static void _erudition_update(struct battle *battle,double elapsed) {

  // Done?
  if (battle->outcome>-2) return;
  
  // Players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    if (battle->outcome>-2) return;
  }
}

/* Render frame.
 * Cover 8 pixels interior to each side.
 */
 
static void render_frame(int x,int y,int w,int h) {
  const int ht=NS_sys_tilesize>>1;
  graf_tile(&g.graf,x+ht,y+ht,0xc0,0);
  graf_tile(&g.graf,x+w-ht,y+ht,0xc0,EGG_XFORM_SWAP|EGG_XFORM_YREV);
  graf_tile(&g.graf,x+ht,y+h-ht,0xc0,EGG_XFORM_SWAP|EGG_XFORM_XREV);
  graf_tile(&g.graf,x+w-ht,y+h-ht,0xc0,EGG_XFORM_XREV|EGG_XFORM_YREV);
  int tx=x+ht+NS_sys_tilesize;
  int end=x+w-NS_sys_tilesize;
  for (;tx<end;tx+=NS_sys_tilesize) {
    graf_tile(&g.graf,tx,y+ht,0xc1,0);
    graf_tile(&g.graf,tx,y+h-ht,0xc1,EGG_XFORM_XREV|EGG_XFORM_YREV);
  }
  int ty=y+ht+NS_sys_tilesize;
  end=y+h-NS_sys_tilesize;
  for (;ty<end;ty+=NS_sys_tilesize) {
    graf_tile(&g.graf,x+ht,ty,0xc1,EGG_XFORM_SWAP|EGG_XFORM_XREV);
    graf_tile(&g.graf,x+w-ht,ty,0xc1,EGG_XFORM_SWAP|EGG_XFORM_YREV);
  }
}

/* Render word bubble background.
 * Caller provides a focus point, the top center of whoever's speaking.
 * Bubbles always go above.
 * We return in (*dstx,*dsty) the starting position of the first word.
 * The (w,h) you provide are for the text you intend to print. The bubble itself will be larger.
 */
 
static void bubble_render(int *dstx,int *dsty,struct battle *battle,int speakerx,int speakery,int w,int h) {

  // Initial stab at the bounds.
  int outerx=speakerx-(w>>1)-4;
  int outerw=w+8;
  int outery=speakery-12-h;
  int outerh=h+8; // Not counting the stem.
  
  /* Preserve a certain border of the source image, assume that it contains edge artifacts.
   * We may assume that anything inside the border can be sliced arbitrarily.
   * This informs our minimum dimensions.
   */
  const int bdr=4;
  const int inr=32-(bdr<<1);
  if (outerw<bdr<<1) {
    outerw=bdr<<1;
    outerx=speakerx-(outerw>>1);
  }
  if (outerh<bdr<<1) {
    outerh=bdr<<1;
    outery=speakery-12-outerh;
  }
  
  // Bunch of decals for the body.
  graf_decal(&g.graf,outerx,outery,208,192,bdr,bdr);
  graf_decal(&g.graf,outerx+outerw-bdr,outery,240-bdr,192,bdr,bdr);
  graf_decal(&g.graf,outerx,outery+outerh-bdr,208,224-bdr,bdr,bdr);
  graf_decal(&g.graf,outerx+outerw-bdr,outery+outerh-bdr,240-bdr,224-bdr,bdr,bdr);
  int x=outerx+bdr;
  int stop=outerx+outerw-bdr;
  for (;;) {
    int w=stop-x;
    if (w<1) break;
    if (w>inr) w=inr;
    graf_decal(&g.graf,x,outery,208+bdr,192,w,bdr);
    graf_decal(&g.graf,x,outery+outerh-bdr,208+bdr,224-bdr,w,bdr);
    int y=outery+bdr;
    int stopy=outery+outerh-bdr;
    for (;;) {
      int h=stopy-y;
      if (h<1) break;
      if (h>inr) h=inr;
      graf_decal(&g.graf,x,y,208+bdr,192+bdr,w,h);
      y+=h;
    }
    x+=w;
  }
  int y=outery+bdr;
  stop=outery+outerh-bdr;
  for (;;) {
    int h=stop-y;
    if (h<1) break;
    if (h>inr) h=inr;
    graf_decal(&g.graf,outerx,y,208,192+bdr,bdr,h);
    graf_decal(&g.graf,outerx+outerw-bdr,y,240-bdr,192+bdr,bdr,h);
    y+=h;
  }
  
  // And a tile for the stem.
  graf_tile(&g.graf,speakerx,outery+outerh-1,0xcf,0);
  
  *dstx=outerx+4;
  *dsty=outery+4;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int w=32,h=48;
  int dsty=120;
  int dstx=40;
  uint8_t xform=0;
  if (player->who) {
    dstx=FBW-dstx-w;
    xform=EGG_XFORM_XREV;
  }
  graf_decal_xform(&g.graf,dstx,dsty,player->srcx+player->mouth*32,player->srcy,w,h,xform);
  
  if (player->wordc>0) {
    const struct word *lastword=BATTLE->wordv+player->wordc-1;
    int wordw=lastword->c*4-1;
    int wx,wy;
    bubble_render(&wx,&wy,battle,dstx+(w>>1),dsty,BATTLE->speechw,lastword->y+5);
    graf_set_input(&g.graf,BATTLE->speech_texid);
    graf_decal(&g.graf,wx,wy,0,0,BATTLE->speechw,lastword->y);
    graf_decal(&g.graf,wx,wy+lastword->y,0,lastword->y,lastword->x+wordw,5);
    graf_set_image(&g.graf,RID_image_battle_erudition);
  }
}

/* Render.
 */
 
static void _erudition_render(struct battle *battle) {

  /* Tasteful eggshell-white wall and hardwood floor.
   * With a cool perspective effect on the floor.
   */
  const int horizon=150;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0xf8f0e0ff);
  graf_fill_rect(&g.graf,0,horizon,FBW,FBH-horizon,0x603010ff);
  int x=0; for (;x<FBW;x+=20) {
    int nx=(x-(FBW>>1))*2+(FBW>>1);
    graf_line(&g.graf,x,horizon,0x502008ff,nx,FBH,0x502008ff);
  }
  graf_fill_rect(&g.graf,0,horizon,FBW,1,0x000000ff);
  
  // Picture and frame.
  int picx=(FBW>>1)-(PICW>>1);
  int picy=20;
  graf_set_image(&g.graf,RID_image_battle_erudition);
  graf_decal(&g.graf,picx,picy,BATTLE->picsrcx,BATTLE->picsrcy,PICW,PICH);
  render_frame(picx-8,picy-8,FRAMEW,FRAMEH);
  
  // Players.
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  // Frog.
  if (battle->outcome&&(battle->outcome>-2)) {
    graf_tile(&g.graf,FBW>>1,144,0xdf,0);
    graf_tile(&g.graf,FBW>>1,160,(g.framec&16)?0xc3:0xc4,(battle->outcome>0)?EGG_XFORM_XREV:0);
  } else {
    graf_tile(&g.graf,FBW>>1,160,0xc2,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_erudition={
  .name="erudition",
  .objlen=sizeof(struct battle_erudition),
  .strix_name=55,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_erudition_del,
  .init=_erudition_init,
  .update=_erudition_update,
  .render=_erudition_render,
};

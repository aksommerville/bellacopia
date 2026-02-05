/* battle_hiring.c
 * Asymmetric game, one player only, where you pick the liar from a set of job applicants.
 * Each applicant has a resume with a list of qualifications.
 * Each qualification is verifiable by looking at the applicant. eg "wears glasses".
 * Also adding a cheesy cpu-vs-cpu mode where the player just picks at random, because this one happens along the Princess's rescue path.
 */

#include "game/bellacopia.h"

#define END_COOLDOWN 1.0
#define APPLICANT_LIMIT 6 /* Must not exceed NAME_COUNT. */
#define NAME_STRIX 81 /* Each name is followed by contact info. */
#define NAME_COUNT 6
#define RESUMEY 65
#define RESUMEW (FBW-40)
#define RESUMEH (FBH-RESUMEY)
#define TIMELIMIT_EASY 30.0
#define TIMELIMIT_HARD 30.0 /* No need to increase the time limit; we're increasing the workload. */
#define SLIDE_IN_SPEED 400.0
#define SLIDE_OUT_SPEED 400.0
#define PRINCESS_STEP_TIME 1.500 /* Max 12 steps. */

// Little-endian order of property bits matches the strings. (93+prop*2+enabled)
#define PROP_FORKED   0x01
#define PROP_FAT      0x02
#define PROP_GLASSES  0x04
#define PROP_TEETH    0x08
#define PROP_NECKTIE  0x10
#define PROP_HAT      0x20

struct battle_hiring {
  uint8_t handicap;
  uint8_t players;
  void (*cb_end)(int outcome,void *userdata);
  void *userdata;
  int outcome;
  double end_cooldown;
  int texid_scratch; // For use while generating resumes.
  int texid_prompt;
  int promptw,prompth;
  double timelimit;
  int oppropc; // How many of the six properties are relevant? We'll only show so many.
  
  struct applicant {
    uint8_t props; // Low 6 bits, orthogonal.
    int texid; // The resume. Full pic, outline and everything.
    uint32_t skin_color; // Primary for all but necktie and hat.
    uint32_t tie_color; // All colors are decorative.
    uint32_t hat_color; // Goblins don't discriminate by skin color in hiring.
    uint8_t hat_choice; // 0..3, entirely decorative.
    int name; // strix in RID_strings_battle. +1 is contact info.
    int dstx; // Center of my 32x32 output.
  } applicantv[APPLICANT_LIMIT];
  int applicantc;
  int cursor; // 0..APPLICANT_COUNT-1
  int liarp;
  
  /* We show one resume at a time, and during transitions slide off the bottom.
   * This lags behind (cursor).
   */
  struct applicant *resume;
  double resumey; // framebuffer pixels: RESUMEY..FBH
  double resumedy; // Speed baked in.
  
  // Princess mode (NS_players_cpu_cpu).
  int princess_choice; // <0 until the choice is made.
  double princess_clock;
};

#define CTX ((struct battle_hiring*)ctx)

/* Delete.
 */
 
static void _hiring_del(void *ctx) {
  struct applicant *applicant=CTX->applicantv;
  int i=CTX->applicantc;
  for (;i-->0;applicant++) {
    egg_texture_del(applicant->texid);
  }
  egg_texture_del(CTX->texid_scratch);
  egg_texture_del(CTX->texid_prompt);
  free(ctx);
}

/* Initialize one applicant.
 * Sets props and colors but does not render the resume yet.
 * (CTX->applicantv,c) must be up to date, and not include the one in question.
 */
 
static struct applicant *applicant_by_props(void *ctx,uint8_t props) {
  struct applicant *applicant=CTX->applicantv;
  int i=CTX->applicantc;
  for (;i-->0;applicant++) {
    if (applicant->props==props) return applicant;
  }
  return 0;
}
 
static struct applicant *applicant_by_name(void *ctx,int name) {
  struct applicant *applicant=CTX->applicantv;
  int i=CTX->applicantc;
  for (;i-->0;applicant++) {
    if (applicant->name==name) return applicant;
  }
  return 0;
}

static uint32_t color_near(uint32_t rgba) {
  int r=(rgba>>24)&0xff;
  int g=(rgba>>16)&0xff;
  int b=(rgba>>8)&0xff;
  // Kick each channel up to 32 points away. So we need 18 bits total, make it a single rand call.
  int bits=rand(),d;
  d=bits&0x3f; if (d&0x20) d|=~0x3f; bits>>=6;
  r+=d; if (r<0) r=0; else if (r>0xff) r=0xff;
  d=bits&0x3f; if (d&0x20) d|=~0x3f; bits>>=6;
  g+=d; if (g<0) g=0; else if (g>0xff) g=0xff;
  d=bits&0x3f; if (d&0x20) d|=~0x3f; bits>>=6;
  b+=d; if (b<0) b=0; else if (b>0xff) b=0xff;
  return (r<<24)|(g<<16)|(b<<8)|0xff;
}
 
static void applicant_init(void *ctx,struct applicant *applicant) {
  
  // (props) and (name) are purely random but must not collide with existing applicants.
  int panic;
  for (panic=100;panic-->0;) {
    applicant->props=(rand()&0x3f);
    if (!applicant_by_props(ctx,applicant->props)) break;
  }
  for (panic=100;panic-->0;) {
    applicant->name=NAME_STRIX+(rand()%NAME_COUNT)*2;
    if (!applicant_by_name(ctx,applicant->name)) break;
  }
  
  // Colors and hats are random, and collisions are allowed. All decorative only.
  applicant->skin_color=color_near(0x2d823dff);
  applicant->tie_color=color_near(0x804020ff);
  applicant->hat_color=color_near(0x204080ff);
  applicant->hat_choice=rand()&3;
}

/* Generate one applicant's resume.
 * If (lie), we'll pick one property at random and say the opposite.
 */
 
static void applicant_generate_resume(void *ctx,struct applicant *applicant,int lie) {
  if (applicant->texid) return;
  applicant->texid=egg_texture_new();
  egg_texture_load_raw(applicant->texid,RESUMEW,RESUMEH,RESUMEW<<2,0,0);
  egg_texture_clear(applicant->texid);
  graf_set_output(&g.graf,applicant->texid);
  
  /* 1 pixel of shadow on the right.
   * Black outline left, top, and right, and white interior.
   * No outline on the bottom.
   */
  graf_fill_rect(&g.graf,RESUMEW-1,2,1,RESUMEH,0x000000c0);
  graf_fill_rect(&g.graf,0,0,RESUMEW-1,RESUMEH,0x000000ff);
  graf_fill_rect(&g.graf,1,1,RESUMEW-3,RESUMEH-1,0xffffffff);
  
  int y=3,srcw,srch;
  #define GETSTR(strix) { \
    graf_flush(&g.graf); /* We're about to overwrite a texture that might be queued for render. */ \
    const char *src=0; \
    int srcc=text_get_string(&src,RID_strings_battle,strix); \
    font_render_to_texture(CTX->texid_scratch,g.font,src,srcc,RESUMEW-5,FBH,0x000000ff); \
    egg_texture_get_size(&srcw,&srch,CTX->texid_scratch); \
  }
  
  // Name centered at the top.
  GETSTR(applicant->name)
  graf_set_input(&g.graf,CTX->texid_scratch);
  graf_decal(&g.graf,(RESUMEW>>1)-(srcw>>1),y,0,0,srcw,srch);
  y+=srch;
  
  /* Contact details just below that, aligned either left or right.
   * Or if there's exactly one space in the details, put each word on one edge.
   */
  {
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_battle,applicant->name+1);
    int spacep=-1,i=0;
    for (;i<srcc;i++) {
      if (src[i]==0x20) {
        if (spacep>=0) { // multiple spaces, use as-is.
          spacep=-1;
          break;
        } else {
          spacep=i;
        }
      }
    }
    if (spacep>=0) { // One space, so we'll split it left/right.
      graf_flush(&g.graf);
      font_render_to_texture(CTX->texid_scratch,g.font,src,spacep,RESUMEW-5,FBH,0x404040ff);
      egg_texture_get_size(&srcw,&srch,CTX->texid_scratch);
      graf_set_input(&g.graf,CTX->texid_scratch);
      graf_decal(&g.graf,3,y,0,0,srcw,srch);
      graf_flush(&g.graf);
      font_render_to_texture(CTX->texid_scratch,g.font,src+spacep+1,srcc-spacep-1,RESUMEW-5,FBH,0x404040ff);
      egg_texture_get_size(&srcw,&srch,CTX->texid_scratch);
      graf_set_input(&g.graf,CTX->texid_scratch);
      graf_decal(&g.graf,RESUMEW-4-srcw,y,0,0,srcw,srch);
    } else { // Render as-is, random alignment.
      graf_flush(&g.graf);
      font_render_to_texture(CTX->texid_scratch,g.font,src,spacep,RESUMEW-5,FBH,0x404040ff);
      egg_texture_get_size(&srcw,&srch,CTX->texid_scratch);
      graf_set_input(&g.graf,CTX->texid_scratch);
      int dstx=(rand()&1)?3:(RESUMEW-4-srcw);
      graf_decal(&g.graf,dstx,y,0,0,srcw,srch);
    }
    y+=srch;
  }
  
  // A tasteful horizontal line under the contact details, optionally.
  switch (rand()%5) {
    case 0: case 1: {
        graf_fill_rect(&g.graf,3,y+1,RESUMEW-7,1,0x000000ff);
        y+=3;
      } break;
    case 2: {
        graf_fill_rect(&g.graf,3,y+1,RESUMEW-7,1,0x1010a0ff);
        y+=3;
      } break;
  }
  
  // Section header.
  y+=8;
  int hdrstrix=105+rand()%4;
  GETSTR(hdrstrix)
  graf_set_input(&g.graf,CTX->texid_scratch);
  graf_decal(&g.graf,5,y,0,0,srcw,srch);
  y+=srch;
  
  /* Properties.
   * Choose a random formatting scheme.
   * If (lie), pick one of the properties to lie about.
   */
  int prop=1,strix=93;
  int liectr=-1;
  if (lie) liectr=rand()%CTX->oppropc;
  char pfx=0;
  switch (rand()%6) {
    case 0: pfx='-'; break;
    case 1: pfx='*'; break;
    case 2: pfx='~'; break;
    case 3: pfx='>'; break;
  }
  int i=CTX->oppropc;
  for (;i-->0;prop<<=1,strix+=2) {
    int positive=(applicant->props&prop)?1:0;
    if (!liectr--) positive^=1;
    const char *src=0;
    int srcc=text_get_string(&src,RID_strings_battle,strix+positive);
    char tmp[128];
    if (pfx) {
      if (srcc>sizeof(tmp)-2) srcc=sizeof(tmp)-2;
      tmp[0]=pfx;
      tmp[1]=' ';
      memcpy(tmp+2,src,srcc);
      src=tmp;
      srcc+=2;
    }
    graf_flush(&g.graf);
    font_render_to_texture(CTX->texid_scratch,g.font,src,srcc,RESUMEW-8,FBH,0x000000ff);
    egg_texture_get_size(&srcw,&srch,CTX->texid_scratch);
    graf_set_input(&g.graf,CTX->texid_scratch);
    graf_decal(&g.graf,5,y,0,0,srcw,srch);
    y+=srch;
  }
  
  #undef GETSTR
  graf_set_output(&g.graf,1);
}

/* New.
 */
 
static void *_hiring_init(
  uint8_t handicap,
  uint8_t players,
  void (*cb_end)(int outcome,void *userdata),
  void *userdata
) {
  void *ctx=calloc(1,sizeof(struct battle_hiring));
  if (!ctx) return 0;
  CTX->handicap=handicap;
  CTX->players=players;
  CTX->cb_end=cb_end;
  CTX->userdata=userdata;
  CTX->outcome=-2;
  CTX->end_cooldown=END_COOLDOWN;
  
  // Unlike most battles, we don't support 2 players.
  switch (CTX->players) {
    case NS_players_cpu_cpu: {
        CTX->princess_choice=-1;
        CTX->princess_clock=PRINCESS_STEP_TIME;
      } break;
    case NS_players_man_cpu: break;
    default: _hiring_del(ctx); return 0;
  }
  
  /* Select count of properties and applicants per handicap.
   * Both, coincidentally, range 3 thru 6.
   * My feeling is that adding an applicant is more significant than adding a property.
   */
  const uint8_t diffv[]={ 0x33,0x34,0x35,0x43,0x44,0x45,0x54,0x55,0x56,0x65,0x66 };
  int diffp=(CTX->handicap*sizeof(diffv))/255;
  if (diffp<0) diffp=0;
  else if (diffp>=sizeof(diffv)) diffp=sizeof(diffv)-1;
  CTX->oppropc=diffv[diffp]&15;
  int applicantc=diffv[diffp]>>4;
  
  // First pick random properties for each applicant.
  struct applicant *applicant=CTX->applicantv;
  for (;CTX->applicantc<applicantc;CTX->applicantc++,applicant++) {
    applicant_init(ctx,applicant);
    applicant->dstx=(FBW*(CTX->applicantc+1))/(applicantc+1);
  }
  
  // Select one as the liar.
  CTX->liarp=rand()%CTX->applicantc;
  
  // Render the resumes.
  CTX->texid_scratch=egg_texture_new();
  int i=0;
  for (applicant=CTX->applicantv;i<CTX->applicantc;i++,applicant++) {
    applicant_generate_resume(ctx,applicant,i==CTX->liarp);
  }
  egg_texture_del(CTX->texid_scratch);
  CTX->texid_scratch=0;
  
  // Generate the prompt.
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_battle,80);
  CTX->texid_prompt=font_render_to_texture(0,g.font,src,srcc,FBW,font_get_line_height(g.font),0xffffffff);
  egg_texture_get_size(&CTX->promptw,&CTX->prompth,CTX->texid_prompt);
  
  // Time limit is important! Since we are entirely user-driven, without this they could wait for the timeout.
  double diff=CTX->handicap/255.0;
  CTX->timelimit=diff*TIMELIMIT_HARD+(1.0-diff)*TIMELIMIT_EASY;
  
  // Select the left applicant initially, and let his resume slide in.
  CTX->cursor=0;
  CTX->resume=CTX->applicantv+CTX->cursor;
  CTX->resumey=FBH;
  CTX->resumedy=-SLIDE_IN_SPEED;
  
  return ctx;
}

/* Commit choice.
 */
 
static void hiring_activate(void *ctx) {
  if ((CTX->cursor<0)||(CTX->cursor>=CTX->applicantc)) return;
  if (CTX->cursor==CTX->liarp) {
    bm_sound(RID_sound_treasure);
    CTX->outcome=1;
  } else {
    bm_sound(RID_sound_reject);
    CTX->outcome=-1;
  }
}

/* Move cursor.
 */
 
static void hiring_move(void *ctx,int d) {
  CTX->cursor+=d;
  if (CTX->cursor<0) CTX->cursor=CTX->applicantc-1;
  else if (CTX->cursor>=CTX->applicantc) CTX->cursor=0;
  bm_sound(RID_sound_uimotion);
}

/* Advance the resume sliding transition, or start it if selection changed.
 */
 
static void hiring_update_resume(void *ctx,double elapsed) {
  
  /* If we're sliding down, usually just keep going.
   * But check whether the cursor has returned to this guy, and in that case reverse direction.
   */
  if (CTX->resumedy>0.0) {
    if (CTX->resume==CTX->applicantv+CTX->cursor) { // Oops, slide it back.
      CTX->resumedy=-SLIDE_IN_SPEED;
      return;
    }
    CTX->resumey+=CTX->resumedy*elapsed;
    // When the slide-out completes, select the one indicated by (cursor) and slide back in.
    if (CTX->resumey>=FBH) {
      CTX->resume=CTX->applicantv+CTX->cursor;
      CTX->resumedy=-SLIDE_IN_SPEED;
    }
    return;
  }
  
  /* Sliding up or not sliding, begin slide-out if the cursor disagrees.
   */
  if (CTX->resume!=CTX->applicantv+CTX->cursor) {
    CTX->resumedy=SLIDE_OUT_SPEED;
  }
  
  // Not sliding? Cool.
  if (CTX->resumedy>=0.0) return;
  
  // Slide up and stop at the top.
  CTX->resumey+=CTX->resumedy*elapsed;
  if (CTX->resumey<=RESUMEY) {
    CTX->resumey=RESUMEY;
    CTX->resumedy=0.0;
  }
}

/* Update, Princess mode.
 */
 
static void hiring_princess_choose(void *ctx) {
  // (handicap) is the odds of an incorrect decision, straight up.
  // At zero, she'll always guess right and at 255 she'll always guess wrong.
  int p=rand()%255;
  if (p<CTX->handicap) { // Pick an incorrect one.
    CTX->princess_choice=rand()%(CTX->applicantc-1);
    if (CTX->princess_choice>=CTX->liarp) CTX->princess_choice++;
  } else { // Pick the correct one.
    CTX->princess_choice=CTX->liarp;
  }
}
 
static void hiring_update_princess(void *ctx,double elapsed) {
  if ((CTX->princess_clock-=elapsed)>0.0) return;
  CTX->princess_clock+=PRINCESS_STEP_TIME;
  if (CTX->princess_choice<0) { // Move right until we reach the end, then make the decision.
    if (CTX->cursor<CTX->applicantc-1) {
      hiring_move(ctx,1);
    } else {
      hiring_princess_choose(ctx);
    }
  } else { // Move left until we're at the choice, then select it.
    if (CTX->cursor>CTX->princess_choice) {
      hiring_move(ctx,-1);
    } else {
      hiring_activate(ctx);
    }
  }
}

/* Update.
 */
 
static void _hiring_update(void *ctx,double elapsed) {

  if (CTX->outcome>-2) {
    if (CTX->end_cooldown>0.0) {
      CTX->end_cooldown-=elapsed;
    } else if (CTX->cb_end) {
      CTX->cb_end(CTX->outcome,CTX->userdata);
      CTX->cb_end=0;
    }
    return;
  }
  
  if ((CTX->timelimit-=elapsed)<=0.0) {
    CTX->outcome=-1;
    return;
  }
  
  hiring_update_resume(ctx,elapsed);
  
  /* Unlike most battles, we are asymmetric and one-player only.
   * So it's fair for us to read input[0].
   */
  if (CTX->players==NS_players_man_cpu) {
    if ((g.input[0]&EGG_BTN_LEFT)&&!(g.pvinput[0]&EGG_BTN_LEFT)) hiring_move(ctx,-1);
    if ((g.input[0]&EGG_BTN_RIGHT)&&!(g.pvinput[0]&EGG_BTN_RIGHT)) hiring_move(ctx,1);
    if ((g.input[0]&EGG_BTN_SOUTH)&&!(g.pvinput[0]&EGG_BTN_SOUTH)) hiring_activate(ctx);
  // ...but we do have a Princess mode:
  } else {
    hiring_update_princess(ctx,elapsed);
  }
}

/* Render one applicant.
 */
 
static void applicant_render(void *ctx,struct applicant *applicant) {
  uint8_t tileid;
  const int ht=NS_sys_tilesize>>1;
  const int ts=NS_sys_tilesize;
  const int dsty=40;
  
  /* First the tail.
   * It's one of two tiles, in the lower-left quadrant.
   */
  tileid=(applicant->props&PROP_FORKED)?0x35:0x34;
  graf_fancy(&g.graf,applicant->dstx-ht,dsty+ht,tileid,0,0,ts,0,applicant->skin_color);
  
  /* Then the body.
   * Similar to tail, but it's four tiles, one in each quadrant.
   */
  tileid=(applicant->props&PROP_FAT)?0x22:0x20;
  graf_fancy(&g.graf,applicant->dstx-ht,dsty-ht,tileid+0x00,0,0,ts,0,applicant->skin_color);
  graf_fancy(&g.graf,applicant->dstx+ht,dsty-ht,tileid+0x01,0,0,ts,0,applicant->skin_color);
  graf_fancy(&g.graf,applicant->dstx-ht,dsty+ht,tileid+0x10,0,0,ts,0,applicant->skin_color);
  graf_fancy(&g.graf,applicant->dstx+ht,dsty+ht,tileid+0x11,0,0,ts,0,applicant->skin_color);
  
  /* If we wear glasses, they overlay the top half.
   * These don't use a primary color, but keep using fancy in order to batch them.
   */
  if (applicant->props&PROP_GLASSES) {
    graf_fancy(&g.graf,applicant->dstx-ht,dsty-ht,0x24,0,0,ts,0,applicant->skin_color);
    graf_fancy(&g.graf,applicant->dstx+ht,dsty-ht,0x25,0,0,ts,0,applicant->skin_color);
  }
  
  /* Choice of teeth, also overlaying the top half.
   */
  tileid=(applicant->props&PROP_TEETH)?0x36:0x26;
  graf_fancy(&g.graf,applicant->dstx-ht,dsty-ht,tileid,0,0,ts,0,applicant->skin_color);
  graf_fancy(&g.graf,applicant->dstx+ht,dsty-ht,tileid+1,0,0,ts,0,applicant->skin_color);
  
  /* Necktie if applicable, overlaying all four.
   */
  if (applicant->props&PROP_NECKTIE) {
    graf_fancy(&g.graf,applicant->dstx-ht,dsty-ht,0x28,0,0,ts,0,applicant->tie_color);
    graf_fancy(&g.graf,applicant->dstx+ht,dsty-ht,0x29,0,0,ts,0,applicant->tie_color);
    graf_fancy(&g.graf,applicant->dstx-ht,dsty+ht,0x38,0,0,ts,0,applicant->tie_color);
    graf_fancy(&g.graf,applicant->dstx+ht,dsty+ht,0x39,0,0,ts,0,applicant->tie_color);
  }
  
  /* Hat if applicable, one tile but not aligned to the grid.
   */
  if (applicant->props&PROP_HAT) {
    int x=applicant->dstx-2;
    int y=dsty-20;
    tileid=0x2a+(applicant->hat_choice&1)+((applicant->hat_choice&2)?0x10:0);
    graf_fancy(&g.graf,x,y,tileid,0,0,ts,0,applicant->hat_color);
  }
}

/* Render.
 */
 
static void _hiring_render(void *ctx) {
  int ms=(int)(CTX->timelimit*1000.0);
  if (ms<0) ms=0;
  int sec=ms/1000;
  ms%=1000;
  if (sec>99) sec=99;

  // Background.
  uint32_t dark=0x3e5362ff;
  uint32_t lite=0x628197ff;
  int ly=40;
  graf_gradient_rect(&g.graf,0,0,FBW,ly,dark,dark,lite,lite);
  graf_gradient_rect(&g.graf,0,ly,FBW,FBH-ly,lite,lite,dark,dark);
  if ((sec<=3)&&(ms>=800)) graf_fill_rect(&g.graf,0,0,FBW,FBH,0xff000040);
  graf_set_input(&g.graf,CTX->texid_prompt);
  graf_decal(&g.graf,(FBW>>1)-(CTX->promptw>>1),5,0,0,CTX->promptw,CTX->prompth);
  
  // Applicants.
  graf_set_image(&g.graf,RID_image_cave_sprites);
  struct applicant *applicant=CTX->applicantv;
  int i=CTX->applicantc;
  for (;i-->0;applicant++) {
    applicant_render(ctx,applicant);
  }
  
  // Point at one applicant. Use a fancy tho tile would work; we can piggyback on the applicants' fancy batch.
  if ((ms||sec)&&(CTX->cursor>=0)&&(CTX->cursor<CTX->applicantc)) {
    applicant=CTX->applicantv+CTX->cursor;
    graf_fancy(&g.graf,applicant->dstx-NS_sys_tilesize,ly,0x17,0,0,NS_sys_tilesize,0,0);
  }
  
  // Resume.
  if (CTX->resume) {
    graf_set_input(&g.graf,CTX->resume->texid);
    graf_decal(&g.graf,(FBW>>1)-(RESUMEW>>1),(int)CTX->resumey,0,0,RESUMEW,RESUMEH);
  }
  
  // Clock.
  if (ms||sec) {
    if (sec<=3) {
      graf_fill_rect(&g.graf,FBW-23,FBH-17,23,17,(ms>=800)?0x800000ff:0x000000ff);
    } else {
      graf_fill_rect(&g.graf,FBW-23,FBH-17,23,17,0x000000ff);
    }
    graf_set_image(&g.graf,RID_image_cave_sprites);
    graf_tile(&g.graf,FBW-17,FBH-9,(sec<10)?0x4a:(0x40+sec/10),0);
    graf_tile(&g.graf,FBW-6,FBH-9,0x40+sec%10,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_hiring={
  .name="hiring",
  .strix_name=54,
  .no_article=0,
  .no_contest=0,
  .supported_players=(1<<NS_players_man_cpu)|(1<<NS_players_cpu_cpu),
  .del=_hiring_del,
  .init=_hiring_init,
  .update=_hiring_update,
  .render=_hiring_render,
};

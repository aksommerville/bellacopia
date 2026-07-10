/* battle_stenography.c
 * Tap all the buttons in order, following a sheet.
 */

#include "game/bellacopia.h"

// The players' sheets can show 24 glyphs. Limit should be at least that, and no sense going higher.
#define TEXT_LIMIT 24

struct battle_stenography {
  struct battle hdr;
  uint8_t text[TEXT_LIMIT]; // tileid: 0x3c,0x3d,0x3e,0x3f = L,R,U,D
  int textc;
  int gradep;
  double gradeclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t text[TEXT_LIMIT];
    int textc;
    // CPU player:
    double clock;
    double perlo,perhi;
    // After everything:
    int score;
  } playerv[2];
};

#define BATTLE ((struct battle_stenography*)battle)

/* Delete.
 */
 
static void _stenography_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  if (player->human=human) { // Human.
  } else { // CPU.
    player->perhi=0.400*player->skill+0.800*(1.0-player->skill);
    player->perlo=player->perhi*0.5;
    player->clock=player->perhi;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x0c7531ff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
}

/* New.
 */
 
static int _stenography_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  int doclen=20;
  while (BATTLE->textc<doclen) {
    BATTLE->text[BATTLE->textc++]=0x3c+(rand()&3);
  }
  
  return 0;
}

/* Add a keystroke.
 */
 
static void stenography_stroke(struct battle *battle,struct player *player,uint8_t tileid) {
  if (player->textc>=BATTLE->textc) return;
  bm_sound_pan(RID_sound_tick,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->text[player->textc++]=tileid;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) stenography_stroke(battle,player,0x3c);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) stenography_stroke(battle,player,0x3d);
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) stenography_stroke(battle,player,0x3e);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) stenography_stroke(battle,player,0x3f);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->textc>=BATTLE->textc) return;
  if ((player->clock-=elapsed)<=0.0) {
    stenography_stroke(battle,player,BATTLE->text[player->textc]);
    double n=(rand()&0xffff)/65535.0;
    player->clock=n*player->perlo+(1.0-n)*player->perhi;
  }
}

/* Update.
 */
 
static void _stenography_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* (gradeclock) is always positive after entry complete.
   * Players do not update during this stage.
   */
  if (BATTLE->gradeclock>0.0) {
    if ((BATTLE->gradeclock-=elapsed)<=0.0) {
      struct player *l=BATTLE->playerv;
      struct player *r=l+1;
      if (BATTLE->gradep>=BATTLE->textc) {
        BATTLE->gradeclock=999.999;
        if (l->score<r->score) battle->outcome=-1;
        else if (l->score>r->score) battle->outcome=1;
        else battle->outcome=0;
      } else {
        BATTLE->gradeclock=0.200;
        if (BATTLE->gradep>=l->textc) ; // No points for a letter you didn't reach.
        else if (l->text[BATTLE->gradep]==BATTLE->text[BATTLE->gradep]) l->score++;
        else l->score--;
        if (BATTLE->gradep>=r->textc) ;
        else if (r->text[BATTLE->gradep]==BATTLE->text[BATTLE->gradep]) r->score++;
        else r->score--;
        BATTLE->gradep++;
        if (BATTLE->gradep<BATTLE->textc) bm_sound(RID_sound_uimotion);
      }
    }
  
  /* Outcome not yet established, so update the players.
   */
  } else {
    struct player *player=BATTLE->playerv;
    int i=2;
    for (;i-->0;player++) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    // When the first player completes her sheet, start grading.
    if ((BATTLE->playerv[0].textc>=BATTLE->textc)||(BATTLE->playerv[1].textc>=BATTLE->textc)) {
      BATTLE->gradeclock=0.200;
    }
  }
}

/* Sheet of paper with text on it.
 */
 
static void stenography_paper_background(struct battle *battle,int x,int y,int w,int h) {
  graf_fill_rect(&g.graf,x,y,w,h,0xffffffff);
  graf_fill_rect(&g.graf,x,y,w,1,0x000000ff);
  graf_fill_rect(&g.graf,x,y,1,h,0x000000ff);
  graf_fill_rect(&g.graf,x+w-1,y,1,h,0x000000ff);
  graf_fill_rect(&g.graf,x,y+h-1,w,1,0x000000ff);
}

static void stenography_paper_foreground(struct battle *battle,int x,int y,int w,int h,const uint8_t *text,int textc,int with_commentary) {
  int x0=x+8;
  int xz=x+w-8;
  int yz=y+h-8;
  x=x0;
  y+=8;
  int p=0;
  for (;textc-->0;text++,p++) {
    if (x>xz) {
      x=x0;
      y+=16;
      if (y>yz) break;
    }
    graf_tile(&g.graf,x,y,*text,0);
    if (BATTLE->gradeclock>0.0) {
      if (with_commentary&&(p<=BATTLE->gradep)) {
        graf_tile(&g.graf,x,y,(*text==BATTLE->text[p])?0x4d:0x4e,0);
      }
      if (p==BATTLE->gradep) {
        graf_tile(&g.graf,x,y,0x4c,0);
      }
    }
    x+=16;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  if (BATTLE->gradeclock>0.0) {
    int y=80;
    int x=(player->who?280:50);
    int n=player->score;
    if (n<0) n=-n;
    if (n>99) n=99;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,x,y,'0'+n%10,0); x-=8;
    if (n>=10) {
      graf_tile(&g.graf,x,y,'0'+n/10,0); x-=8;
    }
    if (player->score<0) {
      graf_tile(&g.graf,x,y,'-',0); x-=8;
    }
  }
}

/* Render.
 */
 
static void _stenography_render(struct battle *battle) {

  int refw=FBW>>1;
  int refx=(FBW>>1)-(refw>>1);
  int refy=20;
  int refh=34;
  int pw=FBW/5;
  int ph=97;
  int lx=(FBW>>1)-pw-10;
  int rx=(FBW>>1)+10;
  int py=FBH-10-ph;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  stenography_paper_background(battle,refx,refy,refw,refh);
  stenography_paper_background(battle,lx,py,pw,ph);
  stenography_paper_background(battle,rx,py,pw,ph);
  
  graf_set_image(&g.graf,RID_image_battle_fractia2);
  stenography_paper_foreground(battle,refx,refy,refw,refh,BATTLE->text,BATTLE->textc,0);
  stenography_paper_foreground(battle,lx,py,pw,ph,BATTLE->playerv[0].text,BATTLE->playerv[0].textc,1);
  stenography_paper_foreground(battle,rx,py,pw,ph,BATTLE->playerv[1].text,BATTLE->playerv[1].textc,1);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  //TODO This would look better with a cartoon of each hero hunched over a typewriter. Just a matter of drawing that.
}

/* Type definition.
 */
 
const struct battle_type battle_type_stenography={
  .name="stenography",
  .objlen=sizeof(struct battle_stenography),
  .id=NS_battle_stenography,
  .strix_name=164,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .input=battle_input_dpad,
  .del=_stenography_del,
  .init=_stenography_init,
  .update=_stenography_update,
  .render=_stenography_render,
};

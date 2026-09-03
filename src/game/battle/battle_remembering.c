/* battle_remembering.c
 */

#include "game/bellacopia.h"

// The grid has to be 3x3; we have exactly nine tiles to put on the cards.
#define COLC 3
#define ROWC 3

struct battle_remembering {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t grid[COLC*ROWC]; // tileid in image:battle_desert, 0x07..0x0f
    int selx,sely; // 0..2
    int guesslimit;
    int guessc; // How many cards turned.
    uint16_t turnmask; // (1<<p), [p] in (grid).
    int win; // Nonzero if our last guess was correct.
    int blackout;
    double movetimelo,movetimehi; // cpu; wait time after moving.
    double turntimelo,turntimehi; // '' after turning a card.
    double cpuwait;
    uint8_t cpuplan[COLC*ROWC]; // Grid index in order, up to (guesslimit).
    int cpuplanp;
  } playerv[2];
  
  double prepareclock; // Counts down at the start, time to memorize the grid.
  uint8_t targettile;
  
  int texid_remember;
  int remw,remh;
};

#define BATTLE ((struct battle_remembering*)battle)

/* Delete.
 */
 
static void _remembering_del(struct battle *battle) {
  egg_texture_del(BATTLE->texid_remember);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  player->selx=1;
  player->sely=1;
  player->guessc=0;
  player->turnmask=0;
       if (player->skill>=0.800) player->guesslimit=6;
  else if (player->skill>=0.600) player->guesslimit=5;
  else if (player->skill>=0.400) player->guesslimit=4;
  else if (player->skill>=0.200) player->guesslimit=3;
  else if (player->skill>=0.100) player->guesslimit=2;
  else player->guesslimit=1;
  
  // Grid is our nine tiles in random order.
  uint8_t srcv[9]={0x07,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f};
  int i=9; while (i>0) {
    int srcp=rand()%i;
    i--;
    player->grid[i]=srcv[srcp];
    memmove(srcv+srcp,srcv+srcp+1,i-srcp);
  }
  
  if (player->human=human) { // Human.
    player->blackout=1;
    
  } else { // CPU.
    player->movetimehi=0.700*(1.0-player->skill)+0.500*player->skill;
    player->turntimehi=1.500*(1.0-player->skill)+0.800*player->skill;
    player->movetimelo=player->movetimehi*0.5;
    player->turntimelo=player->turntimehi*0.5;
    player->cpuwait=1.000; // Take a breath first. And note that this doesn't start ticking until (prepareclock) expires.
    
    // Record the correct grid index.
    int correctgridp=0;
    for (i=9;i-->0;) {
      if (player->grid[i]==BATTLE->targettile) {
        correctgridp=i;
        break;
      }
    }
    
    // The plan index when we guess right is 1..guesslimit inclusive. Never first, and can be OOB.
    int correctplanp=1+rand()%player->guesslimit;
    if (correctplanp<1) correctplanp=1;
    else if (correctplanp>8) correctplanp=8;
    
    // List of options for incorrect plan steps.
    uint8_t available[9]={0,1,2,3,4,5,6,7,8};
    memmove(available+correctgridp,available+correctgridp+1,8-correctgridp);
    int availablec=8;
    
    // Fill in the plan.
    for (i=0;i<9;i++) {
      if (i==correctplanp) {
        player->cpuplan[i]=correctgridp;
      } else {
        int p=rand()%availablec;
        player->cpuplan[i]=available[p];
        availablec--;
        memmove(available+p,available+p+1,availablec-p);
      }
    }
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x1d1d28ff;
        player->tileid=0x15;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x13;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x14;
      } break;
  }
}

/* New.
 */
 
static int _remembering_init(struct battle *battle) {
  BATTLE->prepareclock=3.0;
  BATTLE->targettile=0x07+rand()%9; // Must be set before player_init.
  
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_battle,221);
  BATTLE->texid_remember=font_render_to_texture(0,g.font,src,srcc,FBW,FBH,0x000000ff);
  egg_texture_get_size(&BATTLE->remw,&BATTLE->remh,BATTLE->texid_remember);
  
  return 0;
}

/* Guess a card.
 */
 
static void player_select(struct battle *battle,struct player *player) {
  int p=player->sely*3+player->selx;
  if ((p<0)||(p>=9)) return;
  if ((player->turnmask&(1<<p))||(player->guessc>=player->guesslimit)) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  player->guessc++;
  player->turnmask|=(1<<p);
  if (player->grid[p]==BATTLE->targettile) {
    bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->win=1;
  } else {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
  }
}

/* Move player's cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->selx+=dx; if (player->selx<0) player->selx=2; else if (player->selx>2) player->selx=0;
  player->sely+=dy; if (player->sely<0) player->sely=2; else if (player->sely>2) player->sely=0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1,0);
    if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1,0);
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) player_move(battle,player,0,-1);
    if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_move(battle,player,0,1);
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_select(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Delay most of the time.
  if ((player->cpuwait-=elapsed)>0.0) return;
  
  // Look at the plan, where are we headed?
  if (player->cpuplanp>=9) {
    player->cpuwait=999.999;
    return;
  }
  int dstp=player->cpuplan[player->cpuplanp];
  int dstx=dstp%3;
  int dsty=dstp/3;
  
  // If we're there, flip it and set the "turn" delay.
  if ((dstx==player->selx)&&(dsty==player->sely)) {
    player_select(battle,player);
    player->cpuplanp++;
    double n=(rand()&0xffff)/65535.0;
    player->cpuwait=player->turntimelo*(1.0-n)+player->turntimehi*n;
    return;
  }
  
  /* Proceed toward the target.
   * Don't wrap around the edges.
   * If we need to move on both axes, pick one randomly, otherwise we look too mechanical.
   */
  int needx=(dstx!=player->selx);
  int needy=(dsty!=player->sely);
  char choice;
  if (needx&&needy) choice=(rand()&1)?'x':'y';
  else choice=needx?'x':'y';
  switch (choice) {
    case 'x': player_move(battle,player,(dstx<player->selx)?-1:1,0); break;
    case 'y': player_move(battle,player,0,(dsty<player->sely)?-1:1); break;
  }
  double n=(rand()&0xffff)/65535.0;
  player->cpuwait=player->movetimelo*(1.0-n)+player->movetimehi*n;
}

/* Update.
 */
 
static void _remembering_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  BATTLE->prepareclock-=elapsed;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if ((BATTLE->prepareclock<=0.0)&&!player->win&&(player->guessc<player->guesslimit)) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
  }

  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->win) {
    if (r->win) battle->outcome=0;
    else battle->outcome=1;
  } else if (r->win) battle->outcome=-1;
  else if ((l->guessc>=l->guesslimit)&&(r->guessc>=r->guesslimit)) battle->outcome=0;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player,int midx,int midy) {

  // Grid.
  uint16_t expose=player->turnmask;
  const uint8_t *src=player->grid;
  int ry=-1; for (;ry<=1;ry++) {
    int rx=-1; for (;rx<=1;rx++,src++,expose>>=1) {
      if ((BATTLE->prepareclock>0.0)||(expose&1)) {
        graf_tile(&g.graf,midx+rx*NS_sys_tilesize,midy+ry*NS_sys_tilesize,0x06,0);
        graf_tile(&g.graf,midx+rx*NS_sys_tilesize,midy+ry*NS_sys_tilesize,*src,0);
      } else {
        graf_tile(&g.graf,midx+rx*NS_sys_tilesize,midy+ry*NS_sys_tilesize,0x05,0);
      }
    }
  }
  
  // Guess indicators.
  int gy=midy+NS_sys_tilesize*3;
  int gx=midx-((NS_sys_tilesize*player->guesslimit)>>1)+(NS_sys_tilesize>>1);
  int i=0;
  for (;i<player->guesslimit;i++,gx+=NS_sys_tilesize) {
    uint8_t tileid=0x16;
    if ((i==player->guessc-1)&&player->win) tileid=0x18;
    else if (i<player->guessc) tileid=0x17;
    graf_tile(&g.graf,gx,gy,tileid,0);
  }
  
  // Hand.
  if ((BATTLE->prepareclock<=0.0)&&(battle->outcome==-2)&&(player->guessc<player->guesslimit)) {
    graf_tile(&g.graf,midx+(player->selx-1)*NS_sys_tilesize+1,midy+(player->sely-1)*NS_sys_tilesize+5,player->tileid,0);
  }
  
  // The "Remember!" label comes later, because it's a different texture.
}

/* Render.
 */
 
static void _remembering_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  graf_set_image(&g.graf,RID_image_battle_desert);
  player_render(battle,l,FBW/3,FBH>>1);
  player_render(battle,r,(FBW*2)/3,FBH>>1);
  
  if (BATTLE->prepareclock>0.0) {
    graf_tile(&g.graf,FBW>>1,FBH>>2,0x05,0);
  } else {
    graf_tile(&g.graf,FBW>>1,FBH>>2,0x06,0);
    graf_tile(&g.graf,FBW>>1,FBH>>2,BATTLE->targettile,0);
  }
  
  // At the start, show "Remember!" above each grid.
  if (BATTLE->prepareclock>0.0) {
    graf_set_input(&g.graf,BATTLE->texid_remember);
    graf_decal(&g.graf,(FBW/3)-(BATTLE->remw>>1),(FBH>>1)-NS_sys_tilesize*2-BATTLE->remh,0,0,BATTLE->remw,BATTLE->remh);
    graf_decal(&g.graf,((FBW*2)/3)-(BATTLE->remw>>1),(FBH>>1)-NS_sys_tilesize*2-BATTLE->remh,0,0,BATTLE->remw,BATTLE->remh);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_remembering={
  .name="remembering",
  .objlen=sizeof(struct battle_remembering),
  .id=NS_battle_remembering,
  .strix_name=332,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad_a,
  .imageid_default=0,
  .del=_remembering_del,
  .init=_remembering_init,
  .update=_remembering_update,
  .render=_remembering_render,
};

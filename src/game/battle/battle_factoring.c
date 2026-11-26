/* battle_factoring.c
 */

#include "game/batsup/battle_internal.h"

#define INPUT_LIMIT 10 /* The answer is always ten digits long. */

struct battle_factoring {
  struct battle hdr;
  uint32_t primea,primeb;
  uint64_t query; // primea*primeb
  int msg_texid,msgw,msgh;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid_finger;
    char input[INPUT_LIMIT];
    int inputc;
    int selp; // 0..11
    int inpress,pvinpress;
    int blackout;
    int cpudstx,cpudsty;
    double cpuclock;
    double cpudelaylo,cpudelayhi;
    uint8_t cpuprep[INPUT_LIMIT+1]; // 0..11; button positions, not chars
    int cpuprepc,cpuprepp;
  } playerv[2];
};

#define BATTLE ((struct battle_factoring*)battle)

/* Delete.
 */
 
static void _factoring_del(struct battle *battle) {
  egg_texture_del(BATTLE->msg_texid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->selp=4;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpudstx=-1; // Figure it out during update.
    player->cpuclock=0.500;
    player->cpudelayhi=0.400*(1.0-player->skill)+0.200*player->skill;
    player->cpudelaylo=player->cpudelayhi*0.5;
    
    // At very low skill, enter a random 10-digit number.
    if (player->skill<0.150) {
      while (player->cpuprepc<INPUT_LIMIT) player->cpuprep[player->cpuprepc++]=rand()%9;
      player->cpuprep[player->cpuprepc++]=11;
    // Pick one of the primes and prepare our plan.
    } else {
      uint32_t number=(rand()&1)?BATTLE->primea:BATTLE->primeb;
      int digitc=1;
      uint32_t limit=10;
      while (number>=limit) { digitc++; if (limit>UINT32_MAX/10) break; limit*=10; }
      if (digitc>INPUT_LIMIT) return; // oops
      int i=digitc;
      for (;i-->0;number/=10) {
        int btnp=number%10;
        if (!btnp) btnp=10;
        else btnp--;
        player->cpuprep[i]=btnp;
      }
      player->cpuprep[digitc]=11; // Enter
      player->cpuprepc=digitc+1;
    }
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x895030ff;
        player->tileid_finger=0x9e;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid_finger=0x70;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid_finger=0x72;
      } break;
  }
}

/* Choose two prime numbers between 2**31 and 2**32, and multiply them together.
 * This yields a very large 64-bit integer with just two prime factors, which would be difficult to discover.
 */
 
static inline int factoring_is_prime(uint32_t n) {
  // Check odd integers from 5 up to the square root of (n). The iterator calling us ensures it's not a multiple of 2 or 3.
  int factor=(int)sqrt(n);
  if (!(factor&1)) factor--;
  for (;factor>=5;factor-=2) {
    if (!(n%factor)) return 0;
  }
  return 1;
}
 
static uint32_t factoring_choose_prime() {
  /* There are smart ways to do this, but they go over my head.
   * So we'll be kind of dumb about it: Pick a random 31-bit integer, then iterate neighbors of multiples of six from there up.
   * Every prime number is one above or one below a multiple of six.
   * A few trial runs, this finished in 29260..419003 divisions. Seems a lot, but then it only has to happen once per battle.
   */
  uint32_t q=rand()&0x7fffffff;
  q-=q%6;
  for (;;q+=6) {
    if (q&0x80000000) { // oops wrapped around
      q=0x40000002; // Lowest 31-bit multiple of six.
    }
    if (factoring_is_prime(q-1)) return q-1;
    if (factoring_is_prime(q+1)) return q+1;
  }
}
 
static int factoring_choose_numbers(struct battle *battle) {

  /* If the bias skews heavily toward a human over a robot, make it a sensible factorable number (still a product of two primes).
   * In these cases, the CPU will choose a deliberately wrong answer.
   * Bias for powerglove is 0x40 and goodluck is 0x10. Put our threshold between those.
   */
  if (
    ((battle->args.bias<=0x20)&&battle->args.lctl&&!battle->args.rctl)||
    ((battle->args.bias>=0xe0)&&!battle->args.lctl&&battle->args.rctl)
  ) {
    const int small_primev[]={2,3,5,7,11,13};
    int small_primec=sizeof(small_primev)/sizeof(small_primev[0]);
    BATTLE->primea=small_primev[rand()%small_primec];
    BATTLE->primeb=small_primev[rand()%small_primec];

  // Normal cases, pick two huge primes.
  } else {
    if (!(BATTLE->primea=factoring_choose_prime())) return -1;
    if (!(BATTLE->primeb=factoring_choose_prime())) return -1;
  }
  BATTLE->query=(uint64_t)BATTLE->primea*(uint64_t)BATTLE->primeb;
  
  // Alas, text_format_res doesn't do 64-bit integers.
  // We could update Egg for it, but hmm... how often does this really come up?
  char monster[32]; // Actual limit 19.
  int monsterc=1;
  uint64_t limit=10;
  uint64_t tmp=BATTLE->query;
  while (tmp>=limit) { monsterc++; if (limit>UINT64_MAX/10) break; limit*=10; }
  int i=monsterc;
  for (;i-->0;tmp/=10) monster[i]='0'+tmp%10;
  
  char msg[64];
  struct text_insertion insv[]={
    {.mode='s',.s={.v=monster,.c=monsterc}},
  };
  int msgc=text_format_res(msg,sizeof(msg),RID_strings_battle,438,insv,sizeof(insv)/sizeof(struct text_insertion));
  if ((msgc<1)||(msgc>sizeof(msg))) return -1;
  
  if ((BATTLE->msg_texid=font_render_to_texture(0,g_font,msg,msgc,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY_TEXT]))<1) return -1;
  egg_texture_get_size(&BATTLE->msgw,&BATTLE->msgh,BATTLE->msg_texid);
  
  return 0;
}

/* New.
 */
 
static int _factoring_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  if (factoring_choose_numbers(battle)<0) return -1;
  
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Submit my answer.
 * The first submission ends the game, this guy either wins or loses.
 */
 
static void factoring_submit(struct battle *battle,struct player *player) {
  uint32_t guess=0;
  const char *src=player->input;
  int i=player->inputc;
  for (;i-->0;src++) {
    int digit=(*src)-'0';
    if ((digit<0)||(digit>9)) {
      guess=0;
      break;
    }
    if (guess>UINT32_MAX/10) guess=0; else guess*=10;
    if (guess>UINT32_MAX-digit) guess=0; else guess+=digit;
  }
  if ((guess==BATTLE->primea)||(guess==BATTLE->primeb)) {
    battle->outcome=player->who?-1:1;
  } else {
    battle->outcome=player->who?1:-1;
  }
}

/* Backspace or append digit.
 */
 
static void factoring_input_backspace(struct battle *battle,struct player *player) {
  if (player->inputc<1) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
  } else {
    bm_sound_pan(RID_sound_tick,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->inputc-=1;
  }
}

static void factoring_input_digit(struct battle *battle,struct player *player,int v) {
  if ((v<0)||(v>9)) return;
  if (player->inputc>=INPUT_LIMIT) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
  } else {
    bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->input[player->inputc++]='0'+v;
  }
}

/* Input the focussed button.
 */
 
static void factoring_input(struct battle *battle,struct player *player) {
  switch (player->selp) {
    case 0: factoring_input_digit(battle,player,1); break;
    case 1: factoring_input_digit(battle,player,2); break;
    case 2: factoring_input_digit(battle,player,3); break;
    case 3: factoring_input_digit(battle,player,4); break;
    case 4: factoring_input_digit(battle,player,5); break;
    case 5: factoring_input_digit(battle,player,6); break;
    case 6: factoring_input_digit(battle,player,7); break;
    case 7: factoring_input_digit(battle,player,8); break;
    case 8: factoring_input_digit(battle,player,9); break;
    case 9: factoring_input_backspace(battle,player); break;
    case 10: factoring_input_digit(battle,player,0); break;
    case 11: factoring_submit(battle,player); break;
  }
}

/* Move cursor.
 */
 
static void factoring_move(struct battle *battle,struct player *player,int dx,int dy) {
  int x=player->selp%3+dx; if (x<0) x=2; else if (x>=3) x=0;
  int y=player->selp/3+dy; if (y<0) y=3; else if (y>=4) y=0;
  player->selp=y*3+x;
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) factoring_move(battle,player,-1,0);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) factoring_move(battle,player,1,0);
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) factoring_move(battle,player,0,-1);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) factoring_move(battle,player,0,1);
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    player->inpress=(input&EGG_BTN_SOUTH)?1:0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  if (player->inpress) { // Skip one frame to release the finger.
    player->inpress=0;
    return;
  }
  double n=(rand()&0xffff)/65535.0;
  player->cpuclock+=player->cpudelaylo*(1.0-n)+player->cpudelayhi*n;
  if (player->cpuprepp>=player->cpuprepc) {
    player->inpress=0;
    player->cpuclock=999.999;
  } else {
    int nextp=player->cpuprep[player->cpuprepp];
    int nextx=nextp%3;
    int nexty=nextp/3;
    int nowx=player->selp%3;
    int nowy=player->selp/3;
    if ((nextx!=nowx)&&(nexty!=nowy)) { // Random axis, to make it look less mechanical.
      if (rand()&1) nowx=nextx;
      else nowy=nexty;
    }
    if (nextx!=nowx) factoring_move(battle,player,(nextx<nowx)?-1:1,0);
    else if (nexty!=nowy) factoring_move(battle,player,0,(nexty<nowy)?-1:1);
    else {
      player->inpress=1;
      player->cpuprepp++;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->inpress!=player->pvinpress) {
    if (player->pvinpress=player->inpress) {
      factoring_input(battle,player);
    }
  }
}

/* Update.
 */
 
static void _factoring_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
}

/* Render player.
 */
 
static const uint8_t tileid_by_btnp[12]={
  0x63,0x64,0x65,
  0x66,0x67,0x68,
  0x69,0x6a,0x6b,
  0x6c,0x62,0x6d,
};
 
static void player_render(struct battle *battle,struct player *player) {

  // Buttons.
  int x1=player->who?((FBW*2)/3):(FBW/3);
  int y1=(FBH>>1)+10;
  int x0=x1-NS_sys_tilesize;
  int y0=y1-NS_sys_tilesize;
  int y=y0;
  int yi=4,btnp=0;
  for (;yi-->0;y+=NS_sys_tilesize) {
    int x=x0,xi=3;
    for (;xi-->0;x+=NS_sys_tilesize,btnp++) {
      int pressed=(player->inpress&&(player->selp==btnp));
      graf_tile(g_graf,x,y,pressed?0x61:0x60,0);
      graf_tile(g_graf,x,y+(pressed?2:0),tileid_by_btnp[btnp],0);
    }
  }
  
  // Finger.
  int fx=x0+(player->selp%3)*NS_sys_tilesize;
  int fy=y0+(player->selp/3)*NS_sys_tilesize;
  if (player->inpress) fy+=4; else fy+=2;
  uint8_t ftile=player->tileid_finger;
  if (player->inpress) ftile+=1;
  graf_tile(g_graf,fx,fy,ftile,0);
  
  // Readout.
  int inx=player->who?((FBW>>1)+20):32;
  int iny=y0-25;
  int inputp=player->inputc-INPUT_LIMIT; // Starts negative. Skip until it reaches zero. Because (inputv) is left aligned but our output should be right aligned.
  int i=INPUT_LIMIT;
  for (;i-->0;inx+=12,inputp++) {
    graf_tile(g_graf,inx,iny,(inputp>=0)?(player->input[inputp]-'0'+0x40):0x4a,0);
  }
}

/* Render.
 */
 
static void _factoring_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,26,51,120,17,0x000000ff);
  graf_fill_rect(g_graf,174,51,120,17,0x000000ff);
  
  graf_set_input(g_graf,BATTLE->msg_texid);
  graf_decal(g_graf,(FBW>>1)-(BATTLE->msgw>>1),20,0,0,BATTLE->msgw,BATTLE->msgh);
  
  graf_set_image(g_graf,RID_image_cave_sprites);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_factoring={
  .name="factoring",
  .objlen=sizeof(struct battle_factoring),
  .id=137,
  .strix_name=437,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=0, // In two player mode, the first to guess something loses, it's no fun.
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad_a,
  .imageid_default=RID_image_caves,
  .del=_factoring_del,
  .init=_factoring_init,
  .update=_factoring_update,
  .render=_factoring_render,
};

/* battle_math.c
 */

#include "game/bellacopia.h"

#define PROMPT_LIMIT 8 /* Actually can't go above 7: 2 digits per operand, 1 character operator, and spaces around the operator. */
#define INPUT_LIMIT 3 /* Hard limit; it's all we have space to render. */

struct battle_math {
  struct battle hdr;
  char prompt[PROMPT_LIMIT];
  int promptc;
  int answer;
  const char *waitmsg;
  int waitmsgc;
  double waitclock;
  int roundc; // Maximum count of rounds to play, must be odd.
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t highlight; // Tile shows in place of entry, during waits.
    char input[INPUT_LIMIT];
    int inputc;
    int selx,sely;
    int finger; // Nonzero if pressing.
    int score;
    double cpuwait; // counts down
    double cputime; // const
  } playerv[2];
};

#define BATTLE ((struct battle_math*)battle)

/* Delete.
 */
 
static void _math_del(struct battle *battle) {
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
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cputime=0.700*(1.0-player->skill)+0.300*player->skill;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x5f270dff;
        player->tileid=0x74;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x70;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x72;
      } break;
  }
}

/* Make up a question and answer. Populates (prompt,answer).
 */
 
static void math_new_question(struct battle *battle) {

  /* Some rules:
   *  - All operands and answers must be integers in 1..99.
   *  - Addition, subtraction, multiplication, and division.
   *  - Exactly two operands.
   *  - Schoolbook multiply and divide operators are in our font as 0x10 and 0x11 respectively. Use ASCII's add, subtract, and digits.
   *  - No difficulty scaling. Both parties answer the same question, so difficulty doesn't make sense.
   *  - Multiplication and division will only use operands in 2..9.
   */
  
  int opa,opb;
  char op;
  switch (rand()&3) {
    case 0: { // Addition.
        op='+';
        BATTLE->answer=2+rand()%98; // 2..99, can't be 1.
        opa=1+(rand()%(BATTLE->answer-1));
        opb=BATTLE->answer-opa;
      } break;
    case 1: { // Subtraction.
        op='-';
        opa=2+rand()%98; // 2..99, can't be 1.
        opb=1+(rand()%(opa-1));
        BATTLE->answer=opa-opb;
      } break;
    case 2: { // Multiplication.
        op=0x10;
        opa=2+rand()%8;
        opb=2+rand()%8;
        BATTLE->answer=opa*opb;
      } break;
    case 3: { // Division.
        op=0x11;
        opb=2+rand()%8;
        BATTLE->answer=2+rand()%8;
        opa=opb*BATTLE->answer;
      } break;
  }
  
  BATTLE->promptc=0;
  if (opa>=10) BATTLE->prompt[BATTLE->promptc++]='0'+opa/10;
  BATTLE->prompt[BATTLE->promptc++]='0'+opa%10;
  BATTLE->prompt[BATTLE->promptc++]=' ';
  BATTLE->prompt[BATTLE->promptc++]=op;
  BATTLE->prompt[BATTLE->promptc++]=' ';
  if (opb>=10) BATTLE->prompt[BATTLE->promptc++]='0'+opb/10;
  BATTLE->prompt[BATTLE->promptc++]='0'+opb%10;
  
  BATTLE->waitclock=2.000;
}

/* New.
 */
 
static int _math_init(struct battle *battle) {
  BATTLE->roundc=5;
  BATTLE->playclock=40.0;
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->waitmsgc=text_get_string(&BATTLE->waitmsg,RID_strings_battle,304);
  math_new_question(battle);
  return 0;
}

/* Commit answer.
 */
 
static void player_commit(struct battle *battle,struct player *player) {
  
  // Can't commit during the round's intro countdown (should be prevented before this but hey).
  if (BATTLE->waitclock>0.0) return;
  
  // If input is empty or invalid, reject with a sound.
  int v=0,i=0,valid=1;
  for (;i<player->inputc;i++) {
    char ch=player->input[i];
    if ((ch<'0')||(ch>'9')) {
      valid=0;
      break;
    }
    v*=10;
    v+=ch-'0';
  }
  if (!valid||!player->inputc) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  
  // Get both players.
  struct player *other;
  if (player==BATTLE->playerv) other=BATTLE->playerv+1; else other=BATTLE->playerv;
  
  /* Update both of us, in light of my answer.
   * I get a point if it's right, he gets the point if it's wrong.
   */
  if (v==BATTLE->answer) {
    bm_sound_pan(RID_sound_treasure,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->score++;
    player->highlight=0x78;
    other->highlight=0x7a;
  } else {
    bm_sound_pan(RID_sound_ouch,player->who?PLAYER_PAN:-PLAYER_PAN);
    other->score++;
    player->highlight=0x79;
    other->highlight=0x7b;
  }
  player->inputc=other->inputc=0;
  player->finger=other->finger=0;
  math_new_question(battle);
}

/* Append digit.
 */
 
static void player_append(struct battle *battle,struct player *player,char digit) {
  if (player->inputc>=INPUT_LIMIT) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->input[player->inputc++]=digit;
}

/* Press button.
 */
 
static void player_activate(struct battle *battle,struct player *player) {

  /* Selection OOB or waitclock pending, do nothing.
   */
  if ((player->selx<0)||(player->selx>2)||(player->sely<0)||(player->sely>3)) return;
  if (BATTLE->waitclock>0.0) return;
  
  player->finger=1; // The finger goes down even if the button is inert.
  int p=player->sely*3+player->selx;
  switch (p) {
    case 0: player_append(battle,player,'1'); break;
    case 1: player_append(battle,player,'2'); break;
    case 2: player_append(battle,player,'3'); break;
    case 3: player_append(battle,player,'4'); break;
    case 4: player_append(battle,player,'5'); break;
    case 5: player_append(battle,player,'6'); break;
    case 6: player_append(battle,player,'7'); break;
    case 7: player_append(battle,player,'8'); break;
    case 8: player_append(battle,player,'9'); break;
    case 9: if (player->inputc>0) {
        bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
        player->inputc--;
      } break;
    case 10: player_append(battle,player,'0'); break;
    case 11: player_commit(battle,player); break;
  }
}

/* Move cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->selx+=dx; if (player->selx<0) player->selx=2; else if (player->selx>2) player->selx=0;
  player->sely+=dy; if (player->sely<0) player->sely=3; else if (player->sely>3) player->sely=0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->finger) {
    if (!(input&EGG_BTN_SOUTH)) {
      player->finger=0;
    }
  } else if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1,0);
  else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1,0);
  else if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) player_move(battle,player,0,-1);
  else if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_move(battle,player,0,1);
  else if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_activate(battle,player);
}

/* Update CPU player.
 * To keep things simple, the CPU will always answer correctly.
 * I mean, come on, it's a computer, it has no excuse for getting these wrong :)
 * Difficulty will be driven entirely by speed.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Delaying?
  player->finger=0;
  if (BATTLE->waitclock>0.0) {
    player->cpuwait=player->cputime;
    return;
  }
  if (player->cpuwait>0.0) {
    player->cpuwait-=elapsed;
    return;
  }
  player->cpuwait=player->cputime;
  
  // Decide where to go.
  int dstx=player->selx;
  int dsty=player->sely;
  int reqdigitc=(BATTLE->answer>=10)?2:1;
  if (player->inputc==reqdigitc) {
    dstx=2;
    dsty=3;
  } else {
    int digit;
    if ((BATTLE->answer>=10)&&!player->inputc) digit=BATTLE->answer/10;
    else digit=BATTLE->answer%10;
    if (!digit) {
      dstx=1;
      dsty=3;
    } else {
      digit-=1;
      dstx=digit%3;
      dsty=digit/3;
    }
  }
  
  // Move finger?
  if (player->selx<dstx) { player_move(battle,player,1,0); return; }
  if (player->selx>dstx) { player_move(battle,player,-1,0); return; }
  if (player->sely<dsty) { player_move(battle,player,0,1); return; }
  if (player->sely>dsty) { player_move(battle,player,0,-1); return; }
  player_activate(battle,player);
}

/* Update.
 */
 
static void _math_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if (BATTLE->waitclock>0.0) {
    BATTLE->waitclock-=elapsed;
  }
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
  }
  
  /* Game ends when one player's score crosses half, or when our clock expires.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int thresh=BATTLE->roundc>>1;
  if ((l->score>thresh)||(r->score>thresh)||((BATTLE->playclock-=elapsed)<=0.0)) {
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  
  const int outer_margin=3;
  const int inner_margin=5; // Vertical, between display and keys. Includes one row of padding inside the display.
  const int totalw=outer_margin*2+NS_sys_tilesize*3;
  const int totalh=NS_sys_tilesize*5+outer_margin*2+inner_margin;
  int showdigits=((BATTLE->waitclock<=0.0)||!player->highlight);
  
  // Frame and display background.
  int x0;
  if (player->who) x0=(FBW>>1)+10;
  else x0=(FBW>>1)-10-totalw;
  int y0=(FBH>>1)-(totalh>>1)+20;
  graf_fill_rect(&g.graf,x0,y0,totalw,totalh,0xc0c0c0ff);
  if (showdigits) {
    graf_fill_rect(&g.graf,x0+outer_margin,y0+outer_margin,NS_sys_tilesize*3,NS_sys_tilesize+1,0x100000ff);
  }
  
  // Prepare for griddish tiles.
  int gridx0=x0+outer_margin+(NS_sys_tilesize>>1);
  int displayy=y0+outer_margin+(NS_sys_tilesize>>1);
  int gridy0=displayy+inner_margin+NS_sys_tilesize;
  graf_set_image(&g.graf,RID_image_cave_sprites);
  
  // Display digits or highlight icon.
  if (showdigits) {
    char tileidv[3]={0x4a,0x4a,0x4a};
    switch (player->inputc) {
      case 1: {
          if ((player->input[0]>=0x30)&&(player->input[0]<=0x39)) tileidv[2]=0x40+player->input[0]-0x30;
        } break;
      case 2: {
          if ((player->input[0]>=0x30)&&(player->input[0]<=0x39)) tileidv[1]=0x40+player->input[0]-0x30;
          if ((player->input[1]>=0x30)&&(player->input[1]<=0x39)) tileidv[2]=0x40+player->input[1]-0x30;
        } break;
      case 3: {
          if ((player->input[0]>=0x30)&&(player->input[0]<=0x39)) tileidv[0]=0x40+player->input[0]-0x30;
          if ((player->input[1]>=0x30)&&(player->input[1]<=0x39)) tileidv[1]=0x40+player->input[1]-0x30;
          if ((player->input[2]>=0x30)&&(player->input[2]<=0x39)) tileidv[2]=0x40+player->input[2]-0x30;
        } break;
    }
    graf_tile(&g.graf,gridx0,displayy,tileidv[0],0);
    graf_tile(&g.graf,gridx0+NS_sys_tilesize,displayy,tileidv[1],0);
    graf_tile(&g.graf,gridx0+NS_sys_tilesize*2,displayy,tileidv[2],0);
  } else if (player->highlight) {
    graf_tile(&g.graf,gridx0+NS_sys_tilesize,displayy,player->highlight,0);
  }
  
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
      if ((col==player->selx)&&(row==player->sely)&&player->finger) {
        press=1;
        graf_tile(&g.graf,x,y,0x61,0);
      } else {
        graf_tile(&g.graf,x,y,0x60,0);
      }
      if (tileid) {
        graf_tile(&g.graf,x,y+(press?2:0),tileid,0);
      }
      if (press) {
        graf_tile(&g.graf,x,y+2,player->tileid+1,0);
      } else if ((col==player->selx)&&(row==player->sely)) {
        graf_tile(&g.graf,x,y,player->tileid,0);
      }
    }
  }
}

/* Render.
 */
 
static void _math_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x102040ff);
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  // Prompt.
  const char *msg=0;
  int msgc=0;
  if (battle->outcome>-2) {
    msgc=0;
  } else if (BATTLE->waitclock>0.0) {
    msg=BATTLE->waitmsg;
    msgc=BATTLE->waitmsgc;
  } else {
    msg=BATTLE->prompt;
    msgc=BATTLE->promptc;
  }
  if (msgc) {
    int promptw=msgc*8;
    int x=(FBW>>1)-(promptw>>1)+4;
    int y=30;
    const char *v=msg;
    int i=msgc;
    graf_set_image(&g.graf,RID_image_fonttiles);
    for (;i-->0;v++,x+=8) graf_tile(&g.graf,x,y,*v,0);
  }
  
  // Scoreboard.
  graf_set_image(&g.graf,RID_image_cave_sprites);
  int sbw=BATTLE->roundc*NS_sys_tilesize;
  int sbx=(FBW>>1)-(sbw>>1)+(NS_sys_tilesize>>1);
  int sby=48;
  int i=0;
  for (;i<BATTLE->roundc;i++,sbx+=NS_sys_tilesize) {
    uint8_t tileid=0x76;
    uint32_t color=0x808080ff;
    if (i<l->score) { tileid+=1; color=l->color; }
    else if (i>=BATTLE->roundc-r->score) { tileid+=1; color=r->color; }
    graf_fancy(&g.graf,sbx,sby,tileid,0,0,NS_sys_tilesize,0,color);
  }
  
  // Players.
  player_render(battle,l);
  player_render(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_math={
  .name="math",
  .objlen=sizeof(struct battle_math),
  .id=NS_battle_math,
  .strix_name=303,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad_a,
  .imageid_default=0,
  .del=_math_del,
  .init=_math_init,
  .update=_math_update,
  .render=_math_render,
};

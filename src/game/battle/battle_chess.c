/* battle_chess.c
 * All the interesting stuff lives in the dedicated "chess" unit.
 * Our concern is just setup and presentation, mostly.
 */
 
#include "game/bellacopia.h"
#include "game/chess/chess.h"

#define CLOCK_EASY 45.0
#define CLOCK_HARD 20.0

#define LABEL_LIMIT 3
#define LABEL_RESUME 184
#define LABEL_STALEMATE 185
#define LABEL_FORFEIT 186
#define LABEL_STALEMATEQ 187
#define LABEL_AGREE 188
#define LABEL_REFUSE 189

struct battle_chess {
  struct battle hdr;
  struct chess_game chess_game;
  int use_clock;
  double clock;
  double animclock;
  int animframe;
  double king_taunt;
  
  // Our player[0] is white and player[1] is black, which is reverse of chess_game's convention.
  struct player {
    int who,human;
    uint32_t color;
  } playerv[2];
  
  struct label {
    int texid,x,y,w,h;
    int strix;
  } labelv[LABEL_LIMIT];
  int labelc;
  int labelp;
  int dboxx,dboxy,dboxw,dboxh;
  uint32_t dboxcolor;
  int dboxwho,dboxhuman;
  uint8_t dboxtileid; // For the finger.
};

#define BATTLE ((struct battle_chess*)battle)

/* Cleanup.
 */
 
static void _chess_del(struct battle *battle) {
  struct label *label=BATTLE->labelv;
  int i=BATTLE->labelc;
  for (;i-->0;label++) egg_texture_del(label->texid);
}

/* Player params.
 */
 
static uint32_t chess_color(int face) {
  switch (face) {
    case NS_face_monster: return 0x5a432aff;
    case NS_face_princess: return 0x0d3ac1ff;
    case NS_face_dot: default: return 0x9c6fd2ff; // Her usual color is a bit dark for this, we want the outlines to show.
  }
}

/* Init.
 */
 
static int _chess_init(struct battle *battle) {
  //fprintf(stderr,"%s: seed 0x%08x\n",__func__,get_rand_seed());

  BATTLE->playerv[0].who=0;
  BATTLE->playerv[1].who=1;
  
  // The two players must not use the same face. Most games it wouldn't matter, but for us it would render the board incomprehensible.
  if (battle->args.lface==battle->args.rface) {
    if (battle->args.lface==NS_face_dot) battle->args.rface=NS_face_princess; // Both Dot, make black player Princess.
    else battle->args.lface=NS_face_dot; // Any other duplication, make white player Dot.
  }
  BATTLE->playerv[0].color=chess_color(battle->args.lface);
  BATTLE->playerv[1].color=chess_color(battle->args.rface);
  
  // We support (man,man), (man,cpu), and (cpu,cpu). The (cpu,cpu) mode is asymmetric like (man,cpu).
  // So if they requested (cpu,man), swap them.
  if (!battle->args.lctl&&battle->args.rctl) {
    battle->args.lctl=battle->args.rctl;
    battle->args.rctl=0;
    battle->args.bias=0xff-battle->args.bias;
  }
  BATTLE->playerv[0].human=battle->args.lctl;
  BATTLE->playerv[1].human=battle->args.rctl;
  
  if (battle->args.lctl&&battle->args.rctl) { // Man vs Man.
    chess_game_init(&BATTLE->chess_game,'2',0.5); // Difficulty not relevant.
    BATTLE->use_clock=0;
    
  } else { // Man vs CPU or CPU vs CPU.
    BATTLE->king_taunt=1.0;
    double difficulty=battle_scalar_difficulty(battle);
    chess_game_init(&BATTLE->chess_game,'1',difficulty);
    BATTLE->use_clock=1;
    BATTLE->clock=CLOCK_HARD*difficulty+CLOCK_EASY*(1.0-difficulty);
  }
  
  return 0;
}

/* After losing one-player mode, populate (BATTLE->chess_game.fmovev) with indicators showing why you lost.
 */
 
static void chess_highlight_failure(struct battle *battle) {
  BATTLE->chess_game.fmovec=0;
  
  // No Black King, I don't know what to say. (this can't happen)
  int bkingx,bkingy;
  if (chess_find_piece(&bkingx,&bkingy,BATTLE->chess_game.board,PIECE_BLACK|PIECE_KING)<0) return;
  
  // Black King not in check, highlight him alone.
  if (!chess_is_check(BATTLE->chess_game.board,bkingx,bkingy)) {
    //BATTLE->chess_game.fmovev[0]=CHESS_MOVE_COMPOSE(bkingx,bkingy,bkingx,bkingy);
    //BATTLE->chess_game.fmovec=1;
    BATTLE->king_taunt=30.0;
    return;
  }
  
  // List the Black team's moves with validation, and highlight any one of them.
  // Note that we only highlight the "to" side of (fmovev), so we actually add two records.
  uint16_t movev[CHESS_MOVES_LIMIT_ALL];
  int movec=chess_list_team_moves(movev,CHESS_MOVES_LIMIT_ALL,BATTLE->chess_game.board,0,1);
  if (movec<1) return; // huh.
  BATTLE->chess_game.fmovev[0]=movev[0];
  BATTLE->chess_game.fmovev[1]=(movev[0]<<8)|(movev[0]>>8);
  BATTLE->chess_game.fmovec=2;
}

/* Add a label.
 */
 
static struct label *chess_add_label(struct battle *battle,int strix,uint32_t color) {
  if (BATTLE->labelc>=LABEL_LIMIT) return 0;
  struct label *label=BATTLE->labelv+BATTLE->labelc++;
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_battle,strix);
  label->texid=font_render_to_texture(0,g.font,src,srcc,FBW,FBH,color);
  egg_texture_get_size(&label->w,&label->h,label->texid);
  label->strix=strix;
  label->x=0;
  label->y=0;
  return label;
}

/* Finishing touches on a dialogue box.
 */
 
static void chess_pack_dialogue(struct battle *battle) {
  BATTLE->dboxw=BATTLE->dboxh=0;
  struct label *label=BATTLE->labelv;
  int i=BATTLE->labelc;
  for (;i-->0;label++) {
    if (label->w>BATTLE->dboxw) BATTLE->dboxw+=label->w;
    BATTLE->dboxh+=label->h;
  }
  BATTLE->dboxw+=28;
  BATTLE->dboxh+=6;
  BATTLE->dboxy=(FBH>>1)-(BATTLE->dboxh>>1);
  if (BATTLE->dboxwho) {
    BATTLE->dboxhuman=2;
    BATTLE->dboxx=FBW-BATTLE->dboxw-10;
  } else {
    BATTLE->dboxhuman=1;
    BATTLE->dboxx=10;
  }
  int y=BATTLE->dboxy+4;
  int x=BATTLE->dboxx+16;
  for (label=BATTLE->labelv,i=BATTLE->labelc;i-->0;label++) {
    label->x=x;
    label->y=y;
    y+=label->h;
  }
  BATTLE->labelp=0;
}

/* Begin the 2-player stalemate/forfeit modal.
 */
 
static void chess_pause(struct battle *battle,struct player *player) {
  while (BATTLE->labelc>0) {
    BATTLE->labelc--;
    egg_texture_del(BATTLE->labelv[BATTLE->labelc].texid);
  }
  BATTLE->dboxcolor=player->color;
  BATTLE->dboxwho=player->who;
  // Dot and Princess deliberately have very different luminances. So Dot gets black text and Princess gets white.
  uint32_t textcolor;
  if (BATTLE->dboxcolor&0x80000000) {
    textcolor=0x000000ff;
    BATTLE->dboxtileid=0x0c;
  } else {
    textcolor=0xffffffff;
    BATTLE->dboxtileid=0x0d;
  }
  chess_add_label(battle,LABEL_RESUME,textcolor);
  chess_add_label(battle,LABEL_STALEMATE,textcolor);
  chess_add_label(battle,LABEL_FORFEIT,textcolor);
  chess_pack_dialogue(battle);
}

/* Ask the other player to confirm stalemate.
 */
 
static void chess_confirm_stalemate(struct battle *battle,int who) {
  while (BATTLE->labelc>0) {
    BATTLE->labelc--;
    egg_texture_del(BATTLE->labelv[BATTLE->labelc].texid);
  }
  BATTLE->dboxcolor=BATTLE->playerv[who].color;
  BATTLE->dboxwho=who;
  // Dot and Princess deliberately have very different luminances. So Dot gets black text and Princess gets white.
  uint32_t textcolor;
  if (BATTLE->dboxcolor&0x80000000) {
    textcolor=0x000000ff;
    BATTLE->dboxtileid=0x0c;
  } else {
    textcolor=0xffffffff;
    BATTLE->dboxtileid=0x0d;
  }
  chess_add_label(battle,LABEL_STALEMATEQ,textcolor);
  chess_add_label(battle,LABEL_AGREE,textcolor);
  chess_add_label(battle,LABEL_REFUSE,textcolor);
  chess_pack_dialogue(battle);
  BATTLE->labelp=2; // "Refuse" by default.
}

/* Dismiss dialogue box.
 */
 
static void chess_dismiss_dialogue(struct battle *battle) {
  bm_sound(RID_sound_uicancel);
  while (BATTLE->labelc>0) {
    BATTLE->labelc--;
    egg_texture_del(BATTLE->labelv[BATTLE->labelc].texid);
  }
}

/* Move dialogue cursor.
 */
 
static void chess_move_dialogue(struct battle *battle,int d) {
  bm_sound(RID_sound_uimotion);
  int panic=BATTLE->labelc;
  while (panic-->=0) {
    BATTLE->labelp+=d;
    if (BATTLE->labelp<0) BATTLE->labelp=BATTLE->labelc-1;
    else if (BATTLE->labelp>=BATTLE->labelc) BATTLE->labelp=0;
    if (BATTLE->labelv[BATTLE->labelp].strix==LABEL_STALEMATEQ) continue;
    break;
  }
}

/* Activate dialogue.
 */
 
static void chess_activate_dialogue(struct battle *battle) {
  if ((BATTLE->labelp<0)||(BATTLE->labelp>=BATTLE->labelc)) return;
  bm_sound(RID_sound_uiactivate);
  int strix=BATTLE->labelv[BATTLE->labelp].strix;
  while (BATTLE->labelc>0) {
    BATTLE->labelc--;
    egg_texture_del(BATTLE->labelv[BATTLE->labelc].texid);
  }
  switch (strix) {
    case LABEL_RESUME: break;
    case LABEL_STALEMATE: {
        chess_confirm_stalemate(battle,BATTLE->dboxwho^1);
      } break;
    case LABEL_FORFEIT: {
        battle->outcome=(BATTLE->dboxwho?1:-1);
        BATTLE->chess_game.fmovec=0;
      } break;
    case LABEL_AGREE: {
        battle->outcome=0;
        BATTLE->chess_game.fmovec=0;
      } break;
    case LABEL_REFUSE: break;
  }
}

/* Update, with dialogue box open.
 */
 
static void chess_update_dialogue(struct battle *battle,double elapsed) {
  int input=g.input[BATTLE->dboxhuman];
  int pvinput=g.pvinput[BATTLE->dboxhuman];
  
  // WEST or AUX1 again to dismiss.
  if (
    ((input&EGG_BTN_AUX1)&&!(pvinput&EGG_BTN_AUX1))||
    ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST))
  ) {
    chess_dismiss_dialogue(battle);
    return;
  }
  
  // UP or DOWN to move cursor.
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) chess_move_dialogue(battle,-1);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) chess_move_dialogue(battle,1);
  
  // SOUTH to activate.
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) chess_activate_dialogue(battle);
}

/* Look for promotable Pawns and turn them into Queens.
 */
 
static void chess_check_promotion(struct battle *battle) {
  uint8_t *p;
  int i,promoted=0;
  for (p=BATTLE->chess_game.board,i=8;i-->0;p++) {
    if ((*p)==(PIECE_WHITE|PIECE_PAWN)) {
      *p=PIECE_WHITE|PIECE_QUEEN;
      promoted=1;
    }
  }
  for (p=BATTLE->chess_game.board+7*8,i=8;i-->0;p++) {
    if ((*p)==(PIECE_BLACK|PIECE_PAWN)) {
      *p=PIECE_BLACK|PIECE_QUEEN;
      promoted=1;
    }
  }
  if (promoted) {
    chess_game_reassess(&BATTLE->chess_game);
  }
}

/* Update player. Both man and CPU.
 * This is only called for the player whose turn it is right now.
 */
 
static void chess_update_player(struct battle *battle,struct player *player,double elapsed) {
  if (player->human) {
    int input=g.input[player->human];
    int pvinput=g.pvinput[player->human];
    
    // Motion.
    int dx=0,dy=0;
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) dx=-1;
    else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) dx=1;
    if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) dy=-1;
    else if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) dy=1;
    if (dx||dy) {
      if (chess_game_move(&BATTLE->chess_game,dx,dy)<0) {
        bm_sound(RID_sound_reject);
      } else {
        bm_sound(RID_sound_uimotion);
      }
    }
    
    // Cancellation.
    if ((input&EGG_BTN_WEST)&&!(pvinput&EGG_BTN_WEST)) {
      if (chess_game_cancel(&BATTLE->chess_game)<0) {
        bm_sound(RID_sound_reject);
      } else {
        bm_sound(RID_sound_uicancel);
      }
    }
    
    // Activation.
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
      int err=chess_game_activate(&BATTLE->chess_game);
      if (err<0) {
        bm_sound(RID_sound_reject);
      } else if (err>0) {
        bm_sound(RID_sound_collect);
        chess_check_promotion(battle);
      } else {
        bm_sound(RID_sound_uiactivate);
        chess_check_promotion(battle);
      }
    }
    
    // Pause. Only available when both players are human.
    if ((input&EGG_BTN_AUX1)&&!(pvinput&EGG_BTN_AUX1)&&BATTLE->playerv[0].human&&BATTLE->playerv[1].human) {
      chess_pause(battle,player);
    }
    
  // Black CPU player. He doesn't play. If his turn arises it means the white player failed to achieve checkmate.
  } else if (player->who) {
    fprintf(stderr,"%s:%d: Player reached the dummy CPU player. Assume the human failed to achieve checkmate as required.\n",__FILE__,__LINE__);
    battle->outcome=-1;
    chess_highlight_failure(battle);
    
  // White CPU player. ie the Princess, playing like a human.
  } else {
    fprintf(stderr,"%s:%d: TODO: Princess playing chess. For now let's just assume she loses.\n",__FILE__,__LINE__);
    battle->outcome=-1;
  }
}

/* Update.
 */
 
static void _chess_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  if (BATTLE->king_taunt>0.0) BATTLE->king_taunt-=elapsed;
  
  // Time up?
  if (BATTLE->use_clock) {
    if (BATTLE->clock>0.0) {
      if ((BATTLE->clock-=elapsed)<=0.0) {
        battle->outcome=-1;
        return;
      }
    }
  }
  
  // Game over?
  if (!(BATTLE->chess_game.assess&CHESS_ASSESS_RUNNING)) {
    if (BATTLE->chess_game.assess&CHESS_ASSESS_CHECK) {
      if (BATTLE->chess_game.assess&CHESS_ASSESS_WHO) { // Black wins.
        battle->outcome=-1;
      } else { // White wins.
        battle->outcome=1;
      }
    } else { // Stalemate or invalid game.
      battle->outcome=0;
    }
    return;
  }
  
  // Animate cursor.
  if ((BATTLE->animclock-=elapsed)<=0.0) {
    BATTLE->animclock+=0.200;
    if (++(BATTLE->animframe)>=2) BATTLE->animframe=0;
  }
  
  // If a dialogue box is open, players don't update normally.
  // There can be a dialogue box for the player whose turn it is not, when confirming stalemate.
  if (BATTLE->labelc) {
    chess_update_dialogue(battle,elapsed);
    return;
  }
  
  // Update the appropriate player.
  struct player *white=BATTLE->playerv;
  struct player *black=white+1;
  if (BATTLE->chess_game.turn) { // White's turn.
    chess_update_player(battle,white,elapsed);
  } else {
    chess_update_player(battle,black,elapsed);
  }
}

/* Render.
 */
 
static void _chess_render(struct battle *battle) {

  /* Background.
   * If the clock is in play, blink at the last 5 seconds.
   */
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x304060ff);
  int sec=0;
  if (BATTLE->use_clock&&(BATTLE->clock>0.0)) {
    int ms=(int)(BATTLE->clock*1000.0);
    sec=ms/1000+1;
    if (sec<1) sec=1;
    if (sec<=5) {
      ms%=1000;
      if (ms>750) {
        int alpha=ms-750;
        uint32_t rgb=(sec==1)?0xff000000:0xe0a00000;
        graf_fill_rect(&g.graf,0,0,FBW,FBH,rgb|alpha);
      }
    }
  }
  graf_set_image(&g.graf,RID_image_battle_labyrinth2);
  
  // The 8x8 board.
  int boardw=NS_sys_tilesize*8;
  int boardh=NS_sys_tilesize*8;
  int boardx=(FBW>>1)-(boardw>>1);
  int boardy=(FBH>>1)-(boardh>>1);
  int dstx0=boardx+(NS_sys_tilesize>>1);
  int dsty0=boardy+(NS_sys_tilesize>>1);
  int dsty=dsty0,dstx;
  int yi=8;
  for (;yi-->0;dsty+=NS_sys_tilesize) {
    dstx=dstx0;
    int xi=8;
    for (;xi-->0;dstx+=NS_sys_tilesize) {
      graf_tile(&g.graf,dstx,dsty,((xi&1)==(yi&1))?0x00:0x01,0);
    }
  }
  
  // A tasteful boarder.
  graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty0-NS_sys_tilesize,0x10,0);
  graf_tile(&g.graf,dstx0+boardw         ,dsty0-NS_sys_tilesize,0x12,0);
  graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty0+boardh         ,0x30,0);
  graf_tile(&g.graf,dstx0+boardw         ,dsty0+boardh         ,0x32,0);
  for (yi=8,dstx=dstx0,dsty=dsty0;yi-->0;dstx+=NS_sys_tilesize,dsty+=NS_sys_tilesize) {
    graf_tile(&g.graf,dstx,dsty0-NS_sys_tilesize,0x11,0);
    graf_tile(&g.graf,dstx,dsty0+boardh         ,0x31,0);
    graf_tile(&g.graf,dstx0-NS_sys_tilesize,dsty,0x20,0);
    graf_tile(&g.graf,dstx0+boardw         ,dsty,0x22,0);
  }
  
  // Pieces. Not interleaved with the board, because those are tiles and these are fancies.
  const uint8_t *boardp=BATTLE->chess_game.board;
  for (yi=8,dsty=dsty0;yi-->0;dsty+=NS_sys_tilesize) {
    dstx=dstx0;
    int xi=8;
    for (;xi-->0;dstx+=NS_sys_tilesize,boardp++) {
      uint8_t tileid=(*boardp)&PIECE_ROLE_MASK;
      if (tileid) {
        int dy=0;
        tileid+=1;
        uint8_t xform=0;
        // Kings get some special treatment.
        if (((*boardp)&PIECE_ROLE_MASK)==PIECE_KING) {
          if (!((*boardp)&PIECE_WHITE)&&(BATTLE->king_taunt>0.0)) {
            tileid=0x0b;
          } else if (battle->outcome>-2) {
            if ((*boardp)&PIECE_WHITE) { // White King. Is he surrendered?
              if (battle->outcome<=0) {
                xform=EGG_XFORM_SWAP;
                dy=3;
              }
            } else { // Black King. Is he surrendered?
              if (battle->outcome>=0) {
                xform=EGG_XFORM_SWAP;
                dy=3;
              }
            }
          }
        }
        uint32_t color=((*boardp)&PIECE_WHITE)?BATTLE->playerv[0].color:BATTLE->playerv[1].color;
        graf_fancy(&g.graf,dstx,dsty+dy,tileid,xform,0,NS_sys_tilesize,0,color);
      }
    }
  }
  
  // Cursor.
  if ((BATTLE->chess_game.assess&CHESS_ASSESS_RUNNING)&&(battle->outcome==-2)&&!BATTLE->labelc) {
    struct player *player=BATTLE->playerv+(BATTLE->chess_game.turn?0:1);
    if (player->who&&!player->human) {
      // Don't draw the dummy CPU player's cursor. It does appear active for exactly one frame every time.
    } else {
      int x,y;
      if (player->who) {
        x=BATTLE->chess_game.bx;
        y=BATTLE->chess_game.by;
      } else {
        x=BATTLE->chess_game.wx;
        y=BATTLE->chess_game.wy;
      }
      graf_fancy(&g.graf,
        dstx0+x*NS_sys_tilesize,dsty0+y*NS_sys_tilesize,
        0x08+BATTLE->animframe,0,
        0,NS_sys_tilesize,0,player->color
      );
    }
  }
  
  // Proposals. Should go before cursor, but they won't overlap, so let cursor join the pieces' batch.
  if (BATTLE->chess_game.fmovec&&!BATTLE->labelc) {
    uint16_t *move=BATTLE->chess_game.fmovev;
    int i=BATTLE->chess_game.fmovec;
    for (;i-->0;move++) {
      int x=CHESS_MOVE_TO_COL(*move);
      int y=CHESS_MOVE_TO_ROW(*move);
      x=dstx0+x*NS_sys_tilesize;
      y=dsty0+y*NS_sys_tilesize;
      graf_tile(&g.graf,x,y,0x0a,0);
    }
  }
  
  // Clock, optionally.
  if (sec>0) {
    int cx=(FBW>>1)-4;
    int cy=dsty0-20;
    graf_set_image(&g.graf,RID_image_fonttiles);
    if (sec>=10) graf_tile(&g.graf,cx,cy,'0'+sec/10,0); cx+=8;
    graf_tile(&g.graf,cx,cy,'0'+sec%10,0);
  }
  
  // Dialogue box.
  if (BATTLE->labelc) {
    graf_fill_rect(&g.graf,BATTLE->dboxx,BATTLE->dboxy,BATTLE->dboxw,BATTLE->dboxh,BATTLE->dboxcolor);
    graf_fill_rect(&g.graf,BATTLE->dboxx,BATTLE->dboxy,1,BATTLE->dboxh,0x000000ff);
    graf_fill_rect(&g.graf,BATTLE->dboxx,BATTLE->dboxy,BATTLE->dboxw,1,0x000000ff);
    graf_fill_rect(&g.graf,BATTLE->dboxx+BATTLE->dboxw,BATTLE->dboxy,1,BATTLE->dboxh,0x000000ff);
    graf_fill_rect(&g.graf,BATTLE->dboxx,BATTLE->dboxy+BATTLE->dboxh,BATTLE->dboxw,1,0x000000ff);
    struct label *label=BATTLE->labelv;
    int i=BATTLE->labelc,p=0;
    for (;i-->0;label++,p++) {
      graf_set_input(&g.graf,label->texid);
      graf_decal(&g.graf,label->x,label->y,0,0,label->w,label->h);
      if (p==BATTLE->labelp) {
        graf_set_image(&g.graf,RID_image_battle_labyrinth2);
        graf_tile(&g.graf,label->x-10,label->y+(label->h>>1),BATTLE->dboxtileid,0);
      }
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_chess={
  .name="chess",
  .objlen=sizeof(struct battle_chess),
  .strix_name=180,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .no_timeout=1, // For 2-player mode. Don't expect them to finish in 60 seconds :D
  .del=_chess_del,
  .init=_chess_init,
  .update=_chess_update,
  .render=_chess_render,
};

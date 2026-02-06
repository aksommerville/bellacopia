/* battle_regex.c
 */

#include "game/bellacopia.h"

#define END_COOLDOWN 1.0
#define TERM_COLC 40
#define TERM_ROWC 5
#define STRIX_FIRST 66 /* Each pair is plaintext, then regex. Regex contains '@' once marking the error. */
#define PROBLEM_COUNT 7
#define INITIAL_REPEAT_TIME 0.250
#define ONGOING_REPEAT_TIME 0.080
#define CPU_SPEED_SLOW 0.500
#define CPU_SPEED_FAST 0.200
#define CPU_INITIAL_DELAY_SLOW 5.000
#define CPU_INITIAL_DELAY_FAST 0.750

struct battle_regex {
  struct battle hdr;
  double end_cooldown;
  
  char term[TERM_COLC*TERM_ROWC];
  int errorp; // In line 3, horizontal offset only.
  int prompt_texid,promptw,prompth;
  
  struct player {
    int who;
    int human;
    int blackout;
    int cursorp; // 0..COLC-1
    uint32_t colora,colorb;
    int indx,pvdx;
    double repeatclock;
    int activate;
    double cpuclock;
    double cpuspeed;
  } playerv[2];
};

#define BATTLE ((struct battle_regex*)battle)

/* Delete.
 */
 
static void _regex_del(struct battle *battle) {
  egg_texture_del(BATTLE->prompt_texid);
}

/* Initialize players.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
  if (player==BATTLE->playerv) {
    player->who=0;
  } else {
    player->who=1;
  }
  if (player->human=human) {
  } else {
    double skill=(double)battle->args.bias/255.0;
    if (!player->who) skill=1.0-skill;
    player->cpuspeed=CPU_SPEED_SLOW*(1.0-skill)+CPU_SPEED_FAST*skill;
    player->cpuclock=CPU_INITIAL_DELAY_SLOW*(1.0-skill)+CPU_INITIAL_DELAY_FAST*skill;
  }
  switch (appearance) {
    case 0: { // Globlin
        player->colora=0x2d823dff;
        player->colorb=0x14401cff;
      } break;
    case 1: { // Dot
        player->colora=0x562395ff;
        player->colorb=0x281145ff;
      } break;
    case 2: { // Princess
        player->colora=0x2454e5ff;
        player->colorb=0x102564ff;
      } break;
  }
  player->blackout=1;
}

/* Load problem.
 */
 
static void regex_load_problem(struct battle *battle,int strix) {

  // Get the three strings, and extract the error marker from regex.
  const char *plain,*regexk,*tmpl;
  int plainc=text_get_string(&plain,RID_strings_battle,strix);
  int regexkc=text_get_string(&regexk,RID_strings_battle,strix+1);
  int tmplc=text_get_string(&tmpl,RID_strings_battle,65);
  if (regexkc>=TERM_COLC) {
    fprintf(stderr,"%s: regex %d too long: '%.*s'\n",__func__,strix+1,regexkc,regexk);
    battle->outcome=0;
    return;
  }
  char regex[TERM_COLC];
  int regexc=0;
  int i=0; for (;i<regexkc;i++) {
    if (regexk[i]=='@') {
      memcpy(regex,regexk,i);
      memcpy(regex+i,regexk+i+1,regexkc-i-1);
      regexc=regexkc-1;
      BATTLE->errorp=i;
      break;
    }
  }
  if (!regexc) {
    fprintf(stderr,"%s: regex %d no marker: '%.*s'\n",__func__,strix+1,regexkc,regexk);
    battle->outcome=0;
    return;
  }
  
  memset(BATTLE->term,' ',sizeof(BATTLE->term));
  int dstrow=0;
  char *dst=BATTLE->term;
  int tmplp=0,insplain=0,insregex=0;
  while (tmplp<tmplc) {
    const char *srcline=tmpl+tmplp;
    int srclinec=0,insp=-1;
    while ((tmplp<tmplc)&&(tmpl[tmplp++]!=0x0a)) {
      if ((insp<0)&&(srcline[srclinec]=='%')) insp=srclinec;
      srclinec++;
    }
    if (insp<0) { // Copy line verbatim.
      if (srclinec>TERM_COLC) srclinec=TERM_COLC;
      memcpy(dst,srcline,srclinec);
    } else { // Copy with insertion.
      const char *ins=0;
      int insc=0;
      if (!insplain) {
        ins=plain;
        insc=plainc;
        insplain=1;
      } else if (!insregex) {
        ins=regex;
        insc=regexc;
        insregex=1;
        BATTLE->errorp+=insp;
      }
      int totalw=srclinec-1+insc;
      if (totalw>TERM_COLC) {
        fprintf(stderr,"!!! %s:%d: Line too long, %d>%d\n",__FILE__,__LINE__,totalw,TERM_COLC);
        insc=0;
        if (srclinec>TERM_COLC) srclinec=TERM_COLC;
      }
      memcpy(dst,srcline,insp);
      memcpy(dst+insp,ins,insc);
      memcpy(dst+insp+insc,srcline+insp+1,srclinec-insp-1);
    }
    if (++dstrow>=TERM_ROWC) break;
    dst+=TERM_COLC;
  }
}

/* New.
 */

static int _regex_init(struct battle *battle) {
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  int problemp=rand()%PROBLEM_COUNT;
  int strix=STRIX_FIRST+problemp*2;
  regex_load_problem(battle,strix);
  
  /* Initially I thought, start them on opposite edges.
   * But that really doesn't work. It's a race to a given point in the string.
   * Start them both in the middle.
   */
  const char *line=BATTLE->term+TERM_COLC*3;
  int linec=TERM_COLC,indent=0;
  while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
  while ((indent<linec)&&((unsigned char)line[indent]<=0x20)) indent++;
  if (indent<linec) {
    int p=indent+((linec-indent)>>1);
    BATTLE->playerv[0].cursorp=p;
    BATTLE->playerv[1].cursorp=p;
  }
  
  const char *prompt;
  int promptc=text_get_string(&prompt,RID_strings_battle,64);
  BATTLE->prompt_texid=font_render_to_texture(0,g.font,prompt,promptc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&BATTLE->promptw,&BATTLE->prompth,BATTLE->prompt_texid);
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
    return;
  }
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0;
  }
  if (input&EGG_BTN_SOUTH) player->activate=1;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->blackout) {
    player->blackout=0;
    return;
  }
  if (player->cpuclock>0.0) {
    player->indx=0;
    if ((player->cpuclock-=elapsed)<=0.0) {
      player->cpuclock+=player->cpuspeed;
      if (player->cursorp<BATTLE->errorp) {
        player->indx=1;
      } else if (player->cursorp>BATTLE->errorp) {
        player->indx=-1;
      } else {
        player->activate=1;
      }
    }
  }
}

/* Move cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int d) {
  player->cursorp+=d;
  if (player->cursorp<0) {
    bm_sound(RID_sound_reject);
    player->cursorp=0;
  } else if (player->cursorp>=TERM_COLC) {
    bm_sound(RID_sound_reject);
    player->cursorp=TERM_COLC-1;
  } else {
    bm_sound(RID_sound_uimotion);
  }
}

/* Update either player.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->indx!=player->pvdx) {
    if (player->indx) {
      player_move(battle,player,player->indx);
      player->repeatclock=INITIAL_REPEAT_TIME;
    }
    player->pvdx=player->indx;
  } else if (player->indx) {
    if ((player->repeatclock-=elapsed)<=0.0) {
      player->repeatclock+=ONGOING_REPEAT_TIME;
      player_move(battle,player,player->indx);
    }
  }
  if (player->activate) {
    if (player->cursorp==BATTLE->errorp) {
      bm_sound(RID_sound_treasure);
      battle->outcome=player->who?-1:1;
    } else {
      bm_sound(RID_sound_reject);
      battle->outcome=player->who?1:-1;
    }
  }
}

/* Update.
 */
 
static void _regex_update(struct battle *battle,double elapsed) {

  // Done?
  if (battle->outcome>-2) return;
  
  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
    if (battle->outcome>-2) return;
  }
}

/* Render.
 */
 
static void _regex_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x202820ff);
  
  // Determine terminal bounds and fill it in.
  const int glyphw=6;
  const int glyphh=10;
  const int term_border=4;
  int termw=(term_border*2)+TERM_COLC*glyphw;
  int termh=(term_border*2)+TERM_ROWC*glyphh;
  int termx=(FBW>>1)-(termw>>1);
  int termy=(FBH>>1)-(termh>>1);
  graf_fill_rect(&g.graf,termx,termy,termw,termh,0x101410ff);
  
  // Animated cursor for each player.
  int fy=3;
  int ap=BATTLE->playerv[0].cursorp;
  int bp=BATTLE->playerv[1].cursorp;
  if (ap==bp) { // When on the same place, their 'a' colors alternate.
    uint32_t color=(g.framec&16)?BATTLE->playerv[0].colora:BATTLE->playerv[1].colora;
    graf_fill_rect(&g.graf,termx+term_border+ap*glyphw,termy+term_border+fy*glyphh,glyphw,glyphh,color);
  } else {
    uint32_t color=(g.framec&16)?BATTLE->playerv[0].colora:BATTLE->playerv[0].colorb;
    graf_fill_rect(&g.graf,termx+term_border+ap*glyphw,termy+term_border+fy*glyphh,glyphw,glyphh,color);
    color=(g.framec&16)?BATTLE->playerv[1].colora:BATTLE->playerv[1].colorb;
    graf_fill_rect(&g.graf,termx+term_border+bp*glyphw,termy+term_border+fy*glyphh,glyphw,glyphh,color);
  }
  
  // Print the text.
  graf_set_image(&g.graf,RID_image_termfont);
  graf_set_tint(&g.graf,0xffff80ff);
  const char *src=BATTLE->term;
  int y=termy+term_border+(glyphh>>1);
  int yi=TERM_ROWC;
  for (;yi-->0;y+=glyphh) {
    int x=termx+term_border+(glyphw>>1);
    int xi=TERM_COLC;
    for (;xi-->0;x+=glyphw,src++) {
      if ((unsigned char)(*src)<=0x20) continue;
      graf_tile(&g.graf,x,y,*src,0);
    }
  }
  graf_set_tint(&g.graf,0);
  
  // Static prompt above the terminal.
  graf_set_input(&g.graf,BATTLE->prompt_texid);
  graf_decal(&g.graf,(FBW>>1)-(BATTLE->promptw>>1),termy-BATTLE->prompth-2,0,0,BATTLE->promptw,BATTLE->prompth);
}

/* Type definition.
 */
 
const struct battle_type battle_type_regex={
  .name="regex",
  .objlen=sizeof(struct battle_regex),
  .strix_name=51,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_regex_del,
  .init=_regex_init,
  .update=_regex_update,
  .render=_regex_render,
};

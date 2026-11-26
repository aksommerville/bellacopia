/* battle_imitation.c
 * Between two humans, there's no difficulty bias.
 * CPU players will make a deliberate mistake, further along the sequence at higher skill.
 */

#include "game/batsup/battle_internal.h"

#define SEQ_LIMIT 10

struct battle_imitation {
  struct battle hdr;
  uint8_t seqv[SEQ_LIMIT]; // 1,2,3,4 = left,right,up,down
  int seqc;
  int lbl_texid,lblw,lblh;
  
  int idolx,idoly; // Center of top-left tile (2x2).
  double idolclock;
  int demonstrating; // Nonzero during the demonstration phase, when you only watch the idol.
  int subseqc; // <=seqc, how many are we asking for this cycle?
  int demop; // Gesture showing right now. Can be <0.
  double killclock; // During the repeat stage, if this expires anyone that isn't fully entered loses.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t gesture; // 0,1,2,3,4 = idle,left,right,up,down
    int x,y;
    uint8_t seqv[SEQ_LIMIT];
    int seqc;
    int cpumistakep; // Make a mistake at this sequence point. Greater than the initial (subseqc) to get thru the first cycle.
    double cpuclock;
  } playerv[2];
};

#define BATTLE ((struct battle_imitation*)battle)

/* Delete.
 */
 
static void _imitation_del(struct battle *battle) {
  egg_texture_del(BATTLE->lbl_texid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->y=120;
  player->x=130;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
  }
  
  if (player->human=human) { // Human.
  } else { // CPU.
  
    /* Decide when my mistake will happen.
     */
    player->cpumistakep=(int)(BATTLE->subseqc*(1.0-player->skill)+BATTLE->seqc*player->skill);
    switch (rand()%5) {
      case 0: player->cpumistakep--; break;
      case 4: player->cpumistakep++; break;
    }
    if (player->cpumistakep<0) player->cpumistakep=0;
    else if (player->cpumistakep>=BATTLE->seqc) player->cpumistakep=BATTLE->seqc-1; // Debatable. I say, CPU should always make a mistake even at top skill.
  
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x4f6da0ff;
        player->tileid=0xe0;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0xc0;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0xd0;
      } break;
  }
}

/* New.
 */
 
static int _imitation_init(struct battle *battle) {
  
  /* Compose the sequence.
   */
  while (BATTLE->seqc<7) {
    BATTLE->seqv[BATTLE->seqc++]=1+rand()%4;
  }
  BATTLE->subseqc=3;
  
  const char *src=0;
  int srcc=text_get_string(&src,RID_strings_battle,221); // Borrowed from the defunct Sumo Horse Contest.
  BATTLE->lbl_texid=font_render_to_texture(0,g_font,src,srcc,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND_TEXT]);
  egg_texture_get_size(&BATTLE->lblw,&BATTLE->lblh,BATTLE->lbl_texid);
  
  BATTLE->idolx=(FBW>>1)-(NS_sys_tilesize>>1);
  BATTLE->idoly=40;
  BATTLE->demonstrating=1;
  BATTLE->idolclock=1.000; // Long initial lead-in.
  BATTLE->demop=-1;
  
  /* Initialize players after composing the sequence: CPU players depend on it existing.
   */
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Begin gesture.
 */
 
static void imitation_imitate(struct battle *battle,struct player *player,uint8_t gesture) {
  if (BATTLE->demonstrating) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  player->gesture=gesture;
  if (player->seqc>=SEQ_LIMIT) {
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->seqv[player->seqc++]=gesture;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  switch (player->gesture) {
  
    // If a gesture is currently showing, wait for it to drop.
    case 1: if (!(input&EGG_BTN_LEFT)) player->gesture=0; break;
    case 2: if (!(input&EGG_BTN_RIGHT)) player->gesture=0; break;
    case 3: if (!(input&EGG_BTN_UP)) player->gesture=0; break;
    case 4: if (!(input&EGG_BTN_DOWN)) player->gesture=0; break;
    
    // Otherwise, allow to begin a new gesture.
    default: {
             if ((input&EGG_BTN_LEFT )&&!(pvinput&EGG_BTN_LEFT )) imitation_imitate(battle,player,1);
        else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) imitation_imitate(battle,player,2);
        else if ((input&EGG_BTN_UP   )&&!(pvinput&EGG_BTN_UP   )) imitation_imitate(battle,player,3);
        else if ((input&EGG_BTN_DOWN )&&!(pvinput&EGG_BTN_DOWN )) imitation_imitate(battle,player,4);
      }
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Tick down my clock, and drop my gesture when it's near completion.
   */
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    if (player->cpuclock<0.250) player->gesture=0;
    return;
  }
  player->cpuclock+=0.400;
  
  /* If we've completed the sequence, don't do anything.
   */
  if (player->seqc>=BATTLE->subseqc) return;
  
  /* If this sequence point is our predetermined error, pick a random wrong gesture.
   */
  if (player->seqc==player->cpumistakep) {
    uint8_t candidatev[4];
    int candidatec=0;
    if (BATTLE->seqv[player->seqc]!=1) candidatev[candidatec++]=1;
    if (BATTLE->seqv[player->seqc]!=2) candidatev[candidatec++]=2;
    if (BATTLE->seqv[player->seqc]!=3) candidatev[candidatec++]=3;
    if (BATTLE->seqv[player->seqc]!=4) candidatev[candidatec++]=4;
    imitation_imitate(battle,player,candidatev[rand()%candidatec]);
    return;
  }
  
  /* Begin the correct gesture.
   */
  imitation_imitate(battle,player,BATTLE->seqv[player->seqc]);
}

/* End demonstration stage, begin playback stage.
 */
 
static void imitation_begin_playback(struct battle *battle) {
  BATTLE->demonstrating=0;
  BATTLE->killclock=10.0;
}

/* End playback stage, begin demonstration stage or end game.
 */
 
static void imitation_end_playback(struct battle *battle) {

  /* If either player has a mistake in their sequence, the game is over.
   * If they both have a mistake, the earliest mistake index loses. Ties are of course possible.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  int lok=((l->seqc==BATTLE->subseqc)&&!memcmp(l->seqv,BATTLE->seqv,BATTLE->subseqc));
  int rok=((r->seqc==BATTLE->subseqc)&&!memcmp(r->seqv,BATTLE->seqv,BATTLE->subseqc));
  if (!lok&&!rok) {
    int i=0; for (;i<BATTLE->subseqc;i++) {
      if (l->seqv[i]!=BATTLE->seqv[i]) {
        if (r->seqv[i]!=BATTLE->seqv[i]) {
          battle->outcome=0;
          return;
        }
        battle->outcome=-1;
        return;
      }
      if (r->seqv[i]!=BATTLE->seqv[i]) {
        battle->outcome=1;
        return;
      }
    }
    return;
  }
  if (!lok) {
    battle->outcome=-1;
    return;
  }
  if (!rok) {
    battle->outcome=1;
    return;
  }
  
  /* If we just completed the full sequence, the game is over.
   * The last clause didn't return, so they both repeated it faithfully: It's a tie.
   */
  if (BATTLE->subseqc>=BATTLE->seqc) {
    battle->outcome=0;
    return;
  }
  
  /* Increase (subseqc), start the demonstration stage, and nix the players' input buffers.
   */
  BATTLE->subseqc++;
  BATTLE->demonstrating=1;
  BATTLE->demop=-1;
  BATTLE->idolclock=0.500;
  l->seqc=0;
  r->seqc=0;
  l->gesture=0;
  r->gesture=0;
}

/* Update.
 */
 
static void _imitation_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  /* Demonstration stage: Tick down the idol's clock and advance accordingly.
   */
  if (BATTLE->demonstrating) {
    if ((BATTLE->idolclock-=elapsed)<=0.0) {
      BATTLE->idolclock+=0.750;
      BATTLE->demop++;
      if (BATTLE->demop>=BATTLE->subseqc) {
        imitation_begin_playback(battle);
      }
    }
    return;
  }
  
  /* If (killclock) expires, any incomplete player loses immediately.
   */
  if ((BATTLE->killclock-=elapsed)<=0.0) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if ((l->seqc<BATTLE->subseqc)&&(r->seqc<BATTLE->subseqc)) battle->outcome=0;
    else if (l->seqc<BATTLE->subseqc) battle->outcome=-1;
    else battle->outcome=1;
    l->gesture=0;
    r->gesture=0;
    return;
  }
  
  /* Update players.
   */
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
  }
  
  /* When both players reach (subseqc), commit the playback and either end game or return to demonstration.
   * But delay it until they've dropped their gesture, otherwise it feels interrupty.
   */
  {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (!l->gesture&&!r->gesture) {
      if ((l->seqc>=BATTLE->subseqc)&&(r->seqc>=BATTLE->subseqc)) {
        imitation_end_playback(battle);
      }
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  uint8_t tileid=player->tileid+player->gesture;
  graf_tile(g_graf,player->x,player->y,tileid,0);
  
  /* Input sequence, below the hero.
   * One player's sequence goes above and the other below, otherwise they could overlap.
   * During entry, CPU players show a mystery bubble instead.
   */
  if (player->seqc) {
    const int spacing=12;
    int seqy=player->y;
    if (player->who) seqy-=NS_sys_tilesize;
    else seqy+=NS_sys_tilesize;
    int seqx=player->x-((player->seqc*spacing)>>1)+(spacing>>1);
    int i=0;
    int secret=((battle->outcome==-2)&&!BATTLE->demonstrating&&!player->human);
    for (;i<player->seqc;i++,seqx+=spacing) {
      graf_tile(g_graf,seqx,seqy,secret?0xff:(0xf0+player->seqv[i]),0);
    }
  }
}

/* Render.
 */
 
static void _imitation_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  
  if (BATTLE->demonstrating) {
    graf_set_input(g_graf,BATTLE->lbl_texid);
    graf_decal(g_graf,(FBW>>1)-(BATTLE->lblw>>1),10,0,0,BATTLE->lblw,BATTLE->lblh);
  }
  
  graf_set_image(g_graf,RID_image_battle_jungle);
  
  {
    int x0=BATTLE->idolx,y0=BATTLE->idoly;
    int x1=x0+NS_sys_tilesize,y1=y0+NS_sys_tilesize;
    uint8_t itileid=0xe5;
    if (BATTLE->demonstrating&&(BATTLE->demop>=0)&&(BATTLE->demop<BATTLE->subseqc)&&(BATTLE->idolclock>0.250)) {
      itileid+=BATTLE->seqv[BATTLE->demop]*2;
    }
    graf_tile(g_graf,x0,y0,itileid+0x00,0);
    graf_tile(g_graf,x1,y0,itileid+0x01,0);
    graf_tile(g_graf,x0,y1,itileid+0x10,0);
    graf_tile(g_graf,x1,y1,itileid+0x11,0);
  }
  
  player_render(battle,l);
  player_render(battle,r);
  
  /* Three-second warning to any incomplete player.
   */
  if (!BATTLE->demonstrating&&(BATTLE->killclock<3.0)&&(battle->outcome==-2)) {
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_GROUND_TEXT]);
    uint8_t tileid='1'+(int)BATTLE->killclock;
    if (tileid<'1') tileid='1';
    if (l->seqc<BATTLE->subseqc) {
      graf_tile(g_graf,l->x-NS_sys_tilesize,l->y,tileid,0);
    }
    if (r->seqc<BATTLE->subseqc) {
      graf_tile(g_graf,r->x+NS_sys_tilesize,r->y,tileid,0);
    }
    graf_set_tint(g_graf,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_imitation={
  .name="imitation",
  .objlen=sizeof(struct battle_imitation),
  .id=138,
  .strix_name=440,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad,
  .imageid_default=RID_image_temple,
  .del=_imitation_del,
  .init=_imitation_init,
  .update=_imitation_update,
  .render=_imitation_render,
};

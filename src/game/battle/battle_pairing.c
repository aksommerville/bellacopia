/* battle_pairing.c
 */

#include "game/batsup/battle_internal.h"

// Size of each player's field.
#define COLC 3
#define ROWC 3

// How many orphans to match in the middle. Must be odd.
#define ORPHANC 5

struct battle_pairing {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t fld[COLC*ROWC]; // tileid in image:battle_underground; zero if empty.
    int fldp;
    int blackout;
    int fldx,fldy,fldw,fldh;
    int score;
    double cputimelo,cputimehi;
    double cpuclock;
    int cpunextp;
  } playerv[2];
  
  struct orphan {
    uint8_t tileid;
    int matched; // -1,0,1 = left,unmatched,right
  } orphanv[ORPHANC];
  int ox,oy,ow,oh;
};

#define BATTLE ((struct battle_pairing*)battle)

/* Delete.
 */
 
static void _pairing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  
  player->fldw=32*COLC;
  player->fldh=32*ROWC;
  player->fldy=(FBH>>1)-(player->fldh>>1);
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->fldx=(FBW>>1)-30-player->fldw;
  } else { // Right.
    player->who=1;
    player->fldx=(FBW>>1)+30;
  }
  
  player->fldp=(ROWC>>1)*COLC+(COLC>>1);
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cputimehi=1.000*(1.0-player->skill)+0.400*player->skill;
    player->cputimelo=player->cputimehi*0.5;
    player->cpunextp=-1;
    player->cpuclock=1.000;
  }
  
  switch (face) {
    case NS_face_monster: {
        player->color=0x1fab10ff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
  
  /* Prepare the field.
   * First, the five orphans get placed in random positions.
   * Then all remaining positions are filled with random socks. Could be duplicates, that's ok.
   */
  int emptypv[COLC*ROWC];
  int emptypc=COLC*ROWC;
  int i=emptypc; while (i-->0) emptypv[i]=i;
  int orphanp=ORPHANC;
  while (orphanp-->0) {
    int p=rand()%emptypc;
    player->fld[emptypv[p]]=BATTLE->orphanv[orphanp].tileid;
    emptypc--;
    memmove(emptypv+p,emptypv+p+1,sizeof(int)*(emptypc-p));
  }
  while (emptypc-->0) {
    int p=emptypv[emptypc];
    uint8_t tileid=rand()%18;
    if (tileid>=9) tileid=0x71+(tileid-9)*16;
    else tileid=0x70+tileid*16;
    player->fld[p]=tileid;
  }
}

/* New.
 */
 
static int _pairing_init(struct battle *battle) {

  /* Select the orphans. All are unique.
   * We must do this before player_init, so they can ensure all pairs are possible.
   */
  uint8_t candidatev[]={
    0x70,0x80,0x90,0xa0,0xb0,0xc0,0xd0,0xe0,0xf0,
    0x71,0x81,0x91,0xa1,0xb1,0xc1,0xd1,0xe1,0xf1,
  };
  int candidatec=sizeof(candidatev);
  int orphanc=0;
  while (orphanc<ORPHANC) {
    struct orphan *orphan=BATTLE->orphanv+orphanc++;
    int cp=rand()%candidatec;
    orphan->tileid=candidatev[cp];
    candidatec--;
    memmove(candidatev+cp,candidatev+cp+1,candidatec-cp);
  }
  
  /* Premeasure the orphans' well.
   */
  BATTLE->ow=32;
  BATTLE->oh=ORPHANC*32;
  BATTLE->ox=(FBW>>1)-(BATTLE->ow>>1);
  BATTLE->oy=(FBH>>1)-(BATTLE->oh>>1);

  /* Initialize players.
   */
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Activate.
 */
 
static void player_activate(struct battle *battle,struct player *player) {
  if ((player->fldp<0)||(player->fldp>=COLC*ROWC)) return; // Not possible but stay safe.
  uint8_t tileid=player->fld[player->fldp];
  if (!tileid) { // Selected an empty slot.
    bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
    return;
  }
  struct orphan *orphan=BATTLE->orphanv;
  int i=ORPHANC;
  for (;i-->0;orphan++) {
    if (!orphan->matched&&(orphan->tileid==tileid)) {
      orphan->matched=player->who?1:-1;
      bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
      player->score++;
      player->fld[player->fldp]=0;
      return;
    }
  }
  // Wrong sock.
  bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Move cursor.
 */
 
static void player_move(struct battle *battle,struct player *player,int dx,int dy) {
  int x=player->fldp%COLC+dx; if (x<0) x=COLC-1; else if (x>=COLC) x=0;
  int y=player->fldp/COLC+dy; if (y<0) y=ROWC-1; else if (y>=ROWC) y=0;
  player->fldp=y*COLC+x;
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1,0);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1,0);
  if ((input&EGG_BTN_UP)&&!(pvinput&EGG_BTN_UP)) player_move(battle,player,0,-1);
  if ((input&EGG_BTN_DOWN)&&!(pvinput&EGG_BTN_DOWN)) player_move(battle,player,0,1);
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_activate(battle,player);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  /* Regulate timing. We act at specific intervals, random each time.
   */
  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  double n=(rand()&0xffff)/65535.0;
  player->cpuclock+=player->cputimelo*(1.0-n)+player->cputimehi*n;
  
  /* If we don't have a "next" selection, find one.
   */
  if ((player->cpunextp<0)||(player->cpunextp>=COLC*ROWC)) {
    int candidatev[COLC*ROWC];
    int candidatec=0;
    const struct orphan *orphan=BATTLE->orphanv;
    int i=ORPHANC;
    for (;i-->0;orphan++) { // orphan tileid are all unique; we depend on that here.
      if (orphan->matched) continue;
      int j=COLC*ROWC; while (j-->0) {
        if (player->fld[j]==orphan->tileid) {
          candidatev[candidatec++]=j;
        }
      }
    }
    if (candidatec<1) { // oops?
      player->cpuclock=30.0;
      return;
    }
    player->cpunextp=candidatev[rand()%candidatec];
  }
  
  /* Which way do we have to move to reach (cpunextp)?
   * We won't use wall warp.
   * If both axes need motion, pick one randomly.
   */
  int dx=(player->cpunextp%COLC)-(player->fldp%COLC);
  int dy=(player->cpunextp/COLC)-(player->fldp/COLC);
  if (dx&&dy) {
    if (rand()&1) dx=0; else dy=0;
  }
  if (dx) {
    player_move(battle,player,dx,0);
    return;
  }
  if (dy) {
    player_move(battle,player,0,dy);
    return;
  }
  
  /* Check that it's still orphaned.
   * If so, activate.
   * Then either way, blank (cpunextp).
   */
  uint8_t tileid=player->fld[player->fldp];
  int still_orphaned=0;
  const struct orphan *orphan=BATTLE->orphanv;
  int i=ORPHANC;
  for (;i-->0;orphan++) {
    if (orphan->matched) continue;
    if (orphan->tileid==tileid) {
      still_orphaned=1;
      break;
    }
  }
  if (still_orphaned) {
    player_activate(battle,player);
  }
  player->cpunextp=-1;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  //TODO
}

/* Update.
 */
 
static void _pairing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game ends when the sum of scores reaches ORPHANC.
   * Ties can only happen if we time out (not effected here).
   * But we're coding as if they are possible, maybe if I accidentally change ORPHANC to something even.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->score+r->score>=ORPHANC) {
    if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
  }

  //XXX
  if (g_input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render bits.
 */
 
static void pairing_render_field(struct battle *battle,struct player *player) {
  int x0=player->fldx+16,y=player->fldy+16;
  const uint8_t *tileid=player->fld;
  int yi=ROWC; for (;yi-->0;y+=32) {
    int x=x0,xi=COLC; for (;xi-->0;x+=32,tileid++) {
      if (!*tileid) continue; // Zero is an empty slot. It's a real tileid, but not for this battle.
      graf_tile(g_graf,x,y,*tileid,0);
    }
  }
}

static void pairing_render_cursor(struct battle *battle,struct player *player) {
  int col=player->fldp%ROWC;
  int row=player->fldp/ROWC;
  int x1=player->fldx+16+32*col;
  int y1=player->fldy+16+32*row;
  int x0=x1-NS_sys_tilesize;
  int x2=x1+NS_sys_tilesize;
  int y0=y1-NS_sys_tilesize;
  int y2=y1+NS_sys_tilesize;
  graf_fancy(g_graf,x0,y0,0xfa,0,0,NS_sys_tilesize,0,player->color);
  graf_fancy(g_graf,x1,y0,0xfb,0,0,NS_sys_tilesize,0,player->color);
  graf_fancy(g_graf,x2,y0,0xfa,EGG_XFORM_XREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(g_graf,x0,y1,0xfb,EGG_XFORM_SWAP,0,NS_sys_tilesize,0,player->color);
  graf_fancy(g_graf,x2,y1,0xfb,EGG_XFORM_SWAP|EGG_XFORM_YREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(g_graf,x0,y2,0xfa,EGG_XFORM_YREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(g_graf,x1,y2,0xfb,EGG_XFORM_YREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(g_graf,x2,y2,0xfa,EGG_XFORM_XREV|EGG_XFORM_YREV,0,NS_sys_tilesize,0,player->color);
}

static void pairing_render_orphans(struct battle *battle) {
  int x=BATTLE->ox+(BATTLE->ow>>1);
  int y=BATTLE->oy+16;
  const struct orphan *orphan=BATTLE->orphanv;
  int i=ORPHANC;
  for (;i-->0;orphan++,y+=32) {
    if (orphan->matched<0) {
      graf_tile(g_graf,x-12,y,orphan->tileid,0);
      graf_tile(g_graf,x-8,y,orphan->tileid,0);
    } else if (orphan->matched>0) {
      graf_tile(g_graf,x+12,y,orphan->tileid,0);
      graf_tile(g_graf,x+8,y,orphan->tileid,0);
    } else {
      graf_tile(g_graf,x,y,orphan->tileid,0);
    }
  }
}

/* Render.
 */
 
static void _pairing_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  /* Fill the background and wells.
   */
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,l->fldx,l->fldy,l->fldw,l->fldh,0xffffff80);
  graf_fill_rect(g_graf,r->fldx,r->fldy,r->fldw,r->fldh,0xffffff80);
  graf_fill_rect(g_graf,BATTLE->ox,BATTLE->oy,BATTLE->ow,BATTLE->oh,0xffff0080);
  
  /* Then tiles for all the socks.
   */
  graf_set_image(g_graf,RID_image_battle_underground);
  pairing_render_field(battle,l);
  pairing_render_field(battle,r);
  pairing_render_orphans(battle);
  
  /* Then a cursor for each player.
   * These are separated so we hopefully get just one batch of tiles and one of fancies.
   * (not that that makes even the slightest difference).
   */
  pairing_render_cursor(battle,l);
  pairing_render_cursor(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_pairing={
  .name="pairing",
  .objlen=sizeof(struct battle_pairing),
  .id=120,
  .strix_name=400,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad_a,
  .imageid_default=RID_image_caves,
  .del=_pairing_del,
  .init=_pairing_init,
  .update=_pairing_update,
  .render=_pairing_render,
};

/* battle_mixing.c
 */

#include "game/bellacopia.h"

struct battle_mixing {
  struct battle hdr;
  double playclock;
  uint32_t target; // rgba
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color; // Hero's identity color, nothing to do with the puzzle.
    uint8_t tileid;
    double r,g,b; // Raw quantity of each color, in, let's call it pixels.
    uint32_t mixcolor; // Derived from (r,g,b).
    int mixh; // Height of drink in pixels. May exceed the glass's height (44).
    int handp; // 0,1,2 = r,g,b
    int pouring;
    int blackout;
    double pourrate; // px/s
    int spill;
    double cpur,cpug,cpub;
    double cpuhold;
  } playerv[2];
};

#define BATTLE ((struct battle_mixing*)battle)

/* Delete.
 */
 
static void _mixing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  player->pourrate=10.0;
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
  
    /* CPU decides in advance what color it's going to mix.
     * The target is already established.
     * We have a guaranteed range of 170+170+127 = 467.
     * My worst score after 20 or so plays was 140, and I'm usually under 70.
     * There's a little complication though: One of the output channels must be 0xff.
     * ...I think I screwed this up. It tends to make very close to white.
     * But that's good. CPU is supposed to be really bad at it. Cool.
     */
    int err=(int)(60.0*player->skill+350.0*(1.0-player->skill));
    int tr=(BATTLE->target>>24)&0xff;
    int tg=(BATTLE->target>>16)&0xff;
    int tb=(BATTLE->target>>8)&0xff;
    // Divide the error evenly across all three channels, going up for dark ones and down for light ones.
    err/=3;
    int r,g,b;
    if (tr<0x80) r=tr+err; else r=tr-err;
    if (tg<0x80) g=tg+err; else g=tg-err;
    if (tb<0x80) b=tb+err; else b=tb-err;
    // Then scale those such that we spend 7 seconds pouring.
    player->cpur=r;
    player->cpug=g;
    player->cpub=b;
    double sum=player->cpur+player->cpug+player->cpub;
    double targeth=42.0;///player->pourrate;
    double scale=targeth/sum;
    player->cpur*=scale;
    player->cpug*=scale;
    player->cpub*=scale;
    
    player->cpuhold=0.500;
  
  }
  player->handp=1;
  switch (face) {
    case NS_face_monster: {
        player->color=0x6a5d4eff;
        player->tileid=0x54;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x34;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x44;
      } break;
  }
}

/* New.
 */
 
static int _mixing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  BATTLE->playclock=9.0;
  
  /* Choose a target color.
   * There will always be one channel in its top third, one in its bottom third, and one anywhere.
   * Which of those is which is random, six possibilities.
   * Players' guesses always have a 0xff in them, so most of the time it's not possible to match exactly.
   */
  int x=rand()%85;
  int y=rand()%85;
  int z=rand()%85;
  switch (rand()%6) {
    case 0: x+=170; y*=3; break;
    case 1: x+=170; z*=3; break;
    case 2: y+=170; x*=3; break;
    case 3: y+=170; z*=3; break;
    case 4: z+=170; x*=3; break;
    case 5: z+=170; y*=3; break;
  }
  BATTLE->target=(x<<24)|(y<<16)|(z<<8)|0xff;
  
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  return 0;
}

/* Move player's hand.
 */
 
static void player_move(struct battle *battle,struct player *player,int d) {
  player->handp+=d;
  if (player->handp<0) player->handp=2;
  else if (player->handp>2) player->handp=0;
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1);
  if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1);
  if (player->blackout) {
    player->pouring=0;
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else if (input&EGG_BTN_SOUTH) {
    if (!player->pouring) bm_sound_pan(RID_sound_glug,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->pouring=1;
  } else {
    if (player->pouring) bm_sound_pan(RID_sound_glug2,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->pouring=0;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  if (player->cpuhold>0.0) {
    player->cpuhold-=elapsed;
    return;
  }

  double have,want;
  switch (player->handp) {
    case 0: have=player->r; want=player->cpur; break;
    case 1: have=player->g; want=player->cpug; break;
    case 2: have=player->b; want=player->cpub; break;
  }
  
  if (have<want) {
    if (!player->pouring) bm_sound_pan(RID_sound_glug,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->pouring=1;
    return;
  }
  
  if (player->pouring) {
    bm_sound_pan(RID_sound_glug2,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->pouring=0;
    player->cpuhold=0.250;
    return;
  }

  double diff0=player->cpur-player->r;
  double diff1=player->cpug-player->g;
  double diff2=player->cpub-player->b;
  if ((diff0<=0.050)&&(diff1<=0.050)&&(diff2<=0.050)) { // All very close to plan. Neutralize.
    player->cpuhold=10.0;
    return;
  }
  if ((diff0>=diff1)&&(diff0>=diff2)) {
    if (player->handp>0) {
      player_move(battle,player,-1);
      player->cpuhold=0.250;
    }
  } else if (diff1>=diff2) {
    if (player->handp<1) {
      player_move(battle,player,1);
      player->cpuhold=0.250;
    } else if (player->handp>1) {
      player_move(battle,player,-1);
      player->cpuhold=0.250;
    }
  } else {
    if (player->handp<2) {
      player_move(battle,player,1);
      player->cpuhold=0.250;
    }
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (player->spill) {
    player->pouring=0;
    return;
  }

  if (player->pouring) {
    
    switch (player->handp) {
      case 0: player->r+=player->pourrate*elapsed; break;
      case 1: player->g+=player->pourrate*elapsed; break;
      case 2: player->b+=player->pourrate*elapsed; break;
    }
    
    double sum=player->r+player->g+player->b;
    player->mixh=(int)sum;
    double max=player->r;
    if (player->g>max) max=player->g;
    if (player->b>max) max=player->b;
    int r=(int)((player->r*255.0)/max); if (r<0) r=0; else if (r>0xff) r=0xff;
    int g=(int)((player->g*255.0)/max); if (g<0) g=0; else if (g>0xff) g=0xff;
    int b=(int)((player->b*255.0)/max); if (b<0) b=0; else if (b>0xff) b=0xff;
    player->mixcolor=(r<<24)|(g<<16)|(b<<8)|0xff;
    
    if (player->mixh>=46) { // The exact point where it's one pixel over the cup's lip.
      player->spill=1;
    }
  }
}

/* Judge the two colors and set (battle->outcome).
 */
 
static void mixing_judge(struct battle *battle,struct player *l,struct player *r) {
  int tr=(BATTLE->target>>24)&0xff;
  int tg=(BATTLE->target>>16)&0xff;
  int tb=(BATTLE->target>> 8)&0xff;
  int lr=(l->mixcolor>>24)&0xff; lr-=tr; if (lr<0) lr=-lr;
  int lg=(l->mixcolor>>16)&0xff; lg-=tg; if (lg<0) lg=-lg;
  int lb=(l->mixcolor>> 8)&0xff; lb-=tb; if (lb<0) lb=-lb;
  int rr=(r->mixcolor>>24)&0xff; rr-=tr; if (rr<0) rr=-rr;
  int rg=(r->mixcolor>>16)&0xff; rg-=tg; if (rg<0) rg=-rg;
  int rb=(r->mixcolor>> 8)&0xff; rb-=tb; if (rb<0) rb=-rb;
  int ld=lr+lg+lb;
  int rd=rr+rg+rb;
  if (ld<rd) battle->outcome=1;
  else if (ld>rd) battle->outcome=-1;
  else battle->outcome=0;
}

/* Update.
 */
 
static void _mixing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (!player->spill) {
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
    player_update_common(battle,player,elapsed);
  }
  
  BATTLE->playclock-=elapsed;
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->spill) {
    if (r->spill) battle->outcome=0;
    else battle->outcome=-1;
  } else if (r->spill) battle->outcome=1;
  else if (BATTLE->playclock<=0.0) {
    mixing_judge(battle,l,r);
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int midx=player->who?((FBW*2)/3):(FBW/3);
  int midy=FBH>>1; // Center of the glass's top tile.
  
  /* The mix, behind the glass, and the pouring color if applicable.
   */
  graf_set_input(&g.graf,0);
  if (player->pouring) {
    int x=midx-NS_sys_tilesize+player->handp*NS_sys_tilesize-2;
    int y=midy-NS_sys_tilesize;
    uint32_t color;
    switch (player->handp) {
      case 0: color=0xff0000ff; break;
      case 1: color=0x00ff00ff; break;
      case 2: color=0x0000ffff; break;
    }
    graf_fill_rect(&g.graf,x,y,3,53,color);
  }
  if (player->mixh>0) {
    int x=midx-NS_sys_tilesize-(NS_sys_tilesize>>1)+1;
    int y=midy-(NS_sys_tilesize>>1)+45-player->mixh;
    graf_fill_rect(&g.graf,x,y,46,player->mixh,player->mixcolor);
  }
  
  /* The glass, 9 tiles.
   */
  graf_set_image(&g.graf,RID_image_battle_underground);
  graf_tile(&g.graf,midx-NS_sys_tilesize,midy,0x07,0);
  graf_tile(&g.graf,midx                ,midy,0x08,0);
  graf_tile(&g.graf,midx+NS_sys_tilesize,midy,0x09,0);
  graf_tile(&g.graf,midx-NS_sys_tilesize,midy+NS_sys_tilesize,0x17,0);
  graf_tile(&g.graf,midx                ,midy+NS_sys_tilesize,0x18,0);
  graf_tile(&g.graf,midx+NS_sys_tilesize,midy+NS_sys_tilesize,0x19,0);
  graf_tile(&g.graf,midx-NS_sys_tilesize,midy+NS_sys_tilesize*2,0x27,0);
  graf_tile(&g.graf,midx                ,midy+NS_sys_tilesize*2,0x28,0);
  graf_tile(&g.graf,midx+NS_sys_tilesize,midy+NS_sys_tilesize*2,0x29,0);
  
  /* Hand or spill indicator. If spilled, the hand has hid itself in shame.
   */
  if (player->spill) {
    int x=midx;
    if (player->who) x+=NS_sys_tilesize*2; else x-=NS_sys_tilesize*2;
    graf_tile(&g.graf,x,midy-(NS_sys_tilesize>>1),0x03,0);
  } else {
    graf_tile(&g.graf,midx-NS_sys_tilesize+player->handp*NS_sys_tilesize,midy-NS_sys_tilesize*2,player->tileid,0);
  }
  
  /* The bottles.
   */
  graf_fancy(&g.graf,midx-NS_sys_tilesize,midy-NS_sys_tilesize-2,0x16,(player->pouring&&(player->handp==0))?EGG_XFORM_YREV:0,0,NS_sys_tilesize,0,0xff0000ff);
  graf_fancy(&g.graf,midx                ,midy-NS_sys_tilesize-2,0x16,(player->pouring&&(player->handp==1))?EGG_XFORM_YREV:0,0,NS_sys_tilesize,0,0x00ff00ff);
  graf_fancy(&g.graf,midx+NS_sys_tilesize,midy-NS_sys_tilesize-2,0x16,(player->pouring&&(player->handp==2))?EGG_XFORM_YREV:0,0,NS_sys_tilesize,0,0x0000ffff);
}

/* Render.
 */
 
static void _mixing_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  graf_fill_rect(&g.graf,(FBW>>1)-NS_sys_tilesize+2,(FBH>>1)-NS_sys_tilesize+2,(NS_sys_tilesize<<1)-4,(NS_sys_tilesize<<1)-4,BATTLE->target);
  graf_set_image(&g.graf,RID_image_battle_underground);
  graf_tile(&g.graf,(FBW>>1)-(NS_sys_tilesize>>1),(FBH>>1)-(NS_sys_tilesize>>1),0x0a,0);
  graf_tile(&g.graf,(FBW>>1)+(NS_sys_tilesize>>1),(FBH>>1)-(NS_sys_tilesize>>1),0x0b,0);
  graf_tile(&g.graf,(FBW>>1)-(NS_sys_tilesize>>1),(FBH>>1)+(NS_sys_tilesize>>1),0x1a,0);
  graf_tile(&g.graf,(FBW>>1)+(NS_sys_tilesize>>1),(FBH>>1)+(NS_sys_tilesize>>1),0x1b,0);
  
  if (BATTLE->playclock>0.0) {
    int s=(int)(BATTLE->playclock+0.999);
    if (s<1) s=1; else if (s>9) s=9;
    graf_set_image(&g.graf,RID_image_fonttiles);
    graf_tile(&g.graf,FBW>>1,60,'0'+s,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_mixing={
  .name="mixing",
  .objlen=sizeof(struct battle_mixing),
  .id=NS_battle_mixing,
  .strix_name=292,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_horz_a,
  .imageid_default=0,
  .del=_mixing_del,
  .init=_mixing_init,
  .update=_mixing_update,
  .render=_mixing_render,
};

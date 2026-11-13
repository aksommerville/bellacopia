/* battle_pilfering.c
 */

#include "game/batsup/battle_internal.h"

#define VEG_LIMIT 32

struct battle_pilfering {
  struct battle hdr;
  
  uint8_t surfacetilev[NS_sys_mapw];
  int farmerx,farmery;
  int farmerwhack;
  double farmerclock;
  int farmerframe;
  uint8_t farmerxform;
  double explodex,explodey; // <0 if no explosion
  int carrotc;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double x,y;
    int indx,indy;
    double animclock;
    int animframe;
    double walkspeed; // px/s
    int breach; // Nonzero if surface breached -- you lose.
    int blowedup; // Nonzero if we tripped a bomb -- you lose.
    int score;
    double cputhrulevel; // Constant. CPU digs to this (y) before moving laterally.
    double cputargetx; // <0 if no target active. Reset to -1 generically when carrot collected.
    int cpudx; // Nonzero if moving laterally.
  } playerv[2];
  
  struct veg {
    double x,y;
    uint8_t tileid;
  } vegv[VEG_LIMIT];
  int vegc;
};

#define BATTLE ((struct battle_pilfering*)battle)

/* Delete.
 */
 
static void _pilfering_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=80.0;
  player->y=120.0*(1.0-player->skill)+80.0*player->skill;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  player->x+=((rand()&0xffff)*40.0)/65535.0-20.0;
  
  player->walkspeed=40.0*(1.0-player->skill)+60.0*player->skill;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cputhrulevel=80.0*(1.0-player->skill)+50.0*player->skill;
    player->cputargetx=-1.0;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x7f8689ff;
        player->tileid=0x4b;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x5c;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x7c;
      } break;
  }
}

/* New.
 */
 
static int _pilfering_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  memset(BATTLE->surfacetilev,0x6e,sizeof(BATTLE->surfacetilev));
  BATTLE->farmerx=142; // Deliberately off-center, aiming for a gap between vegetables.
  BATTLE->farmery=18;
  BATTLE->explodex=-1.0;
  BATTLE->explodey=-1.0;
  
  /* Generate vegetables along the surface.
   * Alternate carrots and bombs -- bombs are vegetables too, but hazardous.
   */
  int vegc=11;
  if (vegc>VEG_LIMIT) return -1;
  int spacing=FBW/vegc;
  int x=(FBW>>1)-((spacing*vegc)>>1)+(spacing>>1);
  while (BATTLE->vegc<vegc) {
    struct veg *veg=BATTLE->vegv+BATTLE->vegc++;
    veg->x=x;
    veg->y=NS_sys_tilesize*1.5+4.0;
    veg->tileid=0x6f;
    x+=spacing;
  }
  int i=0; for (;i<BATTLE->vegc;i+=2) { // Then all the even carrots turn into bombs.
    BATTLE->vegv[i].tileid=0x4f;
  }
  for (i=BATTLE->vegc;i-->0;) if (BATTLE->vegv[i].tileid==0x6f) BATTLE->carrotc++;
  
  return 0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  switch (input&(EGG_BTN_LEFT|EGG_BTN_RIGHT)) {
    case EGG_BTN_LEFT: player->indx=-1; break;
    case EGG_BTN_RIGHT: player->indx=1; break;
    default: player->indx=0; break;
  }
  switch (input&(EGG_BTN_UP|EGG_BTN_DOWN)) {
    case EGG_BTN_UP: player->indy=-1; break;
    case EGG_BTN_DOWN: player->indy=1; break;
    default: player->indy=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  player->indx=player->indy=0;
  
  /* If (cputargetx) unset, descend to (cputhrulevel), then pick a new target.
   * Don't just pick the nearest; that's too predictable.
   * Weight each carrot based on distance. Nearer is likelier, but never certain.
   */
  if (player->cputargetx<0.0) {
    player->cpudx=0;
    if (player->y<player->cputhrulevel) {
      player->indy=1;
    } else {
      struct candidate {
        struct veg *veg;
        int weight;
      } candidatev[VEG_LIMIT];
      int candidatec=0;
      int wsum=0;
      struct veg *veg=BATTLE->vegv;
      int i=BATTLE->vegc;
      for (;i-->0;veg++) {
        if (veg->tileid!=0x6f) continue; // Only carrots matter.
        int dx=(int)veg->x-(int)player->x;
        if (dx<0) dx=-dx;
        if (dx>=FBW) continue; // Impossibly far away; don't touch it.
        struct candidate *candidate=candidatev+candidatec++;
        candidate->veg=veg;
        candidate->weight=FBW-dx;
        wsum+=candidate->weight;
      }
      struct veg *best=0;
      if (wsum>0) {
        int choice=rand()%wsum;
        struct candidate *candidate=candidatev;
        for (i=candidatec;i-->0;candidate++) {
          if ((choice-=candidate->weight)<0) {
            best=candidate->veg;
            break;
          }
        }
      }
      if (best) {
        player->cputargetx=best->x;
        if (player->cputargetx<player->x) player->cpudx=-1;
        else player->cpudx=1;
      }
    }
    return;
  }
  
  /* If (cpudx) unset, climb up.
   * When we grab the carrot, (cputargetx) gets unset.
   * If we miss it, let nature take its course.
   */
  if (!player->cpudx) {
    player->indy=-1;
    return;
  }
  
  /* Move horizontally until we cross (cputargetx).
   */
  if (player->cpudx<0) {
    if (player->x<=player->cputargetx) player->cpudx=0;
    else player->indx=player->cpudx;
  } else {
    if (player->x>=player->cputargetx) player->cpudx=0;
    else player->indx=player->cpudx;
  }
}

/* Mark a player as breached.
 */
 
static void pilfering_breach(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->breach=1;
  player->y=26.0;
  int col=(int)player->x/NS_sys_tilesize;
  if ((col>=0)&&(col<NS_sys_mapw)) {
    player->x=col*NS_sys_tilesize+NS_sys_tilesize*0.5;
    BATTLE->surfacetilev[col]=0x8d;
    if (col>0) BATTLE->surfacetilev[col-1]=0x8c;
    if (col<NS_sys_mapw-1) BATTLE->surfacetilev[col+1]=0x8e;
  }
  if (player->x<FBW*0.5) { // Farmer on the right.
    BATTLE->farmerx=player->x+16.0;
    BATTLE->farmerxform=EGG_XFORM_XREV;
  } else { // Farmer on the left.
    BATTLE->farmerx=player->x-16.0;
    BATTLE->farmerxform=0;
  }
  BATTLE->farmerwhack=1;
  BATTLE->farmerclock=0.200;
}

/* Explode a bomb.
 * Does not remove the bomb from the veg list.
 */
 
static void pilfering_explode(struct battle *battle,struct player *player,struct veg *veg) {
  player->blowedup=1;
  bm_sound_pan(RID_sound_bombblow,player->who?PLAYER_PAN:-PLAYER_PAN);
  BATTLE->explodex=veg->x;
  BATTLE->explodey=veg->y;
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (battle->outcome>-2) return;
  if (player->breach) return;
  if (player->blowedup) return;
  
  /* Walking runs one axis at a time.
   * Horizontal takes precedence over vertical.
   */
  int walkdx=0,walkdy=0;
  if (player->indx) walkdx=player->indx;
  else if (player->indy) walkdy=player->indy;
  if (walkdx||walkdy) {
  
    // Move and clamp.
    player->x+=player->walkspeed*elapsed*walkdx;
    player->y+=player->walkspeed*elapsed*walkdy;
    const double margin=8.0;
    if (player->x<margin) player->x=margin;
    else if (player->x>FBW-margin) player->x=FBW-margin;
    if (player->y<margin) player->y=margin; // Not reachable but let's be consistent.
    else if (player->y>FBH-margin) player->y=FBH-margin;
    
    // Set xform.
    if (walkdy&&(player->xform&EGG_XFORM_SWAP)) { // Already xformed to the vertical axis, don't change my foot direction.
      if (walkdy<0) player->xform|=EGG_XFORM_XREV;
      else player->xform&=~EGG_XFORM_XREV;
    } else if (walkdy) { // Switching to vertical, choose the orientation that makes a 90 degree rotation.
      if (player->xform==0) {
        if (walkdy<0) player->xform=EGG_XFORM_SWAP|EGG_XFORM_XREV;
        else player->xform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
      } else {
        if (walkdy<0) player->xform=EGG_XFORM_SWAP|EGG_XFORM_YREV|EGG_XFORM_XREV;
        else player->xform=EGG_XFORM_SWAP;
      }
    } else { // Horizontal, a bit simpler. The feet always point down.
      if (walkdx>0) player->xform=0;
      else player->xform=EGG_XFORM_XREV;
    }
    
    // Animate.
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.150;
      if (++(player->animframe)>=4) player->animframe=0;
    }
    
    // Get shovel-whacked if you breach the surface.
    if (player->y<=24.0) {
      pilfering_breach(battle,player);
      return;
    }
    
    // Pilfer. We depend on the vegetables all being at the same vertical position.
    if (player->y<=40.0) {
      int i=BATTLE->vegc;
      struct veg *veg=BATTLE->vegv+i-1;
      for (;i-->0;veg--) {
        double dx=veg->x-player->x;
        if ((dx>=-12.0)&&(dx<=12.0)) {
          if (veg->tileid==0x4f) { // bomb
            pilfering_explode(battle,player,veg);
          } else { // carrot
            player->score++;
            player->cputargetx=-1.0;
            bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
          }
          BATTLE->vegc--;
          memmove(veg,veg+1,sizeof(struct veg)*(BATTLE->vegc-i));
          break;
        }
      }
    }
    
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
}

/* Update.
 */
 
static void _pilfering_update(struct battle *battle,double elapsed) {
  
  if ((BATTLE->farmerclock-=elapsed)<=0.0) {
    if (BATTLE->farmerwhack) {
      BATTLE->farmerclock+=0.300;
    } else {
      BATTLE->farmerclock+=1.500;
    }
    if (++(BATTLE->farmerframe)>=2) BATTLE->farmerframe=0;
  }
  
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
    if (player->breach||player->blowedup) break; // Game-over conditions; don't update the next guy.
  }
  
  /* Game ends when a player is breached or exploded, or when every carrot is collected.
   * It shouldn't be possible for both players to disqualify, so I won't accomodate the possibility.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->breach||l->blowedup||r->breach||r->blowedup||(l->score+r->score>=BATTLE->carrotc)) {
    if (l->breach||l->blowedup) battle->outcome=-1;
    else if (r->breach||r->blowedup) battle->outcome=1;
    else if (l->score>r->score) battle->outcome=1;
    else if (l->score<r->score) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=(int)player->y;
  uint8_t tileid=player->tileid;
  uint8_t xform=player->xform;
  if (player->breach||player->blowedup) {
    tileid+=3;
    if (player->breach) xform=0;
  } else if (player->indx||player->indy) {
    switch (player->animframe) {
      case 1: tileid+=1; break;
      case 3: tileid+=2; break;
    }
  }
  graf_tile(g_graf,x,y,tileid,xform);
}

/* Render.
 */
 
static void _pilfering_render(struct battle *battle) {

  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_set_image(g_graf,RID_image_cave_sprites);
  
  // Layer of sky and grass on top.
  int x=NS_sys_tilesize>>1,y0=NS_sys_tilesize>>1;
  int y1=y0+NS_sys_tilesize;
  const uint8_t *surfacetile=BATTLE->surfacetilev;
  for (;x<FBW;x+=NS_sys_tilesize,surfacetile++) {
    graf_tile(g_graf,x,y0,0x8f,0);
    graf_tile(g_graf,x,y1,*surfacetile,0);
  }
  
  struct veg *veg=BATTLE->vegv;
  int i=BATTLE->vegc;
  for (;i-->0;veg++) {
    graf_tile(g_graf,(int)veg->x,(int)veg->y,veg->tileid,0);
  }
  
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  if (BATTLE->farmerwhack) {
    uint8_t farmertile=0x88;
    if (BATTLE->farmerframe) farmertile+=2;
    int x0=BATTLE->farmerx;
    int x1=x0;
    if (BATTLE->farmerxform) x1-=NS_sys_tilesize;
    else x1+=NS_sys_tilesize;
    graf_tile(g_graf,x0,BATTLE->farmery,farmertile,BATTLE->farmerxform);
    graf_tile(g_graf,x1,BATTLE->farmery,farmertile+1,BATTLE->farmerxform);
  } else {
    uint8_t farmertile=0x86;
    if (BATTLE->farmerframe) farmertile+=1;
    graf_tile(g_graf,BATTLE->farmerx,BATTLE->farmery,farmertile,BATTLE->farmerxform);
  }
  
  {
    int x=10,y=10,i=0;
    struct player *player=BATTLE->playerv+0;
    for (;i<player->score;i++,x+=12) graf_tile(g_graf,x,y,0x6f,0);
  }
  {
    int x=FBW-10,y=10,i=0;
    struct player *player=BATTLE->playerv+1;
    for (;i<player->score;i++,x-=12) graf_tile(g_graf,x,y,0x6f,0);
  }
  
  if (BATTLE->explodey>=0.0) {
    int dstx=(int)BATTLE->explodex-16;
    int dsty=(int)BATTLE->explodey-16;
    graf_set_image(g_graf,RID_image_hero);
    graf_decal(g_graf,dstx,dsty,160,160,32,32);
    graf_decal(g_graf,dstx,dsty,192,160,32,32);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_pilfering={
  .name="pilfering",
  .objlen=sizeof(struct battle_pilfering),
  .id=127,
  .strix_name=417,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_dpad,
  .imageid_default=RID_image_caves,
  .del=_pilfering_del,
  .init=_pilfering_init,
  .update=_pilfering_update,
  .render=_pilfering_render,
};

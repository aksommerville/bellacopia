/* battle_cinematography.c
 */

#include "game/bellacopia.h"

#define BAT_LIMIT 2
#define SAMPLE_LIMIT 192
#define SAMPLE_INTERVAL 0.050 /* Aim for 20 Hz like an old-timey movie. Should be about every 3rd game frame. */
#define FILMW 150
#define FILMH 100

struct battle_cinematography {
  struct battle hdr;
  
  int replay; // Once nonzero, we're showing the sampled frames, game is basically over.
  int replay_samplep;
  double replayclock;
  double bgdim; // 0..1
  int replay_extrac; // Count of renders since the frame changed, so we can make it flicker.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    double x,y; // Center of camera, in framebuffer pixels.
    double z; // Zoom, 0..1, higher is closer.
    
    int indx,indy,inzoom; // -1,0,1
    int blackout;
    
    int filmtexid;
    double score; // 0..samplec, counts up during replay
    int popc; // How many popped kernels have we rendered, to know whether to make a sound effect. Render makes the sound.
    double penalty; // Score multiplier.
    
    double cpuhold;
    double cpuholdlo,cpuholdhi;
  } playerv[2];
  
  struct bat {
    double x,y; // Final framebuffer position.
    double dx,dy;
    double prex,prey; // Framebuffer position before offset.
    double t; // radians, phase of path offset
    double dt;
    double mag; // pixels, magnitude of path offset
    double animclock;
    int animframe;
    uint8_t xform;
  } batv[BAT_LIMIT];
  int batc;
  
  struct sample {
    double lx,ly,lz; // Left player.
    double rx,ry,rz; // Right player.
    double lbx,lby,lbz; // Left bat.
    double rbx,rby,rbz; // Right bat.
    uint8_t lbtile,lbxform;
    uint8_t rbtile,rbxform;
  } samplev[SAMPLE_LIMIT];
  int samplec;
  double sampleclock;
};

#define BATTLE ((struct battle_cinematography*)battle)

/* Delete.
 */
 
static void _cinematography_del(struct battle *battle) {
  struct player *player=BATTLE->playerv;
  int i=2; for (;i-->0;player++) {
    egg_texture_del(player->filmtexid);
  }
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,struct bat *bat,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->x=FBW*0.200;
  } else { // Right.
    player->who=1;
    player->x=FBW*0.800;
  }
  player->y=75.0;
  player->z=0.500;
  if (player->human=human) { // Human.
    player->blackout=1;
    player->penalty=1.000;
  } else { // CPU.
    player->penalty=0.700;
    player->cpuholdhi=0.300*(1.0-player->skill)+0.050*player->skill;
    player->cpuholdlo=player->cpuholdhi*0.5;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x000000ff;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
      } break;
  }
  player->filmtexid=egg_texture_new();
  egg_texture_load_raw(player->filmtexid,FILMW,FILMH,FILMW<<2,0,0);
  
  bat->x=bat->prex=player->who?(FBW+20.0):-20.0;
  bat->y=bat->prey=FBH*0.75;
  bat->dx=40.0;
  bat->dy=-10.0;
  if (player->who) {
    bat->dx*=-1.0;
    bat->xform=EGG_XFORM_XREV;
  }
  bat->t=((rand()&0xffff)*M_PI*2.0)/65535.0;
  bat->dt=8.000*(1.0-player->skill)+1.000*player->skill;
  bat->mag=20.0*(1.0-player->skill)+10.0*player->skill;
  bat->animclock=((rand()&0xffff)*0.100)/65535.0;
  bat->animframe=rand()%10;
}

/* New.
 */
 
static int _cinematography_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,BATTLE->batv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,BATTLE->batv+1,battle->args.rctl,battle->args.rface);
  BATTLE->batc=2;
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
  if (player->blackout) {
    if (!(input&(EGG_BTN_SOUTH|EGG_BTN_WEST))) player->blackout=0;
  } else switch (input&(EGG_BTN_SOUTH|EGG_BTN_WEST)) {
    case EGG_BTN_SOUTH: player->inzoom=1; break;
    case EGG_BTN_WEST: player->inzoom=-1; break;
    default: player->inzoom=0; break;
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if ((player->cpuhold-=elapsed)>0.0) return;
  if ((player->who<0)||(player->who>=BATTLE->batc)) {
    player->indx=0;
    player->indy=0;
    player->inzoom=0;
    player->cpuhold=999.999;
    return;
  }
  int ndx=player->indx;
  int ndy=player->indy;
  int nz=player->inzoom;
  struct bat *bat=BATTLE->batv+player->who;
  if (bat->x<player->x) ndx=-1; else if (bat->x>player->x) ndx=1; else ndx=0;
  if (bat->y<player->y) ndy=-1; else if (bat->y>player->y) ndy=1; else ndy=0;
  
  // If state changed, set a hold.
  if ((ndx!=player->indx)||(ndy!=player->indy)||(nz!=player->inzoom)) {
    player->indx=ndx;
    player->indy=ndy;
    player->inzoom=nz;
    player->cpuhold=(rand()&0xffff)/65535.0;
    player->cpuhold=player->cpuholdlo*(1.0-player->cpuhold)+player->cpuholdhi*player->cpuhold;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Motion and zoom per inputs.
   */
  player->x+=80.0*player->indx*elapsed;
  player->y+=80.0*player->indy*elapsed;
  player->z+=1.000*player->inzoom*elapsed;
  if (player->x<0.0) player->x=0.0; else if (player->x>320.0) player->x=320.0;
  if (player->y<0.0) player->y=0.0; else if (player->y>180.0) player->y=180.0;
  if (player->z<0.0) player->z=0.0; else if (player->z>1.0) player->z=1.0;
}

/* Update bat.
 */
 
static void bat_update(struct battle *battle,struct bat *bat,double elapsed) {

  // Animation.
  if ((bat->animclock-=elapsed)<=0.0) {
    bat->animclock+=0.080;
    if (++(bat->animframe)>=10) bat->animframe=0;
  }
  
  // Motion.
  bat->prex+=bat->dx*elapsed;
  bat->prey+=bat->dy*elapsed;
  bat->t+=bat->dt*elapsed;
  // Offset is applied directly to the vertical. We could do it right by turning (dx,dy) a quarter turn and offsetting along that, but does it matter?
  double off=sin(bat->t)*bat->mag;
  bat->x=bat->prex;
  bat->y=bat->prey+off;
}

/* Produce players' film textures for (replay_samplep).
 */
 
static void bat_print_film(struct battle *battle,double x,double y,double z,double bx,double by,uint8_t btile,uint8_t bxform) {
  int srcw=NS_sys_tilesize<<1;
  int srcx=NS_sys_tilesize*10;
  if (btile>5) btile=10-btile;
  int srcy=NS_sys_tilesize*3+srcw*btile;
  int xr=lround(18.0+(1.0-z)*18.0);
  int yr=lround(12.0+(1.0-z)*12.0);
  int dstx=75+(int)((bx-x)*75.0)/xr;
  int dsty=50+(int)((by-y)*50.0)/yr;
  double scale=75.0/(double)xr;
  // oops. graf_decal_rotate() can't flop. So we have the flopped images next to the regular ones in the tilesheet.
  if (bxform) srcx+=NS_sys_tilesize<<1;
  graf_decal_rotate(&g.graf,dstx,dsty,srcx,srcy,srcw,0.0,1.0,scale);
}
 
static void player_print_film(struct battle *battle,struct player *player,struct sample *sample) {
  graf_set_output(&g.graf,player->filmtexid);
  egg_texture_clear(player->filmtexid);
  double x,y,z;
  if (player->who) {
    x=sample->rx;
    y=sample->ry;
    z=sample->rz;
  } else {
    x=sample->lx;
    y=sample->ly;
    z=sample->lz;
  }
  graf_set_image(&g.graf,RID_image_battle_underground);
  graf_set_filter(&g.graf,1);
  bat_print_film(battle,x,y,z,sample->lbx,sample->lby,sample->lbtile,sample->lbxform);
  bat_print_film(battle,x,y,z,sample->rbx,sample->rby,sample->rbtile,sample->rbxform);
  graf_set_filter(&g.graf,0);
  graf_set_output(&g.graf,1);
  
  /* Update the score.
   * Add up to 1.0 at each frame.
   * Take the bat nearer our focus point, its distance divided by focus width.
   */
  double ldx=sample->lbx-x;
  double ldy=sample->lby-y;
  double ld2=ldx*ldx+ldy*ldy;
  double rdx=sample->rbx-x;
  double rdy=sample->rby-y;
  double rd2=rdx*rdx+rdy*rdy;
  if (rd2<ld2) {
    ldx=rdx;
    ldy=rdy;
    ld2=rd2;
  }
  double xr=18.0+(1.0-z)*18.0;
  double xr2=xr*xr;
  if (ld2<xr*xr) {
    player->score+=((xr2-ld2)/xr2)*player->penalty;
  }
}
 
static void cinematography_print_film(struct battle *battle) {
  BATTLE->replay_extrac=0;
  struct sample *sample=BATTLE->samplev+BATTLE->replay_samplep;
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  player_print_film(battle,l,sample);
  player_print_film(battle,r,sample);
}

/* Declare winner.
 */
 
static void cinematography_declare_winner(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (l->score>r->score) battle->outcome=1;
  else if (l->score<r->score) battle->outcome=-1;
  else battle->outcome=0;
}

/* Update.
 */
 
static void _cinematography_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  // If replaying, just pay that out, and declare outcome at the end.
  if (BATTLE->replay) {
    if ((BATTLE->bgdim+=elapsed)>=1.0) BATTLE->bgdim=1.0;
    if ((BATTLE->replayclock-=elapsed)<=0.0) {
      BATTLE->replayclock+=SAMPLE_INTERVAL;
      if (++(BATTLE->replay_samplep)>=BATTLE->samplec) {
        cinematography_declare_winner(battle);
      } else {
        cinematography_print_film(battle);
      }
    }
    return;
  }
  
  // Update players.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  // Update bats.
  struct bat *bat=BATTLE->batv;
  for (i=BATTLE->batc;i-->0;bat++) {
    bat_update(battle,bat,elapsed);
  }
  
  // Sample the scene on a coarser-than-frame timer.
  if (BATTLE->samplec>=SAMPLE_LIMIT) {
    BATTLE->replay=1;
    cinematography_print_film(battle);
  } else if ((BATTLE->sampleclock-=elapsed)<=0.0) {
    BATTLE->sampleclock+=SAMPLE_INTERVAL;
    struct sample *sample=BATTLE->samplev+BATTLE->samplec++;
    struct player *lp=BATTLE->playerv;
    struct player *rp=lp+1;
    struct bat *lb=BATTLE->batv;
    struct bat *rb=lb+1;
    sample->lx=lp->x;
    sample->ly=lp->y;
    sample->lz=lp->z;
    sample->rx=rp->x;
    sample->ry=rp->y;
    sample->rz=rp->z;
    sample->lbx=lb->x;
    sample->lby=lb->y;
    sample->lbtile=lb->animframe;
    sample->lbxform=lb->xform;
    sample->rbx=rb->x;
    sample->rby=rb->y;
    sample->rbtile=rb->animframe;
    sample->rbxform=rb->xform;
  }

  //XXX
  if (g.input[0]&EGG_BTN_AUX2) battle->outcome=1;
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int dstx=lround(player->x);
  int dsty=lround(player->y);
  int xr=lround(18.0+(1.0-player->z)*18.0);
  int yr=lround(12.0+(1.0-player->z)*12.0);
  graf_set_image(&g.graf,RID_image_battle_underground);
  graf_fancy(&g.graf,dstx,dsty,0x0e,0,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,dstx-xr,dsty-yr,0x0c,0,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,dstx+xr,dsty-yr,0x0c,EGG_XFORM_XREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,dstx-xr,dsty+yr,0x0c,EGG_XFORM_YREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,dstx+xr,dsty+yr,0x0c,EGG_XFORM_XREV|EGG_XFORM_YREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,dstx,dsty-yr,0x0d,0,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,dstx,dsty+yr,0x0d,EGG_XFORM_YREV,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,dstx-xr,dsty,0x0d,EGG_XFORM_SWAP,0,NS_sys_tilesize,0,player->color);
  graf_fancy(&g.graf,dstx+xr,dsty,0x0d,EGG_XFORM_SWAP|EGG_XFORM_YREV,0,NS_sys_tilesize,0,player->color);
}

/* Render bat.
 */
 
static void bat_render(struct battle *battle,struct bat *bat) {
  int frame=0;
  switch (bat->animframe) {
    case 1: frame=1; break;
    case 2: frame=2; break;
    case 3: frame=3; break;
    case 4: frame=4; break;
    case 5: frame=5; break;
    case 6: frame=4; break;
    case 7: frame=3; break;
    case 8: frame=2; break;
    case 9: frame=1; break;
  }
  int w=NS_sys_tilesize<<1;
  int h=NS_sys_tilesize<<1;
  int srcx=NS_sys_tilesize*14;
  int srcy=NS_sys_tilesize+h*frame;
  int dstx=(int)bat->x-NS_sys_tilesize;
  int dsty=(int)bat->y-NS_sys_tilesize;
  graf_decal_xform(&g.graf,dstx,dsty,srcx,srcy,w,h,bat->xform);
}

/* Render player for replay.
 */
 
static void replay_render(struct battle *battle,struct player *player) {
  int dsty=(FBH>>1)-(FILMH>>1);
  int dstx=player->who?((FBW>>1)+5):((FBW>>1)-5-FILMW);
  graf_fill_rect(&g.graf,dstx,dsty,FILMW,FILMH,0x816d57ff);
  graf_set_input(&g.graf,player->filmtexid);
  graf_decal(&g.graf,dstx,dsty,0,0,FILMW,FILMH);
  /* Flicker. TODO Maybe don't? It doesn't look that great so far, and it's a potential epilepsy hazard.
  switch (BATTLE->replay_extrac) {
    case 1: break;
    case 2: graf_fill_rect(&g.graf,dstx,dsty,FILMW,FILMH,0x00000008); break;
    case 3: graf_fill_rect(&g.graf,dstx,dsty,FILMW,FILMH,0x0000000c); break;
    default:graf_fill_rect(&g.graf,dstx,dsty,FILMW,FILMH,0x00000010); break;
  }
  /**/
  if (BATTLE->replay_samplep&1) {
    graf_fill_rect(&g.graf,dstx,dsty,FILMW,FILMH,0x00000010);
  }
}

/* Render score bar.
 */
 
static void popmeter_render(struct battle *battle,int x,struct player *player) {
  if (!BATTLE->samplec) return;
  int kernc=10;
  int yspacing=12;
  int fullh=kernc*yspacing;
  int y=(FBH>>1)+(fullh>>1)-(yspacing>>1);
  int popc=lround((player->score*kernc)/BATTLE->samplec);
  if (popc>player->popc) {
    bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
    player->popc=popc;
  }
  int i=0;
  for (;i<kernc;i++,y-=yspacing) {
    uint8_t tileid=0x1c,xform=0;
    if (i<popc) {
      tileid+=1;
      xform=(i+x)&7;
    }
    graf_tile(&g.graf,x,y,tileid,xform);
  }
}

/* Render.
 */
 
static void _cinematography_render(struct battle *battle) {

  // Background.
  const int groundy=150;
  graf_fill_rect(&g.graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(&g.graf,0,groundy,FBW,FBH-groundy,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(&g.graf,0,groundy,FBW,1,0x000000ff);
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  // If replaying, show the two silver screens.
  if (BATTLE->replay) {
    BATTLE->replay_extrac++;
    int bgalpha=(int)(BATTLE->bgdim*200.0);
    if (bgalpha>0) graf_fill_rect(&g.graf,0,0,FBW,FBH,0x00000000|bgalpha);
    replay_render(battle,l);
    replay_render(battle,r);
    graf_set_image(&g.graf,RID_image_battle_underground);
    popmeter_render(battle,(FBW>>1)-6,l);
    popmeter_render(battle,(FBW>>1)+6,r);
    return;
  }
  
  // Bats.
  graf_set_image(&g.graf,RID_image_battle_underground);
  struct bat *bat=BATTLE->batv;
  int i=BATTLE->batc;
  for (;i-->0;bat++) bat_render(battle,bat);
  
  // Viewfinders.
  player_render(battle,l);
  player_render(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_cinematography={
  .name="cinematography",
  .objlen=sizeof(struct battle_cinematography),
  .id=NS_battle_cinematography,
  .strix_name=310,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad_ab,
  .imageid_default=RID_image_caves,
  .del=_cinematography_del,
  .init=_cinematography_init,
  .update=_cinematography_update,
  .render=_cinematography_render,
};

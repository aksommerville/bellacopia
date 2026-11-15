/* battle_scaring.c
 */

#include "game/batsup/battle_internal.h"

int store_get_fld(int fldid);

#define GROUNDY 120
#define BARRELC 7

struct battle_scaring {
  struct battle hdr;
  
  struct barrel {
    int x,y; // Center of top-left tile (2x2).
    uint8_t tileid; // 0x90=normal, 0xb0=hatted
  } barrelv[BARRELC];
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid; // 2x3, 3 frames arrange horizontally.
    uint8_t xform;
    int blackout;
    struct barrel *barrel;
    int jumped;
    double animclock;
    int animframe;
    double mandx;
    double manx;
    double mananimclock;
    int mananimframe;
    double scare; // 0..1, if exactly zero he's not scared.
    double vscare; // Counts up to (scare) for animation.
    int mangone; // If we failed to scare him, set this when he leaves the screen.
    double radius;
    double cputhresh; // Horizontal pixels. Jump when the man crosses this.
  } playerv[2];
};

#define BATTLE ((struct battle_scaring*)battle)

/* Delete.
 */
 
static void _scaring_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->manx=-20.0;
    player->mandx=1.0;
    player->xform=EGG_XFORM_XREV;
  } else { // Right.
    player->who=1;
    player->manx=FBW+20.0;
    player->mandx=-1.0;
  }
  
  player->mandx*=90.0*(1.0-player->skill)+40.0*player->skill;
  player->mandx+=((rand()&0xffff)*10.0)/65535.0-5.0; // Randomize man speed just a little to discourage alignment.
  player->radius=20.0*(1.0-player->skill)+40.0*player->skill;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    // We can't set (cputhresh) yet because our barrel isn't assigned.
    // Technically, we're not supposed to know our horizontal position yet.
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xe7eedfff;
        player->tileid=0xc2;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x92;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x98;
      } break;
  }
}

static void scaring_calculate_threshold(struct battle *battle,struct player *player) {
  if (player->human) return;
  player->cputhresh=player->barrel->x;
  if (player->xform&EGG_XFORM_XREV) {
    player->cputhresh-=NS_sys_tilesize; // Optimal...
    player->cputhresh-=20.0*(1.0-player->skill)+15.0*player->skill;
  } else {
    player->cputhresh+=NS_sys_tilesize*2.0; // Optimal...
    player->cputhresh+=20.0*(1.0-player->skill)+15.0*player->skill;
  }
  // Then randomize by up to 10 pixels in either direction.
  // At minimum skill, we were right on radius, so there's a roughly 1/2 chance that he'll jump the gun.
  player->cputhresh+=((rand()&0xffff)*20.0)/65535.0-10.0;
}

/* New.
 */
 
static int _scaring_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  
  /* Place barrels uniformly along the horizon.
   */
  int spacing=FBW/BARRELC;
  int x=(spacing>>1)-(NS_sys_tilesize>>1);
  struct barrel *barrel=BATTLE->barrelv;
  int i=BARRELC;
  for (;i-->0;barrel++,x+=spacing) {
    barrel->x=x;
    barrel->y=GROUNDY-NS_sys_tilesize-(NS_sys_tilesize>>1)+4;
    barrel->tileid=0x90;
  }
  
  /* Assign a barrel to each player.
   */
  l->barrel=BATTLE->barrelv+(BARRELC>>1)-1;
  r->barrel=BATTLE->barrelv+(BARRELC>>1)+1;
  
  /* If they hatted at least one barrel in story mode, hat one of these at random.
   * Strictly speaking, we ought to skip this if (battle->args.no_store), but relax, it's read-only and purely decorative.
   * 18..26 = NS_fld_barrelhat1..NS_fld_barrelhat2; battles aren't supposed to read shared_symbols.h.
   */
  int fldid=18,hatted=0;
  for (;fldid<=26;fldid++) {
    if (store_get_fld(fldid)) {
      hatted=1;
      break;
    }
  }
  if (hatted) {
    barrel=BATTLE->barrelv+(rand()%BARRELC);
    barrel->tileid=0xb0;
  }
  
  /* (player->cputhresh) was left unset initially because we need the barrel positions first.
   * Fill that in now.
   */
  scaring_calculate_threshold(battle,l);
  scaring_calculate_threshold(battle,r);
  
  return 0;
}

/* Jump.
 */
 
static void scaring_jump(struct battle *battle,struct player *player) {
  if (battle->outcome>-2) return;
  if (player->jumped) return;
  bm_sound_pan(RID_sound_boo,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->jumped=1;
  
  /* The barrel is two tiles wide and player overlaps the far column.
   * Man is also two tiles wide. The ideal scare position is when he's overlapping the near column.
   */
  double targetx=(double)player->barrel->x;
  if (player->xform&EGG_XFORM_XREV) {
    targetx-=NS_sys_tilesize;
  } else {
    targetx+=NS_sys_tilesize*2.0;
  }
  double distance=player->manx-targetx;
  if (distance<0.0) distance=-distance;
  if (distance<player->radius) {
    player->scare=1.0-distance/player->radius;
    if (player->scare<=0.0) player->scare=0.001;
    else if (player->scare>1.0) player->scare=1.0;
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->jumped) return;
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    if (input&EGG_BTN_SOUTH) scaring_jump(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->jumped) return;
  if (player->mandx>0.0) {
    if (player->manx>=player->cputhresh) scaring_jump(battle,player);
  } else {
    if (player->manx<=player->cputhresh) scaring_jump(battle,player);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->jumped) {
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.200;
      if (++(player->animframe)>=2) player->animframe=0;
    }
    if (player->vscare<player->scare) {
      if ((player->vscare+=0.500*elapsed)>=player->scare) player->vscare=player->scare;
    }
  }
  if (player->scare<=0.0) {
    player->manx+=player->mandx*elapsed;
    if ((player->mananimclock-=elapsed)<=0.0) {
      player->mananimclock+=0.150;
      if (++(player->mananimframe)>=4) player->mananimframe=0;
    }
    if ((player->manx<-20.0)||(player->manx>FBW+20.0)) {
      player->mangone=1;
    }
  }
}

/* Update.
 */
 
static void _scaring_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Game ends when both men are either scared or gone.
   * A tie at zero is entirely possible.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (
      (l->mangone||((l->scare>0.0)&&(l->vscare>=l->scare)))&&
      (r->mangone||((r->scare>0.0)&&(r->vscare>=r->scare)))
    ) {
      if (l->scare>r->scare) battle->outcome=1;
      else if (l->scare<r->scare) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void sprite2x3(int x0,int y0,uint8_t tileid,uint8_t xform) {
  int x1=x0,y1=y0+NS_sys_tilesize,y2=y0+NS_sys_tilesize*2;
  if (xform&EGG_XFORM_XREV) { // x0 is Left at entry, but Back when we render; it transforms.
    x0+=NS_sys_tilesize;
  } else {
    x1+=NS_sys_tilesize;
  }
  graf_tile(g_graf,x0,y0,tileid+0x00,xform);
  graf_tile(g_graf,x1,y0,tileid+0x01,xform);
  graf_tile(g_graf,x0,y1,tileid+0x10,xform);
  graf_tile(g_graf,x1,y1,tileid+0x11,xform);
  graf_tile(g_graf,x0,y2,tileid+0x20,xform);
  graf_tile(g_graf,x1,y2,tileid+0x21,xform);
}
 
static void player_render(struct battle *battle,struct player *player) {
  int x,y;
  if (player->xform) { // Facing left. Stand by the barrel's right side.
    x=player->barrel->x+NS_sys_tilesize;
  } else { // Facing right. Stand by the barrel's left side.
    x=player->barrel->x-NS_sys_tilesize;
  }
  y=player->barrel->y-NS_sys_tilesize;
  uint8_t tileid=player->tileid;
  if (player->jumped) {
    tileid+=2;
    if (player->animframe) tileid+=2;
    y+=2;
  } else {
    y-=2;
  }
  sprite2x3(x,y,tileid,player->xform);
}

static void scaring_render_man(struct battle *battle,struct player *player) {
  int x=(int)player->manx-(NS_sys_tilesize>>1);
  uint8_t tileid=0xc8;
  if (player->scare>0.0) {
    tileid+=6;
  } else switch (player->mananimframe) {
    case 1: tileid+=2; break;
    case 3: tileid+=4; break;
  }
  uint8_t xform=(player->xform&EGG_XFORM_XREV)?0:EGG_XFORM_XREV;
  int y=GROUNDY-NS_sys_tilesize*2;
  sprite2x3(x,y,tileid,xform);
}

/* Scare meter, after committed.
 */
 
static void scaring_render_meter(struct battle *battle,struct player *player) {
  if (player->scare<=0.0) return;
  int barw=4;
  int barx=(int)player->manx-(barw>>1);
  int barh=70;
  int bary=GROUNDY-35-barh;
  graf_fill_rect(g_graf,barx-1,bary-1,barw+2,barh+2,0x000000ff);
  int fillh=(int)(player->vscare*barh);
  if (fillh>0) {
    if (fillh>barh) fillh=barh;
    graf_fill_rect(g_graf,barx,bary+barh-fillh,barw,fillh,player->color);
  }
}

/* Render.
 */
 
static void _scaring_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);
  graf_set_image(g_graf,RID_image_cave_sprites);
  
  /* Players before barrels if they haven't jumped yet.
   */
  if (!l->jumped) player_render(battle,l);
  if (!r->jumped) player_render(battle,r);
  
  struct barrel *barrel=BATTLE->barrelv;
  int i=BARRELC;
  for (;i-->0;barrel++) {
    int x0=barrel->x,y0=barrel->y;
    int x1=x0+NS_sys_tilesize,y1=y0+NS_sys_tilesize;
    graf_tile(g_graf,x0,y0,barrel->tileid+0x00,0);
    graf_tile(g_graf,x1,y0,barrel->tileid+0x01,0);
    graf_tile(g_graf,x0,y1,barrel->tileid+0x10,0);
    graf_tile(g_graf,x1,y1,barrel->tileid+0x11,0);
  }
  
  /* Players after barrels if they have jumped.
   */
  if (l->jumped) player_render(battle,l);
  if (r->jumped) player_render(battle,r);
  
  /* And regardless of the player's z position, the man is on top.
   */
  if (!l->mangone) scaring_render_man(battle,l);
  if (!r->mangone) scaring_render_man(battle,r);
  
  scaring_render_meter(battle,l);
  scaring_render_meter(battle,r);
}

/* Type definition.
 */
 
const struct battle_type battle_type_scaring={
  .name="scaring",
  .objlen=sizeof(struct battle_scaring),
  .id=129,
  .strix_name=421,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_a,
  .imageid_default=RID_image_caves,
  .del=_scaring_del,
  .init=_scaring_init,
  .update=_scaring_update,
  .render=_scaring_render,
};

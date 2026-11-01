/* battle_hopscotch.c
 */

#include "game/batsup/battle_internal.h"

/* Background tiles cover the whole framebuffer,
 * and on top of that, 2 fields of 6 squares, each composed of 9 tiles.
 * There are shared boundaries between the squares that we cover redundantly, whatever.
 * It works out to 348.
 */
#define SQUAREC 6
#define BGVTXC (NS_sys_mapw*NS_sys_maph+9*SQUAREC*2)

struct battle_hopscotch {
  struct battle hdr;
  double stopclock; // After one player completes, give other a couple seconds.
  int winner; // -2,-1,0,1 just like outcome, but not committed yet.
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    double quality_limit; // s, how long for a hop to reach minimum quality.
    double ratelo,ratehi; // hz, hop rate at the quality thresholds.
    double cpuwaitlo,cpuwaithi;
    uint16_t btnidv[SQUAREC];
    int position; // Index in (btnidv) of the next step (can go one oob).
    double x,y; // Framebuffer pixels, center of 2x2-tile sprite.
    double ybase;
    double xv[SQUAREC+1]; // Horizontal positions, including the oob end but not the oob start. [3,4] should be identical.
    double startclock;
    int input,pvinput;
    int finished;
    double stillclock;
    double cpuwait;
    
    // Rather than operating incrementally on (x,y), we track progress along the hop's arc.
    int hopping;
    double arcax,arczx;
    double arcp;
    double arcrate; // hz, ie advancement of (arcp).
  } playerv[2];
  
  struct egg_render_tile bgvtxv[BGVTXC];
};

#define BATTLE ((struct battle_hopscotch*)battle)

/* Delete.
 */
 
static void _hopscotch_del(struct battle *battle) {
}

/* Init player.
 */
 
static uint16_t random_button() {
  switch (rand()%6) {
    case 0: return EGG_BTN_SOUTH; break;
    case 1: return EGG_BTN_WEST; break;
    case 2: return EGG_BTN_UP; break;
    case 3: return EGG_BTN_DOWN; break;
    case 4: return EGG_BTN_LEFT; break;
    case 5: return EGG_BTN_RIGHT; break;
  }
  return EGG_BTN_SOUTH;
}
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  
  player->startclock=0.250+((rand()&0xffff)*0.250)/65535.0;
  player->pvinput=0xffff; // blackout
  player->quality_limit=0.180*(1.0-player->skill)+0.200*player->skill;
  player->ratehi=1.500*(1.0-player->skill)+2.000*player->skill;
  player->ratelo=player->ratehi*0.5;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->cpuwaithi=0.300*(1.0-player->skill)+0.150*player->skill;
    if ((player->cpuwaitlo=player->cpuwaithi-0.200)<0.0) player->cpuwaitlo=0.0;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x209130ff;
        player->tileid=0xac;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x6c;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x8c;
      } break;
  }
  
  /* Choose the required buttons for each square.
   * Slots [3,4] occur at the same time, so one must be dpad and the other thumb.
   */
  player->btnidv[0]=random_button();
  player->btnidv[1]=random_button();
  player->btnidv[2]=random_button();
  player->btnidv[3]=random_button();
  player->btnidv[5]=random_button();
  if (player->btnidv[3]&(EGG_BTN_SOUTH|EGG_BTN_WEST)) switch (rand()&3) {
    case 0: player->btnidv[4]=EGG_BTN_UP; break;
    case 1: player->btnidv[4]=EGG_BTN_DOWN; break;
    case 2: player->btnidv[4]=EGG_BTN_LEFT; break;
    case 3: player->btnidv[4]=EGG_BTN_RIGHT; break;
  } else switch (rand()&1) {
    case 0: player->btnidv[4]=EGG_BTN_SOUTH; break;
    case 1: player->btnidv[4]=EGG_BTN_WEST; break;
  }
}

/* Generate background tiles for one player's field.
 * Also populates the player's (ybase,xv).
 */
 
static int hopscotch_generate_square_tiles(struct egg_render_tile *vtx,int x,int y,uint16_t btnid) {
  // The tiles are symmetric, so we can swap sloppy, no need to preserve chirality.
  int c=0;
  struct egg_render_tile *focus;
  #define _(dx,dy,tid,xf) { \
    vtx->x=x+(dx*NS_sys_tilesize); \
    vtx->y=y+(dy*NS_sys_tilesize); \
    vtx->tileid=tid; \
    vtx->xform=xf; \
    vtx++; c++; \
  }
  _(-1,-1,0x5a,0)
  _( 0,-1,0x5b,0)
  _( 1,-1,0x5a,EGG_XFORM_XREV)
  _(-1, 0,0x5b,EGG_XFORM_SWAP)
  focus=vtx; _( 0, 0,0x69,0)
  _( 1, 0,0x5b,EGG_XFORM_SWAP)
  _(-1, 1,0x5a,EGG_XFORM_YREV)
  _( 0, 1,0x5b,0)
  _( 1, 1,0x5a,EGG_XFORM_XREV|EGG_XFORM_YREV)
  #undef _
  switch (btnid) {
    case EGG_BTN_LEFT: focus->xform=EGG_XFORM_SWAP; break;
    case EGG_BTN_RIGHT: focus->xform=EGG_XFORM_SWAP|EGG_XFORM_YREV; break;
    case EGG_BTN_UP: break; // Natural arrow orientation.
    case EGG_BTN_DOWN: focus->xform=EGG_XFORM_YREV; break;
    case EGG_BTN_SOUTH: focus->tileid+=1; break;
    case EGG_BTN_WEST: focus->tileid+=2; break;
  }
  return c;
}
 
static int hopscotch_generate_field_tiles(struct egg_render_tile *vtx,struct player *player) {
  int c=0;
  int y=player->who?140:60;
  int squarew=32;
  int x=(FBW>>1)-((squarew*5)>>1)+(squarew>>1);
  player->ybase=y-8;
  player->xv[0]=x; c+=hopscotch_generate_square_tiles(vtx+c,x,y,player->btnidv[0]); x+=squarew;
  player->xv[1]=x; c+=hopscotch_generate_square_tiles(vtx+c,x,y,player->btnidv[1]); x+=squarew;
  player->xv[2]=x; c+=hopscotch_generate_square_tiles(vtx+c,x,y,player->btnidv[2]); x+=squarew;
  player->xv[3]=x; c+=hopscotch_generate_square_tiles(vtx+c,x,y-(squarew>>1),player->btnidv[3]);
  player->xv[4]=x; c+=hopscotch_generate_square_tiles(vtx+c,x,y+(squarew>>1),player->btnidv[4]); x+=squarew;
  player->xv[5]=x; c+=hopscotch_generate_square_tiles(vtx+c,x,y,player->btnidv[5]); x+=squarew;
  player->xv[6]=x;
  return c;
}

/* New.
 */
 
static int _hopscotch_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  BATTLE->winner=-2;
  
  /* Generate random asphalt for the background.
   */
  struct egg_render_tile *bgvtx=BATTLE->bgvtxv;
  int bgvtxc=0;
  int yi=NS_sys_maph,y=NS_sys_tilesize>>1;
  for (;yi-->0;y+=NS_sys_tilesize) {
    int xi=NS_sys_mapw,x=NS_sys_tilesize>>1;
    for (;xi-->0;x+=NS_sys_tilesize,bgvtx++,bgvtxc++) {
      bgvtx->x=x;
      bgvtx->y=y;
      bgvtx->tileid=0x68;
      bgvtx->xform=rand()&7;
    }
  }
  
  /* Then a field for each player.
   */
  int c;
  c=hopscotch_generate_field_tiles(bgvtx,l); bgvtx+=c; bgvtxc+=c;
  c=hopscotch_generate_field_tiles(bgvtx,r); bgvtx+=c; bgvtxc+=c;
  
  // If we didn't land on the expected tile count, we might have just clobbered memory. Get out if we can.
  if (bgvtxc!=BGVTXC) {
    fprintf(stderr,"%s:%d: I messed up the tile count. expect=%d actual=%d\n",__FILE__,__LINE__,BGVTXC,bgvtxc);
    return -1;
  }
  
  /* Set each player's initial position based on some measurements taken during background generation.
   */
  l->x=l->xv[0]-(l->xv[1]-l->xv[0]);
  l->y=l->ybase;
  r->x=r->xv[0]-(r->xv[1]-r->xv[0]);
  r->y=r->ybase;
  
  return 0;
}

/* Begin a hop.
 */
 
static void hopscotch_hop(struct battle *battle,struct player *player) {
  if (player->position>SQUAREC) return; // sic '>'; we're allowed to hop one oob.
  if (player->hopping) return; // whoa now, settle down
  double quality=(player->quality_limit-player->stillclock)/player->quality_limit;
  if (quality<0.0) quality=0.0; else if (quality>1.0) quality=1.0;
  if (!player->position) quality=0.5; // First hop is not interactive. Hit middle quality exactly.
  //fprintf(stderr,"player %d hop to position %d with stillclock %.03f, quality=%.03f\n",player->who,player->position,player->stillclock,quality);
  player->hopping=1;
  bm_sound_pan(RID_sound_jump,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->arcax=player->x;
  player->arczx=player->xv[player->position];
  player->arcp=0.0;
  player->arcrate=player->ratelo*(1.0-quality)+player->ratehi*quality;
}

/* End a hop.
 */
 
static void hopscotch_land(struct battle *battle,struct player *player) {
  player->hopping=0;
  bm_sound_pan(RID_sound_weebump,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->x=player->xv[player->position];
  player->y=player->ybase;
  if (player->position==3) player->position=5;
  else if (player->position<SQUAREC) player->position++;
  else player->finished=1;
  player->stillclock=0.0;
  
  // Update the CPU wait time.
  if (!player->human) {
    double n=(rand()&0xffff)/65535.0;
    player->cpuwait=player->cpuwaitlo*(1.0-n)+player->cpuwaithi*n;
  }
}

/* Faulty keystroke.
 */
 
static void hopscotch_miss(struct battle *battle,struct player *player) {
  bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Key pressed.
 */
 
static void hopscotch_stroke(struct battle *battle,struct player *player,uint16_t btnid) {
  if (player->finished) return;
  if (player->hopping||(player->startclock>0.0)) {
    hopscotch_miss(battle,player);
    return;
  }
  if (player->position<1) return; // Shouldn't be possible.
  uint16_t expect=player->btnidv[player->position-1];
  if (btnid==expect) {
    hopscotch_hop(battle,player);
  } else {
    hopscotch_miss(battle,player);
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  player->input=input;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->hopping) {
    player->input=0;
    return;
  }
  if (player->stillclock<player->cpuwait) {
    player->input=0;
    return;
  }
  if (player->position<1) {
    player->input=0;
    return;
  }
  if (player->position==5) {
    player->input=player->btnidv[3]|player->btnidv[4];
  } else {
    player->input=player->btnidv[player->position-1];
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  /* Check input.
   * Anything gets pressed, one of three things will happen:
   *  - We're at rest and they pressed the right thing: Hop.
   *  - We're hopping: Soft rejection.
   *  - We're at rest and they pressed the wrong thing: Hard rejection.
   * The multi-step is a little different. We wait for both buttons to be on.
   */
  if (player->input!=player->pvinput) {
    if ((player->position==5)&&!player->hopping) {
      uint16_t both=(player->btnidv[3]|player->btnidv[4]);
      uint16_t bad=(EGG_BTN_SOUTH|EGG_BTN_WEST|EGG_BTN_LEFT|EGG_BTN_RIGHT|EGG_BTN_UP|EGG_BTN_DOWN)&~both;
      if ((player->input&both)==both) {
        hopscotch_hop(battle,player);
      } else if (player->input&bad) {
        hopscotch_miss(battle,player);
      }
    } else {
      #define _(tag) if ((player->input&EGG_BTN_##tag)&&!(player->pvinput&EGG_BTN_##tag)) hopscotch_stroke(battle,player,EGG_BTN_##tag);
      _(SOUTH)
      _(WEST)
      _(LEFT)
      _(RIGHT)
      _(UP)
      _(DOWN)
      #undef _
    }
    player->pvinput=player->input;
  }

  /* The first hop happens automatically.
   */
  if (player->startclock>0.0) {
    if ((player->startclock-=elapsed)<=0.0) {
      hopscotch_hop(battle,player);
    }
    return;
  }
  
  /* Hopping?
   */
  if (player->hopping) {
    if ((player->arcp+=player->arcrate*elapsed)>=1.0) {
      hopscotch_land(battle,player);
    } else {
      double r=(player->arczx-player->arcax)*0.5;
      double t=player->arcp*M_PI; // Half a circle.
      double nx=-cos(t);
      double ny=-sin(t);
      double mx=player->arcax+r;
      double my=player->ybase;
      player->x=mx+nx*r;
      player->y=my+ny*r; // We can dampen or augment this for visual effect. Not really feeling a need.
    }
    
  /* Not hopping, tick (stillclock).
   */
  } else {
    player->stillclock+=elapsed;
  }
}

/* Update.
 */
 
static void _hopscotch_update(struct battle *battle,double elapsed) {
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  /* (winner) gets established when either player is (finished).
   * We don't commit to (outcome) until both get there, or a timer expires.
   */
  if (battle->outcome==-2) {
    struct player *l=BATTLE->playerv;
    struct player *r=l+1;
    if (BATTLE->winner==-2) {
      if (l->finished&&r->finished) BATTLE->winner=0;
      else if (l->finished) BATTLE->winner=1;
      else if (r->finished) BATTLE->winner=-1;
    } else {
      BATTLE->stopclock+=elapsed;
      if ((BATTLE->stopclock>5.0)||(l->finished&&r->finished)) {
        battle->outcome=BATTLE->winner;
      }
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int ht=NS_sys_tilesize>>1;
  int x=(int)player->x;
  int y=(int)player->y;
  uint8_t tileid=player->tileid;
  if (player->hopping) tileid+=2;
  graf_tile(g_graf,x-ht,y-ht,tileid+0x00,0);
  graf_tile(g_graf,x+ht,y-ht,tileid+0x01,0);
  graf_tile(g_graf,x-ht,y+ht,tileid+0x10,0);
  graf_tile(g_graf,x+ht,y+ht,tileid+0x11,0);
}

/* Render.
 */
 
static void _hopscotch_render(struct battle *battle) {

  // bgvtxv covers the entire framebuffer.
  graf_set_image(g_graf,RID_image_battle_meadow);
  graf_tile_batch(g_graf,BATTLE->bgvtxv,BGVTXC);

  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_hopscotch={
  .name="hopscotch",
  .objlen=sizeof(struct battle_hopscotch),
  .id=110,
  .strix_name=380,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_dpad_ab,
  .imageid_default=0,
  .del=_hopscotch_del,
  .init=_hopscotch_init,
  .update=_hopscotch_update,
  .render=_hopscotch_render,
};

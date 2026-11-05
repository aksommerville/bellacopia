/* battle_swearing.c
 */

#include "game/batsup/battle_internal.h"

#define VTX_LIMIT 5

struct battle_swearing {
  struct battle hdr;
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    int x,y; // Center of main figure, in framebuffer pixels.
    int speaking;
    int speechx,speechy; // Center of word bubble. Knock it off center randomly so their overlap is more interesting.
    struct egg_render_fancy vtxv[VTX_LIMIT]; // One per "letter".
    int vtxc;
    int blackout;
    double stopclock; // Counts down after the other guy starts speaking; when it expires, our (speaking) goes off.
    double stoptimelo,stoptimehi;
    double cpudelay; // CPU wait for some random interval after becoming eligible to speak.
    double cpudelaylo,cpudelayhi;
  } playerv[2];
  int last_word; // -1,0,1: Which player spoke last? Initially -1, never returns to that.
};

#define BATTLE ((struct battle_swearing*)battle)

/* Delete.
 */
 
static void _swearing_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=80;
  player->y=(FBH>>1)+20;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->stoptimehi=0.500*(1.0-player->skill)+0.200*player->skill;
  player->stoptimelo=player->stoptimehi*0.500;
  
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->cpudelayhi=0.300*(1.0-player->skill)+0.100*player->skill;
    player->cpudelaylo=player->cpudelayhi*0.5;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0xa46d3bff;
        player->tileid=0xa0;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x00;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x50;
      } break;
  }
}

/* New.
 */
 
static int _swearing_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  BATTLE->last_word=-1;
  BATTLE->playclock=9.000;
  return 0;
}

/* Generate vertices for a bad word.
 */
 
static uint8_t swearing_random_tileid() {
  return 0x0b+(rand()%5);
}

// Horizontal advancement.
static int swearing_glyph_left(uint8_t tileid) {
  switch (tileid) {
    case 0x0b: return 8;
    case 0x0c: return 8;
    case 0x0d: return 2;
    case 0x0e: return 5;
    case 0x0f: return 7;
  }
  return 0;
}
static int swearing_glyph_right(uint8_t tileid) {
  switch (tileid) {
    case 0x0b: return 9;
    case 0x0c: return 8;
    case 0x0d: return 3;
    case 0x0e: return 5;
    case 0x0f: return 7;
  }
  return 0;
}

static void swearing_random_color(struct egg_render_fancy *vtx) {
  vtx->pr=0x80+(rand()&0x7f);
  vtx->pg=rand()%vtx->pr;
  vtx->pb=0x40;
}

static void swearing_speak(struct battle *battle,struct player *player) {
  if (player->speaking) return;
  if (player->stopclock>0.0) return;

  // Pick a center for the word bubble.
  player->speechy=player->y-NS_sys_tilesize*3+((rand()%9)-4);
  player->speechx=(FBW>>1);
  if (player->who) player->speechx+=5;
  else player->speechx-=5;
  player->speechx+=(rand()%7)-3;

  // Pick a tile and set some defaults, for each vertex.
  // Set final vertical position, and horizontal based on zero origin.
  player->vtxc=VTX_LIMIT;
  int x=0;
  uint8_t pvtileid=0;
  struct egg_render_fancy *vtx=player->vtxv;
  int i=player->vtxc;
  for (;i-->0;vtx++) {
    vtx->y=player->speechy-2;
    x+=swearing_glyph_right(pvtileid);
    vtx->tileid=swearing_random_tileid();
    x+=swearing_glyph_left(vtx->tileid);
    pvtileid=vtx->tileid;
    vtx->x=x;
    vtx->xform=0;
    vtx->rotation=0;
    vtx->size=NS_sys_tilesize;
    vtx->ta=0;
    vtx->a=0xff;
  }
  
  // Then pick a proper horizontal position, centered in the bubble.
  x+=swearing_glyph_right(pvtileid);
  int x0=player->speechx-(x>>1);
  for (i=player->vtxc,vtx=player->vtxv;i-->0;vtx++) vtx->x+=x0;
  
  // Random color for the first one, then copy it to the others.
  swearing_random_color(player->vtxv);
  for (i=player->vtxc-1,vtx=player->vtxv+1;i-->0;vtx++) {
    vtx->pr=player->vtxv[0].pr;
    vtx->pg=player->vtxv[0].pg;
    vtx->pb=player->vtxv[0].pb;
  }
  
  // Sound effect and bookkeeping.
  bm_sound_pan(RID_sound_bleep,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->speaking=1;
  BATTLE->last_word=player->who;
  struct player *other=BATTLE->playerv+(player->who?0:1);
  double n=(rand()&0xffff)/65535.0;
  other->stopclock=n*(1.0-other->stoptimelo)+n*other->stoptimehi;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
    return;
  }
  if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) {
    swearing_speak(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->speaking) return;
  if (player->cpudelay<=0.0) {
    double n=(rand()&0xffff)/65535.0;
    player->cpudelay=player->cpudelaylo*(1.0-n)+player->cpudelayhi*n;
  }
  if ((player->cpudelay-=elapsed)<=0.0) {
    swearing_speak(battle,player);
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {
  if (player->stopclock>0.0) {
    if ((player->stopclock-=elapsed)<=0.0) {
      player->speaking=0;
    }
  }
}

/* Update.
 */
 
static void _swearing_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }

  // Game ends when the clock expires. Last one to speak wins. Tie only if nobody speaks.
  if ((BATTLE->playclock-=elapsed)<=0.0) {
    switch (BATTLE->last_word) {
      case 0: battle->outcome=1; break;
      case 1: battle->outcome=-1; break;
      default: battle->outcome=0; break;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int w=NS_sys_tilesize*3,h=NS_sys_tilesize*5;
  int srcx=(player->tileid&0x0f)*NS_sys_tilesize;
  int srcy=(player->tileid>>4)*NS_sys_tilesize;
  if (player->speaking) srcx+=NS_sys_tilesize*3;
  graf_decal_xform(g_graf,player->x-(w>>1),player->y-(h>>1),srcx,srcy,w,h,player->xform);
  
  // Word bubble.
  if (player->speaking) {
    const int bubw=NS_sys_tilesize*5,bubh=NS_sys_tilesize*3;
    int bubsrcx=NS_sys_tilesize*6;
    int bubsrcy=0;
    graf_decal_xform(g_graf,player->speechx-(bubw>>1),player->speechy-(bubh>>1),bubsrcx,bubsrcy,bubw,bubh,player->xform);
    graf_fancy_batch(g_graf,player->vtxv,player->vtxc);
  }
}

/* Render.
 */
 
static void _swearing_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_set_image(g_graf,RID_image_battle_sea2);
  if (BATTLE->last_word==1) { // r on top
    player_render(battle,BATTLE->playerv+0);
    player_render(battle,BATTLE->playerv+1);
  } else { // l on top. If (last_word==-1), the order doesn't matter.
    player_render(battle,BATTLE->playerv+1);
    player_render(battle,BATTLE->playerv+0);
  }
  if (battle->outcome==-2) {
    int s=(int)(BATTLE->playclock+0.999);
    if (s<1) s=1; else if (s>9) s=9;
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
    graf_tile(g_graf,FBW>>1,20,'0'+s,0);
    graf_set_tint(g_graf,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_swearing={
  .name="swearing",
  .objlen=sizeof(struct battle_swearing),
  .id=114,
  .strix_name=388,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_a,
  .imageid_default=RID_image_labyrinth,
  .del=_swearing_del,
  .init=_swearing_init,
  .update=_swearing_update,
  .render=_swearing_render,
};

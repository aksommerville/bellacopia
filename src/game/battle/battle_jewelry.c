/* battle_jewelry.c
 * Wrestle for the falling jewelry.
 * There was going to be a bird with a magnet, I even drew it, but it ended up not feeling necessary.
 */

#include "game/batsup/battle_internal.h"

#define GROUNDY 140
#define JEWEL_LIMIT 11
#define FIREWORK_LIMIT 4

struct battle_jewelry {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint8_t xform;
    double x,y;
    int indx;
    double walkspeed;
    double animclock;
    int animframe;
    int score; // Tabulated only at the moment the battle ends.
  } playerv[2];
  
  struct jewel {
    double x,y;
    double dy;
    uint8_t tileid;
    struct player *owner;
    int rx,ry; // If (owner), this is our position relative to their center. (rx) is for when (xform==0).
  } jewelv[JEWEL_LIMIT];
  int jewelc;
  
  struct firework {
    double x,y;
    double dx,dy;
    double ttl;
    uint32_t color;
  } fireworkv[FIREWORK_LIMIT];
  int fireworkc;
  
  double jewelclock;
};

#define BATTLE ((struct battle_jewelry*)battle)

/* Delete.
 */
 
static void _jewelry_del(struct battle *battle) {
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  player->x=60.0;
  player->y=GROUNDY-(NS_sys_tilesize>>1)+1;
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
    player->x=FBW-player->x;
    player->xform=EGG_XFORM_XREV;
  }
  
  player->walkspeed=60.0*(1.0-player->skill)+150.0*player->skill;
  player->walkspeed+=((rand()&0xffff)*10.0)/65535.0;
  
  if (player->human=human) { // Human.
  } else { // CPU.
    player->walkspeed*=0.800;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x870e0eff;
        player->tileid=0x46;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x26;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x36;
      } break;
  }
}

/* New.
 */
 
static int _jewelry_init(struct battle *battle) {
  battle_normalize_bias(&BATTLE->playerv[0].skill,&BATTLE->playerv[1].skill,battle);
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
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
}

/* Update CPU player.
 * No doubt about how to play this: Find the nearest unclaimed jewel and walk toward it. Easy.
 * We'll make bad decisions sometimes when the nearest is freshly spawned, but something a little further away is ready. Meh.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  // Examine the jewels from scratch each frame. Overkill but whatever.
  double targetx=0.0;
  double bestdx=999.999;
  const struct jewel *jewel=BATTLE->jewelv;
  int i=BATTLE->jewelc;
  for (;i-->0;jewel++) {
    if (jewel->owner) continue;
    double dx=jewel->x-player->x;
    if (dx<0.0) dx=-dx;
    if (dx<bestdx) {
      targetx=jewel->x;
      bestdx=dx;
    }
  }
  double dx=targetx-player->x;
  if (dx<-5.0) player->indx=-1;
  else if (dx>5.0) player->indx=1;
  else player->indx=0;
}

/* Catch a jewel.
 */
 
static void jewelry_catch(struct battle *battle,struct player *player,struct jewel *jewel) {
  bm_sound_pan(RID_sound_collect,player->who?PLAYER_PAN:-PLAYER_PAN);
  jewel->owner=player;
  
  /* Locked in position has nothing to do with the catch orientation.
   */
  jewel->rx=rand()%11-5;
  jewel->ry=rand()%11-5;
  
  /* Create a firework if we can.
   */
  if (BATTLE->fireworkc<FIREWORK_LIMIT) {
    struct firework *firework=BATTLE->fireworkv+BATTLE->fireworkc++;
    firework->x=player->x;
    firework->y=player->y-6.0;
    firework->dx=player->who?10.0:-10.0;
    firework->dy=-10.0;
    firework->ttl=0.500;
    firework->color=player->color;
  }
}

/* Update all players, after specific controller.
 */
 
static void player_update_common(struct battle *battle,struct player *player,double elapsed) {

  if (battle->outcome>-2) {
    player->animframe=0;
    return;
  }

  /* Walking?
   */
  if (player->indx) {
    const double edge=6.0;
    player->x+=player->walkspeed*elapsed*player->indx;
    if (player->x<edge) player->x=edge;
    else if (player->x>FBW-edge) player->x=FBW-edge;
    if ((player->animclock-=elapsed)<=0.0) {
      player->animclock+=0.125;
      if (++(player->animframe)>=4) player->animframe=0;
    }
    if (player->indx>0) player->xform=0;
    else player->xform=EGG_XFORM_XREV;
  } else {
    player->animclock=0.0;
    player->animframe=0;
  }
}

/* Update jewel.
 * We check for player pickup here too, and it's important that it be done here and not during the player update.
 * Ties are very common, when the players are pushing each other to catch one.
 */
 
static void jewel_update(struct battle *battle,struct jewel *jewel,double elapsed) {

  if (jewel->owner) return;
  
  const double bottom=GROUNDY;
  if (jewel->y<bottom) {
    jewel->y+=jewel->dy*elapsed;
    if (jewel->y>=bottom) {
      jewel->y=bottom;
      bm_sound_pan(RID_sound_weebump,0.0);
    }
  }
  
  if (jewel->y>GROUNDY-16.0) {
    const double radius=10.0;
    double ldx=jewel->x-BATTLE->playerv[0].x;
    double rdx=jewel->x-BATTLE->playerv[1].x;
    int lnear=((ldx>-radius)&&(ldx<radius));
    int rnear=((rdx>-radius)&&(rdx<radius));
    if (lnear&&rnear) { // Contention! Award it randomly according to skill.
      double n=(rand()&0xffff)/65535.0;
      if (n<=BATTLE->playerv[0].skill) {
        jewelry_catch(battle,BATTLE->playerv+0,jewel);
      } else {
        jewelry_catch(battle,BATTLE->playerv+1,jewel);
      }
    } else if (lnear) {
      jewelry_catch(battle,BATTLE->playerv+0,jewel);
    } else if (rnear) {
      jewelry_catch(battle,BATTLE->playerv+1,jewel);
    }
  }
}

/* Spawn a jewel.
 */
 
static struct jewel *jewelry_spawn(struct battle *battle) {
  if (BATTLE->jewelc>=JEWEL_LIMIT) return 0;
  struct jewel *jewel=BATTLE->jewelv+BATTLE->jewelc++;
  jewel->x=5.0+((rand()&0xffff)*310.0)/65535.0;
  jewel->y=-8.0;
  jewel->dy=50.0+((rand()&0xffff)*50.0)/65535.0;
  jewel->tileid=0x19+rand()%7;
  jewel->owner=0;
  return jewel;
}

/* Update.
 */
 
static void _jewelry_update(struct battle *battle,double elapsed) {

  /* Update fireworks before the completion check; they run to completion.
   */
  struct firework *firework=BATTLE->fireworkv+BATTLE->fireworkc-1;
  int i=BATTLE->fireworkc;
  for (;i-->0;firework--) {
    if ((firework->ttl-=elapsed)<=0.0) {
      BATTLE->fireworkc--;
      memmove(firework,firework+1,sizeof(struct firework)*(BATTLE->fireworkc-i));
    } else {
      firework->x+=firework->dx*elapsed;
      firework->y+=firework->dy*elapsed;
    }
  }

  /* If the battle is over, just force players to their neutral frame and that's it.
   */
  if (battle->outcome>-2) {
    BATTLE->playerv[0].animframe=0;
    BATTLE->playerv[1].animframe=0;
    return;
  }
  
  /* Tick down (jewelclock), and when it expires, spawn a new one.
   */
  if ((BATTLE->jewelclock-=elapsed)<=0.0) {
    BATTLE->jewelclock+=0.700+((rand()&0xffff)*1.000)/65535.0;
    jewelry_spawn(battle);
  }
  
  /* Update player control and motion.
   */
  struct player *player=BATTLE->playerv;
  for (i=2;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g_input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    player_update_common(battle,player,elapsed);
  }
  
  /* Player-on-player physics happens after both update.
   * They can push each other.
   * Due to the different speeds per skill, one player can move the other.
   * But at balanced difficulty, they cancel out exactly.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  const double clearance=14.0;
  double dx=r->x-l->x;
  if ((dx>-clearance)&&(dx<clearance)) {
    double midx=(l->x+r->x)*0.5;
    if (midx<6.0+clearance*0.5) midx=6.0+clearance*0.5;
    else if (midx>FBW-6.0-clearance*0.5) midx=FBW-6.0-clearance*0.5;
    // They can't jump or anything, so (l) is always on the left.
    l->x=midx-clearance*0.5;
    r->x=midx+clearance*0.5;
  }
  
  /* Fall the loose jewels and check for caught ones.
   */
  struct jewel *jewel=BATTLE->jewelv;
  for (i=BATTLE->jewelc;i-->0;jewel++) {
    jewel_update(battle,jewel,elapsed);
  }
  
  /* Game ends when all jewels have been spawned, and they're all either on the floor or owned.
   * At that point, it ends instantly, and the winner is whoever's most blinged.
   * Ties are very possible.
   */
  if (BATTLE->jewelc>=JEWEL_LIMIT) {
    int ready=1;
    l->score=r->score=0;
    for (i=BATTLE->jewelc,jewel=BATTLE->jewelv;i-->0;jewel++) {
      if ((jewel->y<GROUNDY)&&!jewel->owner) { // wait wait, i'm still falling
        ready=0;
        break;
      }
      if (jewel->owner==l) l->score++;
      else if (jewel->owner==r) r->score++;
    }
    if (ready) {
      if (l->score>r->score) battle->outcome=1;
      else if (l->score<r->score) battle->outcome=-1;
      else battle->outcome=0;
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int x=(int)player->x;
  int y=(int)player->y;
  uint8_t tileid=player->tileid;
  switch (player->animframe) {
    case 1: tileid+=1; break;
    case 3: tileid+=2; break;
  }
  graf_tile(g_graf,x,y,tileid,player->xform);
  
  /* Render jewels that I own.
   * Players might be allowed to overlap, so we have to do this right away.
   */
  struct jewel *jewel=BATTLE->jewelv;
  int i=BATTLE->jewelc;
  for (;i-->0;jewel++) {
    if (jewel->owner!=player) continue;
    int jx=x;
    if (player->xform&EGG_XFORM_XREV) jx-=jewel->rx;
    else jx+=jewel->rx;
    graf_tile(g_graf,jx,y+jewel->ry,jewel->tileid,0);
  }
}

/* Render a player's score above her head.
 */
 
static void jewelry_render_score(struct battle *battle,struct player *player) {
  int midx=(int)player->x;
  int midy=(int)player->y-20;
  if (player->score>=10) {
    graf_tile(g_graf,midx-4,midy,'0'+(player->score/10)%10,0);
    graf_tile(g_graf,midx+4,midy,'0'+player->score%10,0);
  } else {
    graf_tile(g_graf,midx,midy,'0'+player->score,0);
  }
}

/* Render.
 */
 
static void _jewelry_render(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;

  graf_fill_rect(g_graf,0,0,FBW,FBH,battle->ctab[BATTLE_COLOR_SKY]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,FBH-GROUNDY,battle->ctab[BATTLE_COLOR_GROUND]);
  graf_fill_rect(g_graf,0,GROUNDY,FBW,1,0x000000ff);

  graf_set_image(g_graf,RID_image_battle_meadow2);
  player_render(battle,l);
  player_render(battle,r);
  
  struct jewel *jewel=BATTLE->jewelv;
  int i=BATTLE->jewelc;
  for (;i-->0;jewel++) {
    if (jewel->owner) continue; // Players render these.
    graf_tile(g_graf,(int)jewel->x,(int)jewel->y,jewel->tileid,0);
  }
  
  struct firework *firework=BATTLE->fireworkv;
  for (i=BATTLE->fireworkc;i-->0;firework++) {
    graf_fancy(g_graf,(int)firework->x,(int)firework->y,0x2c,0,0,NS_sys_tilesize,0,firework->color);
  }
  
  // When finished, draw the count above each player's head.
  if (battle->outcome>-2) {
    graf_set_image(g_graf,RID_image_fonttiles);
    graf_set_tint(g_graf,battle->ctab[BATTLE_COLOR_SKY_TEXT]);
    jewelry_render_score(battle,l);
    jewelry_render_score(battle,r);
    graf_set_tint(g_graf,0);
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_jewelry={
  .name="jewelry",
  .objlen=sizeof(struct battle_jewelry),
  .id=131,
  .strix_name=425,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=1,
  .input=battle_input_horz,
  .imageid_default=RID_image_meadow,
  .del=_jewelry_del,
  .init=_jewelry_init,
  .update=_jewelry_update,
  .render=_jewelry_render,
};

/* battle_gobbling.c
 */

#include "game/bellacopia.h"

#define END_COOLDOWN 1.0
#define TABLEW (NS_sys_tilesize*6)
#define CLOTHW (TABLEW-8)
#define PULL_LIMIT (TABLEW-15)
#define VISIBLE_PULL_LIMIT (TABLEW-24)
#define ENTREE_LIMIT 5
#define PULL_RATE_SLOW (1.0/40.0)
#define PULL_RATE_FAST (1.0/80.0)
#define CPU_PENALTY 1.400 /* Give humans an edge against CPU players. */
#define GOBBLE_TIME 1.000

struct battle_gobbling {
  struct battle hdr;
  double end_cooldown;
  int poison_texid,poisonw,poisonh;
  
  struct player {
    int who;
    int human;
    int srcx,srcy; // 64x48 pixels total. 32x48 idle on the left, and two gobble options for the top half on the right.
    int armsrcx,armsrcy; // 32x16 pixels.
    double skill;
    int pull;
    int btnid;
    double pullrate; // s/px
    double pullclock;
    int armw;
    int grab; // When pulling, have we grabbed the cloth yet?
    int blackout;
    double gobble;
    uint8_t highlight_poison;
    struct entree {
      int discard; // Nonzero if it should render on the floor. (x) ix still relevant.
      int x; // Relative to table. Player is responsible for updating when (pull) changes.
      uint8_t tileid; // Zero if defunct. The actual tile zero is part of Stealing Contest's dragon, not something we could use.
    } entreev[ENTREE_LIMIT];
  } playerv[2];
};

#define BATTLE ((struct battle_gobbling*)battle)

/* Delete.
 */
 
static void _gobbling_del(struct battle *battle) {
  egg_texture_del(BATTLE->poison_texid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int appearance) {
  if (player==BATTLE->playerv) { // Left
    player->who=0;
    player->btnid=EGG_BTN_LEFT;
    player->skill=1.0-(battle->args.bias/255.0);
  } else { // Right
    player->who=1;
    player->btnid=EGG_BTN_RIGHT;
    player->skill=(battle->args.bias/255.0);
  }
  if (player->human=human) { // Human
  } else { // CPU
  }
  switch (appearance) {
    case 0: { // Goblin
        player->srcx=64;
        player->srcy=48;
        player->armsrcx=192;
        player->armsrcy=64;
      } break;
    case 1: { // Dot
        player->srcx=0;
        player->srcy=48;
        player->armsrcx=192;
        player->armsrcy=48;
      } break;
    case 2: { // Princess
        player->srcx=128;
        player->srcy=48;
        player->armsrcx=192;
        player->armsrcy=80;
      } break;
  }
  player->blackout=1;
  player->pullrate=PULL_RATE_SLOW*(1.0-player->skill)+PULL_RATE_FAST*player->skill;
  if (!player->human) player->pullrate*=CPU_PENALTY;
  struct entree *entree=player->entreev;
  int i=ENTREE_LIMIT,x=NS_sys_tilesize;
  for (;i-->0;entree++,x+=NS_sys_tilesize) {
    entree->x=x;
  }
}

/* Assign an entree tileid to the given index of open slots.
 */
 
static void assign_entree(struct player *player,int p,uint8_t tileid) {
  struct entree *entree=player->entreev;
  int i=ENTREE_LIMIT;
  for (;i-->0;entree++) {
    if (entree->tileid) continue;
    if (!p--) {
      entree->tileid=tileid;
      return;
    }
  }
}

/* New.
 */

static int _gobbling_init(struct battle *battle) {
  player_init(battle,BATTLE->playerv+0,battle->args.lctl,battle->args.lface);
  player_init(battle,BATTLE->playerv+1,battle->args.rctl,battle->args.rface);
  
  /* Select six entrees.
   * Both players get the same six things but not in the same order.
   * There are six tiles of food: 1d 1e 1f 2d 2e 2f
   * And let's say two tiles of non-food: 61 64 (cactus and iceskate, borrowed from Laziness)
   * One may object that cacti are in fact edible and delicious, but look: Ours is a potted cactus with the spines still attached. Do not eat.
   * Select one non-food and the remaining five food. Do not repeat any.
   */
  uint8_t menu[ENTREE_LIMIT];
  menu[0]=(rand()&1)?0x61:0x64;
  uint8_t candidatev[]={0x1d,0x1e,0x1f,0x2d,0x2e,0x2f};
  int candidatec=sizeof(candidatev);
  int i=1; for (;i<ENTREE_LIMIT;i++) {
    if (candidatec<1) {
      menu[i]=0x1d;
    } else {
      int p=rand()%candidatec;
      menu[i]=candidatev[p];
      candidatec--;
      memmove(candidatev+p,candidatev+p+1,candidatec-p);
    }
  }
  
  /* Assign these six entrees to random slots in each player.
   * Note that player[0] entrees are ordered near-to-far and player[1] far-to-near. But it doesn't matter.
   */
  int available=ENTREE_LIMIT;
  for (i=0;i<ENTREE_LIMIT;i++,available--) {
    assign_entree(BATTLE->playerv+0,rand()%available,menu[i]);
    assign_entree(BATTLE->playerv+1,rand()%available,menu[i]);
  }
   
  return 0;
}

/* Choose a randomish spot on the floor that isn't occupied yet.
 */
 
static void entree_choose_discard_position(struct entree *entree,struct entree *peerv,int focusx) {
  // Collect the lowest and highest (x) from each discarded entree.
  int lo=focusx,hi=focusx,c=0;
  struct entree *q=peerv;
  int i=ENTREE_LIMIT;
  for (;i-->0;q++) {
    if (!q->discard||!q->tileid||(q==entree)) continue;
    c++;
    if (q->x<lo) lo=q->x;
    else if (q->x>hi) hi=q->x;
  }
  // If we're the first discard, use the focus.
  if (!c) {
    entree->x=focusx;
    return;
  }
  // Whichever of (lo,hi) has the lower absolute value, put us in that spot.
  const int spacing=NS_sys_tilesize>>1;
  lo=-lo;
  if (lo<hi) {
    entree->x=-lo-spacing;
  } else {
    entree->x=hi+spacing;
  }
}

/* Pull, or pull not. All players should call one or the other, every update.
 * The actual pull delta might increase by more than one per frame.
 */
 
static void player_pull(struct battle *battle,struct player *player,double elapsed) {

  /* Arm extending.
   * We won't extend and pull on the same update, but we might leave some remainder in (pullclock) for next time.
   */
  if (!player->grab) {
    int narmw=player->armw;
    if (narmw<8) narmw=8; // Snap to 8 when extending, below that it's not visible.
    player->pullclock-=elapsed;
    while (player->pullclock<0.0) {
      player->pullclock+=player->pullrate; // Also extension rate.
      narmw++;
      if (narmw>=32) {
        player->armw=32;
        player->grab=1;
        return;
      }
    }
    player->armw=narmw;
    return;
  }
  
  /* Retracting arm and cloth together.
   */
  int npull=player->pull;
  int narmw=player->armw;
  if (narmw<=0) return; // Can't pull more. Let go!
  player->pullclock-=elapsed;
  while (player->pullclock<0.0) {
    player->pullclock+=player->pullrate;
    npull++;
    narmw--;
    if (narmw<=0) break;
  }
  player->armw=narmw;
  if (npull>PULL_LIMIT) npull=PULL_LIMIT;
  if (npull==player->pull) return;
  double d=player->pull-npull;
  if (player->who) d=-d;
  player->pull=npull;
  struct entree *entree=player->entreev;
  int i=ENTREE_LIMIT;
  for (;i-->0;entree++) {
    if (!entree->tileid) continue;
    if (entree->discard) continue;
    entree->x+=d;
    if ((entree->x<0)||(entree->x>TABLEW)) {
      entree->discard=1;
      entree_choose_discard_position(entree,player->entreev,player->who?TABLEW:0);
    }
  }
}

static void player_pull_no(struct battle *battle,struct player *player,double elapsed) {
  player->grab=0;
  if (player->armw>0) {
    player->pullclock-=elapsed;
    while (player->pullclock<0.0) {
      player->pullclock+=player->pullrate;
      player->armw--;
      if (player->armw<8) {
        player->armw=0;
        break;
      }
    }
  } else {
    player->pullclock=0.0;
    player->grab=0;
    player->armw=0;
  }
}

/* Generate poison label if we don't have one.
 */
 
static void gobbling_require_poison_label(struct battle *battle) {
  if (BATTLE->poison_texid) return;
  const char *src;
  int srcc=text_get_string(&src,RID_strings_battle,59);
  BATTLE->poison_texid=font_render_to_texture(0,g.font,src,srcc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&BATTLE->poisonw,&BATTLE->poisonh,BATTLE->poison_texid);
}

/* Eat the next thing if it's in range.
 */
 
static int tileid_is_food(uint8_t tileid) {
  switch (tileid) {
    case 0x1d: case 0x1e: case 0x1f:
    case 0x2d: case 0x2e: case 0x2f:
      return 1;
  }
  return 0;
}
 
static void player_gobble(struct battle *battle,struct player *player) {
  player->blackout=1;
  struct entree *nearest=0;
  struct entree *q=player->entreev;
  int i=ENTREE_LIMIT;
  for (;i-->0;q++) {
    if (!q->tileid) continue;
    if (q->discard) continue;
    if (!nearest) nearest=q;
    else if (player->who&&(q->x>nearest->x)) nearest=q;
    else if (!player->who&&(q->x<nearest->x)) nearest=q;
  }
  if (nearest) { // Check range.
    int distance;
    if (player->who) distance=TABLEW-nearest->x;
    else distance=nearest->x;
    if (distance>20) nearest=0;
  }
  if (!nearest) { // Nothing in range.
    bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
  } else if (tileid_is_food(nearest->tileid)) { // Eat.
    nearest->tileid=0;
    player->gobble=GOBBLE_TIME;
    bm_sound_pan(RID_sound_collect,player->who?0.250:-0.250);
  } else { // Eat something that isn't food -- you lose.
    bm_sound_pan(RID_sound_reject,player->who?0.250:-0.250);
    battle->outcome=player->who?1:-1;
    BATTLE->end_cooldown=END_COOLDOWN;
    player->highlight_poison=nearest->tileid;
    gobbling_require_poison_label(battle);
  }
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
    else input&=~EGG_BTN_SOUTH;
  }
  if (player->gobble>0.0) {
    player->gobble-=elapsed;
    player_pull_no(battle,player,elapsed);
    return;
  }
  if (input&player->btnid) {
    player_pull(battle,player,elapsed);
  } else {
    player_pull_no(battle,player,elapsed);
    if (input&EGG_BTN_SOUTH) player_gobble(battle,player);
  }
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  // Gobbling? Carry on.
  if (player->gobble>0.0) {
    player->gobble-=elapsed;
    player_pull_no(battle,player,elapsed);
    return;
  }
  
  // If we're pulling and arm is maximally contracted, spend one cycle ending the pull.
  if (player->grab&&(player->armw<1)) {
    player_pull_no(battle,player,elapsed);
    return;
  }
  
  // Find the nearest entree. Same logic as player_gobble().
  int distance=FBW;
  struct entree *nearest=0;
  struct entree *q=player->entreev;
  int i=ENTREE_LIMIT;
  for (;i-->0;q++) {
    if (!q->tileid) continue;
    if (q->discard) continue;
    if (!nearest) nearest=q;
    else if (player->who&&(q->x>nearest->x)) nearest=q;
    else if (!player->who&&(q->x<nearest->x)) nearest=q;
  }
  if (nearest) { // Check range.
    if (player->who) distance=TABLEW-nearest->x;
    else distance=nearest->x;
  }
  
  // If the nearest entree doesn't exist, the only thing to do is nothing.
  if (!nearest) {
    player_pull_no(battle,player,elapsed);
    return;
  }
  
  // If the nearest entree is poison, keep pulling until it falls.
  // CPU player will never eat poison. (TODO Maybe at very low skill?)
  if (!tileid_is_food(nearest->tileid)) {
    player_pull(battle,player,elapsed);
    return;
  }
  
  // If the nearest entree is too far away, keep pulling.
  // The threshold is 20. We'll aim for less than that, out of pity for the humans. (TODO Could adjust threshold per skill)
  if (distance>10) {
    player_pull(battle,player,elapsed);
    return;
  }
  
  // If our arm is still extended, wait for it to retreat.
  player_pull_no(battle,player,elapsed);
  if (player->armw>0) return;
  
  // Eat it.
  player_gobble(battle,player);
}

/* Check outcome.
 * -2 if not final yet.
 * We do not handle the eating-poison case; that should cause an outcome to be set immediately.
 */
 
static int gobbling_check_outcome(struct battle *battle) {
  struct entree *entree;
  int i;
  // Each of a player's entrees are either on the table, on the floor, or in the belly.
  // For those on the floor, we'll distinguish between food and poison. Poison belongs there; food does not.
  int ltable=0,lfoodfloor=0,lpoisonfloor=0,lbelly=0,rtable=0,rfoodfloor=0,rpoisonfloor=0,rbelly=0;
  for (entree=BATTLE->playerv[0].entreev,i=ENTREE_LIMIT;i-->0;entree++) {
    if (!entree->tileid) lbelly++; // Item removed. Could only have been eaten.
    else if (!entree->discard) ltable++;
    else if (tileid_is_food(entree->tileid)) lfoodfloor++;
    else lpoisonfloor++;
  }
  for (entree=BATTLE->playerv[1].entreev,i=ENTREE_LIMIT;i-->0;entree++) {
    if (!entree->tileid) rbelly++; // Item removed. Could only have been eaten.
    else if (!entree->discard) rtable++;
    else if (tileid_is_food(entree->tileid)) rfoodfloor++;
    else rpoisonfloor++;
  }
  // First empty table wins unless the other player ate more or they have food on the floor.
  if (!ltable&&rtable&&!lfoodfloor&&(lbelly>=rbelly)) return 1;
  if (ltable&&!rtable&&!rfoodfloor&&(lbelly<=rbelly)) return -1;
  // Otherwise if either table remains populated, wait.
  if (ltable||rtable) return -2;
  // If one gobbled more than the other, the fatter wins.
  if (lbelly>rbelly) return 1;
  if (lbelly<rbelly) return -1;
  // If one discarded more than the other, they win. This can only be unequal if one player discarded the poison and the other hasn't reached it yet.
  //if (lfloor>rfloor) return 1;
  //if (lfloor<rfloor) return -1;
  // At this point, we could break ties by (pull) but I think it's better to just declare a tie.
  return 0;
}

/* Update.
 */
 
static void _gobbling_update(struct battle *battle,double elapsed) {

  // Done?
  if (battle->outcome>-2) return;
  
  // Regular update.
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->human) player_update_man(battle,player,elapsed,g.input[player->human]);
    else player_update_cpu(battle,player,elapsed);
    if (battle->outcome>-2) return; // Eating poison forces an outcome immediately.
  }
  
  // Finished?
  if ((battle->outcome=gobbling_check_outcome(battle))>-2) {
    BATTLE->end_cooldown=END_COOLDOWN;
  }
}

/* Render table.
 * (pull) is the pixels of tablecloth displacement. Negative for leftward, positive rightward.
 */
 
static void table_render(struct battle *battle,int dstx,int pull) {

  // The logical pull limit goes a bit beyond what we'll render.
  if (pull>VISIBLE_PULL_LIMIT) pull=VISIBLE_PULL_LIMIT;
  else if (pull<-VISIBLE_PULL_LIMIT) pull=-VISIBLE_PULL_LIMIT;

  // Wood.
  graf_decal(&g.graf,dstx   ,124,160,24,32,24);
  graf_decal(&g.graf,dstx+32,124,176,24,16,24);
  graf_decal(&g.graf,dstx+48,124,176,24,16,24);
  graf_decal(&g.graf,dstx+64,124,176,24,32,24);
  
  // Compute some tablecloth parameters.
  int clothx=dstx+(TABLEW>>1)-(CLOTHW>>1)+pull;
  int clothw=CLOTHW;
  int pileh=0; // Total width not sitting on the table.
  int pilex;
  uint8_t pilexform=0;
  uint8_t ltile=0x0c,rtile=0x0c;
  uint8_t lxform=EGG_XFORM_XREV,rxform=0;
  if (clothx<dstx) {
    pilex=dstx+4;
    pileh=dstx-clothx;
    clothw-=pileh;
    clothx=dstx;
    ltile=0x0a;
    lxform=0;
  } else if (clothx+clothw>dstx+TABLEW) {
    pilex=dstx+TABLEW-4;
    pileh=clothx+clothw-(dstx+TABLEW);
    pilexform=EGG_XFORM_XREV;
    clothw=dstx+TABLEW-clothx;
    rtile=0x0a;
    rxform=EGG_XFORM_XREV;
  }
  
  // Tablecloth, the part resting on the table.
  int ty=124+(NS_sys_tilesize>>1);
  int tlx=clothx+(NS_sys_tilesize>>1);
  int trx=clothx+clothw-(NS_sys_tilesize>>1);
  int midw=trx-tlx-NS_sys_tilesize;
  if (midw>0) { // Draw some tiles in the middle.
    if (pull<0) { // Pulling left: Anchor to the right side.
      int tx=trx-NS_sys_tilesize;
      while (tx>tlx) {
        graf_tile(&g.graf,tx,ty,0x0b,0);
        tx-=NS_sys_tilesize;
      }
    } else { // Pulling right: Anchor to the left side.
      int tx=tlx+NS_sys_tilesize;
      while (tx<trx) {
        graf_tile(&g.graf,tx,ty,0x0b,0);
        tx+=NS_sys_tilesize;
      }
    }
  }
  graf_tile(&g.graf,tlx,ty,ltile,lxform);
  graf_tile(&g.graf,trx,ty,rtile,rxform);
  
  // Tablecloth excess in a pile below the pulling edge.
  // It's not pixel-perfect, not at all. Start with two lumps, the minimum to connect to the corner.
  if (pileh>8) {
    graf_tile(&g.graf,pilex,140,0x0d,pilexform);
    graf_tile(&g.graf,pilex,136,0x0e,pilexform);
    if (pileh>32) {
      graf_tile(&g.graf,pilex,132,0x0e,pilexform);
      if (pileh>64) {
        graf_tile(&g.graf,pilex,128,0x0e,pilexform);
      }
    }
  } else if (pileh>0) {
    graf_tile(&g.graf,pilex,138,0x0f,pilexform);
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  int dstx=30;
  uint8_t xform=0;
  if (player->who) {
    dstx=FBW-dstx-32;
    xform=EGG_XFORM_XREV;
  }
  if (player->armw>0) {
    int armsrcx=player->armsrcx+32-player->armw;
    int armdstx=dstx+16;
    int armdsty=116;
    uint8_t armxform=0;
    if (player->who) {
      armdstx-=player->armw;
      armxform=EGG_XFORM_XREV;
    }
    graf_decal_xform(&g.graf,armdstx,armdsty,armsrcx,player->armsrcy,player->armw,16,armxform);
  }
  if (player->gobble>0.0) {
    int frame=(g.framec&8)?1:0;
    graf_decal_xform(&g.graf,dstx,100,player->srcx+32,player->srcy+frame*24,32,24,xform);
    graf_decal_xform(&g.graf,dstx,124,player->srcx,player->srcy+24,32,24,xform);
  } else {
    graf_decal_xform(&g.graf,dstx,100,player->srcx,player->srcy,32,48,xform);
  }
  
  int tablex;
  if (player->who==0) {
    table_render(battle,tablex=dstx+24,-player->pull);
  } else {
    table_render(battle,tablex=dstx-88,player->pull);
  }
  
  int entreey=117;
  struct entree *entree=player->entreev;
  int i=ENTREE_LIMIT;
  for (;i-->0;entree++) {
    if (!entree->tileid) continue;
    if (entree->tileid==player->highlight_poison) continue;
    if (entree->discard) {
      graf_tile(&g.graf,tablex+entree->x,entreey+24,entree->tileid,0);
    } else {
      graf_tile(&g.graf,tablex+entree->x,entreey,entree->tileid,0);
    }
  }
  
  if (player->highlight_poison) {
    int poisonx=tablex+(player->who?(TABLEW-10):10);
    graf_tile(&g.graf,poisonx,100,player->highlight_poison,0);
    if (g.framec%50>=10) {
      graf_set_input(&g.graf,BATTLE->poison_texid);
      graf_set_tint(&g.graf,0x800000ff);
      graf_decal(&g.graf,poisonx-(BATTLE->poisonw>>1),90-BATTLE->poisonh,0,0,BATTLE->poisonw,BATTLE->poisonh);
      graf_set_tint(&g.graf,0);
      graf_set_image(&g.graf,RID_image_battle_goblins);
    }
  }
}

/* Render.
 */
 
static void _gobbling_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x808080ff);
  
  graf_set_image(&g.graf,RID_image_battle_goblins);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_gobbling={
  .name="gobbling",
  .objlen=sizeof(struct battle_gobbling),
  .strix_name=48,
  .no_article=0,
  .no_contest=0,
  .support_pvp=1,
  .support_cvc=1,
  .del=_gobbling_del,
  .init=_gobbling_init,
  .update=_gobbling_update,
  .render=_gobbling_render,
};

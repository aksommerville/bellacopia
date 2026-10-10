/* battle_poker.c
 */

#include "game/batsup/battle_internal.h"
#include "game/batsup/cards.h"

#define HAND_SIZE 5

struct battle_poker {
  struct battle hdr;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t finger_tileid,finger_xform;
    int y;
    int fingery;
    uint8_t hand[HAND_SIZE];
    uint8_t exchangev[HAND_SIZE];
    int exposed;
    int fingerp; // -1..HAND_SIZE; HAND_SIZE means the "OK" button, and -1 means not interactive.
    int blackout;
    double suspense; // Counts down after committing, before your new cards are revealed.
    int finished;
    struct poker_hand an; // Populated after (finished) for all players; and before for CPU players only.
    int lbltexid,lblw,lblh;
    // CPU state:
    double cpuclock;
    int cpunext; // 0..HAND_SIZE, or <0 to make something up at the next cycle
  } playerv[2];
  
  struct deck deck;
};

#define BATTLE ((struct battle_poker*)battle)

/* Delete.
 */
 
static void _poker_del(struct battle *battle) {
  egg_texture_del(BATTLE->playerv[0].lbltexid);
  egg_texture_del(BATTLE->playerv[1].lbltexid);
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
    player->y=(FBH>>1)+10+(NS_sys_tilesize*2);
    player->fingery=FBH-10;
    player->finger_xform=EGG_XFORM_SWAP|EGG_XFORM_XREV;
  } else { // Right.
    player->who=1;
    player->y=(FBH>>1)-10-(NS_sys_tilesize*2);
    player->fingery=10;
    player->finger_xform=EGG_XFORM_SWAP|EGG_XFORM_YREV;
  }
  player->fingerp=2;
  if (player->human=human) { // Human.
    player->exposed=1;
    player->blackout=1;
  } else { // CPU.
    player->exposed=0;
    player->cpuclock=1.000;
    player->cpunext=-1;
  }
  switch (face) {
    case NS_face_monster: {
        player->color=0x886438ff;
        player->finger_tileid=0x22;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->finger_tileid=0x20;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->finger_tileid=0x21;
      } break;
  }
}

/* Draw the initial hand.
 */
 
static uint8_t poker_cheat(struct battle *battle,uint8_t rank) {
  // "Hey! Look over there!"
  int i=BATTLE->deck.cardidp;
  for (;i<52;i++) {
    int qrank=RANK_FROM_CARDID(BATTLE->deck.cardidv[i]);
    if (qrank==rank) {
      uint8_t tmp=BATTLE->deck.cardidv[i];
      BATTLE->deck.cardidv[i]=BATTLE->deck.cardidv[BATTLE->deck.cardidp];
      BATTLE->deck.cardidv[BATTLE->deck.cardidp]=tmp;
      break;
    }
  }
  // ...and now draw as usual...
  return deck_draw(&BATTLE->deck);
}
 
static void poker_draw_lucky(struct battle *battle,struct player *player) {
  // Three random cards.
  player->hand[0]=deck_draw(&BATTLE->deck);
  player->hand[2]=deck_draw(&BATTLE->deck);
  player->hand[4]=deck_draw(&BATTLE->deck);
  // Then for slots [1] and [3], scan the deck for something that matches the prior slot's rank.
  player->hand[1]=poker_cheat(battle,RANK_FROM_CARDID(player->hand[0]));
  player->hand[3]=poker_cheat(battle,RANK_FROM_CARDID(player->hand[2]));
  // And then just to break the monotony, randomize their order.
  int i=10; while (i-->0) {
    int ap=rand()%5;
    int bp=rand()%5;
    if (ap!=bp) {
      uint8_t tmp=player->hand[ap];
      player->hand[ap]=player->hand[bp];
      player->hand[bp]=tmp;
    }
  }
}

static void poker_draw_normal(struct battle *battle,struct player *player) {
  int i=HAND_SIZE;
  while (i-->0) player->hand[i]=deck_draw(&BATTLE->deck);
}

/* New.
 */
 
static int _poker_init(struct battle *battle) {
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  
  deck_shuffle(&BATTLE->deck);
  // A player with high skill, eg via goodluck, gets dealt two pair (possibly four of a kind).
  if (l->skill>=0.900) poker_draw_lucky(battle,l);
  else poker_draw_normal(battle,l);
  if (r->skill>=0.900) poker_draw_lucky(battle,r);
  else poker_draw_normal(battle,r);
  // CPU players get a prepared analysis; human players do this themselves, or fail to.
  if (!l->human) poker_hand_analyze(&l->an,l->hand);
  if (!r->human) poker_hand_analyze(&r->an,r->hand);
  
  return 0;
}

/* Generate text label describing the player's hand.
 * Both (hand) and (an) must be final.
 */
 
static void poker_generate_label(struct battle *battle,struct player *player) {
  if (player->lbltexid) return;
  char text[256];
  int textc=poker_hand_repr(text,sizeof(text),&player->an);
  if ((textc<0)||(textc>sizeof(text))) return;
  player->lbltexid=font_render_to_texture(0,g_font,text,textc,FBW,FBH,0xffffffff);
  egg_texture_get_size(&player->lblw,&player->lblh,player->lbltexid);
}

/* Validate player for submission.
 * Just checks his exchange count.
 * Nonzero if valid.
 */
 
static int poker_validate(struct battle *battle,struct player *player) {
  int keepace=0,exchangec=0,i=HAND_SIZE;
  while (i-->0) {
    if (player->exchangev[i]) exchangec++;
    else if (RANK_FROM_CARDID(player->hand[i])==0) keepace++;
  }
  if (exchangec>=5) return 0;
  if (exchangec==4) return keepace;
  return 1;
}

/* UI activate. Toggle a card for exchange, or commit the hand.
 */
 
static void poker_activate(struct battle *battle,struct player *player) {
  if (player->fingerp<0) return;
  
  if (player->fingerp>=HAND_SIZE) {
    if (!poker_validate(battle,player)) {
      bm_sound_pan(RID_sound_reject,player->who?PLAYER_PAN:-PLAYER_PAN);
      return;
    }
    player->suspense=1.000;
    int i=HAND_SIZE;
    while (i-->0) {
      if (!player->exchangev[i]) continue;
      player->hand[i]=deck_draw(&BATTLE->deck);
    }
    player->fingerp=-1;
    bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
    
    poker_hand_analyze(&player->an,player->hand);
    poker_generate_label(battle,player);
    
  } else {
    if (player->exchangev[player->fingerp]^=1) {
      bm_sound_pan(RID_sound_affirmative,player->who?PLAYER_PAN:-PLAYER_PAN);
    } else {
      bm_sound_pan(RID_sound_uicancel,player->who?PLAYER_PAN:-PLAYER_PAN);
    }
  }
}

/* Move cursor.
 */
 
static void poker_move(struct battle *battle,struct player *player,int d) {
  if (player->fingerp<0) return;
  player->fingerp+=d;
  if (player->fingerp<0) player->fingerp=HAND_SIZE;
  else if (player->fingerp>HAND_SIZE) player->fingerp=0;
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else {
    if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) poker_activate(battle,player);
    if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) poker_move(battle,player,-1);
    if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) poker_move(battle,player,1);
  }
}

/* Choose the next move for a CPU player.
 * Returns 0..HAND_SIZE.
 */
 
static int poker_picker(struct battle *battle,struct player *player) {

  /* Straight or better, keep it, don't exchange anything.
   * This includes four of a kind, but in that case, exchanging the non-participant can't produce a better hand.
   */
  if (player->an.handid>=HANDID_STRAIGHT) return HAND_SIZE;

  /* Everything below straight has a simple per-hand rule.
   */
  switch (player->an.handid) {

    /* High card.
     * Exchange our three lowest ranks.
     */
    case HANDID_CARD: {
        int choice=HAND_SIZE;
        int choicerank=14;
        int exchangec=0;
        int i=0;
        for (;i<HAND_SIZE;i++) {
          if (player->exchangev[i]) {
            exchangec++;
            continue;
          }
          int rank=RANK_FROM_CARDID(player->hand[i]);
          if (rank<choicerank) {
            choice=i;
            choicerank=rank;
          }
        }
        if (exchangec>=3) return HAND_SIZE;
        return choice;
      }
      
    /* Pair.
     * Exchange the three that aren't participating.
     */
    case HANDID_PAIR: {
        int i=0;
        for (;i<HAND_SIZE;i++) {
          if (RANK_FROM_CARDID(player->hand[i])==player->an.toprank) continue; // participant
          if (player->exchangev[i]) continue; // already marked
          break; // exchange this one
        }
        return i;
      }
      
    /* Two pair.
     * Exchange the one non-particpant.
     */
    case HANDID_TWOPAIR: {
        uint8_t rankv[5];
        int i=5;
        while (i-->0) rankv[i]=RANK_FROM_CARDID(player->hand[i]);
        for (i=5;i-->0;) {
          int ispair=0;
          int j=5; while (j-->0) {
            if ((i!=j)&&(rankv[i]==rankv[j])) {
              ispair=1;
              break;
            }
          }
          if (!ispair) {
            if (player->exchangev[i]) return HAND_SIZE;
            return i;
          }
        }
      } break;
    
    /* Three of a kind.
     * Exchange the two non-participants.
     */
    case HANDID_THREE: {
        int i=5; while (i-->0) {
          int rank=RANK_FROM_CARDID(player->hand[i]);
          if (rank==player->an.toprank) continue;
          if (player->exchangev[i]) continue;
          return i;
        }
      } break;
  }
  return HAND_SIZE;
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {

  if (player->cpuclock>0.0) {
    player->cpuclock-=elapsed;
    return;
  }
  player->cpuclock=0.400+((rand()&0xffff)*0.100)/65535.0;
  
  if (player->cpunext<0) {
    player->cpunext=poker_picker(battle,player);
  }
  
  if (player->cpunext<player->fingerp) poker_move(battle,player,-1);
  else if (player->cpunext>player->fingerp) poker_move(battle,player,1);
  else {
    poker_activate(battle,player);
    player->cpuclock+=0.600+((rand()&0xffff)*0.300)/65535.0;
    player->cpunext=-1;
  }
}

/* Update.
 */
 
static void _poker_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->suspense>0.0) {
      if ((player->suspense-=elapsed)<=0.0) {
        player->finished=1;
        memset(player->exchangev,0,sizeof(player->exchangev));
      }
    } else if (player->fingerp<0) {
      // Committed. Probly nothing further to update.
    } else {
      if (player->human) player_update_man(battle,player,elapsed,g_input[player->human],g_pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
  }
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  
  if (l->finished&&r->finished) {
    l->exposed=1;
    r->exposed=1;
  }
  
  if (battle->outcome==-2) {
    if (l->finished&&r->finished) {
      battle->outcome=poker_hand_compare(r->hand,l->hand); // Flipped, because it returns the better hand.
    }
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {
  const int xspacing=NS_sys_tilesize*3+1;
  int x=15;
  int y=player->y-(NS_sys_tilesize*2);
  int i=0;
  for (;i<HAND_SIZE;i++,x+=xspacing) {
    int cy=y;
    if (player->exchangev[i]) {
      if (player->who) cy+=6; else cy-=6;
    }
    uint8_t cardid=player->hand[i];
    if (!player->exposed) cardid=0xff;
    else if ((player->suspense>0.0)&&player->exchangev[i]) cardid=0xff;
    card_render(x,cy,cardid);
    if (i==player->fingerp) graf_tile(g_graf,x+NS_sys_tilesize+(NS_sys_tilesize>>1),player->fingery,player->finger_tileid,player->finger_xform);
  }
  graf_tile(g_graf,x+10,player->y,(player->fingerp>=0)?0x23:0x43,0);
  if (player->fingerp==HAND_SIZE) {
    y=player->y;
    if (player->who) y-=10; else y+=10;
    graf_tile(g_graf,x+10,y,player->finger_tileid,player->finger_xform);
  }
  
  // Show the hand's name if we have it and it's all exposed.
  if (player->lbltexid&&player->exposed&&(player->suspense<=0.0)&&(player->fingerp<0)) {
    x=(FBW>>1)-(player->lblw>>1);
    y=player->who?3:(FBH-player->lblh-3);
    graf_set_input(g_graf,player->lbltexid);
    graf_decal(g_graf,x,y,0,0,player->lblw,player->lblh);
  }
}

/* Render.
 */
 
static void _poker_render(struct battle *battle) {
  graf_fill_rect(g_graf,0,0,FBW,FBH,0x044c1dff);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
}

/* Type definition.
 */
 
const struct battle_type battle_type_poker={
  .name="poker",
  .objlen=sizeof(struct battle_poker),
  .id=106,
  .strix_name=355,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_horz_a,
  .imageid_default=0,
  .del=_poker_del,
  .init=_poker_init,
  .update=_poker_update,
  .render=_poker_render,
};

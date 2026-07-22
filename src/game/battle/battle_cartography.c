/* battle_cartography.c
 */

#include "game/bellacopia.h"

#define MAPW 133
#define MAPH 88
#define REGIONC 12
#define REGIONCOLC 4
#define REGIONROWC 3
#define BORDER_LIMIT 50 /* Absolute upper bound is (REGIONC*(REGIONC-1)) but I expect topologically speaking, it's lower than that. Realistically should be 40 or so. */

#define PX(rgba) ({ \
  uint32_t _rgba=(rgba); \
  ((_rgba>>24)|((_rgba&0xff0000)>>8)|((_rgba&0xff00)<<8)|(_rgba<<24)); \
})

static const uint32_t palette[4]={
  0xaa1e3dff,
  0x43689cff,
  0x40952bff,
  0xad992aff,
};

struct battle_cartography {
  struct battle hdr;
  
  /* Reference map contains 8-bit cells: Zero for the borders (or unclaimed space, during generation).
   * Otherwise each region has its own cell value, assigned consecutively from one.
   */
  uint8_t *ref;
  
  /* RGBA scratch space.
   */
  uint32_t *scratch;
  
  /* Focus point in the map, per region.
   */
  struct point { int x,y; } focusv[1+REGIONC];
  
  /* (higher<<8)|lower, recording all regions that touch each other.
   */
  uint16_t borderv[BORDER_LIMIT];
  int borderc;
  
  double playclock;
  
  struct player {
    int who; // My index in this list.
    int human; // 0 for CPU, or the input index.
    double skill; // 0..1, reverse of each other.
    uint32_t color;
    uint8_t tileid;
    uint32_t ctab[REGIONC+1]; // [0] unused. Rest are chosen color for each region, or zero if not selected yet. NB: Pixels, not RGBA.
    int ctabp;
    int texid;
    int handp;
    int blackout;
    double runtime;
    int errorv[1+REGIONC]; // Nonzero to mark this region as an error, after score tabulation.
    int plan[1+REGIONC]; // CPU. Value is (handp) 0..3
    double thinkclock;
    double thinktime;
  } playerv[2];
};

#define BATTLE ((struct battle_cartography*)battle)

/* Delete.
 */
 
static void _cartography_del(struct battle *battle) {
  if (BATTLE->ref) free(BATTLE->ref);
  if (BATTLE->scratch) free(BATTLE->scratch);
  struct player *player=BATTLE->playerv;
  int i=2; for (;i-->0;player++) {
    egg_texture_del(player->texid);
  }
}

/* Borders list.
 */
 
static int cartography_borderv_search(const struct battle *battle,uint16_t v) {
  int lo=0,hi=BATTLE->borderc;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    uint16_t q=BATTLE->borderv[ck];
         if (v<q) hi=ck;
    else if (v>q) lo=ck+1;
    else return ck;
  }
  return -lo-1;
}

static int cartography_borderv_insert(struct battle *battle,int p,uint16_t v) {
  if ((p<0)||(p>BATTLE->borderc)) return -1;
  if (BATTLE->borderc>=BORDER_LIMIT) return -1;
  uint16_t *dst=BATTLE->borderv+p;
  memmove(dst+1,dst,sizeof(uint16_t)*(BATTLE->borderc-p));
  *dst=v;
  BATTLE->borderc++;
  return 0;
}

/* Nonzero if this box is in bounds and every pixel is zero.
 * Negative sizes are permitted.
 */
 
static int cartography_rect_all_zero(const uint8_t *map,int x,int y,int w,int h) {
  if (w<0) { x+=w; w=-w; }
  if (h<0) { y+=h; h=-h; }
  if ((x<0)||(y<0)||(w<1)||(h<1)||(x>MAPW-w)||(y>MAPH-h)) return 0;
  map+=y*MAPW+x;
  for (;h-->0;map+=MAPW) {
    const uint8_t *p=map;
    int xi=w;
    for (;xi-->0;p++) if (*p) return 0;
  }
  return 1;
}

/* Dilate one 8-bit image into another.
 * The outer border won't be touched, we assume you're going to overwrite it.
 * We run regions all the way into each other. A border-defining pass is required after.
 * Returns 1 if anything changed or 0 if no changes were made.
 */
 
static int cartography_dilate(uint8_t *dst,const uint8_t *src) {
  int result=0;
  dst+=MAPW+1;
  src+=MAPW+1;
  int yi=MAPH-2;
  for (;yi-->0;dst+=MAPW,src+=MAPW) {
    uint8_t *dstp=dst;
    const uint8_t *srcp=src;
    int xi=MAPW-2;
    for (;xi-->0;dstp++,srcp++) {
    
      // Nonzero: Preserve.
      if (*srcp) {
        *dstp=*srcp;
        continue;
      }
      
      // Read cardinal neighbors.
      uint8_t n=srcp[-MAPW];
      uint8_t w=srcp[-1];
      uint8_t e=srcp[1];
      uint8_t s=srcp[MAPW];
      
      // Take any nonzero value from among my neighbors. If they're all zero, I stay zero.
           if (n) { *dstp=n; result=1; }
      else if (w) { *dstp=w; result=1; }
      else if (e) { *dstp=e; result=1; }
      else if (s) { *dstp=s; result=1; }
      else *dstp=0;
    }
  }
  return result;
}

/* Generate the reference map.
 */
 
static int cartography_generate_ref(struct battle *battle) {

  /* Start with all zeroes.
   */
  if (!BATTLE->ref) {
    if (!(BATTLE->ref=malloc(MAPW*MAPH))) return -1;
  }
  memset(BATTLE->ref,0,MAPW*MAPH);
  
  uint32_t seed=get_rand_seed();
  
  /* Pick a seed point for each region, each with some breathing room.
   * Record the seed points.
   * To ensure good separation, put each point in its own gross cell (REGIONCOLC,REGIONROWC).
   * To ensure separation is not *too* good, fuzz it out a little from there.
   */
  const int colw=MAPW/REGIONCOLC;
  const int rowh=MAPH/REGIONROWC;
  struct point seedptv[REGIONC];
  int i=REGIONC;
  while (i-->0) {
    int col=i%REGIONCOLC;
    int row=i/REGIONCOLC;
    int x=col*colw+(colw>>1);
    int y=row*rowh+(rowh>>1);
    x+=(rand()%(colw>>1))-(colw>>2);
    y+=(rand()%(rowh>>1))-(rowh>>2);
    BATTLE->ref[y*MAPW+x]=i+1;
    seedptv[i]=(struct point){x,y};
    BATTLE->focusv[i+1]=(struct point){x,y};
  }
  
  /* Drunk-walk from each seed point to give it a fun shape.
   * Advance only if there's plenty of space in that direction -- don't touch your neighbors.
   * There's a slight risk that one region will enclose another, but that's actually ok. The four-color theorem permits enclaves (just not exclaves).
   * After this pass, we have a kind of petri dish of bacteria samples, funny to look at.
   */
  for (i=REGIONC;i-->0;) {
    uint8_t color=i+1;
    int seedx=seedptv[i].x;
    int seedy=seedptv[i].y;
    int limit=15+rand()%30;
    while (limit-->0) {
      struct point candidatev[8];
      int candidatec=0;
      #define CK(_dx,_dy) { \
        int _x=seedx+_dx,_y=seedy+_dy; \
        if (cartography_rect_all_zero(BATTLE->ref,_x,_y,_dx*3,_dy*3)) { \
          candidatev[candidatec++]=(struct point){_x,_y}; \
        } \
      }
      CK(-1,-1)
      CK(-1,0)
      CK(-1,1)
      CK(0,-1)
      CK(0,1)
      CK(1,-1)
      CK(1,0)
      CK(1,1)
      #undef CK
      if (candidatec<1) break; // Painted into a corner or something. No worries, we're done.
      int p=rand()%candidatec;
      seedx=candidatev[p].x;
      seedy=candidatev[p].y;
      BATTLE->ref[seedy*MAPW+seedx]=color;
    }
  }
  
  /* Dilate until all regions have reached their maximum size.
   * Do not create any cardinal neighbors between regions. We're preserving a skinny border of zeroes between them all.
   * Oh shoot, we need a second buffer for this.
   * Two passes at a time, so the final map always ends up in (BATTLE->ref).
   */
  uint8_t *ref2=calloc(MAPW,MAPH);
  if (!ref2) return -1;
  for (;;) {
    if (!cartography_dilate(ref2,BATTLE->ref)) break;
    if (!cartography_dilate(BATTLE->ref,ref2)) {
      memcpy(BATTLE->ref,ref2,MAPW*MAPH);
      break;
    }
  }
  free(ref2);
  
  /* Detect edges.
   * Put zeroes in the image between regions, and also note the neighbor relationships.
   * Nothing inside the border should be zero when we start. But beware that we will examine the border too.
   */
  #define NEIGHBORS(a,b) { \
    uint16_t v; \
    if (a>b) v=(a<<8)|b; \
    else v=(b<<8)|a; \
    int bp=cartography_borderv_search(battle,v); \
    if (bp<0) cartography_borderv_insert(battle,-bp-1,v); \
  }
  uint8_t *row=BATTLE->ref+MAPW+1;
  int yi=MAPH-2;
  for (;yi-->0;row+=MAPW) {
    uint8_t *p=row;
    int xi=MAPW-2;
    for (;xi-->0;p++) {
      if (p[0]&&p[1]&&(p[0]!=p[1])) {
        NEIGHBORS(p[0],p[1]);
        *p=0;
      } else if (p[0]&&p[MAPW]&&(p[0]!=p[MAPW])) {
        // Verticals touch the other instead of this. With that little correction, we make skinny lines.
        // Otherwise they'd be fat where slope is positive and skinny where negative.
        NEIGHBORS(p[0],p[MAPW])
        p[MAPW]=0;
      }
    }
  }
  #undef NEIGHBORS
  
  return 0;
}

/* Redraw the entire map for one player, including their color table selections.
 */
 
static void cartography_redraw_player_map(struct battle *battle,struct player *player) {
  uint32_t *dst=BATTLE->scratch;
  const uint8_t *src=BATTLE->ref;
  int i=MAPW*MAPH;
  for (;i-->0;dst++,src++) {
    if (*src<player->ctabp) *dst=player->ctab[*src];
    else *dst=PX(0xd9ba9fff); // The special "unvisited" color.
  }
  egg_texture_load_raw(player->texid,MAPW,MAPH,MAPW<<2,BATTLE->scratch,MAPW*MAPH*4);
}

/* Calculate player's score and populate (errorv).
 * Score is the count of errors; lower is better.
 * This is called at end of game, and also repeatedly during CPU plan generation.
 */
 
static int player_score(struct battle *battle,struct player *player) {
  int score=0;
  memset(player->errorv,0,sizeof(player->errorv));
  int a=1; for (;a<=REGIONC;a++) {
    if (a>=player->ctabp) { // Player didn't finish; blank regions are an error.
      score++;
      player->errorv[a]=1;
    } else {
      // Consider all neighbors of (a), and if any has the same color, (a) is in error.
      // We're visiting every region regardless, so don't mark (b) in this pass.
      const uint16_t *border=BATTLE->borderv;
      int i=BATTLE->borderc;
      for (;i-->0;border++) {
        int ba=((*border)>>8)&0xff;
        int bb=(*border)&0xff;
        if (ba==a) {
          if ((bb<player->ctabp)&&(player->ctab[a]==player->ctab[bb])) {
            score++;
            player->errorv[a]=1;
            break;
          }
        } else if (bb==a) {
          if ((ba<player->ctabp)&&(player->ctab[a]==player->ctab[ba])) {
            score++;
            player->errorv[a]=1;
            break;
          }
        }
      }
    }
  }
  return score;
}

/* With (player->pan) composed of bitmaps, remove the bits for the given region from that region's neighbors.
 */
 
static void player_eliminate_neighbor_plan_bits(struct battle *battle,struct player *player,int rid) {
  if ((rid<1)||(rid>REGIONC)) return;
  int nmask=~player->plan[rid];
  const uint16_t *border=BATTLE->borderv;
  int i=BATTLE->borderc;
  for (;i-->0;border++) {
    int ba=((*border)>>8)&0xff;
    int bb=(*border)&0xff;
    if (ba==rid) player->plan[bb]&=nmask;
    else if (bb==rid) player->plan[ba]&=nmask;
  }
}

/* Generate plan for a CPU player.
 * We'll try to make the stipulated count of errors.
 */
 
static void player_generate_plan(struct battle *battle,struct player *player,int errorc) {
  
  /* Start by generating a perfect plan. ...actually I think this algorithm will make mistakes. But that's ok!
   * Fill (plan) with bitmaps of (1<<handp), the allowable colors for each region.
   * And keep a list (pendingv) of regions with more than one bit set.
   */
  int i=1; for (;i<=REGIONC;i++) player->plan[i]=0xf;
  int pendingv[REGIONC];
  for (i=0;i<REGIONC;i++) pendingv[i]=i+1;
  int pendingc=REGIONC;
  
  /* While multi-bit regions exist, pick one at random and commit to one of its bits at random.
   * Then eliminate that bit from all neighbor regions.
   */
  while (pendingc>0) {
    int p=rand()%pendingc;
    int rid=pendingv[p];
    pendingc--;
    memmove(pendingv+p,pendingv+p+1,sizeof(int)*(pendingc-p));
    int bits=player->plan[rid];
    int candidatev[4];
    int candidatec=0;
    if (bits&1) candidatev[candidatec++]=1;
    if (bits&2) candidatev[candidatec++]=2;
    if (bits&4) candidatev[candidatec++]=4;
    if (bits&8) candidatev[candidatec++]=8;
    if (candidatec<2) continue; // oops this one was not actually pending. It's already removed, so just move on. This will happen, because bit removal is not recursive.
    p=rand()%candidatec;
    player->plan[rid]=candidatev[p];
    player_eliminate_neighbor_plan_bits(battle,player,rid);
  }
  
  /* Turn the bitmaps into color indices.
   * If we ended up with something invalid, eg empty, pick a color at random.
   */
  for (i=1;i<=REGIONC;i++) {
    switch (player->plan[i]) {
      case 1: player->plan[i]=0; break;
      case 2: player->plan[i]=1; break;
      case 4: player->plan[i]=2; break;
      case 8: player->plan[i]=3; break;
      default: player->plan[i]=rand()&3; break;
    }
  }
  
  /* Count the errors.
   * While valid regions exist and error count is less than desired, pick a valid region at random and change it randomly to something else.
   * Stop when we have enough or too many errors.
   */
  for (;;) {
    memcpy(player->ctab,player->plan,sizeof(player->plan));
    player->ctabp=REGIONC+1;
    int have_errorc=player_score(battle,player);
    player->ctabp=1;
    if (have_errorc>=errorc) return; // Great, done!
    // Pick a valid region at random and change it. player_score() repopulates (errorv), we can use that.
    int validc=0;
    for (i=1;i<=REGIONC;i++) if (!player->errorv[i]) validc++;
    if (!validc) return; // Nothing is valid but we don't have enough errors? Weird. Get out.
    int choice=rand()%validc;
    for (i=1;i<=REGIONC;i++) {
      if (player->errorv[i]) continue;
      if (!choice--) {
        choice=rand()%3;
        if (choice>=player->plan[i]) choice++;
        player->plan[i]=choice;
        break;
      }
    }
  }
}

/* Init player.
 */
 
static void player_init(struct battle *battle,struct player *player,int human,int face) {
  if (player==BATTLE->playerv) { // Left.
    player->who=0;
  } else { // Right.
    player->who=1;
  }
  player->texid=egg_texture_new();
  if (player->human=human) { // Human.
    player->blackout=1;
  } else { // CPU.
    player->thinktime=0.300*player->skill+0.800*(1.0-player->skill);
    player->thinkclock=player->thinktime+0.250;
    int errorc=(int)(2.0*player->skill+10.0*(1.0-player->skill));
    if (errorc<2) errorc=2; else if (errorc>10) errorc=10;
    player_generate_plan(battle,player,errorc);
  }
  // Prep ctab after generating the plan; player_generate_plan uses it as scratch
  player->ctab[0]=PX(0x43210eff); // Borders and such.
  player->ctabp=1;
  switch (face) {
    case NS_face_monster: {
        player->color=0x11491dff;
        player->tileid=0x5d;
      } break;
    case NS_face_dot: {
        player->color=0x411775ff;
        player->tileid=0x5b;
      } break;
    case NS_face_princess: {
        player->color=0x0d3ac1ff;
        player->tileid=0x5c;
      } break;
  }
}

/* New.
 */
 
static int _cartography_init(struct battle *battle) {
  
  // Generate map before initializing players; CPU players need the metadata.
  if (!(BATTLE->scratch=malloc(MAPW*MAPH*4))) return -1;
  if (cartography_generate_ref(battle)<0) return -1;
  
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  battle_normalize_bias(&l->skill,&r->skill,battle);
  player_init(battle,l,battle->args.lctl,battle->args.lface);
  player_init(battle,r,battle->args.rctl,battle->args.rface);
  
  cartography_redraw_player_map(battle,l);
  cartography_redraw_player_map(battle,r);
  
  BATTLE->playclock=30.0;
  
  return 0;
}

/* Choose the color I'm pointing at.
 */
 
static void player_activate(struct battle *battle,struct player *player) {
  if (player->ctabp>REGIONC) return;
  bm_sound_pan(RID_sound_uiactivate,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->ctab[player->ctabp++]=PX(palette[player->handp]);
  cartography_redraw_player_map(battle,player);
}

/* Move finger.
 */
 
static void player_move(struct battle *battle,struct player *player,int d) {
  bm_sound_pan(RID_sound_uimotion,player->who?PLAYER_PAN:-PLAYER_PAN);
  player->handp+=d;
  if (player->handp<0) player->handp=3;
  else if (player->handp>3) player->handp=0;
}

/* Update human player.
 */
 
static void player_update_man(struct battle *battle,struct player *player,double elapsed,int input,int pvinput) {
  if ((input&EGG_BTN_LEFT)&&!(pvinput&EGG_BTN_LEFT)) player_move(battle,player,-1);
  else if ((input&EGG_BTN_RIGHT)&&!(pvinput&EGG_BTN_RIGHT)) player_move(battle,player,1);
  else if (player->blackout) {
    if (!(input&EGG_BTN_SOUTH)) player->blackout=0;
  } else if ((input&EGG_BTN_SOUTH)&&!(pvinput&EGG_BTN_SOUTH)) player_activate(battle,player);
}

/* Update CPU player.
 */
 
static void player_update_cpu(struct battle *battle,struct player *player,double elapsed) {
  if (player->ctabp>REGIONC) return;
  if ((player->thinkclock-=elapsed)>0.0) return;
  player->thinkclock+=player->thinktime;
  int dst=player->plan[player->ctabp];
  if (player->handp<dst) player_move(battle,player,1);
  else if (player->handp>dst) player_move(battle,player,-1);
  else player_activate(battle,player);
}

/* Update.
 */
 
static void _cartography_update(struct battle *battle,double elapsed) {
  if (battle->outcome>-2) return;
  
  BATTLE->playclock-=elapsed;
  
  struct player *player=BATTLE->playerv;
  int i=2;
  for (;i-->0;player++) {
    if (player->ctabp<=REGIONC) {
      player->runtime+=elapsed;
      if (player->human) player_update_man(battle,player,elapsed,g.input[player->human],g.pvinput[player->human]);
      else player_update_cpu(battle,player,elapsed);
    }
  }
  
  /* Game is over when both players are done, or playclock expires.
   * Zero or one point per region: Zero if unvisited or touches a neighbor of the same color.
   * If the coloring score is a tie, break it by (runtime).
   * Two dead men will tie, but otherwise it's unlikely.
   */
  struct player *l=BATTLE->playerv;
  struct player *r=l+1;
  if (((l->ctabp>REGIONC)&&(r->ctabp>REGIONC))||(BATTLE->playclock<=0.0)) {
    int lscore=player_score(battle,l);
    int rscore=player_score(battle,r);
    if (lscore>rscore) battle->outcome=-1;
    else if (lscore<rscore) battle->outcome=1;
    else if (l->runtime<r->runtime) battle->outcome=1;
    else if (l->runtime>r->runtime) battle->outcome=-1;
    else battle->outcome=0;
  }
}

/* Render player.
 */
 
static void player_render(struct battle *battle,struct player *player) {

  // The map.
  int dstx=player->who?((FBW*3)>>2):(FBW>>2);
  dstx-=MAPW>>1;
  int dsty=(FBH>>1)-(MAPH>>1);
  graf_set_input(&g.graf,player->texid);
  graf_decal(&g.graf,dstx,dsty,0,0,MAPW,MAPH);
  
  // Error indicators.
  graf_set_image(&g.graf,RID_image_battle_tundra);
  if (battle->outcome>-2) {
    int i=1; for (;i<=REGIONC;i++) {
      if (!player->errorv[i]) continue;
      graf_fancy(&g.graf,dstx+BATTLE->focusv[i].x,dsty+BATTLE->focusv[i].y,0x5f,0,0,NS_sys_tilesize,0,0x808080ff);
    }
  }
  
  // Four paint buckets.
  graf_fancy(&g.graf,dstx+20,dsty+MAPH+10,0x5e,0,0,NS_sys_tilesize,0,palette[0]);
  graf_fancy(&g.graf,dstx+50,dsty+MAPH+10,0x5e,0,0,NS_sys_tilesize,0,palette[1]);
  graf_fancy(&g.graf,dstx+80,dsty+MAPH+10,0x5e,0,0,NS_sys_tilesize,0,palette[2]);
  graf_fancy(&g.graf,dstx+110,dsty+MAPH+10,0x5e,0,0,NS_sys_tilesize,0,palette[3]);
  
  // Two fingers. Use fancies to share the paint buckets' batch.
  if ((player->ctabp<=REGIONC)&&(BATTLE->playclock>0.0)) {
    graf_fancy(&g.graf,dstx+20+player->handp*30,dsty+MAPH+25,player->tileid,0,0,NS_sys_tilesize,0,0x808080ff);
    graf_fancy(&g.graf,dstx+BATTLE->focusv[player->ctabp].x,dsty+BATTLE->focusv[player->ctabp].y+10,player->tileid,0,0,NS_sys_tilesize,0,player->color);
  }
}

/* Render.
 */
 
static void _cartography_render(struct battle *battle) {
  graf_fill_rect(&g.graf,0,0,FBW,FBH,0x6f6053ff);
  player_render(battle,BATTLE->playerv+0);
  player_render(battle,BATTLE->playerv+1);
  
  if (BATTLE->playclock>0.0) {
    int s=(int)(BATTLE->playclock+0.999);
    if (s<1) s=1; else if (s>99) s=99;
    graf_set_image(&g.graf,RID_image_fonttiles);
    if (s>=10) {
      graf_tile(&g.graf,(FBW>>1)-4,22,'0'+s/10,0);
      graf_tile(&g.graf,(FBW>>1)+4,22,'0'+s%10,0);
    } else {
      graf_tile(&g.graf,FBW>>1,22,'0'+s,0);
    }
    if ((BATTLE->playclock<5.0)&&!(g.framec&0x10)) {
      graf_tile(&g.graf,(FBW>>1)-12,22,'!',0);
      graf_tile(&g.graf,(FBW>>1)+12,22,'!',0);
    }
  }
}

/* Type definition.
 */
 
const struct battle_type battle_type_cartography={
  .name="cartography",
  .objlen=sizeof(struct battle_cartography),
  .id=NS_battle_cartography,
  .strix_name=296,
  .no_article=0,
  .no_contest=0,
  .no_timeout=0,
  .support_pvp=1,
  .support_cvc=1,
  .update_during_report=0,
  .input=battle_input_horz_a,
  .imageid_default=0,
  .del=_cartography_del,
  .init=_cartography_init,
  .update=_cartography_update,
  .render=_cartography_render,
};

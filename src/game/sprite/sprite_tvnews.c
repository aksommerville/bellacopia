/* sprite_tvnews.c
 * Just an animated decoration that occasionally shows the latest poll results.
 * I kind of got carried away.
 */
 
#include "game/bellacopia.h"

#define STAGE_TIME 6.000

#define STAGE_POLLS 0
#define STAGE_CAR 1
#define STAGE_POTION 2
#define STAGE_CANDY 3
#define STAGE_COUNT 4

#define BARW 18

struct sprite_tvnews {
  struct sprite hdr;
  int stage;
  double stageclock;
  int barw_dot;
  int barw_cat;
  int polls_available;
};

#define SPRITE ((struct sprite_tvnews*)sprite)

static int _tvnews_init(struct sprite *sprite) {

  // When the election quest is underway, we show polls interleaved with ads. Otherwise just ads.
  if (store_get_fld(NS_fld_election_start)&&!store_get_fld(NS_fld_mayor)) {
    SPRITE->stage=rand()%STAGE_COUNT;
    SPRITE->polls_available=1;
  } else {
    SPRITE->stage=1+rand()%(STAGE_COUNT-1);
  }
  SPRITE->stageclock=((rand()&0xffff)*STAGE_TIME)/65535.0;
  
  // Gather numbers for the infographic. They can't change while you're inside the BoE.
  const int endorsementc=6;
  int dotc=0;
  if (store_get_fld(NS_fld_endorse_food)) dotc++;
  if (store_get_fld(NS_fld_endorse_public)) dotc++;
  if (store_get_fld(NS_fld_endorse_athlete)) dotc++;
  if (store_get_fld(NS_fld_endorse_hospital)) dotc++;
  if (store_get_fld(NS_fld_endorse_casino)) dotc++;
  if (store_get_fld(NS_fld_endorse_labor)) dotc++;
  SPRITE->barw_dot=(dotc*BARW)/endorsementc;
  if (SPRITE->barw_dot<2) SPRITE->barw_dot=2;
  SPRITE->barw_cat=((endorsementc-dotc)*BARW)/endorsementc;
  if (SPRITE->barw_cat<2) SPRITE->barw_cat=2;
  
  return 0;
}

static void _tvnews_update(struct sprite *sprite,double elapsed) {
  // When stageclock expires, alternate between polls and a random ad.
  // Polls are stage zero; all nonzeroes are ads.
  // But if there's no election underway, just ads.
  if ((SPRITE->stageclock-=elapsed)<=0.0) {
    SPRITE->stageclock+=STAGE_TIME;
    // Both of these flags can change while you're in the Board of Elections, so we have to check them dynamically after all.
    SPRITE->polls_available=(store_get_fld(NS_fld_election_start)&&!store_get_fld(NS_fld_mayor));
    if ((SPRITE->stage==STAGE_POLLS)||!SPRITE->polls_available) {
      SPRITE->stage=1+rand()%(STAGE_COUNT-1);
    } else {
      SPRITE->stage=STAGE_POLLS;
    }
  }
}

static uint8_t tvnews_get_tile_POLLS(struct sprite *sprite) {
  // 0x00,0x10,0x20 = "latest polls", blank, infographic
  if (SPRITE->stageclock>=5.0) return 0x10;
  if (SPRITE->stageclock>=4.0) return 0x00;
  return 0x20;
}

static void tvnews_post_render_POLLS(struct sprite *sprite,int x,int y) {
  const int barh=4;
  const uint32_t dot_color=0x3b0c66ff;
  const uint32_t cat_color=0xc76f15ff;
  double t;
  if (SPRITE->stageclock<=2.0) t=1.0;
  else t=1.0-(SPRITE->stageclock-2.0)/2.0;
  // Translate (x,y) to the origin of the left tile, helps me think straight.
  x-=NS_sys_tilesize;
  y-=NS_sys_tilesize>>1;
  // Then to Dot's bar.
  x+=9;
  y+=3;
  graf_fill_rect(&g.graf,x,y,lround(SPRITE->barw_dot*t),barh,dot_color);
  // And then the Cat's.
  y+=5;
  graf_fill_rect(&g.graf,x,y,lround(SPRITE->barw_cat*t),barh,cat_color);
}

static uint8_t tvnews_get_tile_CAR(struct sprite *sprite) {
  // 0x30,0x40,0x50,0x60 = blank, pic, "buy car!", "BUY CAR!"
  int frame=((int)(SPRITE->stageclock*6.0))&1;
  if (SPRITE->stageclock>=3.0) {
    return frame?0x30:0x40;
  }
  return frame?0x50:0x60;
}

static uint8_t tvnews_get_tile_POTION(struct sprite *sprite) {
  // 0x70,0x80,0x90,0xa0,0xb0,0xc0 = 3 stages of two frames each
  int frame=((int)(SPRITE->stageclock*4.0))&1;
  if (SPRITE->stageclock>=4.0) return 0x70+frame*16;
  if (SPRITE->stageclock>=2.5) return 0x90+frame*16;
  return 0xb0+frame*16;
}

static uint8_t tvnews_get_tile_CANDY(struct sprite *sprite) {
  // 0xd0,0xe0,0xf0 = "you want", pic, "CANDY!"
  if (SPRITE->stageclock>=3.5) return 0xd0;
  int frame=((int)(SPRITE->stageclock*7.0))&1;
  return frame?0xe0:0xf0;
}

static void _tvnews_render(struct sprite *sprite,int x,int y) {
  uint8_t tileid=0x00;
  switch (SPRITE->stage) {
    case STAGE_POLLS: tileid=tvnews_get_tile_POLLS(sprite); break;
    case STAGE_CAR: tileid=tvnews_get_tile_CAR(sprite); break;
    case STAGE_POTION: tileid=tvnews_get_tile_POTION(sprite); break;
    case STAGE_CANDY: tileid=tvnews_get_tile_CANDY(sprite); break;
  }
  graf_set_image(&g.graf,sprite->imageid);
  graf_tile(&g.graf,x-(NS_sys_tilesize>>1),y,tileid,0);
  graf_tile(&g.graf,x+(NS_sys_tilesize>>1),y,tileid+1,0);
  if (tileid==0x20) { // The infographic with animated poll results.
    tvnews_post_render_POLLS(sprite,x,y);
  }
}

const struct sprite_type sprite_type_tvnews={
  .name="tvnews",
  .objlen=sizeof(struct sprite_tvnews),
  .init=_tvnews_init,
  .update=_tvnews_update,
  .render=_tvnews_render,
};

#include "bellacopia.h"

#define STATUEMAZE_RULE_NONE 0 /* No danger. */
#define STATUEMAZE_RULE_ALL  1 /* All directions forbidden. */
#define STATUEMAZE_RULE_HORZ 2 /* Left and right forbidden. */
#define STATUEMAZE_RULE_UP   3 /* Only up forbidden. */
#define STATUEMAZE_RULE_DOWN 4 /* Only down forbidden. */

/* Private globals.
 */
 
static struct {
  
  /* Seed and state of our private PRNG.
   * If (seed) doesn't match NS_fld16_statuemaze_seed, we must rebuild.
   */
  int seed;
  int prng;
  
  /* STATUEMAZE_RULE_* indexed by STATUEMAZE_APPEARANCE_*, both in 0..4.
   * Any of the statues can have any of the rules applied to it; they all get applied exactly once.
   */
  int rule_by_appearance[5];
  int appearance_by_rule[5];
  
  /* The final puzzle, in its purest form.
   */
  uint8_t grid[STATUEMAZE_COLC*STATUEMAZE_ROWC];
  
} sm={0};

/* Private PRNG.
 */
 
static int sm_rand(int range) {
  if (range<2) return 0;
  sm.prng^=sm.prng<<13;
  sm.prng^=sm.prng>>17;
  sm.prng^=sm.prng<<5;
  return (sm.prng&0x7fffffff)%range;
}

/* Blocked bits, from our enumerated rules.
 */
 
static uint8_t statuemaze_bits_for_rule(int rule) {
  switch (rule) {
    case STATUEMAZE_RULE_NONE: return 0;
    case STATUEMAZE_RULE_ALL: return STATUEMAZE_N|STATUEMAZE_S|STATUEMAZE_W|STATUEMAZE_E;
    case STATUEMAZE_RULE_HORZ: return STATUEMAZE_W|STATUEMAZE_E;
    case STATUEMAZE_RULE_UP: return STATUEMAZE_N;
    case STATUEMAZE_RULE_DOWN: return STATUEMAZE_S;
  }
  return 0;
}

/* Require.
 */
 
void statuemaze_require() {

  /* Get our seed from the store.
   * If it's zero, generate a new one. That will only happen once per campaign.
   * Then, if it matches the seed in our private globals, we're already ready.
   */
  int trueseed=store_get_fld16(NS_fld16_statuemaze_seed);
  
  if (!trueseed) { // Must make up a new one.
    trueseed=rand()&0xffff;
    trueseed|=1; // Don't let it be zero.
    fprintf(stderr,"%s:%d: Generated fresh PRNG seed %d.\n",__FILE__,__LINE__,trueseed);
    store_set_fld16(NS_fld16_statuemaze_seed,trueseed);
  }
  if (trueseed==sm.seed) return;
  
  // Reset.
  memset(&sm,0,sizeof(sm));
  sm.seed=sm.prng=trueseed;
  
  /* Assign rules randomly to appearances.
   * There are 5! (==120) possible arrangements.
   */
  int tmp[5]={0,1,2,3,4};
  int choice=sm_rand(120);
  int appearance=5;
  while (appearance-->0) {
    int tmpp=choice%(appearance+1);
    sm.rule_by_appearance[appearance]=tmp[tmpp];
    sm.appearance_by_rule[tmp[tmpp]]=appearance;
    memmove(tmp+tmpp,tmp+tmpp+1,sizeof(int)*(4-tmpp));
  }
  
  /* For each column of the model, select a row that will remain passable.
   * There are (n+1) rows here: Caller must permit passage both above and below the statue grid.
   * Four rows. So 2 bits 5 times. Generate the whole thing in one tick of the RNG.
   * If all five columns come up equal, force them otherwise.
   */
  int passrow_template=sm_rand(1024);
  if (
    (((passrow_template>>2)&3)==(passrow_template&3))&&
    (((passrow_template>>4)&3)==(passrow_template&3))&&
    (((passrow_template>>6)&3)==(passrow_template&3))&&
    (((passrow_template>>8)&3)==(passrow_template&3))
  ) {
    passrow_template^=0x11;
  }
  
  /* Make a 9x7 grid (top and bottom rows are both navigable).
   * Each cell will be either passable, impassable, or undecided.
   */
  const uint8_t undecided=0;
  const uint8_t passable=1;
  const uint8_t impassable=2;
  #if ((STATUEMAZE_COLC!=5)||(STATUEMAZE_ROWC!=3))
    #error "hard-coded assumptions of STATUEMAZE_COLC and STATUEMAZE_ROWC"
  #endif
  uint8_t grid[9*7]={0};
  
  // Fill in the statues' positions as impassable.
  int y=1;
  for (;y<7;y+=2) {
    int x=0;
    for (;x<10;x+=2) {
      grid[y*9+x]=impassable;
    }
  }
  
  /* Trace a path left to right according to (passrow_template).
   */
  int x=0;
  y=(passrow_template&3)*2;
  passrow_template>>=2;
  grid[y*9+x]=passable;
  x++;
  while (x<9) {
    grid[y*9+x]=passable;
    int nexty=(passrow_template&3)*2;
    passrow_template>>=2;
    while (y<nexty) {
      y++;
      grid[y*9+x]=passable;
    }
    while (y>nexty) {
      y--;
      grid[y*9+x]=passable;
    }
    x++;
    grid[y*9+x]=passable;
    x++;
  }
  
  /* XXX Dump what we got so far.
   */
  if (0) {
  for (y=0;y<7;y++) {
    char tmp[20];
    int tmpc=0;
    const uint8_t *src=grid+y*9;
    for (x=0;x<9;x++,src++) {
      if (*src==undecided) { tmp[tmpc++]=' '; tmp[tmpc++]=' '; }
      else if (*src==passable) { tmp[tmpc++]='-'; tmp[tmpc++]='_'; }
      else if (*src==impassable) { tmp[tmpc++]='x'; tmp[tmpc++]='X'; }
      else { tmp[tmpc++]='?'; tmp[tmpc++]='!'; }
    }
    fprintf(stderr,"%.*s\n",tmpc,tmp);
  }
  }
  
  /* Now visit each of the statue positions and choose a rule for it.
   * We don't need to impassable every undecided cell; it's fine if there ends up being more than one solution.
   */
  uint8_t *dst=sm.grid;
  int sy=0;
  for (;sy<STATUEMAZE_ROWC;sy++) {
    int sx=0;
    for (;sx<STATUEMAZE_COLC;sx++,dst++) {
      const uint8_t *src=grid+(sy*2+1)*9+sx*2;
      // Which of the four directions are we allowed to block?
      int n=(src[-9]!=passable);
      int s=(src[9]!=passable);
      int w=((sx<=0)||(src[-1]!=passable));
      int e=((sx>=8)||(src[1]!=passable));
      // Build up a list of candidate rules.
      int candidatev[5];
      int candidatec=0;
      if (n&&s&&w&&e) candidatev[candidatec++]=STATUEMAZE_RULE_ALL;
      if (w&&e) candidatev[candidatec++]=STATUEMAZE_RULE_HORZ;
      if (n) candidatev[candidatec++]=STATUEMAZE_RULE_UP;
      if (s) candidatev[candidatec++]=STATUEMAZE_RULE_DOWN;
      if (!candidatec) candidatev[candidatec++]=STATUEMAZE_RULE_NONE; // use the NONE statue only if nothing else works.
      // Pick a rule.
      int rule=candidatev[sm_rand(candidatec)];
      int appearance=sm.appearance_by_rule[rule];
      *dst=(appearance<<4)|statuemaze_bits_for_rule(rule);
    }
  }
}

/* Get the grid.
 */

void statuemaze_get_grid(uint8_t *dst) {
  statuemaze_require();
  memcpy(dst,sm.grid,STATUEMAZE_COLC*STATUEMAZE_ROWC);
}

/* Get a message.
 */

int statuemaze_get_message(char *dst,int dsta,int msgid) {
  statuemaze_require();
  // (msgid+1) is the rule. String templates correspond to rules.
  int rule=1+(msgid&3);
  int strix_template=104+rule;
  int strix_name=109+sm.appearance_by_rule[rule];
  struct text_insertion ins={.mode='r',.r={.rid=RID_strings_dialogue,.strix=strix_name}};
  return text_format_res(dst,dsta,RID_strings_dialogue,strix_template,&ins,1);
}

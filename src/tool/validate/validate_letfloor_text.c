#include "validate_internal.h"

/* strings:item, the names of things you can have in inventory.
 * We're not dynamically generating the list of inventoriable things, since at this writing it's final.
 * (one could parse inventory.c for that, or maybe just include inventory.c? meh, not worth it)
 */
static const int strix_name[]={
  1,3,5,7,9,11,13,15,17,19,21,
  23,25,34,52,54,56,67,69,71,
  73,75,77,79,81,83,
};

/* strings:item, the letfloor clues.
 */
static const int strix_clue[]={
  110,111,
};

/* Search a sorted integer list.
 */
 
static int int_list_contains(const int *v,int c,int q) {
  int lo=0,hi=c;
  while (lo<hi) {
    int ck=(lo+hi)>>1;
    int va=v[ck];
         if (q<va) hi=ck;
    else if (q>va) lo=ck+1;
    else return 1;
  }
  return 0;
}

/* Context for main loop.
 */
 
struct context {
  int filec; // How many strings files in total?
  int langc; // How many with id 2?
  int okc; // 0..langc, how many pass validation?
};

struct solver_state {
  int cluep;
  int equipped; // Index in (namev) or <0.
  int namep; // Position in (namev[equipped]).
  char dst[16];
  int dstc;
};

/* String operations.
 */
 
struct string { const char *v; int c; };

static int normalize_string(char *v,int c) {
  int dstp=0,srcp=0;
  for (;srcp<c;srcp++) {
    char ch=v[srcp];
    if ((ch>='A')&&(ch<='Z')) v[dstp++]=ch;
    else if ((ch>='a')&&(ch<='z')) v[dstp++]=ch-0x20;
  }
  memset(v+dstp,' ',c-dstp);
  return dstp;
}

// Nonzero if everything in (clue) is present in (name), in order. ie, you can solve this clue with just this one name.
static int all_letters_contained_by_string(const char *clue,int cluec,const char *name,int namec) {
  int cluep=0,namep=0;
  for (;;) {
    if (cluep>=cluec) return 1;
    char q=clue[cluep++];
    while ((namep<namec)&&(name[namep]!=q)) namep++;
    if (namep>=namec) return 0;
    namep++;
  }
}

static int index_in_string(const char *src,int srcc,char ch) {
  int i=0;
  for (;i<srcc;i++) if (src[i]==ch) return i;
  return -1;
}

/* Solve the remainder of this clue, with a given intermediate state.
 * Recursive.
 */
 
static int solve_clue_from_state(char *msg,int msga,const char *clue,int cluec,const struct string *namev,int namec,struct solver_state *state) {

  /* If we arrive here with an empty clue, the puzzle is solved, hooray!
   * Dump the final grid state at the end of the message.
   */
  if (cluec<1) {
    int more=7+state->dstc;
    if (more>msga) return 0;
    memcpy(msg," final:",7);
    memcpy(msg+7,state->dst,state->dstc);
    return more;
  }

  struct solver_state state0=*state;

  /* Prefer to keep the current item equipped.
   * Check that string first.
   */
  if (state->equipped>=0) {
    int skipc=index_in_string(namev[state->equipped].v+state->namep,namev[state->equipped].c-state->namep,clue[0]);
    if ((skipc>=0)&&(state->dstc+skipc+1<=16)) { // Cool, got it.
      memcpy(state->dst+state->dstc,namev[state->equipped].v+state->namep,skipc+1);
      state->dstc+=skipc+1;
      state->namep+=skipc+1;
      int err=solve_clue_from_state(msg,msga,clue+1,cluec-1,namev,namec,state);
      if (err>=0) return err;
      *state=state0;
    }
  }
  
  /* Try each other name from the start.
   */
  int namep=0;
  for (;namep<namec;namep++) {
    if (namep==state->equipped) continue;
    int skipc=index_in_string(namev[namep].v,namev[namep].c,clue[0]);
    if ((skipc<0)||(state->dstc+skipc+1>16)) continue;
    memcpy(state->dst+state->dstc,namev[namep].v,skipc+1);
    state->dstc+=skipc+1;
    int msgc=namev[namep].c+1;
    if (msgc<=msga) {
      memcpy(msg,namev[namep].v,namev[namep].c);
      msg[msgc-1]=',';
    }
    state->equipped=namep;
    state->namep=skipc+1;
    int err=solve_clue_from_state(msg+msgc,msga-msgc,clue+1,cluec-1,namev,namec,state);
    if (err>=0) return msgc+err;
    *state=state0;
  }

  return -1;
}

/* Solve one clue, with the given names.
 * Returns <0 if we can't find a solution.
 * Otherwise populates (msg) with a description of the solution and returns that length.
 * A valid solution consumes no more than 16 letters total, from the start of anything in (namev), and contains all letters in (clue).
 * Input strings must be normalized first: Uppercase letters only, and no more than 16 of them.
 * *** We don't find the best solution, just a valid one. ***
 */

static int solve_clue(char *msg,int msga,const char *clue,int cluec,const struct string *namev,int namec) {

  int msgc=0;
  #define APPEND(src,srcc) { \
    int _srcc=(srcc); \
    if (msgc<=msga-_srcc) memcpy(msg+msgc,src,_srcc); \
    msgc+=_srcc; \
  }

  /* First the obvious thing: Is it reachable with a single name?
   * This is of course an edge case of the more general solution below, 
   * but it's worth checking special for, since a human being would gravitate toward single-name solutions.
   * Find all that apply.
   */
  int i=0;
  for (;i<namec;i++) {
    if (all_letters_contained_by_string(clue,cluec,namev[i].v,namev[i].c)) {
      if (msgc) APPEND(" or ",4)
      APPEND("single:",7)
      APPEND(namev[i].v,namev[i].c)
    }
  }
  if (msgc) return msgc;
  
  /* What is the general solution?
   * Something a little naive: Find the nearest occurrence of each letter, tracking the one equipped name.
   * But when there's a tie, we need to branch out.
   */
  struct solver_state state={
    .cluep=0,
    .equipped=-1,
    .namep=0,
  };
  msgc=solve_clue_from_state(msg,msga,clue,cluec,namev,namec,&state);
  if (msgc<0) return -1;
  
  #undef APPEND
  return msgc;
}

/* Validate one file in memory.
 * Note that (src) is not "const" -- we normalize strings in place, as an optimization.
 */
 
static int validate_letfloor_text_1(char *src,int srcc,const char *path) {
  
  /* First, parse the text and compose two lists.
   * Strings files are allowed to contain JSON instead of raw text, but that won't be the case for any of these.
   */
  struct string namev[26],cluev[10];
  int namec=0,cluec=0;
  struct sr_decoder decoder={.v=src,.c=srcc};
  const char *line;
  int linec,lineno=1;
  for (;(linec=sr_decode_line(&line,&decoder))>0;lineno++) {
    while (linec&&((unsigned char)line[linec-1]<=0x20)) linec--;
    while (linec&&((unsigned char)line[0]<=0x20)) { line++; linec--; }
    if (!linec||(line[0]=='#')) continue;
    int strix=0,linep=0;
    while ((linep<linec)&&(line[linep]>='0')&&(line[linep]<='9')) {
      strix*=10;
      strix+=line[linep]-'0';
      linep++;
    }
    while ((linep<linec)&&((unsigned char)line[linep]<=0x20)) linep++;
    int remc=normalize_string((char*)line+linep,linec-linep);
    if (remc>16) remc=16;
    if (int_list_contains(strix_name,sizeof(strix_name)/sizeof(int),strix)) {
      if (namec>=26) {
        fprintf(stderr,"%s: Too many names. (likely a problem with %s, not with the data)\n",path,__FILE__);
        return -1;
      }
      namev[namec++]=(struct string){line+linep,remc};
    } else if (int_list_contains(strix_clue,sizeof(strix_clue)/sizeof(int),strix)) {
      if (cluec>=10) {
        fprintf(stderr,"%s: Too many clues. (likely a problem with %s, not with the data)\n",path,__FILE__);
        return -1;
      }
      cluev[cluec++]=(struct string){line+linep,remc};
    }
  }
  
  /* Optionally show me the final normalized strings.
   */
  if (0) {
    const struct string *string;
    int i;
    fprintf(stderr,"%s: %d names:\n",path,namec);
    for (i=namec,string=namev;i-->0;string++) {
      fprintf(stderr,"  '%.*s'\n",string->c,string->v);
    }
    fprintf(stderr,"%s: %d clues:\n",path,cluec);
    for (i=cluec,string=cluev;i-->0;string++) {
      fprintf(stderr,"  '%.*s'\n",string->c,string->v);
    }
  }
  
  /* Validate counts.
   */
  if ((namec!=26)||(cluec<5)) {
    fprintf(stderr,"%s: Expected exactly 26 names and at least 5 clues, found %d and %d.\n",path,namec,cluec);
    return -1;
  }
  
  /* Test each clue.
   */
  int show_valid_results=0; // <-- Turn on, to see a solution for each clue.
  int cluep=0,result=0;
  for (;cluep<cluec;cluep++) {
    char msg[256];
    int msgc=solve_clue(msg,sizeof(msg),cluev[cluep].v,cluev[cluep].c,namev,namec);
    if (msgc<0) {
      fprintf(stderr,"%s: Clue '%.*s' has no solution among the available item names.\n",path,cluev[cluep].c,cluev[cluep].v);
      result=-1;
    } else if (msgc>sizeof(msg)) {
      if (show_valid_results) fprintf(stderr,"%s:%.*s: Solvable but message too long (%d)\n",path,cluev[cluep].c,cluev[cluep].v,msgc);
    } else {
      if (show_valid_results) fprintf(stderr,"%s:%.*s: %.*s\n",path,cluev[cluep].c,cluev[cluep].v,msgc,msg);
    }
  }
  
  return result;
}

/* Callback for one file in the strings directory.
 */
 
static int cb_strings(const char *path,const char *base,char ftype,void *userdata) {
  struct context *ctx=userdata;
  if (!ftype) ftype=file_get_type(path);
  if (ftype!='f') return 0;
  ctx->filec++;
  
  // Split basename.
  int basep=0;
  const char *langname=base+basep;
  int langnamec=0;
  while ((base[basep]>='a')&&(base[basep]<='z')) { langnamec++; basep++; }
  if (base[basep]=='-') basep++;
  int id=0;
  while ((base[basep]>='0')&&(base[basep]<='9')) {
    id*=10;
    id+=base[basep]-'0';
    basep++;
  }
  if (base[basep]=='-') basep++;
  const char *name=base+basep;
  int namec=0;
  while (name[namec]&&(name[namec]!='.')) namec++;
  
  // If ID is invalid, log a warning and get out. Don't fail the test suite.
  if ((id<1)||(id>63)) {
    fprintf(stderr,"%s: Invalid id %d for strings resource.\n",path,id);
    return 0;
  }
  
  // We're only interested in ID 2, and it should have name "item".
  // Fail if that name appears on any other resource.
  if (id!=2) {
    if ((namec==4)&&!memcmp(name,"item",4)) {
      fprintf(stderr,"%s: strings:item is supposed to be id 2. What gives?\n",path);
      return -1;
    }
    return 0;
  } else {
    if (namec&&((namec!=4)||memcmp(name,"item",4))) {
      fprintf(stderr,"%s: strings:2 is expected to have no name or 'item'.\n",path);
      return -1;
    }
  }
  
  // OK, it's strings:item. Do the proper validation.
  char *src=0;
  int srcc=file_read(&src,path);
  if (srcc<0) {
    fprintf(stderr,"%s: Failed to read file\n",path);
    return -1;
  }
  int err=validate_letfloor_text_1(src,srcc,path);
  free(src);
  ctx->langc++;
  if (err>=0) ctx->okc++;
  
  return 0;
}

/* Check the clues for letfloor and confirm that they can all be produced with the available item names.
 * Runs in every defined language.
 */
 
int validate_letfloor_text() {
  struct context ctx={0};
  if (dir_read("src/data/strings",cb_strings,&ctx)<0) return -1;
  //fprintf(stderr,"%s: filec=%d langc=%d okc=%d\n",__func__,ctx.filec,ctx.langc,ctx.okc);
  if (!ctx.okc||(ctx.okc<ctx.langc)) return -1;
  //fprintf(stderr,"%s: %d files ok\n",__func__,ctx.okc);
  return 0;
}

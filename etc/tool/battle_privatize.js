/* battle_privatize.js
 * Modifies all src/game/battle/*.c in place, removing references to bellacopia.h and replacing with battle_internal.h
 */
 
const fs = require("fs");

/* Some battles need global symbols and we're just going to allow it.
 * Call them out here a la carte.
 */
const SKIP_BATTLES = [
  "chopping",
  "election",
];

/* Capture all definitions in shared_symbols.h
 * We need to inline the NS_battle_* reference that appears once in every battle.
 */
const shared_symbols = {};
{
  const src = fs.readFileSync("src/game/shared_symbols.h");
  for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
    let nlp = src.indexOf(0x0a, srcp);
    if (nlp < 0) nlp = src.length;
    const line = src.toString("utf8", srcp, nlp);
    srcp = nlp + 1;
    const match = line.match(/#define\s+([a-zA-Z0-9_]+)\s+([0-9a-fA-FxX]+)/);
    if (!match) continue;
    const v = +match[2];
    if (isNaN(v)) continue;
    const k = match[1];
    shared_symbols[k] = v;
  }
}

/* Rewrite one line in memory.
 */
function rewriteLine(src) {
  let dst = "";
  const isident = (ch) => (((ch >= 0x41) && (ch <= 0x5a)) || ((ch >= 0x61) && (ch <= 0x7a)) || (ch === 0x5f));
  for (let srcp=0; srcp<src.length; ) {
    const ch = src.charCodeAt(srcp);
    
    if (ch <= 0x20) {
      dst += src[srcp++];
      continue;
    }
    
    if (ch === 0x26) { // "&": Look for "&g.graf"
      const next = src.substring(srcp);
      if (next.startsWith("&g.graf")) {
        dst += "g_graf";
        srcp += 7;
        continue;
      }
    }
    
    if (ch === 0x67) { // "g": Lots of global references need to change.
      const next = src.substring(srcp);
      if (next.startsWith("g.font")) { dst += "g_font"; srcp += 6; continue; }
      if (next.startsWith("g.input")) { dst += "g_input"; srcp += 7; continue; }
      if (next.startsWith("g.pvinput")) { dst += "g_pvinput"; srcp += 9; continue; }
      if (next.startsWith("g.framec")) { dst += "g_framec"; srcp+= 8; continue; }
    }
    
    if (ch === 0x62) { // "b": "bm_sound(N)" has to change to "bm_sound_pan(N,0.0)".
      const next = src.substring(srcp);
      const match = next.match(/^bm_sound\((RID_sound_[a-zA-Z0-9_]*)\)/);
      if (match && match[0].length) {
        dst += `bm_sound_pan(${match[1]},0.0)`;
        srcp += match[0].length;
        continue;
      }
    }
    
    if (isident(ch)) { // Consume multiple identifier chars at once, so we don't restart mid-identifier.
      let toklen = 1;
      while ((srcp + toklen < src.length) && isident(src.charCodeAt(srcp + toklen))) toklen++;
      dst += src.substring(srcp, srcp + toklen);
      srcp += toklen;
      continue;
    }
    
    dst += src[srcp++];
  }
  return dst;
}

/* Rewrite one file in memory.
 * Return (src) or anything false to keep untouched.
 */
function rewriteFile(src, name) {
  let dst = "";
  for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
    let nlp = src.indexOf("\n", srcp);
    if (nlp < 0) nlp = src.length;
    let line = src.substring(srcp, nlp);
    srcp = nlp + 1;
    
    // Include the private header instead of bellacopia.h.
    if (line.match(/^#include "game\/bellacopia\.h"$/)) {
      dst += "#include \"game/batsup/battle_internal.h\"\n";
      continue;
    }
    
    // The battle's metadata contains its own ID, which must be inlined.
    if (line.match(/^\s+\.id=NS_battle_.*,$/)) {
      dst += "  .id=" + shared_symbols["NS_battle_" + name] + ",\n";
      continue;
    }
    
    // Replace symbols more tokenwise.
    line = rewriteLine(line);
    
    dst += line;
    dst += "\n";
  }
  return dst;
}

/* Visit each battle file.
 */
for (const base of fs.readdirSync("src/game/battle")) {
  const match = base.match(/^battle_([a-zA-Z0-9_]+)\.c$/);
  if (!match) continue;
  const name = match[1];
  if (SKIP_BATTLES.indexOf(name) >= 0) continue;
  const src = fs.readFileSync("src/game/battle/" + base).toString("utf8");
  const dst = rewriteFile(src, name);
  if (dst && (dst !== src)) {
    console.log(`${name}: Rewriting. ${src.length} => ${dst.length}`);
    fs.writeFileSync("src/game/battle/" + base, dst);
  } else {
    //console.log(`${name}: No need to rewrite.`);
  }
}

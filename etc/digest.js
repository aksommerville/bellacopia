/* digest.js
 * For showing stats from minigames.txt.
 */

const fs = require("fs");

const srcpath = "minigames.txt";

/* Acquire the database.
 */
const src = fs.readFileSync(srcpath);
const minigames = []; // { ref:boolean, count:int, name, opponent, zone, lineno }
for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
  try {
    const linep = srcp;
    let nlp = src.indexOf(0x0a, srcp);
    if (nlp < 0) nlp = src.length;
    srcp = nlp + 1;
    const line = src.toString("utf8", linep, nlp).split("#")[0].trim();
    if (!line) continue;
    const [flags, name, opponent, zone] = line.split(/\s+/g);
    const ref = (flags.indexOf("R") >= 0);
    let count = 0;
    if (flags.indexOf("1") >= 0) count = 1;
    else if (flags.indexOf("2") >= 0) count = 2;
    minigames.push({ ref, count, name, opponent, zone, lineno });
  } catch (e) {
    console.log(`${srcpath}:${lineno}: ${e.message}`);
    throw e;
  }
}
console.log(`${srcpath}: Found ${minigames.length} games.`);

/* Generic validation and bucketting.
 */
const byName = {};
const byOpponent = {};
const byZone = {};
let singlec = 0;
let doublec = 0;
for (let ai=minigames.length; ai-->0; ) {
  const a = minigames[ai];
  
  // Player count must be 1 or 2 and is required for all.
  switch (a.count) {
    case 1: singlec++; break;
    case 2: doublec++; break;
    default: throw new Error(`${srcpath}:${a.lineno}: ${JSON.stringify(a.name)} has invalid player count ${JSON.stringify(a.count)}`);
  }
  
  // Bucket by name, opponent, and zone.
  let bucket;
  if (!(bucket = byName[a.name])) bucket = byName[a.name] = []; bucket.push(a);
  if (bucket.length > 1) throw new Error(`${srcpath}:${a.lineno}: Duplicate name ${JSON.stringify(a.name)}, other at line ${bucket[0].lineno}`);
  if (!(bucket = byOpponent[a.opponent])) bucket = byOpponent[a.opponent] = []; bucket.push(a);
  if (!(bucket = byZone[a.zone])) bucket = byZone[a.zone] = []; bucket.push(a);
}
console.log(`${singlec} for one player only; ${doublec} for two.`);

/* If any opponent is used more than once, log that on its own line.
 * That's not an error necessarily, just we ought to know it.
 */
for (const opponent of Object.keys(byOpponent)) {
  const games = byOpponent[opponent];
  if (games.length > 1) {
    console.log(`!!! ${games.length} games for opponent ${JSON.stringify(opponent)}: ${games.map(g => g.name).join(", ")}`);
  }
}

/* Show all the counts by zone.
 */
console.log(`Counts by zone:`);
const zrpt = Object.keys(byZone).map(k => [byZone[k].length, k]);
zrpt.sort((a, b) => b[0] - a[0]);
for (const [count, zone] of zrpt) {
  console.log(`  ${count.toString().padStart(3)} ${zone}`);
}

/* If a zone is named by argv, print its content.
 */
for (const arg of process.argv.slice(2)) {
  const games = byZone[arg];
  if (games) {
    console.log(`Games in zone ${JSON.stringify(arg)}:`);
    for (const game of games) {
      console.log(`  ${game.name} (${game.opponent},${game.count})`);
    }
  } else {
    console.log(`${process.argv[1]}: Unexpected argument ${JSON.stringify(arg)}`);
  }
}

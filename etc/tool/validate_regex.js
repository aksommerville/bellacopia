/* validate_regex.js
 * Examines the content for Regex Contest and confirms that the regexes do not in fact match.
 */
 
const fs = require("fs");

// If we ever translate, do this all in a loop for each language.
const lang = "en";
const srcpath = `src/data/strings/${lang}-4-battle`;

// null if not participating, or [clueIndex, isRegex]
function describeStrix(strix) {
  if (strix < 66) return null;
  if (strix > 79) return null;
  return [(strix - 66) >> 1, strix & 1];
}

const cluev = []; // {text,regex,atp,expectText?}
const src = fs.readFileSync(srcpath);
for (let srcp=0, lineno=1; srcp<src.length; lineno++) {
  let nlp = src.indexOf(0x0a, srcp);
  if (nlp < 0) nlp = src.length;
  const line = src.toString("utf8", srcp, nlp).trim();
  srcp = nlp + 1;
  const match = line.match(/^(\d+)\s+(.*)$/);
  if (!match) continue; // eg empty or comment
  const strix = +match[1];
  const desc = describeStrix(strix);
  if (!desc) continue; // non-participating content
  const text = JSON.parse(match[2]); // All source strings are quoted.
  if (!cluev[desc[0]]) cluev[desc[0]] = { text: "", regex: "", atp: -1 };
  if (desc[1]) {
    const atp = text.indexOf("@");
    if (atp < 0) throw new Error(`${srcpath}:${lineno}: Expected '@'`);
    if (!text.startsWith("/") || !text.endsWith("/")) throw new Error(`${srcpath}:${lineno}: Expected '/' fore and aft`);
    const stripped = text.substring(1, atp) + text.substring(atp + 1, text.length - 1);
    try {
      cluev[desc[0]].regex = new RegExp(stripped);
    } catch (e) {
      if (stripped === "^[a-ZA-Z]+, [a-z]+\\.$") { // <-- any regex typo'd so badly that it can't compile, call it out here.
        cluev[desc[0]].expectText = "Grumble, grumble.";
      } else {
        console.log(`${srcpath}:${lineno}: Regex ${JSON.stringify(stripped)} failed to compile. Assuming that that's due to the typo. Please confirm manually.`);
      }
      cluev[desc[0]].regex = /^zzzzzzzzzzzzz$/;
    }
    cluev[desc[0]].atp = atp;
  } else {
    cluev[desc[0]].text = text;
  }
}

if ((cluev.length !== 7) || cluev.find(c => !c.text || !c.regex)) {
  console.log(`${srcpath}: Missing something, or too many.`);
  console.dir(cluev);
}

for (const clue of cluev) {
  if (clue.expectText) {
    if (clue.expectText !== clue.text) console.log(`!!! Regex for ${JSON.stringify(clue.text)} called out the wrong expectation (${JSON.stringify(clue.expectText)})`);
  } else if (clue.text.match(clue.regex)) {
    console.log(`!!! Regex already matches! text=${JSON.stringify(clue.text)} regex=${clue.regex.toString()}`);
  }
}

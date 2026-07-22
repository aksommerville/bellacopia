# Bellacopia Maleficia Monthly Progress

This is by far the largest game I've ever written.
So it's behooveful to estimate things carefully and track progress against those assumptions, for posterity's sake.
I'd like to get in the habit of making a full project review at the end of each month.

## January 2026

The ideas have been percolating for over two years, but only started work for real on 18 December 2025.
Around 15 January, had made substantial progress but threw most of it away to rework from scratch.
We're well beyond parity with that first attempt. I don't expect another restart.

You can currently reach 93% completion, and at a leisurely pace but knowing what to do, it takes about 40 minutes.
7 flowers, 5 side quests, 16 items, 3 puzzles.
Telescope and Magnifier can only be got in the Cave of Cheating.
One puzzle, the cave just south of start, is probably temporary.
The world map is noticeably sparse, biome edges are not blended at all, and jigsaw colors don't always agree across biomes.
6 battles are implemented: Fishing, Chopping, Exterminating, Boomerang, Strangling, and Laziness.
None is fully polished but all playable.
The quests that exist are mostly placeholders. Tap a stompbox to mark it complete.
Barrel Hats and Rescue the Princess are pretty much done.

One day per minigame might be too optimistic.
I earlier estimated completion in June but now I'm thinking more like 200 days, which would be September.
Maybe more like end of year.

I am feeling really good about the progress so far, and the feasibility of the project as a whole.

Stats as of d4930d199ba4b436a82694e9111a7476dac323b6, 2026-01-31:
 - Code: 18760 lines. (`src/**.[ch]`, naive count)
 - ROM size: 413151
 - Non-placeholder battles: 7 (bluefish, redfish, and all the goblins except Laziness are placeholders)
 - Casual full-clear time, no cheating: 38:25. 91% (only 14/26 items currently reachable without cheating; 16 if you cheat).
 
## February 2026

Goals:
- [x] Finish the 10 outstanding goblin battles. ...5 feb
- [x] 10 other battles. ...26 feb
- [x] Crypto puzzle in the goblins' cave.
- [x] Generate the labyrinth. Do the layout around it but needn't be final. ...14 feb
- [x] Letter exchange at battlefield. ...12 feb
- [x] Election. Just the battle and the broad framework. Not expecting all the guilds' battles yet.

GDEX will be mid-October. I'm confident we can finish before that.
Most of the minigames are simple and only take a couple hours, now that I've got the hang of it.

Went from 6 to 29 battles. Made the Goblins, Fractia, Battlefield, and Labyrinth quests mostly work.
We're now the largest Egg game by every metric except song count and songs length.

I'm particularly proud of the Chess battle, it's real Chess.

Stats as of 7bc584d2c8f33d06a3baa4677ce6a027dd2829c9, 2026-02-28:
 - Code: 37958 lines.
 - `rom=879491 code=455801 image=2276076px*38 song=4:29.782*6 sound=0:15.530*51 map=51840m*216`
 - Non-placeholder battles: 29
 - Casual full-clear time: 51:02, 93%, 17 items.

## March 2026

Goals:
- [ ] Finish placeholder battles (redfish+bluefish+fractia+labyrinth: 26). ...missed pretty bad
- [ ] Clear the short-term TODO list or punt to long-term. ...missed, but mostly covered
- [x] Make 100% completion reachable. Not the final 100% of course, but let me see that number. ...2 March
- [x] All items working. ...2 March

Missed my goal of finishing the placeholders, in fact I really dragged my feet on battles this month. Just 6 new ones.
Did add a ton of new stuff, and I'm still feeling pretty good about finishing before GDEX.
But if it's not *finished* finished then, that's fine.

I entered and judged the Uplifting Jam this month, and still managed to keep Bellacopia moving forward.

Stats as of 7120125e360b8001c1019dd3344d247a8305b30e, 2026-03-31:
 - Code: 43594 lines.
 - `rom=1022550 code=521109 image=2464748px*41 song=7:25.678*10 sound=0:16.234*54 map=77520m*323`
 - Non-placeholder battles: 35
 - Casual full-clear time: 1:36:38, 100%.

## April 2026

Goals:
- [ ] 30 new battles, starting with the 22 outstanding placeholders. ...missed, made 10.
- [ ] Treasures placed and recorded. ...missed
- [ ] Cutscenes started, finish at least one. ...missed
- [ ] Proper acquisition of stories. ...missed
- [x] First public showing. Maybe a COGG session, or an after-work game night? ...kind of. Showed some devs at the COGG Clippers promo.
- [ ] Write the final 2 outerworld songs, and put them all in a sane order. ...close
- [ ] Determine how to make or order stuffed witches. ...missed

Got very little done this month.
I devoted too much time to the Gamedev.js jam, that's still running, and took some asides for fife, ra4, and a COGG event.
The Fractia battles are about halfway implemented (I planned to have all of them done by now).
Added 4 songs but haven't got them in the right order yet, and most of them will need some rework.
Probably the coolest add this month was Bridget, some bridge-building side quests that I hadn't thought of before.

I'm counting on May, June, and July being very productive to make up for my lackluster March and April.

Stats as of a12ce9b64970160505fabdc526720aa3fddf35ee, 2026-05-01 (plus some dirty workspace for the next commit):
 - Code: 48632 lines.
 - `rom=1141114 code=585816 image=2763756px*46 song=9:37.514*14 sound=0:20.248*64 map=77520m*323`
 - Non-placeholder battles: 45
 - Casual full-clear time: 1:39:45

## May 2026

Goals:
- [ ] 30 new battles. 12 placeholders still outstanding, finish those first. ...missed. Down to 4 placeholders.
- [ ] Start cutscenes and proper story acquisition.
- [x] Sand Castle. Use the 5th jigsaw slot, make a big castle full of side quests, in the southwestern desert. Apportion it, not expecting to have it all complete.
- - ...Changed my mind to Ice Palace, and it's just an empty shell, but yep we got it.
- [ ] Outerworld final shape and monsters everywhere. ...progress, but still very incomplete.

Another slow month. I need to stop doing game jams on the side.
Placed the ice palace, filled in a lot of the outer world, and finished most of the placeholder battles: Only the Athletes' Guild remains.
Stories can now be acquired properly, most of them. The others, there's a cheat switch in the Cave of Cheating.
Reaching 100% completion is pretty difficult now because there's buried treasure all over the place, but no hints for them yet. (have to use the magnifier)

Stats as of eaf8fd1d23ab31b45690b4ca2be5a84b5a63f0f1, 2026-05-31:
 - Code: 52100 lines.
 - `rom=1234036 code=639357 image=3091436px*51 song=10:26.895*15 sound=0:23.624*73 map=84720m*353`
 - Non-placeholder battles: 53
 - Casual full-clear time: 1:49:13. Painful, due to the buried treasures.

## June 2026

Goals:
- [ ] 30 new battles. Finish Fractia first. Then start populating all the outer world regions. ...missed, but 11 isn't too bad.
- [ ] User-friendly Arcade Mode. ...missed
- [ ] Outer world: Blend biome edges, ensure treasures and gates are all in the right places, no dead zones, etc. ...progress, but not yet.
- [ ] Story cutscenes. Implement at least one fully. ...missed

Added cartographer, so reaching 100% is easy again (in fact it's more fun than it was before, it's a real treasure hunt!).
In that spirit, also got the crystal ball working. Both cartographer and crystal ball will need routine touch-up as we add more things.
ROM crossed the auspicious "1234567" size. And pretty close to 1.44 MB now; the Linux build is already over.
Finished the Fractia battles, finally!
Made substantial decorative progress: Potion shop, Fish processor, and Hospital interiors are now finalish. And a pretty Game Over modal.
Ordered the first of the merch: T-shirts, sticker sheets, logo stickers, fishing buttons, and tree pins.

The checklist doesn't show it but I feel like I'm making great progress.
We won't be finished before GDEX but we'll definitely have something worth showing.
Finishing before year end still sounds perfectly possible.

Stats as of 863805772e927286f08a03cf482670292951f3ac, 2026-06-30:
 - Code: 60447 lines.
 - `rom=1395016 code=735203 image=3697260px*61 song=10:26.895*15 sound=0:23.874*74 map=84960m*354`
 - Non-placeholder battles: 64
 - Casual full-clear time: 1:48:58

## July 2026

Battle count now, just take the assigned `NS_battle_` symbols from `shared_symbols.c`. That's exactly what we had before, just easier to grok. (ie all but `battle_placeholder`)

On 5 July, the ROM crossed floppy disk size. Web zip is still under a meg.
We've surpassed egg2 in source size (egg2 is currently 71272 lines), and we're the 3rd-largest project I've ever written. Will be first by the end, no doubt at all.
I also checked the graphics size (pixel count) of all my older games -- we're the largest, and it's not even close.
Crossed the halfway point for battles! There are now more implemented battles than planned ones.

Goals:
- [x] 20 new battles.
- [ ] Cutscenes.

## August 2026

LowRez and js13k are this month, and I am doing both. So don't expect a lot of Bellacopia progress.

Goals:
- [ ] All battles complete.

## September 2026

Goals:
- [ ] Feature-complete. Entire game is playable, presentable, and arguably finished.

## October 2026

## November 2026

## December 2026

Goals:
- [ ] Bend every effort toward finishing before end of year, if we haven't yet.

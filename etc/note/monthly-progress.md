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
- [ ] Finish placeholder battles (redfish+bluefish+fractia+labyrinth: 26).
- [ ] Clear the short-term TODO list or punt to long-term.
- [ ] Make 100% completion reachable. Not the final 100% of course, but let me see that number.
- [ ] All items working.

## April 2026

Goals:
- [ ] 30 new battles.
- [ ] Treasures placed and recorded.
- [ ] Cutscenes started, finish at least one.
- [ ] Proper acquisition of stories.

## May 2026

Goals:
- [ ] 30 new battles.
- [ ] All cutscenes complete.
- [ ] Outerworld final. Blend biome edges, ensure treasures and gates are all in the right places, no dead zones, etc.
- [ ] User-friendly Arcade Mode.

## June 2026

Goals:
- [ ] 30 new battles.
- [ ] Feature-complete. Entire game is playable, presentable, and arguably finished. Of course, we'll still have touch-up, tuning, and bugs.

# Bellacopia Maleficia

Requires [Egg](https://github.com/aksommerville/egg2) to build.

## Timeline

- 2023-08-ish: Started musing on the concept.
- 2025-12-18: Start work in earnest.
- 2025-12-31: Assess, after 6 full days of work:
- - Got world map, one minigame, pause modal, jigsaws, physics.
- - Everything looks doable.
- - Let's figure one day per minigame, and 30 days for the rest, averaging out over work days and weekends... 159 days. Early June.
- - I'm off today. Confirm ^ that assumption by making 4 minigames. ...Finished Boomerang and Chopping over about 9 hours. One day per battle might be optimistic.
- 2026-01-01: Outer world works crudely. Jigsaws are done-ish. Dialogue and pause mostly there. 4 battles: fishing, chopping, boomerang, exterminating.
- - Reassess at the end of January to confirm we're on track to finish in June. Or at least, by GDEX.
- 2026-01-09: Finished Inversion for Uplifting Jam #6 and got right back to this. So we have scientifically established that it is possible to do game jams concurrently with Bellacopia. :)
- 2026-01-11: With stand-in root devils, tried a leisurely run without touching any cheat things: 29:14 to complete. Feeling good about the one-hour-minimum target.
- 2026-01-15: I think I fucked up coordinates management, it's becoming seriously difficult. `maps.c:physics_at_sprite_position` is broken and I'm not sure how to fix it.
- 2026-01-19: Fully committed to a rewrite. Expect mark 2 to be at parity with mark 1 before end of January.
- 2026-01-27: Last features of mark 1 implemented in mark 2, and a bunch of other stuff. Looking good.
- 2026-01-29: Added some shops and placeholder King. Should be possible to do everything except Magnifier and Telescope now without cheating. ...confirmed. Done in about 22 minutes.
- 2026-01-31: On track I think. Starting monthly reports.
- 2026-02-07: Fed up with GIMP 3, so me and Alex are taking a brief aside to write a new text editor and image editor. How hard could that be?
- - ...well that's kind of back-burnered now, don't worry, i'm on it.
- 2026-03-02: Dividing time for a while, making a game and judging Uplifting Jam #7.

## TODO

- [x] Killing a Root Devil should clear the phonograph selection. Otherwise the user doesn't know she just got a new song.
- [ ] Earthquake doesn't work at screen edges. Noticed near the nortwest corner of underworld.
- [ ] We're treating L1/R1 equivalent to L2/R2 in the pause modal, so we ought to do the same for swapping items. Or why not just alias them globally, is that doable?
- [ ] Handicap for monster and fishpole.
- [ ] Strangling contest animation. It's all coded and ready, but the two animation frames are identical.
- [ ] Animate digging with shovel.
- [ ] Battle wrapper: Issue a warning before timing out. And 60 s feels too long. Gather some stats on how long they are actually taking.
- [ ] broomrace: Player faces. And can we do cool swooshing frames like apothecary?
- [ ] Full clear time doesn't register immediately, if you achieve it by finishing the maps.
- [ ] One more huge interior zone. We have room for one more jigsaw.
- - Maybe an ice palace up in the tundra? Or a sand palace in the desert? <-- Sand Castle
- [ ] Add some safe buffer around the goblins' secret door. I've bumped into monsters immediately on passing thru.
- [ ] Bridge-builder side quests, in places where you have to hookshot or broom across first, eventually you can get a convenient bridge built.
- - First one, "I'm building a bridge out of sticks! I need 8 more sticks.". And every other bridge is some other item, growing more ridiculous the further you go. Candy, fish, telescope...
- [x] Phonograph modal should dismiss noop on WEST. We are treating it same as SOUTH.
- - ...i was mistaken. WEST restores the default, as it ought to. Default happened to be the thing I had highlighted.
- [ ] Petrifying: Make knights slower, and the line of sight wider.
- [ ] Broom Race: Can we show a hint where the next item will appear? So you can decide to forfeit one for a better position on the next.

- For exploration some time in the uncertain future.
- [ ] Acquire stories.
- [ ] Some fanfare and cooldown at gameover. See `sprite_hero.c:hero_hurt()`
- [ ] Usable IP for the erudition contest.
- [ ] Might be cool to re-engage with the Princess after her quest. Could do further side quests like "will you show me the jungle temple?"
- [ ] Is it possible to reach inconsistent states by pausing while item in progress?
- [ ] Spread barrels out. It's OK to cluster them near the knitter, but the cluster in Fractia right now is annoying.
- [ ] Remove the fake French text, or even better, get it translated correctly.
- [ ] Modal blotter: Can we have the generic layer track changes and ease in and out? Then also we would want the implementations to turn off their (blotter) request during animated dismissal.
- [ ] `camera_warp()` updates the hero's position immediately, so she blinks out during the transition.
- - We're only using it for wand, and the effect is agreeable. But might need mitigation if we use for other things.
- [ ] Crying Contest: No handicap variance in 2-player mode. Should we force some?
- [ ] Is it possible to render Racketeering Contest to work with red-and-blue 3D glasses?
- [ ] Racketeering contest badly needs more juice when you hit the ball.
- [ ] fractia: Don't show the outdoor endorser signs or allow endorser battles except when the election is running.
- [ ] ^ Actually do allow endorser battles outside the election: They get more difficult each time, and if you beat the whole guild there's a one-time prize.
- - And if you do that before the election, you already have the endorsement.
- [ ] Should firepot be switchable? It wouldn't take much. But I don't have a use for it lined up.
- [ ] Obscure but easy action to force deterministic behavior from the cryptmsg, labyrinth and similar private PRNGs. I'm picturing, circle a statue three times. Probly in the Temple?
- [ ] Also an offeratory box where you can drop a coin, then the next time you go fishing you'll catch a red fish.
- [ ] Some kind of lucky charm that makes the next battle minimum difficulty. Very hard to get, but also repeatable. So there's always a way to win any battle, if you work for it.
- - [ ] Make it a spell: The Spell of Taming. Must cast close to the monster to be tamed, so there's some challenge and inconvenience to it. No effect on Root Devils.
- [x] cakecarrying: I'm thinking of a full physics simulation. Is that crazy? Punt this one, do some others first, maybe there's a more corner-cutty way that would still be appealing.
- - [x] The full sim would be crazy, an appropriate kind of crazy. Do it, but separate so we can reuse in other battles.
- - ...opted for more of an axis-aligned accordion stack than the original idea of rotating the stack. Less physics involved, and arguably more fun.
- [ ] More spells. Not sure what...
- [ ] 2 more outerworld songs. See `completion.c:bm_song_for_outerworld()`
- [ ] Properer graphics for Crystal Ball. Very rough today.
- [ ] Content for Crystal Ball. `targets.c:game_get_advice()`. Should follow roughly the same pattern as the compass, choose the logical next step.
- [ ] Parasites in the sea monster.
- [ ] I don't like how camera briefly returns to the outerworld when getting swallowed by sea monster.
- [ ] The temple's pool needs a sunbathing monk with a pina colada.
- [ ] Make the songs longer. Aim for 2 minutes per song.
- [ ] For purposes of inventory completion, maybe we should flag "intermediate" items like Barrel Hat and Letter.
- [ ] Cartographer: For a small fee, mark 3 undiscovered secrets on the map.
- [ ] Dig in an incorrect spot, occasionally dig up a really difficult monster. To raise the cost of guessing.

- Beta test. Aim to have this underway before GDEX.
- - [ ] Automated system in-app to gather a log.
- - [ ] Modify the Egg runtime to send collected logs to me.
- - [ ] Stand a service on AWS to receive them.
- - [ ] Probably use the same service for detecting and reporting carnival winners. I'm thinking "win five battles in a row"...
- - [ ] Host on itch or aksommerville.com. Invite friends.
- - [ ] Mail out rewards? Like, first ten people to beat the main quests get a stuffed witch?
- - [ ] Tools to digest logs on my end.
- - [ ] I want to see 2-player mode too. By March or so, we should engage at game night and COGG meetings.

- Validation.
- [ ] Within each plane, if one map has a parent, they all must.
- [ ] No grandparent maps.
- [ ] Singleton items can't appear more than once.
- [ ] Treasure chest with a quantity item must have a flag.
- [ ] Plane edges must be solid, or have wind if we add that.
- [ ] Map edges on plane zero must be solid.
- [ ] Continuous root paths leading to each root devil.
- [ ] No conflicting `NS_fld_*`. I typo'd a few numbers and it's not obvious until weird things break.

- Promo merch. Plan to order all by early July, well in advance of Matsuricon and GDEX.
- GDEX being in mid-October, let's set a drop-dead date of 6 September. Order things by then or don't order.
- - [ ] Pins, buttons, stickers.
- - [ ] Homemade stuffed witches, if I can work that out.
- - - There are mail-order companies that do this, eg customplushmaker.com. Long lead times (~90 days), and I don't know about pricing.
- - - ^ prefer bearsforhumanity.com
- - - ...but I really would prefer "Made in Flytown"
- - - Whatever we're doing, figure it out by the end of April.
- - [ ] Witch hats. Same idea as the dolls.
- - [ ] Jigsaw puzzles? Would be on-theme, and I've ordered these before, it's a snap. Too expensive to give away probly (~$30 ea at a glance).
- - [ ] Mini comic?
- - [ ] Thumb drives. I still have 20 leftover from Spelling Bee, if we want to make the shell ourselves.
- - - customusb.com has some gorgeous shells and cases (they did the Plunder Squad bottles). $5-10 ea for the drives and another $5-10 for cases. A bit much.
- - [ ] Instruction manual + strategy guide, to bundle with thumb drives.
- - [ ] Book of sheet music.
- - [ ] Videos.
- - [ ] Book of Cheating. Maybe a digital edition?
- - [ ] Big banners, the kind that roll up into a case.

## Quests and Prizes

- Don't delete finished items.
- [x] End the war => Hookshot
- [ ] Run for mayor => no prize
- [x] Hat the barrels => Bell
- [ ] Catch em all => (incremental; multiple)
- [x] Rescue the Princess => purse+100
- [x] Decipher the goblins' text => Shovel
- [x] Escape the labyrinth => no prize?
- [x] Pay the toll trolls => no prize
- [ ] The toad and the boulder => no prize?
- [x] Inventory critic => hc3
- [ ] Expensive health care => Heart Container, plus incremental prizes. Can't be gold.
- [ ] Worldwide broom races => ?
- [ ] Tree stories => ?
- [ ] Reverse Sokoban => ?
- This set of quests doesn't feel adequate. Need like a dozen more.

- Prizes unassigned.
- [x] Magnifier. Buy in Temple Gift Shop. Maybe temporary.
- [x] Telescope. Buy in Temple Gift Shop. Maybe temporary.
- [x] hc1: castleshop
- [x] hc2: temple pool
- [x] hc3: invcritic
- [ ] hc4
- [ ] hc5
- [x] Bomb: inconvenience
- [x] Stopwatch: underworld, temporarily
- [x] Bus Stop: inconvenience
- [x] Snowglobe: underworld, temporarily
- [x] Tape Measure: underworld, temporarily
- [x] Phonograph: underworld, temporarily
- [x] Crystal Ball: underworld, temporarily
- [x] Power Glove: Sea monster (Labyrinth).
- [x] Marionette: underworld, temporarily
- Can add as many purse upgrades as we like. I guess no more than 9, so it stays within 3 digits?

## Sprites with placeholder battle

- executioner: CheeseCutting
- teacher: Latin
- firefighter: Rescuing
- wrestler: SumoHorse
- samurai: Fencing
- dancer: Jeter
- ballplayer: HomeRunDerby
- doctor: Dissection
- fractia_nurse: CPR
- secretary: Stenography
- clerk: Sorting
- gambler: Slapping
- dealer: Shuffling
- hustler: Cheating
- plumber: Plumbing
- contractor: Building
- fractia_carpenter: Sawing
- gardener: Lawnmowing

## Acknowledgements

Unofficially collecting things I've borrowed or referred to.
Before the first release, validate and clean up this list. And if in-game credits are warranted, do that.

- Chess checkmate scenarios: https://www.chess.com/terms/checkmate-chess
- Erudition Contest paintings, copied from Wikipedia. TODO Determine license terms.
- - The Art of Painting; by Johannes Vermeer; 1666–1668; oil on canvas; 1.3 × 1.1 m; Kunsthistorisches Museum (Vienna, Austria)
- - A Sunday Afternoon on the Island of La Grande Jatte, 1884–1886, oil on canvas, 207.5 × 308.1 cm, Art Institute of Chicago
- - The Swing; by Jean-Honoré Fragonard; 1767–1768; oil on canvas; Wallace Collection
- - The Garden of Earthly Delights; by Hieronymus Bosch; c. 1504; oil on panel, Museo del Prado
- Erudition Contest text. TODO Not acquired yet.

## Morally Questionable

I'm keeping this game appropriate for children in my opinion, like all of Dot Vine's games.
Record everything that someone might object to on moral grounds, so we can declare it all up front.

- Witchcraft. No getting around that!
- Stealing Contest.
- Election rigging.
- Shaking Contest (champagne).
- Telekinesis Contest (drinking game).
- Casino.
- Strangling Contest.

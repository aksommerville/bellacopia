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

## TODO

- [ ] Geographic and temporal variety in fish. See `game.c:game_choose_fish()`
- [ ] Catch a sea monster in the Labyrinth.
- [ ] bluefish and redfish battles. Right now they are the same as greenfish, but I'd like extra gimmicks.
- [ ] Some fanfare and cooldown at gameover. See `sprite_hero.c:hero_hurt()`
- [ ] Handicap for monster and fishpole.
- [ ] Pepper
- [ ] Bomb
- [ ] Stopwatch
- [ ] Portable Bus Stop
- [ ] Snowglobe
- [ ] Five more items...
- [ ] Strangling contest animation. It's all coded and ready, but the two animation frames are identical.
- [ ] Knitter incremental prizes.
- [ ] Animate digging with shovel.
- [ ] Boomerang: Drop the other rang after one collides.
- [ ] Boomerang: Speed it up a little? Make the koala lose sooner.
- [ ] Battle wrapper: Issue a warning before timing out. And 60 s feels too long. Gather some stats on how long they are actually taking.
- [ ] Have monster pause a little after a battle completes, is that possible? Kind of annoying when you get mobbed.
- [ ] Hiring: Randomize choice and order of criteria above a certain difficulty.
- [ ] `cryptmsg.c:dress_battle()` hacked with English. Should be doable via battle.
- [ ] broomrace: Player faces. And can we do cool swooshing frames like apothecary?
- [ ] Saved game is still getting wiped out sometimes when I visit Arcade Mode.
- - I swear this is real, but on careful observation I can't reproduce it.

- For exploration some time in the uncertain future.
- [ ] Can we randomize the statuemaze like we did cryptmsg?
- [ ] Usable IP for the erudition contest.
- [ ] Might be cool to re-engage with the Princess after her quest. Could do further side quests like "will you show me the jungle temple?"
- [ ] Is it possible to reach inconsistent states by pausing while item in progress?
- [ ] Spread barrels out. It's OK to cluster them near the knitter, but the cluster in Fractia right now is annoying.
- [ ] Remove the fake French text, or even better, get it translated correctly.
- [ ] Modal blotter: Can we have the generic layer track changes and ease in and out? Then also we would want the implementations to turn off their (blotter) request during animated dismissal.
- [ ] `camera_warp()` updates the hero's position immediately, so she blinks out during the transition.
- - We're only using it for wand, and the effect is agreeable. But might need mitigation if we use for other things.
- [ ] Crying Contest: No handicap variance in 2-player mode. Should we force some?
- [ ] rsprite: Monsters are sparse at first and accumulate the longer you stay in a zone. Can we thumb the scale a bit to try keeping population near the middle?
- [ ] Is it possible to render Racketeering Contest to work with red-and-blue 3D glasses?
- [ ] Put a toll troll near the beginning, blocking access to Fractia and Battlefield. Cheap, say 3 or 4 gold. Just make sure they've played some battles first.
- [ ] fractia: Don't show the outdoor endorser signs or allow endorser battles except when the election is running.
- [ ] ^ Actually do allow endorse battles outside the election: They get more difficult each time, and if you beat the whole guild there's a one-time prize.
- - And if you do that before the election, you already have the endorsement.
- [ ] Friendly UI for editing saved games. (non-public, obviously) Actually, I dunno, how bad do we need this? Would I ever use it?
- [ ] Should firepot be switchable? It wouldn't take much. But I don't have a use for it lined up.
- [ ] Obscure but easy action to force deterministic behavior from the cryptmsg, labyrinth and similar private PRNGs. I'm picturing, circle a statue three times. Probly in the Temple?
- [ ] Also an offeratory box where you can drop a coin, then the next time you go fishing you'll catch a red fish.
- [ ] Some kind of lucky charm that makes the next battle minimum difficulty. Very hard to get, but also repeatable. So there's always a way to win any battle, if you work for it.
- [ ] cakecarrying: I'm thinking of a full physics simulation. Is that crazy? Punt this one, do some others first, maybe there's a more corner-cutty way that would still be appealing.
- - [ ] The full sim would be crazy, an appropriate kind of crazy. Do it, but separate so we can reuse in other battles.
- [ ] More spells. Not sure what...
- [ ] 5 more outerworld songs. See `completion.c:bm_song_for_outerworld()`

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
- [ ] Catch em all => (incremental)
- [x] Rescue the Princess => purse+100
- [x] Decipher the goblins' text => Shovel
- [x] Escape the labyrinth => no prize?
- [x] Pay the toll trolls => no prize
- [ ] The toad and the boulder => no prize?
- [ ] Inventory critic => ? obvs not an item
- [ ] Expensive health care => Heart Container, plus incremental prizes. Can't be gold.
- [ ] Worldwide broom races => ?
- [ ] Tree stories => ?
- [ ] Reverse Sokoban => ?
- This set of quests doesn't feel adequate. Need like a dozen more.

- Prizes unassigned.
- [x] Magnifier. Buy in Temple Gift Shop. Maybe temporary.
- [x] Telescope. Buy in Temple Gift Shop. Maybe temporary.
- [x] hc1: castleshop
- [ ] hc2
- [ ] hc3
- [ ] hc4
- [ ] hc5
- Can add as many purse upgrades as we like. I guess no more than 9, so it stays within 3 digits?

## Sprites with placeholder battle

- baker: CakeCarrying
- cop: Traffic
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
- psychic: Telekinesis
- ogre: Smashing
- golem: Pushing
- medusa: Petrifying
- nyarlathotep: MindControl

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


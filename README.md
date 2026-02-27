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

- [x] Poke compass after you get the thing. Tricky...
- [ ] Geographic and temporal variety in fish. See `game.c:game_choose_fish()`
- [ ] bluefish and redfish battles. Right now they are the same as greenfish, but I'd like extra gimmicks.
- [ ] Some fanfare and cooldown at gameover. See `sprite_hero.c:hero_hurt()`
- [x] Story modal overlay: Highlight HP and gold briefly when they change. ...nix this. Usually they change due to a battle or dialogue.
- [ ] Handicap for monster and fishpole.
- [x] Include the Egg Universal Menu stuff in system vellum: Input config, Language, Audio levels.
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
- [ ] Chopping: Start with the belt populated. Too much lead time as is.
- [ ] Chopping: Randomize CPU play a little. At middling difficulty, Princess and Goat *always* end in a tie.
- [ ] Exterminating: AI is really bad, it gets stuck in loops sometimes. Princess shouldn't play this in real life, but still, it's painful to watch.
- [ ] Battle wrapper: Issue a warning before timing out. And 60 s feels too long. Gather some stats on how long they are actually taking.
- [ ] Princess could still use some attention. Sticks too long on corners, and continues moving while hookshotted. (these are tolerable, just not perfect).
- [ ] Princess: Can we give her a rotating arm, and she always points toward the castle? We already have logic like that for the compass.
- [ ] Does using the telescope kill the Princess? I suspect it will. Mitigate. ...it does. It also changes song if you pass over a map with the `song` command.
- - Can we just mark her as "never delete due to distance"?
- [ ] Suspend pumpkin's movement while hookshotted.
- [ ] Have monster pause a little after a battle completes, is that possible? Kind of annoying when you get mobbed.
- [ ] Add a shop near the castle. Because as you're returning the Princess, you probably have lots of gold, but your purse is about to get maxed, great shopping opportunity.
- [ ] Hiring: Randomize choice and order of criteria above a certain difficulty.
- [x] Instead of East to toggle item, use L1/R1 to step one-dimensionally thru the inventory. Since those buttons have to exist anyway.
- [ ] `cryptmsg.c:dress_battle()` hacked with English. Should be doable via battle.
- [x] Sprites have been observed stuck in the Labyrinth wall. Probably walked into the wall space before my first visit to the map and happened to be there.
- - I think the right mitigation is in `sprite_monster`, have them detect stuck-in-wall and force themselves out.
- - If not that, we'd have to refresh cells for the entire plane on entry. Which I guess is not too crazy?
- - Need a better way to surface the bug. It's random, so maybe not an exact repro, but can we thumb the rsprite generator to make it likelier?
- - Use a manual trap, a giant floor with a treadle that we can make it solid on demand. ...repro'd, easy.
- [x] escalator: Continuously ramp down the motion for sprites near the bottom to mitigate jitter.
- [ ] broomrace: Player faces. And can we do cool swooshing frames like apothecary?
- [ ] The two outer world bits attached to the Temple, don't change song when you go outside, keep playing the Temple song.
- [ ] Catch a sea monster in the Labyrinth.
- [ ] Saved game is still getting wiped out sometimes when I visit Arcade Mode.
- [ ] racketeering: Ball can get stuck at the floor, and make obnoxious repetitive sound effects. Playwise, sticking on the floor doesn't seem so bad.

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
- - [ ] Pins, buttons, stickers.
- - [ ] Homemade stuffed witches, if I can work that out.
- - - There are mail-order companies that do this, eg customplushmaker.com. Long lead times (~90 days), and I don't know about pricing.
- - - ^ prefer bearsforhumanity.com
- - - ...but I really would prefer "Made in Flytown"
- - - Whatever we're doing, figure it out by the end of April.
- - [ ] Witch hats. Same idea as the dolls.
- - [ ] Jigsaw puzzles? Would be on-theme, and I've ordered these before, it's a snap. Too expensive to give away probly (~$30 ea at a glance).
- - [ ] Mini comic?
- - [ ] Thumb drives. I still have 20 leftover from Spelling Bee.
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
- [ ] Magnifier.
- [ ] Telescope.
- [ ] hc1
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


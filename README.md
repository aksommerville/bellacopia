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

## TODO

- [x] Return Princess to her cage after losing a battle.
- [ ] How to get Princess through doors? "you don't" isn't an option, alas.
- [x] Should there be a prize when Princess wins a battle? It shouldn't come up often. ...no
- [x] Princess: Follow me.
- [x] Princess battle: Strings not inserted in welcome message.
- [x] Princess battle: Took a heart from me instead of killing her.
- [x] Monsters don't seem interested in the Princess. Did I forget to update that? ...yep forgot.
- [x] Add Princess graphics to existing games. Exterminating doesn't need it.
- [ ] Door transitions. Fade to black? Spotlight?
- [ ] Tree stories. These don't go in regular inventory. Need a list of them somewhere in the pause modal, where you can replay.
- [x] When multiple dialogue modals, there are multiple blotters. Can we have just one blotter, and magically put it at the right layer? (eg "Not enough gold" at a shop)
- - While in there, pause also needs a blotter.
- [ ] `modal_story.c:story_render_overlay()`: Cache overlay texture.
- [ ] Friendly UI for editing saved games. (non-public, obviously)
- [ ] More spells. Not sure what...
- [ ] Invalid spell, should we do a head-shake like in Full Moon?
- [x] Don't animate walking when bumping into a wall. Maybe a pushing face?
- [ ] Should firepot be switchable?
- [ ] Poke compass after you get the thing. Tricky...
- [ ] Geographic and temporal variety in fish. See `game.c:game_choose_fish()`
- [ ] bluefish and redfish battles. Right now they are the same as greenfish, but I'd like extra gimmicks.
- [ ] Some fanfare and cooldown at gameover. See `sprite_hero.c:hero_hurt()`
- [ ] Story modal overlay: Highlight HP and gold briefly when they change.
- [ ] Handicap for monster and fishpole.
- [x] Progress and side quests for vellum_stats.
- [x] Dynamically center vellum_stats, to accomodate multiple languages.
- [ ] Include the Egg Universal Menu stuff in system vellum: Input config, Language, Audio levels.
- [ ] Poke jigsaw after rotating a piece in case it's positioned to join.
- [ ] Have monsters consume the candy; it only lasts so long.
- [ ] Pepper
- [ ] Bomb
- [ ] Stopwatch
- [ ] Portable Bus Stop
- [ ] Snowglobe
- [ ] Strangling contest animation. It's all coded and ready, but the two animation frames are identical.
- [ ] Knitter incremental prizes.
- [ ] Animate digging with shovel.
- [ ] Boomerang: Drop the other rang after one collides.
- [ ] Boomerang: Speed it up a little? Make the koala lose sooner.
- [ ] Chopping: Start with the belt populated. Too much lead time as is.
- [ ] Chopping: Randomize CPU play a little. At middling difficulty, Princess and Goat *always* end in a tie.
- [ ] Exterminating: AI is really bad, it gets stuck in loops sometimes. Princess shouldn't play this in real life, but still, it's painful to watch.
- [ ] Battle wrapper: Issue a warning before timing out. And 60 s feels too long. Gather some stats on how long they are actually taking.

- For exploration some time in the uncertain future.
- [ ] There's room for badges under the currency counters in the inventory vellum. Maybe for the flowers? Or keys, something like that.
- [ ] Is it possible to reach inconsistent states by pausing while item in progress?
- [ ] Spread barrels out. It's OK to cluster them near the knitter, but the cluster in Fractia right now is annoying.
- [ ] Remove the fake French text, or even better, get it translated correctly.
- [ ] Modal blotter: Can we have the generic layer track changes and ease in and out? Then also we would want the implementations to turn off their (blotter) request during animated dismissal.

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

- Promo merch. Plan to order all by early July, well in advance of Matsuricon and GDEX.
- - [ ] Pins, buttons, stickers.
- - [ ] Homemade stuffed witches, if I can work that out.
- - - There are mail-order companies that do this, eg customplushmaker.com. Long lead times (~90 days), and I don't know about pricing.
- - - ^ prefer bearsforhumanity.com
- - - ...but I really would prefer "Made in Flytown"
- - - Whatever we're doing, figure it out by the end of April.
- - [ ] Witch hats. Same idea as the dolls.
- - [ ] Jigsaw puzzles? Would be on-theme, and I've ordered these before, it's a snap. Too expensive to give away probly.
- - [ ] Mini comic?
- - [ ] Thumb drives. I still have 20 leftover from Spelling Bee.
- - [ ] Book of sheet music.
- - [ ] Videos.
- - [ ] Book of Cheating. Maybe a digital edition?
- - [ ] Big banners, the kind that roll up into a case.

## Quests and Prizes

- [ ] End the war => Hookshot
- [ ] Run for mayor => no prize
- [x] Hat the barrels => Bell
- [ ] Catch em all => (incremental)
- [ ] Rescue the Princess => purse+100
- [ ] Decipher the goblins' text => Shovel
- [ ] Escape the labyrinth => no prize?
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
- [x] Shovel.
- [ ] hc1
- [ ] hc2
- [ ] hc3
- [ ] hc4
- [ ] hc5
- Can add as many purse upgrades as we like. I guess no more than 9, so it stays within 3 digits?

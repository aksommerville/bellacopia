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

- [ ] Tree stories. These don't go in regular inventory. Need a list of them somewhere in the pause modal, where you can replay.
- [ ] `modal_story.c:story_render_overlay()`: Cache overlay texture.
- [ ] Friendly UI for editing saved games. (non-public, obviously)
- [ ] More spells. Not sure what...
- [ ] Invalid spell, should we do a head-shake like in Full Moon?
- [ ] Should firepot be switchable?
- [ ] Poke compass after you get the thing. Tricky...
- [ ] Geographic and temporal variety in fish. See `game.c:game_choose_fish()`
- [ ] bluefish and redfish battles. Right now they are the same as greenfish, but I'd like extra gimmicks.
- [ ] Some fanfare and cooldown at gameover. See `sprite_hero.c:hero_hurt()`
- [ ] Story modal overlay: Highlight HP and gold briefly when they change.
- [ ] Handicap for monster and fishpole.
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
- [ ] Princess could still use some attention. Sticks too long on corners, and continues moving while hookshotted. (these are tolerable, just not perfect).
- [ ] Princess: Can we give her a rotating arm, and she always points toward the castle? We already have logic like that for the compass.
- [ ] Does using the telescope kill the Princess? I suspect it will. Mitigate. ...it does. It also changes song if you pass over a map with the `song` command.
- - Can we just mark her as "never delete due to distance"?
- [ ] Toll Troll and the Mayor's Bridge should be passes thru a solid mass, so you can't broom over them.
- [ ] Suspend pumpkin's movement while hookshotted.
- [ ] Have monster pause a little after a battle completes, is that possible? Kind of annoying when you get mobbed.
- [ ] Add a shop near the castle. Because as you're returning the Princess, you probably have lots of gold, but your purse is about to get maxed, great shopping opportunity.
- [ ] Hiring: Randomize choice and order of criteria above a certain difficulty.
- [ ] Consider East button to toggle equipped item with the pause menu's focus (typically the most recently equipped thing).

- For exploration some time in the uncertain future.
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
- [ ] End the war => Hookshot
- [ ] Run for mayor => no prize
- [x] Hat the barrels => Bell
- [ ] Catch em all => (incremental)
- [x] Rescue the Princess => purse+100
- [x] Decipher the goblins' text => Shovel
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
- [ ] hc1
- [ ] hc2
- [ ] hc3
- [ ] hc4
- [ ] hc5
- Can add as many purse upgrades as we like. I guess no more than 9, so it stays within 3 digits?

## Sprites with placeholder battle

- cook1: Watching
- cook2: Flapjack
- hotdog: Topping
- waiter: Stacking
- bartender: Shaking
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

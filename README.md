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

## TODO

- [x] Gobbling: Can't pull the last item off the table!
- [ ] Goblin cave: Try making the whole thing dark, with lots of firepots. Must be able to reach entrance, jail, and Skull Lake without matches.
- [x] Ooops I forgot to make the real sprites for erudition contest
- [ ] Monster battles should sometimes reward a heart, if you need one.
- [ ] Refactor the battle mode to orthogonalize skin and control: Skin is (Monster,Dot,Princess) for each side, and Control is (Man,CPU) for each side.
- - Ultimately I'd like 2-player mode to let you choose your hero.
- - And might like to add a Dot-vs-(CPU)Princess side quest after rescuing her. "Hey let's play that one game again".
- - [ ] Distinguish "difficulty" from "handicap". How hard is the game, and how skilled the players, these are orthogonal concerns.
- - Do finish the goblin games first as is, then update.
- - [ ] After this refactor, rewrite the placeholder and incorporate all the boilerplate I've been repeating on each goblin game.
- - [ ] Ensure that we're preventing the Princess from entering battles that don't support cpu-vs-cpu. The battles between the jail and the castle must support cpu-vs-cpu.
- [x] After a wee change to hero->blocked, now she jitters sometimes on first touching a wall.
- - Due to off-axis correction. New policy: Off-axis correction counts as movement. You're not "blocked" until fudge brings you to a standstill.
- [x] If you get killed in the goblins' cave before escaping, you should spawn in the jail, not at home.
- - Need a "trigger activity once" POI. And we'll start the kidnapping quest the same way.
- [x] Seems like playing Arcade Mode kills the saved game sometimes. Maybe all the time.
- - Update: It happens when you enter Arcade Mode and play one, before doing Story Mode. Story Mode first, it seems to persist as expected.
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

- Second batch of minigames: The Eleven Goblin Games
- [x] gs- Throwing: Loblin
- [x] gs- Stealing: Roblin
- [x] gs- Regex: Globlin. ~Skipping this because I have no idea what to do. If nothing comes to me, drop it.~
- - Show a jasmine unit test like `expect( LITERAL_STRING ).toMatch( REGEX )`, and you have to select the one wrong character in REGEX.
- [x] gs- Racketeering: Moblin
- [x] gs- Laziness: Sloblin
- [x] gs- Hiring: Joblin. Another tricky one. Come back to it.
- [x] gs- Gobbling: Goblin
- [x] gs- Erudition: Snoblin
- [x] gs- Crying: Soblin
- [x] gs- Cobbling: Coblin
- [x] gs- Apples: Boblin

- [ ] Crypto side quest.
- - Clues to the Bone and Leaf rooms are by Skull Lake. Encrypted, plus you can't read them in the dark.
- - Bone and Leaf tasks should both be "Use item X twice while standing on the seal.".
- - - Depletable items only, to discourage guessing. Potion, match, candy, bugspray, vanishing cream.
- - Opening the Star room requires losing a battle, to a specific goblin, with a specific item equipped, while standing on the seal.
- - Arrange dynamically for that goblin species not to spawn in the seal room.
- - Express the random parts in 16 bits exactly, so we can store as fld16 and regenerate on demand. Alphabet order, Bone task, Leaf task, Star goblin, Star item.

- For exploration some time in the uncertain future.
- [ ] Usable IP for the erudition contest.
- [ ] Might be cool to re-engage with the Princess after her quest. Could do further side quests like "will you show me the jungle temple?"
- [x] There's room for badges under the currency counters in the inventory vellum. Maybe for the flowers? Or keys, something like that.
- - `vellum_inventory.c:inventory_rebuild_extra()`, using for jailkey, and we should follow the same pattern for similar items and achievements.
- [ ] Is it possible to reach inconsistent states by pausing while item in progress?
- [ ] Spread barrels out. It's OK to cluster them near the knitter, but the cluster in Fractia right now is annoying.
- [ ] Remove the fake French text, or even better, get it translated correctly.
- [ ] Modal blotter: Can we have the generic layer track changes and ease in and out? Then also we would want the implementations to turn off their (blotter) request during animated dismissal.
- [ ] `camera_warp()` updates the hero's position immediately, so she blinks out during the transition.
- - We're only using it for wand, and the effect is agreeable. But might need mitigation if we use for other things.
- [ ] Crying Contest: No handicap variance in 2-player mode. Should we force some?

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
- [ ] hc1
- [ ] hc2
- [ ] hc3
- [ ] hc4
- [ ] hc5
- Can add as many purse upgrades as we like. I guess no more than 9, so it stays within 3 digits?

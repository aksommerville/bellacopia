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
- 2026-01-27: Last features of mark 1 implemented in mark2, and a bunch of other stuff. Looking good.

## TODO

- [ ] Battle
- - [x] greenfish
- - [x] bluefish
- - [x] redfish
- - [ ] strangling

- Bugs, future, ...
- [ ] Door transitions. Fade to black? Spotlight?
- [ ] When multiple dialogue modals, there are multiple blotters. Can we have just one blotter, and magically put it at the right layer? (eg "Not enough gold" at a shop)
- - While in there, pause also needs a blotter.
- [ ] There's room for badges under the currency counters in the inventory vellum. Maybe for the flowers? Or keys, something like that.
- [ ] `modal_story.c:story_render_overlay()`: Cache overlay texture.
- [ ] Friendly UI for editing saved games. (non-public, obviously)
- [ ] More spells. Not sure what...
- [ ] Invalid spell, should we do a head-shake like in Full Moon?
- [ ] Don't animate walking when bumping into a wall. Maybe a pushing face?
- [ ] Should firepot be switchable?
- [ ] Poke compass after you get the thing. Tricky...
- [ ] Geographic and temporal variety in fish. See `game.c:game_choose_fish()`
- [ ] bluefish and redfish battles. Right now they are the same as greenfish, but I'd like extra gimmicks.
- [ ] Some fanfare and cooldown at gameover. See `sprite_hero.c:hero_hurt()`
- [ ] Story modal overlay: Highlight HP and gold briefly when they change.
- [ ] Handicap for monster and fishpole.
- [ ] Progress, side quests, and maps for vellum_stats.
- [ ] Dynamically center vellum_stats, to accomodate multiple languages.
- [ ] Include the Egg Universal Menu stuff in system vellum: Input config, Language, Audio levels.
- [ ] Carpenter: Don't show singleton items if possessed. Do show matches even if at the limit.
- [ ] Have monsters consume the candy; it only lasts so long.
- [ ] Need either 3, 7, or 12 new items. (if not 12, reduce the inventory size).
- - Battlefield letter will be inventoriable but doesn't count -- its slot gets filled by hookshot after the quest.
- - Portable Bus Stop
- - Bomb
- - Stopwatch
- - Pepper: Creates a bonfire. Long-lasting light source, but fixed position.
- - Telescope: Pan the camera in a cardinal direction, to its limit.
- - Snowglobe? Maybe too powerful, as implemented in Full Moon and Dead Weight.
- - Shovel?

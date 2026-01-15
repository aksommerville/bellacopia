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

## TODO

- [x] I don't like `rsprite` for spawning random encounters. Move to something more continuous, more random.
- - Bothersome when you turn around and the guy you just beat is back, like always.
- - Maybe keep using `rsprite` commands, since they have to be recorded somewhere, but trigger in a looser fashion: "I want a sprite here, which sprite should it be?"
- [ ] How to select handicap? See `sprite_monster.c`
- [ ] Monsters should have configurable detect radius and chase speed. Also idle speed i guess.
- [ ] Yeti and Chupacabra: They'll appear at the polar limits and must be extremely difficult. To discourage polar exploration.
- [ ] Songs per map: Crossfade and retain playhead.
- [ ] Cache rendered maps in camera. Currently drawing from scratch each frame.
- [x] How to define root paths for the Divining Rod? See `hero_item.c:hero_roots_present` ...`CMD_map_root`
- [ ] Chopping: Prepopulate conveyor belt.
- [ ] After collecting treasure, poke hero (qx,qy) in case the divining rod changed.
- [ ] Also poke initially. As is, if you start on a POI and don't touch the dpad, nothing triggers.
- [ ] Can we not trigger the divining rod's visual feedback during conversation? (eg talk to the carpenter with DR armed).
- [ ] Dialogue needs to be more in-your-face. Maybe partial blackout of the background, or move the box near the hero.
- [ ] Chopping contest: Box textures don't always clear, seems to happen when triggered multiple times nearby.
- [ ] Diegetic witch toys.
- - [x] Broom. Free rotation, like Thirty Seconds Apothecary.
- - [x] Hookshot.
- - [ ] Fish Pole.
- - [x] Wand. Not sure what it does, but a witch needs a wand. Maybe arbitrary spell casting? ...yep, Full Moon style.
- [ ] Passive items.
- - [ ] Boat. Lets you convert at docks, and travel freely over water.
- - [ ] Fetch-quest subjects.
- - [ ] Badges.
- [x] Currencies.
- - [x] Gold
- - [x] Fish
- - [x] Blood
- [ ] Improve presentation after hookshot-into-water panic. `hero_motion.c`
- [ ] Broom: Animation like in Thirty Seconds Apothecary: Flapping hat, and screechy turns.
- - But before moving on that, get the single frame right. I don't like the antialiasing as is, too much of it.
- [ ] Broom: Ongoing whoosh sound?
- [ ] Give Dot a push-wall face. Important for dark rooms, so you can tell when you're blocked.
- [x] Gold from treasure chests, even tho it's not an "item".
- [ ] Heart containers. Like items, don't place them yet. Reserve that for late in the process, so we can spread treasures out evenly. How many containers in all?
- [ ] Item-bearing battles.
- [ ] Buying blood and selling it should not be the same merchant, shouldn't even be the same house.
- - Don't buy blood, buy potions to put in a bottle. "Frequent Shopper Rewards Club".
- [ ] Merchants should allow multiple quantity purchase, and should show your existing quantity when focussed.
- [ ] `modal_battle.c`: Show graphic input description per battle.
- [ ] `modal_dialogue.c`: Typewriter text. Should we? I actually don't like that effect, and it would take considerable effort.
- - Usually when I do typewriter, the text is a tile array. That won't fly here, it has to be font.
- [ ] `modal_dialogue.c`: Arrive and dismiss animation, and a blinking indicator when no choices.
- [ ] Pause: Remember inventory cursor position. But do force it as before, when changing pages. OK to remember only for the session, like pause page.
- [ ] Is it feasible to make ornamental footprints in the snow and sand as you walk? That might look cool.
- Extra tooling:
- - [ ] Sprite hitbox.
- Data validation:
- - [ ] Jigpiece in every mappable map, just once.
- - [ ] Mappable planes rectangular with no gaps.
- - [ ] Every monster sprite has a battle declared.
- - [ ] rsprite monsters in every map that ought to.
- - [ ] root paths
- - [ ] Convenient tool for checking audio levels. Not BM-specific, so do it in Egg Editor.
- Beta test logging.
- - [ ] Client-side log service. Write tersely, and store to a new storage key every session.
- - [ ] Some cudgel in the web runtime to pull those logs when the session ends, send them to me, and then drop from localStorage.
- - - This cudgel must also interfere with the UI and explain itself. (partly because we will not spy on users without telling them, and partly so I don't accidentally deploy prod like that).
- - [ ] Remote service (AWS?) to receive those logs.
- - [ ] Dev-side tooling to digest logs.

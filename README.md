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

## TODO

- [ ] Can we cause new jigsaw pieces to appear on vacant space instead of overlapping existing ones? Best effort only, of course.
- [ ] Jigsaw breaks near the south center, every time I leave the pause modal.
- - UPDATE: With the entire jigsaw assembled, it does stay put.
- [ ] Monster sprite.
- - [ ] How to spawn? Static spawn points in a map is not the right way.
- - [ ] How to select handicap? See `sprite_monster.c`
- [ ] Songs per map: Crossfade and retain playhead.
- [ ] Cache rendered maps in camera. Currently drawing from scratch each frame.
- [ ] Off-axis correction for the hero when blocked.
- [ ] Diegetic witch toys.
- - [ ] Broom. Free rotation, like Thirty Seconds Apothecary.
- - [ ] Hookshot.
- - [ ] Divining Rod.
- - [ ] Fish Pole.
- - [ ] Matches. Quantity.
- - [ ] Bug Spray. Quantity.
- - [ ] Candy. Quantity.
- - [ ] Wand. Not sure what it does, but a witch needs a wand. Maybe arbitrary spell casting?
- [ ] `modal_battle.c`: Show graphic input description per battle.
- [ ] `modal_dialogue.c`: Typewriter text. Should we? I actually don't like that effect, and it would take considerable effort.
- - Usually when I do typewriter, the text is a tile array. That won't fly here, it has to be font.
- [ ] `modal_dialogue.c`: Arrive and dismiss animation, and a blinking indicator when no choices.
- Extra tooling:
- - [ ] Sprite hitbox.
- Data validation:
- - [ ] Jigpiece in every mappable map, just once.
- - [ ] Mappable planes rectangular with no gaps.
- - [ ] Every monster sprite has a battle declared.
- Beta test logging.
- - [ ] Client-side log service. Write tersely, and store to a new storage key every session.
- - [ ] Some cudgel in the web runtime to pull those logs when the session ends, send them to me, and then drop from localStorage.
- - - This cudgel must also interfere with the UI and explain itself. (partly because we will not spy on users without telling them, and partly so I don't accidentally deploy prod like that).
- - [ ] Remote service (AWS?) to receive those logs.
- - [ ] Dev-side tooling to digest logs.

# Bellacopia Maleficia

Requires [Egg](https://github.com/aksommerville/egg2) to build.

## Timeline

2023-08-ish: Started musing on the concept.
2025-12-18: Start work in earnest.

## TODO

- [x] Modal stack.
- [x] Outer world, first pass.
- - [x] Map loader.
- - [x] Sprite framework.
- - [x] Hero sprite, placeholder.
- - [x] World edges. Loop horizontally, after an unreasonable distance, and extend forever vertically.
- - [x] Prevent respawn of existing sprites.
- [x] Nicer graphics, enough to work with.
- [x] Physics.
- [x] Proper hero sprite. ...good for now but obviously incomplete
- [x] Doors.
- [ ] Monster sprite.
- - [x] Walk about randomly.
- - [x] Trigger battles on contact.
- - [ ] How to spawn? Static spawn points in a map is not the right way.
- [x] Camera: Removed sprites can re-spawn while in view. Check for that, and only spawn offscreen. Ensure the first screen after a cut, they do spawn.
- [ ] Pole fairies: If you travel too far north or south, a pole fairy occasionally appears and offers to warp you back home.
- [ ] Songs per map: Crossfade and retain playhead.
- [ ] Cache rendered maps in camera. Currently drawing from scratch each frame.
- [ ] Global state. Flags and small integers. Persist.
- [ ] Pause modal.
- - [ ] Inventory.
- - [ ] Achievements.
- - [ ] Map. (jigsaw)
- [x] Minigames framework.
- [ ] Diegetic witch toys.
- - [ ] Broom. Free rotation, like Thirty Seconds Apothecary.
- - [ ] Hookshot.
- - [ ] Divining Rod.
- - [ ] Fish Pole.
- - [ ] Matches. Quantity.
- - [ ] Bug Spray. Quantity.
- - [ ] Candy. Quantity.
- - [ ] Wand. Not sure what it does, but a witch needs a wand. Maybe arbitrary spell casting?
- [ ] Dialogue.
- [x] `sprite_monster.c`: Can we keep the sprite visible during modal_battle's intro?
- [ ] `modal_battle.c`: Show graphic input description per battle.

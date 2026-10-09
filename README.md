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
- 2026-05-02: Slower than I hoped, mostly because I keep getting distracted by game jams. But I still think we'll finish this year.
- 2026-06-07: Public showing at CORGS Con. Pleasantly surprised by how little guidance anyone needed.
- 2026-06-14: Finally finished the placeholder battles, and I'm devoting June and July to Bellacopia. No jams or conventions.
- 2026-06-17: Played thru, letting the Crystal Ball drive. 2h20 to full clear. It made some very bad choices, like, I'd have bought the broom way earlier.
- 2026-07-07: Playtest at work. Xiangsi, Nick, Shawn, Katy, mostly Katy. Good reception, and got some actionable advice.
- - Zoos were a hit. Players understood fast and kind of gravitated toward the zooage.
- - Players were not at all drawn toward Root Devils. Maybe that will change when there's more narrative setup?
- 2026-08-12: June and July were very productive. August and September won't be, due to jams and GDEX, but I feel like we're over the hump.
- 2026-09-12: js13k and Uplifting 9 close tomorrow, and GDEX jam starts in a week. Sept probably done for dev, but it was a great 12 days, feeling really good about GDEX readiness.

## TODO

- Low-hanging fruit to address this last week before GDEX:
- [x] Fish odds are still broken somehow. Immediately after buying the fishpole, I fished twice in the same spot and the second time rejected. You should get at least 3.
- - Because `g.fishclock` was treated as a bound for a random choice. Nixed that, now it's just a clock, and you can usually catch 3 fish in a row.
- [x] Guild sprites: Timeout the reentry protection.
- [x] Move hookshot into blue captain's tent, that's where the quest ends.
- [x] Fishwife seems to reject a sale if it would saturate your gold. Fine to reject if you're already maxed, but do it if partial.
- [x] Cartographer should not sell hints before the Princess is rescued. Before that, he should direct you to the mountains.
- [x] Regex contest has `/[a-zA-z]/` as a typo, but I think that's actually technically valid. Rephrase. ...confirmed, it is valid in JS.
- - ...added `etc/tool/validate_regex.js` to check these faster in the future. The one called out above does fail it.
- [x] Passing thru an unopened buried door backward should open it. eg you can reach the south jungle heart container with snowglobe instead of shovel.
- [x] Ignore dpad as the pause modal closes. Sometimes I register an extra stroke after dismissing.
- [x] 10-key modal, at the surveyor puzzle, failed to wrap position downward.
- [ ] Look for incomplete edges in the outerworld. I know at least the jungle/isthmus junction is still unfinished.
- [ ] Lengthen `break_soil`. It's going to be playing over and over on the cab at GDEX, we're going to get very sick of it.
- [x] What happens if you straight up abandon the Princess on the way back from a walk, but do enter the castle? Needs to be some distance threshold where the walk doesn't count.
- - ...and now that I think about it, this applies to the rescue quest too.
- - We should require that she be visible when you enter a door.
- - We're good already. She fails to walk thru the door right around the visibility limit.

- [x] Does the Princess quest need nerfed a little? It might be too much walking, and not enough dodging space.
- [ ] Make something happen if you beat a guild outside the election.
- [ ] More spells. Not sure what...
- - [ ] Spell of Bridging: Generate a temporary walkable spot, over adjacent water cells. Lasts long enough that you can step there and re-cast the spell.
- - - ...maybe not. I was thinking of this as a solution to Grandpa's puzzle, but fishing works better.
- [ ] Some little fanfare on reaching 100%.
- [ ] `sprite:130-buck` is available for reuse; orphaned because i didn't know what "steer" means, oops.
- [ ] Things, hearts, and purse stories: Maybe not necessary to run it the first time. It's a source of conflict, maybe we just drop the trigger.
- [ ] Poker at the casino. UI in place and activity ready to write.
- [ ] Blackjack at the casino. UI in place and activity ready to write.
- [ ] Some fireworks when an animal gets captured. "+5 Gold" does the trick, but when your purse is maxed, it feels dead.
- [ ] Have the knitter give a purse upgrade instead of Bell. Make Wrapping Contest the only way to get Bell.
- [ ] Make a fish that can only be caught in the dark.
- [ ] `sprite:nurse` and `activity:bloodbank` have been removed. We can delete the sprite and activity if that's final.
- [x] Eliminate random no-fish. Fishing should only fail when you've exhausted it. ...it's already eliminated. The clock can be deceptive maybe? But I think behavior is correct.
- [ ] Earn the labyrinth story travelling either direction. If you skip the escalator -- can do without cheating -- you get the story.
- [ ] Can we make the decorative flying Ice Dragon disappear when a stationary one becomes visible? It's possible, with a Broom.
- [ ] Fairy Gobmother, in one of the new goblin-cave rooms. General advice.
- [ ] Walking the Princess feels excessive. Reduce the mandatory walks to 2 or 3. Maybe also make a few battles mandatory?

- Challenges for Ice Palace and other bonus zones. Underworld. Back of the temple? Goblins' cave?
- - We can really cut loose with these and make them ridiculously hard, since they'll never be mandatory.
- - Do be mindful of the Minimalist path, it needs to stay item-free.
- - [ ] Timed flamethrowers and projectiles. Can do really fast ones to require a Stopwatch.
- - [ ] Conveyor belts.
- - [ ] Somewhere a Spell Bee style side quest where you complete a dungeon, then have to go back in and clean up after yourself.
- - [ ] An aggressive monster that wins every time so you have to use Bug Spray or Vanishing Cream. "Invincibull". Factoring contest! "Factor this 64-bit integer"

- Fill out maps.
- [ ] Fractia
- - [ ] Thing Store
- - [ ] Labor Union
- - [ ] Athletes' Guild
- - [ ] Grandpa's Puzzle House
- - [ ] Underground entrance house
- - [ ] Public Sector Employees' Union
- - [ ] Food Service Guild
- [ ] Forest / Cheapside / Meadow
- - [ ] Exteriors.
- - [ ] Dot's house
- - - Ensure the underground entrance is not obvious but doesn't need items to enter.
- [x] Battlefield
- - [x] Blue Captain's tent
- - [x] Red Captain's tent
- [ ] Tundra
- - [ ] "tuns" of exterior space to fill.
- - [ ] Magnetic North interior.
- - - [ ] Morse Code practice.
- - [ ] Ice Palace: Puzzles and monsters.
- [ ] Mountains
- [ ] Goblins' Cave
- [ ] Botire
- - [ ] Exterior. Make houses slightly less ugly.
- - [ ] Inventory Critic
- - [ ] Knitter
- - [ ] Underground entrance house
- [ ] Jungle
- [ ] Temple
- - [ ] Lots of unused space at the north edge. Put some bonus challenges here or eliminate it. ...remains pretty sparse even after adding the Sphinx.
- - [ ] Gift shop
- - [ ] Roof access room
- [ ] Sea monster
- - [ ] Parasites etc
- - [ ] Path to the treasure should be dark.
- - [ ] Right now, one could enter and still miss the treasure. Don't let that happen, make it obvious.
- [ ] South jungle
- [ ] East desert
- - [ ] Castle
- [ ] West desert
- - [ ] Inconvenience Store
- [ ] Underground.
- - [ ] Lots of monsters everywhere, and we can put really hard ones down here.
- - [ ] We made `flammable` cells but haven't used yet. Block some regions such that you have to bomb thru. Maybe the Wishing Well?

- Battle repairs.
- [ ] broomrace: Get real prizes. Also don't call it "Broom Race", since that's a thing now.
- - If there's real prizes, they need to be in addition to the score-bearing ones, like have two things available at once, otherwise Minimalists can't avoid them.
- [ ] cpr: It's weird how your winning stroke is Down but the patient pops Up. Can we delay his reaction or something, make it feel bouncier?
- [ ] homerunderby: I don't like how foul tips count immediately as a Strike.
- [ ] racketeering: Try an option for red-and-blue 3d glasses. Maybe AUX2 to toggle? That's how Rad Racer did it.
- [ ] rescuing: Eliminate the blood and make it more cartoony somehow. Blood might put us in a harsher ratings category.
- [ ] seamonster: Butt ugly, and not in a good way.
- [ ] topping: Too many numbers. Use sliding bars instead.
- [ ] wrapping: For certain gifts, allow delivering to my pocket instead of wrapping. Add candy, bomb, etc. Allow to get the Marionette and Bell this way?
- [ ] Find more opportunities for special battle prizes like Stealing and Fishing.
- - Ensure that if real goods are awarded, the player is able to avoid them, to keep Minimalist Completion possible.

- TODO Punted items, assess closer to release.
- [ ] Final credits. See notes below.
- [ ] Finalize zoo and rsprite assignment, once all battles are written.
- [ ] ^ also the kidnappers at `sprite_princess.c:princess_spawn_kidnapper()`
- [ ] It might be a problem that we use aggregate input state in the outer world but `[1]` in one-player battles. Maybe mitigate that somehow during a one-player battle?
- [ ] Princess battles are currently random, neutral bias, and consequence-free. We could make it more interesting if we want.
- [ ] What if we kept a huge set of per-battle high scores? Maybe accessible via the zoos?
- [ ] Is it possible to reach inconsistent states by pausing while item in progress?
- [ ] Review economy, balance prices etc.
- [ ] Can we passively enable mouse for all modals? Today you can use the mouse for jigsaw but it stops working when you click any other tab. I think users won't like that.
- [ ] Make the songs longer. Aim for 2 minutes per song.
- [ ] When a zookeeper is complete, what if the animals appear fixed on his carpet and you can challenge them any time?
- [ ] Need a venue to report broom race times. Status vellum is the obvious place, but it's already pretty crowded. Think it over, no hurry.
- [ ] Review song and sound levels, right now they're pretty heterogenous.
- [ ] `camera_warp()` updates the hero's position immediately, so she blinks out during the transition.
- - We're only using it for wand, and the effect is agreeable. But might need mitigation if we use for other things.
- [ ] Remove the fake French text, or even better, get it translated correctly.
- - Do at least a machine-generated translation for Spanish and French. German? Portugese? Anything non-Latin is off the table alas.
- [ ] Inside the temple, compass points you to the front door for the root devil and the heart container.
- - I don't think we need to solve this generally, but can we make it point to the pool door instead? (that would be wrong if it's pointing to anything else, but I think that's less bad than current).
- - UPDATE: Also impacts hc4, and expect more. I think we do need a general solution.
- - Delay this, because I'm doubting now. We don't want the compass (or any hints) to be perfect. Maybe it's ok to leave just as it is?
- [ ] Some dialogue ends up with two lines and a single word on the second line. Can we break text more balancedly?
- - Punt this until the entire set of dialogue is more or less finished.
- [ ] "Are you sure?" at New Game if there's a save with anything done. But *do not* implement this yet! I want an unencumbered New Game during development.
- - Or we could sidestep the issue by allowing multiple save files. Consider it.
- [ ] Review accessibility once all battles are complete. Rhythm games should be possible without audio, color-based games should work for the colorblind, etc.
- [ ] Remove Debug Mode, or have it require a launch parameter or something. (it's ok if players get into it, just i don't want to present it as a recommendation)

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
- [ ] Zoo assignments agree with rsprite.
- [ ] All tiles in jigsaw maps have jigctab assigned.
- [ ] All possible surveyor remote locations are sensible.
- [ ] Getting the last heart container and purse upgrade trigger their cutscene sensibly, no matter which one comes last. (and anything else triggering cutscenes like this?)
- Manual validation before release.
- [ ] Ensure I removed all AUX2-to-win from battles.
- [ ] Minimal completion possible.
- [ ] 100% completion possible.
- [ ] Crystal Ball, compass, and cartographer always give sane advice.
- [ ] Every battle plays sensibly in arcade mode.
- [ ] Validate Ice Palace wall manually. It has lots of awkward cross-map edges, and I'm probably going to break them when adding details.

- Promo merch. No particular timeline for ordering, just make sure we have plenty of stuff before any cons.
- - [ ] Homemade stuffed witches, if I can work that out.
- - - There are mail-order companies that do this, eg customplushmaker.com. Long lead times (~90 days), and I don't know about pricing.
- - - ^ prefer bearsforhumanity.com
- - - ...but I really would prefer "Made in Flytown"
- - - Whatever we're doing, figure it out by the end of April.
- - - A semicircle of felt 12cm diameter rolls up into a pretty good witch hat, just the size for a fist puppet.
- - - - The white felt sheets I already have are the perfect size for this, alas they're white and not purple.
- - - Oooh how about sock puppets?
- - [ ] Witch hats. Same idea as the dolls.
- - [ ] Jigsaw puzzles? Would be on-theme, and I've ordered these before, it's a snap. Too expensive to give away probly (~$30 ea at a glance).
- - [ ] Mini comic?
- - - If we draw one, get in touch with Hardwired, Back Alley Games, and similar, see if they'll publish it.
- - [ ] Thumb drives. I still have 20 leftover from Spelling Bee, if we want to make the shell ourselves.
- - - customusb.com has some gorgeous shells and cases (they did the Plunder Squad bottles). $5-10 ea for the drives and another $5-10 for cases. A bit much for giving away.
- - [ ] Instruction manual + strategy guide, to bundle with thumb drives.
- - [ ] Book of sheet music.
- - [ ] Videos.
- - - [ ] 30 second demo reel. Gameplay only. For linking on storefronts.
- - - [ ] 10-15 minute promo loop. High resolution with overlay messaging. For running in the background at cons.
- - - [ ] Walkthrough: Full clear. And play every battle.
- - - [ ] Walkthrough: Any% speed run.
- - - [ ] Walkthrough: Minimalist.
- - [ ] Book of Cheating. Maybe a digital edition?
- - [ ] Big banners, the kind that roll up into a case.
- - - $130 at bannerbuzz.com.

## Quests and Prizes

- Don't delete finished items.
- [x] End the war => Hookshot
- [x] Run for mayor => no prize
- [x] Hat the barrels => Bell
- [x] Catch em all => (incremental; multiple)
- [x] Rescue the Princess => purse+100
- [x] Decipher the goblins' text => Phonograph
- [x] Escape the labyrinth => no prize?
- [x] Pay the toll trolls => no prize
- [x] The toad and the boulder => eliminated
- [x] Inventory critic => hc3
- [ ] Expensive health care => Heart Container, plus incremental prizes. Can't be gold.
- [x] Worldwide broom races => ?
- [x] Tree stories => ?
- [ ] Reverse Sokoban => ?
- [x] Bridges => The bridges are their own prize.
- This set of quests doesn't feel adequate. Need like a dozen more.

- Prizes unassigned.
- [ ] hc5
- [ ] purse3 (the king's purse upgrade doesn't have a "purse" name, purse3 is the fourth and last)

## Battles That Aren't Real Battles

- Technicalities, not part of the game.
- - placeholder
- Cutscenes, implemented as battle for silly technical reasons.
- - seamonster
- Interactions you trigger with an item.
- - greenfish
- - bluefish
- - redfish
- - shovelthrowing
- Narrative one-offs.
- - strangling
- - election
- - medomat

## Acknowledgements

Unofficially collecting things I've borrowed or referred to.
Before the first release, validate and clean up this list. And if in-game credits are warranted, do that.

- Chess checkmate scenarios: https://www.chess.com/terms/checkmate-chess
- Erudition Contest paintings, copied from Wikipedia.
- - The Art of Painting; by Johannes Vermeer; 1666–1668; oil on canvas; 1.3 × 1.1 m; Kunsthistorisches Museum (Vienna, Austria)
- - - Criticism by Jonathan Janson:
- - - https://www.essentialvermeer.com/vermeer's_methodolgy.html
- - - Accuracy of tone and contour, rather than the methodic accumulation of descriptive elements and their minute description, sustain the illusion of reality.
- - A Sunday Afternoon on the Island of La Grande Jatte, 1884–1886, oil on canvas, 207.5 × 308.1 cm, Art Institute of Chicago
- - - Criticism by Meyer Schapiro (d 1996), "Modern Art":
- - - https://noteaccess.com/APPROACHES/Seurat.htm
- - - This artificial micro-pattern serves the painter as a means of order portioning and nuancing sensation beyond the familiar qualities of the objects that the colors evoke.
- - The Swing; by Jean-Honoré Fragonard; 1767–1768; oil on canvas; Wallace Collection
- - - Criticism by Wilhelm Lubke (c 1860):
- - - https://www.artandpopularculture.com/Jean-Honor%C3%A9_Fragonard
- - - In his hands art degenerates into an uncurbed lascivious play, which, however, was found compatible with serious technical achievement. 
- - The Garden of Earthly Delights; by Hieronymus Bosch; c. 1504; oil on panel, Museo del Prado
- - - Criticism by Larry Silver:
- - - https://jhna.org/articles/jheronimus-bosch-issue-of-origins/
- - - Bosch's formulations lay securely founded in Christian theology, chiefly as articulated by the church father Saint Augustine, as well as other late medieval manifestations.
- - The Fragonard quote is public domain, being published around 1860.
- - The Seurat quote is not, but I haven't been able to find an email for Meyer Schapiro's estate to ask for permission.
- - The Vermeer and Bosch quotes are both modern with living authors; try to contact their authors.
- - - Vermeer: Jonathan Janson. Affirmative 2026-06-24.
- - - Bosch: Larry Silver. Affirmative 2026-06-21.
- - - Emailed both 2026-06-21.
- - All four images were copied from Wikipedia, and Wikipedia asserts that all four are Public Domain.
- - We will of course give all four painters and authors a screen credit.
- Quoth Wikipedia:
- - The official position taken by the Wikimedia Foundation is that "faithful reproductions of two-dimensional public domain works of art are public domain".
- - This photographic reproduction is therefore also considered to be in the public domain in the United States. 
- Telekinesis Contest: Reference to the old janx spirit drinking game in Hitchhiker's Guide to the Galaxy.
- Morse Code Contest: "Northern Union" is a reference to the telegraph company Western Union, used without permission (I assume Fair Use).
- Morse Code Contest: "What hath God wrought" -Samuel Morse
- Play testers: Alex, Xiangsi, Nick, Shawn, Katy
- One of the Regex Contest clues is "Grumble, grumble", a reference to Zelda.
- "Nyarlathotep" borrowed from Lovecraft, I think it was The Case of Charles Dexter Ward?
- "Nosferatu" borrowed from the FW Murnau film, tho I think it's an ordinary word? ...a little googling suggests it's a corruption of the Romanian word for "plague", introduced by Bram Stoker.
- Chanting contest text is all from Psalms, copied from here: https://www.sacredbible.org/studybible/OT-21_Psalms.htm
- - Verses 24:2 71:3 72:3 72:22 74:3 75:5 81:6 86:3
- QR-code-generator by nayuki. https://github.com/nayuki/QR-Code-generator/

## Morally Questionable

I'm keeping this game appropriate for children in my opinion, like all of Dot Vine's games.
Record everything that someone might object to on moral grounds, so we can declare it all up front.

- Witchcraft. No getting around that!
- Stealing Contest.
- Election rigging.
- Shaking Contest (champagne).
- Telekinesis Contest (drinking game).
- Wining Contest (wine).
- Casino.
- Strangling Contest.
- Dead babies splattered on the sidewalk: Rescuing Contest.

## Gameplay that changes between campaigns

Be sure to note all these things prominently in the docs, so people don't assume they're constant.
Maybe point out that buried treasure, bridges, and races *are* constant.

- Old Goblish alphabet.
- Assignment of encrypted messages to stones. Same messages every time, mostly, but in random places.
- Choice of items and battle for the goblin seals.
- Rules and layout of the statue maze.
- Position of the remote surveyor points (and therefore distances).

And things that randomize even within one campaign:

- Layout of the Labyrinth.
- Minesweeper puzzles.
- Treadlepass gate.

## Lessons Learned

Collecting lil dev things here, since it's such a large project. Write up a neat recap around release time.

- Egg is awesome.
- Stitching together single-screen maps was probably a mistake. I think one map per plane would have worked smoother.
- The "activity" abstraction works great, I should do something similar in every big game.
- Monthly goals and reports, my estimates were all way off, but this seems a healthy practice. Make a habit of it for large games.
- Pick an orientation for sprites! I've settled on rightward as the default, but some early sprites (eg Dot) are leftward. Good to be consistent about that.
- Adding things to `shared_symbols.h`, which one does a lot, forces a full rebuild. This needs a solution from the eggdev end, and I'm not sure what that will look like.
- - Can be mitigated with discipline on the project end. I've refactored battle to not use `shared_symbols.h`. Future large projects, we should partition like that all around.
- Some items should only interact from within the pause menu (eg Phonograph). Not worth changing Bellacopia, but keep that in mind next time around. Three kinds of item: Equippable, passive, modal.
- Capturing `sprite->arg` by reference is a hazard, it keeps shooting me in the ass. Smarter to copy args, and have a separate dedicated "reference to rom" field.
- (from younap, not bellacopia) Really need song tooling that a dev can share with a composer. Musicians can't run eggdev, and would be lost in its interface even if they could.

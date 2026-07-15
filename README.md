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

## TODO

- [x] Goblin cave isn't fully darkening anymore. ...I forgot to remove hero from `GRP(light)` when burning a flammable.
- - That happened during a full run. Quit and retry, it darkens as expected.
- - Use a match to burn something. It seems to stick on.
- [ ] Gambling challenge in the casino. Choose a difficulty, implies a wager and payout, and play a random battle.
- [ ] Maybe a warning when you leave a guild with the endorsement partially won? User wouldn't assume that it resets.
- [ ] Might be cool to re-engage with the Princess after her quest.
- - Definitely. Have three options: Take a walk, Play a game, or Gossip.
- - Some subtle but noticeable indicator near her to show how many walks you've taken and games you've played. A prize when you've done them all.
- [ ] Modal blotter: Can we have the generic layer track changes and ease in and out? Then also we would want the implementations to turn off their (blotter) request during animated dismissal.
- [ ] Make something happen if you beat a guild outside the election.
- [ ] More spells. Not sure what...
- [ ] The Toad and the Boulder. I kind of forgot about this and its Root Devil is just sitting there in the open.
- [ ] Game over song.
- [ ] New song during the election, maybe?
- [ ] Add Blackjack and maybe Poker at the Casino. We already have lovely playing card graphics.
- [ ] When the war is over, rsprites there should settle down. You can still engage, but maybe they just sit still in fixed positions? Like a zoo but fit for men.
- [ ] Consider eliminating rsprite by zoos a la carte, one monster at a time. Leads to some strategy: "I need to get rid of this walrus!"
- [ ] Show in-progress Minimalist badge on stats vellum, to give users a hint whether they still have it.
- [ ] Have the bus stop try a little harder, especially in tight spaces it can be annoying when it rejects.
- [ ] I forgot Power Glove and Goodluck for NPC-triggered battles (zookeeper and siren).
- [ ] Goblins' treasure shouldn't be the phonograph -- that place has fixed music, phonograph won't work in there.

- Battles written but not placed.

- Challenges for Ice Palace and other bonus zones. Underworld. Back of the temple? Goblins' cave?
- - We can really cut loose with these and make them ridiculously hard, since they'll never be mandatory.
- - Do be mindful of the Minimalist path, it needs to stay item-free.
- - [x] Economy of Motion style keystroke tracker.
- - [x] Sokoban in the real world. Include Ounce blocks that you can Hookshot, and unreachable blocks that you have to Snowglobe.
- - - Or bits where you could either Vanishing Cream into a side channel to push, or Snowglobe.
- - [x] Also diegetic Bomberman, same idea as Sokoban. ...already doable, use `CMD_map_flammable`.
- - [ ] Timed flamethrowers and projectiles. Can do really fast ones to require a Stopwatch.
- - [x] Constant flamethrower with a stompbox behind it, to require Marionette.
- - [x] Treadle that you need a timed release remotely. Bomb or maybe Marionette or Snowglobe.
- - [ ] Conveyor belts.
- - [x] Something like Full Moon's trick floor, where you have to use Compass to know the path? Make it work with Divining Rod too.
- - [ ] Somewhere a Spell Bee style side quest where you complete a dungeon, then have to go back in and clean up after yourself.
- - [ ] An aggressive monster that wins every time so you have to use Bug Spray or Vanishing Cream.
- - [ ] A cow or something, that approaches you when you ring the bell.
- - [x] Secret knowledge accessible only with the telescope. This would be a great fit in the Temple, already has dead space at the edges.
- - [x] Ice floor, that you slide until you hit the wall.
- - [ ] Use Snowglobe to put unreachable alphabet blocks in order.
- - [ ] Motion sensor. Has a visible spook scale. You can cross its sight laboriously by starting and stopping, but realistically need to outrun it or block it or something.
- - [x] Six treadles that you press in order. Use Divining Rod to see the order.

- Fill out maps.
- [ ] Fractia
- - [ ] Outer. BoE / City Hall signage, statue, litter...
- - - Maybe eliminate the northernmost street, pad with trees for more separation from tundra.
- - [ ] Thing Store
- - [ ] Labor Union
- - [ ] Vacant house next to Labor Union
- - [ ] Athletes' Guild
- - [ ] Vacant house between Athletes' Guild and Casino
- - [ ] Casino
- - [ ] Underground entrance house
- - [ ] Public Sector Employees' Union
- - [ ] Food Service Guild
- [ ] Forest / Cheapside / Meadow
- - [ ] Exteriors.
- - [ ] Dot's house
- - - Ensure the underground entrance is not obvious but doesn't need items to enter.
- - [ ] Blood Bank. Are we even keeping this?
- - - Replace with Goody's house: She tells you about the wand and teaches spells.
- - [ ] Underground entrance house
- - - Same concerns as Dot's ladder, make it a little secret.
- - [ ] Fishwife
- - [ ] Carpenter
- [ ] Battlefield
- - [ ] Blue Captain's tent
- - [ ] Red Captain's tent
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
- - [ ] Lots of unused space at the north edge. Put some bonus challenges here or eliminate it.
- - [x] Some kind of hint in the southwest detached room, or change it to "Hidden Message" a la Sergio.
- - [ ] Gift shop
- - [ ] Roof access room
- [ ] Sea monster
- - [ ] Parasites etc
- - [ ] Path to the treasure should be dark.
- [ ] South jungle
- [ ] East desert
- - [ ] Castle
- [ ] West desert
- - [ ] Inconvenience Store
- [ ] Underground.
- - [ ] Lots of monsters everywhere, and we can put really hard ones down here.
- - [ ] Dark some regions.
- - [ ] Mr and Mrs Rabbit at the surveyor challenge.
- - [x] Dots<->Fractia door.
- - [x] Dots<->Cheapside door.

- Battle repairs.
- [x] calligraphy: No fun, eliminate. ...actually! If we show the reference image always, it's pretty nice. The challenge is the etch-a-sketch controls, not the guessing-where-pixels-go.
- [ ] cpr: Score by counting strokes and comparing their timing to their own standard deviation -- should be completely immune to audio latency.
- [ ] homerunderby: Show the continuous tie-break score somehow too.
- [ ] mindcontrol: Make a more continuous connection state, like sometimes the connection is better than others.
- [ ] morsecode: At normal difficulty, getting every letter right should be a win, regardless of extra spaces.
- [ ] racketeering: Badly needs more juice when you hit the ball. Consider dropping or rewriting altogether; players are really struggling with the perspective thing.
- [ ] seamonster: Butt ugly, and not in a good way.
- [ ] shuffling: Redo the whole appearance, use nice card graphics and animate the interleaving.
- [ ] sumohorse: Eliminate? Or rethink from the top. It's no fun.
- [ ] Find more opportunities for special battle prizes like Stealing and Fishing.
- - Ensure that if real goods are awarded, the player is able to avoid them, to keep Minimalist Completion possible.

- Cutscenes, first pass: Text, placement, and placeholder images.
- - [x] mayor
- - [x] war
- - [x] kidnap
- - [x] rescue
- - [x] labyrinth
- - [ ] toad
- - [x] barrel
- - [x] things
- - [x] broom
- - [ ] map
- - [ ] carpenter
- - [ ] fish
- - [ ] flowers
- - [ ] hearts
- - [ ] gold
- - [ ] potion
- [ ] Cutscenes, second pass: Proper images.

- TODO Punted items, assess closer to release.
- [ ] What if we kept a huge set of per-battle high scores? Maybe accessible via the zoos?
- [ ] Is it possible to reach inconsistent states by pausing while item in progress?
- [ ] Review economy, balance prices etc.
- [ ] Can we passively enable mouse for all modals? Today you can use the mouse for jigsaw but it stops working when you click any other tab. I think users won't like that.
- [ ] Make the songs longer. Aim for 2 minutes per song.
- [ ] Home Run Derby pleases me so good, I want to make a whole baseball minigame. Is that crazy? Can we work it in there somewhere?
- [ ] When a zookeeper is complete, what if the animals appear fixed on his carpet and you can challenge them any time?
- [ ] Need a venue to report broom race times. Status vellum is the obvious place, but it's already pretty crowded. Think it over, no hurry.
- [ ] Is it possible to render Racketeering Contest to work with red-and-blue 3D glasses?
- [ ] Review song and sound levels, right now they're pretty heterogenous.
- [ ] Should there be a visible indication where a buried treasure has already been collected?
- [ ] `camera_warp()` updates the hero's position immediately, so she blinks out during the transition.
- - We're only using it for wand, and the effect is agreeable. But might need mitigation if we use for other things.
- [ ] Remove the fake French text, or even better, get it translated correctly.
- [ ] Check ladders in the outerworld, they probably all need some safe buffer.
- [ ] Inside the temple, compass points you to the front door for the root devil and the heart container.
- - I don't think we need to solve this generally, but can we make it point to the pool door instead? (that would be wrong if it's pointing to anything else, but I think that's less bad than current).
- - UPDATE: Also impacts hc4, and expect more. I think we do need a general solution.
- - Delay this, because I'm doubting now. We don't want the compass (or any hints) to be perfect. Maybe it's ok to leave just as it is?
- [ ] Some dialogue ends up with two lines and a single word on the second line. Can we break text more balancedly?
- - Punt this until the entire set of dialogue is more or less finished.
- [ ] "Are you sure?" at New Game if there's a save with anything done. But *do not* implement this yet! I want an unencumbered New Game during development.
- - Or we could sidestep the issue by allowing multiple save files. Consider it.

- GDEX prep. If we don't get to the Beta Test stuff below, at least get this much done.
- - [ ] Separate process to run on consoles. Scan for saved games and deliver them to our local C&C server.
- - [ ] Web app on C&C server to compose emails with saved-game links. So I can hit that from my phone, then email to the player on demand.
- - [ ] Accept saved game from query param and prompt if there's conflict.
- - [ ] Option to log to a file. (in Egg or Romassist)

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

- Promo merch. Plan to order all by early July, well in advance of Matsuricon and GDEX.
- GDEX being in mid-October, let's set a drop-dead date of 6 September. Order things by then or don't order.
- - [x] Pins, buttons, stickers.
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
- - [x] Sticker sheet with mix-n-match heads and hats. Sticker Mule does these. ...ordered (these 4 things) 2026-06-27
- - - `title_vines_padded-5923px.png`: 3x2" sticker
- - - `mixnmatch-master.png`: 4x6" sticker sheet
- - - `pin-tree-256px.png`: Acrylic pin
- - - `button_fishing-444px.png`: Button
- - [ ] T-shirts with a silhouette of Dot reading to a tree and its roots spell out Bellacopia Maleficia. ...ordered 2026-06-27
- - [ ] In-person fishing contest. Make little fishpoles with a clothespin at the end and a big occluding sea.
- - - [ ] Made the fishpoles, they're hilarious, and I have a blue posterboard, just need to figure out how to stand it up.

## Quests and Prizes

- Don't delete finished items.
- [x] End the war => Hookshot
- [ ] Run for mayor => no prize
- [x] Hat the barrels => Bell
- [ ] Catch em all => (incremental; multiple)
- [x] Rescue the Princess => purse+100
- [x] Decipher the goblins' text => Phonograph
- [x] Escape the labyrinth => no prize?
- [x] Pay the toll trolls => no prize
- [ ] The toad and the boulder => no prize?
- [x] Inventory critic => hc3
- [ ] Expensive health care => Heart Container, plus incremental prizes. Can't be gold.
- [ ] Worldwide broom races => ?
- [ ] Tree stories => ?
- [ ] Reverse Sokoban => ?
- [x] Bridges => The bridges are their own prize.
- This set of quests doesn't feel adequate. Need like a dozen more.

- Prizes unassigned.
- [x] Magnifier. Buy in Temple Gift Shop. Maybe temporary.
- [x] Telescope. Buy in Temple Gift Shop. Maybe temporary.
- [x] hc1: castleshop
- [x] hc2: temple pool
- [x] hc3: invcritic
- [x] hc4: south jungle
- [ ] hc5
- [x] Bomb: inconvenience
- [x] Stopwatch: underworld, temporarily
- [x] Bus Stop: inconvenience
- [x] Snowglobe: Ice Palace.
- [x] Tape Measure: underworld, temporarily
- [x] Phonograph: Goblins' cave.
- [x] Crystal Ball: underworld, temporarily
- [x] Power Glove: Sea monster (Labyrinth).
- [x] Marionette: underworld, temporarily
- Can add as many purse upgrades as we like. I guess no more than 9, so it stays within 3 digits?

- Stories
- `story1` `NS_fld_mayor`
- `story2` `NS_fld_war_over`
- `story3` `NS_fld_kidnapped`
- `story4` `NS_fld_recued_princess`
- `story5` `NS_fld_escaped_labyrinth`
- `story6` `NS_fld_root7`: Desert root devil.
- `story7` `NS_fld_barrelhat_all`
- `story8` `NS_fld_hc3`: Too Many Things = Pass inventory critic. Not perfect.
- `story9` Broom Races: TODO
- `story10` World Traveller: TODO (and not at all sure that it will be "World Traveller")
- `story11` On Carpentry: TODO ('')
- `story12` De Re Piscatarii: TODO ('')
- `story13` `NS_fld_root_all`
- `story14` The Witch With Lots Of Heart: TODO (could be anything)
- `story15` Pocketfuls of Gold: TODO ('')
- `story16` The Brewing of Potions: Sell directly at the potion shop.

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

## Morally Questionable

I'm keeping this game appropriate for children in my opinion, like all of Dot Vine's games.
Record everything that someone might object to on moral grounds, so we can declare it all up front.

- Witchcraft. No getting around that!
- Stealing Contest.
- Election rigging.
- Shaking Contest (champagne).
- Telekinesis Contest (drinking game).
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
- Layout of the labyrinth. (actually not constant even within one campaign).
- Position of the remote surveyor points (and therefore distances).

## Lessons Learned

Collecting lil dev things here, since it's such a large project. Write up a neat recap around release time.

- Egg is awesome.
- Stitching together single-screen maps was probably a mistake. I think one map per plane would have worked smoother.
- The "activity" abstraction works great, I should do something similar in every big game.
- Monthly goals and reports, my estimates were all way off, but this seems a healthy practice. Make a habit of it for large games.
- Pick an orientation for sprites! I've settled on rightward as the default, but some early sprites (eg Dot) are leftward. Good to be consistent about that.

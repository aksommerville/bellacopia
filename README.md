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

## TODO

- [ ] Inside the temple, compass points you to the front door for the root devil and the heart container.
- - I don't think we need to solve this generally, but can we make it point to the pool door instead? (that would be wrong if it's pointing to anything else, but I think that's less bad than current).
- - UPDATE: Also impacts hc4, and expect more. I think we do need a general solution.
- - Delay this, because I'm doubting now. We don't want the compass (or any hints) to be perfect. Maybe it's ok to leave just as it is?
- [ ] Animate digging with shovel.
- [ ] Dig in an incorrect spot, occasionally dig up a really difficult monster. To raise the cost of guessing.
- - Maybe: "Skeleton challenges you to a Shovel Throwing Contest", he yoinks your shovel and throws it and you have to chase it. No prize.
- [ ] Full clear time doesn't register immediately, if you achieve it by finishing the maps.
- [ ] Populate Ice Palace.
- [ ] Ice Dragon. 6 battles.
- [ ] Check ladders in the outerworld, they probably all need some safe buffer.
- [ ] chopping: Try adding one real item. Fish, coin, vanishing cream, .... If you win the game and don't chop it, you get it.
- [ ] Hide more jigpieces. It's easy to bury them or hide underwater.
- [ ] Gambling challenge in the casino. Choose a difficulty, implies a wager and payout, and play a random battle.
- [ ] Maybe a warning when you leave a guild with the endorsement partially won? User wouldn't assume that it resets.
- [ ] Acquire stories.
- [ ] Some fanfare and cooldown at gameover. See `sprite_hero.c:hero_hurt()`
- [ ] Usable IP for the erudition contest.
- [ ] Might be cool to re-engage with the Princess after her quest. Could do further side quests like "will you show me the jungle temple?". Or one-on-one practice battles.
- [ ] Is it possible to reach inconsistent states by pausing while item in progress?
- [ ] Remove the fake French text, or even better, get it translated correctly.
- [ ] Modal blotter: Can we have the generic layer track changes and ease in and out? Then also we would want the implementations to turn off their (blotter) request during animated dismissal.
- [ ] `camera_warp()` updates the hero's position immediately, so she blinks out during the transition.
- - We're only using it for wand, and the effect is agreeable. But might need mitigation if we use for other things.
- [ ] Crying Contest: No handicap variance in 2-player mode. Should we force some?
- [ ] Is it possible to render Racketeering Contest to work with red-and-blue 3D glasses?
- [ ] Racketeering contest badly needs more juice when you hit the ball.
- [ ] Make something happen if you beat a guild outside the election.
- [ ] Rearrange Fractia to make the log and Board of Elections more prominent.
- [ ] Should firepot be switchable? It wouldn't take much. But I don't have a use for it lined up.
- [ ] Offeratory box in the temple where you can drop a coin, then the next time you go fishing you'll catch a red fish.
- [ ] Some kind of lucky charm that makes the next battle minimum difficulty. Very hard to get, but also repeatable. So there's always a way to win any battle, if you work for it.
- - [ ] Make it a spell: The Spell of Taming. Must cast close to the monster to be tamed, so there's some challenge and inconvenience to it. No effect on Root Devils.
- [ ] More spells. Not sure what...
- [x] Properer graphics for Crystal Ball. Very rough today.
- [x] Content for Crystal Ball. `targets.c:game_get_advice()`. Should follow roughly the same pattern as the compass, choose the logical next step.
- [ ] Parasites in the sea monster.
- [ ] I don't like how camera briefly returns to the outerworld when getting swallowed by sea monster.
- [x] The temple's pool needs a sunbathing monk with a pina colada.
- [ ] Make the songs longer. Aim for 2 minutes per song.
- [ ] For purposes of inventory completion, maybe we should flag "intermediate" items like Barrel Hat and Letter.
- [ ] I think the recorded 100% Time can get reset.
- [ ] Find more opportunities for special battle prizes like Stealing and Fishing.
- [ ] Mind Control Contest: Make a more continuous connection state, like sometimes the connection is better than others.
- [ ] Fishing contest: Smarter cvc decisions; right now they run exactly the same.
- [ ] Maybe after so many tree stories are told, the trees can give hints. "I'd love to hear a story about the jungle temple" kind of thing.
- [ ] Some dialogue ends up with two lines and a single word on the second line. Can we break text more balancedly?
- [ ] Maybe an exterminator in the outer world? He's heading to one region to clear out the monsters, but he stopped for lunch. Bring something expensive like four red fish. One exterminator at a time.
- - The intent is a high-cost option to eliminate battles for players that really just can't. So at the limit, they can fish their way out of most battles.
- [x] Move the Labyrinth's exit to the southeast corner, and add at least one more landmark somewhere.
- [x] chess: Do more to highlight the enemy king's position at startup.
- [ ] Big signs to make the city names unmistakable.
- [ ] Nerf the building contest.
- [ ] What happens if you warp out of jail? Wand or bus stop. It should count as escaping.
- [ ] When does `cryptmsg_require` get called? I was about halfway thru and `cryptmsg_seed` was already set, despite never having seen an encrypted message or stood on a seal.

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
- Manual validation before release.
- [ ] Minimal completion possible.
- [ ] 100% completion possible.
- [ ] Crystal Ball, compass, and cartographer always give sane advice.
- [ ] Every battle plays sensibly in arcade mode.

- Promo merch. Plan to order all by early July, well in advance of Matsuricon and GDEX.
- GDEX being in mid-October, let's set a drop-dead date of 6 September. Order things by then or don't order.
- - [ ] Pins, buttons, stickers.
- - [ ] Homemade stuffed witches, if I can work that out.
- - - There are mail-order companies that do this, eg customplushmaker.com. Long lead times (~90 days), and I don't know about pricing.
- - - ^ prefer bearsforhumanity.com
- - - ...but I really would prefer "Made in Flytown"
- - - Whatever we're doing, figure it out by the end of April.
- - - A semicircle of felt 12cm diameter rolls up into a pretty good witch hat, just the size for a fist puppet.
- - - - The white felt sheets I already have are the perfect size for this, alas they're white and not purple.
- - [ ] Witch hats. Same idea as the dolls.
- - [ ] Jigsaw puzzles? Would be on-theme, and I've ordered these before, it's a snap. Too expensive to give away probly (~$30 ea at a glance).
- - [ ] Mini comic?
- - - If we draw one, get in touch with Hardwired, Back Alley Games, and similar, see if they'll publish it.
- - [ ] Thumb drives. I still have 20 leftover from Spelling Bee, if we want to make the shell ourselves.
- - - customusb.com has some gorgeous shells and cases (they did the Plunder Squad bottles). $5-10 ea for the drives and another $5-10 for cases. A bit much.
- - [ ] Instruction manual + strategy guide, to bundle with thumb drives.
- - [ ] Book of sheet music.
- - [ ] Videos.
- - [ ] Book of Cheating. Maybe a digital edition?
- - [ ] Big banners, the kind that roll up into a case.
- - - $130 at bannerbuzz.com.

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
- [x] Snowglobe: underworld, temporarily
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
- `story5` `NS_fld_root4`: Labyrinth story. Completes when you strangle the root devil, not a perfect trigger.
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
- Telekinesis Contest: Reference to the old janx spirit drinking game in Hitchhiker's Guide to the Galaxy.

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

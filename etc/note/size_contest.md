# How big is Bellacopia compared to my other games?

By any reasonable standard, it's the largest game I've ever written.
It amuses me to quantify this.

Stats for bellacopia are from dfe4f66a58280d502f690ccb7c6323b81f86e50c, 15 July 2026. Still very incomplete.
Currently the largest by 3/8 criteria.

Would be cool to compare to others' games too, especially broadly-familiar ones like Zelda.

## Distributable size

SUMMARY: Hopefully will not be first. I expect 3 MB or so in the end.

Using the same standard as my [journal](https://github.com/aksommerville/journal/games/README.md) repo, ie the ROM size for Bellacopia.

- 4816896 Sitter 2009. Bloated due to storing graphics in individual files, PCM sound effects, and unwise formats for song and map.
- 3563520 Bandit. Bloated due to uncompressed graphics, and surely lots of other poor design choices.
- 3051520 Master Zen. ''
- 2448768 Campaign Trail of the Mummy. Lots of high-resolution graphics. Not super wasteful tho.
- 1607415 <<< Bellacopia Maleficia.
- 1494423 Secret of the Octopotamus. Pygame, so the bloated source is part of the distro.
- 1243282 Full Moon.
- 1189146 Sitter 3. Pygame.
- 1163062 Plunder Squad.
-  854630 Spelling Bee. Next largest Egg game (and also a conspicuous outlier!).

## Source line count

SUMMARY: Will surely be first, and my first project over 100k lines.

Bear in mind that Bellacopia is an Egg game, so its source doesn't include drivers or much tooling.
Source for everything before Secret of the Octopotamus has been lost. I doubt there's anything above 10k from those days.

-  96226 Plunder Squad. Includes drivers and tooling.
-  87747 Full Moon. Includes drivers and tooling.
-  72145 <<< Bellacopia Maleficia.
-  22247 Campaign Trail of the Mummy.
-  21584 Sitter 2009.
-  20009 Ivan Denisovich. No idea why this ended up so big.

## Graphics size

SUMMARY: Already first, by far.

Combined pixel count of all images shipped with the game.
`egglist list -fsize DIRECTORY` if they're stored loose.
Bandit and Master Zen are unknown; got to figure out how to decode their graphics. I bet they're both over a million.

- 4616044 <<< Bellacopia Maleficia.
- 2412544 Campaign Trail of the Mummy.
- 1787678 Spelling Bee.
- 1594368 Full Moon.
- 1407119 Season of Penance.
- 1037824 Sitter 2009.
-  311296 Plunder Squad.

## Music duration

SUMMARY: Unlikely to be first but second seems likely. I'm going to revisit the music, and aim for 2 minutes per song. So expect about 30 min total.

Bandit, Sitter 2009, and Plunder Squad all have substantial music, but all in tricky formats.

- 1:03:27 (19) Master Zen. Expect this record to stand for all time!
- 0:17:40 (14) Full Moon.
- 0:10:40 (14) Spelling Bee.
- 0:10:33 (17) <<< Bellacopia Maleficia.

## World size in screenfuls

SUMMARY: Already first, by far.

Bandit and Zen are not measured, would need some figuring out.

- 354 (84960m) <<< Bellacopia Maleficia (map count).
- 240 (57600m) Campaign Trail of the Mummy. Doesn't have a fixed screensize, count is based on a made-up 20x12m screen.
- 205          Plunder Squad (blueprint count).
- 150          Full Moon (map count).
-  78 (20885m) Sitter 2009. Assuming a 266-meter screen, because that's the most common map size.
-  50 (11208m) Spelling Bee.

## Minimum play time, story mode

SUMMARY: Will probably not be first, since we are keeping story mode deliberately lean. My time here would never happen to a first-time player.

How fast can I reach the end credits?
Bandit and Zen are unknown, and I can't reliably run either right now. They're both big.
My little games are almost always under 0:05, and often under one minute.
Plunder Squad doesn't count because it's configurable.
Campaign Trail of the Mummy doesn't count because it's on a timer.

- 0:36:46 Sitter 2009. Cooperative mode, just me. After a few years away, and some mistakes were made.
- 0:22:29 Tag Team Adventure Quest. Playing solo, and it's been a while.
- 0:18:11 <<< Bellacopia Maleficia.
- 0:12:41 Spelling Bee. Takes some knowledge. A more naive path takes more like one hour.
- 0:02:19 Full Moon. Using some very non-obvious tricks. Most first-time players take a few hours.

## Minimum full clear time

SUMMARY: Already first, by a ridiculous margin, and will get bigger. Our full clear is a much more involved thing than any game I've made in the past.

For games that distinguish Full Clear from Any%.

- 2:14:00 <<< Bellacopia Maleficia.
- 0:21:23 Spelling Bee.
- 0:03:59 Full Moon. Kind of ridiculous.

## Development time

SUMMARY: Likely to be first, tho I still hold out hope of finishing in 2026.

Measure via git commits. Only count thru the release, don't count post-release repairs or currency.
Nothing older than CTM has reliable version control logs; they were all gitted after the fact if at all.
Bandit and Zen were both huge projects, probably in the neighborhood of 6 months.

- 380 2017-09-15..2018-09-30 Plunder Squad. First commit says "begin version control". Unclear how much work had been done before that; can't be much.
- 265 2023-01-06..2023-09-29 Full Moon.
- 208 2025-12-18..2026-07-15 <<< Bellacopia Maleficia.
-  77 2024-10-07..2024-12-23 Spelling Bee.
-  53 2015-08-12..2015-10-04 Campaign Trail of the Mummy. Work had been done before version control.

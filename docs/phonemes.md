# Phoneme table

A phoneme knob's value divided by 7 selects one of these. The formant columns are the bytes the engine actually sends to the tract and what they come to in Hz (one unit is 19.53 Hz).

| No. | Knob value | Phoneme | f1 / f2 / f3 / f4 (bytes) | f1 / f2 / f3 / f4 (Hz) |
|---|---|---|---|---|
| 0–2 | 0–20 | silence | unchanged | |
| 3 | 21–27 | s**oa**p | 27 / 48 / 122 / 163 | 527 / 937 / 2383 / 3183 |
| 4 | 28–34 | b**ee**t (breathy) | 15 / 102 / 158 / 189 | 293 / 1992 / 3086 / 3691 |
| 5 | 35–41 | h**a**t | 37 / 87 / 128 / 174 | 723 / 1699 / 2500 / 3398 |
| 6 | 42–48 | h**o**t | 38 / 53 / 122 / 163 | 742 / 1035 / 2383 / 3183 |
| 7 | 49–55 | bo**rr**ow | 29 / 61 / 76 / 240 | 566 / 1191 / 1484 / 4687 |
| 8 | 56–62 | ba**ll** | 28 / 43 / 133 / 184 | 547 / 840 / 2597 / 3594 |
| 9 | 63–69 | ee | 23 / 170 / 253 / 44 | 449 / 3320 / 4941 / 859 |
| 10 | 70–76 | i | 38 / 172 / 225 / 42 | 742 / 3359 / 4394 / 820 |
| 11 | 77–83 | e | 46 / 163 / 217 / 27 | 898 / 3183 / 4238 / 527 |
| 12 | 84–90 | a | 58 / 150 / 213 / 27 | 1133 / 2930 / 4160 / 527 |
| 13 | 91–97 | o | 65 / 92 / 208 / 36 | 1269 / 1797 / 4062 / 703 |
| 14 | 98–104 | oh | 48 / 75 / 204 / 36 | 937 / 1465 / 3984 / 703 |
| 15 | 105–111 | foot | 35 / 83 / 200 / 36 | 684 / 1621 / 3906 / 703 |
| 16 | 112–118 | boot | 23 / 71 / 200 / 36 | 449 / 1387 / 3906 / 703 |
| 17 | 119–125 | r | 47 / 100 / 125 / 254 | 918 / 1953 / 2441 / 4961 |
| 18 | 126–132 | l | 47 / 68 / 225 / 44 | 918 / 1328 / 4394 / 859 |
| 19 | 133–139 | uh | 58 / 108 / 217 / 2 | 1133 / 2109 / 4238 / 39 |
| 20–33, 36 | 140–237, 252–255 | noise | 20 / 43 / 143 / 192, with frication on | 391 / 840 / 2793 / 3750 |
| 34 | 238–244 | the word "blah" | several frames | |
| 35 | 245–251 | sustained "ah", slightly nasal | 37 / 53 / 128 / 178 | 723 / 1035 / 2500 / 3476 |

## Why phonemes 9–19 look wrong

They are faithful to the original. `Voice.spin` converts its vowel table (in Hz) with a divisor of 12 where the tract's unit calls for 19.53, and stores the result in a byte without clamping. So those eleven vowels sit about 1.6 times higher than the table says, and every f4 except r's overflows the byte and wraps around to a low value. For example ee's f4 of 3600 Hz becomes 300, which is stored as 44, or 859 Hz.

That is how the instrument has always sounded, so the port keeps it. Correcting the table would be a change to the instrument's voice, not a bug fix, and belongs behind a setting if it is ever wanted.

## Other things a phoneme sets

- Phonemes 3–19 set the glottal amplitude to 30 (before the key back-off); the phase's volume knob then replaces it.
- Phonemes 9–19 turn frication off. Phonemes 3–8 leave it as it was, so after the noise phoneme they keep hissing until one of 9–19 is used.
- Nothing but the word and "ah" phonemes touch the nasal settings, and only the closing fade (used by Words and Formants modes and on a mode change) turns them off.

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
| 9 | 63–69 | ee | 14 / 104 / 155 / 184 | 273 / 2031 / 3027 / 3594 |
| 10 | 70–76 | i | 23 / 105 / 138 / 182 | 449 / 2051 / 2695 / 3554 |
| 11 | 77–83 | e | 28 / 99 / 133 / 174 | 547 / 1933 / 2597 / 3398 |
| 12 | 84–90 | a | 35 / 92 / 130 / 174 | 684 / 1797 / 2539 / 3398 |
| 13 | 91–97 | o | 39 / 56 / 128 / 179 | 762 / 1094 / 2500 / 3496 |
| 14 | 98–104 | oh | 29 / 46 / 125 / 179 | 566 / 898 / 2441 / 3496 |
| 15 | 105–111 | foot | 21 / 51 / 122 / 179 | 410 / 996 / 2383 / 3496 |
| 16 | 112–118 | boot | 14 / 43 / 122 / 179 | 273 / 840 / 2383 / 3496 |
| 17 | 119–125 | r | 28 / 61 / 76 / 156 | 547 / 1191 / 1484 / 3047 |
| 18 | 126–132 | l | 28 / 41 / 138 / 184 | 547 / 801 / 2695 / 3594 |
| 19 | 133–139 | uh | 35 / 66 / 133 / 158 | 684 / 1289 / 2597 / 3086 |
| 20–33, 36 | 140–237, 252–255 | noise | 20 / 43 / 143 / 192, with frication on | 391 / 840 / 2793 / 3750 |
| 34 | 238–244 | the word "blah" | several frames | |
| 35 | 245–251 | sustained "ah", slightly nasal | 37 / 53 / 128 / 178 | 723 / 1035 / 2500 / 3476 |

## Where phonemes 9–19 differ from the original

`Voice.spin` converted this group's vowel table (in Hz) with a divisor of 12 where the tract's unit calls for 19.53, and stored the result in a byte without clamping. Those eleven vowels came out about 1.6 times too high, and every f4 except r's overflowed the byte and wrapped to a low value. For example ee's f4 of 3600 Hz became 300, stored as 44, which is 859 Hz.

The port corrects this: the table is converted with the same formula as phonemes 3–8, so the vowels sit at the frequencies the table names. These eleven phonemes therefore sound different from the original instrument.

## Other things a phoneme sets

- Phonemes 3–19 set the glottal amplitude to 30 (before the key back-off); the phase's volume knob then replaces it.
- Every phoneme starts with frication and the nasal branch off. Only the noise phoneme turns frication on, and only the word and "ah" phonemes turn the nasal branch on.
- Silence, including the fade at the end of every release, turns frication off as well as the voice and breath.

The original did not reset these. A voice that had played the noise phoneme went on hissing after its key was released, through every later note, until the mode switch was moved; and the nasal setting of "ah" or the word stayed with the voice in the same way.

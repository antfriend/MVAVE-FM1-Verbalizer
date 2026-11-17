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
| 20 | 140–146 | noise | 20 / 43 / 143 / 192, with frication on | 391 / 840 / 2793 / 3750 |
| 21–33 | 147–237 | added consonants, see below | | |
| 34 | 238–244 | the word "blah" | several frames | |
| 35 | 245–251 | sustained "ah", slightly nasal | 37 / 53 / 128 / 178 | 723 / 1035 / 2500 / 3476 |
| 36 | 252–255 | added consonant G, see below | | |

## Added consonants

In the original, numbers 20–33 and 36 were all the same noise phoneme. Noise stays at 20; the other fourteen are now consonants, which use the tract's nasal branch, frication and breath. A build can leave them out (`VB_NO_EXTRAS`), and then they are noise again; the combined FM-1 firmware does.

| No. | Knob value | Phoneme | How it is made | f1 / f2 / f3 / f4 (Hz) |
|---|---|---|---|---|
| 21 | 147–153 | m | voice, nasal notch at 1000 Hz | 250 / 1100 / 2200 / 3300 |
| 22 | 154–160 | n | voice, nasal notch at 1500 Hz | 250 / 1500 / 2600 / 3400 |
| 23 | 161–167 | ng | voice, nasal notch at 3000 Hz | 250 / 2100 / 2700 / 3400 |
| 24 | 168–174 | w | voice | 290 / 610 / 2150 / 3300 |
| 25 | 175–181 | y | voice | 260 / 2200 / 3100 / 3700 |
| 26 | 182–188 | h | breath only | 500 / 1500 / 2500 / 3500 |
| 27 | 189–195 | f | weak hiss at 7500 Hz, a little breath | 400 / 1100 / 2400 / 3500 |
| 28 | 196–202 | s | hiss at 6200 Hz | 300 / 1700 / 2600 / 3500 |
| 29 | 203–209 | sh | hiss at 2700 Hz | 300 / 2000 / 2500 / 3400 |
| 30 | 210–216 | v | f with some voice | 300 / 1100 / 2400 / 3500 |
| 31 | 217–223 | z | s with some voice | 300 / 1700 / 2600 / 3500 |
| 32 | 224–230 | b | a quiet voiced closure | 200 / 800 / 2200 / 3300 |
| 33 | 231–237 | d | a quiet voiced closure | 200 / 1700 / 2700 / 3500 |
| 36 | 252–255 | g | a quiet voiced closure | 200 / 2100 / 2400 / 3400 |

How they behave:

- **They are targets, like the vowels.** A stop such as b is heard in the glide between it and the vowel next to it, so it works as a trigger or a release with a short time, and as a sustain it is only a murmur.
- **The phase's volume knob scales the whole sound**, hiss and breath included. With the original's noise phoneme the knob only sets the voice under a fixed hiss.
- **Each has its own loudness** relative to a vowel at the same volume: nasals and glides are quieter, closures much quieter.
- The hiss and the nasal notch start at their own frequency instead of sweeping to it from wherever the last sound left them.
- They have been checked on spectrograms and balanced for loudness, but **not yet tuned by ear**.

`host/scores/consonants.txt` plays each one as a trigger and as a release.

## Where phonemes 9–19 differ from the original

`Voice.spin` converted this group's vowel table (in Hz) with a divisor of 12 where the tract's unit calls for 19.53, and stored the result in a byte without clamping. Those eleven vowels came out about 1.6 times too high, and every f4 except r's overflowed the byte and wrapped to a low value. For example ee's f4 of 3600 Hz became 300, stored as 44, which is 859 Hz.

The port corrects this: the table is converted with the same formula as phonemes 3–8, so the vowels sit at the frequencies the table names. These eleven phonemes therefore sound different from the original instrument.

## Other things a phoneme sets

- Phonemes 3–19 set the glottal amplitude to 30 (before the key back-off); the phase's volume knob then replaces it.
- Every phoneme starts with frication and the nasal branch off. Of the original's phonemes only noise turns frication on, and only the word and "ah" turn the nasal branch on.
- Silence, including the fade at the end of every release, turns frication off as well as the voice and breath.

The original did not reset these. A voice that had played the noise phoneme went on hissing after its key was released, through every later note, until the mode switch was moved; and the nasal setting of "ah" or the word stayed with the voice in the same way.

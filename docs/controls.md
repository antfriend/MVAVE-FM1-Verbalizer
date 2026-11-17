# Controls of the original, as the engine sees them

The engine keeps the original Verbalizer's control model: sixteen knobs with values 0–255 (`verbalizer.pot[]`), a three-way mode switch and a keyboard. The FM-1 firmware will page its seven encoders onto these sixteen. The names in the tables are the `VB_POT_*` constants in [engine/voice.h](../engine/voice.h).

## Keys and pitch

Keys use the original's numbering. Key 1 is A1 (55 Hz) and each key is one semitone, so `key = MIDI note − 32`. The original keyboard had keys 1–37 (A1 to A4). The engine accepts keys −12 to 51 (MIDI 20 to 83), which is the range the tract's pitch byte can hold.

Notes can also carry a velocity, bend in pitch and be held by a sustain pedal. Those arrive over MIDI and are described in [midi.md](midi.md).

Glottal volume is backed off as keys go up: `key / 2` is subtracted from the requested amplitude, stopping at silence. On the top key of the original keyboard (37) that is 18, so a volume knob below about 57 is silent there, and below about 21 at key 13.

## Trigger / Sustain / Release mode

Each phase of a keystroke has five knobs.

| Knob | Trigger | Sustain | Release | Scaling |
|---|---|---|---|---|
| Phoneme | 0 | 5 | 10 | value / 7 picks a phoneme, see [phonemes.md](phonemes.md) |
| Time | 1 | 6 | 11 | value × 2 glide steps of 700 µs (0 to 357 ms) |
| Volume | 2 | 7 | 13 | value / 3 becomes the glottal amplitude |
| Vibrato depth | 3 | 8 | 14 | value / 3, in 1/48 octave |
| Vibrato rate | 4 | 9 | 15 | value / 3, in 0.0763 Hz steps |

Knob 12 set the amount of echo on the original (the spatializer's settings). It does nothing yet.

What a keystroke does:

1. **Key down.** A free voice glides to the trigger phoneme over the trigger time.
2. **Key held.** Whenever the voice runs out of queued frames it glides to the sustain phoneme over the sustain time, again and again, so sustain knob changes are heard while a key is down.
3. **Key up.** The voice glides to the release phoneme over the release time, then fades to silence in 7 ms.

There are four voices. A fifth key waits until a voice has finished. A key that is struck again while its own release is still sounding also waits for that release to end.

## Words mode

| Knob | Use |
|---|---|
| 0 | Which word (value / 15) |
| 1 | Word duration |

Each key says the chosen word at its pitch. The word ends on its own; holding the key does nothing more. The falling tail comes from the closing fade gliding to key 1's pitch.

| Knob 0 | Word | | Knob 0 | Word |
|---|---|---|---|---|
| 0–14 | blah | | 135–149 | mama |
| 15–29 | yeah | | 150–164 | no |
| 30–44 | wow | | 165–179 | now |
| 45–59 | why | | 180–194 | one |
| 60–74 | you | | 195–209 | amen |
| 75–89 | oh | | 210–224 | yes |
| 90–104 | la | | 225–239 | hello |
| 105–119 | ah-oo | | 240–255 | robot |
| 120–134 | really | | | |

"blah" is the original's one word, and knob 0 did nothing in the original. The other sixteen are additions. In them the duration knob stretches the vowels and glides and leaves the consonants as they are, and each has a small rise or fall of pitch built in. They have been balanced for loudness and checked on spectrograms, but **not yet tuned by ear**: expect some to need work.

A build can leave the additions out (`VB_NO_EXTRAS`, see `engine/vb_platform.h`); then "blah" is the only word. The combined FM-1 firmware does, for lack of room.

## Formants mode

The knobs set the tract's parameters directly. Only voice 0 plays. While keys are held the knobs are read again every time the voice's queue empties; several held keys take turns.

| Knob | Parameter | Scaling |
|---|---|---|
| 0 | `aa` aspiration amplitude | direct |
| 1 | `f4` | direct, 19.53 Hz units |
| 2 | `f3` | direct |
| 3 | `f1` | direct |
| 4 | `f2` | direct |
| 5 | pitch offset | value / 21 − 6 keys (128 is no offset) |
| 6 | glide time | value × 2 steps of 700 µs |
| 7 | `ga` glottal amplitude | value / 3 |
| 8 | `vp` vibrato depth | value / 3 |
| 9 | `vr` vibrato rate | value / 3 |
| 10 | `fa` frication amplitude | direct |
| 13 | `ff` frication frequency | direct, 39.06 Hz units |
| 14 | `na` nasal amplitude | direct |
| 15 | `nf` nasal frequency | direct, 19.53 Hz units |

Knobs 11 and 12 do nothing in this mode.

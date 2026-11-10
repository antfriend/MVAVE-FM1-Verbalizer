# Controls of the original, as the engine sees them

The engine keeps the original Verbalizer's control model: sixteen knobs with values 0–255 (`verbalizer.pot[]`), a three-way mode switch and a keyboard. The FM-1 firmware will page its four knobs onto these sixteen. The names in the tables are the `VB_POT_*` constants in [engine/voice.h](../engine/voice.h).

## Keys and pitch

Keys use the original's numbering. Key 1 is A1 (55 Hz) and each key is one semitone, so `key = MIDI note − 32`. The original keyboard had keys 1–37 (A1 to A4). The engine accepts keys −12 to 51 (MIDI 20 to 83), which is the range the tract's pitch byte can hold.

Glottal volume is backed off as keys go up: a requested amplitude above `key / 2` is reduced by `key / 2`.

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
| 1 | Word duration |
| 11 | value / 7 picks the phoneme whose formants the word's closing fade glides towards (0 leaves them alone) |

Each key says the one built-in word ("blah") at its pitch. The word ends on its own; holding the key does nothing more. The falling tail comes from the closing fade gliding to key 1's pitch.

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

Knobs 11 and 12 are not tract parameters in this mode. Knob 11 still picks the phoneme used for the fade when a key is released, as it does in the original; leave it at 0 for a plain fade.

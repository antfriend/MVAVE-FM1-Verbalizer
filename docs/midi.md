# MIDI

`vb_midi()` in [engine/verbalizer.h](../engine/verbalizer.h) takes one channel message at a time. The channel is ignored. This is the engine's side only: nothing yet reads a MIDI port or file, and the FM-1 firmware will be what calls it.

## Messages

| Message | Effect |
|---|---|
| Note on, notes 20–83 | Starts the note. Other notes are ignored. Velocity 0 is a note off. |
| Note off | Releases the note. |
| Pitch bend | ±2 semitones, in quarter-semitone steps. |
| CC 64, sustain pedal | 64 and above is down. Notes released while it is down keep sounding until it comes up. |
| CC 102–117 | Set knobs 0–15. The value 0–127 is spread over the knob's 0–255. |
| CC 118 | Mode: 0–42 Words, 43–85 Trigger / Sustain / Release, 86–127 Formants. |
| CC 119 | Crackle: 64 and above brings back the original's overflow noise. |
| CC 120, all sound off | Drops every key and fades all voices at once. |
| CC 123, all notes off | Releases every key, pedal or not. |

Anything else is ignored and `vb_midi()` returns `false`.

What each knob does in each mode is in [controls.md](controls.md). In Formants mode the knobs are the tract's parameters, so CC 102–117 reach all thirteen of them. Three of those (`ga`, `vp`, `vr`) are divided by 3 on the way, as they were on the original, so they stop at a third of the tract's full range, and pitch moves in whole keys.

## Velocity

Velocity scales the loudness of everything the note queues: the glottal, aspiration and frication amplitudes are multiplied by velocity / 127. At 127 the note plays exactly what the volume knobs say. The original had no velocity, so 127 is also what the plain `vb_key_down()` and `vb_note_on()` calls use.

## Pitch bend

The tract's pitch unit is 1/48 octave, a quarter of a semitone, and bend moves in those steps: eight down, seven up.

Bend is applied when frames are queued, not continuously. A held note in Trigger / Sustain / Release mode picks it up each time its sustain phase repeats, so how quickly it follows the wheel depends on the sustain time knob. With a short sustain time it tracks closely; with a long one it lags and glides.

## Very short notes

Keys are looked at every 2 ms. A note that starts and ends inside one of those windows still plays, at the shortest length the patch allows: its trigger phase followed at once by its release. If all four voices are busy it waits up to 100 ms for one, then is dropped. A held key waits for as long as it is held.

# MVAVE FM1 Verbalizer

A vocal-tract synthesizer firmware for the M-VAVE FM-1 pocket synth: a port of [The Verbalizer](https://github.com/antfriend/Verbalizer), a one-of-a-kind formant instrument originally built on the Parallax Propeller.

Play phonemes, formants and word fragments from the FM-1's keys instead of FM patches.

> **Status: the sound engine runs on a desktop, and a first test firmware builds but has never been run on an FM-1.** The vocal tract and the voice logic are ported to portable C and render WAV files on a PC. A test firmware with the engine inside M-VAVE's open firmware compiles and packages; do not flash it without a recovery method ready. Sections marked *planned* are not implemented.

> **Not affiliated with M-VAVE.** Custom firmware is installed at your own risk. See [Safety](#safety-and-recovery).

## What it is

The original Verbalizer drives four copies of Chip Gracey's `VocalTract` object, a real-time model of a human vocal tract controlled by 13 parameters:

| Parameter | Meaning |
|---|---|
| `aa` | aspiration (breath) amplitude |
| `ga`, `gp` | glottal (voice) amplitude and pitch |
| `vp`, `vr` | vibrato depth and rate |
| `f1`–`f4` | the four formant resonator frequencies |
| `na`, `nf` | nasal anti-resonator amplitude and frequency |
| `fa`, `ff` | frication (noise) amplitude and frequency |

Each keystroke moves through three phases (trigger, sustain, release), and each phase has its own phoneme and duration. That is what makes it speak rather than just sing a vowel.

This project moves that engine onto the FM-1, which adds a screen, patch storage and MIDI over USB, DIN and Bluetooth.

## Why the FM-1

- The FM-1's JieLi AC791N runs at 240 MHz and already renders 12 voices of 6-operator FM. One vocal tract needs a single 80 MHz Propeller cog.
- The tract is integer-only DSP (shift-add multiply and CORDIC), so it needs no FPU.
- M-VAVE publishes an open firmware and SDK for the FM-1, with the hardware code in source, an installer and a route back to the official firmware. There is an active custom-firmware scene around it.

## Planned design

### Modes

The original's three-position mode switch becomes three modes:

| Mode | What it does |
|---|---|
| **Words** | Each key triggers a word; a knob sets its duration. |
| **Trigger / Sustain / Release** | The main mode. Key down, hold and key up each play their own phoneme. |
| **Formants** | Direct control of all 13 tract parameters for building new sounds. |

### Controls

The original has about sixteen dedicated knobs. The FM-1 has seven endless encoders and one analogue knob, plus 27 note keys (F3 to G5), 14 buttons and a 240 × 240 screen, so parameters are paged across the encoders.

| Page | Enc 1 | Enc 2 | Enc 3 | Enc 4 | Enc 5 | Enc 6 | Enc 7 |
|---|---|---|---|---|---|---|---|
| Trigger | phoneme | duration | volume | vibrato depth | vibrato rate | — | — |
| Sustain | phoneme | duration | volume | vibrato depth | vibrato rate | — | — |
| Release | phoneme | duration | volume | vibrato depth | vibrato rate | — | echo |
| Formants A | `f1` | `f2` | `f3` | `f4` | `na` | `nf` | glide time |
| Formants B | `ga` | `aa` | `fa` | `ff` | `vp` | `vr` | pitch offset |

Each phase of a keystroke has a page to itself, with the same five controls in the same places, and the fourteen Formants controls fill two pages exactly. Every control the engine has is on a page. In Formants mode the pitch `gp` comes from the key plus the pitch offset. Words mode uses the Trigger page's duration. The echo control does nothing until the spatializer is ported. The analogue knob and the buttons are not assigned yet. [docs/controls.md](docs/controls.md) says what each control does.

This layout is a draft and will change once it is playable.

### New on the FM-1 (*planned*)

- Save Formants-mode creations as user phonemes, usable in the Trigger / Sustain / Release mode.
- MIDI CC control of all 13 tract parameters. *Done in the engine:* controllers 102–117 set the sixteen knobs, which in Formants mode are the tract parameters. Velocity, pitch bend and the sustain pedal work too. See [docs/midi.md](docs/midi.md).
- An on-screen vocal-tract diagram showing the live parameter values.
- Saturation in place of the original's overflow crackle, with the crackle kept as a setting. *Done in the engine:* overdriven voices clip by default, and `vb_set_crackle` brings the crackle back.

## Porting plan

1. **`VocalTract` in C.** *Done.* The Propeller assembly is translated to portable integer C. Its output matches the original assembly sample for sample, checked by running that assembly in an emulator. Comparing against recordings of the original hardware is still to do.
2. **Sample rate.** *Done.* Everything in the tract depends on its 20 kHz rate, so the engine keeps running at 20 kHz and stays bit-exact. A resampler converts its output to the FM-1's fixed 44.1 kHz and delivers it in the FM-1's block format (128 stereo frames, 24-bit).
3. **Voice logic.** *Done.* `Voice.spin` and the key handling are ported: the phonemes, the three modes and four-voice allocation. The engine takes MIDI messages (notes with velocity, pitch bend, sustain pedal, controllers for every knob). The desktop build plays it from a text score; it does not read MIDI files or ports.
4. **FM-1 integration.** *Started.* A first test firmware builds: the engine inside M-VAVE's official firmware, playing beside the stock instrument from the keys and MIDI, with one fixed patch and no screens of its own. It has not been run on a device. See [firmware/README.md](firmware/README.md); [docs/fm1-platform.md](docs/fm1-platform.md) records what the SDK requires. Still to do: the paged UI, patch storage, and making room for them, since the FM-1's program flash is full.
5. **Hardware testing.** *Not started.* CPU load with four voices, stereo output for the spatializer, and battery behaviour.

The stereo spatializer, which also gave the original its echo, is not ported. The engine mixes its voices to mono for now.

[docs/porting-notes.md](docs/porting-notes.md) has the details, including where the C code differs from the original on purpose.

## Repository layout

```
engine/     portable C vocal tract, voice logic and resampler, no hardware dependencies
host/       desktop tools: render scores and tract scripts to WAV files
tests/      engine tests and the reference hashes from the original assembly
tools/      table generators and the reference emulator for VocalTract.spin
docs/       control map, phoneme table, MIDI map, porting notes, FM-1 platform notes
firmware/   glue, patch and install script that put the engine into M-VAVE's FM-1 firmware
```

## Building

The engine, the desktop tools and the tests build with CMake and any C99 compiler:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The FM-1 test firmware needs M-VAVE's SDK and JieLi's Windows toolchain 2.5.3 (PI32v2 clang 4.0.1). [firmware/README.md](firmware/README.md) has the steps.

## Hearing it

`verbalize` plays a text score through the whole instrument and writes a mono WAV file. The demo score visits all three modes:

```
build/verbalize host/scores/demo.txt demo.wav
build/verbalize host/scores/demo.txt demo-fm1.wav 44100
```

The first is the engine's own 20 kHz output. The second goes through the resampler and 24-bit output stage the firmware will use, at the FM-1's 44.1 kHz.

With a multi-configuration generator such as Visual Studio the program is in `build/Debug` or `build/Release`. The score format is described at the top of [host/verbalize.c](host/verbalize.c), and the knobs in [docs/controls.md](docs/controls.md) and [docs/phonemes.md](docs/phonemes.md).

## Safety and recovery

- The FM-1 keeps its firmware in a single flash bank. An interrupted update can leave the device unable to boot.
- Back up your projects, voices and samples before installing any custom firmware.
- Keep the official firmware file and a recovery method ready before you flash. Community recovery tools exist, including flash access through a small RP2040 board wired to the USB lines.
- Newer FM-1 units use a different flash chip. Check that your installer supports your unit.

## Credits

- **Dan Ray** — The Verbalizer, original instrument and firmware.
- **Chip Gracey / Parallax, Inc.** — `VocalTract` and `StereoSpatializer` (MIT License).
- The FM-1 custom-firmware community, whose reverse engineering and open hardware layers make this possible.

## License

MIT. See [LICENSE](LICENSE). The vocal tract is derived from Chip Gracey's `VocalTract`, which Parallax also released under the MIT License; its notice is kept in [engine/vocal_tract.c](engine/vocal_tract.c).

The FM-1 firmware is built on M-VAVE's official SDK, whose application code is GPL-3.0, so a built firmware and its source are distributed under GPL-3.0. In this repository that applies to `firmware/fm1_open.patch`, which modifies M-VAVE's files. The engine, tools and the rest of `firmware/` stay MIT.

## Related

- [antfriend/Verbalizer](https://github.com/antfriend/Verbalizer) — the original Propeller instrument
- [m-vave/fm1_open](https://github.com/m-vave/fm1_open) — M-VAVE's open FM-1 firmware and SDK
- [hugelton/Felucca](https://github.com/hugelton/Felucca) — open multi-engine FM-1 firmware
- [aroum/fm1-custom-fw](https://github.com/aroum/fm1-custom-fw) — FM-1 custom firmware resource hub
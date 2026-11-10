# MVAVE FM1 Verbalizer

A vocal-tract synthesizer firmware for the M-VAVE FM-1 pocket synth: a port of [The Verbalizer](https://github.com/antfriend/Verbalizer), a one-of-a-kind formant instrument originally built on the Parallax Propeller.

Play phonemes, formants and word fragments from the FM-1's keys instead of FM patches.

> **Status: the sound engine runs on a desktop; nothing is flashable yet.** The vocal tract and the voice logic are ported to portable C and render WAV files on a PC. The FM-1 firmware itself has not been started. Sections marked *planned* are not implemented.

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
- The FM-1 has an active open custom-firmware scene with existing hardware layers, installers and a route back to the official firmware.

## Planned design

### Modes

The original's three-position mode switch becomes three modes:

| Mode | What it does |
|---|---|
| **Words** | Each key triggers a word; a knob sets its duration. |
| **Trigger / Sustain / Release** | The main mode. Key down, hold and key up each play their own phoneme. |
| **Formants** | Direct control of all 13 tract parameters for building new sounds. |

### Controls

The original has about sixteen dedicated knobs. The FM-1 has four plus MASTER, so parameters are paged.

| Page | Knob 1 | Knob 2 | Knob 3 | Knob 4 |
|---|---|---|---|---|
| Trigger | phoneme | duration | — | — |
| Sustain | phoneme | duration | vibrato depth | vibrato rate |
| Release | phoneme | duration | — | — |
| Formants A | `f1` | `f2` | `f3` | `f4` |
| Formants B | `aa` | `ga` | `na` | `nf` |
| Formants C | `fa` | `ff` | pitch offset | glide time |

This layout is a first draft and will change once it is playable.

### New on the FM-1 (*planned*)

- Save Formants-mode creations as user phonemes, usable in the Trigger / Sustain / Release mode.
- MIDI CC control of all 13 tract parameters.
- An on-screen vocal-tract diagram showing the live parameter values.
- Optional output saturation in place of the original's overflow crackle, with the crackle kept as a setting.

## Porting plan

1. **`VocalTract` in C.** *Done.* The Propeller assembly is translated to portable integer C. Its output matches the original assembly sample for sample, checked by running that assembly in an emulator. Comparing against recordings of the original hardware is still to do.
2. **Sample rate.** *Decided in part.* Everything in the tract depends on its 20 kHz rate, so the engine keeps running at 20 kHz and stays bit-exact. Resampling its output to the FM-1's rate is not written yet.
3. **Voice logic.** *Done.* `Voice.spin` and the key handling are ported: the phonemes, the three modes and four-voice allocation. The desktop build plays it from a text score of MIDI note numbers; it does not read MIDI files or ports.
4. **FM-1 integration.** *Not started.* Wrap the engine for the FM-1 on top of an existing open hardware layer, and build the paged UI.
5. **Hardware testing.** *Not started.* CPU load with four voices, stereo output for the spatializer, and battery behaviour.

The stereo spatializer, which also gave the original its echo, is not ported. The engine mixes its voices to mono for now.

[docs/porting-notes.md](docs/porting-notes.md) has the details, including where the C code differs from the original on purpose.

## Repository layout

```
engine/     portable C vocal tract and voice logic, no hardware dependencies
host/       desktop tools: render scores and tract scripts to WAV files
tests/      engine tests and the reference hashes from the original assembly
tools/      table generator and the reference emulator for VocalTract.spin
docs/       control map, phoneme table, porting notes
firmware/   (planned) FM-1 integration: UI pages, parameter storage, MIDI mapping
```

## Building

The engine, the desktop tools and the tests build with CMake and any C99 compiler:

```
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

The FM-1 firmware build does not exist yet. It will need JieLi's PI32v2 toolchain (clang 4.0.1).

## Hearing it

`verbalize` plays a text score through the whole instrument and writes a 20 kHz mono WAV file. The demo score visits all three modes:

```
build/verbalize host/scores/demo.txt demo.wav
```

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

To be decided. `VocalTract` and `StereoSpatializer` remain under their MIT terms. If the firmware is built on a GPL-3.0 codebase, the combined firmware will be GPL-3.0.

## Related

- [antfriend/Verbalizer](https://github.com/antfriend/Verbalizer) — the original Propeller instrument
- [m-vave/fm1_open](https://github.com/m-vave/fm1_open) — M-VAVE's open FM-1 firmware and SDK
- [hugelton/Felucca](https://github.com/hugelton/Felucca) — open multi-engine FM-1 firmware
- [aroum/fm1-custom-fw](https://github.com/aroum/fm1-custom-fw) — FM-1 custom firmware resource hub
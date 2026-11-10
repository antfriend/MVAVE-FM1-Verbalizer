# MVAVE FM1 Verbalizer

A vocal-tract synthesizer firmware for the M-VAVE FM-1 pocket synth: a port of [The Verbalizer](https://github.com/antfriend/Verbalizer), a one-of-a-kind formant instrument originally built on the Parallax Propeller.

Play phonemes, formants and word fragments from the FM-1's keys instead of FM patches.

> **Status: planning / early development.** Nothing here is flashable yet. This README describes the design and the porting plan; sections marked *planned* are not implemented.

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

1. **`VocalTract` in C.** Translate the Propeller assembly to portable fixed-point C and build it on a desktop. Compare its output against recordings of the original hardware.
2. **Sample rate.** The original runs at 20 kHz and its formant units (19.53 Hz steps) depend on that. Either rescale the coefficients for the FM-1's 44.1 kHz path, or run the tract at 22.05 kHz and upsample 2×.
3. **Voice logic.** Port `Voice.spin`: the phoneme tables and the trigger, sustain and release sequencing. Drive it from MIDI on the desktop build.
4. **FM-1 integration.** Wrap the engine for the FM-1 on top of an existing open hardware layer, and build the paged UI.
5. **Hardware testing.** CPU load with four voices, stereo output for the spatializer, and battery behaviour.

## Repository layout (*planned*)

```
engine/     portable C vocal tract and voice logic, no hardware dependencies
host/       desktop test harness: renders WAV files, compares against references
firmware/   FM-1 integration: UI pages, parameter storage, MIDI mapping
docs/       control maps, phoneme tables, porting notes
```

## Building

Not available yet. The FM-1 firmware build will need JieLi's PI32v2 toolchain (clang 4.0.1). The `engine/` and `host/` parts will build with any standard C compiler.

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
# FM-1 firmware

This folder holds what turns M-VAVE's open FM-1 firmware into one with the Verbalizer in it. It is a first test build: it compiles, links, passes the SDK's checks and packages into an update file. **It has never been run on a device.**

## What this build does

- The Verbalizer runs **beside** the stock instrument and its output is added to the stock sound. The stock firmware's screens, sequencer and tracks are untouched, so the device should behave as a normal FM-1 with a second voice on top.
- Panel keys and incoming MIDI (DIN, USB, Bluetooth) play the Verbalizer as well as the stock tracks.
- **The SCL button opens the Verbalizer's page** (and closes it again; HOME also leaves). On it the SELECT encoder turns through seven pages and the other six encoders edit what the page shows. The note keys keep playing. See "The Verbalizer page" below.
- It starts in Trigger / Sustain / Release mode with trigger "boot", sustain "a", release "oh". Nothing is stored: every start-up begins with the same values. MIDI controllers 102–119 work too (see [docs/midi.md](../docs/midi.md)), and the page shows what they set.
- SCL no longer jumps to the DX7 scaling pages; they are still reached through EDIT.
- **The stock DX7 factory library is cut from 128 voices to 6**, and the DX7 algorithm diagram is gone from the FM edit page. Program numbers above 6 repeat the first 6. This was needed to fit; see below.

## The Verbalizer page

| Page | Mode | ALGORITHM | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | PRESETS |
|---|---|---|---|---|---|---|---|
| 1 Words | Words | | length | | | | |
| 2 Trigger | Trigger / Sustain / Release | phoneme | time | volume | vibrato depth | vibrato rate | |
| 3 Sustain | Trigger / Sustain / Release | phoneme | time | volume | vibrato depth | vibrato rate | |
| 4 Release | Trigger / Sustain / Release | phoneme | time | volume | vibrato depth | vibrato rate | |
| 5 Formant 1 | Formants | voice (`ga`) | `f1` | `f2` | `f3` | `f4` | glide time |
| 6 Formant 2 | Formants | breath (`aa`) | nasal (`na`) | nasal frequency (`nf`) | frication (`fa`) | frication frequency (`ff`) | pitch offset |
| 7 Formant 3 | Formants | | vibrato depth (`vp`) | vibrato rate (`vr`) | | | |

The page you are on sets the engine's mode. Each mode has its own set of the sixteen knobs, so visiting the Formants pages does not disturb the Trigger / Sustain / Release sounds; Formants starts as the vowel "a".

Phonemes are shown by name, times in milliseconds and formant frequencies in hertz. The phoneme control steps one phoneme at a time and passes over the fourteen identical noise phonemes as one.

[fm1_verbalizer_ui.c](fm1_verbalizer_ui.c) holds this table and the value formats; the drawing is in the patch to `app/screen.c`.

## Why the DX7 library had to shrink

The stock firmware fills its part of the flash to within 345 bytes. The Verbalizer adds about 9 KB, even after:

- building its two 4 KB lookup tables in RAM at start-up instead of storing them,
- compiling everything that does not run per sample for size.

The factory DX7 bank is the largest piece of data the open application owns (10.5 KB packed), so that is where the room came from: 16 voices were kept, to fit the engine.

The Verbalizer page then needed another 2 KB. That came from removing the DX7 algorithm diagram (0.9 KB), the old SCL shortcut, and ten more factory voices. The image now has 73 bytes to spare, so anything further (storing patches, for one) has to replace more of the stock screens first.

## Files

| File | What it is | Licence |
|---|---|---|
| `fm1_verbalizer.c`, `fm1_verbalizer.h` | Glue between the SDK and the engine: a MIDI queue filled by the firmware's tasks, and the render call for the audio interrupt | MIT |
| `fm1_verbalizer_ui.c`, `fm1_verbalizer_ui.h` | The Verbalizer page's contents: which control is on which encoder, how it steps and how its value reads. No drawing | MIT |
| `fm1_open.patch` | Changes to five of M-VAVE's source files: four in the firmware (audio interrupt, MIDI router, factory bank, screen), one in the browser simulator | GPL-3.0, as a derivative of them |
| `install.py` | Copies the engine and glue into an `fm1_open` checkout, applies the patch and trims the factory bank | MIT |

The firmware built from this is a modified version of M-VAVE's application and is under GPL-3.0 with their SDK linking exception. Anyone you give a build to must also be able to get this source.

## Building

You need:

- a checkout of [m-vave/fm1_open](https://github.com/m-vave/fm1_open) at commit `4c7496d` (V14), for example in `C:\git\fm1_open`;
- the JieLi toolchain 2.5.3 `pi32` folder, for example `C:\JL\pi32`. JieLi's installer wants the Code::Blocks editor; the folder can instead be unpacked from the installer with `innoextract`. The SDK build verifies the toolchain's files by hash.

Then:

```
python firmware\install.py C:\git\fm1_open
cd C:\git\fm1_open
powershell -ExecutionPolicy Bypass -File sdk\Build.ps1 -FirmwareOnly -Toolchain C:\JL\pi32
```

The update file is `build\dx7\generated\FM1_OFFICIAL_OPEN.fwsc`.

Run `install.py` again after changing the engine; it refreshes the copy in the checkout.

## Playing it in a browser

The SDK can also compile the application to WebAssembly and wrap it in a page with the device's panel, screen and keys. The patch puts the Verbalizer into that build the same way as into the firmware, so the same glue and the same MIDI routing run there.

```
cd C:\git\fm1_open
powershell -ExecutionPolicy Bypass -File tools\Build-Web.ps1 -LocalPreview
```

The first run downloads the WASI SDK (518 MB) into `build\tools`. Open `build\preview\index.html` in Chrome or Edge. The page's keys and a connected MIDI keyboard play the Verbalizer on top of the stock instrument. Click SCL for the Verbalizer's page, then use the page's dials. To hear the Verbalizer alone, send notes on a MIDI channel no track listens to, or turn the stock tracks down.

`node tests/web_smoke.mjs`, run from this repository, checks the compiled simulator without a browser.

In the browser the stock instrument is M-VAVE's open stand-in for their closed sound library, so it does not sound exactly like the device. The Verbalizer is the same code in both.

## Before flashing

Do not flash this until:

- you have a recovery route ready that does not depend on the firmware working (the main README lists the RP2040 method), and the stock firmware file to hand;
- your projects, voices and samples are backed up.

The build still identifies itself as the official open V14 (`app/version.h` is unchanged), so the updater cannot tell it from a stock build.

## What has been checked, and what has not

Checked on a PC:

- The SDK's own build checks pass, including its checks on which code sits in RAM.
- In the linked image, all of the engine's code is in the RAM code section, and its disassembly shows no calls or jumps into flash and no C library calls. This matters because the audio interrupt keeps running during flash writes, when flash cannot be reached.
- The engine code that is compiled here is the same source that passes the desktop tests, including the bit-exact comparison with the original.

- In the browser simulator, notes and controllers sent to the application's MIDI input reach the Verbalizer through the firmware's own router, it sounds, and it falls silent again; SCL opens the Verbalizer page, it draws, its encoders change the sound, and each mode keeps its own knobs (`tests/web_smoke.mjs`). This runs the glue and the patched router, but not the device's audio interrupt or its interrupt lock.

Not checked, because it needs a device:

- Whether it runs at all.
- Whether there is processor time for four vocal tracts on top of the stock instrument. If not, the audio will break up; the stock firmware counts such overruns.
- Whether masking interrupts in the MIDI queue is the right lock on this processor.
- Whether the first audio block, which builds the lookup tables, finishes in time. If it does not, one block is dropped at start-up.

# FM-1 firmware

This folder holds what turns M-VAVE's open FM-1 firmware into one with the Verbalizer in it. It is a first test build: it compiles, links, passes the SDK's checks and packages into an update file. **It has never been run on a device.**

## What this build does

- The Verbalizer runs **beside** the stock instrument and its output is added to the stock sound. The stock firmware's screens, sequencer and tracks are untouched, so the device should behave as a normal FM-1 with a second voice on top.
- Panel keys and incoming MIDI (DIN, USB, Bluetooth) play the Verbalizer as well as the stock tracks.
- The Verbalizer has no screen or knobs yet. It starts in Trigger / Sustain / Release mode with one fixed patch: trigger "boot", sustain "a", release "oh". MIDI controllers 102–119 do work (see [docs/midi.md](../docs/midi.md)).
- **The stock DX7 factory library is cut from 128 voices to 16.** Program numbers above 16 repeat the first 16. This was needed to fit; see below.

## Why the DX7 library had to shrink

The stock firmware fills its part of the flash to within 345 bytes. The Verbalizer adds about 9 KB, even after:

- building its two 4 KB lookup tables in RAM at start-up instead of storing them,
- compiling everything that does not run per sample for size.

The factory DX7 bank is the largest piece of data the open application owns (10.5 KB packed), so that is where the room came from. A finished Verbalizer firmware would replace the stock screens instead, which frees far more.

## Files

| File | What it is | Licence |
|---|---|---|
| `fm1_verbalizer.c`, `fm1_verbalizer.h` | Glue between the SDK and the engine: a MIDI queue filled by the firmware's tasks, and the render call for the audio interrupt | MIT |
| `fm1_open.patch` | Changes to three of M-VAVE's source files | GPL-3.0, as a derivative of them |
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

Not checked, because it needs a device:

- Whether it runs at all.
- Whether there is processor time for four vocal tracts on top of the stock instrument. If not, the audio will break up; the stock firmware counts such overruns.
- Whether masking interrupts in the MIDI queue is the right lock on this processor.
- Whether the first audio block, which builds the lookup tables, finishes in time. If it does not, one block is dropped at start-up.

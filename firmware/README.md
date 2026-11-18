# FM-1 firmware

This folder holds what turns M-VAVE's open FM-1 firmware into one with the Verbalizer in it. There are two builds:

- **Combined**: the Verbalizer plays beside the stock instrument. This is the smaller step away from the stock firmware and the one to flash first.
- **Verbalizer-only**: the Verbalizer is the whole instrument and the stock sound, sequencer and screens are gone. See "The Verbalizer-only build" below.

Both compile, link, pass the SDK's checks and package into an update file. **Neither has ever been run on a device.**

## What the combined build does

- The Verbalizer runs **beside** the stock instrument and its output is added to the stock sound. The stock firmware's screens, sequencer and tracks are untouched, so the device should behave as a normal FM-1 with a second voice on top.
- Panel keys and incoming MIDI (DIN, USB, Bluetooth) play the Verbalizer as well as the stock tracks.
- **The SCL button opens the Verbalizer's page** (and closes it again; HOME also leaves). On it the SELECT encoder turns through seven pages and the other six encoders edit what the page shows. The note keys keep playing. See "The Verbalizer page" below.
- **SAVE, while on the Verbalizer's page, keeps its knobs** so that the next start-up comes back with them. Until the first save it starts in Trigger / Sustain / Release mode with trigger "boot", sustain "a", release "oh". MIDI controllers 102–119 work too (see [docs/midi.md](../docs/midi.md)), and the page shows what they set.
- SCL no longer jumps to the DX7 scaling pages; they are still reached through EDIT.
- **The stock DX7 factory library is cut from 128 voices to 4.** Program numbers above 4 repeat the first 4. The DX7 algorithm diagram is gone from the FM edit page, and so is the start-up animation, which announced the official firmware. All of this was needed to fit; see below.
- **New sample banks can be 4 KB smaller than on the stock firmware**, because the Verbalizer's knobs are kept in the last sector of the sample area. See "Where the knobs are kept" below.

## The Verbalizer page

| Page | Mode | ALGORITHM | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 | PRESETS |
|---|---|---|---|---|---|---|---|
| 1 Words | Words | word (Verbalizer-only build) | length | | | | |
| 2 Trigger | Trigger / Sustain / Release | phoneme | time | volume | vibrato depth | vibrato rate | |
| 3 Sustain | Trigger / Sustain / Release | phoneme | time | volume | vibrato depth | vibrato rate | |
| 4 Release | Trigger / Sustain / Release | phoneme | time | volume | vibrato depth | vibrato rate | |
| 5 Formant 1 | Formants | voice (`ga`) | `f1` | `f2` | `f3` | `f4` | glide time |
| 6 Formant 2 | Formants | breath (`aa`) | nasal (`na`) | nasal frequency (`nf`) | frication (`fa`) | frication frequency (`ff`) | pitch offset |
| 7 Formant 3 | Formants | | vibrato depth (`vp`) | vibrato rate (`vr`) | | | |

The page you are on sets the engine's mode. Each mode has its own set of the sixteen knobs, so visiting the Formants pages does not disturb the Trigger / Sustain / Release sounds; Formants starts as the vowel "a".

Phonemes are shown by name, times in milliseconds and formant frequencies in hertz. The phoneme control steps one phoneme at a time and passes over the fourteen identical noise phonemes as one.

[fm1_verbalizer_ui.c](fm1_verbalizer_ui.c) holds this table and the value formats; the drawing is in the patch to `app/screen.c`.

## Where the knobs are kept

The stock firmware uses all of the user flash: projects, one sample bank, four DX7 banks and a tuning sector. The Verbalizer takes the 4 KB sector just before the DX7 banks, which was the end of the sample area, and the firmware now tells the web tool that the sample area is one sector shorter.

One save stores the mode and all three modes' knobs, 60 bytes with a check value. What to know:

- **It never overwrites anything else.** If the sector holds data that is neither blank nor an earlier save, which can only be a sample bank from before this firmware that reaches the very end of the sample area, SAVE answers "NO ROOM - SAMPLE BANK IS FULL" and writes nothing. That bank keeps working.
- **A save erases the sector and then writes it.** Power lost in between loses the saved knobs; the next start-up uses the built-in ones. A record that fails its check is ignored in the same way.
- **Audio stops for a moment during a save**, as for any flash write on this device.
- **It is one set of sounds for the device**, not one per project. Loading a stock project does not change it.
- Going back to stock firmware leaves the 60 bytes where they are. Stock firmware would only notice if a sample bank grew into that sector, and it would then overwrite them.

## Why the DX7 library had to shrink

The stock firmware fills its part of the flash to within 345 bytes. The Verbalizer adds about 9 KB, even after:

- building its two 4 KB lookup tables in RAM at start-up instead of storing them,
- compiling everything that does not run per sample for size.

The factory DX7 bank is the largest piece of data the open application owns (10.5 KB packed), so that is where the room came from: 16 voices were kept, to fit the engine.

The Verbalizer page then needed another 2 KB. That came from removing the DX7 algorithm diagram (0.9 KB), the old SCL shortcut, and ten more factory voices.

Saving the knobs needed 0.5 KB more: the start-up animation went, and two more factory voices. The image now has 169 bytes to spare. The small things are used up; the next addition has to replace a whole stock screen, and the DX7 editor is the largest.

## The Verbalizer-only build

`install.py --only` makes a firmware in which the Verbalizer is the only instrument.

What it does:

- **It starts on the Verbalizer's page**, which works as described above: SELECT turns the pages, the other six encoders edit, SAVE keeps the knobs.
- **The keys play the Verbalizer**, from F below middle C as on the stock firmware. OCT − and OCT + shift them by up to two octaves.
- **MIDI from DIN, USB and Bluetooth plays it, on every channel.**
- **MASTER is the volume.** USB audio and Bluetooth audio from a computer or phone are still mixed into the output.
- **GLO opens the Bluetooth page** (HOME or GLO again goes back). KNOB 1 turns Bluetooth on and off, KNOB 2 Bluetooth audio, KNOB 3 chooses whether the FM-1 is a Bluetooth MIDI device or host. PRESETS picks an action and SAVE confirms it, as on the stock page. These settings are stored a moment after they change.

- **The screen is in green CRT colours.** FX swaps between that and M-VAVE's colours, and the choice is stored with the Bluetooth settings.

- **The Toot Toot Engineering eye watches from the header.** A white eyeball with a red iris, the symbol of antfriend.github.io, sits right of the page title on every page. Its iris and pupil glance to a new spot every 1.5 to 3 seconds, and the screen repaints only that small square each time. It is not in the combined build, which has no flash to spare.

What is gone:

- Every stock sound, the sequencer and arpeggiator, the effects, projects, samples and the DX7 library, and all their screens.
- **MIDI out.** The keys are not sent to DIN, USB or Bluetooth, and there is no thru or clock. The stock router did that for its tracks.
- **The web tool's transfers.** Requests for projects, samples and DX7 banks are ignored. Firmware updates do not go through the application, so M-VAVE's updater should still work; that is the first thing to confirm on a device.

What to know:

- **The stock sound code is still in the image, but never runs.** It is inside M-VAVE's closed library, and the SDK's build refuses an image without it. The audio interrupt no longer calls it, so its processor time goes to the Verbalizer. Whether the library minds never being called is not known until it runs on a device.
- **It has seventeen words and fourteen more phonemes.** On the Words page ALGORITHM picks the word, and the phoneme controls of the Trigger, Sustain and Release pages go on past NOISE through M, N, NG, W, Y, H, F, S, SH, V, Z, B, D and G. [docs/controls.md](../docs/controls.md) and [docs/phonemes.md](../docs/phonemes.md) list them. The combined build has no room for these: there the Words page has its one word and those phoneme numbers are all noise, as on the original instrument.
- **The image is 391 KB of the 454 KB allowed, so 63 KB are free**, against 169 bytes in the combined build.
- **Stock data in flash is left alone.** Projects, the sample bank and the DX7 banks are neither read nor written, and are still there on going back to the stock firmware. The knobs are kept in the same sector as in the combined build, so a saved sound survives a change between the two. The Bluetooth settings take the sector before it. If a stock sample bank already reaches into either sector, that save is refused and the bank is left as it is.
- It needs **a checkout of its own**; `install.py` refuses to mix the two builds in one.

## Files

| File | What it is | Licence |
|---|---|---|
| `fm1_verbalizer.c`, `fm1_verbalizer.h` | Glue between the SDK and the engine: a queue filled by the firmware's tasks, the render call for the audio interrupt, a set of knobs for each mode, and saving them to flash | MIT |
| `fm1_verbalizer_ui.c`, `fm1_verbalizer_ui.h` | The Verbalizer page's contents: which control is on which encoder, how it steps and how its value reads. No drawing | MIT |
| `fm1_open.patch` | Combined build: changes to six of M-VAVE's source files, five in the firmware (audio interrupt, MIDI router, factory bank, screen, flash layout) and one in the browser simulator | GPL-3.0, as a derivative of them |
| `fm1_open_only.patch` | Verbalizer-only build: changes to three of M-VAVE's files (audio interrupt, flash layout, browser simulator) | GPL-3.0, as a derivative of them |
| `only/app/screen.c` | Verbalizer-only build: the screen and panel handling, cut down from M-VAVE's `app/screen.c` | GPL-3.0, as a derivative of it |
| `only/app/project.c`, `midi_router.c`, `transfer.c` | Verbalizer-only build: the Bluetooth settings, MIDI in, and the empty entry points M-VAVE's library still calls | MIT |
| `only/web/engine/only_stubs.c` | Verbalizer-only build, browser only: answers for the simulator page's stock tabs | MIT |
| `install.py` | Puts either build into an `fm1_open` checkout. For the combined build it also writes `vb_config.h`, which leaves the added words and phonemes out | MIT |

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

The Verbalizer-only build is made the same way in a second checkout of the same commit. From the first one, `git worktree add -b verbalizer-only C:\git\fm1_open_only 4c7496d` makes it. Then:

```
python firmware\install.py --only C:\git\fm1_open_only
cd C:\git\fm1_open_only
powershell -ExecutionPolicy Bypass -File sdk\Build.ps1 -FirmwareOnly -Toolchain C:\JL\pi32
```

## Playing it in a browser

The SDK can also compile the application to WebAssembly and wrap it in a page with the device's panel, screen and keys. The patch puts the Verbalizer into that build the same way as into the firmware, so the same glue and the same MIDI routing run there.

```
cd C:\git\fm1_open
powershell -ExecutionPolicy Bypass -File tools\Build-Web.ps1 -LocalPreview
```

The first run downloads the WASI SDK (518 MB) into `build\tools`. Open `build\preview\index.html` in Chrome or Edge. The page's keys and a connected MIDI keyboard play the Verbalizer on top of the stock instrument. Click SCL for the Verbalizer's page, then use the page's dials. SAVE works there too: the page keeps its copy of the flash in the browser, so a saved sound is still there after a reload. To hear the Verbalizer alone, send notes on a MIDI channel no track listens to, or turn the stock tracks down.

`node tests/web_smoke.mjs`, run from this repository, checks the compiled simulator without a browser.

In the browser the stock instrument is M-VAVE's open stand-in for their closed sound library, so it does not sound exactly like the device. The Verbalizer is the same code in both.

The Verbalizer-only build runs in the browser in the same way, from its own checkout, and `node tests/web_only_smoke.mjs` checks it. The second checkout can share the WASI SDK: make `build\tools` in it and link the first one's `wasi-sdk-27.0-x86_64-windows` folder there with `mklink /J`. The page around the device is M-VAVE's and cannot be changed without failing the SDK's build checks, so its tabs for projects, samples, DX7 voices and MIDI routing are still shown and do nothing.

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

- In the browser simulator, notes and controllers sent to the application's MIDI input reach the Verbalizer through the firmware's own router, it sounds, and it falls silent again; SCL opens the Verbalizer page, it draws, its encoders change the sound, and each mode keeps its own knobs; SAVE writes a record, a restart on that flash brings the knobs back, a damaged record is ignored and foreign data in the sector is left alone (`tests/web_smoke.mjs`). This runs the glue and the patched router, but not the device's audio interrupt or its interrupt lock.

- For the Verbalizer-only build, in the browser simulator: it starts on the Verbalizer page; keys and MIDI on any channel play it; with its volume knobs at zero a key is silent, so nothing else sounds underneath; the octave buttons, the pages and SAVE work; GLO opens the Bluetooth page, a changed setting is stored and comes back after a restart; stock data in either sector is left alone (`tests/web_only_smoke.mjs`).

Not checked, because it needs a device:

- For the Verbalizer-only build: whether M-VAVE's library runs correctly when its instrument is never rendered, whether Bluetooth, USB and the updater behave as on the stock firmware, and whether the stripped application starts at all. The simulator does not contain the closed library, so none of this can be seen on a PC.

- Whether saving works on real flash, and how long the audio stops for it. In the simulator flash is an array.

- Whether it runs at all.
- Whether there is processor time for four vocal tracts on top of the stock instrument. If not, the audio will break up; the stock firmware counts such overruns.
- Whether masking interrupts in the MIDI queue is the right lock on this processor.
- Whether the first audio block, which builds the lookup tables, finishes in time. If it does not, one block is dropped at start-up.

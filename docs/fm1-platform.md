# The FM-1 as a target

What the firmware stage has to fit into. Everything here is read from M-VAVE's official open firmware and SDK, [m-vave/fm1_open](https://github.com/m-vave/fm1_open) (V14, commit `4c7496d`, October 2026). Nothing has been tried on a device yet.

## Decision: build on the official SDK

The firmware will be built on `fm1_open`. Its application code is open: the audio interrupt, key and encoder scanning, screen, MIDI routing, flash storage and the update protocol. The stock instrument itself is one call into a closed library, `fm1_engine_render`. The Verbalizer firmware replaces that call with this repository's engine and keeps the rest.

Consequences:

- **Licence.** The SDK's application code is GPL-3.0 with a linking exception for M-VAVE's binary library. A firmware built from it is distributed under GPL-3.0, with its source. The engine in this repository stays MIT; MIT code can be combined into a GPL work.
- **Toolchain.** JieLi's Windows toolchain 2.5.3 (PI32v2 clang 4.0.1). The SDK build checks the toolchain's files against a manifest.
- **Way back.** The SDK ships the stock V15 image and the same update protocol, so a device can return to stock.

## Audio

| | |
|---|---|
| Sample rate | 44,100 Hz, set by the codec clock (11.2896 MHz, 256 fs). The board header gives the real figure as 44,118 Hz. No other rate is offered. |
| Block | 128 stereo frames per interrupt, about 2.9 ms |
| Format | signed 24-bit (Q23) in 32-bit words, interleaved left and right |
| Where sound is made | inside the audio interrupt, once per block, with a deadline of one block |
| After the instrument | the application applies the master level, mixes in USB and Bluetooth audio, clips to 24 bits and feeds USB recording |

[engine/resampler.h](../engine/resampler.h) produces exactly this: `vb_output_render()` fills a block of interleaved stereo Q23 at 44.1 kHz from the 20 kHz engine. Each 128-frame block draws 58 or 59 engine samples.

The 0.04% difference between 44,100 and 44,118 Hz raises pitch by less than a cent and is ignored.

## Rules for audio code

- **It must run from RAM.** Audio keeps running while flash is being written, so every function the audio interrupt calls has to be placed in the SDK's RAM code section (`FM1_AUDIO_CODE`) and every table it reads in the RAM data section (`FM1_AUDIO_DATA`). For this engine that is `vt_tick`, the voice and instrument code reached from `vb_render`, the resampler, and three tables: antilog (4 KB), sine (4 KB) and the resampler filter (2 KB).
- **No blocking, allocation or flash access** in the interrupt. The engine does none.
- **Key and MIDI events arrive in another context.** The engine's event calls (`vb_midi`, `vb_key_down`, knob writes) are not safe against a concurrent `vb_render`. The firmware must queue events and apply them at the start of a block, or bracket them with the SDK's short interrupt lock.

## Controls

From the SDK's `fm1_controls`:

| Control | Count | Notes |
|---|---|---|
| Note keys | 27 | F3 to G5, MIDI 53–79 |
| Panel buttons | 14 | |
| Encoders | 7 | endless, reported as signed detents since the last read |
| Analogue knob | 1 | 10-bit reading |
| Key LEDs | 41 | one per button and note key |
| Screen | 240 × 240 | RGB565 |

The note keys sit inside the engine's range (MIDI 20–83). The encoders are relative, so the firmware keeps each knob value itself and can show it on screen; there is no pick-up problem when changing pages.

MIDI comes in over DIN, USB and Bluetooth as complete messages, which is what `vb_midi()` takes.

## Storage

The SDK exposes the user flash partition as raw 4 KB sectors with read, write and erase. Writes briefly silence audio. It is not transactional, so patches need versioned, checksummed records.

## What has been tried

On a PC, with no device involved:

- **The stock firmware builds.** The unmodified SDK builds its V14 firmware in a few seconds with the JieLi 2.5.3 toolchain. All 22 toolchain files match the hashes in the SDK's manifest.
- **A firmware with the Verbalizer in it builds.** [firmware/](../firmware/README.md) adds the engine to the stock application, where it plays beside the stock instrument. It links, passes the SDK's checks and packages into an update file. It has not been run.
- **The engine's code ends up where it must.** In the linked image all of it is in the RAM code section, and its disassembly shows no calls or jumps into flash.

JieLi's installer refuses to run without the Code::Blocks editor installed. The toolchain files can instead be unpacked from the installer with `innoextract`; only the `pi32` folder is needed.

## The program image is full

This turned out to be the tightest constraint. The application's share of the flash is 453,921 bytes and the stock firmware uses 453,576 of them: 345 bytes are free. The size of the user partition (projects and samples) is fixed by the SDK's packager, which is hash-checked, so the image cannot grow.

The engine first added 18.9 KB. What brought it down:

| Measure | Saved |
|---|---|
| Build the antilog and sine tables in RAM at start-up ([engine/vt_tables_gen.c](../engine/vt_tables_gen.c)) instead of storing them. An integer routine reproduces both tables exactly; the tests compare them. | 7.6 KB |
| Compile everything that does not run per sample for size (`VB_COLD_CODE`). | 2.2 KB |
| Cut the stock DX7 factory bank from 128 voices to 16. | 9.1 KB |

The last one takes something away from the stock instrument, and was only acceptable because this is a test build. Whatever comes next (screens, patch storage) needs room too, and the only large things the open application owns are its own screens (`app/screen.c`) and demo content. A finished Verbalizer firmware will have to replace those.

RAM is not tight: the engine needs about 4 KB of working memory, 8 KB for the two tables and 9 KB of RAM-resident code and filter table, against 276 KB the stock firmware already uses.

## Rules the engine now follows for this target

- Every function carries `VB_CODE` or `VB_COLD_CODE` and every constant table `VB_DATA` ([engine/vb_platform.h](../engine/vb_platform.h)), which on this compiler place them in the SDK's RAM sections.
- No C library calls. `memcpy`, `memmove` and `memset` live in flash on this firmware; the engine uses its own small loops.
- No 64-bit division, which would be a library call.

## Not yet known

- **Processor time.** How long four vocal tracts plus the resampler take per block on the AC791N, and so whether four voices fit. The stock firmware's own budget is not documented. This needs a device.
- **What the analogue knob and each button do** on the panel, beyond their numbers. The stock application's source will say.
- **Whether the engine can replace `fm1_engine_render` outright.** The SDK's build script checks that the stock engine's functions are present, so the test build leaves it running and adds the Verbalizer to its output. The stock engine itself is in the closed library and cannot be left out.
- **Whether masking interrupts is a sufficient lock** for the MIDI queue between the firmware's tasks and the audio interrupt.

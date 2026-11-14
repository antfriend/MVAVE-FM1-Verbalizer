// Checks the Verbalizer inside the fm1_open browser simulator: that MIDI
// reaches it through the firmware's own router, that it makes sound, that its
// controllers work and that it falls silent again.
//
//     node tests/web_smoke.mjs [path to simulator.wasm]
//
// The default path is ../fm1_open/build/web/simulator.wasm, which
// `build.bat --web` (or tools\Build-Web.ps1 -LocalPreview) writes in an
// fm1_open checkout after firmware/install.py has been run on it.
//
// Notes are sent on MIDI channel 16. No stock track listens there, so the
// stock instrument stays silent and everything heard is the Verbalizer.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const path = process.argv[2] ?? join(here, '..', '..', 'fm1_open', 'build', 'web', 'simulator.wasm');
const c = new WebAssembly.Instance(new WebAssembly.Module(readFileSync(path)), {}).exports;
c._initialize();
c.sim_boot(1);

const frames = c.sim_block_frames();
const CHANNEL = 15;
let failures = 0;

function midi(...bytes) {
  new Uint8Array(c.memory.buffer, c.sim_file(), bytes.length).set(bytes);
  return c.sim_midi(bytes.length);
}

// Renders for `seconds` and returns the loudest sample, full scale = 1.
function peak(seconds) {
  let loudest = 0;
  for (let block = 0; block < Math.ceil(seconds * 44100 / frames); block++) {
    const at = c.sim_render();
    for (const sample of new Float32Array(c.memory.buffer, at, frames * 2))
      loudest = Math.max(loudest, Math.abs(sample));
  }
  return loudest;
}

function check(name, ok, detail) {
  console.log(`${ok ? 'ok  ' : 'FAIL'} ${name} (${detail})`);
  if (!ok) failures++;
}

const SILENT = 1e-4, AUDIBLE = 0.01;
let level;

level = peak(0.5);
check('silent after start-up', level < SILENT, level.toExponential(2));

midi(0x90 | CHANNEL, 60, 127);
level = peak(0.6);
check('a note on channel 16 sounds', level > AUDIBLE, level.toFixed(4));
check('and does not clip', level < 0.999, level.toFixed(4));

midi(0x80 | CHANNEL, 60, 0);
peak(1.0);
level = peak(0.2);
check('silent after the note is released', level < SILENT, level.toExponential(2));

// Turn all three volume knobs down (CC 104, 109, 115 are knobs 2, 7 and 13).
// If the note is now silent, the sound above was the Verbalizer's and its
// controllers arrive.
for (const controller of [104, 109, 115]) midi(0xb0 | CHANNEL, controller, 0);
midi(0x90 | CHANNEL, 60, 127);
level = peak(0.6);
check('silent with the volume knobs at zero', level < SILENT, level.toExponential(2));
midi(0x80 | CHANNEL, 60, 0);
peak(0.5);

// Volume back up, Words mode (CC 118 = 0): the word ends on its own.
for (const controller of [104, 109, 115]) midi(0xb0 | CHANNEL, controller, 90);
midi(0xb0 | CHANNEL, 118, 0);
midi(0x90 | CHANNEL, 57, 127);
level = peak(0.3);
check('Words mode speaks', level > AUDIBLE, level.toFixed(4));
peak(3.0);
level = peak(0.2);
check('and the word ends while the key is still held', level < SILENT, level.toExponential(2));
midi(0x80 | CHANNEL, 57, 0);

// All sound off (CC 120) cuts a held note.
midi(0xb0 | CHANNEL, 118, 64);
midi(0x90 | CHANNEL, 60, 127);
peak(0.3);
midi(0xb0 | CHANNEL, 120, 0);
peak(0.3);
level = peak(0.2);
check('all sound off silences a held note', level < SILENT, level.toExponential(2));

// A panel key (kind 1) plays the stock instrument and the Verbalizer together.
c.sim_control(1, 7, 1);
level = peak(0.5);
check('a panel key sounds', level > AUDIBLE, level.toFixed(4));
c.sim_control(1, 7, 0);

// Far more messages than the queue holds, between two blocks: the extra ones
// are dropped and nothing breaks.
for (let i = 0; i < 500; i++) midi(0xb0 | CHANNEL, 102 + (i % 16), i & 127);
level = peak(0.5);
check('survives a flood of controllers', Number.isFinite(level), level.toFixed(4));

console.log(failures ? `${failures} check(s) failed` : 'all checks passed');
process.exit(failures ? 1 : 0);

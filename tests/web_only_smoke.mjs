// Checks the Verbalizer-only firmware inside the fm1_open browser simulator:
// that it starts on the Verbalizer's page, that keys and MIDI play it and
// nothing else, that its knobs and the Bluetooth settings are kept, and that
// the page's stock shortcuts do no harm.
//
//     node tests/web_only_smoke.mjs [path to simulator.wasm]
//
// The default path is ../fm1_open_only/build/web/simulator.wasm, which
// tools\Build-Web.ps1 -LocalPreview writes in an fm1_open checkout after
// `firmware/install.py --only` has been run on it.
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const here = dirname(fileURLToPath(import.meta.url));
const path = process.argv[2] ?? join(here, '..', '..', 'fm1_open_only', 'build', 'web', 'simulator.wasm');
const module = new WebAssembly.Module(readFileSync(path));
const FLASH_BYTES = 536576;
let c;

// Starts a fresh device. Without `flash` its storage is as from the factory;
// with it, the device starts up on that storage, as after a power cycle.
function start(flash) {
  c = new WebAssembly.Instance(module, {}).exports;
  c._initialize();
  if (flash) new Uint8Array(c.memory.buffer, c.sim_flash(), FLASH_BYTES).set(flash);
  c.sim_boot(flash ? 0 : 1);
}
const flashCopy = () => new Uint8Array(c.memory.buffer, c.sim_flash(), FLASH_BYTES).slice();
start();

const frames = c.sim_block_frames();
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
const OCT_UP = 1, GLO = 7, HOME = 8, SAVE = 9;           // buttons
const SELECT = 0, ALGORITHM = 1, KNOB1 = 2, KNOB2 = 3;   // encoders
const VERBALIZER_PAGE = 19, BLUETOOTH_PAGE = 7;
const state = () => new Uint8Array(c.memory.buffer, c.sim_state(), 225);
const shownPage = () => state()[1];
const led = (button) => state()[168 + button];
const press = (button) => { c.sim_control(0, button, 1); c.sim_control(0, button, 0); };
const turn = (encoder, detents) => {
  for (let i = 0; i < Math.abs(detents); i++) {
    c.sim_control(2, encoder, Math.sign(detents));
    c.sim_render();
  }
};
// How many pixels of the 240 x 240 screen are not background.
const lit = () => new Uint16Array(c.memory.buffer, c.sim_pixels(), 57600).reduce((n, p) => n + (p !== 0), 0);
const KNOBS = FLASH_BYTES - 6 * 4096, SETTINGS = FLASH_BYTES - 7 * 4096;
const word = (flash, at) => new DataView(flash.buffer).getUint32(at, true);
let level;

level = peak(0.5);
check('silent after start-up', level < SILENT, level.toExponential(2));
check('it starts on the Verbalizer page', shownPage() === VERBALIZER_PAGE, `page ${shownPage()}`);
check('which draws something', lit() > 2000, `${lit()} pixels`);

// MIDI on any channel plays it. Channel 1 was the stock DX7's.
midi(0x90, 60, 127);
level = peak(0.6);
check('a MIDI note on channel 1 sounds', level > AUDIBLE, level.toFixed(4));
check('and does not clip', level < 0.999, level.toFixed(4));
midi(0x80, 60, 0);
peak(1.0);
level = peak(0.2);
check('silent after the note is released', level < SILENT, level.toExponential(2));

// A panel key (kind 1) plays it too, and lights its key.
c.sim_control(1, 7, 1);
level = peak(0.5);
check('a panel key sounds', level > AUDIBLE, level.toFixed(4));
check('and its light is on', state()[168 + 14 + 7] === 1, 'key light');
c.sim_control(1, 7, 0);
peak(1.0);
level = peak(0.2);
check('silent after the key is released', level < SILENT, level.toExponential(2));

// With the Verbalizer's three volume knobs at zero (CC 104, 109, 115) a key
// is silent: nothing else is sounding underneath.
for (const controller of [104, 109, 115]) midi(0xb0, controller, 0);
c.sim_control(1, 7, 1);
level = peak(0.6);
check('no stock instrument plays underneath', level < SILENT, level.toExponential(2));
c.sim_control(1, 7, 0);
for (const controller of [104, 109, 115]) midi(0xb0, controller, 90);
peak(0.5);

press(OCT_UP);
peak(0.1);
check('OCT + lights up', led(OCT_UP) === 1, 'button light');

// The page's shortcuts into the stock editors have nothing to open.
check('a stock editor shortcut is refused', c.sim_open(2) === -1, 'sim_open(2)');
check('and the page stays', shownPage() === VERBALIZER_PAGE, `page ${shownPage()}`);

// From Trigger, three pages on is Formant 1: Formants mode, a voiced vowel.
turn(SELECT, 3);
midi(0x90, 60, 127);
level = peak(0.5);
check('Formant pages play the Formants patch', level > AUDIBLE, level.toFixed(4));
turn(ALGORITHM, -40);  // VOICE down to nothing
peak(0.3);
level = peak(0.2);
check('turning VOICE down silences it', level < SILENT, level.toExponential(2));
midi(0x80, 60, 0);

// SAVE keeps the knobs.
check('the knob sector starts blank', word(flashCopy(), KNOBS) === 0xffffffff, word(flashCopy(), KNOBS).toString(16));
press(SAVE);
peak(0.2);
check('SAVE writes the knobs', word(flashCopy(), KNOBS) === 0x31424256, word(flashCopy(), KNOBS).toString(16));

// GLO opens the Bluetooth page; KNOB 1 anticlockwise turns Bluetooth off, and
// the setting is written a moment later without a SAVE.
press(GLO);
peak(0.1);
check('GLO opens the Bluetooth page', shownPage() === BLUETOOTH_PAGE, `page ${shownPage()}`);
check('the settings sector starts blank', word(flashCopy(), SETTINGS) === 0xffffffff, word(flashCopy(), SETTINGS).toString(16));
turn(KNOB1, -1);
peak(2.0);
let saved = flashCopy();
check('a changed Bluetooth setting is written', word(saved, SETTINGS) === 0x31534256, word(saved, SETTINGS).toString(16));
check('as Bluetooth off', saved[SETTINGS + 4] === 1, `flags ${saved[SETTINGS + 4]}`);
press(HOME);
peak(0.1);
check('HOME goes back to the Verbalizer page', shownPage() === VERBALIZER_PAGE, `page ${shownPage()}`);

// A power cycle: Formants mode with VOICE at nothing, so a note is silent.
start(saved);
peak(0.3);
midi(0x90, 60, 127);
level = peak(0.5);
check('after a restart the saved knobs are back', level < SILENT, level.toExponential(2));
midi(0x80, 60, 0);
// Bluetooth is still off: turning Bluetooth audio off as well must store both.
press(GLO);
turn(KNOB2, -1);
peak(2.0);
saved = flashCopy();
check('and so is the Bluetooth setting', saved[SETTINGS + 4] === 3, `flags ${saved[SETTINGS + 4]}`);

// Stock data in either sector (a sample bank reaching that far) is never
// overwritten.
const foreign = saved.slice();
foreign.fill(0x5a, SETTINGS, SETTINGS + 2 * 4096);
start(foreign);
peak(0.3);
press(SAVE);
press(GLO);
turn(KNOB1, 1);
peak(2.0);
check("someone else's data in the two sectors is left alone",
  flashCopy().subarray(SETTINGS, SETTINGS + 2 * 4096).every((byte) => byte === 0x5a), 'sectors compared');

console.log(failures ? `${failures} check(s) failed` : 'all checks passed');
process.exit(failures ? 1 : 0);

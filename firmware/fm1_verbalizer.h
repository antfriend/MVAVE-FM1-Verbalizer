/* SPDX-License-Identifier: MIT */
/*
 * The Verbalizer inside M-VAVE's open FM-1 firmware.
 *
 * Two entry points, one for each side of the firmware:
 *
 *  - fm1_verbalizer_midi() is called from the firmware's tasks with each
 *    complete MIDI message (panel keys and DIN/USB/Bluetooth input). It only
 *    queues the message.
 *  - fm1_verbalizer_render() is called from the audio interrupt once per
 *    block. It applies the queued messages and renders the block.
 *
 * The engine itself is only ever touched from the audio interrupt.
 */
#ifndef FM1_VERBALIZER_H
#define FM1_VERBALIZER_H

#include "fm1.h"

/* Any task. Returns 0 if queued, -1 if the message is not a complete short
 * channel message or the queue is full (the message is then dropped). */
int fm1_verbalizer_midi(const uint8_t *message, size_t bytes);

/* Audio interrupt only. Writes `frames` interleaved stereo Q23 frames
 * (frames must be FM1_BLOCK_FRAMES or fewer). */
FM1_AUDIO_CODE void fm1_verbalizer_render(int32_t *output, unsigned frames);

/* Messages dropped because the queue was full. Any context. */
uint32_t fm1_verbalizer_dropped(void);

/*
 * The knobs, for a screen. Any task.
 *
 * The engine's sixteen knobs mean different things in each of its three modes
 * (VB_MODE_*), so the firmware keeps a set of sixteen per mode and gives the
 * engine the current mode's set. Changing mode swaps the set. Controllers
 * 102-117 and 118 arriving over MIDI go through the same calls, so a screen
 * that reads these always shows what the engine is playing.
 *
 * The set calls return 0, or -1 if the queue to the audio interrupt is full,
 * in which case nothing has changed.
 */
unsigned fm1_verbalizer_mode(void);
int fm1_verbalizer_set_mode(unsigned mode);
unsigned fm1_verbalizer_knob(unsigned knob);                 /* 0..255 */
int fm1_verbalizer_set_knob(unsigned knob, unsigned value);  /* 0..255 */

#endif

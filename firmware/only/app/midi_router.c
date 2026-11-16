/* SPDX-License-Identifier: MIT */
/*
 * Verbalizer-only firmware: MIDI.
 *
 * Everything that arrives on DIN, USB or Bluetooth goes to the Verbalizer.
 * Nothing is sent: the stock firmware's MIDI out, thru and clock belonged to
 * its sequencer and tracks. M-VAVE's library still calls the stock router's
 * entry points, so they exist here and do nothing.
 */
#include "instrument.h"
#include "verbalizer/fm1_verbalizer.h"

void fm1_app_midi_received(unsigned source, const uint8_t *m, size_t n)
{
    (void)source;
    if (m && n && n <= 3) (void)fm1_verbalizer_midi(m, n);
}

void groove_midi_init(void) {}
void groove_midi_poll(void) {}
void groove_midi_panic(void) {}
void groove_midi_clock_reset(void) {}
FM1_DSP_CODE unsigned groove_midi_mode(unsigned track) { (void)track; return 0; }
FM1_DSP_CODE void groove_midi_note(unsigned track, unsigned note, unsigned velocity)
{ (void)track; (void)note; (void)velocity; }
FM1_DSP_CODE void groove_midi_track_reset(unsigned track) { (void)track; }
FM1_DSP_CODE void groove_midi_transport(unsigned owner, uint8_t status) { (void)owner; (void)status; }
FM1_DSP_CODE void groove_midi_clock(unsigned owner, unsigned tempo) { (void)owner; (void)tempo; }

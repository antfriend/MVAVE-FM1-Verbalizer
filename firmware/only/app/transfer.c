/* SPDX-License-Identifier: MIT */
/*
 * Verbalizer-only firmware: no file transfers.
 *
 * The stock firmware answered the web tool's requests for projects, samples
 * and DX7 banks here. Those are gone, so application SysEx is ignored.
 * Firmware update messages never come this way: M-VAVE's library handles
 * them itself.
 */
#include "instrument.h"

void fm1_app_sysex(const uint8_t *m, size_t n) { (void)m; (void)n; }
void groove_transfer_init(void) {}
void groove_transfer_poll(void) {}

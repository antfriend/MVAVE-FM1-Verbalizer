/* SPDX-License-Identifier: MIT */
/*
 * Verbalizer-only firmware, browser build only.
 *
 * The simulator page still has its tabs for projects, samples, DX7 banks and
 * MIDI routing, and the simulator calls the application for them. None of
 * that exists any more, so each call answers "nothing here". The device
 * build does not compile this file.
 */
#include "instrument.h"
#include "fm_patch.h"

uint32_t groove_crc32(uint32_t crc, const void *data, unsigned bytes) { (void)data; (void)bytes; return crc; }
uint32_t groove_project_revision(void) { return 0; }
int groove_project_export(uint8_t *bytes, unsigned capacity) { (void)bytes; (void)capacity; return GROOVE_PROJECT_INVALID; }
int groove_project_import(const uint8_t *bytes, unsigned length) { (void)bytes; (void)length; return GROOVE_PROJECT_INVALID; }
const char *groove_factory_name(unsigned index) { (void)index; return ""; }
int groove_factory_build(unsigned index, struct groove_project *project) { (void)index; (void)project; return GROOVE_PROJECT_INVALID; }
int groove_fm_library_read(unsigned bank, uint8_t vmem[4096]) { (void)bank; (void)vmem; return -1; }
int groove_fm_library_write(unsigned bank, const uint8_t vmem[4096]) { (void)bank; (void)vmem; return -1; }
unsigned groove_samples_status(void) { return 0; }
unsigned groove_midi_setting(unsigned track, unsigned parameter) { (void)track; (void)parameter; return 0; }
void groove_settings_midi(unsigned track, unsigned parameter, int delta) { (void)track; (void)parameter; (void)delta; }

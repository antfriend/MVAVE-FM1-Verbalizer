/* SPDX-License-Identifier: MIT */
/*
 * Verbalizer-only firmware: what is left of the stock storage.
 *
 * M-VAVE's library still calls the application's project, sample-bank and
 * DX7-library entry points, so they exist here and do nothing. The one thing
 * kept is the Bluetooth settings (on/off, audio, MIDI role) and the screen's
 * colour scheme, in a sector of their own; the Verbalizer's knobs are stored by fm1_verbalizer.c.
 *
 * No stock data is touched: projects, the sample bank and the DX7 banks stay
 * in flash as they were, unread.
 */
#include "instrument.h"
#include "fm_patch.h"
#include "fm1_device.h"
#include <string.h>

#define SETTINGS_MAGIC 0x31534256U /* "VBS1" */
enum { SETTINGS_IN_USE = -2 };

struct stored {
    uint32_t magic;
    uint8_t radio_off, role, scheme, spare;
    uint32_t check;
};
static struct { uint8_t radio_off, role, scheme; } settings;
static uint8_t load_pending, role_pending, settings_dirty;
static uint32_t settings_changed;
static int settings_result;
static struct groove_project scratch;

static uint32_t check_of(const struct stored *record)
{
    return ~(record->magic ^ ((uint32_t)record->radio_off | (uint32_t)record->role << 8 |
                              (uint32_t)record->scheme << 16));
}
static int settings_load(void)
{
    struct stored record;
    int result = fm1_flash_read(GROOVE_SETTINGS_OFFSET, &record, sizeof record);
    if (result) return result;
    if (record.magic == SETTINGS_MAGIC && record.check == check_of(&record) &&
        record.radio_off <= GROOVE_RADIO_ALL && record.role <= FM1_BLE_CENTRAL) {
        settings.radio_off = record.radio_off;
        settings.role = record.role;
        settings.scheme = record.scheme;
    }
    return 0;
}
static int settings_save(void)
{
    const uint8_t *sector = fm1_flash_map(GROOVE_SETTINGS_OFFSET, FM1_FLASH_SECTOR_BYTES);
    struct stored record;
    int result = sector ? fm1_flash_read(GROOVE_SETTINGS_OFFSET, &record, sizeof record) : -1;
    if (result) return result;
    /* Only ever erase a blank sector or an earlier record. */
    if (record.magic != SETTINGS_MAGIC)
        for (unsigned i = 0; i < FM1_FLASH_SECTOR_BYTES; ++i)
            if (sector[i] != 0xff) return SETTINGS_IN_USE;
    memset(&record, 0, sizeof record);
    record.magic = SETTINGS_MAGIC;
    record.radio_off = settings.radio_off;
    record.role = settings.role;
    record.scheme = settings.scheme;
    record.check = check_of(&record);
    result = fm1_flash_erase(GROOVE_SETTINGS_OFFSET);
    return result ? result : fm1_flash_write(GROOVE_SETTINGS_OFFSET, &record, sizeof record);
}

/* BLE MIDI follows the radio master switch, as on the stock firmware. */
unsigned groove_settings_radio_flags(void) { return (GROOVE_RADIO_ALL ^ settings.radio_off) | GROOVE_RADIO_MIDI; }
unsigned groove_settings_role_value(void) { return settings.role; }
int groove_settings_status(void) { return settings_result; }
int groove_settings_radio(unsigned flags, unsigned role)
{
    if ((flags & ~GROOVE_RADIO_ALL) || role > FM1_BLE_CENTRAL) return -1;
    flags |= GROOVE_RADIO_MIDI;
    if (flags == groove_settings_radio_flags() && role == settings.role) return 0;
    settings.radio_off = (uint8_t)(GROOVE_RADIO_ALL ^ flags);
    settings.role = (uint8_t)role;
    role_pending = 2;
    settings_dirty = 1;
    settings_changed = fm1_millis();
    settings_result = 0;
    return 0;
}
/* The screen's colour scheme: 0, the green CRT, unless one has been stored. */
unsigned vb_settings_scheme(void) { return settings.scheme; }
void vb_settings_scheme_set(unsigned value)
{
    settings.scheme = (uint8_t)value;
    settings_dirty = 1;
    settings_changed = fm1_millis();
    settings_result = 0;
}

/* The SDK's build refuses an image without these library calls, which the
 * stock project and screen code used. Taking their addresses keeps them
 * linked. */
static volatile uintptr_t linked;
void groove_project_init(void)
{
    linked = (uintptr_t)groove_tuning_capture;
    linked = (uintptr_t)groove_tuning_apply;
    linked = (uintptr_t)groove_project_apply_edit;
    linked = (uintptr_t)groove_audio_stats;
    memset(&settings, 0, sizeof settings);
    load_pending = 1;
    role_pending = 2;
    settings_dirty = 0;
    settings_result = 0;
}
void groove_project_poll(void)
{
    if (load_pending) {
        if (settings_load() == FM1_FLASH_BUSY) return;
        load_pending = 0;
    }
    if (role_pending == 2) {
        unsigned flags = groove_settings_radio_flags();
        unsigned features = flags & GROOVE_RADIO_ON ? flags >> 1 : 0;
        if (!fm1_bluetooth_configure(features)) role_pending = 1;
    }
    if (role_pending == 1) {
        int result = fm1_ble_set_role(settings.role);
        if (!result || (result != -2 && result != -3)) role_pending = 0;
    }
    /* A changed setting is written once it has been left alone for a moment. */
    if (settings_dirty && fm1_millis() - settings_changed >= 1500U) {
        int result = settings_save();
        if (result == FM1_FLASH_BUSY) return;
        settings_result = result;
        settings_dirty = 0;
    }
}
void groove_project_name(char name[13]) { memcpy(name, "VERBALIZER  ", 13); }
void groove_project_metadata(const struct groove_project *p) { (void)p; }
struct groove_project *groove_project_scratch(void) { return &scratch; }

/* The sample bank and the DX7 banks are never read. */
void groove_samples_init(void) {}
void groove_samples_poll(void) {}
void groove_fm_library_init(void) {}
/* The library's DX7 asks for its voices here even though it is never
 * rendered: every program is the DX7's plain initial voice. */
FM1_DSP_CODE void groove_fm_program(uint8_t p[156], unsigned program)
{
    (void)program;
    memset(p, 0, 156);
    for (unsigned op = 0; op < 6; ++op) {
        uint8_t *q = p + op * 21;
        memset(q, 99, 7); /* rates 1-4, levels 1-3 */
        q[8] = 39;        /* break point */
        q[18] = 1;        /* coarse */
        q[20] = 7;        /* detune, centre */
    }
    p[105 + 16] = 99;         /* operator 1 output level */
    memset(p + 126, 99, 4);   /* pitch envelope rates */
    memset(p + 130, 50, 4);   /* and levels */
    p[136] = 1; p[137] = 35; p[141] = 1; p[143] = 3; p[144] = 24;
    memcpy(p + 145, "INIT VOICE", 10);
    p[155] = 63;
}

/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
 *
 * The screen of the Verbalizer-only firmware. It takes the place of M-VAVE's
 * app/screen.c (fm1_open V14) and is cut down from it: the font, the band
 * painter, the text and knob drawing and the Bluetooth page are M-VAVE's.
 * Every other stock page is gone. What is left:
 *
 *   - the Verbalizer's page, which is where the device starts;
 *   - the Bluetooth page, on GLO (HOME or GLO again goes back);
 *   - the keys, which play the Verbalizer, and OCT - / OCT +;
 *   - MASTER, which is still the stock output level;
 *   - two colour schemes, swapped with FX: a green CRT and M-VAVE's colours.
 */
#include "instrument.h"
#include "fm1_engine_control.h"
#include "verbalizer/fm1_verbalizer.h"
#include "verbalizer/fm1_verbalizer_ui.h"
#include <string.h>
/* The colours are looked up in the current scheme. The first seven have the
 * stock names; the other eight are the knob colours. */
enum { SCHEME_CRT, SCHEME_STOCK, SCHEMES, SCHEME_COLORS = 15 };
static const uint16_t schemes[SCHEMES][SCHEME_COLORS] = {
    /* a green phosphor screen: black, three greens and a near-white highlight */
    [SCHEME_CRT] = {0x0000, 0x0140, 0x37e6, 0xaff5, 0x0400, 0x07e0, 0xaff5,
                    0x07e0, 0x37e6, 0x07e0, 0x37e6, 0x07e0, 0x37e6, 0x07e0, 0x37e6},
    [SCHEME_STOCK] = {0x0000, 0x18e3, 0xef5d, 0x07ff, 0x738e, 0x07e0, 0xfd20,
                      0x3c9f, 0x07bf, 0x4eae, 0x06d4, 0xac9f, 0xed18, 0xff06, 0xfba5}
};
static const uint16_t *scheme = schemes[SCHEME_CRT];
static unsigned scheme_shown;
#define BACK   (scheme[0])
#define PANEL  (scheme[1])
#define TEXT   (scheme[2])
#define ACCENT (scheme[3])
#define DIM    (scheme[4])
#define GREEN  (scheme[5])
#define AMBER  (scheme[6])
#define colors (scheme + 7)
/* Kept with the Bluetooth settings, in project.c. */
unsigned vb_settings_scheme(void);
void vb_settings_scheme_set(unsigned value);
/* Five columns per glyph, bit 0 at the top. Digits followed by uppercase. */
static const uint8_t font[36][5] = {
    {0x3e, 0x51, 0x49, 0x45, 0x3e}, {0, 0x42, 0x7f, 0x40, 0},       {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4b, 0x31}, {0x18, 0x14, 0x12, 0x7f, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3c, 0x4a, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03}, {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1e}, {0x7e, 0x11, 0x11, 0x11, 0x7e}, {0x7f, 0x49, 0x49, 0x49, 0x36},
    {0x3e, 0x41, 0x41, 0x41, 0x22}, {0x7f, 0x41, 0x41, 0x22, 0x1c}, {0x7f, 0x49, 0x49, 0x49, 0x41},
    {0x7f, 0x09, 0x09, 0x09, 0x01}, {0x3e, 0x41, 0x49, 0x49, 0x7a}, {0x7f, 0x08, 0x08, 0x08, 0x7f},
    {0, 0x41, 0x7f, 0x41, 0},       {0x20, 0x40, 0x41, 0x3f, 0x01}, {0x7f, 0x08, 0x14, 0x22, 0x41},
    {0x7f, 0x40, 0x40, 0x40, 0x40}, {0x7f, 0x02, 0x0c, 0x02, 0x7f}, {0x7f, 0x04, 0x08, 0x10, 0x7f},
    {0x3e, 0x41, 0x41, 0x41, 0x3e}, {0x7f, 0x09, 0x09, 0x09, 0x06}, {0x3e, 0x41, 0x51, 0x21, 0x5e},
    {0x7f, 0x09, 0x19, 0x29, 0x46}, {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7f, 0x01, 0x01},
    {0x3f, 0x40, 0x40, 0x40, 0x3f}, {0x1f, 0x20, 0x40, 0x20, 0x1f}, {0x3f, 0x40, 0x38, 0x40, 0x3f},
    {0x63, 0x14, 0x08, 0x14, 0x63}, {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43}};
/* Compose the final pixels before touching the LCD. Sixteen rows cost 7,680
 * bytes; unchanged bands and unchanged horizontal margins are not transferred.
 * A page change never exposes the temporary background clear on the panel. */
enum { PAINT_ROWS = 16, PAINT_BANDS = FM1_SCREEN_HEIGHT / PAINT_ROWS };
_Static_assert(FM1_SCREEN_WIDTH * PAINT_ROWS <= FM1_SCREEN_DMA_PIXELS, "LCD band capacity");
static uint16_t paint_pixels[FM1_SCREEN_WIDTH * PAINT_ROWS];
static uint8_t paint_left[PAINT_BANDS], paint_right[PAINT_BANDS];
static unsigned paint_rendering, paint_top, paint_next, paint_x, paint_end;
static uint32_t paint_priority;
static struct fm1_ble_status frame_ble;
static struct fm1_bluetooth_status frame_radio;
static unsigned frame_radio_flags, frame_radio_role;
static void paint_mark(unsigned x, unsigned y, unsigned w, unsigned h)
{
    for (unsigned b = y / PAINT_ROWS; b <= (y + h - 1) / PAINT_ROWS; ++b) {
        paint_priority |= 1U << b;
        if (x < paint_left[b]) paint_left[b] = (uint8_t)x;
        if (x + w > paint_right[b]) paint_right[b] = (uint8_t)(x + w);
    }
}
/* Many LCD pages share this bounded fill; keep a single native copy. */
static __attribute__((noinline)) void paint_fill(unsigned x, unsigned y, unsigned w, unsigned h, uint16_t color)
{
    if (!w || !h || x >= FM1_SCREEN_WIDTH || y >= FM1_SCREEN_HEIGHT) return;
    if (w > FM1_SCREEN_WIDTH - x) w = FM1_SCREEN_WIDTH - x;
    if (h > FM1_SCREEN_HEIGHT - y) h = FM1_SCREEN_HEIGHT - y;
    if (!paint_rendering) { paint_mark(x, y, w, h); return; }
    if (x >= paint_end || x + w <= paint_x) return;
    unsigned right = x + w < paint_end ? x + w : paint_end;
    if (x < paint_x) x = paint_x;
    unsigned first = y < paint_top ? paint_top : y;
    unsigned end = y + h < paint_top + PAINT_ROWS ? y + h : paint_top + PAINT_ROWS;
    for (unsigned row = first; row < end; ++row)
        for (unsigned col = x; col < right; ++col)
            paint_pixels[(row - paint_top) * (paint_end - paint_x) + col - paint_x] = color;
}
static const uint8_t punctuation[][6] = {
    {45,8,8,8,8,8}, {47,0x60,0x18,6,1,0}, {43,8,8,0x3e,8,8},
    {35,0x14,0x7f,0x14,0x7f,0x14}, {37,0x63,0x13,8,0x64,0x63},
    {62,0x41,0x22,0x14,8,0}, {63,2,1,0x51,9,6},
    {33,0,0,0x5f,0,0}, {34,0,7,0,7,0}, {36,0x24,0x2a,0x7f,0x2a,0x12},
    {38,0x36,0x49,0x55,0x22,0x50}, {39,0,5,3,0,0}, {40,0,0x1c,0x22,0x41,0},
    {41,0,0x41,0x22,0x1c,0}, {42,0x14,8,0x3e,8,0x14}, {44,0,0x50,0x30,0,0},
    {46,0,0x60,0x60,0,0}, {58,0,0x36,0x36,0,0}, {59,0,0x56,0x36,0,0},
    {60,8,0x14,0x22,0x41,0}, {61,0x14,0x14,0x14,0x14,0x14},
    {64,0x32,0x49,0x79,0x41,0x3e}, {91,0,0x7f,0x41,0x41,0},
    {92,1,6,0x18,0x60,0}, {93,0,0x41,0x41,0x7f,0}, {94,4,2,1,2,4},
    {95,0x40,0x40,0x40,0x40,0x40}, {96,0,1,2,4,0}, {123,0,8,0x36,0x41,0},
    {124,0,0,0x7f,0,0}, {125,0,0x41,0x36,8,0}, {126,8,4,8,0x10,8}
};
static void text(unsigned x, unsigned y, const char *s, unsigned scale, uint16_t color,
                 uint16_t background)
{
    if (!s || !scale || scale > 4 || x >= FM1_SCREEN_WIDTH ||
        y >= FM1_SCREEN_HEIGHT || 7U * scale > FM1_SCREEN_HEIGHT - y) return;
    unsigned w = 6U * scale, h = 7U * scale;
    if (paint_rendering && (y >= paint_top + PAINT_ROWS || y + h <= paint_top)) return;
    if (!paint_rendering) {
        unsigned n = (unsigned)strlen(s), available = (FM1_SCREEN_WIDTH - x) / w;
        if (n > available) n = available;
        if (n) paint_mark(x, y, n * w, h);
        return;
    }
    for (; *s && x + w <= FM1_SCREEN_WIDTH; ++s, x += w) {
        if (x >= paint_end) break;
        if (x + w <= paint_x) continue;
        char letter = *s >= 'a' && *s <= 'z' ? (char)(*s - 'a' + 'A') : *s;
        static const uint8_t blank[5] = {0};
        const uint8_t *shape = blank;
        if (*s >= '0' && *s <= '9')
            shape = font[*s - '0'];
        else if (letter >= 'A' && letter <= 'Z')
            shape = font[10 + letter - 'A'];
        else if (*s != ' ')
            for (unsigned i = 0; i < sizeof punctuation / sizeof punctuation[0]; ++i)
                if ((uint8_t)*s == punctuation[i][0]) {
                    shape = punctuation[i] + 1;
                    break;
                }
        unsigned first_row = y < paint_top ? paint_top : y;
        unsigned end_row = y + h < paint_top + PAINT_ROWS ? y + h : paint_top + PAINT_ROWS;
        unsigned first_col = x < paint_x ? paint_x : x;
        unsigned end_col = x + w < paint_end ? x + w : paint_end;
        unsigned stride = paint_end - paint_x;
        for (unsigned row = first_row; row < end_row; ++row) {
            unsigned mask = 1U << ((row - y) / scale);
            uint16_t *pixel = paint_pixels + (row - paint_top) * stride + first_col - paint_x;
            for (unsigned col = first_col; col < end_col; ++col) {
                unsigned column = (col - x) / scale;
                *pixel++ = column < 5U && (shape[column] & mask) ? color : background;
            }
        }
    }
}
/* The page numbers are the stock ones, so the browser page reads the same. */
enum { BLE = 7, VERBALIZER = 19 };
enum { OCT_DOWN, OCT_UP, FX, SCL, ENV, LFO, EDIT, GLO, HOME, SAVE, ARP, SEQ, PLAY, REC };
#define BUTTON(b) (1U << (b))
static unsigned page = VERBALIZER, vb_page = 1, vb_loaded;
static int octave;
static const char *notice;
static uint32_t notice_until;
static char master_notice[24];
static uint8_t key_note[27];
static uint32_t previous_buttons, previous_keys, frame_ms;
static uint32_t encoder_time[7];
static int8_t encoder_direction[7];
static uint8_t encoder_speed[7];
static unsigned ble_device, ble_view, ble_action, ble_forget, ble_stage, ble_seconds;
static int ble_result;
static uint32_t ble_stage_since;
static uint32_t health_at, health_dropped, health_dma;
struct draw_cache {
    unsigned shown_page;
    uint32_t title_cache, hint_cache, eye_cache;
    uint32_t foot_cache[4], row_cache[8], tile_cache[8];
};
static struct draw_cache saved_cache, band_cache, *cache = &saved_cache;
static uint32_t led_cache;
static uint16_t button_led_cache;
static struct fm1_ble_device frame_devices[FM1_BLE_MAX_DEVICES];
static uint32_t frame_time;
static __attribute__((noinline)) int clamp(int value, int low, int high)
{
    return value < low ? low : value > high ? high : value;
}
static unsigned encoder_active(const int16_t values[7])
{
    for (unsigned i = 0; i < 7; ++i)
        if (values[i])
            return 1;
    return 0;
}
static void read_encoders(const struct fm1_controls *controls, uint32_t now, unsigned boundary)
{
    /* Apply the first detent immediately. A continuing fast turn accelerates
     * only continuous parameters; page/option selectors retain raw detents.
     * Reversal, a pause or a new control context returns to fine adjustment. */
    if (boundary) memset(encoder_direction, 0, sizeof encoder_direction);
    for (unsigned i = 0; i < 7; ++i) {
        int direction = (controls->encoders[i] > 0) - (controls->encoders[i] < 0);
        if (!direction) continue;
        encoder_speed[i] = direction == encoder_direction[i] && now - encoder_time[i] <= 40U
            ? (uint8_t)clamp(encoder_speed[i] + 1, 1, 4) : 1;
        encoder_direction[i] = (int8_t)direction;
        encoder_time[i] = now;
    }
}
#ifdef FM1_HOST_TEST
static unsigned web_fine;
void groove_screen_web_fine(unsigned fine) { web_fine = fine; }
#endif
static int fm_encoder_delta(unsigned encoder, int16_t value)
{
#ifdef FM1_HOST_TEST
    if (web_fine) return value;
#endif
    int amount = value < 0 ? -(int)value : value;
    int gain = amount < 2 ? 1 : amount < 4 ? 2 : amount == 4 ? 3 : 4;
    if (gain < encoder_speed[encoder]) gain = encoder_speed[encoder];
    return value * gain;
}
static __attribute__((noinline)) void decimal(char out[12], int value, unsigned digits)
{
    unsigned n = value < 0 ? (unsigned)-value : (unsigned)value, length;
    /* All displayed values are within -24..300; minimum width is optional. */
    length = 1;
    for (unsigned v = n; v >= 10; v /= 10)
        ++length;
    if (length < digits)
        length = digits;
    unsigned start = value < 0;
    if (start)
        out[0] = '-';
    out[start + length] = 0;
    while (length) {
        out[start + --length] = (char)('0' + n % 10);
        n /= 10;
    }
}
/* Share numeric painting across pages instead of expanding it at every call. */
static __attribute__((noinline)) void number(unsigned x, unsigned y, int value, unsigned digits, unsigned scale,
                   uint16_t foreground, uint16_t background)
{
    char out[12];
    decimal(out, value, digits);
    text(x, y, out, scale, foreground, background);
}
static uint32_t signature(uint32_t seed, const void *bytes, unsigned n)
{
    const uint8_t *p = bytes;
    for (unsigned i = 0; i < n; ++i)
        seed = seed * 33U + p[i];
    return seed;
}
#ifdef FM1_HOST_TEST
static char web_knobs[4][2][16];
#endif
static void knob(unsigned col, const char *label, const char *value, uint16_t color)
{
#ifdef FM1_HOST_TEST
    if (col < 4) {
        memset(web_knobs[col], 0, sizeof web_knobs[col]);
        unsigned n = (unsigned)strlen(label); if (n > 15) n = 15;
        memcpy(web_knobs[col][0], label, n);
        n = (unsigned)strlen(value); if (n > 15) n = 15;
        memcpy(web_knobs[col][1], value, n);
    }
#endif
    if (paint_rendering && (paint_top >= 227 || paint_top + PAINT_ROWS <= 195)) return;
    uint32_t sig =
        signature(signature(color, label, (unsigned)strlen(label)), value, (unsigned)strlen(value));
    if (cache->foot_cache[col] == sig)
        return;
    cache->foot_cache[col] = sig;
    unsigned x = 6 + 59 * col;
    paint_fill(x, 195, 54, 32, BACK);
    paint_fill(x, 195, 54, 2, color);
    text(x, 200, label, 1, TEXT, BACK);
    text(x, 212, value, strlen(value) <= 4 ? 2 : 1, color, BACK);
}
/* The Toot Toot Engineering eye: a white eyeball with a red iris that glances
 * about, as in the animated symbol on antfriend.github.io (its 100-unit
 * drawing scaled to a 27-pixel ball). It sits in the header, right of the
 * title, on every page. The iris and pupil jump to a new spot every 1.5 to 3
 * seconds; only then is the eye marked for repainting. */
enum { EYE_X = 168, EYE_Y = 14, EYE_R = 13, EYE_RED = 0xf800, EYE_WHITE = 0xffff };
static int8_t eye_dx, eye_dy;
static uint32_t eye_next, eye_random = 0x2545f491U;
/* Look at (x, y), in pixels from the centre, until `until`. */
static void eye_look(int x, int y, uint32_t until)
{
    if (x * x + y * y > 20) { x = x * 3 / 4; y = y * 3 / 4; } /* the iris stays inside the ball */
    eye_dx = (int8_t)x;
    eye_dy = (int8_t)y;
    eye_next = until;
}
/* Glance toward the control that was just used: the encoders in the order of
 * the table are SELECT, ALGORITHM, KNOB 1-4 and PRESETS, each toward where its
 * value is drawn; a key looks along the keyboard, left to right. */
static void eye_glance_encoder(unsigned encoder, uint32_t now)
{
    static const int8_t toward[7][2] = {{-3, -3}, {-4, 1}, {-3, 3}, {-1, 3}, {1, 3}, {3, 3}, {-2, 3}};
    eye_look(toward[encoder][0], toward[encoder][1], now + 1500U);
}
static void eye_glance_key(unsigned key, uint32_t now) { eye_look((int)(key * 8U / 26U) - 4, 2, now + 1500U); }
static void eye_update(uint32_t now)
{
    if ((int32_t)(now - eye_next) < 0) return;
    eye_random ^= eye_random << 13; eye_random ^= eye_random >> 17; eye_random ^= eye_random << 5;
    int x = (int)(eye_random % 9U) - 4, y = (int)((eye_random >> 8) % 9U) - 4;
    eye_look(x, y, now + 1500U + (eye_random >> 16) % 1500U);
}
static __attribute__((noinline)) void eye_draw(void)
{
    if (!paint_rendering) {
        uint32_t sig = 0x10000U | (uint32_t)(eye_dx + 8) << 8 | (uint32_t)(eye_dy + 8);
        if (cache->eye_cache == sig) return;
        cache->eye_cache = sig;
        paint_mark(EYE_X - EYE_R, EYE_Y - EYE_R, 2 * EYE_R + 1, 2 * EYE_R + 1);
        return;
    }
    unsigned top = EYE_Y - EYE_R, bottom = EYE_Y + EYE_R + 1;
    unsigned left = EYE_X - EYE_R, right = EYE_X + EYE_R + 1;
    unsigned row = top < paint_top ? paint_top : top;
    unsigned row_end = bottom < paint_top + PAINT_ROWS ? bottom : paint_top + PAINT_ROWS;
    unsigned col_start = left < paint_x ? paint_x : left;
    unsigned col_end = right < paint_end ? right : paint_end;
    unsigned stride = paint_end - paint_x;
    for (; row < row_end; ++row)
        for (unsigned col = col_start; col < col_end; ++col) {
            int dx = (int)col - EYE_X, dy = (int)row - EYE_Y, d2 = dx * dx + dy * dy;
            if (d2 > 160) continue; /* r 12.6: no spurs at the compass points */
            int ix = dx - eye_dx, iy = dy - eye_dy, i2 = ix * ix + iy * iy;
            int hx = dx + 3, hy = dy + 3;
            uint16_t color = d2 > 130 ? TEXT /* outline */
                : hx * hx + hy * hy <= 6 ? EYE_WHITE /* highlight */
                : i2 <= 9 ? BACK /* pupil */
                : i2 <= 49 ? EYE_RED /* iris */
                : EYE_WHITE;
            paint_pixels[(row - paint_top) * stride + col - paint_x] = color;
        }
}
static void radio_line(unsigned slot, unsigned y, const char *value, uint16_t color)
{
    uint32_t sig = signature(color, value, (unsigned)strlen(value));
    if (cache->tile_cache[slot] == sig) return;
    cache->tile_cache[slot] = sig;
    paint_fill(8, y, 224, 7, BACK);
    text(8, y, value, 1, color, BACK);
}
static __attribute__((noinline)) void draw_ble(const struct fm1_ble_status *status)
{
    static const char *const states[] = {"OFF", "IDLE", "WAITING FOR DEVICE",
        "SCANNING", "CONNECTING", "FINDING MIDI", "CONNECTED", "MIDI READY", "DISCONNECTING"};
    static const char *const errors[] = {
        "", "STACK BUSY", "TIMED OUT", "NOT BLE MIDI", "GATT FAILED", "LINK LOST"};
    unsigned on = (frame_radio_flags & GROOVE_RADIO_ON) != 0;
    unsigned midi = frame_radio_flags & GROOVE_RADIO_MIDI ? 1U + frame_radio_role : 0;
    if (cache->title_cache != ble_view) {
        cache->title_cache = ble_view;
        memset(cache->row_cache, 0xff, sizeof cache->row_cache);
        memset(cache->tile_cache, 0xff, sizeof cache->tile_cache);
        paint_fill(0, 0, 240, 240, BACK);
        paint_fill(0, 29, 240, 1, PANEL);
        text(8, 8, ble_view ? "MIDI HOST" : "BLUETOOTH", 2, ACCENT, BACK);
    }
    const char *state = !on ? "BLUETOOTH OFF" : !frame_radio.ready ? "RADIO AVAILABLE ON DEVICE"
        : status->pending ? "APPLYING SETTINGS"
        : !midi ? "MIDI OFF" : status->state == FM1_BLE_OFF ? "STARTING MIDI"
        : status->state <= FM1_BLE_DISCONNECTING ? states[status->state] : "UNKNOWN";
    char progress[40];
    if (on && frame_radio.ready && (status->pending || status->state == FM1_BLE_SCANNING ||
        status->state == FM1_BLE_CONNECTING || status->state == FM1_BLE_DISCOVERING)) {
        unsigned n = (unsigned)strlen(state);
        memcpy(progress, state, n);
        progress[n++] = ' ';
        decimal(progress + n, (int)ble_seconds, 1);
        n = (unsigned)strlen(progress);
        progress[n++] = 'S'; progress[n] = 0;
        state = progress;
    }
    radio_line(0, 39, state, on ? GREEN : DIM);
    if (!ble_view) {
        static const char *const labels[] = {"K1 BLUETOOTH", "K2 BT AUDIO", "K3 BLE ROLE"};
        unsigned values[] = {on, (frame_radio_flags & GROOVE_RADIO_AUDIO) != 0, midi};
        const char *const names[] = {on ? "ON" : "OFF", values[1] ? "ON" : "OFF",
            midi == 2 ? "HOST" : midi == 1 ? "DEVICE" : "OFF"};
        for (unsigned i = 0; i < 3; ++i) {
            uint32_t sig = values[i] | (on << 8);
            if (cache->row_cache[i] == sig) continue;
            cache->row_cache[i] = sig;
            unsigned y = 61 + i * 30;
            paint_fill(8, y, 224, 20, BACK);
            text(8, y + 4, labels[i], 1, TEXT, BACK);
            text(146, y, names[i], 2, i && i < 3 && !on ? DIM : ACCENT, BACK);
        }
        const char *audio = !on || !values[1] ? "AUDIO OFF" : frame_radio.audio_playing ? "AUDIO PLAYING"
            : frame_radio.audio_connected ? "AUDIO CONNECTED" : "AUDIO WAITING FOR CONNECTION";
        radio_line(1, 184, audio, frame_radio.audio_playing ? GREEN : DIM);
        radio_line(2, 196, ble_action ? "> FORGET MIDI PAIRS" : "> MIDI DEVICE LIST (HOST)", ACCENT);
    } else {
        for (unsigned i = 0; i < FM1_BLE_MAX_DEVICES; ++i) {
            const struct fm1_ble_device *device = &frame_devices[i];
            uint32_t sig = signature(i == ble_device, device, sizeof *device);
            sig = sig * 33U + (i < status->count);
            if (!status->count) sig = sig * 33U + status->state;
            if (cache->row_cache[i] == sig) continue;
            cache->row_cache[i] = sig;
            unsigned y = 60 + i * 17;
            uint16_t bg = i < status->count && i == ble_device ? PANEL : BACK;
            paint_fill(8, y, 224, 15, bg);
            if (i < status->count) {
                number(12, y + 4, i + 1, 1, 1, ACCENT, bg);
                text(27, y + 4, device->name[0] ? device->name : "UNNAMED MIDI DEVICE", 1, TEXT, bg);
                char signal[12];
                decimal(signal, device->rssi, 1);
                text(204, y + 4, signal, 1, DIM, bg);
            } else if (!i)
                text(12, y + 4, status->state == FM1_BLE_SCANNING ? "LOOKING FOR MIDI DEVICES..."
                    : "NO DEVICES - SELECT SCAN BELOW", 1, DIM, bg);
        }
        static const char *const actions[] = {"> CONNECT SELECTED", "> SCAN", "> DISCONNECT", "> SETTINGS"};
        radio_line(1, 200, actions[ble_action], ACCENT);
    }
    const char *error = ble_forget == 1 ? "FORGET MIDI PAIRS? SAVE YES / HOME NO"
        : ble_forget == 2 && !status->pending && !status->error ? "PAIR AGAIN; FORGET FM1 ON PEER TOO"
        : groove_settings_status() ? "SETTINGS NOT SAVED - RETRY"
        : ble_result == -2 ? "BUSY - RETRY" : ble_result == -3 ? "RADIO AVAILABLE ON DEVICE"
        : ble_result ? "COMMAND UNAVAILABLE"
        : status->error <= FM1_BLE_LINK_LOST ? errors[status->error] : "";
    radio_line(3, 213, error, AMBER);
    radio_line(4, 229, "PRESETS ACTION  SAVE OK  HOME BACK", DIM);
}
static __attribute__((noinline, cold, minsize)) void verbalizer_draw(void)
{
    static const char *const encoder[2] = {"ALGORITHM", "PRESETS"};
    char value[12];
    if (cache->title_cache != vb_page) {
        cache->title_cache = vb_page;
        paint_fill(0, 0, 240, 29, BACK);
        paint_fill(0, 30, 240, 164, BACK);
        text(8, 8, "VERBALIZER", 2, ACCENT, BACK);
        number(198, 8, (int)vb_page + 1, 1, 2, TEXT, BACK);
        text(210, 15, "/7", 1, DIM, BACK);
        text(8, 40, fm1_vb_page_name(vb_page), 3, TEXT, BACK);
        memset(cache->row_cache, 0xff, sizeof cache->row_cache);
    }
    for (unsigned i = 0; i < FM1_VB_SLOTS; ++i) {
        const char *label = fm1_vb_label(vb_page, i);
        fm1_vb_value(vb_page, i, value);
        if (i >= 1 && i <= 4) { knob(i - 1, label ? label : "", value, colors[i]); continue; }
        if (!label) continue;
        unsigned row = i ? 1 : 0, y = 76 + row * 58;
        uint32_t sig = signature(i, value, (unsigned)strlen(value));
        if (cache->row_cache[row] == sig) continue;
        cache->row_cache[row] = sig;
        paint_fill(8, y, 224, 50, PANEL);
        text(14, y + 6, encoder[row], 1, DIM, PANEL);
        text(14, y + 22, label, 2, TEXT, PANEL);
        text(110, y + 16, value, 3, colors[row ? 5 : 0], PANEL);
    }
    const char *hint = notice ? notice : "SELECT PAGE  SAVE KEEP  GLO RADIO";
    uint32_t sig = signature(0, hint, (unsigned)strlen(hint));
    if (cache->hint_cache == sig) return;
    cache->hint_cache = sig;
    paint_fill(6, 231, 228, 7, BACK);
    text(6, 231, hint, 1, DIM, BACK);
}
static void leds(void)
{
    uint32_t bits = 0;
    for (unsigned key = 0; key < 27; ++key)
        if (key_note[key] != 0xff) bits |= 1U << key;
    uint32_t changed = bits ^ led_cache;
    for (unsigned key = 0; key < 27; ++key)
        if (changed & (1U << key)) fm1_led_set(key + 14, (bits >> key) & 1U);
    led_cache = bits;
    bits = ((octave < 0) << OCT_DOWN) | ((octave > 0) << OCT_UP) |
           ((page == VERBALIZER) << HOME) | ((page == BLE) << GLO);
    changed = bits ^ button_led_cache;
    for (unsigned key = 0; key < 14; ++key)
        if (changed & (1U << key)) fm1_led_set(key, (bits >> key) & 1U);
    button_led_cache = (uint16_t)bits;
}
static void draw_page(void)
{
    if (page != cache->shown_page) {
        cache->shown_page = page;
        paint_fill(0, 0, 240, 240, BACK);
        paint_fill(0, 29, 240, 1, PANEL);
        cache->title_cache = cache->hint_cache = cache->eye_cache = ~0U;
        memset(cache->foot_cache, 0xff, sizeof cache->foot_cache);
        memset(cache->row_cache, 0xff, sizeof cache->row_cache);
        memset(cache->tile_cache, 0xff, sizeof cache->tile_cache);
    }
    if (!paint_rendering) leds();
    if (page == BLE) draw_ble(&frame_ble);
    else verbalizer_draw();
    eye_draw(); /* last, so it is on top of the page's header */
}
static void draw(const struct fm1_ble_status *ble)
{
    frame_time = fm1_millis();
    if (notice && (int32_t)(frame_time - notice_until) >= 0) notice = NULL;
    /* Read here, not only on FX: the stored choice arrives after start-up. */
    unsigned wanted = vb_settings_scheme() < SCHEMES ? vb_settings_scheme() : SCHEME_CRT;
    if (wanted != scheme_shown) {
        scheme_shown = wanted;
        scheme = schemes[wanted];
        saved_cache.shown_page = ~0U;
    }
    eye_update(frame_time);
    frame_ble = *ble;
    if (page == BLE) {
        fm1_bluetooth_get_status(&frame_radio);
        frame_radio_flags = groove_settings_radio_flags();
        frame_radio_role = groove_settings_role_value();
        memset(frame_devices, 0, sizeof frame_devices);
        for (unsigned i = 0; i < ble->count && i < FM1_BLE_MAX_DEVICES; ++i)
            fm1_ble_get_device(i, &frame_devices[i]);
    }
    /* Merge new damage with unfinished bands. Input can replace a pending
     * frame; no stale queue of old encoder positions needs to be displayed. */
    paint_priority = 0;
    cache = &saved_cache;
    draw_page();
}
static void paint_flush(void)
{
    uint32_t started = fm1_millis();
    paint_rendering = 1;
    cache = &band_cache;
    for (unsigned visited = 0; visited < PAINT_BANDS; ++visited) {
        int pending = fm1_screen_pending();
        if (pending) {
            if (pending < 0) saved_cache.shown_page = ~0U;
            break;
        }
        unsigned band = paint_next;
        /* Fresh knob/key feedback goes ahead of the rest of a page repaint.
         * Rotate within that set so continuous motion cannot starve its text. */
        if (paint_priority)
            for (unsigned i = 0; i < PAINT_BANDS; ++i) {
                unsigned candidate = (paint_next + i) % PAINT_BANDS;
                if (paint_priority & (1U << candidate)) { band = candidate; break; }
            }
        paint_next = (band + 1U) % PAINT_BANDS;
        unsigned left = paint_left[band], width = paint_right[band] - left;
        if (left >= paint_right[band]) continue;
        paint_top = band * PAINT_ROWS;
        paint_x = left;
        paint_end = left + width;
        memset(cache, 0xff, sizeof *cache);
        draw_page();
        /* Rasterize directly into the tightly packed DMA rectangle. */
        if (fm1_screen_submit(left, paint_top, width, PAINT_ROWS,
                            paint_pixels, width * PAINT_ROWS)) {
            saved_cache.shown_page = ~0U; /* Retry the whole page after a transport error. */
            break;
        }
        paint_left[band] = FM1_SCREEN_WIDTH;
        paint_right[band] = 0;
        paint_priority &= ~(1U << band);
        /* The DMA borrows paint_pixels. The next iteration/poll must observe
         * completion before composing another band into that same buffer. */
        if (fm1_millis() - started >= 4U) break;
    }
    paint_rendering = 0;
    cache = &saved_cache;
}
void groove_screen_init(void)
{
    page = VERBALIZER;
    octave = 0;
    notice = NULL;
    previous_buttons = previous_keys = 0;
    ble_device = ble_view = ble_action = ble_forget = ble_stage = ble_seconds = 0;
    ble_result = 0;
    ble_stage_since = health_at = fm1_millis();
    struct fm1_audio_stats dma;
    fm1_audio_stats_read(&dma);
    health_dma = dma.overruns;
    health_dropped = fm1_verbalizer_dropped();
    cache->shown_page = ~0U;
    led_cache = button_led_cache = 0;
    memset(key_note, 0xff, sizeof key_note);
    memset(paint_left, FM1_SCREEN_WIDTH, sizeof paint_left);
    memset(paint_right, 0, sizeof paint_right);
    paint_next = paint_rendering = paint_priority = 0;
    memset(encoder_direction, 0, sizeof encoder_direction);
    memset(encoder_speed, 0, sizeof encoder_speed);
    fm1_controls_init();
    for (unsigned key = 0; key < 41; ++key) fm1_led_set(key, 0);
    fm1_screen_init();
    struct fm1_ble_status ble = {0};
    fm1_ble_get_status(&ble);
    draw(&ble);
    paint_flush();
    frame_ms = fm1_millis();
}
/* The keys play the Verbalizer directly: F below middle C upwards, as on the
 * stock firmware. A message that does not fit the queue is tried again on
 * the next pass, so a key is never left sounding. */
static void play_keys(uint32_t keys)
{
    for (unsigned key = 0; key < 27; ++key) {
        unsigned note = (unsigned)clamp(53 + (int)key + octave * 12, 0, 127);
        unsigned down = (keys >> key) & 1U;
        if (key_note[key] != 0xff && (!down || key_note[key] != note)) {
            const uint8_t off[3] = {0x80, key_note[key], 0};
            if (fm1_verbalizer_midi(off, 3)) return;
            key_note[key] = 0xff;
        }
        if (down && key_note[key] == 0xff) {
            const uint8_t on[3] = {0x90, (uint8_t)note, 100};
            if (fm1_verbalizer_midi(on, 3)) return;
            key_note[key] = (uint8_t)note;
        }
    }
}
static void ble_controls(const struct fm1_controls *controls, uint32_t pressed, const struct fm1_ble_status *ble)
{
    if (encoder_active(controls->encoders)) ble_forget = 0;
    ble_action = (unsigned)clamp((int)ble_action + controls->encoders[6], 0, ble_view ? 3 : 1);
    if (!ble_view) {
        unsigned flags = groove_settings_radio_flags(), role = groove_settings_role_value();
        for (unsigned i = 0; i < 2; ++i) {
            int delta = controls->encoders[2 + i];
            if (delta > 0) flags |= 1U << i;
            else if (delta < 0) flags &= ~(1U << i);
        }
        if (controls->encoders[4])
            role = (unsigned)clamp((int)role + controls->encoders[4], FM1_BLE_PERIPHERAL, FM1_BLE_CENTRAL);
        if (flags != groove_settings_radio_flags() || role != groove_settings_role_value())
            ble_result = groove_settings_radio(flags, role);
        if (pressed & BUTTON(SAVE)) {
            if (ble_action) {
                if (ble_forget != 1) ble_forget = 1;
                else { ble_result = fm1_ble_forget_bonds(); ble_forget = ble_result ? 0 : 2; }
            } else if ((flags & (GROOVE_RADIO_ON | GROOVE_RADIO_MIDI)) ==
                (GROOVE_RADIO_ON | GROOVE_RADIO_MIDI) && role == FM1_BLE_CENTRAL) {
                ble_view = 1; ble_action = ble->count ? 0 : 1; ble_forget = 0;
                if (ble->state == FM1_BLE_IDLE) ble_result = fm1_ble_scan();
            } else ble_result = -1;
        }
    } else {
        unsigned count = ble->count > FM1_BLE_MAX_DEVICES ? FM1_BLE_MAX_DEVICES : ble->count;
        ble_device = (unsigned)clamp((int)ble_device + controls->encoders[0], 0, count ? (int)count - 1 : 0);
        if (pressed & BUTTON(SAVE)) {
            if (!ble_action) ble_result = fm1_ble_connect(ble_device);
            else if (ble_action == 1) {
                ble_result = fm1_ble_scan();
                if (!ble_result) ble_device = 0;
            } else if (ble_action == 2) ble_result = fm1_ble_disconnect();
            else { ble_view = ble_action = ble_forget = 0; ble_result = 0; }
        }
    }
}
static void verbalizer_controls(const struct fm1_controls *controls, uint32_t pressed, uint32_t now)
{
    if (pressed & BUTTON(SAVE)) {
        int result = fm1_verbalizer_save(GROOVE_VERBALIZER_OFFSET);
        notice = !result ? "VERBALIZER SAVED"
               : result == FM1_VERBALIZER_SECTOR_IN_USE ? "NO ROOM - STOCK SAMPLES ARE THERE" : "SAVE FAILED";
        notice_until = now + 2000U;
    }
    if (controls->encoders[0]) {
        vb_page = (unsigned)clamp((int)vb_page + (controls->encoders[0] > 0 ? 1 : -1), 0, FM1_VB_PAGES - 1);
        fm1_vb_page_open(vb_page);
    }
    for (unsigned i = 1; i < 7; ++i)
        fm1_vb_turn(vb_page, i - 1, fm_encoder_delta(i, controls->encoders[i]));
}
void groove_screen_control(void)
{
    struct fm1_controls controls;
    fm1_controls_read(&controls);
    uint32_t now = fm1_millis(), pressed = controls.buttons & ~previous_buttons;
    /* MASTER is still the stock output level, applied in fm1_output_mix. */
    unsigned master_changed = groove_master_update(controls.knob);
    if (master_changed && (!notice || notice == master_notice || (int32_t)(now - notice_until) >= 0)) {
        unsigned level = fm1_engine_output_level();
        if (level) {
            memcpy(master_notice, "MASTER ", 7);
            decimal(master_notice + 7, (int)((level * 100U + 63U) / 127U), 1);
            memcpy(master_notice + strlen(master_notice), " / 100", sizeof " / 100");
        }
        else memcpy(master_notice, "MASTER MUTED", sizeof "MASTER MUTED");
        notice = master_notice;
        notice_until = now + 1200U;
    }
    if (now - health_at >= 500U) {
        struct fm1_audio_stats dma;
        fm1_audio_stats_read(&dma);
        uint32_t dropped = fm1_verbalizer_dropped();
        if (dma.overruns != health_dma) {
            notice = "AUDIO OVERLOAD"; notice_until = now + 2500U;
        } else if (dropped != health_dropped) {
            notice = "MIDI FULL - MESSAGES LOST"; notice_until = now + 2500U;
        }
        health_dma = dma.overruns; health_dropped = dropped; health_at = now;
    }
    unsigned input_changed = master_changed || controls.buttons != previous_buttons ||
                             controls.keys != previous_keys || encoder_active(controls.encoders);
    struct fm1_ble_status ble = {0};
    fm1_ble_get_status(&ble);
    unsigned stage = ble.state | (ble.pending << 8);
    if (stage != ble_stage) { ble_stage = stage; ble_stage_since = now; }
    ble_seconds = (now - ble_stage_since) / 1000U;
    if (ble_seconds > 999) ble_seconds = 999;
    read_encoders(&controls, now, controls.buttons != previous_buttons || controls.encoders[0] || controls.encoders[6]);
    if (!vb_loaded) {
        /* Here, not at start-up, because flash belongs to this task. */
        vb_loaded = 1;
        if (!fm1_verbalizer_load(GROOVE_VERBALIZER_OFFSET)) {
            unsigned mode = fm1_verbalizer_mode();
            vb_page = mode ? mode * 3 - 2 : 0;
        }
    }
    for (unsigned i = 0; i < 7; ++i)
        if (controls.encoders[i]) eye_glance_encoder(i, now);
    uint32_t keys_down = controls.keys & ~previous_keys;
    for (unsigned key = 0; key < 27; ++key)
        if (keys_down & (1U << key)) { eye_glance_key(key, now); break; }
    unsigned old_page = page;
    if (pressed & BUTTON(GLO)) page = page == BLE ? VERBALIZER : BLE;
    if (pressed & BUTTON(HOME)) page = VERBALIZER;
    if (pressed & BUTTON(FX)) vb_settings_scheme_set((vb_settings_scheme() + 1U) % SCHEMES);
    if (page != old_page) { ble_view = ble_forget = ble_action = 0; ble_result = 0; }
    octave = clamp(octave + !!(pressed & BUTTON(OCT_UP)) - !!(pressed & BUTTON(OCT_DOWN)), -2, 2);
    if (page == BLE) ble_controls(&controls, pressed, &ble);
    else verbalizer_controls(&controls, pressed, now);
    play_keys(controls.keys);
    previous_buttons = controls.buttons;
    previous_keys = controls.keys;
    /* Input changes do not wait for the animation timer. LCD work is bounded
     * separately and continues on later polls, even between animation frames. */
    if (now - frame_ms >= 33U || page != old_page || input_changed || saved_cache.shown_page == ~0U) {
        frame_ms = now;
        draw(&ble);
    }
    paint_flush();
}
#ifdef FM1_HOST_TEST
/* What the browser page asks of the screen. Its shortcuts into the stock
 * editors have nothing to open here. */
void groove_screen_snapshot(uint8_t state[8])
{
    memset(state, 0, 8);
    state[1] = (uint8_t)page;
    state[7] = (uint8_t)(octave + 24);
}
int groove_screen_open(unsigned target)
{
    if (target) return -1;
    page = VERBALIZER;
    return 0;
}
void groove_screen_tap(uint32_t now) { (void)now; }
int groove_screen_preset(unsigned slot) { (void)slot; return -1; }
unsigned groove_screen_preset_position(void) { return 0; }
unsigned groove_screen_web_context(void) { return page | vb_page << 8 | ble_view << 16; }
unsigned groove_screen_web_mask(void) { return 0; }
const void *groove_screen_web_targets(void) { return web_knobs; }
unsigned groove_screen_web_target_count(void) { return 0; }
int groove_screen_web_adjust(unsigned context, unsigned action, unsigned index, unsigned owner, int delta)
{ (void)context; (void)action; (void)index; (void)owner; (void)delta; return -1; }
const char *groove_screen_web_knobs(void) { return &web_knobs[0][0][0]; }
unsigned groove_screen_web_first(void) { return 0; }
int groove_screen_web_step(unsigned step, unsigned toggle) { (void)step; (void)toggle; return -1; }
#endif

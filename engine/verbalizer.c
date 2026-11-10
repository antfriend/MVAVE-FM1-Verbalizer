/*
 * The Verbalizer instrument. A port of VerbalizeIt.spin and the main loop of
 * Verbalizer.spin by Dan Ray.
 *
 * Differences from the original, all on purpose:
 *
 *  - Key states change on key events instead of on a matrix scan. The states
 *    and what each mode does with them every 2 ms are the same.
 *  - Formants mode feeds voice 0 when its queue has run out, taking held keys
 *    in turn. The original re-queued every held key on every pass and relied
 *    on go() blocking to pace itself.
 *  - Changing mode releases all keys and fades the voices, so a held note
 *    cannot be left droning.
 *  - The stereo spatializer is not ported yet; the voices are mixed to mono.
 */
#include "verbalizer.h"

#include <string.h>

/* key states, as in Verbalizer.spin */
enum { SILENCE = 0, RELEASE = 1, SUSTAIN = 2, TRIGGER = 3 };

#define NO_KEY (-1000)

#define KEY_OF(index) ((int)(index) + VB_KEY_MIN)

typedef void (*play_fn)(vb_voice *v, int key);

/* A key wants a voice. Returns true if it got one. */
static bool go_if_available(verbalizer *vb, int key, play_fn play)
{
    int i;

    /* if a voice last played this key, only that voice may take it */
    for (i = 0; i < VB_VOICES; i++) {
        if (vb->voice_key[i] == key) {
            if (!vb_voice_done(&vb->voice[i]))
                return false; /* this key is in use */
            play(&vb->voice[i], key);
            return true;
        }
    }
    for (i = 0; i < VB_VOICES; i++) {
        if (vb_voice_done(&vb->voice[i])) {
            vb->voice_key[i] = key;
            play(&vb->voice[i], key);
            return true;
        }
    }
    return false; /* all voices busy; the key keeps trying */
}

static void scan_tsr(verbalizer *vb)
{
    int k, i;

    for (k = 0; k < VB_NUM_KEYS; k++) {
        if (vb->key_state[k] != RELEASE)
            continue;
        for (i = 0; i < VB_VOICES; i++) {
            if (vb->voice_key[i] == KEY_OF(k))
                vb_voice_release(&vb->voice[i], KEY_OF(k));
        }
    }
    for (k = 0; k < VB_NUM_KEYS; k++) {
        if (vb->key_state[k] != SUSTAIN)
            continue;
        for (i = 0; i < VB_VOICES; i++) {
            if (vb->voice_key[i] == KEY_OF(k) && vb_voice_done(&vb->voice[i]))
                vb_voice_sustain(&vb->voice[i], KEY_OF(k));
        }
    }
    for (k = 0; k < VB_NUM_KEYS; k++) {
        if (vb->key_state[k] == TRIGGER && go_if_available(vb, KEY_OF(k), vb_voice_trigger))
            vb->key_state[k] = SUSTAIN;
    }
}

static void scan_words(verbalizer *vb)
{
    int k;

    for (k = 0; k < VB_NUM_KEYS; k++) {
        if (vb->key_state[k] == TRIGGER && go_if_available(vb, KEY_OF(k), vb_voice_word))
            vb->key_state[k] = SUSTAIN;
    }
}

static void scan_formants(verbalizer *vb)
{
    vb_voice *v = &vb->voice[0];
    int k, n;

    /* Fade on each release. If frames are piling up (where the original
     * would have stalled waiting for a free buffer), hold the fade back
     * until there is room rather than lose it. */
    for (k = 0; k < VB_NUM_KEYS; k++) {
        if (vb->key_state[k] == RELEASE)
            vb->formants_fade_pending++;
    }
    while (vb->formants_fade_pending && vt_queued(&v->vt) <= VT_QUEUE_FRAMES / 2) {
        vb_voice_null(v);
        vb->formants_fade_pending--;
    }
    if (!vb_voice_done(v))
        return;
    for (n = 1; n <= VB_NUM_KEYS; n++) {
        k = (vb->formants_cursor + n) % VB_NUM_KEYS;
        if (vb->key_state[k] == TRIGGER || vb->key_state[k] == SUSTAIN) {
            vb_voice_formants(v, KEY_OF(k));
            vb->key_state[k] = SUSTAIN;
            vb->formants_cursor = k;
            break;
        }
    }
}

static void scan(verbalizer *vb)
{
    int k;

    switch (vb->mode) {
    case VB_MODE_WORDS:    scan_words(vb); break;
    case VB_MODE_TSR:      scan_tsr(vb); break;
    case VB_MODE_FORMANTS: scan_formants(vb); break;
    }
    /* a release is seen for one scan only */
    for (k = 0; k < VB_NUM_KEYS; k++) {
        if (vb->key_state[k] == RELEASE)
            vb->key_state[k] = SILENCE;
    }
}

void vb_init(verbalizer *vb)
{
    int i;

    memset(vb, 0, sizeof *vb);
    vb->mode = VB_MODE_TSR;
    for (i = 0; i < VB_VOICES; i++) {
        vb_voice_start(&vb->voice[i], vb->pot);
        vt_advance_noise(&vb->voice[i].vt, (unsigned)i * 1000u);
        vb->voice_key[i] = NO_KEY;
    }
    for (i = 0; i < VB_VOICES; i++)
        vb_voice_null(&vb->voice[i]);
}

void vb_set_mode(verbalizer *vb, vb_mode mode)
{
    int i;

    if (mode == vb->mode)
        return;
    vb->mode = mode;
    vb->formants_fade_pending = 0;
    memset(vb->key_state, SILENCE, sizeof vb->key_state);
    for (i = 0; i < VB_VOICES; i++)
        vb_voice_null(&vb->voice[i]);
}

bool vb_key_down(verbalizer *vb, int key)
{
    uint8_t *state;

    if (key < VB_KEY_MIN || key > VB_KEY_MAX)
        return false;
    state = &vb->key_state[key - VB_KEY_MIN];
    if (*state != SUSTAIN)
        *state = TRIGGER;
    return true;
}

bool vb_key_up(verbalizer *vb, int key)
{
    uint8_t *state;

    if (key < VB_KEY_MIN || key > VB_KEY_MAX)
        return false;
    state = &vb->key_state[key - VB_KEY_MIN];
    *state = (uint8_t)(*state == SUSTAIN ? RELEASE : SILENCE);
    return true;
}

bool vb_note_on(verbalizer *vb, int note)
{
    return vb_key_down(vb, note - VB_MIDI_OFFSET);
}

bool vb_note_off(verbalizer *vb, int note)
{
    return vb_key_up(vb, note - VB_MIDI_OFFSET);
}

void vb_all_keys_up(verbalizer *vb)
{
    int key;

    for (key = VB_KEY_MIN; key <= VB_KEY_MAX; key++)
        vb_key_up(vb, key);
}

bool vb_idle(const verbalizer *vb)
{
    int i;

    if (vb->formants_fade_pending)
        return false;
    for (i = 0; i < VB_NUM_KEYS; i++) {
        if (vb->key_state[i] != SILENCE)
            return false;
    }
    for (i = 0; i < VB_VOICES; i++) {
        if (!vb_voice_done(&vb->voice[i]))
            return false;
    }
    return true;
}

void vb_render(verbalizer *vb, int32_t *out, size_t count)
{
    while (count) {
        size_t n, i;

        if (vb->scan_countdown == 0) {
            scan(vb);
            vb->scan_countdown = VB_SCAN_SAMPLES;
        }
        n = count < vb->scan_countdown ? count : vb->scan_countdown;
        for (i = 0; i < n; i++) {
            int64_t sum = 0;
            int v;

            for (v = 0; v < VB_VOICES; v++)
                sum += vt_tick(&vb->voice[v].vt);
            sum /= 2;
            if (sum > INT32_MAX)
                sum = INT32_MAX;
            if (sum < INT32_MIN)
                sum = INT32_MIN;
            out[i] = (int32_t)sum;
        }
        out += n;
        count -= n;
        vb->scan_countdown -= (unsigned)n;
    }
}

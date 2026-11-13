/*
 * The Verbalizer instrument. A port of VerbalizeIt.spin and the main loop of
 * Verbalizer.spin by Dan Ray.
 *
 * Differences from the original, all on purpose:
 *
 *  - Key states change on key events instead of on a matrix scan. The states
 *    and what each mode does with them every 2 ms are the same, except that
 *    a key released before it was noticed still plays.
 *  - Formants mode feeds voice 0 when its queue has run out, taking held keys
 *    in turn. The original re-queued every held key on every pass and relied
 *    on go() blocking to pace itself.
 *  - Changing mode releases all keys and fades the voices, so a held note
 *    cannot be left droning.
 *  - The stereo spatializer is not ported yet; the voices are mixed to mono.
 *  - Overdriven voices clip instead of wrapping into noise, unless
 *    vb_set_crackle() asks for the original behaviour.
 *  - Silent voices are brought to rest so the output returns to exactly zero.
 *  - Velocity, pitch bend, the sustain pedal and MIDI controllers are new.
 */
#include "verbalizer.h"

/* Key states. The first four are the original's. */
enum {
    SILENCE = 0,
    RELEASE = 1,
    SUSTAIN = 2,
    TRIGGER = 3,
    TAP = 4,       /* pressed and released before a scan saw it */
    TAP_PLAYED = 5 /* a tap that got its voice this scan; released next scan */
};

#define NO_KEY (-1000)

#define KEY_OF(index) ((int)(index) + VB_KEY_MIN)

typedef void (*play_fn)(vb_voice *v, int key);

VB_CODE static bool wants_voice(uint8_t state)
{
    return state == TRIGGER || state == TAP;
}

VB_COLD_CODE static void started(verbalizer *vb, int k)
{
    vb->key_state[k] = (uint8_t)(vb->key_state[k] == TAP ? TAP_PLAYED : SUSTAIN);
}

VB_COLD_CODE static void play(verbalizer *vb, int voice, int k, play_fn fn)
{
    vb->voice[voice].velocity = vb->key_velocity[k];
    fn(&vb->voice[voice], KEY_OF(k));
}

/* A key wants a voice. Returns true if it got one. */
VB_COLD_CODE static bool go_if_available(verbalizer *vb, int k, play_fn fn)
{
    int key = KEY_OF(k);
    int i;

    /* if a voice last played this key, only that voice may take it */
    for (i = 0; i < VB_VOICES; i++) {
        if (vb->voice_key[i] == key) {
            if (!vb_voice_done(&vb->voice[i]))
                return false; /* this key is in use */
            play(vb, i, k, fn);
            return true;
        }
    }
    for (i = 0; i < VB_VOICES; i++) {
        if (vb_voice_done(&vb->voice[i])) {
            vb->voice_key[i] = key;
            play(vb, i, k, fn);
            return true;
        }
    }
    return false; /* all voices busy; the key keeps trying */
}

VB_COLD_CODE static void scan_tsr(verbalizer *vb)
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
        if (wants_voice(vb->key_state[k]) && go_if_available(vb, k, vb_voice_trigger))
            started(vb, k);
    }
}

VB_COLD_CODE static void scan_words(verbalizer *vb)
{
    int k;

    for (k = 0; k < VB_NUM_KEYS; k++) {
        if (wants_voice(vb->key_state[k]) && go_if_available(vb, k, vb_voice_word))
            started(vb, k);
    }
}

VB_COLD_CODE static void scan_formants(verbalizer *vb)
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
        if (wants_voice(vb->key_state[k]) || vb->key_state[k] == SUSTAIN) {
            play(vb, 0, k, vb_voice_formants);
            started(vb, k);
            vb->formants_cursor = k;
            break;
        }
    }
}

VB_COLD_CODE static void scan(verbalizer *vb)
{
    int k, i;

    switch (vb->mode) {
    case VB_MODE_WORDS:    scan_words(vb); break;
    case VB_MODE_TSR:      scan_tsr(vb); break;
    case VB_MODE_FORMANTS: scan_formants(vb); break;
    }
    for (k = 0; k < VB_NUM_KEYS; k++) {
        switch (vb->key_state[k]) {
        case RELEASE:    /* a release is seen for one scan only */
            vb->key_state[k] = SILENCE;
            break;
        case TAP:        /* no voice free yet: wait a little, then give up */
            if (vb->key_tap_wait[k] == 0 || --vb->key_tap_wait[k] == 0)
                vb->key_state[k] = SILENCE;
            break;
        case TAP_PLAYED: /* now let the next scan release it */
            vb->key_state[k] = RELEASE;
            break;
        default:
            break;
        }
    }
    for (i = 0; i < VB_VOICES; i++)
        vt_settle(&vb->voice[i].vt);
}

VB_COLD_CODE void vb_init(verbalizer *vb)
{
    int i;

    vb_fill(vb, 0, sizeof *vb);
    vb->mode = VB_MODE_TSR;
    vb_fill(vb->key_velocity, 127, sizeof vb->key_velocity);
    for (i = 0; i < VB_VOICES; i++) {
        vb_voice_start(&vb->voice[i], vb->pot);
        vt_advance_noise(&vb->voice[i].vt, (unsigned)i * 1000u);
        vt_set_saturate(&vb->voice[i].vt, true);
        vb->voice_key[i] = NO_KEY;
    }
    for (i = 0; i < VB_VOICES; i++)
        vb_voice_null(&vb->voice[i]);
}

VB_COLD_CODE void vb_set_crackle(verbalizer *vb, bool on)
{
    int i;

    for (i = 0; i < VB_VOICES; i++)
        vt_set_saturate(&vb->voice[i].vt, !on);
}

VB_COLD_CODE static void silence_all(verbalizer *vb)
{
    int i;

    vb->formants_fade_pending = 0;
    vb_fill(vb->key_state, SILENCE, sizeof vb->key_state);
    vb_fill(vb->key_pedalled, 0, sizeof vb->key_pedalled);
    for (i = 0; i < VB_VOICES; i++)
        vb_voice_null(&vb->voice[i]);
}

VB_COLD_CODE void vb_set_mode(verbalizer *vb, vb_mode mode)
{
    if (mode == vb->mode)
        return;
    vb->mode = mode;
    silence_all(vb);
}

VB_COLD_CODE bool vb_key_down_velocity(verbalizer *vb, int key, int velocity)
{
    int k = key - VB_KEY_MIN;

    if (key < VB_KEY_MIN || key > VB_KEY_MAX)
        return false;
    if (velocity < 1)
        velocity = 1;
    if (velocity > 127)
        velocity = 127;
    vb->key_pedalled[k] = 0;
    if (vb->key_state[k] != SUSTAIN) {
        vb->key_state[k] = TRIGGER;
        vb->key_velocity[k] = (uint8_t)velocity;
    }
    return true;
}

VB_COLD_CODE bool vb_key_down(verbalizer *vb, int key)
{
    return vb_key_down_velocity(vb, key, 127);
}

VB_COLD_CODE static void release_key(verbalizer *vb, int k)
{
    uint8_t *state = &vb->key_state[k];

    vb->key_pedalled[k] = 0;
    switch (*state) {
    case SUSTAIN: *state = RELEASE; break;
    case TRIGGER:
        *state = TAP;
        vb->key_tap_wait[k] = VB_TAP_WAIT_SCANS;
        break;
    default:      break; /* already on its way out */
    }
}

VB_COLD_CODE bool vb_key_up(verbalizer *vb, int key)
{
    int k = key - VB_KEY_MIN;

    if (key < VB_KEY_MIN || key > VB_KEY_MAX)
        return false;
    if (vb->pedal) {
        if (vb->key_state[k] == SUSTAIN || vb->key_state[k] == TRIGGER)
            vb->key_pedalled[k] = 1;
    } else {
        release_key(vb, k);
    }
    return true;
}

VB_COLD_CODE bool vb_note_on(verbalizer *vb, int note)
{
    return vb_key_down(vb, note - VB_MIDI_OFFSET);
}

VB_COLD_CODE bool vb_note_on_velocity(verbalizer *vb, int note, int velocity)
{
    return vb_key_down_velocity(vb, note - VB_MIDI_OFFSET, velocity);
}

VB_COLD_CODE bool vb_note_off(verbalizer *vb, int note)
{
    return vb_key_up(vb, note - VB_MIDI_OFFSET);
}

VB_COLD_CODE void vb_set_pedal(verbalizer *vb, bool down)
{
    int k;

    vb->pedal = down;
    if (down)
        return;
    for (k = 0; k < VB_NUM_KEYS; k++) {
        if (vb->key_pedalled[k])
            release_key(vb, k);
    }
}

VB_COLD_CODE void vb_set_pitch_bend(verbalizer *vb, int bend)
{
    int i;

    if (bend < -8192)
        bend = -8192;
    if (bend > 8191)
        bend = 8191;
    /* 4 gp units to the semitone */
    bend = bend * (VB_BEND_SEMITONES * 4) / 8192;
    for (i = 0; i < VB_VOICES; i++)
        vb->voice[i].bend = (int8_t)bend;
}

VB_COLD_CODE bool vb_midi(verbalizer *vb, uint8_t status, uint8_t data1, uint8_t data2)
{
    data1 &= 0x7F;
    data2 &= 0x7F;

    switch (status & 0xF0) {
    case 0x80:
        return vb_note_off(vb, data1);
    case 0x90:
        if (data2 == 0)
            return vb_note_off(vb, data1);
        return vb_note_on_velocity(vb, data1, data2);
    case 0xE0:
        vb_set_pitch_bend(vb, ((data2 << 7) | data1) - 8192);
        return true;
    case 0xB0:
        if (data1 >= VB_CC_KNOB_FIRST && data1 < VB_CC_KNOB_FIRST + VB_NUM_POTS) {
            /* spread 0..127 over 0..255 */
            vb->pot[data1 - VB_CC_KNOB_FIRST] = (uint8_t)((data2 << 1) | (data2 >> 6));
            return true;
        }
        switch (data1) {
        case VB_CC_PEDAL:
            vb_set_pedal(vb, data2 >= 64);
            return true;
        case VB_CC_MODE:
            vb_set_mode(vb, data2 < 43 ? VB_MODE_WORDS : data2 < 86 ? VB_MODE_TSR : VB_MODE_FORMANTS);
            return true;
        case VB_CC_CRACKLE:
            vb_set_crackle(vb, data2 >= 64);
            return true;
        case VB_CC_ALL_SOUND_OFF:
            silence_all(vb);
            return true;
        case VB_CC_ALL_NOTES_OFF:
            vb_all_keys_up(vb);
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}

VB_COLD_CODE void vb_all_keys_up(verbalizer *vb)
{
    int k;

    for (k = 0; k < VB_NUM_KEYS; k++)
        release_key(vb, k);
}

VB_COLD_CODE bool vb_idle(const verbalizer *vb)
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

/*
 * Mix limiter. Sums up to half of full scale pass through untouched. Beyond
 * that the excess e is replaced by h * e / (e + h), h being the headroom left
 * (also half of full scale), which meets the straight part with the same
 * slope and approaches full scale without reaching it. Four full-scale
 * voices come out at 94%.
 *
 * It is computed as h - h * (h / (e + h)) with the ratio in 16 fractional
 * bits, which needs only a 32-bit division.
 */
VB_CODE static int32_t soft_clip(int64_t sum)
{
    const int64_t knee = (int64_t)1 << 30;
    uint32_t ratio, mag;
    int64_t total;

    if (sum >= -knee && sum <= knee)
        return (int32_t)sum;
    total = sum < 0 ? -sum : sum;                        /* e + h, up to 2^33 */
    ratio = ((uint32_t)1 << 30) / (uint32_t)(total >> 16); /* h / (e + h), Q16 */
    mag = ((uint32_t)1 << 31) - ((uint32_t)1 << 14) * ratio;
    if (mag > (uint32_t)INT32_MAX)
        mag = (uint32_t)INT32_MAX;
    return sum < 0 ? -(int32_t)mag : (int32_t)mag;
}

VB_CODE void vb_render(verbalizer *vb, int32_t *out, size_t count)
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
            out[i] = soft_clip(sum);
        }
        out += n;
        count -= n;
        vb->scan_countdown -= (unsigned)n;
    }
}

/* 20 kHz to 44.1 kHz sample rate converter and FM-1 output stage. */
#include "resampler.h"

VB_COLD_CODE void rs_init(resampler *rs)
{
    vb_fill(rs, 0, sizeof *rs);
}

VB_CODE size_t rs_input_needed(const resampler *rs, size_t out_count)
{
    return (rs->position + RS_DOWN * out_count) / RS_UP;
}

VB_CODE static int32_t clip32(int64_t v)
{
    if (v > INT32_MAX)
        return INT32_MAX;
    if (v < INT32_MIN)
        return INT32_MIN;
    return (int32_t)v;
}

VB_CODE void rs_process(resampler *rs, const int32_t *in, int32_t *out, size_t out_count)
{
    size_t n;

    for (n = 0; n < out_count; n++) {
        /* split the position into a table row and the way to the next row,
         * the latter in 512ths so that no 64-bit division is needed */
        unsigned scaled = rs->position * RS_PHASES;
        unsigned row = scaled / RS_UP;
        int64_t blend = (scaled % RS_UP) * 512u / RS_UP;
        const int16_t *c0 = rs_table[row];
        const int16_t *c1 = rs_table[row + 1];
        int64_t y0 = 0, y1 = 0;
        int t;

        for (t = 0; t < RS_TAPS; t++) {
            y0 += (int64_t)rs->history[t] * c0[t];
            y1 += (int64_t)rs->history[t] * c1[t];
        }
        y0 += ((y1 - y0) * blend) >> 9;
        out[n] = clip32((y0 + (1 << 14)) >> 15);

        rs->position += RS_DOWN;
        if (rs->position >= RS_UP) {
            rs->position -= RS_UP;
            vb_copy(rs->history, rs->history + 1, (RS_TAPS - 1) * sizeof rs->history[0]);
            rs->history[RS_TAPS - 1] = *in++;
        }
    }
}

VB_COLD_CODE void vb_output_init(vb_output *o, verbalizer *vb)
{
    o->vb = vb;
    rs_init(&o->rs);
}

VB_CODE void vb_output_render(vb_output *o, int32_t *pcm, size_t frames)
{
    /* 128 output frames need at most 59 input samples */
    int32_t in[(VB_OUTPUT_MAX_FRAMES * RS_DOWN + RS_UP - 1) / RS_UP + 1];
    int32_t mono[VB_OUTPUT_MAX_FRAMES];
    size_t i;

    if (frames > VB_OUTPUT_MAX_FRAMES)
        frames = VB_OUTPUT_MAX_FRAMES;
    vb_render(o->vb, in, rs_input_needed(&o->rs, frames));
    rs_process(&o->rs, in, mono, frames);
    for (i = 0; i < frames; i++) {
        int32_t q23 = mono[i] >> 8;

        pcm[2 * i] = q23;
        pcm[2 * i + 1] = q23;
    }
}

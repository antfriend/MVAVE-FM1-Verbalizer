/*
 * vt_script: play a .vts tract script through the C vocal tract.
 *
 *   vt_script script.vts [out.wav]
 *
 * Prints "<samples> <fnv1a64>", the same line tools/pasm_ref.py prints for the
 * original Propeller assembly, so the two can be compared directly.
 */
#include <inttypes.h>
#include <stdio.h>

#include "hostlib.h"
#include "vocal_tract.h"

int main(int argc, char **argv)
{
    samples s = { 0 };
    char err[256];

    if (argc < 2 || argc > 3) {
        fprintf(stderr, "usage: vt_script script.vts [out.wav]\n");
        return 2;
    }
    if (!vts_play(argv[1], &s, err, sizeof err)) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    printf("%zu %016" PRIx64 "\n", s.count, samples_hash(&s));
    if (argc == 3 && !wav_write(argv[2], &s, VT_SAMPLE_RATE)) {
        fprintf(stderr, "%s: cannot write\n", argv[2]);
        return 1;
    }
    samples_free(&s);
    return 0;
}

/* Desktop helpers shared by the host tools and the tests. */
#ifndef HOSTLIB_H
#define HOSTLIB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A growable buffer of samples. Zero-initialise, free with samples_free(). */
typedef struct {
    int32_t *data;
    size_t   count;
    size_t   capacity;
} samples;

/* Make room for `extra` more samples and return where to write them, or NULL
 * if out of memory. Call samples_commit() with how many were written. */
int32_t *samples_reserve(samples *s, size_t extra);
void     samples_commit(samples *s, size_t written);
void     samples_free(samples *s);

/* FNV-1a 64 over the samples as little-endian 32-bit words. */
uint64_t samples_hash(const samples *s);

/* Write 16-bit mono PCM, taking the top 16 bits of each sample. */
bool wav_write(const char *path, const samples *s, unsigned sample_rate);

/*
 * Play a .vts tract script (format described in tools/pasm_ref.py) through one
 * vocal tract and append the samples to `out`. On failure returns false with
 * a message in `err`.
 */
bool vts_play(const char *path, samples *out, char *err, size_t err_size);

#endif

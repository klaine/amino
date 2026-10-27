/* Small realtime-safe helpers shared by the DSP sources. */
#ifndef AMINO_UTIL_H
#define AMINO_UTIL_H

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define AMINO_TAU (2.0f * (float)M_PI)

static inline float am_clampf(float x, float lo, float hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline int am_clampi(int x, int lo, int hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

/* Fast tanh: rational (Pade) approximation, within ~0.025 of tanh.  The
 * curve peaks at exactly +-1 at |x| = 3, so clamping there keeps it
 * continuous and bounded.  Keeps the four-stage filter affordable at 16
 * voices. */
static inline float am_tanh(float x) {
    if (x < -3.0f) return -1.0f;
    if (x > 3.0f) return 1.0f;
    float x2 = x * x;
    float n = x * (27.0f + x2);
    float d = 27.0f + 9.0f * x2;
    return n / d;
}

static inline float am_note_hz(float note) {
    return 440.0f * exp2f((note - 69.0f) / 12.0f);
}

static inline uint32_t am_rand_u32(uint32_t *s) {
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x ? x : 0x1234567u;
    return *s;
}

static inline float am_rand_bipolar(uint32_t *s) {
    return (float)(am_rand_u32(s) & 0xFFFFFFu) * (2.0f / 16777215.0f) - 1.0f;
}

static inline float am_rand_unit(uint32_t *s) {
    return (float)(am_rand_u32(s) & 0xFFFFFFu) * (1.0f / 16777215.0f);
}

/* docs/03 section 8: 0..20 s, curved. */
static inline float am_env_time(float x) {
    if (x < 0.0f) x = 0.0f;
    float t = 20.0f * x * x * x;
    return t < 0.0005f ? 0.0005f : t;
}

/* docs/03 section 9: 0.01..50 Hz. */
static inline float am_lfo_hz(float x) {
    if (x < 0.0f) x = 0.0f;
    if (x > 1.2f) x = 1.2f;
    return 0.01f * powf(5000.0f, x);
}

/* Mod-matrix Via blends from an unscaled route (depth 0) to the legacy
 * full-strength source multiplier (depth 1). */
static inline float am_via_multiplier(float signal, float depth) {
    depth = am_clampf(depth, 0.0f, 1.0f);
    return 1.0f - depth + depth * signal;
}

/* docs/03 section 4.5: 0..5 s. */
static inline float am_glide_time(float x) {
    return 5.0f * x * x;
}

static inline float am_one_pole(float hz, float sr) {
    float f = hz;
    if (f > sr * 0.45f) f = sr * 0.45f;
    return 1.0f - expf(-AMINO_TAU * f / sr);
}

/* Wrap a wavetable phase into [0, 128).  For a tiny negative q the floor
 * form rounds to exactly 128, one sample past the table; fold that to 0. */
static inline float am_wrap128(float q) {
    q -= 128.0f * floorf(q * (1.0f / 128.0f));
    if (q < 0.0f) q += 128.0f;
    if (q >= 128.0f) q -= 128.0f;
    return q;
}

/* Equal-power pan gains. */
static inline void am_pan_gains(float pan, float *l, float *r) {
    float a = (am_clampf(pan, -1.0f, 1.0f) + 1.0f) * (float)M_PI * 0.25f;
    *l = cosf(a);
    *r = sinf(a);
}

#endif

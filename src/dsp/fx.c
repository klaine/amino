/* Amino effects: two slots, twelve algorithms, one preallocated arena per slot.
 *
 * The DSP mirrors tools/emulator/fx.py.  No allocation happens at runtime: a
 * slot bump-allocates its delay lines out of a fixed 820000-byte arena and
 * clears that region incrementally (64 KB per block) after a type change.
 */
#include "amino.h"
#include "util.h"

#include <math.h>
#include <string.h>

#define FX_CLEAR_CHUNK 65536
#define FX_FADE_SECONDS 0.020f
/* Slow modulation (chorus taps, phaser sweep, reverb line wobble) is
 * evaluated at 16-sample points and interpolated linearly in between. */
#define FX_MOD_CHUNK 16

/* ------------------------------------------------------------------ helpers */

static float lin(float x, float lo, float hi) {
    return lo + (hi - lo) * am_clampf(x, 0.0f, 1.0f);
}

static float exps(float x, float lo, float hi) {
    return lo * powf(hi / lo, am_clampf(x, 0.0f, 1.0f));
}

static int enumv(float x, int n) {
    int i = (int)(am_clampf(x, 0.0f, 1.0f) * (float)(n - 1) + 0.5f);
    return am_clampi(i, 0, n - 1);
}

static const int REVERB_BASE[8] = { 1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116 };

static float div_seconds(int idx, float bpm) {
    /* Off, 1/32, 1/16T, 1/16, 1/16d, 1/8T, 1/8, 1/8d, 1/4T, 1/4, 1/4d, 1/2, 1 bar */
    static const float beats[13] = {
        0.0f, 0.125f, 0.1666667f, 0.25f, 0.375f, 0.3333333f, 0.5f,
        0.75f, 0.6666667f, 1.0f, 1.5f, 2.0f, 4.0f,
    };
    if (idx < 1) idx = 1;
    if (idx > 12) idx = 12;
    if (bpm < 20.0f) bpm = 120.0f;
    return beats[idx] * 60.0f / bpm;
}

static float phase_wrap(float p) {
    p -= floorf(p);
    return p;
}

/* Shortest signed distance from `from` to `to`, in cycles. */
static float phase_error(float from, float to) {
    float d = from - to;
    d -= floorf(d + 0.5f);
    return d;
}

static float flanger_beats(int idx) {
    /* Off, 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars, 4 bars */
    static const float beats[8] = { 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f };
    if (idx < 1) idx = 1;
    if (idx > 7) idx = 7;
    return beats[idx];
}

static float flanger_seconds(int idx, float bpm) {
    if (bpm < 20.0f) bpm = 120.0f;
    return flanger_beats(idx) * 60.0f / bpm;
}

static float trem_beats(int idx) {
    /* Off, 1/32, 1/16T, 1/16, 1/8T, 1/8, 1/4T, 1/4, 1/2, 1 bar */
    static const float beats[10] = {
        0.0f, 0.125f, 0.1666667f, 0.25f, 0.3333333f, 0.5f,
        0.6666667f, 1.0f, 2.0f, 4.0f,
    };
    if (idx < 1) idx = 1;
    if (idx > 9) idx = 9;
    return beats[idx];
}

static float trem_seconds(int idx, float bpm) {
    if (bpm < 20.0f) bpm = 120.0f;
    return trem_beats(idx) * 60.0f / bpm;
}

static float one_pole_coef(float fc, float sr) {
    if (fc < 1.0f) fc = 1.0f;
    if (fc > sr * 0.45f) fc = sr * 0.45f;
    return 1.0f - expf(-AMINO_TAU * fc / sr);
}

/* ------------------------------------------------------------------ arena */

static void *fx_mem(AminoFxSlot *s, int bytes) {
    uint32_t pos = (s->arena_pos + 15u) & ~15u;
    if (pos + (uint32_t)bytes > AMINO_FX_ARENA) return 0;
    s->arena_pos = pos + (uint32_t)bytes;
    return s->arena + pos;
}

static int dl_init(AminoFxSlot *s, AminoDelay *d, float seconds, float sr) {
    int n = (int)(seconds * sr) + 4;
    if (n < 8) n = 8;
    float *buf = (float *)fx_mem(s, n * (int)sizeof(float));
    if (!buf) return 0;
    d->buf = buf;
    d->size = n;
    d->w = 0;
    return 1;
}

static void dl_write(AminoDelay *d, float x) {
    d->buf[d->w] = x;
    d->w++;
    if (d->w >= d->size) d->w = 0;
}

static float dl_read(const AminoDelay *d, float delay) {
    float pos = (float)d->w - 1.0f - delay;
    int size = d->size;
    /* Every caller keeps delay below the line size, so one wrap suffices;
     * the loop only guards a pathological value. */
    if (pos < 0.0f) {
        pos += (float)size;
        while (pos < 0.0f) pos += (float)size;
    }
    int i = (int)pos;
    float f = pos - (float)i;
    if (i >= size) i -= size;
    const float *b = d->buf;
    float x0 = b[i];
    int i1 = i + 1 < size ? i + 1 : 0;
    float x1 = b[i1];
    if (f == 0.0f) return x0;
    if (delay < 2.0f) return x0 + (x1 - x0) * f;
    int im = i - 1;
    if (im < 0) im += size;
    int i2 = i1 + 1 < size ? i1 + 1 : 0;
    float xm1 = b[im], x2 = b[i2];
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

/* ------------------------------------------------------------------ biquad */

static void bq_set(AminoBiquad *b, int kind, float f, float gain_db, float q,
                   float sr) {
    float A = powf(10.0f, gain_db / 40.0f);
    float w = AMINO_TAU * f / sr;
    float cw = cosf(w), sw = sinf(w);
    float al = sw / (2.0f * q);
    float b0, b1, b2, a0, a1, a2;
    if (kind == 0) {                    /* peak */
        b0 = 1.0f + al * A; b1 = -2.0f * cw; b2 = 1.0f - al * A;
        a0 = 1.0f + al / A; a1 = -2.0f * cw; a2 = 1.0f - al / A;
    } else if (kind == 1) {             /* low shelf */
        float s = 2.0f * sqrtf(A) * al;
        b0 = A * ((A + 1.0f) - (A - 1.0f) * cw + s);
        b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw);
        b2 = A * ((A + 1.0f) - (A - 1.0f) * cw - s);
        a0 = (A + 1.0f) + (A - 1.0f) * cw + s;
        a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw);
        a2 = (A + 1.0f) + (A - 1.0f) * cw - s;
    } else if (kind == 2) {             /* high shelf */
        float s = 2.0f * sqrtf(A) * al;
        b0 = A * ((A + 1.0f) + (A - 1.0f) * cw + s);
        b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw);
        b2 = A * ((A + 1.0f) + (A - 1.0f) * cw - s);
        a0 = (A + 1.0f) - (A - 1.0f) * cw + s;
        a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw);
        a2 = (A + 1.0f) - (A - 1.0f) * cw - s;
    } else {                            /* high pass */
        b0 = (1.0f + cw) * 0.5f; b1 = -(1.0f + cw); b2 = (1.0f + cw) * 0.5f;
        a0 = 1.0f + al; a1 = -2.0f * cw; a2 = 1.0f - al;
    }
    float inv = 1.0f / a0;
    b->b0 = b0 * inv; b->b1 = b1 * inv; b->b2 = b2 * inv;
    b->a1 = a1 * inv; b->a2 = a2 * inv;
}

static float bq_run(AminoBiquad *b, float x) {
    float y = b->b0 * x + b->z1;
    b->z1 = b->b1 * x - b->a1 * y + b->z2;
    b->z2 = b->b2 * x - b->a2 * y;
    return y;
}

/* ------------------------------------------------------------------ shapers */

#define FX_LN2 0.6931471805599453f

static float logcosh(float x) {
    float ax = fabsf(x);
    return ax + log1pf(expf(-2.0f * ax)) - FX_LN2;
}

/* ------------------------------------------------------------------ defaults */

/* type -> six knob defaults (see fx.py to_knob).  index 0 is Off. */
static const float FX_DEFAULTS[12][6] = {
    { 0, 0, 0, 0, 0, 0 },                                        /* Off */
    { 0.5396f, 0.4000f, 0.2143f, 0.5000f, 0.5000f, 0.0000f },    /* Chorus */
    { 0.4064f, 0.6000f, 0.7632f, 0.5880f, 0.5000f, 0.0000f },    /* Flanger */
    { 0.4357f, 0.6000f, 0.7105f, 0.3333f, 0.5275f, 0.5000f },    /* Phaser */
    { 0.6962f, 0.5000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f },    /* Tremolo */
    { 0.3333f, 0.5000f, 0.0000f, 0.7500f, 0.0000f, 0.3000f },    /* Drive */
    { 0.5500f, 0.4628f, 0.6667f, 0.5880f, 0.2500f, 0.5000f },    /* Comp */
    { 0.5000f, 0.5000f, 0.4363f, 0.5000f, 0.4241f, 0.0000f },    /* EQ */
    { 0.7707f, 0.5833f, 0.4000f, 0.4000f, 0.0000f, 0.1000f },    /* Delay */
    { 0.6000f, 0.5485f, 0.5000f, 0.1000f, 0.3000f, 1.0000f },    /* Reverb */
    { 0.7000f, 0.7385f, 0.4000f, 0.4000f, 0.6000f, 0.4000f },    /* Shimmer */
    { 0.0000f, 0.5000f, 0.0400f, 0.1600f, 0.6000f, 0.0000f },    /* Trance Gate */
};

const float *amino_fx_defaults(int type) {
    if (type < 0 || type > 11) return FX_DEFAULTS[0];
    return FX_DEFAULTS[type];
}

/* ------------------------------------------------------------------ init */

static int fx_alloc(AminoInstance *in, AminoFxSlot *s, int type) {
    float sr = (float)in->sr;
    memset(&s->st, 0, sizeof s->st);
    s->arena_pos = 0;
    s->need_clear = 0;
    s->clear_pos = 0;
    s->clear_len = 0;
    s->fade = 0.0f;
    s->wet_last = 0.0f;
    s->fading_out = 0;
    s->type = type;
    s->pending_type = -1;
    {
        int si = (int)(s - in->fx);
        float *base = si == 0 ? &in->g.fx1_p1 : &in->g.fx2_p1;
        for (int i = 0; i < 6; i++) {
            s->p[i] = base[i];
            s->sp[i] = base[i];
        }
        s->smix = am_clampf(si == 0 ? in->g.fx1_mix : in->g.fx2_mix, 0.0f, 1.0f);
        s->mix = s->smix;
    }

    int ok = 1;
    switch (type) {
    case 1:                            /* Chorus */
        ok &= dl_init(s, &s->st.d[0], 0.08f, sr);
        ok &= dl_init(s, &s->st.d[1], 0.08f, sr);
        s->st.phase = 0.0f;
        s->st.voices = s->p[5] >= 0.5f ? 3 : 2;
        if (!ok) return 0;
        break;
    case 2:                            /* Flanger */
        ok &= dl_init(s, &s->st.d[0], 0.03f, sr);
        ok &= dl_init(s, &s->st.d[1], 0.03f, sr);
        if (!ok) return 0;
        break;
    case 3:                            /* Phaser */
        s->st.pn = 12;
        break;
    case 4:                            /* Tremolo */
        s->st.tg[0] = s->st.tg[1] = s->st.tg[2] = s->st.tg[3] = 1.0f;
        break;
    case 5:                            /* Drive */
        break;
    case 6:                            /* Comp */
        s->st.cenv = -120.0f;
        break;
    case 7:                            /* EQ */
        break;
    case 8: {                          /* Delay */
        ok &= dl_init(s, &s->st.d[0], 2.1f, sr);
        ok &= dl_init(s, &s->st.d[1], 2.1f, sr);
        if (!ok) return 0;
        s->st.dt = -1.0f;
        s->st.dstart = -1.0f;
        s->st.dfade = 1.0f;
        break;
    }
    case 9:                            /* Reverb */
    case 10: {                         /* Shimmer */
        static const int DIFF[4] = { 142, 107, 379, 277 };
        static const int DIFF_R[4] = { 151, 113, 389, 293 };
        float size = lin(s->p[0], 0.0f, 1.0f);
        float scale = 0.6f + 1.4f * size;
        for (int k = 0; k < 8; k++) {
            int len = (int)((float)REVERB_BASE[k] * scale);
            if (len < 16) len = 16;
            s->st.len[k] = len;
            /* Reserve the maximum line size so Size can change while active. */
            int max_len = REVERB_BASE[k] * 2;
            ok &= dl_init(s, &s->st.lines[k], (float)max_len / sr + 0.01f, sr);
            if (!ok) return 0;
        }
        for (int c = 0; c < 2; c++) {
            for (int k = 0; k < 4; k++) {
                int d = c == 0 ? DIFF[k] : DIFF_R[k];
                ok &= dl_init(s, &s->st.diff[c][k], (float)d / sr + 0.001f, sr);
                if (!ok) return 0;
            }
            ok &= dl_init(s, &s->st.pre[c], 0.21f, sr);
            if (!ok) return 0;
        }
        if (type == 10)
            ok &= dl_init(s, &s->st.sh, 0.1f, sr);
        if (!ok) return 0;
        s->st.dc = one_pole_coef(500.0f, sr);
        for (int k = 0; k < 8; k++) s->st.mph[k] = (float)k / 8.0f;
        s->st.sphase = 0.0f;
        s->st.slp = s->st.shp = 0.0f;
        s->st.samt = 0.0f;
        break;
    }
    case 11:                            /* Trance Gate: no arena required */
        s->st.sync_locked = 0;
        break;
    default:
        break;
    }

    s->clear_len = (s->arena_pos + 63u) & ~63u;
    if (s->clear_len > AMINO_FX_ARENA) s->clear_len = AMINO_FX_ARENA;
    s->clear_pos = 0;
    s->need_clear = s->clear_len > 0;
    return 1;
}

void amino_fx_init(AminoInstance *in, int slot) {
    AminoFxSlot *s = &in->fx[slot];
    memset(s, 0, sizeof *s);
    s->arena = in->fx_arena[slot];
    s->type = 0;
    s->pending_type = -1;
    s->smix = 0.0f;
    for (int i = 0; i < 6; i++) s->sp[i] = 0.0f;
}

void amino_fx_release(AminoInstance *in, int slot) {
    (void)in;
    (void)slot;
}

void amino_fx_set_type(AminoInstance *in, int slot, int type) {
    if (slot < 0 || slot > 1) return;
    if (type < 0) type = 0;
    if (type > 11) type = 11;
    AminoFxSlot *s = &in->fx[slot];
    if (type == s->type && !s->fading_out) return;
    if (s->fading_out) {
        s->pending_type = type;
        return;
    }
    if (s->type == 0) {
        if (type == 0) return;
        /* Fresh selection from Off with every knob still at minimum: use the
         * musical defaults documented for this type (docs/11 section 4). */
        float *base = slot == 0 ? &in->g.fx1_p1 : &in->g.fx2_p1;
        int fresh = 1;
        for (int i = 0; i < 6; i++)
            if (base[i] > 0.0001f) fresh = 0;
        if (fresh)
            for (int i = 0; i < 6; i++) base[i] = FX_DEFAULTS[type][i];
        if (!fx_alloc(in, s, type)) { s->type = 0; s->pending_type = -1; }
        return;
    }
    if (type == 0) {
        s->pending_type = 0;
        s->fading_out = 1;
        s->fade = 1.0f;
        return;
    }
    s->pending_type = type;
    s->fading_out = 1;
    s->fade = 1.0f;
}

/* ------------------------------------------------------------------ effects */

static void fx_chorus(AminoFxSlot *s, float *l, float *r, int n, float sr) {
    AminoFxState *st = &s->st;
    float rate = exps(s->sp[0], 0.05f, 5.0f);
    float depth = s->sp[1];
    float base = lin(s->sp[2], 2.0f, 30.0f) * 0.001f * sr;
    float fb = lin(s->sp[3], -0.9f, 0.9f);
    float spread = lin(s->sp[4], 0.0f, 180.0f) * (float)M_PI / 180.0f;
    int nv = s->sp[5] >= 0.5f ? 3 : 2;
    float inc = rate / sr;
    float inv_nv = 1.0f / (float)nv;
    for (int i0 = 0; i0 < n; i0 += FX_MOD_CHUNK) {
        int m = n - i0;
        if (m > FX_MOD_CHUNK) m = FX_MOD_CHUNK;
        float ph0 = st->phase * AMINO_TAU;
        float ph1 = (st->phase + inc * (float)m) * AMINO_TAU;
        float tap[2][3], step[2][3];
        for (int c = 0; c < 2; c++) {
            for (int k = 0; k < nv; k++) {
                float o = (float)k * AMINO_TAU / (float)nv + (float)c * spread;
                float a = base * (1.0f + 0.8f * depth * sinf(ph0 + o));
                float b = base * (1.0f + 0.8f * depth * sinf(ph1 + o));
                tap[c][k] = a;
                step[c][k] = (b - a) / (float)m;
            }
        }
        for (int j = 0; j < m; j++) {
            int i = i0 + j;
            float out[2];
            for (int c = 0; c < 2; c++) {
                float acc = 0.0f;
                for (int k = 0; k < nv; k++)
                    acc += dl_read(&st->d[c], tap[c][k] + step[c][k] * (float)j);
                out[c] = acc * inv_nv;
            }
            dl_write(&st->d[0], l[i] + fb * out[0]);
            dl_write(&st->d[1], r[i] + fb * out[1]);
            l[i] = out[0];
            r[i] = out[1];
            st->phase += inc;
            if (st->phase >= 1.0f) st->phase -= floorf(st->phase);
        }
    }
}

static void fx_flanger(AminoFxSlot *s, float *l, float *r, int n, float sr,
                       float bpm, double beat0, int beat_locked) {
    AminoFxState *st = &s->st;
    int sync = enumv(s->sp[5], 8);
    float rate = sync == 0 ? exps(s->sp[0], 0.02f, 10.0f)
                           : 1.0f / flanger_seconds(sync, bpm);
    float depth = s->sp[1];
    float fb = lin(s->sp[2], -0.95f, 0.95f);
    float base = exps(s->sp[3], 0.1f, 10.0f) * 0.001f * sr;
    float spread = lin(s->sp[4], 0.0f, 180.0f) * (float)M_PI / 180.0f;
    float inc = rate / sr;
    double beat_phase = 0.0;
    double beat_inc = 0.0;
    int locked = sync > 0 && beat_locked;
    if (locked) {
        double beats = (double)flanger_beats(sync);
        beat_phase = fmod(beat0 / beats, 1.0);
        beat_inc = (double)bpm / (60.0 * (double)sr * beats);
        if (!st->sync_locked) {
            st->sync_offset = phase_error(st->phase, (float)beat_phase);
            st->sync_locked = 1;
        }
    } else {
        st->sync_locked = 0;
    }
    float offset = st->sync_offset;
    float offset_decay = expf(-1.0f / (0.050f * sr));
    for (int i = 0; i < n; i++) {
        float phase = st->phase;
        if (locked) {
            phase = phase_wrap((float)beat_phase + offset);
            beat_phase += beat_inc;
            if (beat_phase >= 1.0) beat_phase -= 1.0;
            offset *= offset_decay;
        }
        float ph = phase * AMINO_TAU;
        float yl = dl_read(&st->d[0], 1.0f + base * (1.0f + 0.95f * depth * sinf(ph)));
        float yr = dl_read(&st->d[1], 1.0f + base * (1.0f + 0.95f * depth * sinf(ph + spread)));
        dl_write(&st->d[0], l[i] + fb * yl);
        dl_write(&st->d[1], r[i] + fb * yr);
        l[i] = yl;
        r[i] = yr;
        st->phase = locked ? phase_wrap((float)beat_phase + offset)
                           : phase_wrap(phase + inc);
    }
    st->sync_offset = offset;
}

static void fx_phaser(AminoFxSlot *s, float *l, float *r, int n, float sr) {
    AminoFxState *st = &s->st;
    static const int STAGES[4] = { 4, 6, 8, 12 };
    int stages = STAGES[enumv(s->sp[3], 4)];
    float rate = exps(s->sp[0], 0.02f, 10.0f);
    float depth = s->sp[1];
    float fb = lin(s->sp[2], -0.95f, 0.95f);
    float center = exps(s->sp[4], 100.0f, 4000.0f);
    float spread = lin(s->sp[5], 0.0f, 180.0f) * (float)M_PI / 180.0f;
    float inc = rate / sr;
    float *bufs[2] = { l, r };
    for (int i0 = 0; i0 < n; i0 += FX_MOD_CHUNK) {
        int m = n - i0;
        if (m > FX_MOD_CHUNK) m = FX_MOD_CHUNK;
        float ph[2] = { st->phase * AMINO_TAU,
                        (st->phase + inc * (float)m) * AMINO_TAU };
        float a0[2], da[2];
        for (int c = 0; c < 2; c++) {
            float coef[2];
            for (int e = 0; e < 2; e++) {
                float f = center * exp2f(2.0f * depth * sinf(ph[e] + (float)c * spread));
                if (f > 18000.0f) f = 18000.0f;
                if (f < 20.0f) f = 20.0f;
                float t = tanf((float)M_PI * f / sr);
                coef[e] = (t - 1.0f) / (t + 1.0f);
            }
            a0[c] = coef[0];
            da[c] = (coef[1] - coef[0]) / (float)m;
        }
        for (int j = 0; j < m; j++) {
            int i = i0 + j;
            for (int c = 0; c < 2; c++) {
                float a = a0[c] + da[c] * (float)j;
                float x = bufs[c][i] + fb * st->plast[c];
                float *z = st->pz[c];
                for (int k = 0; k < stages; k++) {
                    float y = a * x + z[k];
                    z[k] = x - a * y;
                    x = y;
                }
                st->plast[c] = x;
                bufs[c][i] = x;
            }
            st->phase += inc;
            if (st->phase >= 1.0f) st->phase -= floorf(st->phase);
        }
    }
}

static void fx_tremolo(AminoFxSlot *s, float *l, float *r, int n, float sr,
                       float bpm, double beat0, int beat_locked) {
    AminoFxState *st = &s->st;
    float depth = s->sp[1];
    int shape = enumv(s->sp[2], 6);
    int sync = enumv(s->sp[3], 10);
    float stereo = s->sp[4];
    float phase_offset = s->sp[5];
    float period;
    if (sync == 0) period = 1.0f / exps(s->sp[0], 0.1f, 20.0f);
    else period = trem_seconds(sync, bpm);
    if (shape == 5 && sync == 0) period = trem_seconds(7, bpm);   /* Pump = 1/4 */
    float inc = 1.0f / (period * sr);
    float k = one_pole_coef(1.0f / (AMINO_TAU * 0.001f), sr);
    double beat_phase = 0.0;
    double beat_inc = 0.0;
    int locked = sync > 0 && beat_locked;
    if (locked) {
        double beats = (double)trem_beats(sync);
        beat_phase = fmod(beat0 / beats, 1.0);
        beat_inc = (double)bpm / (60.0 * (double)sr * beats);
        if (!st->sync_locked) {
            st->sync_offset = phase_error(st->phase, (float)beat_phase);
            st->sync_locked = 1;
        }
    } else {
        st->sync_locked = 0;
    }
    float sync_offset = st->sync_offset;
    float offset_decay = expf(-1.0f / (0.050f * sr));
    for (int i = 0; i < n; i++) {
        float phase = st->phase;
        if (locked) {
            phase = phase_wrap((float)beat_phase + sync_offset);
            beat_phase += beat_inc;
            if (beat_phase >= 1.0) beat_phase -= 1.0;
            sync_offset *= offset_decay;
        }
        for (int c = 0; c < 2; c++) {
            float p = phase;
            p += phase_offset + (float)c * 0.5f * stereo;
            p -= floorf(p);
            float g;
            if (shape == 5) {
                g = 1.0f - depth * expf(-p / 0.18f);
            } else {
                float u;
                if (shape == 0) u = 0.5f - 0.5f * cosf(AMINO_TAU * p);
                else if (shape == 1) u = 1.0f - fabsf(2.0f * p - 1.0f);
                else if (shape == 2) u = p < 0.5f ? 1.0f : 0.0f;
                else if (shape == 3) u = p;
                else u = 1.0f - p;
                g = 1.0f - depth * (1.0f - u);
            }
            st->tg[c] += (g - st->tg[c]) * k;
            st->tg[c + 2] += (st->tg[c] - st->tg[c + 2]) * k;
        }
        st->phase = locked ? phase_wrap((float)beat_phase + sync_offset)
                           : phase_wrap(phase + inc);
        l[i] *= st->tg[2];
        r[i] *= st->tg[3];
    }
    st->sync_offset = sync_offset;
}

static void fx_trance_gate(AminoFxSlot *s, float *l, float *r, int n, float sr,
                           float bpm, double beat0, int beat_locked) {
    static const uint16_t PATTERNS[7] = {
        0x3333u, /* Classic 8ths */
        0xCCCCu, /* Offbeat 8ths */
        0x5555u, /* 16th Pulse */
        0x1111u, /* 32nd Stutter */
        0xDB6Du, /* Rolling Trance */
        0x6D35u, /* Broken Rave */
        0x0FFFu, /* Long Gate */
    };
    AminoFxState *st = &s->st;
    int pattern = enumv(s->sp[0], 7);
    int timing = enumv(s->sp[1], 3);
    static const float STEP_BEATS[3] = { 0.5f, 0.25f, 0.125f };
    float step_beats = STEP_BEATS[am_clampi(timing, 0, 2)];
    float gate_len = am_clampf(s->sp[4], 0.0f, 1.0f);
    float floor_gain = am_clampf(s->sp[5], 0.0f, 1.0f);
    float attack = lin(s->sp[2], 0.0f, 50.0f) * 0.001f;
    float release = lin(s->sp[3], 0.0f, 50.0f) * 0.001f;
    if (bpm < 20.0f) bpm = 120.0f;

    double cycle_beats = (double)step_beats * 16.0;
    double beat_phase = fmod(beat0 / cycle_beats, 1.0);
    if (beat_phase < 0.0) beat_phase += 1.0;
    double phase_inc = (double)bpm / (60.0 * (double)sr * cycle_beats);
    float free_inc = (float)phase_inc;
    if (beat_locked) {
        if (!st->sync_locked) {
            st->sync_offset = phase_error(st->phase, (float)beat_phase);
            st->sync_locked = 1;
        }
    } else {
        st->sync_locked = 0;
    }
    float offset = st->sync_offset;
    float offset_decay = expf(-1.0f / (0.050f * sr));
    float step_seconds = step_beats * 60.0f / bpm;
    float open_seconds = step_seconds * gate_len;
    uint16_t mask = PATTERNS[pattern];

    for (int i = 0; i < n; i++) {
        float phase;
        if (beat_locked) {
            phase = phase_wrap((float)beat_phase + offset);
            beat_phase += phase_inc;
            if (beat_phase >= 1.0) beat_phase -= floor(beat_phase);
            offset *= offset_decay;
        } else {
            phase = st->phase;
        }
        st->phase = phase_wrap(phase + free_inc);

        float step_pos = phase * 16.0f;
        int step_index = (int)step_pos;
        if (step_index > 15) step_index = 15;
        float within = step_pos - (float)step_index;
        float gain = floor_gain;
        if ((mask & (uint16_t)(1u << step_index)) && open_seconds > 0.0f) {
            float elapsed = within * step_seconds;
            if (elapsed < open_seconds) {
                float env = 1.0f;
                if (attack > 0.000001f && elapsed < attack)
                    env = elapsed / attack;
                if (release > 0.000001f && open_seconds - elapsed < release) {
                    float tail = (open_seconds - elapsed) / release;
                    if (tail < env) env = tail;
                }
                gain += (1.0f - floor_gain) * am_clampf(env, 0.0f, 1.0f);
            }
        }
        l[i] *= gain;
        r[i] *= gain;
    }
    st->sync_offset = offset;
}

static float shaper_soft(float x) { return am_tanh(x); }
static float shaper_tube(float x) {
    return x > 0.0f ? am_tanh(x) : am_tanh(0.6f * x) / 0.6f * 0.8f;
}
static float shaper_hard(float x) {
    return x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
}
static float shaper_fold(float x) { return sinf(x); }

static float adaa_F(int type, float x) {
    switch (type) {
    case 0: return logcosh(x);
    case 1: return x > 0.0f ? logcosh(x) : 0.8f / 0.36f * logcosh(0.6f * x);
    case 2: return fabsf(x) > 1.0f ? fabsf(x) - 0.5f : 0.5f * x * x;
    default: return -cosf(x);
    }
}

static float shaper_for(int type, float x) {
    switch (type) {
    case 0: return shaper_soft(x);
    case 1: return shaper_tube(x);
    case 2: return shaper_hard(x);
    default: return shaper_fold(x);
    }
}

/* F is adaa_F(type, x), which the caller keeps for the next sample. */
static float adaa(int type, float F, float x, float xp, float fp) {
    float dx = x - xp;
    if (fabsf(dx) > 1e-5f) return (F - fp) / dx;
    return shaper_for(type, 0.5f * (x + xp));
}

static void fx_drive(AminoFxSlot *s, float *l, float *r, int n, float sr) {
    AminoFxState *st = &s->st;
    float gain = powf(10.0f, lin(s->sp[0], 0.0f, 36.0f) / 20.0f);
    float out = powf(10.0f, lin(s->sp[3], -24.0f, 0.0f) / 20.0f);
    float hpc = one_pole_coef(exps(s->sp[4], 20.0f, 500.0f), sr);
    float tilt = lin(s->sp[1], -6.0f, 6.0f);
    float lo_g = powf(10.0f, -tilt / 40.0f), hi_g = powf(10.0f, tilt / 40.0f);
    float tc = one_pole_coef(1000.0f, sr);
    float crush = s->sp[5];
    float rate = powf(2000.0f / sr, crush);
    float levels = powf(2.0f, 16.0f - 12.0f * crush) * 0.5f;
    int type = enumv(s->sp[2], 5);
    int use_adaa = type < 4;
    float *bufs[2] = { l, r };
    for (int i = 0; i < n; i++) {
        int take = 0;
        if (!use_adaa) {
            st->cnt += rate;
            if (st->cnt >= 1.0f) { st->cnt -= 1.0f; take = 1; }
        }
        for (int c = 0; c < 2; c++) {
            float xin = bufs[c][i];
            st->hp[c] += (xin - st->hp[c]) * hpc;
            float x = (xin - st->hp[c]) * gain;
            if (use_adaa) {
                float F = adaa_F(type, x);
                float y = adaa(type, F, x, st->xp[c], st->fp[c]);
                st->xp[c] = x;
                st->fp[c] = F;
                x = y;
            } else {
                if (take)
                    st->hold[c] = roundf(am_tanh(x) * levels) / levels;
                x = st->hold[c];
            }
            st->lp[c] += (x - st->lp[c]) * tc;
            bufs[c][i] = (st->lp[c] * lo_g + (x - st->lp[c]) * hi_g) * out;
        }
    }
}

static void fx_comp(AminoFxSlot *s, float *l, float *r, int n, float sr) {
    AminoFxState *st = &s->st;
    float thr = lin(s->sp[0], -40.0f, 0.0f);
    float ratio = exps(s->sp[1], 1.0f, 20.0f);
    float att = expf(-1.0f / (exps(s->sp[2], 0.1f, 100.0f) * 0.001f * sr));
    float rel = expf(-1.0f / (exps(s->sp[3], 10.0f, 1000.0f) * 0.001f * sr));
    float makeup = lin(s->sp[4], 0.0f, 24.0f);
    float knee = lin(s->sp[5], 0.0f, 12.0f);
    float slope = 1.0f - 1.0f / ratio;
    for (int i = 0; i < n; i++) {
        float lvl = fabsf(l[i]) > fabsf(r[i]) ? fabsf(l[i]) : fabsf(r[i]);
        if (lvl < 1e-6f) lvl = 1e-6f;
        /* 20 log10(x) as 6.0206 log2(x), and 10^(x/20) as 2^(0.1661 x):
         * the base-2 forms are the cheaper libm calls, once per sample. */
        float db = 6.0205999f * log2f(lvl);
        float c = db > st->cenv ? att : rel;
        st->cenv = db + (st->cenv - db) * c;
        float over = st->cenv - thr;
        float gr;
        if (over > knee * 0.5f) gr = over * slope;
        else if (over > -knee * 0.5f && knee > 0.0f)
            gr = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);
        else gr = 0.0f;
        float g = exp2f((makeup - gr) * 0.16609640f);
        l[i] *= g;
        r[i] *= g;
    }
}

static void fx_eq(AminoFxSlot *s, float *l, float *r, int n, float sr) {
    AminoFxState *st = &s->st;
    float low = lin(s->sp[0], -15.0f, 15.0f);
    float mid = lin(s->sp[1], -15.0f, 15.0f);
    float midf = exps(s->sp[2], 200.0f, 8000.0f);
    float high = lin(s->sp[3], -15.0f, 15.0f);
    float q = exps(s->sp[4], 0.3f, 4.0f);
    float lowcut = exps(s->sp[5], 20.0f, 300.0f);
    /* Each band owns its biquad slot: 0 low cut, 1 low shelf, 2 peak,
     * 3 high shelf. The gain bands always run (unity at 0 dB) so crossing
     * 0 dB never hands one band's filter state to another. */
    int use_cut = lowcut > 21.0f;
    /* The coefficients only move with the knobs: rebuild them (eight biquads,
     * each a powf and a sin/cos pair) only when a smoothed knob changed. */
    const float key[6] = { low, mid, midf, high, q, lowcut };
    int moved = !st->eq_ready;
    for (int k = 0; k < 6; k++) moved |= key[k] != st->eq_key[k];
    for (int c = 0; c < 2; c++) {
        if (!use_cut) st->bq[c][0].z1 = st->bq[c][0].z2 = 0.0f;
        if (!moved) continue;
        if (use_cut) bq_set(&st->bq[c][0], 3, lowcut, 0.0f, 0.707f, sr);
        bq_set(&st->bq[c][1], 1, 120.0f, low, 0.707f, sr);
        bq_set(&st->bq[c][2], 0, midf, mid, q, sr);
        bq_set(&st->bq[c][3], 2, 8000.0f, high, 0.707f, sr);
    }
    if (moved) {
        for (int k = 0; k < 6; k++) st->eq_key[k] = key[k];
        st->eq_ready = 1;
    }
    for (int c = 0; c < 2; c++) {
        float *buf = c == 0 ? l : r;
        AminoBiquad *bq = st->bq[c];
        for (int i = 0; i < n; i++) {
            float x = buf[i];
            if (use_cut) x = bq_run(&bq[0], x);
            x = bq_run(&bq[1], x);
            x = bq_run(&bq[2], x);
            x = bq_run(&bq[3], x);
            buf[i] = x;
        }
    }
}

static void fx_delay(AminoFxSlot *s, float *l, float *r, int n, float sr,
                     float bpm) {
    AminoFxState *st = &s->st;
    int sync = enumv(s->sp[1], 13);
    float tsec;
    if (sync == 0) tsec = exps(s->sp[0], 1.0f, 2000.0f) * 0.001f;   /* docs/11: exp */
    else tsec = div_seconds(sync, bpm);
    while (tsec > 2.0f) tsec *= 0.5f;
    float target = tsec * sr;
    if (st->dt < 0.0f) {
        st->dt = st->dstart = st->dtarget = target;
        st->dfade = 1.0f;
    } else if (fabsf(target - st->dtarget) > 1.0f) {
        /* A synced tempo/transport change must not sweep the read head: that
         * Doppler-glides every echo tail. Crossfade two fixed taps instead. */
        float f = am_clampf(st->dfade, 0.0f, 1.0f);
        st->dstart += (st->dtarget - st->dstart) * f;
        st->dt = st->dstart;
        st->dtarget = target;
        st->dfade = 0.0f;
    }
    float fb = s->sp[2];
    float pp = s->sp[4];
    float mod = s->sp[5];
    float tone = lin(s->sp[3], -1.0f, 1.0f);
    float c = tone < 0.0f
                  ? one_pole_coef(20000.0f * powf(1000.0f / 20000.0f, tone), sr)
                  : one_pole_coef(20.0f * powf(50.0f, tone), sr);
    int lp = tone < 0.0f;
    float wow = mod * 0.002f * sr;
    float fade_step = 1.0f / (0.050f * sr);
    float wow_inc = 0.5f / sr;
    for (int i = 0; i < n; i++) {
        float tap_fade = am_clampf(st->dfade, 0.0f, 1.0f);
        float mod_delay = wow != 0.0f ? wow * sinf(AMINO_TAU * st->dphase) : 0.0f;
        st->dphase += wow_inc;
        if (st->dphase >= 1.0f) st->dphase -= floorf(st->dphase);
        float old_delay = st->dstart + mod_delay;
        float new_delay = st->dtarget + mod_delay;
        float yl, yr;
        if (tap_fade >= 1.0f) {
            yl = dl_read(&st->d[0], new_delay);
            yr = dl_read(&st->d[1], new_delay);
        } else {
            float old_l = dl_read(&st->d[0], old_delay);
            float old_r = dl_read(&st->d[1], old_delay);
            float new_l = dl_read(&st->d[0], new_delay);
            float new_r = dl_read(&st->d[1], new_delay);
            yl = old_l + (new_l - old_l) * tap_fade;
            yr = old_r + (new_r - old_r) * tap_fade;
        }
        st->dt = st->dstart + (st->dtarget - st->dstart) * tap_fade;
        if (st->dfade < 1.0f) {
            st->dfade += fade_step;
            if (st->dfade >= 1.0f) {
                st->dfade = 1.0f;
                st->dstart = st->dtarget;
                st->dt = st->dtarget;
            }
        }
        st->df[0] += (yl - st->df[0]) * c;
        st->df[1] += (yr - st->df[1]) * c;
        float fl = lp ? st->df[0] : yl - st->df[0];
        float fr = lp ? st->df[1] : yr - st->df[1];
        float mono = 0.5f * (l[i] + r[i]);
        float inl = l[i] * (1.0f - pp) + mono * pp;
        float inr = r[i] * (1.0f - pp);
        dl_write(&st->d[0], am_tanh(inl + fb * ((1.0f - pp) * fl + pp * fr)));
        dl_write(&st->d[1], am_tanh(inr + fb * ((1.0f - pp) * fr + pp * fl)));
        l[i] = yl;
        r[i] = yr;
    }
}

static void fx_reverb(AminoFxSlot *s, float *l, float *r, int n, float sr,
                      int shimmer) {
    AminoFxState *st = &s->st;
    static const int MOD_RATE[8] = { 61, 83, 71, 97, 53, 89, 67, 77 }; /* x0.01 Hz */
    float pre = shimmer ? 0.02f * sr : lin(s->sp[3], 0.0f, 200.0f) * 0.001f * sr;
    float width = shimmer ? 1.0f : s->sp[5];
    float mod = (shimmer ? s->sp[5] : s->sp[4]) * 12.0f;
    float decay = exps(s->sp[1], 0.2f, 20.0f);
    float damp = s->sp[2];
    int freeze = decay >= 19.99f;
    float t60 = decay < 0.2f ? 0.2f : decay;
    float dc = one_pole_coef(500.0f + 19500.0f * (1.0f - damp) * (1.0f - damp), sr);
    float size_scale = 0.6f + 1.4f * lin(s->sp[0], 0.0f, 1.0f);
    for (int k = 0; k < 8; k++) {
        st->len[k] = (int)((float)REVERB_BASE[k] * size_scale);
        float L = (float)st->len[k];
        st->gain[k] = freeze ? 1.0f : powf(10.0f, -3.0f * L / (t60 * sr));
    }
    float samt = shimmer ? (freeze ? 0.0f : s->sp[3]) : 0.0f;
    float ratio = 1.0f;
    /* Interval enum is -12,5,7,12,19,24; map the index explicitly. */
    if (shimmer) {
        static const int INTERVALS[6] = { -12, 5, 7, 12, 19, 24 };
        ratio = exp2f((float)INTERVALS[enumv(s->sp[4], 6)] / 12.0f);
    }
    float win = 0.08f * sr;
    float mod_pos[8], mod_step[8], mod_inc[8];
    for (int k = 0; k < 8; k++) mod_inc[k] = (float)MOD_RATE[k] * 0.01f / sr;
    for (int i = 0; i < n; i++) {
        if (i % FX_MOD_CHUNK == 0) {
            int m = n - i;
            if (m > FX_MOD_CHUNK) m = FX_MOD_CHUNK;
            for (int k = 0; k < 8; k++) {
                float a = mod * sinf(AMINO_TAU * st->mph[k]);
                float b = mod * sinf(AMINO_TAU * (st->mph[k] + mod_inc[k] * (float)m));
                mod_pos[k] = a;
                mod_step[k] = (b - a) / (float)m;
            }
        }
        float xs[2];
        const float difftaps[2][4] = { { 142, 107, 379, 277 }, { 151, 113, 389, 293 } };
        for (int c = 0; c < 2; c++) {
            float *src = c == 0 ? l : r;
            dl_write(&st->pre[c], src[i]);
            float x = freeze ? 0.0f : dl_read(&st->pre[c], pre);
            for (int k = 0; k < 4; k++) {
                float y = dl_read(&st->diff[c][k], difftaps[c][k] - 1.0f);
                float w = x + 0.6f * y;
                dl_write(&st->diff[c][k], w);
                x = y - 0.6f * w;
            }
            xs[c] = x;
        }
        float o[8];
        for (int k = 0; k < 8; k++) {
            o[k] = dl_read(&st->lines[k], (float)st->len[k] - 1.0f + mod_pos[k]);
            mod_pos[k] += mod_step[k];
            st->mph[k] += mod_inc[k];
            if (st->mph[k] >= 1.0f) st->mph[k] -= floorf(st->mph[k]);
        }
        if (!freeze) {
            for (int k = 0; k < 8; k++) {
                st->damp[k] += (o[k] - st->damp[k]) * dc;
                o[k] = st->damp[k];
            }
        }
        float a = o[0], b = o[1], c = o[2], d = o[3];
        float e = o[4], f = o[5], g = o[6], h = o[7];
        a = a + b; b = o[0] - o[1];
        c = c + d; d = o[2] - o[3];
        e = e + f; f = o[4] - o[5];
        g = g + h; h = o[6] - o[7];
        float a2 = a + c, b2 = b + d, c2 = a - c, d2 = b - d;
        float e2 = e + g, f2 = f + h, g2 = e - g, h2 = f - h;
        float mix[8];
        mix[0] = a2 + e2; mix[1] = b2 + f2; mix[2] = c2 + g2; mix[3] = d2 + h2;
        mix[4] = a2 - e2; mix[5] = b2 - f2; mix[6] = c2 - g2; mix[7] = d2 - h2;
        float shim = 0.0f;
        if (shimmer && samt > 0.0f) {
            dl_write(&st->sh, 0.25f * (o[0] + o[1] + o[2] + o[3]));
            st->sphase += (1.0f - ratio) / win;
            st->sphase -= floorf(st->sphase);
            float p1 = st->sphase;
            float p2 = p1 + 0.5f;
            if (p2 >= 1.0f) p2 -= 1.0f;
            float s1 = dl_read(&st->sh, p1 * win) * sinf((float)M_PI * p1);
            s1 *= s1;
            float s2 = dl_read(&st->sh, p2 * win) * sinf((float)M_PI * p2);
            s2 *= s2;
            float sv = s1 + s2;
            st->slp += (sv - st->slp) * 0.6f;
            st->shp += (st->slp - st->shp) * 0.02f;
            shim = am_tanh((st->slp - st->shp) * 1.5f) * samt;
        }
        for (int k = 0; k < 8; k++) {
            float fbk = mix[k] * 0.35355339f * st->gain[k];
            if (shimmer && k < 4) fbk = fbk * (1.0f - 0.5f * samt) + shim;
            dl_write(&st->lines[k], fbk + xs[k & 1]);
        }
        float lo = (o[0] + o[2] + o[4] + o[6]) * 0.3f;
        float ro = (o[1] + o[3] + o[5] + o[7]) * 0.3f;
        float mid = 0.5f * (lo + ro);
        float side = 0.5f * (lo - ro) * width;
        l[i] = mid + side;
        r[i] = mid - side;
    }
}

static void fx_run(AminoInstance *in, AminoFxSlot *s, int type, float *l,
                   float *r, int n) {
    switch (type) {
    case 1: fx_chorus(s, l, r, n, (float)in->sr); break;
    case 2: fx_flanger(s, l, r, n, (float)in->sr, in->bpm,
                        in->sync_beat, in->sync_transport); break;
    case 3: fx_phaser(s, l, r, n, (float)in->sr); break;
    case 4: fx_tremolo(s, l, r, n, (float)in->sr, in->bpm,
                        in->sync_beat, in->sync_transport); break;
    case 5: fx_drive(s, l, r, n, (float)in->sr); break;
    case 6: fx_comp(s, l, r, n, (float)in->sr); break;
    case 7: fx_eq(s, l, r, n, (float)in->sr); break;
    case 8: fx_delay(s, l, r, n, (float)in->sr, in->bpm); break;
    case 9: fx_reverb(s, l, r, n, (float)in->sr, 0); break;
    case 10: fx_reverb(s, l, r, n, (float)in->sr, 1); break;
    case 11: fx_trance_gate(s, l, r, n, (float)in->sr, in->bpm,
                            in->sync_beat, in->sync_transport); break;
    default: break;
    }
}

static int fx_is_insert(int type) {
    return type == 4 || type == 5 || type == 6 || type == 7 || type == 11;
}

/* Dry/wet gains for wet amount m: linear for inserts, equal power for sends. */
static void fx_mix_gains(int insert, float m, float *dg, float *wg) {
    if (insert) {
        *dg = 1.0f - m;
        *wg = m;
    } else {
        *dg = cosf(m * (float)M_PI * 0.5f);
        *wg = sinf(m * (float)M_PI * 0.5f);
    }
}

void amino_fx_process(AminoInstance *in, int slot, float *l, float *r,
                      int frames) {
    if (slot < 0 || slot > 1 || frames <= 0) return;
    if (frames > AMINO_BLOCK) frames = AMINO_BLOCK;
    AminoFxSlot *s = &in->fx[slot];
    float *g = slot == 0 ? &in->g.fx1_mix : &in->g.fx2_mix;
    float mix = am_clampf(*g, 0.0f, 1.0f);
    float prm[6];
    float *base = slot == 0 ? &in->g.fx1_p1 : &in->g.fx2_p1;
    for (int i = 0; i < 6; i++) prm[i] = base[i];
    float sm = 0.06f;                 /* ~8 ms one-pole per block */
    s->smix += (mix - s->smix) * sm;
    /* While the old type fades out, the knobs may already hold the next
     * type's defaults; keep the outgoing effect on its own settings. */
    if (!s->fading_out)
        for (int i = 0; i < 6; i++) s->sp[i] += (prm[i] - s->sp[i]) * sm;
    s->mix = mix;
    for (int i = 0; i < 6; i++) s->p[i] = prm[i];

    /* Incremental clear after a type change. */
    if (s->need_clear) {
        uint32_t left = s->clear_len - s->clear_pos;
        uint32_t chunk = left > FX_CLEAR_CHUNK ? FX_CLEAR_CHUNK : left;
        if (chunk > 0) memset(s->arena + s->clear_pos, 0, chunk);
        s->clear_pos += chunk;
        if (s->clear_pos >= s->clear_len) {
            s->need_clear = 0;
            s->clear_pos = 0;
            s->fade = 0.0f;
        }
        return;                       /* dry while the arena is being cleared */
    }

    if (s->type == 0 && !s->fading_out) return;

    float dryl[AMINO_BLOCK], dryr[AMINO_BLOCK];
    memcpy(dryl, l, (size_t)frames * sizeof(float));
    memcpy(dryr, r, (size_t)frames * sizeof(float));

    fx_run(in, s, s->type, l, r, frames);

    float step = (float)frames / (FX_FADE_SECONDS * (float)in->sr);
    if (s->fading_out) {
        if (s->fade > step) s->fade -= step;
        else s->fade = 0.0f;
    } else if (s->fade < 1.0f) {
        s->fade += step;
        if (s->fade > 1.0f) s->fade = 1.0f;
    }

    /* Ramp from last block's wet amount so mix and type fades are smooth
     * rather than stepping once per block. */
    float m = am_clampf(s->smix * s->fade, 0.0f, 1.0f);
    int insert = fx_is_insert(s->type);
    float dg, wg, dg1, wg1;
    fx_mix_gains(insert, s->wet_last, &dg, &wg);
    fx_mix_gains(insert, m, &dg1, &wg1);
    s->wet_last = m;
    float ddg = (dg1 - dg) / (float)frames, dwg = (wg1 - wg) / (float)frames;
    for (int i = 0; i < frames; i++) {
        dg += ddg;
        wg += dwg;
        l[i] = dryl[i] * dg + l[i] * wg;
        r[i] = dryr[i] * dg + r[i] * wg;
    }

    if (s->fading_out && s->fade <= 0.0f) {
        int next = s->pending_type;
        s->fading_out = 0;
        s->pending_type = -1;
        if (next <= 0) {
            s->type = 0;
            s->need_clear = 0;
            s->clear_len = 0;
        } else {
            float *base = slot == 0 ? &in->g.fx1_p1 : &in->g.fx2_p1;
            int fresh = 1;
            for (int i = 0; i < 6; i++)
                if (base[i] > 0.0001f) fresh = 0;
            if (fresh)
                for (int i = 0; i < 6; i++) base[i] = FX_DEFAULTS[next][i];
            if (!fx_alloc(in, s, next)) {
                s->type = 0;
                s->pending_type = -1;
            }
        }
    }
}

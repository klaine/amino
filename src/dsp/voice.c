/* Voice allocation, MIDI note handling and the per-voice DSP chain.
 *
 * Mirrors tools/emulator/engine.py: two wavetable oscillators (4x nearest
 * sample in Vintage mode, 1x linear in Clean), cross modulation, dirt, drive,
 * a ZDF 4-pole/2-pole filter, three ADSRs, two LFOs and the 8-slot matrix.
 */
#include "amino.h"
#include "util.h"

#include <math.h>
#include <string.h>

/* ------------------------------------------------------------------ tables */

static const signed char CHORD_OFFSETS[11][4] = {
    { 0, 0, 0, 0 },       /* Off */
    { 0, 4, 7, 0 },       /* Maj */
    { 0, 3, 7, 0 },       /* Min */
    { 0, 4, 7, 10 },      /* 7 */
    { 0, 4, 7, 11 },      /* Maj7 */
    { 0, 3, 7, 10 },      /* m7 */
    { 0, 2, 7, 0 },       /* sus2 */
    { 0, 5, 7, 0 },       /* sus4 */
    { 0, 7, 0, 0 },       /* 5th */
    { 0, 12, 0, 0 },      /* Oct */
    { 0, 12, 24, 36 },    /* Stack4 */
};

static const unsigned short SCALE_MASK[12] = {
    0xFFFF,                                  /* Off (unused) */
    0x05AD,                                  /* Set = C minor */
    0x0AB5,                                  /* Major */
    0x05AD,                                  /* Minor */
    0x06AD,                                  /* Dorian */
    0x05AB,                                  /* Phryg */
    0x0AD5,                                  /* Lydian */
    0x06B5,                                  /* Mixo */
    0x0295,                                  /* Penta */
    0x04A9,                                  /* MinPen */
    0x04E9,                                  /* Blues */
    0x0FFF,                                  /* Chrom */
};

static int ascii_equal_fold(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
    }
    return *a == 0 && *b == 0;
}

void amino_move_scale_update(AminoInstance *in, int root_note, const char *scale) {
    if (root_note >= 0 && root_note < 12) in->move_root_note = root_note;
    if (!scale || !scale[0]) return;

    int index = -1;
    if (ascii_equal_fold(scale, "major")) index = 2;
    else if (ascii_equal_fold(scale, "minor")) index = 3;
    else if (ascii_equal_fold(scale, "dorian")) index = 4;
    else if (ascii_equal_fold(scale, "phrygian") || ascii_equal_fold(scale, "phryg")) index = 5;
    else if (ascii_equal_fold(scale, "lydian")) index = 6;
    else if (ascii_equal_fold(scale, "mixolydian") || ascii_equal_fold(scale, "mixo")) index = 7;
    else if (ascii_equal_fold(scale, "major pentatonic") ||
             ascii_equal_fold(scale, "pentatonic")) index = 8;
    else if (ascii_equal_fold(scale, "minor pentatonic") ||
             ascii_equal_fold(scale, "min pentatonic")) index = 9;
    else if (ascii_equal_fold(scale, "blues")) index = 10;
    else if (ascii_equal_fold(scale, "chromatic")) index = 11;

    if (index >= 0) in->move_scale_mask = SCALE_MASK[index];
}

static int quantize_scale(AminoInstance *in, int note, int scale) {
    if (scale <= 0 || scale >= 12) return note;
    unsigned mask = SCALE_MASK[scale];
    int root = 0;
    if (scale == 1) {
        mask = in->move_scale_mask ? in->move_scale_mask : SCALE_MASK[3];
        root = am_clampi(in->move_root_note, 0, 11);
    }
    int n = note;
    for (int guard = 0; guard < 24; guard++) {
        int pc = (n - root) % 12;
        if (pc < 0) pc += 12;
        if (mask & (1u << pc)) break;
        n--;
    }
    return am_clampi(n, 0, 127);
}

static float div_beats(int idx) {
    static const float beats[14] = {
        0.0833333f, 0.125f, 0.1666667f, 0.25f, 0.3333333f, 0.5f,
        0.6666667f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f, 64.0f,
    };
    if (idx < 0) idx = 0;
    if (idx > 13) idx = 13;
    return beats[idx];
}

/* Rusty; the modulation matrix sources/targets (docs/03 section 10). */
enum {
    SRC_OFF = 0, SRC_E1, SRC_E2, SRC_E3, SRC_LFO1, SRC_LFO2, SRC_LFO1MW,
    SRC_LFO2MW, SRC_VEL, SRC_RELVEL, SRC_KEY, SRC_POLYAT, SRC_CHANAT,
    SRC_MW, SRC_BREATH, SRC_FOOT, SRC_EXPR, SRC_BEND, SRC_TIMBRE,
    SRC_SUSTAIN, SRC_RAND, SRC_RANDBI, SRC_VOICE, SRC_ALT, SRC_CONST,
    SRC_FLAV, SRC_E4,
};

enum {
    DST_OFF = 0, DST_PITCH, DST_OSC1P, DST_OSC2P, DST_OSC1W, DST_OSC2W,
    DST_OSC1L, DST_OSC2L, DST_DIRT, DST_CUT, DST_RES, DST_DRIVE, DST_PAN,
    DST_AMP, DST_LFO1R, DST_LFO2R, DST_LFO1D, DST_LFO2D, DST_E1D, DST_E2D,
    DST_E3D, DST_E1A, DST_E3R, DST_GLIDE, DST_SEND, DST_XMOD,
};

/* Filter Type option indices (state v2 order, see params.c migration). */
enum {
    FT_LP24 = 0, FT_LP12, FT_LADDER, FT_HP24, FT_HP12, FT_BP12, FT_OFF,
};

static int is_lfo_src(int src) {
    return src >= SRC_LFO1 && src <= SRC_LFO2MW;
}

/* Moog ladder stage saturator: unity small-signal gain, soft ceiling. */
static inline float moog_sat(float x) {
    return am_tanh(1.4f * x) * (1.0f / 1.4f);
}

/* ------------------------------------------------------------------ envelopes */

static float env_step(int *stage, float *level, float a, float d, float s,
                      float r, float a_mod, float d_mod, float r_mod,
                      int n, float sr) {
    int st = *stage;
    float lv = *level;
    if (st == AMINO_ENV_ATTACK) {
        float t = am_env_time(a + a_mod) / 1.466f;
        lv += (1.3f - lv) * (1.0f - expf(-(float)n / (t * sr)));
        if (lv >= 1.0f) { lv = 1.0f; st = AMINO_ENV_DECAY; }
    } else if (st == AMINO_ENV_DECAY) {
        if (lv != s) {
            float t = am_env_time(d + d_mod) / 4.6f;
            lv += (s - lv) * (1.0f - expf(-(float)n / (t * sr)));
            if (fabsf(s - lv) < 1e-5f) lv = s;   /* settled: -100 dB */
        }
    } else if (st == AMINO_ENV_RELEASE) {
        float t = am_env_time(r + r_mod) / 4.6f;
        lv -= lv * (1.0f - expf(-(float)n / (t * sr)));
        if (lv < 1e-4f) { lv = 0.0f; st = AMINO_ENV_OFF; }
    }
    *stage = st;
    *level = lv;
    return lv;
}

static void env_gate(AminoVoice *v, int idx, float a, float d, float s, float r) {
    v->env_stage[idx] = AMINO_ENV_ATTACK;
    (void)a; (void)d; (void)s; (void)r;
}

static void env_release(AminoVoice *v) {
    for (int i = 0; i < AMINO_NUM_ENVS; i++)
        if (v->env_stage[i] != AMINO_ENV_OFF)
            v->env_stage[i] = AMINO_ENV_RELEASE;
}

/* ------------------------------------------------------------------ LFO */

static float lfo_shape(const AminoLfo *l, int shape, float p) {
    switch (shape) {
    case 0: return sinf(AMINO_TAU * p);
    case 1:
        if (p < 0.25f) return 4.0f * p;
        if (p < 0.75f) return 2.0f - 4.0f * p;
        return 4.0f * p - 4.0f;
    case 2: return p < 0.5f ? 1.0f : -1.0f;
    case 3: return 1.0f - 2.0f * p;
    case 4: return 2.0f * p - 1.0f;
    case 5: return l->prev + (l->cur - l->prev) * p;
    default: return l->cur;
    }
}

static float lfo_step(AminoLfo *l, float hz, int shape, int n, int sr,
                      uint32_t *rng) {
    float p = l->phase + hz * (float)n / (float)sr;
    if (p >= 1.0f) {
        p -= floorf(p);
        l->prev = l->cur;
        l->cur = am_rand_bipolar(rng);
    }
    l->phase = p;
    return lfo_shape(l, shape, p);
}

/* ------------------------------------------------------------------ helpers */

static float layer_flavour(AminoInstance *in, int li) {
    float f = in->l[li].flavour + in->g.flavour_global;
    return am_clampf(f, 0.0f, 1.0f);
}

static int layer_enabled(AminoInstance *in, int li) {
    int count = am_clampi((int)(in->g.layer_count + 0.5f), 1, 4);
    return li < count && in->l[li].on >= 0.5f;
}

static AminoVoice *find_voice(AminoInstance *in, int li) {
    for (int i = 0; i < AMINO_MAX_VOICES; i++)
        if (in->v[i].active && in->v[i].layer == li && in->v[i].gate)
            return &in->v[i];
    /* Mono retriggers can leave older release tails active. Never let the
     * first release-tail voice hide the currently gated note from note-off. */
    for (int i = 0; i < AMINO_MAX_VOICES; i++)
        if (in->v[i].active && in->v[i].layer == li &&
            in->v[i].fade_total > 0 && in->v[i].pending_layer == li)
            return &in->v[i];
    for (int i = 0; i < AMINO_MAX_VOICES; i++)
        if (in->v[i].active && in->v[i].layer == li)
            return &in->v[i];
    return 0;
}

static int voice_has_pending_start(const AminoVoice *v) {
    return v->pending_layer >= 0 && v->fade_total > 0;
}

static AminoVoice *alloc_candidate(AminoInstance *in, int li, int limit,
                                  uint32_t reserved, int start, int pool,
                                  int pending_only, int *picked) {
    AminoVoice *best = 0;
    uint64_t best_t = 0;
    for (int off = 0; off < limit; off++) {
        int i = (start + off) % limit;
        AminoVoice *v = &in->v[i];
        if (reserved & (1u << i)) continue;
        if (voice_has_pending_start(v) != pending_only) continue;
        if (pool == 0 && v->gate) continue;       /* release tails */
        if (pool == 1 && v->layer != li) continue; /* same layer */
        if (pending_only) {
            /* Pending notes have the same old t_on; rotate to avoid
             * overwriting one queued chord tone on the next MIDI event. */
            *picked = i;
            return v;
        }
        if (!best || v->t_on < best_t) {
            best = v;
            best_t = v->t_on;
            *picked = i;
        }
    }
    return best;
}

static AminoVoice *alloc_voice(AminoInstance *in, int li, uint32_t reserved) {
    int limit = am_clampi((int)(in->g.voices + 0.5f), 1, AMINO_MAX_VOICES);
    int start = (int)(in->alloc_cursor % (uint32_t)limit);
    int picked = -1;

    for (int off = 0; off < limit; off++) {
        int i = (start + off) % limit;
        if (!(reserved & (1u << i)) && !in->v[i].active) {
            in->alloc_cursor = (uint32_t)((i + 1) % limit);
            return &in->v[i];
        }
    }

    /* Prefer unclaimed release tails, then voices from this layer, then any
     * voice. Only reuse a queued start when no non-pending voice remains. */
    for (int pending = 0; pending <= 1; pending++) {
        for (int pool = 0; pool < 3; pool++) {
            AminoVoice *v = alloc_candidate(in, li, limit, reserved, start,
                                             pool, pending, &picked);
            if (v) {
                in->alloc_cursor = (uint32_t)((picked + 1) % limit);
                return v;
            }
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ start */

static void voice_start_now(AminoInstance *in, AminoVoice *v, int li, int note,
                            int root, float vel, int legato, int channel) {
    LayerParams *p = &in->l[li];
    float sr = (float)in->sr;

    v->layer = li;
    v->active = 1;
    v->gate = 1;
    v->sustained = 0;
    v->root_note = root;
    v->note = note;
    v->vel = vel;
    v->release_vel = 0.5f;
    v->midi_ch = channel;
    v->mpe_ch = (in->g.mpe >= 0.5f && channel >= 0 && channel < 16) ? channel : -1;
    v->poly_seen = 0;
    v->poly_at = 0.0f;
    v->pressure = v->mpe_ch >= 0 ? in->mpe_at[v->mpe_ch] : in->chan_at;
    v->t_on = in->clock;
    v->fade = 0;
    v->fade_total = 0;
    v->pending_layer = -1;
    v->pending_root = -1;
    v->rand = am_rand_unit(&v->rng);
    v->rand_bi = am_rand_bipolar(&v->rng);
    v->layer_alt = (in->counter & 1) ? 1.0f : -1.0f;

    float flav = layer_flavour(in, li);
    v->flav_wave = am_rand_bipolar(&v->rng) * 3.0f * flav;
    v->flav_pitch = am_rand_bipolar(&v->rng) * 0.12f * flav;
    v->flav_cut = am_rand_bipolar(&v->rng) * flav;
    v->flav_src = am_rand_bipolar(&v->rng) * flav;

    float target = (float)(note + 12 * (int)floorf(p->octave + 0.5f));
    v->target = target;
    if (!legato || p->glide <= 0.0f) v->pitch = target;

    if (!legato) {
        memset(v->env, 0, sizeof v->env);
        for (int i = 0; i < AMINO_NUM_ENVS; i++) v->env_stage[i] = AMINO_ENV_OFF;
        v->p1 = v->p2 = 0.0f;
        v->z[0] = v->z[1] = v->z[2] = v->z[3] = 0.0f;
        v->burst_env = 0.0f;
        v->out_gl = v->out_gr = 0.0f;  /* envelopes restart from zero */
        v->dc_x = v->dc_y = 0.0f;
        v->click_left = (int)(0.004f * sr);
        v->w1 = (int)(p->osc1_wave + 0.5f);
        v->w2 = (int)(p->osc2_wave + 0.5f);
        v->w1 = am_clampi(v->w1, 0, 63);
        v->w2 = am_clampi(v->w2, 0, 63);

        for (int k = 0; k < 2; k++) {
            int phase_mode = (int)((k == 0 ? p->lfo1_phase : p->lfo2_phase) + 0.5f);
            float ph = 0.0f;
            if (phase_mode != 1) {
                float hz;
                if ((k == 0 ? p->lfo1_sync : p->lfo2_sync) >= 0.5f) {
                    int d = (int)((k == 0 ? p->lfo1_div : p->lfo2_div) + 0.5f);
                    hz = in->bpm / 60.0f / div_beats(d);
                } else {
                    hz = am_lfo_hz(k == 0 ? p->lfo1_rate : p->lfo2_rate);
                }
                /* double: a float clock stalls the phase after hours */
                ph = (float)fmod((double)in->clock * (double)hz / (double)in->sr, 1.0);
            }
            v->lfo[k].phase = ph;
            v->lfo[k].prev = am_rand_bipolar(&v->rng);
            v->lfo[k].cur = am_rand_bipolar(&v->rng);
        }
    }

    /* The LFO fade-in restarts with the envelopes; a Legato note keeps both. */
    int keep_envs = legato && (int)(p->poly + 0.5f) == 2;
    if (!keep_envs) v->lfo_t0 = in->clock;

    /* Pan mode. */
    float pan = p->pan;
    int pm = (int)(p->pan_mode + 0.5f);
    if (pm == 1) v->pan = am_rand_bipolar(&v->rng) * pan;
    else if (pm == 2) v->pan = (in->counter & 1) ? pan : -pan;
    else if (pm == 3) {
        int vid = (int)(v - in->v);
        v->pan = ((float)(vid % 8) / 3.5f - 1.0f) * pan;
    } else if (pm == 4 || pm == 5) v->pan = 0.0f;
    else v->pan = pan;

    if (keep_envs) {
        /* Legato: keep envelopes running, only retarget pitch. */
        return;
    }
    for (int i = 0; i < AMINO_NUM_ENVS; i++)
        env_gate(v, i, p->e1_a, p->e1_d, p->e1_s, p->e1_r);
}

static void voice_trigger(AminoInstance *in, AminoVoice *v, int li, int note,
                          int root, float vel, int legato, int channel) {
    if (!v) return;
    if (v->active && !legato) {
        /* Fast fade out, then start the new note at the end of the block. */
        v->sustained = 0;
        v->pending_note = note;
        v->pending_root = root;
        v->pending_layer = li;
        v->pending_ch = channel;
        v->pending_vel = vel;
        if (v->fade <= 0) {
            int f = (int)(0.002f * (float)in->sr);
            if (f < 1) f = 1;
            if (f > AMINO_BLOCK) f = AMINO_BLOCK;
            v->fade_total = f;
            v->fade = f;
        }
        return;
    }
    voice_start_now(in, v, li, note, root, vel, legato, channel);
}

static void cancel_pending_note(AminoVoice *v) {
    if (!v) return;
    v->fade = 0;
    v->fade_total = 0;
    v->pending_layer = -1;
    v->pending_root = -1;
}

void amino_voice_init(AminoInstance *in, int vi) {
    AminoVoice *v = &in->v[vi];
    memset(v, 0, sizeof *v);
    v->rng = 0x9E3779B9u ^ (uint32_t)(vi * 2654435761u) ^ in->seed;
    if (!v->rng) v->rng = 0x1234567u;
    v->mpe_ch = -1;
    v->layer = -1;
    am_pan_gains(0.0f, &v->pan_gl, &v->pan_gr);
}

/* ------------------------------------------------------------------ note on */

static int gather_targets(AminoInstance *in, int note, int channel, int *out) {
    int n = 0;
    int mode = (int)(in->g.layer_mode + 0.5f);
    int enabled[4], ec = 0;
    for (int li = 0; li < 4; li++)
        if (layer_enabled(in, li)) enabled[ec++] = li;
    if (!ec) return 0;

    if (mode == 1) {                       /* Round robin */
        out[0] = enabled[in->rr % (uint32_t)ec];
        in->rr++;
        return 1;
    }
    if (mode == 2) {                       /* Random, no immediate repeat */
        int choices[4], cc = 0;
        for (int i = 0; i < ec; i++)
            if ((uint32_t)enabled[i] != in->last_rr) choices[cc++] = enabled[i];
        if (cc == 0) {
            for (int i = 0; i < ec; i++) choices[cc++] = enabled[i];
        }
        int pick = choices[am_rand_u32(&in->rng) % (uint32_t)cc];
        in->last_rr = pick;
        out[0] = pick;
        return 1;
    }
    if (mode == 3) {                       /* Key split */
        for (int i = 0; i < ec; i++) {
            int li = enabled[i];
            int lo = (int)(in->l[li].key_lo + 0.5f);
            int hi = (int)(in->l[li].key_hi + 0.5f);
            if (note >= lo && note <= hi) out[n++] = li;
        }
        return n;
    }
    if (mode == 4) {                       /* Channel */
        for (int i = 0; i < ec; i++) {
            int li = enabled[i];
            int ch = (int)(in->l[li].midi_ch + 0.5f);
            if (ch == channel + 1) out[n++] = li;
        }
        return n;
    }
    for (int i = 0; i < ec; i++) out[n++] = enabled[i];
    return n;
}

static void mono_push(AminoInstance *in, int li, int note, int channel) {
    int16_t *st = in->mono_stack[li];
    uint8_t *ch = in->mono_channel[li];
    int c = in->mono_count[li];
    for (int i = 0; i < c; i++) {
        if (st[i] == note && ch[i] == channel) {
            for (int j = i; j < c - 1; j++) {
                st[j] = st[j + 1];
                ch[j] = ch[j + 1];
            }
            c--;
            break;
        }
    }
    if (c >= AMINO_MAX_VOICES) {
        for (int j = 0; j < AMINO_MAX_VOICES - 1; j++) {
            st[j] = st[j + 1];
            ch[j] = ch[j + 1];
        }
        c = AMINO_MAX_VOICES - 1;
    }
    st[c++] = (int16_t)note;
    ch[c - 1] = (uint8_t)channel;
    in->mono_count[li] = c;
}

static void mono_pop(AminoInstance *in, int li, int note, int channel) {
    int16_t *st = in->mono_stack[li];
    uint8_t *ch = in->mono_channel[li];
    int c = in->mono_count[li];
    for (int i = 0; i < c; i++) {
        if (st[i] == note && ch[i] == channel) {
            for (int j = i; j < c - 1; j++) {
                st[j] = st[j + 1];
                ch[j] = ch[j + 1];
            }
            c--;
            break;
        }
    }
    in->mono_count[li] = c;
}

void amino_note_on(AminoInstance *in, int note, float vel, int channel) {
    if (note < 0 || note > 127) return;
    if (vel <= 0.0f) { amino_note_off(in, note, channel); return; }
    vel = am_clampf(vel, 0.0f, 1.0f);
    in->counter++;

    int targets[4], nt = gather_targets(in, note, channel, targets);
    for (int t = 0; t < nt; t++) {
        int li = targets[t];
        LayerParams *p = &in->l[li];
        int scale = (int)(p->scale + 0.5f);
        int n0 = scale > 0 ? quantize_scale(in, note, scale) : note;
        int poly = (int)(p->poly + 0.5f);

        if (poly != 0) {
            /* Keep the physical/root note in the key stack. Quantized notes
             * can alias, and note-off must pair with the original MIDI key. */
            mono_push(in, li, note, channel);
            AminoVoice *cur = 0;
            for (int i = 0; i < AMINO_MAX_VOICES; i++) {
                AminoVoice *v = &in->v[i];
                if (v->active && v->layer == li && v->gate) { cur = v; break; }
            }
            if (cur && cur->fade <= 0) {
                /* Mono retriggers the envelopes but keeps oscillator, filter,
                 * and LFO state; Legato also keeps the envelope stages. */
                voice_start_now(in, cur, li, n0, note, vel, 1, channel);
            } else {
                AminoVoice *v = alloc_voice(in, li, 0);
                voice_trigger(in, v, li, n0, note, vel, 0, channel);
            }
            continue;
        }

        int chord = (int)(p->chord + 0.5f);
        if (chord < 0 || chord > 10) chord = 0;
        int count = chord == 0 ? 1 : (chord == 10 ? 4 : (chord == 3 || chord == 4 || chord == 5 ? 4 : 3));
        if (chord == 8 || chord == 9) count = 2;
        if (chord == 6 || chord == 7) count = 3;
        uint32_t reserved = 0;
        for (int ci = 0; ci < count; ci++) {
            int n = n0 + CHORD_OFFSETS[chord][ci];
            if (n < 0) n = 0;
            if (n > 127) n = 127;
            AminoVoice *v = alloc_voice(in, li, reserved);
            if (!v) break; /* don't overwrite another tone in this chord */
            reserved |= 1u << (uint32_t)(v - in->v);
            voice_trigger(in, v, li, n, note, vel, 0, channel);
        }
    }
}

void amino_note_off(AminoInstance *in, int note, int channel) {
    for (int li = 0; li < 4; li++) {
        int poly = (int)(in->l[li].poly + 0.5f);
        if (poly != 0) {
            int scale = (int)(in->l[li].scale + 0.5f);
            int had = 0;
            for (int i = 0; i < in->mono_count[li]; i++)
                if (in->mono_stack[li][i] == note &&
                    in->mono_channel[li][i] == channel) had = 1;
            if (!had) continue;
            mono_pop(in, li, note, channel);
            AminoVoice *v = find_voice(in, li);
            if (!v) continue;
            if (in->mono_count[li] > 0) {
                int top_idx = in->mono_count[li] - 1;
                int top_root = in->mono_stack[li][top_idx];
                int top_ch = in->mono_channel[li][top_idx];
                int top = scale > 0 ? quantize_scale(in, top_root, scale) : top_root;
                if (v->fade_total > 0 && v->pending_layer == li &&
                    v->pending_root == note && v->pending_ch == channel) {
                    /* The released key was waiting to be retriggered after a
                     * short anti-click fade. Retarget that pending start to
                     * the still-held top key instead of reviving the released
                     * note. */
                    v->pending_note = top;
                    v->pending_root = top_root;
                    v->pending_vel = v->vel;
                    v->pending_ch = top_ch;
                } else if (v->gate) {
                    v->note = top;
                    v->root_note = top_root;
                    v->midi_ch = top_ch;
                    v->mpe_ch = (in->g.mpe >= 0.5f && top_ch >= 0 && top_ch < 16)
                                    ? top_ch : -1;
                    v->target = (float)(top + 12 * (int)in->l[li].octave);
                }
            } else if (in->sustain) {
                if (!v->gate && v->fade_total > 0 && v->pending_layer == li)
                    v->sustained = 1;
                else if (v->gate)
                    v->sustained = 1;
            } else if (v->fade_total > 0 && v->pending_layer == li) {
                /* A note released before its deferred start must never become
                 * a gate-on voice after the fade completes. */
                cancel_pending_note(v);
                if (v->gate) {
                    v->release_vel = v->pressure;
                    v->gate = 0;
                    env_release(v);
                }
            } else if (!v->gate) {
                continue;
            } else {
                v->release_vel = v->pressure;
                v->gate = 0;
                env_release(v);
            }
            continue;
        }
        for (int i = 0; i < AMINO_MAX_VOICES; i++) {
            AminoVoice *v = &in->v[i];
            if (!v->active || v->layer != li) continue;
            if (v->fade_total > 0 && v->pending_layer == li &&
                v->pending_root == note && v->pending_ch == channel) {
                cancel_pending_note(v);
                if (v->root_note != note || v->midi_ch != channel || !v->gate)
                    continue;
            }
            if (!v->gate) continue;
            if (v->root_note != note || v->midi_ch != channel) continue;
            if (in->sustain) v->sustained = 1;
            else {
                v->release_vel = v->pressure;
                v->gate = 0;
                env_release(v);
            }
        }
    }
}

void amino_sustain(AminoInstance *in, int on) {
    in->sustain = on ? 1 : 0;
    if (on) return;
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        if (!v->active || !v->sustained) continue;
        v->sustained = 0;
        int poly = (int)(in->l[v->layer].poly + 0.5f);
        if (poly != 0 && in->mono_count[v->layer] > 0) continue;
        v->release_vel = v->pressure;
        v->gate = 0;
        env_release(v);
    }
}

void amino_poly_at(AminoInstance *in, int note, float value, int channel) {
    value = am_clampf(value, 0.0f, 1.0f);
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        if (v->active && v->root_note == note && v->midi_ch == channel) {
            v->poly_seen = 1;
            v->poly_at = value;
            v->pressure = value;
        }
    }
}

void amino_chan_at(AminoInstance *in, float value) {
    in->chan_at = am_clampf(value, 0.0f, 1.0f);
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        if (v->active && !v->poly_seen) v->pressure = in->chan_at;
    }
}

void amino_kill_all(AminoInstance *in, int fast) {
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        cancel_pending_note(v);
        if (fast) {
            v->active = 0;
            v->gate = 0;
            v->fade = 0;
            v->fade_total = 0;
            for (int e = 0; e < AMINO_NUM_ENVS; e++) {
                v->env[e] = 0.0f;
                v->env_stage[e] = AMINO_ENV_OFF;
            }
        } else if (v->active) {
            v->gate = 0;
            env_release(v);
        }
    }
    for (int li = 0; li < 4; li++) in->mono_count[li] = 0;
}

void amino_kill_channel(AminoInstance *in, int channel, int fast) {
    if (channel < 0 || channel >= 16) return;

    /* Remove released-channel keys while preserving held keys on other MIDI
     * channels in each Mono/Legato layer. */
    for (int li = 0; li < AMINO_MAX_LAYERS; li++) {
        int count = in->mono_count[li], out = 0;
        for (int i = 0; i < count; i++) {
            if (in->mono_channel[li][i] == channel) continue;
            in->mono_stack[li][out] = in->mono_stack[li][i];
            in->mono_channel[li][out] = in->mono_channel[li][i];
            out++;
        }
        in->mono_count[li] = out;
    }

    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        int pending = v->pending_layer >= 0 && v->fade_total > 0;
        int pending_matches = pending && v->pending_ch == channel;
        if (pending_matches) cancel_pending_note(v);
        if (!v->active || v->midi_ch != channel) continue;

        int li = v->layer;
        if (li >= 0 && li < AMINO_MAX_LAYERS && in->mono_count[li] > 0) {
            int top = in->mono_count[li] - 1;
            int root = in->mono_stack[li][top];
            int ch = in->mono_channel[li][top];
            int scale = (int)(in->l[li].scale + 0.5f);
            int note = scale > 0 ? quantize_scale(in, root, scale) : root;
            v->root_note = root;
            v->note = note;
            v->midi_ch = ch;
            v->mpe_ch = (in->g.mpe >= 0.5f && ch < 16) ? ch : -1;
            v->poly_seen = 0;
            v->poly_at = 0.0f;
            v->target = (float)(note + 12 * (int)in->l[li].octave);
            continue;
        }

        if (fast) {
            /* A note queued on another channel still belongs to that channel;
             * let it start immediately rather than dropping it with this one. */
            if (pending && !pending_matches && v->pending_note >= 0) {
                int next_layer = v->pending_layer;
                int next_note = v->pending_note;
                int next_root = v->pending_root;
                int next_ch = v->pending_ch;
                float next_vel = v->pending_vel;
                voice_start_now(in, v, next_layer, next_note, next_root,
                                next_vel, 0, next_ch);
            } else {
                v->active = 0;
                v->gate = 0;
                v->fade = 0;
                v->fade_total = 0;
                for (int e = 0; e < AMINO_NUM_ENVS; e++) {
                    v->env[e] = 0.0f;
                    v->env_stage[e] = AMINO_ENV_OFF;
                }
            }
        } else {
            v->gate = 0;
            v->sustained = in->sustain ? 1 : 0;
            if (!v->sustained) env_release(v);
        }
    }
}

/* Voices at or above a lowered Voices limit: release held notes, and let a
 * queued steal finish its fade without starting the queued note. */
void amino_release_voices_from(AminoInstance *in, int first) {
    for (int i = first < 0 ? 0 : first; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        if (!v->active) continue;
        if (voice_has_pending_start(v)) {
            v->pending_note = -1;
            v->pending_root = -1;
        }
        v->sustained = 0;
        if (v->gate) {
            v->release_vel = v->pressure;
            v->gate = 0;
            env_release(v);
        }
    }
}

/* Voice Mode changed on a layer.  Poly voices are not in the Mono key stack
 * and Mono keys are not tracked per voice, so neither mode can pair the other's
 * note-offs: release the layer and start the stack empty. */
void amino_voice_mode_changed(AminoInstance *in, int li) {
    if (li < 0 || li >= AMINO_MAX_LAYERS) return;
    in->mono_count[li] = 0;
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        if (voice_has_pending_start(v) && v->pending_layer == li)
            cancel_pending_note(v);
        if (!v->active || v->layer != li) continue;
        v->sustained = 0;
        if (v->gate) {
            v->release_vel = v->pressure;
            v->gate = 0;
            env_release(v);
        }
    }
}

/* ------------------------------------------------------------------ drive */

static float shaper(int type, float x) {
    switch (type) {
    case 1: return am_tanh(x + 0.2f) - 0.19738f;      /* Transistor */
    case 2: return x > 0.0f ? am_tanh(x) : am_tanh(0.6f * x) / 0.6f * 0.8f;
    case 3: return x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x);
    case 4: return sinf(x);
    default: return x;
    }
}

/* ------------------------------------------------------------------ render */

static void smooth_layer_params(AminoInstance *in, int li, int frames,
                                int in_use) {
    AminoLayerRt *rt = &in->lr[li];
    LayerParams *target = &in->l[li];
    LayerParams *smoothed = &rt->smooth_params;
    int segments = (frames + AMINO_CTRL - 1) / AMINO_CTRL;

    /* No voice reads this layer: jump to the targets so the next note starts
     * on them, and leave the per-slice snapshots stale. */
    if (!in_use) {
        *smoothed = *target;
        rt->smooth_ready = 1;
        rt->slices_valid = 0;
        return;
    }
    if (!rt->smooth_ready) {
        *smoothed = *target;
        rt->smooth_ready = 1;
        rt->slices_valid = 0;
    }
    /* Settled: every slice already holds the targets. */
    if (memcmp(smoothed, target, sizeof *target) == 0) {
        if (!rt->slices_valid) {
            for (int seg = 0; seg < AMINO_BLOCK / AMINO_CTRL; seg++)
                rt->smooth_slices[seg] = *smoothed;
            rt->slices_valid = 1;
        }
        return;
    }
    rt->slices_valid = 0;

    /* Advance the one-pole response every control slice
     * rather than making one audible-sized step per audio block. The resulting
     * snapshots are shared by every voice in this layer for the current block.
     * Values within float stalling distance snap to the target so a layer
     * settles exactly and stops costing work. */
    for (int seg = 0; seg < segments; seg++) {
        int n = frames - seg * AMINO_CTRL;
        if (n > AMINO_CTRL) n = AMINO_CTRL;
        float coef = 1.0f - expf(-(float)n /
                                 (AMINO_PARAM_SMOOTH_SECONDS * (float)in->sr));
        for (int i = 0; i < AMINO_LAYER_PARAM_COUNT; i++) {
            const AminoParamDef *d = &amino_layer_params[i];
            float *sm = (float *)((char *)smoothed + d->offset);
            float value = *(const float *)((const char *)target + d->offset);
            float diff = value - *sm;
            if (d->kind != AMINO_PK_FLOAT ||
                fabsf(diff) <= 1e-5f * (1.0f + fabsf(value)))
                *sm = value;
            else
                *sm += diff * coef;
        }
        rt->smooth_slices[seg] = *smoothed;
    }
}

static void render_voice(AminoInstance *in, AminoVoice *v,
                         float *dl, float *dr, float *f1l, float *f1r,
                         float *f2l, float *f2r, int frames) {
    int li = v->layer;
    if (li < 0 || li >= AMINO_MAX_LAYERS) { v->active = 0; return; }
    AminoLayerRt *rt = &in->lr[li];
    LayerParams *p = &rt->smooth_slices[0];
    float sr = (float)in->sr;
    int vidx = (int)(v - in->v);

    int route = (int)(p->route + 0.5f);
    if (route < 0) route = 0;
    if (route > 2) route = 2;
    if (route > 0 && in->fx[route - 1].type == 0) route = 0;
    float send_base = route == 0 ? 1.0f : am_clampf(p->send, 0.0f, 1.0f);
    float send = send_base;
    float *bl = route == 0 ? dl : (route == 1 ? f1l : f2l);
    float *br = route == 0 ? dr : (route == 1 ? f1r : f2r);

    int osc_mode = (int)(p->osc_mode + 0.5f);
    int vintage = osc_mode == 0;
    int xmod_type = (int)(p->xmod_type + 0.5f);
    int ftype = (int)(p->ftype + 0.5f);
    int drv_type = (int)(p->drv_type + 0.5f);
    int drv_pre = (int)(p->drv_pos + 0.5f) == 0;
    int dirt_type = (int)(p->dirt_type + 0.5f);
    int pan_mode = (int)(p->pan_mode + 0.5f);
    int nw1 = am_clampi((int)(p->osc1_wave + 0.5f), 0, 63);
    int nw2 = am_clampi((int)(p->osc2_wave + 0.5f), 0, 63);
    int link = p->osc2_link >= 0.5f;
    int t1 = rt->table1;
    int t2 = rt->table2;
    if (t1 < 0 || t1 >= AMINO_NUM_TABLES) t1 = 0;
    if (t2 < 0 || t2 >= AMINO_NUM_TABLES) t2 = 0;

    int over = vintage ? 4 : 1;
    float d1 = 0.0f, d2 = 0.0f;
    /* 5 Hz one-pole DC blocker: asymmetric waves and shapers (Pulse tables,
     * Tube, Fold, Ring dirt) otherwise put DC on the bus, which costs
     * headroom and thumps under the amp envelope. */
    const float dc_r = 1.0f - AMINO_TAU * 5.0f / sr;

    for (int base = 0; base < frames; base += AMINO_CTRL) {
        p = &rt->smooth_slices[base / AMINO_CTRL];
        send_base = route == 0 ? 1.0f : am_clampf(p->send, 0.0f, 1.0f);
        int n = frames - base;
        if (n > AMINO_CTRL) n = AMINO_CTRL;

        float e1_prev = v->env[0], e2_prev = v->env[1], e3_prev = v->env[2];
        float vel = v->vel;
        float at = v->poly_seen ? v->poly_at
                    : (v->mpe_ch >= 0 && v->mpe_ch < 16
                           ? in->mpe_at[v->mpe_ch] : in->chan_at);
        v->pressure = at;

        float src[27];
        for (int i = 0; i < 27; i++) src[i] = 0.0f;
        src[SRC_E1] = e1_prev;
        src[SRC_E2] = e2_prev;
        src[SRC_E3] = e3_prev;
        src[SRC_E4] = v->env[3];
        src[SRC_LFO1] = v->lfo[0].cur;
        src[SRC_LFO2] = v->lfo[1].cur;
        src[SRC_LFO1MW] = v->lfo[0].cur * in->modwheel;
        src[SRC_LFO2MW] = v->lfo[1].cur * in->modwheel;
        src[SRC_VEL] = vel;
        src[SRC_RELVEL] = v->release_vel;
        src[SRC_KEY] = ((float)v->note - 60.0f) / 60.0f;
        src[SRC_POLYAT] = at;
        src[SRC_CHANAT] = v->mpe_ch >= 0 ? at : in->chan_at;
        src[SRC_MW] = in->modwheel;
        src[SRC_BREATH] = in->breath;
        src[SRC_FOOT] = in->foot;
        src[SRC_EXPR] = in->expr;
        {
            float bend = in->bend;
            if (in->g.mpe >= 0.5f && v->mpe_ch >= 0 && v->mpe_ch < 16)
                bend = in->mpe_bend[v->mpe_ch];
            src[SRC_BEND] = bend;
        }
        src[SRC_TIMBRE] = v->mpe_ch >= 0 ? in->mpe_timbre[v->mpe_ch] : in->timbre;
        src[SRC_SUSTAIN] = in->sustain ? 1.0f : 0.0f;
        src[SRC_RAND] = v->rand;
        src[SRC_RANDBI] = v->rand_bi;
        src[SRC_VOICE] = ((float)(vidx % 8)) / 3.5f - 1.0f;
        src[SRC_ALT] = v->layer_alt;
        src[SRC_CONST] = 1.0f;
        src[SRC_FLAV] = v->flav_src;

        float mdst[26];
        for (int i = 0; i < 26; i++) mdst[i] = 0.0f;

        /* Pass 1: routes that neither come from nor are scaled by an LFO.
         * Their LFO rate/depth targets feed the LFO update below. */
        for (int s = 0; s < rt->count; s++) {
            int so = rt->src[s];
            if (is_lfo_src(so) || is_lfo_src(rt->via[s])) continue;
            float amt = *(const float *)((const char *)p + rt->amt_offset[s]);
            if (rt->via[s]) {
                float via_depth = *(const float *)((const char *)p + rt->via_depth_offset[s]);
                amt *= am_via_multiplier(src[rt->via[s]], via_depth);
            }
            mdst[rt->dst[s]] += amt * src[so];
        }

        /* Dedicated Aux Env routes (docs/04 section 3.10) join pass 1, so
         * they can reach the LFO rate/depth targets like a matrix slot. */
        {
            int d1 = am_clampi((int)(p->e4_dst1 + 0.5f), 0, 25);
            int d2 = am_clampi((int)(p->e4_dst2 + 0.5f), 0, 25);
            if (d1) mdst[d1] += p->e4_amt1 * src[SRC_E4];
            if (d2) mdst[d2] += p->e4_amt2 * src[SRC_E4];
        }

        /* LFO depth targets scale their own slots. */
        float depth1 = mdst[DST_LFO1D] > -0.99f ? 1.0f + mdst[DST_LFO1D] : 0.0f;
        float depth2 = mdst[DST_LFO2D] > -0.99f ? 1.0f + mdst[DST_LFO2D] : 0.0f;

        for (int k = 0; k < 2; k++) {
            int shape = (int)((k == 0 ? p->lfo1_shape : p->lfo2_shape) + 0.5f);
            int phase_mode = (int)((k == 0 ? p->lfo1_phase : p->lfo2_phase) + 0.5f);
            int sync = (k == 0 ? p->lfo1_sync : p->lfo2_sync) >= 0.5f;
            float rate_mod = (k == 0 ? mdst[DST_LFO1R] : mdst[DST_LFO2R]) * 0.5f;
            float hz;
            if (sync) {
                int d = (int)((k == 0 ? p->lfo1_div : p->lfo2_div) + 0.5f);
                hz = in->bpm / 60.0f / div_beats(d);
                if (rate_mod != 0.0f) hz *= exp2f(rate_mod * 4.0f);
            } else {
                hz = am_lfo_hz((k == 0 ? p->lfo1_rate : p->lfo2_rate) + rate_mod);
            }
            if (phase_mode == 2) {
                /* Global synced LFOs follow the host beat, not instance age. */
                if (sync && in->sync_transport) {
                    int d = (int)((k == 0 ? p->lfo1_div : p->lfo2_div) + 0.5f);
                    double beat = in->sync_beat + (double)base * (double)in->bpm /
                                  (60.0 * (double)in->sr);
                    double rate = rate_mod != 0.0f ? (double)exp2f(rate_mod * 4.0f) : 1.0;
                    v->lfo[k].phase = (float)fmod(beat / (double)div_beats(d) * rate, 1.0);
                } else {
                    /* double: a float clock stalls the phase after hours */
                    v->lfo[k].phase = (float)fmod((double)(in->clock + (uint64_t)base) *
                                                  (double)hz / (double)in->sr, 1.0);
                }
            }
            float val = lfo_step(&v->lfo[k], hz, shape, n, in->sr, &v->rng);
            float fade = k == 0 ? p->lfo1_fade : p->lfo2_fade;
            if (fade > 0.0f) {
                float tn = (float)(in->clock - v->lfo_t0) / sr;
                float ft = am_env_time(fade);
                if (tn < ft) val *= tn / ft;
            }
            if ((k == 0 ? p->lfo1_mw : p->lfo2_mw) >= 0.5f)
                val *= in->modwheel;
            src[SRC_LFO1 + k] = val;
            src[SRC_LFO1MW + k] = val * in->modwheel;
        }

        /* Pass 2: LFO-sourced or LFO-scaled slots, now that the LFO values
         * exist. LFO depth scales the slots the LFO is the source of. */
        for (int s = 0; s < rt->count; s++) {
            int so = rt->src[s];
            if (!is_lfo_src(so) && !is_lfo_src(rt->via[s])) continue;
            float amt = *(const float *)((const char *)p + rt->amt_offset[s]);
            if (is_lfo_src(so)) {
                int k = (so == SRC_LFO1 || so == SRC_LFO1MW) ? 0 : 1;
                amt *= k == 0 ? depth1 : depth2;
            }
            if (rt->via[s]) {
                float via_depth = *(const float *)((const char *)p + rt->via_depth_offset[s]);
                amt *= am_via_multiplier(src[rt->via[s]], via_depth);
            }
            mdst[rt->dst[s]] += amt * src[so];
        }

        /* Envelopes. */
        v->env[0] = env_step(&v->env_stage[0], &v->env[0],
                             p->e1_a, p->e1_d, p->e1_s, p->e1_r,
                             mdst[DST_E1A], mdst[DST_E1D], 0.0f, n, sr);
        v->env[1] = env_step(&v->env_stage[1], &v->env[1],
                             p->e2_a, p->e2_d, p->e2_s, p->e2_r,
                             0.0f, mdst[DST_E2D], 0.0f, n, sr);
        v->env[2] = env_step(&v->env_stage[2], &v->env[2],
                             p->e3_a, p->e3_d, p->e3_s, p->e3_r,
                             0.0f, mdst[DST_E3D], mdst[DST_E3R], n, sr);
        /* Aux envelope: a free modulation source, no dedicated destination. */
        v->env[3] = env_step(&v->env_stage[3], &v->env[3],
                             p->e4_a, p->e4_d, p->e4_s, p->e4_r,
                             0.0f, 0.0f, 0.0f, n, sr);
        if (v->env_stage[2] == AMINO_ENV_OFF && v->fade_total <= 0) {
            v->active = 0;
            return;
        }

        float e1 = v->env[0], e2 = v->env[1], e3 = v->env[2];

        /* Glide. */
        float gl = am_clampf(p->glide + mdst[DST_GLIDE], 0.0f, 1.0f);
        if (gl > 0.0f && v->pitch != v->target) {
            float c = 1.0f - expf(-(float)n / (am_glide_time(gl) / 4.6f * sr));
            v->pitch += (v->target - v->pitch) * c;
            if (fabsf(v->pitch - v->target) < 0.001f) v->pitch = v->target;
        } else {
            v->pitch = v->target;
        }

        float bend = src[SRC_BEND];
        float bend_st = bend * (in->g.mpe >= 0.5f && v->mpe_ch >= 0 ? 48.0f
                                                                    : in->g.bend_range);
        float vib = p->at_vib * at * src[SRC_LFO1] * 1.5f;
        /* LFO to Pitch knobs: a dedicated route each, scaled by LFO depth. */
        vib += (p->lfo1_pitch * depth1 * src[SRC_LFO1]
                + p->lfo2_pitch * depth2 * src[SRC_LFO2]) * AMINO_LFO_PITCH_RANGE;
        float pitch_common = v->pitch + v->flav_pitch + vib + bend_st
                             + mdst[DST_PITCH] * AMINO_PITCH_RANGE;

        float xa = am_clampf(p->xmod + mdst[DST_XMOD], 0.0f, 1.0f);
        float sync_up = xmod_type == 3 ? xa * AMINO_SYNC_RANGE : 0.0f;
        float base1 = pitch_common + mdst[DST_OSC1P] * AMINO_PITCH_RANGE + sync_up;
        float base2 = pitch_common + mdst[DST_OSC2P] * AMINO_PITCH_RANGE;
        if (p->osc1_kbd < 0.5f) base1 = 60.0f + (base1 - v->pitch);
        if (p->osc2_kbd < 0.5f) base2 = 60.0f + (base2 - v->pitch);
        float f1 = am_note_hz(base1 + (float)(int)p->osc1_semi + p->osc1_fine / 100.0f);
        float f2 = am_note_hz(base2 + (float)(int)p->osc2_semi + p->osc2_fine / 100.0f);
        if (f1 < 0.01f) f1 = 0.01f;
        if (f2 < 0.01f) f2 = 0.01f;

        float at_w = p->at_wave * at * AMINO_WAVE_RANGE;
        float w1f = p->osc1_wave + v->flav_wave + at_w
                    + p->osc1_wenv * e1 * AMINO_WAVE_RANGE
                    + p->osc1_vel * vel * AMINO_WAVE_RANGE
                    + mdst[DST_OSC1W] * AMINO_WAVE_RANGE;
        float w2f;
        if (link) w2f = w1f;
        else w2f = p->osc2_wave + v->flav_wave + at_w
                   + p->osc2_wenv * e1 * AMINO_WAVE_RANGE
                   + p->osc2_vel * vel * AMINO_WAVE_RANGE
                   + mdst[DST_OSC2W] * AMINO_WAVE_RANGE;
        int base_w1 = am_clampi((int)(p->osc1_wave + 0.5f), 0, 63);
        int base_w2 = link ? base_w1 : am_clampi((int)(p->osc2_wave + 0.5f), 0, 63);
        if (base_w1 >= 61) nw1 = base_w1 > 63 ? 63 : base_w1;
        else nw1 = am_clampi((int)w1f, 0, 60);
        if (base_w2 >= 61) nw2 = base_w2 > 63 ? 63 : base_w2;
        else nw2 = am_clampi((int)w2f, 0, 60);

        float lv1 = p->osc1_level + mdst[DST_OSC1L];
        float lv2 = p->osc2_level + mdst[DST_OSC2L];
        if (lv1 < 0.0f) lv1 = 0.0f;
        if (lv2 < 0.0f) lv2 = 0.0f;
        float dlvl = p->dirt + mdst[DST_DIRT];
        dlvl = am_clampf(dlvl, 0.0f, 1.0f);

        float cut = p->cutoff + p->fenv * e2 * AMINO_CUTOFF_RANGE
                    + p->fkbd * (v->pitch - 60.0f) + p->fvel * vel * 48.0f
                    + p->at_cut * at * AMINO_CUTOFF_RANGE + v->flav_cut
                    + mdst[DST_CUT] * AMINO_CUTOFF_RANGE;
        float fc = am_note_hz(cut);
        if (fc < 20.0f) fc = 20.0f;
        if (fc > 18000.0f) fc = 18000.0f;
        float g = tanf((float)M_PI * fc / sr);
        float res = am_clampf(p->res + mdst[DST_RES], 0.0f, 1.0f);
        float G = g / (1.0f + g);
        float G2 = G * G, G3 = G2 * G, G4 = G2 * G2;
        float one_g = 1.0f - G;
        float k4 = res * 3.9f;
        float Rsvf = 1.0f - res * 0.97f;
        float km = res * 4.5f;               /* Moog ladder feedback; >4 self-oscillates */
        float mgain = 1.0f + 0.3f * km;      /* partial passband make-up */
        float hsvf = 1.0f / (1.0f + 2.0f * Rsvf * g + g * g);
        /* Loop invariants of the ladder solves, so the sample loop has no
         * divide beyond the tanh. */
        float inv_k4 = 1.0f / (1.0f + k4 * G4);
        float inv_km = 1.0f / (1.0f + km * G4);
        float cz1 = G3 * one_g, cz2 = G2 * one_g, cz3 = G * one_g;

        float drv = p->drive + mdst[DST_DRIVE];
        drv = am_clampf(drv, 0.0f, 1.0f);
        float dgain = 1.0f + 15.0f * drv;
        float dcomp = 1.0f / (1.0f + drv);
        int use_drive = drv_type != 0 && drv > 0.0f;

        float amp_lvl = (1.0f - p->amp_vel * (1.0f - vel)) * p->vol
                        * AMINO_VOICE_GAIN;
        float al = 1.0f + mdst[DST_AMP];
        if (al < 0.0f) al = 0.0f;
        amp_lvl *= al;
        float pan = v->pan;
        if (pan_mode == 4 || pan_mode == 5) pan = src[pan_mode == 4 ? SRC_LFO1 : SRC_LFO2] * p->pan;
        pan = am_clampf(pan + mdst[DST_PAN], -1.0f, 1.0f);
        if (pan != v->pan_in) {
            am_pan_gains(pan, &v->pan_gl, &v->pan_gr);
            v->pan_in = pan;
        }

        float dp1 = f1 * 128.0f / (sr * (float)over);
        float dp2 = f2 * 128.0f / (sr * (float)over);
        /* Pitch modulation can put the oscillator far above the sample rate.
         * Reduce the phase increment modulo one table cycle before indexing;
         * a single wrap in the inner loop is then sufficient at every pitch. */
        d1 = dp1 >= 128.0f ? fmodf(dp1, 128.0f) : dp1;
        d2 = dp2 >= 128.0f ? fmodf(dp2, 128.0f) : dp2;
        /* FX Send modulates the send level; a dry-routed layer has none. */
        send = route == 0 ? 1.0f : send_base * (1.0f + mdst[DST_SEND]);
        if (send < 0.0f) send = 0.0f;
        if (send > 2.0f) send = 2.0f;

        /* Ramp the whole output gain from where the last slice ended, so
         * amp, pan and send modulation never step at the slice boundary. */
        float a1 = e3 * amp_lvl * send;
        float gain_l1 = a1 * v->pan_gl, gain_r1 = a1 * v->pan_gr;
        float gain_l = v->out_gl, gain_r = v->out_gr;
        float dgain_l = (gain_l1 - gain_l) / (float)n;
        float dgain_r = (gain_r1 - gain_r) / (float)n;

        float p1 = v->p1, p2 = v->p2;
        const signed char *wv1 = amino_wavetable_data[t1][v->w1 & 63];
        const signed char *wv2 = amino_wavetable_data[t2][v->w2 & 63];
        const signed char *nwv1 = amino_wavetable_data[t1][nw1];
        const signed char *nwv2 = amino_wavetable_data[t2][nw2];
        float pm = xmod_type == 1 ? AMINO_PM_RANGE * xa * xa * 128.0f : 0.0f;
        float amd = xmod_type == 2 ? xa : 0.0f;
        int hard = xmod_type == 3;
        int xm = hard || (xmod_type != 0 && xa > 0.0f);
        int use1 = lv1 > 0.0f;
        int use2 = lv2 > 0.0f || dirt_type == 5 || xm;
        float z1 = v->z[0], z2 = v->z[1], z3 = v->z[2], z4 = v->z[3];

        for (int i = 0; i < n; i++) {
            float o1 = 0.0f, o2 = 0.0f;
            if (xm) {
                float acc1 = 0.0f, acc2 = 0.0f;
                for (int os = 0; os < over; os++) {
                    float s2;
                    if (vintage) s2 = (float)wv2[(int)p2];
                    else {
                        int j = (int)p2;
                        float fr = p2 - (float)j;
                        s2 = (float)wv2[j] + ((float)wv2[(j + 1) & 127] - (float)wv2[j]) * fr;
                    }
                    /* Table storage is signed 8-bit; DSP signals are normalized
                     * to [-1, 1]. Keep this normalized value for PM/AM math. */
                    float s2n = s2 * (1.0f / 128.0f);
                    p2 += d2;
                    int wrapped = 0;
                    if (p2 >= 128.0f) { p2 -= 128.0f; wv2 = nwv2; v->w2 = nw2; wrapped = 1; }
                    float q = p1;
                    if (pm != 0.0f) q = am_wrap128(q + s2n * pm);
                    float s1;
                    if (vintage) s1 = (float)wv1[(int)q];
                    else {
                        int j = (int)q;
                        float fr = q - (float)j;
                        s1 = (float)wv1[j] + ((float)wv1[(j + 1) & 127] - (float)wv1[j]) * fr;
                    }
                    if (amd != 0.0f) s1 *= 1.0f - amd * (0.5f - 0.5f * s2n);
                    p1 += d1;
                    if (p1 >= 128.0f) { p1 -= 128.0f; wv1 = nwv1; v->w1 = nw1; }
                    if (hard && wrapped) {
                        if (d2 > 1e-9f) p1 = p2 * d1 / d2;
                        wv1 = nwv1;
                        v->w1 = nw1;
                    }
                    acc1 += s1;
                    acc2 += s2;
                }
                o1 = acc1 / (float)over;
                o2 = acc2 / (float)over;
            } else {
                if (use1) {
                    if (vintage) {
                        float s = (float)wv1[(int)p1]; p1 += d1;
                        if (p1 >= 128.0f) { p1 -= 128.0f; wv1 = nwv1; v->w1 = nw1; }
                        s += (float)wv1[(int)p1]; p1 += d1;
                        if (p1 >= 128.0f) { p1 -= 128.0f; wv1 = nwv1; v->w1 = nw1; }
                        s += (float)wv1[(int)p1]; p1 += d1;
                        if (p1 >= 128.0f) { p1 -= 128.0f; wv1 = nwv1; v->w1 = nw1; }
                        s += (float)wv1[(int)p1]; p1 += d1;
                        if (p1 >= 128.0f) { p1 -= 128.0f; wv1 = nwv1; v->w1 = nw1; }
                        o1 = s * 0.25f;
                    } else {
                        int j = (int)p1;
                        float fr = p1 - (float)j;
                        float a = (float)wv1[j];
                        o1 = a + ((float)wv1[(j + 1) & 127] - a) * fr;
                        p1 += d1;
                        if (p1 >= 128.0f) { p1 -= 128.0f; wv1 = nwv1; v->w1 = nw1; }
                    }
                }
                if (use2) {
                    if (vintage) {
                        float s = (float)wv2[(int)p2]; p2 += d2;
                        if (p2 >= 128.0f) { p2 -= 128.0f; wv2 = nwv2; v->w2 = nw2; }
                        s += (float)wv2[(int)p2]; p2 += d2;
                        if (p2 >= 128.0f) { p2 -= 128.0f; wv2 = nwv2; v->w2 = nw2; }
                        s += (float)wv2[(int)p2]; p2 += d2;
                        if (p2 >= 128.0f) { p2 -= 128.0f; wv2 = nwv2; v->w2 = nw2; }
                        s += (float)wv2[(int)p2]; p2 += d2;
                        if (p2 >= 128.0f) { p2 -= 128.0f; wv2 = nwv2; v->w2 = nw2; }
                        o2 = s * 0.25f;
                    } else {
                        int j = (int)p2;
                        float fr = p2 - (float)j;
                        float a = (float)wv2[j];
                        o2 = a + ((float)wv2[(j + 1) & 127] - a) * fr;
                        p2 += d2;
                        if (p2 >= 128.0f) { p2 -= 128.0f; wv2 = nwv2; v->w2 = nw2; }
                    }
                }
            }

            /* The emulator and the wavetable format use signed 8-bit samples
             * normalized by 128 before mixing, filtering, or ring modulation. */
            o1 *= 1.0f / 128.0f;
            o2 *= 1.0f / 128.0f;
            float x = o1 * lv1 + (dirt_type == 5 ? 0.0f : o2 * lv2);
            if (dlvl > 0.0f) {
                float r;
                switch (dirt_type) {
                case 0:
                    r = am_rand_unit(&v->rng) * 2.0f - 1.0f;
                    x += r * dlvl;
                    break;
                case 1:
                    if (am_rand_unit(&v->rng) < 0.002f + 0.02f * dlvl) {
                        r = am_rand_unit(&v->rng) * 2.0f - 1.0f;
                        x += r * 2.0f * dlvl;
                    }
                    break;
                case 2:
                    if (am_rand_unit(&v->rng) < 0.0008f + 0.004f * dlvl)
                        v->burst_env = 1.0f;
                    if (v->burst_env > 0.001f) {
                        r = am_rand_unit(&v->rng) * 2.0f - 1.0f;
                        x += r * v->burst_env * dlvl * 2.0f;
                        v->burst_env *= 0.93f;
                    }
                    break;
                case 3:
                    if (v->click_left > 0) {
                        r = am_rand_unit(&v->rng) * 2.0f - 1.0f;
                        x += r * dlvl * 2.0f * (float)v->click_left
                             / (0.004f * sr);
                        v->click_left--;
                    }
                    break;
                case 4:
                    if (v->burst_env <= 0.001f && am_rand_unit(&v->rng) < 0.0004f)
                        v->burst_env = 1.0f;
                    if (v->burst_env > 0.001f) {
                        r = am_rand_unit(&v->rng) * 2.0f - 1.0f;
                        x += r * v->burst_env * dlvl;
                        v->burst_env *= 0.9997f;
                    }
                    break;
                default:
                    x += o1 * o2 * dlvl * 2.0f;
                    break;
                }
            }
            x *= 0.5f;

            if (use_drive && drv_pre)
                x = shaper(drv_type, x * dgain) * dcomp;

            if (ftype == FT_LP24 || ftype == FT_HP24) {
                float S = cz1 * z1 + cz2 * z2 + cz3 * z3 + one_g * z4;
                float y4 = (G4 * x + S) * inv_k4;
                float u = am_tanh((x - k4 * y4) * (1.0f + 0.5f * k4) * 0.8f) * 1.25f;
                float vv = (u - z1) * G; float y1 = vv + z1; z1 = y1 + vv;
                vv = (y1 - z2) * G; float y2 = vv + z2; z2 = y2 + vv;
                vv = (y2 - z3) * G; float y3 = vv + z3; z3 = y3 + vv;
                vv = (y3 - z4) * G; y4 = vv + z4; z4 = y4 + vv;
                x = ftype == FT_LP24 ? y4 : (u - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4);
            } else if (ftype == FT_LADDER) {
                /* Ladder 24dB (Moog-style): four zero-delay-feedback one-pole stages with a
                 * tanh at every stage input (Huovilainen's improved model, in
                 * trapezoidal form after Zavalishin).  The delay-free feedback
                 * loop is solved from the linear response, so the cost is
                 * five cheap tanh per sample and no oversampling.  Unlike the
                 * LP24 above, the passband sags as resonance rises (only a
                 * partial make-up), and the top of the Res range self-oscillates. */
                float S = cz1 * z1 + cz2 * z2 + cz3 * z3 + one_g * z4;
                float xin = x * mgain + 1e-4f;      /* -80 dB kick so k > 4 starts ringing */
                float y4e = (G4 * xin + S) * inv_km;
                float u = moog_sat(xin - km * y4e);
                float vv = (u - z1) * G; float y1 = vv + z1; z1 = y1 + vv;
                vv = (moog_sat(y1) - z2) * G; float y2 = vv + z2; z2 = y2 + vv;
                vv = (moog_sat(y2) - z3) * G; float y3 = vv + z3; z3 = y3 + vv;
                vv = (moog_sat(y3) - z4) * G; y4e = vv + z4; z4 = y4e + vv;
                x = y4e;
            } else if (ftype == FT_LP12 || ftype == FT_HP12 || ftype == FT_BP12) {
                float hp = (x - (2.0f * Rsvf + g) * z1 - z2) * hsvf;
                float bp = g * hp + z1; z1 = g * hp + bp;
                float lp = g * bp + z2; z2 = g * bp + lp;
                if (ftype == FT_LP12) x = lp;
                else if (ftype == FT_HP12) x = hp;
                else x = bp * 2.0f * Rsvf;
            }

            if (use_drive && !drv_pre)
                x = shaper(drv_type, x * dgain) * dcomp;

            float dc_y = x - v->dc_x + dc_r * v->dc_y;
            v->dc_x = x;
            v->dc_y = dc_y;
            x = dc_y;

            gain_l += dgain_l;
            gain_r += dgain_r;
            if (v->fade_total > 0) {
                /* Steal fade: once it reaches zero the old note stays silent
                 * for the rest of the block; the pending note starts after. */
                x *= (float)v->fade / (float)v->fade_total;
                if (v->fade > 0) v->fade--;
            }
            bl[base + i] += x * gain_l;
            br[base + i] += x * gain_r;
        }
        v->out_gl = gain_l1;
        v->out_gr = gain_r1;

        v->p1 = p1;
        v->p2 = p2;
        v->z[0] = z1; v->z[1] = z2; v->z[2] = z3; v->z[3] = z4;
    }

    if (v->fade_total > 0 && v->fade <= 0 && v->pending_note < 0) {
        /* Faded out with nothing queued (see amino_release_voices_from). */
        cancel_pending_note(v);
        v->active = 0;
        v->gate = 0;
        return;
    }
    if (v->fade_total > 0 && v->fade <= 0) {
        int li2 = v->pending_layer;
        if (li2 < 0 || li2 >= 4) li2 = li;
        int sustained = v->sustained;
        voice_start_now(in, v, li2, v->pending_note, v->pending_root,
                        v->pending_vel, 0, v->pending_ch);
        v->sustained = sustained;
    }
}

void amino_render_voices(AminoInstance *in, float *dl, float *dr,
                         float *f1l, float *f1r, float *f2l, float *f2r,
                         int frames) {
    if (frames <= 0) return;
    if (frames > AMINO_BLOCK) frames = AMINO_BLOCK;
    int in_use[AMINO_MAX_LAYERS] = { 0 };
    for (int i = 0; i < AMINO_MAX_VOICES; i++)
        if (in->v[i].active && in->v[i].layer >= 0 &&
            in->v[i].layer < AMINO_MAX_LAYERS)
            in_use[in->v[i].layer] = 1;
    for (int li = 0; li < AMINO_MAX_LAYERS; li++)
        smooth_layer_params(in, li, frames, in_use[li]);
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        if (!v->active) continue;
        render_voice(in, v, dl, dr, f1l, f1r, f2l, f2r, frames);
    }
    in->clock += (uint64_t)frames;
    /* Filter state denormal guard. */
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        for (int k = 0; k < 4; k++)
            if (fabsf(v->z[k]) < 1e-20f) v->z[k] = 0.0f;
        if (fabsf(v->dc_y) < 1e-20f) v->dc_y = 0.0f;
    }
}

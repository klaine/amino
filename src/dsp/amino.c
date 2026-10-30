/* Schwung v2 entry point and the block-level routing/master section. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "amino.h"
#include "util.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef AMINO_PROFILE
#include <time.h>
#endif

#if defined(__linux__)
#include "move_info.h"
#else
/* Native Windows tests have no Move host-info export. Keep the small prefix
 * used below available so they exercise the documented C-minor fallback. */
typedef struct {
    uint8_t valid;
    int8_t root_note;
    char scale[24];
} move_info_t;
static int move_info_read(move_info_t *out) { (void)out; return 0; }
#endif

static AminoInstance g_pool[AMINO_NUM_SLOTS];
static const host_api_v1_t *g_host;

static void update_move_scale(AminoInstance *in) {
    move_info_t mi;
    memset(&mi, 0, sizeof mi);
    if (!move_info_read(&mi) || !mi.valid) return;
    /* Nearly every block sees the same set: skip the scale-name compares. */
    if (mi.root_note == in->move_scale_root &&
        memcmp(mi.scale, in->move_scale_name, sizeof in->move_scale_name) == 0)
        return;
    in->move_scale_root = mi.root_note;
    memcpy(in->move_scale_name, mi.scale, sizeof in->move_scale_name);
    amino_move_scale_update(in, mi.root_note, mi.scale);
}

/* ------------------------------------------------------------------ create */

static void *amino_create(const char *module_dir, const char *json_defaults) {
    (void)module_dir;
    (void)json_defaults;
    AminoInstance *in = 0;
    for (int i = 0; i < AMINO_NUM_SLOTS; i++) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&g_pool[i].used, &expected, 1, 0,
                                        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            in = &g_pool[i];
            break;
        }
    }
    if (!in) return 0;

    /* fx_arena is the last member: this clears every other field, so state
     * from the slot's previous instance never leaks into this one. */
    memset(in, 0, offsetof(AminoInstance, fx_arena));
    in->used = 1;
    in->host = g_host;
    in->sr = (g_host && g_host->sample_rate > 0) ? g_host->sample_rate : 44100;
    /* Preserve the old C-minor fallback on hosts that do not publish Move
     * set info. The first move_info_read is deliberately in create_instance:
     * Schwung resolves its export here, not on the audio callback. */
    in->move_root_note = 0;
    in->move_scale_mask = 0x05AD;
    update_move_scale(in);
    in->bpm = 120.0f;
    if (g_host && g_host->get_bpm) {
        float b = g_host->get_bpm();
        if (b >= 20.0f && b <= 400.0f) in->bpm = b;
    }
    in->rui_bpm = in->bpm;
    for (int i = 0; i < AMINO_AUDITION_NOTES; i++) in->aud_note[i] = -1;
    in->sm_master = -1.0f;
    /* Preset zero is the initial patch. Preset loads reseed from the preset's
     * own seed, so random streams are repeatable per (preset, voice) on every
     * slot and do not change when the bank is reordered. */
    in->seed = 1u;
    in->rng = in->seed;
    in->last_rr = (uint32_t)-1;

    for (int i = 0; i < AMINO_MAX_VOICES; i++) amino_voice_init(in, i);
    for (int s = 0; s < 2; s++) amino_fx_init(in, s);
    amino_params_reset(in);
    in->preset_index = 0;
    return in;
}

static void amino_destroy(void *instance) {
    AminoInstance *in = (AminoInstance *)instance;
    if (!in) return;
    amino_kill_all(in, 1);
    for (int s = 0; s < 2; s++) {
        in->fx[s].type = 0;
        in->fx[s].need_clear = 0;
    }
    __atomic_store_n(&in->used, 0, __ATOMIC_RELEASE);
}

/* ------------------------------------------------------------------ midi */

static void on_midi(void *instance, const uint8_t *msg, int len, int source) {
    (void)source;
    AminoInstance *in = (AminoInstance *)instance;
    if (!in || !msg || len < 2) return;
    uint8_t status = msg[0];
    if (status < 0x80) return;
    int type = status & 0xF0;
    int ch = status & 0x0F;

    switch (type) {
    case 0x90:
        if (len < 3) return;
        if (msg[2] == 0) amino_note_off(in, msg[1], ch);
        else amino_note_on(in, msg[1], (float)msg[2] / 127.0f, ch);
        break;
    case 0x80:
        if (len < 3) return;
        amino_note_off(in, msg[1], ch);
        break;
    case 0xA0:
        if (len < 3) return;
        amino_poly_at(in, msg[1], (float)msg[2] / 127.0f, ch);
        break;
    case 0xB0: {
        if (len < 3) return;
        int cc = msg[1], v = msg[2];
        float f = (float)v / 127.0f;
        switch (cc) {
        case 1: in->modwheel = f; break;
        case 2: in->breath = f; break;
        case 4: in->foot = f; break;
        case 11: in->expr = f; break;
        case 64: amino_sustain(in, v >= 64); break;
        case 74:
            in->timbre = f;
            if (in->g.mpe >= 0.5f) in->mpe_timbre[ch] = f;
            break;
        case 120: amino_kill_channel(in, ch, 1); break;
        case 123: amino_kill_channel(in, ch, 0); break;
        case 121:
            in->modwheel = in->breath = in->foot = in->expr = 0.0f;
            in->bend = 0.0f;
            in->timbre = 0.0f;
            for (int i = 0; i < 16; i++) {
                in->mpe_bend[i] = 0.0f;
                in->mpe_timbre[i] = 0.0f;
            }
            amino_sustain(in, 0);
            break;
        default: break;
        }
        break;
    }
    case 0xD0: {
        float f = (float)msg[1] / 127.0f;
        if (in->g.mpe >= 0.5f) {
            in->mpe_at[ch] = f;
            for (int i = 0; i < AMINO_MAX_VOICES; i++) {
                AminoVoice *v = &in->v[i];
                if (v->active && v->mpe_ch == ch) {
                    v->pressure = f;
                }
            }
        } else {
            amino_chan_at(in, f);
        }
        break;
    }
    case 0xE0: {
        if (len < 3) return;
        int raw = ((int)msg[2] << 7) | msg[1];
        float bend = ((float)raw - 8192.0f) / 8192.0f;
        if (in->g.mpe >= 0.5f) in->mpe_bend[ch] = bend;
        else in->bend = bend;
        break;
    }
    default: break;
    }
}

/* ------------------------------------------------------------------ remote ui */

/* Channel an audition note uses: in Chan mode the focused layer's own MIDI
 * channel, so the note reaches that layer; otherwise channel 1. */
static int audition_channel(AminoInstance *in) {
    if ((int)(in->g.layer_mode + 0.5f) != 4) return 0;
    int li = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    return am_clampi((int)(in->l[li].midi_ch + 0.5f), 1, 16) - 1;
}

static void audition_release(AminoInstance *in, int slot) {
    if (in->aud_note[slot] < 0) return;
    amino_note_off(in, in->aud_note[slot], in->aud_ch[slot]);
    in->aud_note[slot] = -1;
    in->aud_left[slot] = 0;
}

/* set_param("audition", "note:vel"): vel 1..127 plays, 0 releases; "off"
 * releases every audition note.  Notes still held after
 * AMINO_AUDITION_TIMEOUT_S are released by render_block, so a browser that
 * vanishes mid-note cannot leave one hanging. */
void amino_audition(AminoInstance *in, const char *val) {
    if (strcmp(val, "off") == 0) {
        for (int i = 0; i < AMINO_AUDITION_NOTES; i++) audition_release(in, i);
        return;
    }
    char *end = 0;
    long note = strtol(val, &end, 10);
    if (!end || *end != ':' || note < 0 || note > 127) return;
    long vel = strtol(end + 1, 0, 10);
    if (vel < 0) vel = 0;
    if (vel > 127) vel = 127;
    int slot = -1, free_slot = -1;
    for (int i = 0; i < AMINO_AUDITION_NOTES; i++) {
        if (in->aud_note[i] == note) slot = i;
        else if (in->aud_note[i] < 0 && free_slot < 0) free_slot = i;
    }
    if (vel == 0) {
        if (slot >= 0) audition_release(in, slot);
        return;
    }
    if (slot >= 0) audition_release(in, slot);   /* retrigger */
    else if (free_slot >= 0) slot = free_slot;
    else { audition_release(in, 0); slot = 0; }  /* all busy: drop the first */
    in->aud_note[slot] = (int8_t)note;
    in->aud_ch[slot] = (uint8_t)audition_channel(in);
    in->aud_left[slot] = AMINO_AUDITION_TIMEOUT_S * in->sr;
    amino_note_on(in, (int)note, (float)vel / 127.0f, in->aud_ch[slot]);
}

/* get_param("rui_live"): a small snapshot for the Remote UI, declared as a
 * viz extra key so the manager pushes it when it changes.
 *   v=total,l1,l2,l3,l4        active voices
 *   l=ph1,cur1,ph2,cur2        LFO phase and held random value of the focused
 *                              layer's newest voice ("-" without one)
 *   e=st,lv x4                 Wave/Filter/Amp/Aux envelope stage and level
 *   p=peak                     output peak, ~300 ms decay
 *   b=bpm                      tempo the synced LFOs follow
 *   t=ms                       instance clock, to time-align the above */
int amino_live(AminoInstance *in, char *buf, int len) {
    int count[4] = { 0, 0, 0, 0 }, total = 0;
    int focus = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    const AminoVoice *newest = 0;
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        const AminoVoice *v = &in->v[i];
        if (!v->active || v->layer < 0 || v->layer > 3) continue;
        count[v->layer]++;
        total++;
        if (v->layer == focus && (!newest || v->t_on > newest->t_on)) newest = v;
    }
    char lfo[64] = "-", env[96] = "-";
    if (newest) {
        snprintf(lfo, sizeof lfo, "%.3f,%.3f,%.3f,%.3f",
                 (double)newest->lfo[0].phase, (double)newest->lfo[0].cur,
                 (double)newest->lfo[1].phase, (double)newest->lfo[1].cur);
        snprintf(env, sizeof env, "%d,%.3f,%d,%.3f,%d,%.3f,%d,%.3f",
                 newest->env_stage[0], (double)newest->env[0],
                 newest->env_stage[1], (double)newest->env[1],
                 newest->env_stage[2], (double)newest->env[2],
                 newest->env_stage[3], (double)newest->env[3]);
    }
    int n = snprintf(buf, (size_t)len, "v=%d,%d,%d,%d,%d;l=%s;e=%s;p=%.3f;b=%.1f;t=%llu",
                     total, count[0], count[1], count[2], count[3], lfo, env,
                     (double)in->rui_peak, (double)in->bpm,
                     (unsigned long long)(in->clock * 1000ull / (uint64_t)in->sr));
    return (n < 0 || n >= len) ? -1 : n;
}

/* ------------------------------------------------------------------ params */

static void set_param(void *instance, const char *key, const char *val) {
    AminoInstance *in = (AminoInstance *)instance;
    if (!in) return;
    amino_param_set(in, key, val);
}

static int get_param(void *instance, const char *key, char *buf, int buf_len) {
    AminoInstance *in = (AminoInstance *)instance;
    if (!in) return -1;
#ifdef AMINO_PROFILE
    if (key && strcmp(key, "cpu_us") == 0) {
        int n = snprintf(buf, (size_t)buf_len, "%.1f", (double)in->cpu_us);
        return n < 0 || n >= buf_len ? -1 : n;
    }
#endif
    return amino_param_get(in, key, buf, buf_len);
}

static int get_error(void *instance, char *buf, int buf_len) {
    (void)instance;
    if (buf && buf_len > 0) buf[0] = 0;
    return 0;
}

/* ------------------------------------------------------------------ render */

static void render_chunk(AminoInstance *in, int16_t *out, int frames);

static void render_block(void *instance, int16_t *out, int frames) {
    AminoInstance *in = (AminoInstance *)instance;
    if (!in || !out || frames <= 0) return;
#ifdef AMINO_PROFILE
    struct timespec prof_t0, prof_t1;
    clock_gettime(CLOCK_MONOTONIC, &prof_t0);
#endif

    /* The set can change while the instrument remains loaded. After the first
     * create-time resolution this is a lock-free seqlock copy from Schwung. */
    update_move_scale(in);

    if (in->host && in->host->get_bpm) {
        float b = in->host->get_bpm();
        if (b >= 20.0f && b <= 400.0f) in->bpm = b;
    }
    in->rui_bpm = in->bpm;
    in->sync_transport = 0;
    in->rui_on = 0;
    if (in->host && in->host->get_clock_status &&
        in->host->get_clock_status() == MOVE_CLOCK_STATUS_RUNNING) {
        in->rui_on = 1;
        double beat = in->host->get_beat_position ? in->host->get_beat_position() : -1.0;
        if (beat >= 0.0) {
            in->sync_beat = beat;
            in->sync_transport = 1;
            in->rui_tick = beat * 24.0;
        }
        else in->rui_tick += (double)frames * 24.0 * (double)in->bpm /
                             (60.0 * (double)in->sr);
        in->rui_devms = (uint32_t)(in->clock * 1000ull / (uint64_t)in->sr);
    }

    /* The engine works in AMINO_BLOCK pieces; a longer host block is rendered
     * as consecutive pieces so every output frame is written. */
    double beats_per_frame = (double)in->bpm / (60.0 * (double)in->sr);
    while (frames > 0) {
        int n = frames > AMINO_BLOCK ? AMINO_BLOCK : frames;
        render_chunk(in, out, n);
        out += 2 * n;
        frames -= n;
        if (in->sync_transport) in->sync_beat += (double)n * beats_per_frame;
    }
#ifdef AMINO_PROFILE
    clock_gettime(CLOCK_MONOTONIC, &prof_t1);
    in->cpu_us += 0.05f * ((float)(prof_t1.tv_sec - prof_t0.tv_sec) * 1e6f
                           + (float)(prof_t1.tv_nsec - prof_t0.tv_nsec) * 1e-3f - in->cpu_us);
#endif
}

static void render_chunk(AminoInstance *in, int16_t *out, int frames) {
    for (int i = 0; i < AMINO_AUDITION_NOTES; i++) {
        if (in->aud_note[i] < 0) continue;
        in->aud_left[i] -= frames;
        if (in->aud_left[i] <= 0) audition_release(in, i);
    }

    float bus[3][2][AMINO_BLOCK];
    memset(bus, 0, sizeof bus);
    amino_render_voices(in, bus[0][0], bus[0][1], bus[1][0], bus[1][1],
                        bus[2][0], bus[2][1], frames);

    int serial = in->g.fx_chain >= 0.5f;
    if (!serial) {
        amino_fx_process(in, 0, bus[1][0], bus[1][1], frames);
        amino_fx_process(in, 1, bus[2][0], bus[2][1], frames);
    } else {
        amino_fx_process(in, 0, bus[1][0], bus[1][1], frames);
        for (int i = 0; i < frames; i++) {
            bus[2][0][i] += bus[1][0][i];
            bus[2][1][i] += bus[1][1][i];
        }
        amino_fx_process(in, 1, bus[2][0], bus[2][1], frames);
    }

    float vol = am_clampf(in->g.master_vol, 0.0f, 1.0f) *
                powf(10.0f, am_clampf(in->g.patch_gain, -24.0f, 24.0f) * 0.05f);
    if (in->sm_master < 0.0f) in->sm_master = vol;
    float g = in->sm_master;
    {
        float coef = 1.0f - expf(-(float)frames / (0.008f * (float)in->sr));
        in->sm_master += (vol - in->sm_master) * coef;
    }
    /* Ramp across the block rather than stepping at its start. */
    float dg = (in->sm_master - g) / (float)frames;
    float peak = in->rui_peak * expf(-(float)frames / (0.3f * (float)in->sr));

    for (int i = 0; i < frames; i++) {
        float l = bus[0][0][i];
        float r = bus[0][1][i];
        if (serial) {
            l += bus[2][0][i];
            r += bus[2][1][i];
        } else {
            l += bus[1][0][i] + bus[2][0][i];
            r += bus[1][1][i] + bus[2][1][i];
        }
        g += dg;
        l = am_tanh(l * g);           /* bounded to +-1, no clamp needed */
        r = am_tanh(r * g);
        float a = fabsf(l) > fabsf(r) ? fabsf(l) : fabsf(r);
        if (a > peak) peak = a;
        out[i * 2] = (int16_t)(l * 32767.0f);
        out[i * 2 + 1] = (int16_t)(r * 32767.0f);
    }
    in->rui_peak = peak;
}

/* ------------------------------------------------------------------ api */

static plugin_api_v2_t g_api = {
    MOVE_PLUGIN_API_VERSION_2,
    amino_create,
    amino_destroy,
    on_midi,
    set_param,
    get_param,
    get_error,
    render_block,
};

plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host) {
    g_host = host;
    return &g_api;
}

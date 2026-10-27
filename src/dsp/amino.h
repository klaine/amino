/* Amino - shared types, constants and the instance layout.
 *
 * Everything an audio callback touches lives in a statically allocated
 * AminoInstance.  There is no malloc anywhere in this module.
 */
#ifndef AMINO_H
#define AMINO_H

#include <stdint.h>
#include <stddef.h>

#include "plugin_api.h"
#include "params_gen.h"
#include "factory_presets.h"
#include "wavetables.h"

#define AMINO_MAX_VOICES 16
#define AMINO_AUDITION_NOTES 8
#define AMINO_AUDITION_TIMEOUT_S 30
#define AMINO_MAX_LAYERS 4
#define AMINO_BLOCK 128
#define AMINO_CTRL 16                 /* control-rate block, docs/03 section 10 */
#define AMINO_PARAM_SMOOTH_SECONDS 0.030f
#define AMINO_NUM_SLOTS 4             /* static instance pool */
#define AMINO_FX_ARENA 820000         /* bytes per FX slot, docs/11 section 3 */
#define AMINO_PRESET_NAME 48

#define AMINO_NUM_ENVS 4              /* Wave, Filter, Amp, Aux */
#define AMINO_VOICE_GAIN 0.9f
#define AMINO_PITCH_RANGE 48.0f
#define AMINO_CUTOFF_RANGE 72.0f
#define AMINO_WAVE_RANGE 63.0f
#define AMINO_PM_RANGE 1.5f
#define AMINO_SYNC_RANGE 36.0f
#define AMINO_LFO_PITCH_RANGE 12.0f   /* semitones at LFO to Pitch = 100% */

typedef enum {
    AMINO_ENV_OFF = 0,
    AMINO_ENV_ATTACK = 1,
    AMINO_ENV_DECAY = 2,
    AMINO_ENV_RELEASE = 3
} AminoEnvStage;

typedef struct {
    float phase;
    float prev, cur;
} AminoLfo;

typedef struct {
    int layer;                 /* 0..3 */
    int active, gate;
    int root_note, note;       /* root_note is the key that was pressed */
    int midi_ch;               /* MIDI channel that owns this voice */
    int mpe_ch;                /* -1 when not MPE */
    float vel, pressure, poly_at, release_vel;
    int poly_seen;
    float pitch, target;       /* note units (60 = C3) */
    float p1, p2;              /* oscillator phase, 0..128 samples */
    int w1, w2;                /* current wave slot, updated on cycle wrap */
    float z[4];
    float pan;
    uint64_t t_on;             /* sample clock at note-on */
    float rand, rand_bi, layer_alt;
    float flav_wave, flav_pitch, flav_cut, flav_src;
    float env[AMINO_NUM_ENVS];
    int env_stage[AMINO_NUM_ENVS];
    float burst_env;
    int click_left;
    AminoLfo lfo[2];
    uint64_t lfo_t0;           /* LFO fade-in origin, reset with the envelopes */
    /* click-free stealing: fade the old note, then start the pending one */
    int fade, fade_total;
    int pending_note, pending_root, pending_layer, pending_ch;
    float pending_vel;
    uint32_t rng;
    int sustained;
    /* output gain (amp env x level x send x pan) at the end of the last
     * control slice; the next slice ramps from here so modulation is smooth */
    float out_gl, out_gr;
    float pan_in, pan_gl, pan_gr;  /* cached equal-power gains for pan_in */
    float dc_x, dc_y;              /* DC blocker state (AC-coupled output) */
} AminoVoice;

typedef struct {
    int src[8], dst[8], via[8];
    uint16_t amt_offset[8];
    uint16_t via_depth_offset[8];
    int count;
    int table1, table2;        /* resolved wavetable indices */
    LayerParams smooth_params;
    LayerParams smooth_slices[AMINO_BLOCK / AMINO_CTRL];
    int smooth_ready;
    int slices_valid;          /* every slice equals smooth_params */
} AminoLayerRt;

/* ---- effects ---------------------------------------------------------- */
typedef struct {
    float *buf;
    int size, w;
} AminoDelay;

typedef struct {
    float b0, b1, b2, a1, a2;
    float z1, z2;
} AminoBiquad;

typedef struct {
    /* Chorus / Flanger */
    AminoDelay d[2];
    float phase;
    int voices;
    /* Phaser */
    float pz[2][12];
    int pn;
    float plast[2];
    /* Tremolo */
    float tg[4];
    /* Drive */
    float hp[2], lp[2], hold[2], xp[2], fp[2], cnt;
    /* Compressor */
    float cenv;
    /* EQ */
    AminoBiquad bq[2][4];
    /* Delay */
    float dt, dtarget, dstart, dfade, df[2], dphase;
    float sync_offset;
    int sync_locked;
    /* Reverb / Shimmer */
    AminoDelay lines[8], diff[2][4], pre[2], sh;
    int len[8];
    float gain[8], damp[8], mph[8];
    float dc;
    float sphase;
    float slp, shp, samt;
} AminoFxState;

typedef struct {
    int type;                  /* active type index, 0 = Off */
    int pending_type;
    int fading_out;
    int need_clear;
    uint32_t clear_pos;
    uint32_t clear_len;
    uint32_t arena_pos;
    float fade;                /* 0..1 wet fade */
    float wet_last;            /* smix * fade at the end of the last block */
    float mix, p[6];
    float smix, sp[6];
    uint8_t *arena;
    AminoFxState st;
} AminoFxSlot;

typedef struct AminoInstance {
    int used;
    int sr;
    float bpm;
    int move_root_note;        /* Move set root for the Scale=Set option */
    uint16_t move_scale_mask;  /* pitch classes relative to move_root_note */
    double sync_beat;          /* host beat position at the current block */
    int sync_transport;        /* beat position is valid while transport runs */
    const host_api_v1_t *host;

    GlobalParams g;
    LayerParams l[AMINO_MAX_LAYERS];
    AminoLayerRt lr[AMINO_MAX_LAYERS];
    AminoVoice v[AMINO_MAX_VOICES];

    /* performance / MIDI state */
    float modwheel, breath, foot, expr, timbre, bend;
    int sustain;
    float chan_at;
    float mpe_bend[16], mpe_at[16], mpe_timbre[16];
    uint32_t counter, rr, last_rr, alloc_cursor, rng, seed;
    uint64_t clock;            /* samples since instance start */
    int preset_index;
    char preset_name[AMINO_PRESET_NAME];
    int load_preset;           /* pending Load Layer source */
    int load_layer;
    float sm_master;

    AminoFxSlot fx[2];

    /* layer note stacks for Mono / Legato */
    int16_t mono_stack[AMINO_MAX_LAYERS][AMINO_MAX_VOICES];
    uint8_t mono_channel[AMINO_MAX_LAYERS][AMINO_MAX_VOICES];
    int mono_count[AMINO_MAX_LAYERS];

    uint32_t rev;              /* RUI digest */
    uint32_t rui_on;           /* transport running, for rui_poll */
    uint32_t rui_devms;        /* device/instance clock ms, playing only */
    double rui_tick;           /* 24-PPQN transport tick */
    float rui_bpm;
    float rui_peak;            /* output peak, ~300 ms decay, for rui_live */
    /* Remote UI audition notes (set_param "audition"), released on timeout */
    int8_t aud_note[AMINO_AUDITION_NOTES];
    uint8_t aud_ch[AMINO_AUDITION_NOTES];
    int32_t aud_left[AMINO_AUDITION_NOTES]; /* samples until forced release */
    int dirty_tables;
    int dirty_mods;
    char move_scale_name[24];  /* last Move scale string seen */
    int move_scale_root;

    /* Keep last: amino_create zeroes everything before it in one memset. */
    uint8_t fx_arena[2][AMINO_FX_ARENA] __attribute__((aligned(16)));
} AminoInstance;

/* ---- params.c --------------------------------------------------------- */
void amino_params_reset(AminoInstance *in);
void amino_apply_preset(AminoInstance *in, int index);
void amino_apply_factory_values(AminoInstance *in, int index);
void amino_init_focused_layer(AminoInstance *in);
void amino_load_focused_layer(AminoInstance *in, int preset, int layer);
void amino_randomize_focused_layer(AminoInstance *in);
int  amino_param_get(AminoInstance *in, const char *key, char *buf, int len);
void amino_param_set(AminoInstance *in, const char *key, const char *val);
int  amino_state_get(AminoInstance *in, char *buf, int len);
void amino_state_set(AminoInstance *in, const char *json);
int  amino_lookup_key(const char *key, int *layer, int *index);
float amino_param_value(AminoInstance *in, int layer, int index);
void amino_param_store(AminoInstance *in, int layer, int index, float v);
void amino_rebuild_layer(AminoInstance *in, int layer);
void amino_rebuild_layer_param(AminoInstance *in, int layer, int index);
void amino_all_notes_off(AminoInstance *in);

/* ---- amino.c ---------------------------------------------------------- */
void amino_audition(AminoInstance *in, const char *val);
int  amino_live(AminoInstance *in, char *buf, int len);

/* ---- voice.c ---------------------------------------------------------- */
void amino_voice_init(AminoInstance *in, int vi);
void amino_note_on(AminoInstance *in, int note, float vel, int channel);
void amino_note_off(AminoInstance *in, int note, int channel);
void amino_sustain(AminoInstance *in, int on);
void amino_poly_at(AminoInstance *in, int note, float value, int channel);
void amino_chan_at(AminoInstance *in, float value);
void amino_render_voices(AminoInstance *in, float *dl, float *dr,
                         float *f1l, float *f1r, float *f2l, float *f2r,
                         int frames);
void amino_kill_all(AminoInstance *in, int fast);
void amino_kill_channel(AminoInstance *in, int channel, int fast);
void amino_release_voices_from(AminoInstance *in, int first);
void amino_voice_mode_changed(AminoInstance *in, int layer);
void amino_move_scale_update(AminoInstance *in, int root_note, const char *scale);

/* ---- fx.c ------------------------------------------------------------- */
void amino_fx_init(AminoInstance *in, int slot);
void amino_fx_set_type(AminoInstance *in, int slot, int type);
void amino_fx_process(AminoInstance *in, int slot, float *l, float *r, int frames);
void amino_fx_release(AminoInstance *in, int slot);
const float *amino_fx_defaults(int type);

/* ---- contract --------------------------------------------------------- */
extern const char amino_chain_params_json[];
extern const char amino_ui_hierarchy_json[];

#endif

/* Parameter storage, key lookup, state blob and layer operations.
 *
 * The descriptor tables are generated (params_gen.c).  Everything here is
 * allocation-free: the state serialiser writes straight into the caller's
 * buffer and the parser walks the incoming string in place.
 */
#include "amino.h"
#include "contract.h"
#include "util.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AM_KEYMAX 64
#define AM_VALMAX 64
#define AM_STATE_VERSION 2

/* State v1 stored Filter Type as LP24 LP12 HP24 HP12 BP12 Off Moog; v2 moved
 * the ladder up after the low-pass types (docs/06 section 6). */
static const unsigned char FTYPE_V1_TO_V2[7] = { 0, 1, 3, 4, 5, 6, 2 };

/* v1 engines read Delay Time linearly over 1..2000 ms; v2 uses the documented
 * exponential curve (docs/11).  Keep a saved delay at the time it played. */
static float delay_time_v1_to_v2(float knob) {
    float ms = 1.0f + 1999.0f * am_clampf(knob, 0.0f, 1.0f);
    return logf(ms) / logf(2000.0f);
}

/* ------------------------------------------------------------------ lookup */

static const AminoParamDef *find_def(const AminoParamDef *tab, int count,
                                     const char *key) {
    /* Small table, but this is called from set_param/get_param which the host
     * may drive every block.  Compare the first byte before strcmp so the
     * common miss is one load and a branch. */
    char c0 = key[0];
    for (int i = 0; i < count; i++) {
        const char *k = tab[i].key;
        if (k[0] == c0 && strcmp(k, key) == 0)
            return &tab[i];
    }
    return 0;
}

static int parse_layer_key(const char *key, int *layer, const char **bare) {
    if (key[0] != 'l' || key[1] < '1' || key[1] > '4' || key[2] != '_')
        return 0;
    *layer = key[1] - '1';
    *bare = key + 3;
    return 1;
}

int amino_lookup_key(const char *key, int *layer, int *index) {
    const char *bare;
    int li;
    if (parse_layer_key(key, &li, &bare)) {
        const AminoParamDef *d = find_def(amino_layer_params,
                                          AMINO_LAYER_PARAM_COUNT, bare);
        if (!d) {
            if (layer) *layer = -1;
            if (index) *index = -1;
            return -1;
        }
        if (layer) *layer = li;
        if (index) *index = (int)(d - amino_layer_params);
        return 1;
    }
    const AminoParamDef *d = find_def(amino_global_params,
                                      AMINO_GLOBAL_PARAM_COUNT, key);
    if (!d) {
        /* A bare layer key (the child-template form used by chain_params)
         * targets the focused layer. */
        d = find_def(amino_layer_params, AMINO_LAYER_PARAM_COUNT, key);
        if (d) {
            if (layer) *layer = -2;
            if (index) *index = (int)(d - amino_layer_params);
            return 2;
        }
        if (layer) *layer = -1;
        if (index) *index = -1;
        return -1;
    }
    if (layer) *layer = -1;
    if (index) *index = (int)(d - amino_global_params);
    return 0;
}

float amino_param_value(AminoInstance *in, int layer, int index) {
    if (layer < 0 || layer >= AMINO_MAX_LAYERS) {
        if (index < 0 || index >= AMINO_GLOBAL_PARAM_COUNT) return 0.0f;
        const AminoParamDef *d = &amino_global_params[index];
        return *(const float *)((const char *)&in->g + d->offset);
    }
    if (index < 0 || index >= AMINO_LAYER_PARAM_COUNT) return 0.0f;
    const AminoParamDef *d = &amino_layer_params[index];
    return *(const float *)((const char *)&in->l[layer] + d->offset);
}

void amino_param_store(AminoInstance *in, int layer, int index, float v) {
    if (layer < 0 || layer >= AMINO_MAX_LAYERS) {
        if (index < 0 || index >= AMINO_GLOBAL_PARAM_COUNT) return;
        const AminoParamDef *d = &amino_global_params[index];
        *(float *)((char *)&in->g + d->offset) = v;
        return;
    }
    if (index < 0 || index >= AMINO_LAYER_PARAM_COUNT) return;
    const AminoParamDef *d = &amino_layer_params[index];
    *(float *)((char *)&in->l[layer] + d->offset) = v;
}

/* ------------------------------------------------------------------ clamp */

static float clamp_desc(const AminoParamDef *d, float v) {
    if (d->kind == AMINO_PK_FLOAT) {
        if (v != v) return d->def;            /* NaN */
        return am_clampf(v, d->min, d->max);
    }
    if (v != v) return d->def;
    float x = floorf(v + 0.5f);
    return am_clampf(x, d->min, d->max);
}

static float parse_enum(const AminoParamDef *d, const char *s) {
    if (!s) return d->def;
    while (*s == ' ') s++;
    int numeric = (*s == '-' || *s == '+' || (*s >= '0' && *s <= '9'));
    if (numeric) return clamp_desc(d, (float)strtod(s, 0));
    for (int i = 0; i < d->option_count; i++) {
        const char *o = d->options[i];
        const char *a = s, *b = o;
        while (*a && *b) {
            char ca = *a, cb = *b;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
            if (ca != cb) break;
            a++; b++;
        }
        if (!*a && !*b) return (float)i;
    }
    return d->def;
}

static float parse_value(const AminoParamDef *d, const char *s) {
    if (d->kind == AMINO_PK_ENUM) return parse_enum(d, s);
    if (!s || !*s) return d->def;
    float v = (float)strtod(s, 0);
    return clamp_desc(d, v);
}

/* ------------------------------------------------------------------ defaults */

static void default_layer(AminoInstance *in, int li) {
    for (int i = 0; i < AMINO_LAYER_PARAM_COUNT; i++) {
        const AminoParamDef *d = &amino_layer_params[i];
        amino_param_store(in, li, i, d->def);
    }
    in->l[li].midi_ch = (float)(li + 1);
}

void amino_params_reset(AminoInstance *in) {
    for (int i = 0; i < AMINO_GLOBAL_PARAM_COUNT; i++) {
        const AminoParamDef *d = &amino_global_params[i];
        amino_param_store(in, -1, i, d->def);
    }
    for (int li = 0; li < AMINO_MAX_LAYERS; li++)
        default_layer(in, li);
    in->l[0].on = 1.0f;              /* Init is one sounding layer */

    in->preset_index = 0;
    in->preset_name[0] = 0;
    strcpy(in->preset_name, "Init");
    in->load_preset = -1;
    in->load_layer = 0;
    in->dirty_tables = 1;
    in->dirty_mods = 1;
    for (int li = 0; li < AMINO_MAX_LAYERS; li++)
        amino_rebuild_layer(in, li);
}

static void rebuild_layer_tables(AminoInstance *in, int li) {
    if (li < 0 || li >= AMINO_MAX_LAYERS) return;
    AminoLayerRt *rt = &in->lr[li];
    LayerParams *p = &in->l[li];
    int t1 = (int)(p->osc1_table + 0.5f);
    int t2 = (int)(p->osc2_table + 0.5f);
    t1 = am_clampi(t1, 0, AMINO_NUM_TABLES - 1);
    t2 = am_clampi(t2, 0, AMINO_NUM_TABLES - 1);
    rt->table1 = t1;
    rt->table2 = p->osc2_link >= 0.5f ? t1 : t2;
    in->dirty_tables = 1;
}

static void rebuild_layer_mods(AminoInstance *in, int li) {
    if (li < 0 || li >= AMINO_MAX_LAYERS) return;
    AminoLayerRt *rt = &in->lr[li];
    rt->count = 0;
    for (int m = 0; m < 8; m++) {
        int i0 = LP_M1_SRC + m * 5;
        int src = (int)(amino_param_value(in, li, i0 + 0) + 0.5f);
        int dst = (int)(amino_param_value(in, li, i0 + 1) + 0.5f);
        int via = (int)(amino_param_value(in, li, i0 + 3) + 0.5f);
        src = am_clampi(src, 0, 26);
        dst = am_clampi(dst, 0, 25);
        via = am_clampi(via, 0, 26);
        if (src == 0 || dst == 0) continue;
        int n = rt->count++;
        rt->src[n] = src;
        rt->dst[n] = dst;
        rt->via[n] = via;
        rt->amt_offset[n] = amino_layer_params[i0 + 2].offset;
        rt->via_depth_offset[n] = amino_layer_params[i0 + 4].offset;
    }
    in->dirty_mods = 1;
}

void amino_rebuild_layer(AminoInstance *in, int li) {
    if (li < 0 || li >= AMINO_MAX_LAYERS) return;
    rebuild_layer_tables(in, li);
    rebuild_layer_mods(in, li);
}

void amino_rebuild_layer_param(AminoInstance *in, int li, int index) {
    if (li < 0 || li >= AMINO_MAX_LAYERS) return;
    if (index == LP_OSC1_TABLE || index == LP_OSC2_TABLE ||
        index == LP_OSC2_LINK)
        rebuild_layer_tables(in, li);
    if (index >= LP_M1_SRC && index <= LP_M8_VIA_DEPTH) {
        int field = (index - LP_M1_SRC) % 5;
        if (field == 0 || field == 1 || field == 3)
            rebuild_layer_mods(in, li);
    }
}

/* ------------------------------------------------------------------ presets */

static void copy_name(char *dst, const char *src) {
    int i = 0;
    if (!src) src = "";
    while (src[i] && i < AMINO_PRESET_NAME - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

void amino_apply_factory_values(AminoInstance *in, int index) {
    if (index < 0 || index >= amino_factory_preset_count) return;
    const AminoFactoryPreset *fp = &amino_factory_presets[index];
    for (int i = 0; i < fp->value_count; i++) {
        uint16_t pid = fp->values[i].param;
        float v = fp->values[i].value;
        int li, pi;
        if (pid & 0x8000) {
            li = (pid >> 9) & 3;
            pi = pid & 0x1FF;
            if (pi >= AMINO_LAYER_PARAM_COUNT) continue;
            v = clamp_desc(&amino_layer_params[pi], v);
            amino_param_store(in, li, pi, v);
        } else {
            pi = pid;
            if (pi >= AMINO_GLOBAL_PARAM_COUNT) continue;
            v = clamp_desc(&amino_global_params[pi], v);
            amino_param_store(in, -1, pi, v);
        }
    }
}

void amino_apply_preset(AminoInstance *in, int index) {
    if (index < 0 || index >= amino_factory_preset_count) return;
    amino_kill_all(in, 1);
    amino_params_reset(in);
    in->seed = (uint32_t)index + 1u;
    if (!in->seed) in->seed = 1u;
    in->rng = in->seed;
    in->counter = 0;
    in->rr = 0;
    in->last_rr = (uint32_t)-1;
    in->alloc_cursor = 0;
    for (int i = 0; i < AMINO_MAX_VOICES; i++) amino_voice_init(in, i);
    amino_apply_factory_values(in, index);
    in->preset_index = index;
    copy_name(in->preset_name, amino_factory_presets[index].name);
    for (int li = 0; li < AMINO_MAX_LAYERS; li++)
        amino_rebuild_layer(in, li);
    for (int li = 0; li < AMINO_MAX_LAYERS; li++)
        in->lr[li].smooth_ready = 0;
    for (int s = 0; s < 2; s++) {
        int t = (int)((s == 0 ? in->g.fx1_type : in->g.fx2_type) + 0.5f);
        amino_fx_set_type(in, s, t);
    }
}

/* ------------------------------------------------------------------ layer ops */

void amino_init_focused_layer(AminoInstance *in) {
    int li = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    amino_kill_all(in, 1);
    default_layer(in, li);
    in->l[li].on = 1.0f;
    amino_rebuild_layer(in, li);
}

void amino_load_focused_layer(AminoInstance *in, int preset, int layer) {
    int li = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    if (preset < 0 || preset >= amino_factory_preset_count) return;
    if (layer < 1 || layer > 4) return;
    amino_kill_all(in, 1);
    default_layer(in, li);
    const AminoFactoryPreset *fp = &amino_factory_presets[preset];
    int want = layer - 1;
    for (int i = 0; i < fp->value_count; i++) {
        uint16_t pid = fp->values[i].param;
        if (!(pid & 0x8000)) continue;
        if (((pid >> 9) & 3) != want) continue;
        int pi = pid & 0x1FF;
        if (pi >= AMINO_LAYER_PARAM_COUNT) continue;
        amino_param_store(in, li, pi,
                          clamp_desc(&amino_layer_params[pi], fp->values[i].value));
    }
    in->l[li].on = 1.0f;
    amino_rebuild_layer(in, li);
}

void amino_randomize_focused_layer(AminoInstance *in) {
    int li = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    LayerParams *p = &in->l[li];
    float amt = am_clampf(in->g.rand_amt, 0.0f, 1.0f);
    if (amt <= 0.0f) return;

#define RRND(lo, hi) ((lo) + ((hi) - (lo)) * am_rand_unit(&in->rng))
#define RJIT(v, span) ((v) + am_rand_bipolar(&in->rng) * (span) * amt)

    /* Pitch character */
    p->osc1_table = (float)am_clampi((int)RRND(0, AMINO_NUM_TABLES), 0,
                                     AMINO_NUM_TABLES - 1);
    p->osc2_table = (float)am_clampi((int)RRND(0, AMINO_NUM_TABLES), 0,
                                     AMINO_NUM_TABLES - 1);
    if (am_rand_unit(&in->rng) < 0.55f) p->osc1_wave = RRND(0, 61);
    if (am_rand_unit(&in->rng) < 0.35f) p->osc2_wave = RRND(0, 61);
    if (am_rand_unit(&in->rng) < 0.30f) {
        p->osc1_semi = (float)((int)RRND(-12, 13));
        p->osc2_semi = (float)((int)RRND(-12, 13));
    }
    p->osc1_fine = RJIT(p->osc1_fine, 12.0f);
    p->osc2_fine = RJIT(p->osc2_fine, 12.0f);
    p->osc2_level = am_clampf(RRND(0.0f, 0.8f) * amt, 0.0f, 1.0f);
    p->osc2_link = am_rand_unit(&in->rng) < 0.3f ? 1.0f : 0.0f;

    /* Filter */
    p->cutoff = am_clampf(RRND(38.0f, 127.0f), 0.0f, 127.0f);
    p->res = am_clampf(RRND(0.0f, 0.75f) * (0.4f + amt), 0.0f, 1.0f);
    p->fenv = am_rand_bipolar(&in->rng) * 0.8f * amt;
    p->fkbd = am_clampf(RJIT(p->fkbd, 0.4f), 0.0f, 2.0f);
    p->fvel = am_rand_bipolar(&in->rng) * 0.6f * amt;
    if (am_rand_unit(&in->rng) < 0.5f) {
        static const unsigned char pick[4] = { 0, 1, 3, 4 }; /* LP24 LP12 HP24 HP12 */
        p->ftype = (float)pick[am_clampi((int)RRND(0, 4), 0, 3)];
    }

    /* Envelopes: stay inside 0..1 and keep attack short enough to play. */
    p->e1_a = am_clampf(RRND(0.0f, 0.55f) * amt, 0.0f, 1.0f);
    p->e1_d = am_clampf(RJIT(p->e1_d, 0.3f), 0.0f, 1.0f);
    p->e1_s = am_clampf(RRND(0.0f, 0.8f), 0.0f, 1.0f);
    p->e2_a = am_clampf(RRND(0.0f, 0.45f) * amt, 0.0f, 1.0f);
    p->e2_d = am_clampf(RJIT(p->e2_d, 0.3f), 0.0f, 1.0f);
    p->e2_s = am_clampf(RRND(0.0f, 0.7f), 0.0f, 1.0f);
    p->e3_a = am_clampf(RRND(0.0f, 0.28f) * amt, 0.0f, 1.0f);
    p->e3_d = am_clampf(RJIT(p->e3_d, 0.25f), 0.0f, 1.0f);
    p->e3_s = am_clampf(RRND(0.45f, 1.0f), 0.0f, 1.0f);
    p->e3_r = am_clampf(RJIT(p->e3_r, 0.2f), 0.0f, 1.0f);

    /* Dirt / drive */
    p->dirt_type = (float)(int)RRND(0, 6);
    p->dirt = am_rand_unit(&in->rng) < 0.45f
                  ? am_clampf(RRND(0.0f, 0.35f) * amt, 0.0f, 1.0f) : 0.0f;
    p->drv_type = am_rand_unit(&in->rng) < 0.4f ? (float)(int)RRND(0, 5) : 0.0f;
    p->drive = am_clampf(RRND(0.0f, 0.55f) * amt, 0.0f, 1.0f);

    /* LFOs */
    p->lfo1_shape = (float)(int)RRND(0, 7);
    p->lfo2_shape = (float)(int)RRND(0, 7);
    p->lfo1_rate = am_clampf(RRND(0.15f, 0.8f), 0.0f, 1.0f);
    p->lfo2_rate = am_clampf(RRND(0.15f, 0.8f), 0.0f, 1.0f);
    p->lfo1_fade = am_clampf(RRND(0.0f, 0.5f) * amt, 0.0f, 1.0f);

    /* XMod */
    if (am_rand_unit(&in->rng) < 0.35f) {
        p->xmod_type = (float)(int)RRND(0, 4);
        p->xmod = am_clampf(RRND(0.0f, 0.7f) * amt, 0.0f, 1.0f);
    }

    /* Pan */
    p->pan_mode = am_rand_unit(&in->rng) < 0.5f ? (float)(int)RRND(0, 4) : 0.0f;
    p->pan = am_rand_bipolar(&in->rng) * 0.8f * amt;

    amino_rebuild_layer(in, li);
#undef RRND
#undef RJIT
}

/* ------------------------------------------------------------------ JSON out */

typedef struct {
    char *buf;
    int len, cap, over;
} JOut;

static void jc(JOut *o, char c) {
    if (o->len + 1 >= o->cap) { o->over = 1; return; }
    o->buf[o->len++] = c;
}

static void js(JOut *o, const char *s) {
    if (!s) return;
    while (*s) jc(o, *s++);
}

static void jesc(JOut *o, const char *s) {
    if (!s) return;
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { jc(o, '\\'); jc(o, (char)c); }
        else if (c < 0x20) { jc(o, ' '); }
        else if (c < 0x80) jc(o, (char)c);
        else jc(o, '?');       /* names are ASCII; keep the blob valid */
    }
}

static void key_prefix(JOut *o, int first, const char *key) {
    if (!first) jc(o, ',');
    jc(o, '"');
    js(o, key);
    js(o, "\":\"");
}

static void value_str(const AminoParamDef *d, float v, char *tmp, int n) {
    if (d->kind == AMINO_PK_FLOAT) {
        float x = am_clampf(v, d->min, d->max);
        snprintf(tmp, (size_t)n, "%.3f", (double)x);
    } else {
        int iv = (int)floorf(am_clampf(v, d->min, d->max) + 0.5f);
        snprintf(tmp, (size_t)n, "%d", iv);
    }
}

static int state_build(AminoInstance *in, char *buf, int len) {
    if (!buf || len < 8) return -1;
    JOut o = { buf, 0, len - 1, 0 };
    char tmp[40];
    snprintf(tmp, sizeof tmp, "%d", AM_STATE_VERSION);
    js(&o, "{\"fmt\":\"amino\",\"v\":\"");
    js(&o, tmp);
    js(&o, "\",\"rev\":\"");
    snprintf(tmp, sizeof tmp, "%lu", (unsigned long)in->rev);
    js(&o, tmp);
    js(&o, "\",\"name\":\"");
    jesc(&o, in->preset_name);
    js(&o, "\"");
    for (int i = 0; i < AMINO_GLOBAL_PARAM_COUNT; i++) {
        const AminoParamDef *d = &amino_global_params[i];
        key_prefix(&o, 0, d->key);
        value_str(d, amino_param_value(in, -1, i), tmp, sizeof tmp);
        js(&o, tmp);
        jc(&o, '"');
    }
    char key[AM_KEYMAX];
    for (int li = 0; li < AMINO_MAX_LAYERS; li++) {
        for (int i = 0; i < AMINO_LAYER_PARAM_COUNT; i++) {
            const AminoParamDef *d = &amino_layer_params[i];
            snprintf(key, sizeof key, "l%d_%s", li + 1, d->key);
            key_prefix(&o, 0, key);
            value_str(d, amino_param_value(in, li, i), tmp, sizeof tmp);
            js(&o, tmp);
            jc(&o, '"');
        }
    }
    if (o.over) return -1;
    jc(&o, '}');
    if (o.over) return -1;
    buf[o.len] = 0;
    return o.len;
}

int amino_state_get(AminoInstance *in, char *buf, int len) {
    return state_build(in, buf, len);
}

/* ------------------------------------------------------------------ JSON in */

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

static const char *read_string(const char *p, char *dst, int cap) {
    int n = 0;
    if (*p != '"') return 0;
    p++;
    while (*p && *p != '"') {
        char c = *p++;
        if (c == '\\' && *p) {
            char e = *p++;
            if (e == 'n') c = '\n';
            else if (e == 't') c = '\t';
            else if (e == 'r') c = '\r';
            else c = e;
        }
        if (n < cap - 1) dst[n++] = c;
    }
    dst[n] = 0;
    if (*p == '"') p++;
    return p;
}

static const char *read_bare(const char *p, char *dst, int cap) {
    int n = 0;
    while (*p && *p != ',' && *p != '}' && *p != ' ' && *p != '\n' && *p != '\r') {
        if (n < cap - 1) dst[n++] = *p;
        p++;
    }
    dst[n] = 0;
    return p;
}

static void apply_key(AminoInstance *in, const char *key, const char *val,
                      int *saw_rev, int *version) {
    if (strcmp(key, "fmt") == 0) return;
    if (strcmp(key, "v") == 0) {
        *version = (int)strtol(val, 0, 10);
        return;
    }
    if (strcmp(key, "rev") == 0) {
        in->rev = (uint32_t)strtoul(val, 0, 10);
        *saw_rev = 1;
        return;
    }
    if (strcmp(key, "name") == 0) {
        copy_name(in->preset_name, val);
        return;
    }
    if (strcmp(key, "state") == 0) return;
    int layer, index;
    int kind = amino_lookup_key(key, &layer, &index);
    if (kind < 0) return;                    /* unknown: ignore, by design */
    if (kind == 2) layer = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    const AminoParamDef *d = layer < 0 ? &amino_global_params[index]
                                       : &amino_layer_params[index];
    float v = parse_value(d, val);
    if (*version < 2 && layer >= 0 && index == LP_FTYPE &&
        val[0] >= '0' && val[0] <= '9')
        v = (float)FTYPE_V1_TO_V2[am_clampi((int)(v + 0.5f), 0, 6)];
    if (*version < 2 && layer < 0 &&
        ((index == GP_FX1_P1 && (int)(in->g.fx1_type + 0.5f) == 8) ||
         (index == GP_FX2_P1 && (int)(in->g.fx2_type + 0.5f) == 8)))
        v = delay_time_v1_to_v2(v);
    /* No per-key rebuild: amino_state_set rebuilds every layer at the end. */
    amino_param_store(in, layer, index, v);
    if (strcmp(key, "fx1_type") == 0 || strcmp(key, "fx2_type") == 0) {
        int slot = key[2] == '1' ? 0 : 1;
        amino_fx_set_type(in, slot, (int)(v + 0.5f));
    }
}

void amino_state_set(AminoInstance *in, const char *json) {
    if (!json || !*json) return;
    amino_kill_all(in, 1);
    /* Blobs carry every key, so this only matters for blobs saved before
     * Patch Gain existed: they play at unity, as they did then. */
    in->g.patch_gain = 0.0f;
    const char *p = json;
    int saw_rev = 0;
    int version = AM_STATE_VERSION;   /* a blob without "v" is current */
    while (*p) {
        p = skip_ws(p);
        if (*p == '{') { p++; continue; }
        if (*p == '}') { p++; continue; }
        if (*p == ',') { p++; continue; }
        char key[AM_KEYMAX], val[AM_VALMAX];
        if (*p != '"') { p++; continue; }
        p = read_string(p, key, sizeof key);
        if (!p) break;
        p = skip_ws(p);
        if (*p != ':') continue;
        p = skip_ws(p + 1);
        if (*p == '"') p = read_string(p, val, sizeof val);
        else p = read_bare(p, val, sizeof val);
        if (!p) break;
        apply_key(in, key, val, &saw_rev, &version);
    }
    in->preset_index = -1;
    int n = am_clampi((int)(in->g.layer_count + 0.5f), 1, 4);
    if (in->g.edit_layer > (float)n) in->g.edit_layer = (float)n;
    for (int li = 0; li < AMINO_MAX_LAYERS; li++)
        amino_rebuild_layer(in, li);
    /* A state load snaps the per-layer smoothers (docs/06 section 2). */
    for (int li = 0; li < AMINO_MAX_LAYERS; li++)
        in->lr[li].smooth_ready = 0;
    if (!saw_rev) {
        in->rev++;
        if (!in->rev) in->rev = 1;
    }
}

/* ------------------------------------------------------------------ get */

static int copy_out(const char *s, char *buf, int len) {
    int n = (int)strlen(s);
    if (!buf || len <= n) return -1;
    memcpy(buf, s, (size_t)n + 1);
    return n;
}

static int appendf(char *buf, int len, int *used, const char *fmt, ...) {
    if (*used < 0 || *used >= len) return -1;
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *used, (size_t)(len - *used), fmt, args);
    va_end(args);
    if (n < 0 || n >= len - *used) return -1;
    *used += n;
    return 0;
}

static int mod_matrix_state(AminoInstance *in, char *buf, int len) {
    int layer = am_clampi((int)(in->g.edit_layer + 0.5f), 1, AMINO_MAX_LAYERS) - 1;
    int used = 0;
    if (appendf(buf, len, &used, "{\"layer\":%d,\"routes\":[", layer + 1) < 0)
        return -1;
    for (int m = 0; m < 8; m++) {
        int i0 = LP_M1_SRC + m * 5;
        if (m && appendf(buf, len, &used, ",") < 0) return -1;
        if (appendf(buf, len, &used, "[%d,%d,%.4f,%d,%.4f]",
                    (int)(amino_param_value(in, layer, i0) + 0.5f),
                    (int)(amino_param_value(in, layer, i0 + 1) + 0.5f),
                    (double)amino_param_value(in, layer, i0 + 2),
                    (int)(amino_param_value(in, layer, i0 + 3) + 0.5f),
                    (double)amino_param_value(in, layer, i0 + 4)) < 0)
            return -1;
    }
    if (appendf(buf, len, &used, "],\"lfos\":[") < 0) return -1;
    for (int i = 0; i < 2; i++) {
        int base = i == 0 ? LP_LFO1_SHAPE : LP_LFO2_SHAPE;
        if (i && appendf(buf, len, &used, ",") < 0) return -1;
        if (appendf(buf, len, &used, "[%d,%.4f,%d,%d]",
                    (int)(amino_param_value(in, layer, base) + 0.5f),
                    (double)amino_param_value(in, layer, base + 1),
                    (int)(amino_param_value(in, layer, base + 2) + 0.5f),
                    (int)(amino_param_value(in, layer, base + 3) + 0.5f)) < 0)
            return -1;
    }
    if (appendf(buf, len, &used, "],\"envs\":[") < 0) return -1;
    for (int i = 0; i < 3; i++) {
        int base = i == 0 ? LP_E1_A : (i == 1 ? LP_E2_A : LP_E3_A);
        if (i && appendf(buf, len, &used, ",") < 0) return -1;
        if (appendf(buf, len, &used, "[%.4f,%.4f,%.4f,%.4f]",
                    (double)amino_param_value(in, layer, base),
                    (double)amino_param_value(in, layer, base + 1),
                    (double)amino_param_value(in, layer, base + 2),
                    (double)amino_param_value(in, layer, base + 3)) < 0)
            return -1;
    }
    if (appendf(buf, len, &used, "]}") < 0) return -1;
    return used;
}

static int wave_view(AminoInstance *in, int osc, char *buf, int len) {
    int li = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    int table = osc == 1 ? in->lr[li].table1 : in->lr[li].table2;
    const LayerParams *p = &in->l[li];
    int slot = (int)((osc == 1 ? p->osc1_wave : p->osc2_wave) + 0.5f);
    slot = am_clampi(slot, 0, 63);
    if (len < 129) return -1;
    static const char hex[] = "0123456789ABCDEF";
    const signed char *w = amino_wavetable_data[table][slot];
    for (int i = 0; i < 64; i++) {
        int a = w[i * 2] + 128;
        int b = w[i * 2 + 1] + 128;
        int v = (a + b) >> 1;
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        buf[i * 2] = hex[v >> 4];
        buf[i * 2 + 1] = hex[v & 15];
    }
    buf[128] = 0;
    return 128;
}

static int effective_wave(AminoInstance *in, int layer, int osc) {
    for (int i = 0; i < AMINO_MAX_VOICES; i++) {
        AminoVoice *v = &in->v[i];
        if (v->active && v->layer == layer)
            return osc == 1 ? v->w1 : v->w2;
    }
    return osc == 1 ? (int)(in->l[layer].osc1_wave + 0.5f)
                    : (int)(in->l[layer].osc2_wave + 0.5f);
}

int amino_param_get(AminoInstance *in, const char *key, char *buf, int len) {
    if (!key) return -1;

    if (strcmp(key, "state") == 0)
        return state_build(in, buf, len);
    if (strcmp(key, "chain_params") == 0)
        return copy_out(amino_chain_params_json, buf, len);
    if (strcmp(key, "ui_hierarchy") == 0)
        return -1;
    if (strcmp(key, "ui_pages") == 0)
        return copy_out(amino_ui_pages_json, buf, len);
    if (strcmp(key, "mod_matrix_state") == 0)
        return mod_matrix_state(in, buf, len);
    if (strcmp(key, "layer_src_list") == 0)
        return copy_out(amino_layer_src_list_json, buf, len);
    if (strcmp(key, "preset_list") == 0)
        return copy_out(amino_preset_list_json, buf, len);
    if (strcmp(key, "split_voices") == 0)
        return copy_out("", buf, len);
    if (strcmp(key, "preset_count") == 0) {
        char tmp[16];
        snprintf(tmp, sizeof tmp, "%d", amino_factory_preset_count);
        return copy_out(tmp, buf, len);
    }
    if (strcmp(key, "preset_name") == 0)
        return copy_out(in->preset_name, buf, len);
    if (strcmp(key, "preset") == 0) {
        char tmp[16];
        snprintf(tmp, sizeof tmp, "%d", in->preset_index);
        return copy_out(tmp, buf, len);
    }
    if (strcmp(key, "layer_load_src") == 0) {
        char tmp[16];
        snprintf(tmp, sizeof tmp, "%d", in->load_preset);
        return copy_out(tmp, buf, len);
    }
    if (strcmp(key, "wave_view") == 0)
        return wave_view(in, 1, buf, len);
    if (strcmp(key, "wave_view2") == 0)
        return wave_view(in, 2, buf, len);
    if (strcmp(key, "rui_live") == 0)
        return amino_live(in, buf, len);
    if (strcmp(key, "rui_poll") == 0) {
        char tmp[96];
        if (in->rui_on)
            snprintf(tmp, sizeof tmp, "%lu:1:%llu:%.1f:%lu",
                     (unsigned long)in->rev,
                     (unsigned long long)in->rui_tick,
                     (double)in->rui_bpm,
                     (unsigned long)in->rui_devms);
        else
            snprintf(tmp, sizeof tmp, "%lu:0:0:%.1f",
                     (unsigned long)in->rev, (double)in->rui_bpm);
        return copy_out(tmp, buf, len);
    }

    /* lN_oscX_wave:effective */
    size_t kl = strlen(key);
    if (kl > 10 && strcmp(key + kl - 10, ":effective") == 0) {
        char base[AM_KEYMAX];
        if (kl - 10 >= sizeof base) return -1;
        memcpy(base, key, kl - 10);
        base[kl - 10] = 0;
        int layer, index;
        int kind = amino_lookup_key(base, &layer, &index);
        if (kind < 0) return -1;
        if (kind == 2) layer = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
        if (layer < 0) return -1;
        int osc = (index == LP_OSC2_WAVE) ? 2 : 1;
        if (index != LP_OSC1_WAVE && index != LP_OSC2_WAVE) {
            /* Any other key: report the base value. */
            char tmp[40];
            value_str(kind == 0 ? &amino_global_params[index]
                                : &amino_layer_params[index],
                      amino_param_value(in, layer, index), tmp, sizeof tmp);
            return copy_out(tmp, buf, len);
        }
        char tmp[16];
        snprintf(tmp, sizeof tmp, "%d", effective_wave(in, layer, osc));
        return copy_out(tmp, buf, len);
    }

    int layer, index;
    int kind = amino_lookup_key(key, &layer, &index);
    if (kind < 0) return -1;
    if (kind == 2) layer = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    const AminoParamDef *d = layer < 0 ? &amino_global_params[index]
                                       : &amino_layer_params[index];
    char tmp[40];
    value_str(d, amino_param_value(in, layer, index), tmp, sizeof tmp);
    return copy_out(tmp, buf, len);
}

/* ------------------------------------------------------------------ set */

static int source_index_for_value(const char *val) {    /* Numeric source index, or an exact/prefix match on the label. */
    while (*val == ' ') val++;
    if ((*val >= '0' && *val <= '9') || *val == '-') {
        int n = (int)strtod(val, 0);
        if (n >= 0 && n < amino_layer_source_count) return n;
        return -1;
    }
    size_t vl = strlen(val);
    for (int i = 0; i < amino_layer_source_count; i++) {
        const char *lab = amino_layer_sources[i].label;
        size_t ll = strlen(lab);
        size_t n = vl < ll ? vl : ll;
        int ok = 1;
        for (size_t k = 0; k < n; k++) {
            char a = val[k], b = lab[k];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) { ok = 0; break; }
        }
        if (ok) return i;
    }
    return -1;
}

static void rui_edit(AminoInstance *in) {
    in->rev++;
    if (!in->rev) in->rev = 1;
}

void amino_param_set(AminoInstance *in, const char *key, const char *val) {
    if (!key || !val) return;

    if (strcmp(key, "state") == 0) {
        amino_state_set(in, val);
        return;
    }
    if (strcmp(key, "preset") == 0) {
        int n = (int)strtod(val, 0);
        amino_apply_preset(in, n);
        rui_edit(in);
        return;
    }
    if (strcmp(key, "name") == 0 || strcmp(key, "preset_name") == 0) {
        if (strcmp(in->preset_name, val) != 0) {
            copy_name(in->preset_name, val);
            in->preset_index = -1;
            rui_edit(in);
        }
        return;
    }
    if (strcmp(key, "audition") == 0) {
        /* Plays notes only; not a sound edit, so rev and preset stay. */
        amino_audition(in, val);
        return;
    }
    if (strcmp(key, "init_layer") == 0) {
        amino_init_focused_layer(in);
        in->preset_index = -1;
        rui_edit(in);
        return;
    }
    if (strcmp(key, "randomize") == 0) {
        amino_randomize_focused_layer(in);
        in->preset_index = -1;
        rui_edit(in);
        return;
    }
    if (strcmp(key, "layer_load_src") == 0) {
        int idx = source_index_for_value(val);
        if (idx >= 0) {
            in->load_preset = amino_layer_sources[idx].preset;
            in->load_layer = amino_layer_sources[idx].layer;
            amino_load_focused_layer(in, in->load_preset, in->load_layer);
            in->preset_index = -1;
            rui_edit(in);
        }
        return;
    }
    if (strcmp(key, "layer_load_go") == 0) {
        if (in->load_preset >= 0) {
            amino_load_focused_layer(in, in->load_preset, in->load_layer);
            in->preset_index = -1;
            rui_edit(in);
        }
        return;
    }
    if (strcmp(key, "preset_count") == 0 || strcmp(key, "fmt") == 0 ||
        strcmp(key, "v") == 0)
        return;

    int layer, index;
    int kind = amino_lookup_key(key, &layer, &index);
    if (kind < 0) return;
    if (kind == 2) layer = am_clampi((int)(in->g.edit_layer + 0.5f), 1, 4) - 1;
    const AminoParamDef *d = layer < 0 ? &amino_global_params[index]
                                       : &amino_layer_params[index];
    float before = amino_param_value(in, layer, index);
    float v = parse_value(d, val);
    amino_param_store(in, layer, index, v);

    if (layer < 0 && strcmp(key, "layer_count") == 0) {
        /* Layers sets lN_on for every layer inside the count (docs 05 4.14). */
        int n = am_clampi((int)(v + 0.5f), 1, 4);
        for (int i = 0; i < n; i++) in->l[i].on = 1.0f;
        if (in->g.edit_layer > (float)n) in->g.edit_layer = (float)n;
    }
    if (layer < 0 && strcmp(key, "edit_layer") == 0) {
        int n = am_clampi((int)(in->g.layer_count + 0.5f), 1, 4);
        float e = am_clampf(v, 1.0f, (float)n);
        in->g.edit_layer = e;
    }

    if (layer >= 0 && amino_param_value(in, layer, index) != before) {
        amino_rebuild_layer_param(in, layer, index);
        if (index == LP_POLY) amino_voice_mode_changed(in, layer);
    }

    if (strcmp(key, "fx1_type") == 0 || strcmp(key, "fx2_type") == 0) {
        int slot = key[2] == '1' ? 0 : 1;
        int type = (int)(v + 0.5f);
        /* A direct type change loads that type's defaults (docs/11 section 3).
         * Preset and state loads go through their own paths and keep their
         * stored p-values. */
        if (type > 0 && v != before) {
            float *base = slot == 0 ? &in->g.fx1_p1 : &in->g.fx2_p1;
            const float *def = amino_fx_defaults(type);
            for (int i = 0; i < 6; i++) base[i] = def[i];
        }
        amino_fx_set_type(in, slot, type);
    }
    if (strcmp(key, "voices") == 0) {
        int n = (int)(v + 0.5f);
        if (n < 1) n = 1;
        if (n > AMINO_MAX_VOICES) n = AMINO_MAX_VOICES;
        amino_release_voices_from(in, n);
    }
    if (amino_param_value(in, layer, index) != before) {
        /* Only a real sound edit detaches the patch from its preset; a no-op
         * write or a change of the focused layer leaves it loaded. */
        if (!(layer < 0 && index == GP_EDIT_LAYER)) in->preset_index = -1;
        rui_edit(in);
    }
}

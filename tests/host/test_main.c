/* Native smoke test for the Amino DSP module.  Not shipped. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "amino.h"
#include "plugin_api.h"
#include "util.h"

extern plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

static float g_bpm = 120.0f;
static double g_beats = 0.0;
static int g_clock_status = MOVE_CLOCK_STATUS_RUNNING;

static float host_bpm(void) { return g_bpm; }
static int host_clock(void) { return g_clock_status; }
static double host_beats(void) { return g_beats; }

static int g_fail;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); g_fail++; } \
} while (0)

static double render_rms(plugin_api_v2_t *api, void *inst, int blocks,
                         int16_t *sink, int *peak_out) {
    int16_t out[128 * 2];
    double sum = 0.0;
    int peak = 0;
    for (int b = 0; b < blocks; b++) {
        api->render_block(inst, out, 128);
        for (int i = 0; i < 256; i++) {
            int v = out[i];
            if (v < 0) v = -v;
            if (v > peak) peak = v;
            sum += (double)out[i] * (double)out[i];
            if (sink) sink[(b * 256 + i) % (128 * 2)] = out[i];
        }
    }
    if (peak_out) *peak_out = peak;
    return sqrt(sum / (double)(blocks * 256));
}

/* Float wire values are printed with three decimals, enums as indices. */
static int near_str(const char *s, double want) {
    return fabs(strtod(s, 0) - want) < 1e-3;
}

/* Power of x[0..n) at hz (Goertzel), normalised by the total power. */
static double band_share(const float *x, int n, double hz, double sr) {
    double c = 2.0 * cos(2.0 * M_PI * hz / sr), s1 = 0.0, s2 = 0.0, tot = 0.0;
    for (int i = 0; i < n; i++) {
        double s0 = x[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
        tot += (double)x[i] * x[i];
    }
    double pw = s1 * s1 + s2 * s2 - c * s1 * s2;
    return tot > 0.0 ? pw / (tot * n) : 0.0;
}

/* Largest sample-to-sample step of the left channel over `blocks` blocks,
 * starting from `*last` (the previous block's final sample). */
static int max_step(plugin_api_v2_t *api, void *inst, int blocks, int *last) {
    int16_t out[128 * 2];
    int worst = 0;
    for (int b = 0; b < blocks; b++) {
        api->render_block(inst, out, 128);
        for (int i = 0; i < 128; i++) {
            int d = abs(out[i * 2] - *last);
            if (d > worst) worst = d;
            *last = out[i * 2];
        }
    }
    return worst;
}

static int gated_voices(AminoInstance *in) {
    int n = 0;
    for (int i = 0; i < AMINO_MAX_VOICES; i++)
        if (in->v[i].active && in->v[i].gate) n++;
    return n;
}

/* A low note through a closed LP24: close to a sine, so any gain step shows
 * up as a sample-to-sample jump far above the waveform's own slope. */
static void *smooth_tone(plugin_api_v2_t *api) {
    void *in = api->create_instance("", 0);
    api->set_param(in, "l1_ftype", "LP24");
    api->set_param(in, "l1_cutoff", "40");
    api->set_param(in, "l1_res", "0");
    api->set_param(in, "l1_e3_a", "0");
    api->set_param(in, "l1_e3_s", "1");
    api->set_param(in, "l1_route", "Dry");
    api->on_midi(in, (const uint8_t[]){ 0x90, 36, 100 }, 3, 0);
    render_rms(api, in, 40, 0, 0);
    return in;
}

/* Regression tests for the 2026-10-26 C review. */
static void regression_tests(plugin_api_v2_t *api) {
    int saved_clock = g_clock_status;
    g_clock_status = MOVE_CLOCK_STATUS_STOPPED;

    /* Global LFO phase after hours of uptime: a float clock froze it. */
    {
        AminoInstance *in = api->create_instance("", 0);
        api->set_param(in, "l1_lfo1_phase", "Global");
        api->set_param(in, "l1_lfo1_sync", "Off");
        api->set_param(in, "l1_lfo1_rate", "0.8");
        in->clock = (uint64_t)(4.0 * 3600.0 * 44100.0);
        api->on_midi(in, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
        double want = (double)am_lfo_hz(in->l[0].lfo1_rate) * 128.0 / 44100.0;
        render_rms(api, in, 1, 0, 0);
        float prev = in->v[0].lfo[0].phase;
        double worst = 0.0;
        for (int b = 0; b < 40; b++) {
            render_rms(api, in, 1, 0, 0);
            double d = (double)in->v[0].lfo[0].phase - prev;
            if (d < 0.0) d += 1.0;
            if (fabs(d - want) > worst) worst = fabs(d - want);
            prev = in->v[0].lfo[0].phase;
        }
        printf("global lfo @4h: worst per-block phase error %.6f\n", worst);
        CHECK(worst < 1e-4, "Global LFO keeps its rate after 4 h uptime");
        api->destroy_instance(in);
    }

    /* Voice Mode changes with a key held must not leave a stuck note. */
    {
        AminoInstance *in = api->create_instance("", 0);
        api->set_param(in, "l1_poly", "Mono");
        amino_note_on(in, 60, 1.0f, 0);
        render_rms(api, in, 1, 0, 0);
        api->set_param(in, "l1_poly", "Poly");
        amino_note_off(in, 60, 0);
        api->set_param(in, "l1_poly", "Mono");
        amino_note_on(in, 62, 1.0f, 0);
        render_rms(api, in, 1, 0, 0);
        amino_note_off(in, 62, 0);
        render_rms(api, in, 2, 0, 0);
        CHECK(gated_voices(in) == 0, "Mono->Poly->Mono leaves no held note");

        api->set_param(in, "l1_poly", "Poly");
        amino_note_on(in, 64, 1.0f, 0);
        render_rms(api, in, 1, 0, 0);
        api->set_param(in, "l1_poly", "Mono");
        amino_note_off(in, 64, 0);
        render_rms(api, in, 2, 0, 0);
        CHECK(gated_voices(in) == 0, "Poly->Mono releases the held poly note");
        api->destroy_instance(in);
    }

    /* PM phase wrap stays inside the 128-sample table. */
    CHECK(am_wrap128(-1e-6f) < 128.0f && am_wrap128(-1e-6f) >= 0.0f,
          "tiny negative phase wraps below 128");
    CHECK(am_wrap128(128.0f) == 0.0f && am_wrap128(-0.5f) == 127.5f &&
          am_wrap128(300.0f) == 44.0f, "phase wrap values");

    /* A host block longer than AMINO_BLOCK renders every frame, and matches
     * the same audio rendered as two 128-frame blocks. */
    {
        void *a = api->create_instance("", 0);
        void *b = api->create_instance("", 0);
        api->on_midi(a, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
        api->on_midi(b, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
        static int16_t one[256 * 2], two[256 * 2];
        for (int i = 0; i < 512; i++) one[i] = 0x5A5A;
        api->render_block(a, one, 256);
        api->render_block(b, two, 128);
        api->render_block(b, two + 256, 128);
        CHECK(memcmp(one, two, sizeof one) == 0,
              "256-frame block equals two 128-frame blocks");
        api->destroy_instance(a);
        api->destroy_instance(b);
    }

    /* Only a real sound edit detaches the patch from its preset. */
    {
        void *in = api->create_instance("", 0);
        char got[16];
        api->set_param(in, "preset", "5");
        api->set_param(in, "edit_layer", "1");
        api->get_param(in, "l1_cutoff", got, sizeof got);
        api->set_param(in, "l1_cutoff", got);
        api->get_param(in, "preset", got, sizeof got);
        CHECK(strcmp(got, "5") == 0, "focus change and no-op write keep the preset");
        api->set_param(in, "l1_cutoff", "12");
        api->get_param(in, "preset", got, sizeof got);
        CHECK(strcmp(got, "-1") == 0, "a real edit detaches the preset");
        api->destroy_instance(in);
    }

    /* LFO to Amp must not step at the 16-sample control rate. */
    {
        double share[2];
        for (int pass = 0; pass < 2; pass++) {
            void *in = smooth_tone(api);
            api->set_param(in, "l1_lfo1_rate", "0.9");
            api->set_param(in, "l1_m1_src", "LFO1");
            api->set_param(in, "l1_m1_dst", "Amp Level");
            api->set_param(in, "l1_m1_amt", pass ? "1" : "0");
            render_rms(api, in, 20, 0, 0);
            static float x[128 * 100];
            int16_t out[128 * 2];
            for (int b = 0; b < 100; b++) {
                api->render_block(in, out, 128);
                for (int i = 0; i < 128; i++) x[b * 128 + i] = out[i * 2];
            }
            share[pass] = band_share(x, 128 * 100, 44100.0 / 16.0, 44100.0);
            api->destroy_instance(in);
        }
        printf("control-rate band share: steady %.3g, LFO->Amp %.3g\n",
               share[0], share[1]);
        /* Stepped gains measured ~1.5e5 x the steady share; ramped ~1e2 x
         * (AM sidebands of the tone's own harmonics). */
        CHECK(share[1] < share[0] * 1000.0, "LFO->Amp has no control-rate zipper");
    }

    /* Master volume and an FX type fade ramp inside the block. */
    {
        void *in = smooth_tone(api);
        int last = 0;
        max_step(api, in, 1, &last);
        int steady = max_step(api, in, 4, &last);
        api->set_param(in, "master_vol", "0.1");
        int jump = max_step(api, in, 4, &last);
        printf("master vol step: steady %d, change %d\n", steady, jump);
        CHECK(jump < steady * 2, "master volume change is ramped");
        api->destroy_instance(in);
    }
    {
        AminoInstance *in = smooth_tone(api);
        /* EQ low shelf at -15 dB, faded in from Off.  Set the knobs first and
         * switch the slot directly so no defaults load. */
        static const char *const eq[6] = { "0", "0.5", "0.44", "0.5", "0.42", "0" };
        static const char *const keys[6] = { "fx1_p1", "fx1_p2", "fx1_p3",
                                             "fx1_p4", "fx1_p5", "fx1_p6" };
        for (int i = 0; i < 6; i++) api->set_param(in, keys[i], eq[i]);
        api->set_param(in, "fx1_mix", "1");
        api->set_param(in, "l1_route", "FX1");
        int last = 0;
        max_step(api, in, 1, &last);
        int steady = max_step(api, in, 4, &last);
        in->g.fx1_type = 7.0f;
        amino_fx_set_type(in, 0, 7);
        int jump = max_step(api, in, 12, &last);
        printf("fx fade step: steady %d, fade %d\n", steady, jump);
        CHECK(jump < steady * 2, "FX type fade is ramped");
        api->destroy_instance(in);
    }

    /* A reused pool slot starts clean, including fields after the FX state. */
    {
        AminoInstance *a = api->create_instance("", 0);
        a->rui_tick = 123.0;
        a->rui_on = 1;
        a->rui_devms = 77;
        a->aud_ch[0] = 9;
        api->destroy_instance(a);
        AminoInstance *b = api->create_instance("", 0);
        CHECK(b == a, "pool reuses the freed slot");
        CHECK(b->rui_tick == 0.0 && b->rui_on == 0 && b->rui_devms == 0 &&
              b->aud_ch[0] == 0, "recreated instance has no stale fields");
        api->destroy_instance(b);
    }

    g_clock_status = saved_clock;
}

/* Production pass on the factory bank (docs/18): AC-coupled voices, the
 * Patch Gain trim and the documented Delay Time curve. */
static void bank_polish_tests(plugin_api_v2_t *api) {
    int saved_clock = g_clock_status;
    g_clock_status = MOVE_CLOCK_STATUS_STOPPED;

    /* DR Fractal Engine (Fold drive on an asymmetric table) used to sit at
     * about -0.29 full-scale DC. */
    {
        void *in = api->create_instance("", 0);
        api->set_param(in, "preset", "53");
        api->set_param(in, "fx2_type", "Off");
        api->on_midi(in, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
        render_rms(api, in, 100, 0, 0);
        int16_t out[128 * 2];
        double sum = 0.0;
        for (int b = 0; b < 200; b++) {
            api->render_block(in, out, 128);
            for (int i = 0; i < 256; i++) sum += out[i];
        }
        double dc = sum / (200.0 * 256.0 * 32768.0);
        printf("fractal engine dc %.5f\n", dc);
        CHECK(fabs(dc) < 0.002, "voices are AC-coupled (no DC on the output)");
        api->destroy_instance(in);
    }

    /* Patch Gain is a clean dB trim, and a blob without it plays at 0 dB. */
    {
        double r[2];
        for (int pass = 0; pass < 2; pass++) {
            void *in = api->create_instance("", 0);
            api->set_param(in, "patch_gain", pass ? "6" : "0");
            api->on_midi(in, (const uint8_t[]){ 0x90, 48, 60 }, 3, 0);
            render_rms(api, in, 20, 0, 0);
            r[pass] = render_rms(api, in, 100, 0, 0);
            api->destroy_instance(in);
        }
        double db = 20.0 * log10(r[1] / r[0]);
        printf("patch gain +6 dB -> %.2f dB\n", db);
        CHECK(fabs(db - 6.0) < 0.5, "Patch Gain +6 dB raises the output ~6 dB");

        void *in = api->create_instance("", 0);
        char got[16];
        api->set_param(in, "patch_gain", "12");
        api->set_param(in, "state", "{\"fmt\":\"amino\",\"v\":\"2\",\"l1_cutoff\":\"90\"}");
        api->get_param(in, "patch_gain", got, sizeof got);
        CHECK(near_str(got, 0.0), "a blob without patch_gain loads at 0 dB");
        api->destroy_instance(in);
    }

    /* Delay Time follows docs/11 (1..2000 ms, exp): knob .7707 = 350 ms. */
    {
        AminoInstance *in = api->create_instance("", 0);
        static const char *const keys[6] = { "fx1_p1", "fx1_p2", "fx1_p3",
                                             "fx1_p4", "fx1_p5", "fx1_p6" };
        static const char *const vals[6] = { "0.7707", "0", "0", "0.5", "0", "0" };
        for (int i = 0; i < 6; i++) api->set_param(in, keys[i], vals[i]);
        api->set_param(in, "fx1_mix", "1");
        in->g.fx1_type = 8.0f;
        amino_fx_set_type(in, 0, 8);
        float l[128], r[128];
        for (int b = 0; b < 40; b++) {
            memset(l, 0, sizeof l);
            memset(r, 0, sizeof r);
            amino_fx_process(in, 0, l, r, 128);
        }
        int at = -1;
        float best = 0.0f;
        for (int b = 0; b < 300; b++) {
            memset(l, 0, sizeof l);
            memset(r, 0, sizeof r);
            if (b == 0) l[0] = r[0] = 1.0f;
            amino_fx_process(in, 0, l, r, 128);
            for (int i = 0; i < 128; i++)
                if (fabsf(l[i]) > best && b * 128 + i > 64) { best = fabsf(l[i]); at = b * 128 + i; }
        }
        printf("delay echo at %d samples (%.1f ms)\n", at, at * 1000.0 / 44100.0);
        CHECK(abs(at - 15435) < 60, "Delay Time knob .7707 echoes at 350 ms");
        api->destroy_instance(in);
    }

    /* A v1 blob's Delay Time knob is converted so the delay keeps its time:
     * linear 500 ms (.2496) -> exp knob log(500)/log(2000) = .818. */
    {
        void *in = api->create_instance("", 0);
        char got[16];
        api->set_param(in, "state",
                       "{\"fmt\":\"amino\",\"v\":\"1\",\"fx1_type\":\"8\",\"fx1_p1\":\"0.2496\","
                       "\"fx2_type\":\"9\",\"fx2_p1\":\"0.2496\"}");
        api->get_param(in, "fx1_p1", got, sizeof got);
        CHECK(near_str(got, 0.818), "v1 Delay Time migrates to the exp curve");
        api->get_param(in, "fx2_p1", got, sizeof got);
        CHECK(near_str(got, 0.250), "non-Delay slots keep their p1");
        api->destroy_instance(in);
    }

    g_clock_status = saved_clock;
}

int main(void) {
    host_api_v1_t host;
    memset(&host, 0, sizeof host);
    host.api_version = MOVE_PLUGIN_API_VERSION;
    host.sample_rate = 44100;
    host.frames_per_block = 128;
    host.get_bpm = host_bpm;
    host.get_clock_status = host_clock;
    host.get_beat_position = host_beats;

    plugin_api_v2_t *api = move_plugin_init_v2(&host);
    CHECK(api != NULL, "init");
    if (!api) return 1;
    CHECK(api->api_version == 2, "api version");
    CHECK(fabsf(am_via_multiplier(-1.0f, 0.0f) - 1.0f) < 1e-6f,
          "Via depth zero leaves the base route unscaled");
    CHECK(fabsf(am_via_multiplier(-1.0f, 1.0f) + 1.0f) < 1e-6f,
          "Via depth one preserves the legacy multiplier");
    CHECK(fabsf(am_via_multiplier(-1.0f, 0.5f)) < 1e-6f,
          "Via depth interpolates between neutral and full scaling");

    void *inst = api->create_instance("modules/sound_generators/amino", 0);
    CHECK(inst != NULL, "create");
    if (!inst) return 1;

    static char buf[131072];
    int n = api->get_param(inst, "chain_params", buf, sizeof buf);
    printf("chain_params: %d bytes\n", n);
    CHECK(n > 1000 && n < 65536, "chain_params size");
    n = api->get_param(inst, "ui_hierarchy", buf, sizeof buf);
    printf("ui_hierarchy: %d bytes\n", n);
    CHECK(n < 0, "custom UI refuses the host hierarchy so ui_chain can load");
    n = api->get_param(inst, "ui_pages", buf, sizeof buf);
    CHECK(n > 1000 && n < 65536, "custom UI page contract size");
    CHECK(strstr(buf, "\"levels\"") != NULL && strstr(buf, "\"root\"") != NULL,
          "ui_pages exposes direct levels object");
    CHECK(strstr(buf, "\"ui_hierarchy\"") == NULL,
          "ui_pages response is not doubly wrapped");
    CHECK(strstr(buf, "\"modmatrix\"") != NULL &&
          strstr(buf, "\"key\":\"mod_matrix\"") != NULL,
          "custom UI page tree includes Pad Matrix");
    CHECK(strstr(buf, "\"child_names\":[\"Layer Tools (L1)\",\"Layer Tools (L2)\"") != NULL,
          "dedicated layer-tools page retains child actions");
    CHECK(strstr(buf, "\"fx1_reverb\":{\"name\":\"FX 1 Reverb\"") != NULL &&
          strstr(buf, "{\"key\":\"fx1_p2\",\"short_name\":\"Decay\"}") != NULL &&
          strstr(buf, "\"visible_if\":{\"param\":\"fx1_type\",\"equals\":9}") != NULL,
          "FX 1 has a Reverb page gated on its type, with named knobs");
    n = api->get_param(inst, "state", buf, sizeof buf);
    printf("state: %d bytes\n", n);
    CHECK(n > 5000 && n < 65536, "state size");
    CHECK(strstr(buf, "\"fmt\":\"amino\"") != NULL, "state fmt");
    CHECK(strstr(buf, "\"l4_m8_via\":") != NULL, "state l4 key");
    CHECK(strstr(buf, "\"l4_m8_via_depth\":") != NULL,
          "state includes the new Via Depth parameter");
    n = api->get_param(inst, "mod_matrix_state", buf, sizeof buf);
    CHECK(n > 100 && strstr(buf, "\"routes\"") && strstr(buf, "\"lfos\"") &&
          strstr(buf, "\"envs\""), "matrix page receives one compact live snapshot");
    n = api->get_param(inst, "preset_count", buf, sizeof buf);
    printf("preset_count: %s\n", buf);
    char expected_preset_count[16];
    snprintf(expected_preset_count, sizeof expected_preset_count, "%d",
             amino_factory_preset_count);
    CHECK(strcmp(buf, expected_preset_count) == 0, "preset count");
    n = api->get_param(inst, "preset_name", buf, sizeof buf);
    printf("preset_name: %s\n", buf);
    CHECK(strcmp(buf, "Init") == 0, "init name");
    api->get_param(inst, "fx_chain", buf, sizeof buf);
    CHECK(strcmp(buf, "1") == 0, "default FX chain is Serial");

    n = api->get_param(inst, "layer_src_list", buf, sizeof buf);
    printf("layer_src_list: %d bytes\n", n);
    CHECK(n > 100 && n < 65536, "layer list size");

    n = api->get_param(inst, "preset_list", buf, sizeof buf);
    printf("preset_list: %d bytes\n", n);
    CHECK(n > 1000 && buf[0] == '[' && n < 65536, "preset list");

    /* Remote UI digest: rev:on:tick:bpm[:devms]. */
    g_beats = 4.0;
    render_rms(api, inst, 1, 0, 0);
    n = api->get_param(inst, "rui_poll", buf, sizeof buf);
    printf("rui_poll: %s\n", buf);
    unsigned r0 = 0, on = 0, devms = 0;
    unsigned long long tick = 0;
    float poll_bpm = 0.0f;
    int fields = sscanf(buf, "%u:%u:%llu:%f:%u", &r0, &on, &tick, &poll_bpm, &devms);
    CHECK(fields == 5, "rui_poll playing format");
    CHECK(r0 == 0, "rui_poll initial rev");
    CHECK(on == 1, "rui_poll transport on");
    CHECK(tick == 96, "rui_poll 24-PPQN tick");
    CHECK(fabs((double)poll_bpm - 120.0) < 0.05, "rui_poll bpm");

    api->set_param(inst, "l1_cutoff", "51");
    n = api->get_param(inst, "rui_poll", buf, sizeof buf);
    unsigned r1 = (unsigned)strtoul(buf, 0, 10);
    CHECK(r1 == r0 + 1, "rui_poll rev bumps on edit");
    api->set_param(inst, "l1_cutoff", "51");
    n = api->get_param(inst, "rui_poll", buf, sizeof buf);
    CHECK((unsigned)strtoul(buf, 0, 10) == r1, "rui_poll rev stable on no-op");

    g_clock_status = MOVE_CLOCK_STATUS_STOPPED;
    g_beats = 99.0;
    render_rms(api, inst, 1, 0, 0);
    char p1[64], p2[64];
    api->get_param(inst, "rui_poll", p1, sizeof p1);
    api->get_param(inst, "rui_poll", p2, sizeof p2);
    CHECK(strcmp(p1, p2) == 0, "rui_poll stopped digest stable");
    unsigned stopped_rev = 0;
    CHECK(sscanf(p1, "%u:%u:%llu:%f", &stopped_rev, &on, &tick, &poll_bpm) == 4 &&
          on == 0, "rui_poll stopped format");
    g_clock_status = MOVE_CLOCK_STATUS_RUNNING;

    /* The live host tempo and transport position are reflected by the module. */
    g_bpm = 105.0f;
    g_beats = 8.5;
    void *tempo_inst = api->create_instance("", 0);
    CHECK(tempo_inst != NULL, "tempo startup instance");
    if (tempo_inst) {
        api->get_param(tempo_inst, "rui_poll", buf, sizeof buf);
        CHECK(sscanf(buf, "%u:%u:%llu:%f", &r0, &on, &tick, &poll_bpm) == 4 &&
              fabs((double)poll_bpm - 105.0) < 0.05,
              "host tempo sampled at instance creation");
        api->destroy_instance(tempo_inst);
    }
    render_rms(api, inst, 1, 0, 0);
    api->get_param(inst, "rui_poll", buf, sizeof buf);
    CHECK(sscanf(buf, "%u:%u:%llu:%f", &r0, &on, &tick, &poll_bpm) == 4 &&
          on == 1 && tick == 204 && fabs((double)poll_bpm - 105.0) < 0.05,
          "host tempo and beat position update live");
    g_bpm = 120.0f;
    g_beats = 0.0;

    api->get_param(inst, "l1_m1_via_depth", buf, sizeof buf);
    CHECK(strcmp(buf, "1.000") == 0, "Via Depth defaults to legacy full-strength behavior");
    api->set_param(inst, "l1_m1_via_depth", "0.250");
    api->get_param(inst, "l1_m1_via_depth", buf, sizeof buf);
    CHECK(strcmp(buf, "0.250") == 0, "Via Depth can be read and written as a saved parameter");
    api->set_param(inst, "l1_m1_src", "LFO1");
    api->set_param(inst, "l1_m1_dst", "Cutoff");
    api->set_param(inst, "l1_m1_amt", "0.500");
    api->set_param(inst, "l1_m1_via", "ModWheel");
    n = api->get_param(inst, "mod_matrix_state", buf, sizeof buf);
    CHECK(n > 100 && strstr(buf, "\"routes\":[[4,9,0.5000,13,0.2500]") != NULL,
          "matrix snapshot serializes source, target, amounts and Via Depth in slot order");

    /* Random LFO/flavour streams are preset+voice seeded, independent of the
     * instance slot. Repeated renders of the same random-LFO preset must match. */
    void *rng_a = api->create_instance("", 0);
    void *rng_b = api->create_instance("", 0);
    CHECK(rng_a != NULL && rng_b != NULL, "random parity instances");
    if (rng_a && rng_b) {
        api->set_param(rng_a, "preset", "79");
        api->set_param(rng_b, "preset", "79");
        static const uint8_t rng_notes[] = { 48, 55, 60 };
        for (int i = 0; i < 3; i++) {
            api->on_midi(rng_a, (const uint8_t[]){ 0x90, rng_notes[i], 110 }, 3, 0);
            api->on_midi(rng_b, (const uint8_t[]){ 0x90, rng_notes[i], 110 }, 3, 0);
        }
        int16_t ra[256], rb[256];
        for (int b = 0; b < 12; b++) {
            api->render_block(rng_a, ra, 128);
            api->render_block(rng_b, rb, 128);
            CHECK(memcmp(ra, rb, sizeof ra) == 0,
                  "random LFO preset repeats across instance slots");
        }
        api->destroy_instance(rng_a);
        api->destroy_instance(rng_b);
    } else {
        if (rng_a) api->destroy_instance(rng_a);
        if (rng_b) api->destroy_instance(rng_b);
    }

    /* Mono retriggers restart envelopes but preserve the running oscillator,
     * filter, and LFO phases, matching the emulator's Mono behavior. */
    void *mono_phase_inst = api->create_instance("", 0);
    CHECK(mono_phase_inst != NULL, "mono phase instance");
    if (mono_phase_inst) {
        api->set_param(mono_phase_inst, "preset", "205");
        api->on_midi(mono_phase_inst, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
        render_rms(api, mono_phase_inst, 1, 0, 0);
        AminoVoice *mv = &((AminoInstance *)mono_phase_inst)->v[0];
        float p1 = mv->p1, lfo_phase = mv->lfo[0].phase, filter_z = mv->z[0];
        api->on_midi(mono_phase_inst, (const uint8_t[]){ 0x90, 55, 100 }, 3, 0);
        CHECK(fabsf(mv->p1 - p1) < 1e-6f &&
              fabsf(mv->lfo[0].phase - lfo_phase) < 1e-6f &&
              fabsf(mv->z[0] - filter_z) < 1e-6f,
              "Mono retrigger preserves oscillator/filter/LFO state");
        CHECK(mv->env_stage[2] == AMINO_ENV_ATTACK,
              "Mono retrigger restarts amp envelope");
        api->destroy_instance(mono_phase_inst);
    }

    /* Init should be a clean, normalized saw rather than hard-clipped table
     * bytes. This catches accidental loss of the signed-8-bit /128 scaling. */
    for (int p = 0; p < 3; p++) {
        static const int parallel_presets[] = { 56, 57, 122 };
        char preset_num[8];
        snprintf(preset_num, sizeof preset_num, "%d", parallel_presets[p]);
        api->set_param(inst, "preset", preset_num);
        api->get_param(inst, "fx_chain", buf, sizeof buf);
        CHECK(strcmp(buf, "0") == 0,
              "factory presets that authored parallel FX retain Parallel");
    }
    api->set_param(inst, "preset", "0");
    api->get_param(inst, "fx_chain", buf, sizeof buf);
    CHECK(strcmp(buf, "1") == 0, "Init preset keeps the FX chain Serial");
    api->on_midi(inst, (const uint8_t[]){ 0x90, 48, 102 }, 3, 0);
    int16_t init_block[256];
    api->render_block(inst, init_block, 128);
    int init_nonzero = 0;
    for (int i = 0; i < 128; i++) if (init_block[i * 2] != 0) init_nonzero++;
    CHECK(init_nonzero > 100, "Init fills full render block across control slices");
    int init_peak = 0;
    double init_rms = render_rms(api, inst, 199, 0, &init_peak);
    printf("Init rms %.1f peak %d\n", init_rms, init_peak);
    CHECK(init_rms > 2000.0 && init_rms < 10000.0, "Init level normalized and audible");
    CHECK(init_peak < 30000, "Init not hard-clipped");
    api->on_midi(inst, (const uint8_t[]){ 0xB0, 123, 0 }, 3, 0);
    render_rms(api, inst, 10, 0, 0);

    /* Global flavour is a distinct key from the per-layer flavour. */
    api->set_param(inst, "flavour_global", "0.4");
    n = api->get_param(inst, "flavour_global", buf, sizeof buf);
    CHECK(near_str(buf, 0.4), "global flavour");
    api->set_param(inst, "l1_flavour", "0.25");
    n = api->get_param(inst, "l1_flavour:effective", buf, sizeof buf);
    CHECK(near_str(buf, 0.25), "layer flavour effective");

    /* Enum accepts names, reports indices. */
    api->set_param(inst, "fx1_type", "Reverb");
    n = api->get_param(inst, "fx1_type", buf, sizeof buf);
    printf("fx1_type after name: %s\n", buf);
    CHECK(strcmp(buf, "9") == 0, "enum name set");
    api->set_param(inst, "fx1_type", "Off");

    /* Preset render. */
    api->set_param(inst, "preset", "1");
    n = api->get_param(inst, "preset_name", buf, sizeof buf);
    printf("preset 1 name: %s\n", buf);
    api->on_midi(inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
    api->on_midi(inst, (const uint8_t[]){ 0x90, 64, 100 }, 3, 0);
    api->on_midi(inst, (const uint8_t[]){ 0x90, 67, 100 }, 3, 0);
    int peak = 0;
    double rms = render_rms(api, inst, 40, 0, &peak);
    printf("preset1 rms %.1f peak %d\n", rms, peak);
    CHECK(rms > 100.0, "preset 1 audible");
    CHECK(peak < 32767, "preset 1 not clipped hard");
    api->on_midi(inst, (const uint8_t[]){ 0xB0, 123, 0 }, 3, 0);
    render_rms(api, inst, 10, 0, 0);

    /* A rapid chord retrigger must reserve a separate release voice for each
     * chord tone, even when the patch uses its full polyphony for that chord. */
    void *chord_inst = api->create_instance("", 0);
    CHECK(chord_inst != NULL, "chord regression instance");
    if (chord_inst) {
        static const uint8_t old_roots[] = { 60, 64, 67, 71 };
        static const uint8_t new_roots[] = { 62, 65, 69, 72 };
        static const int chord_intervals[] = { 0, 4, 7, 11 };
        api->set_param(chord_inst, "voices", "16");
        api->set_param(chord_inst, "l1_chord", "Maj7");
        api->set_param(chord_inst, "l1_e3_r", "1");
        for (int i = 0; i < 4; i++)
            api->on_midi(chord_inst, (const uint8_t[]){ 0x90, old_roots[i], 100 }, 3, 0);
        render_rms(api, chord_inst, 8, 0, 0); /* establish a long release tail */
        for (int i = 0; i < 4; i++)
            api->on_midi(chord_inst, (const uint8_t[]){ 0x80, old_roots[i], 0 }, 3, 0);
        for (int i = 0; i < 4; i++)
            api->on_midi(chord_inst, (const uint8_t[]){ 0x90, new_roots[i], 100 }, 3, 0);

        AminoInstance *ci = (AminoInstance *)chord_inst;
        int pending = 0;
        int pending_per_root[4] = { 0, 0, 0, 0 };
        unsigned pending_masks[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < AMINO_MAX_VOICES; i++) {
            AminoVoice *v = &ci->v[i];
            if (v->pending_layer != 0) continue;
            for (int root = 0; root < 4; root++) {
                if (v->pending_root != new_roots[root]) continue;
                pending++;
                pending_per_root[root]++;
                for (int tone = 0; tone < 4; tone++)
                    if (v->pending_note == new_roots[root] + chord_intervals[tone])
                        pending_masks[root] |= 1u << tone;
            }
        }
        CHECK(pending == 16, "rapid MIDI chord queues all sixteen voices");
        for (int root = 0; root < 4; root++)
            CHECK(pending_per_root[root] == 4 && pending_masks[root] == 15,
                  "rapid retrigger preserves every generated chord tone");
        render_rms(api, chord_inst, 2, 0, 0);
        int playing = 0;
        int playing_per_root[4] = { 0, 0, 0, 0 };
        unsigned playing_masks[4] = { 0, 0, 0, 0 };
        for (int i = 0; i < AMINO_MAX_VOICES; i++) {
            AminoVoice *v = &ci->v[i];
            if (!v->active || !v->gate) continue;
            for (int root = 0; root < 4; root++) {
                if (v->root_note != new_roots[root]) continue;
                playing++;
                playing_per_root[root]++;
                for (int tone = 0; tone < 4; tone++)
                    if (v->note == new_roots[root] + chord_intervals[tone])
                        playing_masks[root] |= 1u << tone;
            }
        }
        CHECK(playing == 16, "new four-note chord progression plays over long tails");
        for (int root = 0; root < 4; root++)
            CHECK(playing_per_root[root] == 4 && playing_masks[root] == 15,
                  "all notes in each expanded chord reach the voices");
        api->destroy_instance(chord_inst);
    }

    /* A sequence of four ordinary MIDI notes must retrigger over long release
     * tails without requiring chord expansion or more than eight voices. */
    void *sequence_inst = api->create_instance("", 0);
    CHECK(sequence_inst != NULL, "four-note sequence regression instance");
    if (sequence_inst) {
        static const uint8_t old_roots[] = { 60, 64, 67, 71, 56, 60, 63, 68 };
        static const uint8_t new_roots[] = { 62, 65, 69, 72 };
        api->set_param(sequence_inst, "voices", "8");
        api->set_param(sequence_inst, "l1_chord", "Off");
        api->set_param(sequence_inst, "l1_e3_r", "1");
        for (int i = 0; i < 8; i++)
            api->on_midi(sequence_inst, (const uint8_t[]){ 0x90, old_roots[i], 100 }, 3, 0);
        render_rms(api, sequence_inst, 8, 0, 0);
        for (int i = 0; i < 8; i++)
            api->on_midi(sequence_inst, (const uint8_t[]){ 0x80, old_roots[i], 0 }, 3, 0);
        for (int i = 0; i < 4; i++)
            api->on_midi(sequence_inst, (const uint8_t[]){ 0x90, new_roots[i], 100 }, 3, 0);

        AminoInstance *qi = (AminoInstance *)sequence_inst;
        int pending = 0;
        unsigned pending_mask = 0;
        for (int i = 0; i < AMINO_MAX_VOICES; i++) {
            AminoVoice *v = &qi->v[i];
            if (v->pending_layer != 0 || v->pending_root != v->pending_note) continue;
            for (int note = 0; note < 4; note++) {
                if (v->pending_note == new_roots[note]) {
                    pending++;
                    pending_mask |= 1u << note;
                }
            }
        }
        CHECK(pending == 4 && pending_mask == 15,
              "four MIDI chord notes queue on distinct release voices");
        render_rms(api, sequence_inst, 2, 0, 0);
        int playing = 0;
        unsigned playing_mask = 0;
        for (int i = 0; i < AMINO_MAX_VOICES; i++) {
            AminoVoice *v = &qi->v[i];
            if (!v->active || !v->gate || v->root_note != v->note) continue;
            for (int note = 0; note < 4; note++) {
                if (v->note == new_roots[note]) {
                    playing++;
                    playing_mask |= 1u << note;
                }
            }
        }
        CHECK(playing == 4 && playing_mask == 15,
              "four MIDI chord notes play over long release tails");
        api->destroy_instance(sequence_inst);
    }

    /* Continuous layer controls (filter, dirt, drive, etc.) should ramp toward
     * live targets rather than jumping at the next audio block. */
    void *smooth_inst = api->create_instance("", 0);
    CHECK(smooth_inst != NULL, "parameter smoothing regression instance");
    if (smooth_inst) {
        api->set_param(smooth_inst, "l1_m1_src", "LFO1");
        api->set_param(smooth_inst, "l1_m1_dst", "Cutoff");
        api->set_param(smooth_inst, "l1_m1_amt", "0");
        api->set_param(smooth_inst, "l1_xmod_type", "PM");
        api->on_midi(smooth_inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
        render_rms(api, smooth_inst, 1, 0, 0);
        AminoInstance *si = (AminoInstance *)smooth_inst;
        CHECK(si->lr[0].smooth_ready, "layer continuous controls initialized");
        si->dirty_tables = 0;
        si->dirty_mods = 0;
        api->set_param(smooth_inst, "l1_fenv", "1");
        api->set_param(smooth_inst, "l1_e2_d", "1");
        api->set_param(smooth_inst, "l1_dirt", "1");
        api->set_param(smooth_inst, "l1_drv_type", "Trans");
        api->set_param(smooth_inst, "l1_drive", "1");
        api->set_param(smooth_inst, "l1_m1_amt", "1");
        api->set_param(smooth_inst, "l1_xmod", "1");
        render_rms(api, smooth_inst, 1, 0, 0);
        LayerParams *sp = &si->lr[0].smooth_params;
        float first_slice_xmod = si->lr[0].smooth_slices[0].xmod;
        CHECK(sp->fenv > 0.0f && sp->fenv < 1.0f &&
              sp->e2_d > 0.4f && sp->e2_d < 1.0f &&
              sp->dirt > 0.0f && sp->dirt < 1.0f &&
              sp->drive > 0.0f && sp->drive < 1.0f &&
              sp->m1_amt > 0.0f && sp->m1_amt < 1.0f &&
              sp->xmod > 0.0f && sp->xmod < 1.0f,
              "filter, dirt, drive and modulation controls smooth toward targets");
        CHECK(first_slice_xmod > 0.0f && first_slice_xmod < 0.02f,
              "continuous controls use a click-resistant 30 ms ramp");
        CHECK(si->dirty_tables == 0 && si->dirty_mods == 0,
              "ordinary parameter edits do not rebuild table/modulation caches");
        render_rms(api, smooth_inst, 60, 0, 0);
        CHECK(sp->fenv > 0.99f && sp->e2_d > 0.99f &&
              sp->dirt > 0.99f && sp->drive > 0.99f &&
              sp->m1_amt > 0.99f && sp->xmod > 0.99f,
              "continuous layer controls settle at live targets");
        api->destroy_instance(smooth_inst);
    }

    /* Reverb Size must affect an already allocated effect, not only initialize
     * its delay-line lengths when the effect type is selected. */
    void *reverb_inst = api->create_instance("", 0);
    CHECK(reverb_inst != NULL, "reverb size regression instance");
    if (reverb_inst) {
        AminoInstance *ri = (AminoInstance *)reverb_inst;
        api->set_param(reverb_inst, "fx1_type", "Reverb");
        render_rms(api, reverb_inst, 12, 0, 0); /* complete incremental arena clear */
        int old_size = ri->fx[0].st.len[0];
        api->set_param(reverb_inst, "fx1_p1", "1");
        render_rms(api, reverb_inst, 80, 0, 0);
        CHECK(ri->fx[0].type == 9 && ri->fx[0].st.len[0] > old_size,
              "Reverb Size changes line lengths while active");
        api->destroy_instance(reverb_inst);
    }

    /* Synced effect phases should re-lock smoothly after a transport stop,
     * and a tempo change should crossfade delay taps rather than Doppler-sweep
     * the sounding tail. */
    void *sync_fx_inst = api->create_instance("", 0);
    CHECK(sync_fx_inst != NULL, "synced FX transport regression instance");
    if (sync_fx_inst) {
        AminoInstance *sxi = (AminoInstance *)sync_fx_inst;
        api->set_param(sync_fx_inst, "fx1_type", "Flanger");
        api->set_param(sync_fx_inst, "fx1_p6", "1");
        g_clock_status = MOVE_CLOCK_STATUS_RUNNING;
        g_bpm = 120.0f;
        g_beats = 8.0;
        render_rms(api, sync_fx_inst, 4, 0, 0);
        CHECK(sxi->fx[0].type == 2 && sxi->fx[0].st.sync_locked,
              "synced Flanger follows a running transport");
        g_clock_status = MOVE_CLOCK_STATUS_STOPPED;
        render_rms(api, sync_fx_inst, 1, 0, 0);
        float stopped_phase = sxi->fx[0].st.phase;
        g_clock_status = MOVE_CLOCK_STATUS_RUNNING;
        g_beats = 20.25;
        render_rms(api, sync_fx_inst, 1, 0, 0);
        float phase_delta = fabsf(sxi->fx[0].st.phase - stopped_phase);
        if (phase_delta > 0.5f) phase_delta = 1.0f - phase_delta;
        CHECK(phase_delta < 0.05f,
              "Flanger phase does not jump when transport resumes");
        api->destroy_instance(sync_fx_inst);
    }

    void *tempo_fx_inst = api->create_instance("", 0);
    CHECK(tempo_fx_inst != NULL, "synced delay tempo regression instance");
    if (tempo_fx_inst) {
        AminoInstance *txi = (AminoInstance *)tempo_fx_inst;
        api->set_param(tempo_fx_inst, "fx1_type", "Delay");
        g_bpm = 120.0f;
        render_rms(api, tempo_fx_inst, 20, 0, 0); /* finish clear and prime tap */
        float old_delay = txi->fx[0].st.dtarget;
        g_bpm = 90.0f;
        render_rms(api, tempo_fx_inst, 1, 0, 0);
        CHECK(txi->fx[0].st.dtarget != old_delay &&
              txi->fx[0].st.dfade > 0.0f && txi->fx[0].st.dfade < 1.0f &&
              fabsf(txi->fx[0].st.dt - old_delay) <
                  fabsf(txi->fx[0].st.dtarget - old_delay) * 0.1f,
              "tempo change crossfades synced Delay taps without a pitch sweep");
        g_bpm = 120.0f;
        api->destroy_instance(tempo_fx_inst);
    }
    g_clock_status = MOVE_CLOCK_STATUS_RUNNING;
    g_bpm = 120.0f;
    g_beats = 0.0;

    /* State round-trip on a richer preset. */
    api->set_param(inst, "preset", "100");
    api->get_param(inst, "l1_scale", buf, sizeof buf);
    CHECK(strcmp(buf, "0") == 0,
          "factory preset leaves scale at the user's default instead of forcing Set");
    static char s1[131072], s2[131072];
    int n1 = api->get_param(inst, "state", s1, sizeof s1);
    CHECK(n1 > 0, "state 1");
    api->set_param(inst, "state", s1);
    int n2 = api->get_param(inst, "state", s2, sizeof s2);
    CHECK(n2 == n1, "state length stable");
    CHECK(strcmp(s1, s2) == 0, "state byte-identical round trip");
    printf("state round trip: %d bytes\n", n1);

    /* Every factory preset must round-trip through the flat state blob. */
    int rt_bad = 0;
    for (int p = 0; p < amino_factory_preset_count; p++) {
        char ps[16];
        snprintf(ps, sizeof ps, "%d", p);
        api->set_param(inst, "preset", ps);
        int a = api->get_param(inst, "state", s1, sizeof s1);
        api->set_param(inst, "state", s1);
        int b = api->get_param(inst, "state", s2, sizeof s2);
        if (a != b || strcmp(s1, s2) != 0) {
            rt_bad++;
            printf("state mismatch preset %d (%d/%d)\n", p, a, b);
        }
    }
    CHECK(rt_bad == 0, "state round trip all presets");

    /* Every factory preset must load and render without NaN. */
    int bad = 0;
    for (int p = 0; p < amino_factory_preset_count; p++) {
        char ps[16];
        snprintf(ps, sizeof ps, "%d", p);
        api->set_param(inst, "preset", ps);
        api->on_midi(inst, (const uint8_t[]){ 0x90, 60, 90 }, 3, 0);
        int pk = 0;
        double rr = render_rms(api, inst, 4, 0, &pk);
        if (!(rr == rr) || pk > 32767) { bad++; printf("bad preset %d rms %f pk %d\n", p, rr, pk); }
        api->on_midi(inst, (const uint8_t[]){ 0xB0, 123, 0 }, 3, 0);
        render_rms(api, inst, 2, 0, 0);
    }
    CHECK(bad == 0, "all presets finite");

    /* FX type sweep, including both chains. */
    api->set_param(inst, "preset", "1");
    api->set_param(inst, "fx1_mix", "0.6");
    api->set_param(inst, "fx2_mix", "0.6");
    api->set_param(inst, "l1_route", "FX1");
    for (int chain = 0; chain < 2; chain++) {
        api->set_param(inst, "fx_chain", chain ? "Serial" : "Parallel");
        for (int t = 1; t <= 11; t++) {
            char ts[8];
            snprintf(ts, sizeof ts, "%d", t);
            api->set_param(inst, "fx1_type", ts);
            api->on_midi(inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
            int pk = 0;
            double rr = render_rms(api, inst, 80, 0, &pk);
            if (!(rr == rr) || pk > 32767) {
                bad++;
                printf("bad fx %d chain %d rms %f pk %d\n", t, chain, rr, pk);
            }
            api->on_midi(inst, (const uint8_t[]){ 0xB0, 123, 0 }, 3, 0);
            render_rms(api, inst, 4, 0, 0);
        }
    }
    CHECK(bad == 0, "fx sweep finite");

    /* Trance Gate syncs its 16-step pattern to the host beat grid and honors
     * the preset's open/closed steps at full wet mix. */
    void *gate_inst = api->create_instance("", 0);
    CHECK(gate_inst != NULL, "trance gate regression instance");
    if (gate_inst) {
        AminoInstance *gi = (AminoInstance *)gate_inst;
        api->set_param(gate_inst, "fx1_type", "Trance Gate");
        api->set_param(gate_inst, "fx1_mix", "1");
        api->set_param(gate_inst, "fx1_p1", "0"); /* Classic 8ths */
        api->set_param(gate_inst, "fx1_p2", "0"); /* 1/8 */
        api->set_param(gate_inst, "fx1_p3", "0");
        api->set_param(gate_inst, "fx1_p4", "0");
        api->set_param(gate_inst, "fx1_p5", "1");
        api->set_param(gate_inst, "fx1_p6", "0");
        gi->sync_transport = 1;
        gi->bpm = 120.0f;
        double open_gain = 0.0, closed_gain = 1.0;
        int total_samples = 0;
        for (int b = 0; b < 280; b++) {
            float left[AMINO_BLOCK], right[AMINO_BLOCK];
            for (int i = 0; i < AMINO_BLOCK; i++) left[i] = right[i] = 1.0f;
            gi->sync_beat = (double)total_samples * 2.0 / (double)gi->sr;
            amino_fx_process(gi, 0, left, right, AMINO_BLOCK);
            double center = ((double)total_samples + AMINO_BLOCK * 0.5) / gi->sr;
            if (center > 0.19 && center < 0.21) open_gain = left[AMINO_BLOCK / 2];
            if (center > 0.69 && center < 0.71) closed_gain = left[AMINO_BLOCK / 2];
            total_samples += AMINO_BLOCK;
        }
        CHECK(gi->fx[0].type == 11 && gi->fx[0].st.sync_locked,
              "Trance Gate selects and locks to transport");
        CHECK(open_gain > 0.8 && closed_gain < 0.05,
              "Trance Gate follows the selected rhythmic pattern");
        api->destroy_instance(gate_inst);
    }

    /* Multi-layer stack. */
    api->set_param(inst, "preset", "0");
    api->set_param(inst, "layer_count", "2");
    n = api->get_param(inst, "l2_on", buf, sizeof buf);
    CHECK(strcmp(buf, "1") == 0, "layer count enables layers");
    api->set_param(inst, "edit_layer", "9");
    n = api->get_param(inst, "edit_layer", buf, sizeof buf);
    CHECK(strcmp(buf, "2") == 0, "edit layer clamped to count");
    api->set_param(inst, "preset", "55");
    api->set_param(inst, "layer_count", "4");
    api->set_param(inst, "layer_mode", "Stack");
    for (int li = 1; li <= 4; li++) {
        char k[32], v[8];
        snprintf(k, sizeof k, "l%d_on", li);
        api->set_param(inst, k, "On");
        snprintf(k, sizeof k, "l%d_amp_vel", li);
        api->set_param(inst, k, "0.5");
        (void)v;
    }
    api->on_midi(inst, (const uint8_t[]){ 0x90, 48, 110 }, 3, 0);
    rms = render_rms(api, inst, 20, 0, &peak);
    printf("4-layer rms %.1f peak %d\n", rms, peak);
    CHECK(rms > 100.0, "4-layer audible");
    api->on_midi(inst, (const uint8_t[]){ 0xB0, 123, 0 }, 3, 0);

    /* Mono / legato and sustain. */
    api->set_param(inst, "l1_poly", "Legato");
    api->set_param(inst, "l1_glide", "0.3");
    api->on_midi(inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
    render_rms(api, inst, 4, 0, 0);
    api->on_midi(inst, (const uint8_t[]){ 0x90, 67, 100 }, 3, 0);
    render_rms(api, inst, 4, 0, 0);
    api->on_midi(inst, (const uint8_t[]){ 0x80, 67, 0 }, 3, 0);
    api->on_midi(inst, (const uint8_t[]){ 0x80, 60, 0 }, 3, 0);
    render_rms(api, inst, 8, 0, 0);

    /* A note-off may arrive while a mono retrigger is still in its anti-click
     * fade. That queued note must be cancelled, not left sustaining forever. */
    void *mono_inst = api->create_instance("", 0);
    CHECK(mono_inst != NULL, "mono regression instance");
    api->set_param(mono_inst, "preset", "6");
    api->set_param(mono_inst, "l1_e1_d", "0");
    api->set_param(mono_inst, "l1_e1_s", "1");
    api->set_param(mono_inst, "l1_e1_r", "0");
    api->set_param(mono_inst, "l1_e3_r", "0");
    api->on_midi(mono_inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
    render_rms(api, mono_inst, 1, 0, 0);
    api->on_midi(mono_inst, (const uint8_t[]){ 0x80, 60, 0 }, 3, 0);
    api->on_midi(mono_inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
    api->on_midi(mono_inst, (const uint8_t[]){ 0x80, 60, 0 }, 3, 0);
    render_rms(api, mono_inst, 350, 0, 0); /* let the filter/release tail settle */
    int mono_peak = 0;
    double mono_tail = render_rms(api, mono_inst, 100, 0, &mono_peak);
    printf("rapid mono tail rms %.3f peak %d\n", mono_tail, mono_peak);
    CHECK(mono_peak <= 1 && mono_tail < 1.0, "rapid mono retrigger does not stick");
    api->destroy_instance(mono_inst);

    /* MIDI channels own note-off, poly pressure and channel-mode messages. */
    void *channel_inst = api->create_instance("", 0);
    CHECK(channel_inst != NULL, "MIDI channel regression instance");
    if (channel_inst) {
        AminoInstance *ci = (AminoInstance *)channel_inst;
        api->set_param(channel_inst, "layer_count", "2");
        api->set_param(channel_inst, "layer_mode", "Chan");
        api->set_param(channel_inst, "l2_midi_ch", "2");
        api->on_midi(channel_inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
        api->on_midi(channel_inst, (const uint8_t[]){ 0x91, 60, 100 }, 3, 0);
        api->on_midi(channel_inst, (const uint8_t[]){ 0x80, 60, 0 }, 3, 0);
        int l1_gate = 0, l2_gate = 0;
        for (int i = 0; i < AMINO_MAX_VOICES; i++) {
            if (!ci->v[i].active || !ci->v[i].gate) continue;
            if (ci->v[i].layer == 0) l1_gate = 1;
            if (ci->v[i].layer == 1) l2_gate = 1;
        }
        CHECK(!l1_gate && l2_gate,
              "note-off only releases its own MIDI channel/layer");
        api->on_midi(channel_inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
        api->on_midi(channel_inst, (const uint8_t[]){ 0xB1, 120, 0 }, 3, 0);
        int ch0_gate = 0, ch1_gate = 0;
        for (int i = 0; i < AMINO_MAX_VOICES; i++) {
            if (!ci->v[i].active || !ci->v[i].gate) continue;
            if (ci->v[i].midi_ch == 0) ch0_gate = 1;
            if (ci->v[i].midi_ch == 1) ch1_gate = 1;
        }
        CHECK(ch0_gate && !ch1_gate,
              "CC120 immediately silences only its MIDI channel");
        api->on_midi(channel_inst, (const uint8_t[]){ 0xB0, 1, 127 }, 3, 0);
        api->on_midi(channel_inst, (const uint8_t[]){ 0xB0, 74, 127 }, 3, 0);
        api->on_midi(channel_inst, (const uint8_t[]){ 0xB0, 121, 0 }, 3, 0);
        CHECK(ci->modwheel == 0.0f && ci->timbre == 0.0f,
              "CC121 resets supported continuous controllers");
        api->destroy_instance(channel_inst);
    }

    /* Regular sound pages use bare aliases, resolved through edit_layer just
     * like Main; concrete layer keys remain available to other clients. */
    void *focused_inst = api->create_instance("", 0);
    CHECK(focused_inst != NULL, "focused-layer alias regression instance");
    if (focused_inst) {
        api->set_param(focused_inst, "layer_count", "2");
        api->set_param(focused_inst, "edit_layer", "2");
        api->set_param(focused_inst, "cutoff", "60");
        api->get_param(focused_inst, "l2_cutoff", buf, sizeof buf);
        CHECK(near_str(buf, 60.0), "bare sound-page key targets edit layer");
        api->get_param(focused_inst, "l1_cutoff", buf, sizeof buf);
        CHECK(near_str(buf, 127.0), "bare sound-page key leaves other layer alone");
        api->set_param(focused_inst, "l2_cutoff", "80");
        api->get_param(focused_inst, "cutoff", buf, sizeof buf);
        CHECK(near_str(buf, 80.0), "bare focused read follows explicit layer write");
        api->destroy_instance(focused_inst);
    }

    void *mono_channel_inst = api->create_instance("", 0);
    CHECK(mono_channel_inst != NULL, "Mono channel stack regression instance");
    if (mono_channel_inst) {
        AminoInstance *mci = (AminoInstance *)mono_channel_inst;
        api->set_param(mono_channel_inst, "l1_poly", "Mono");
        api->on_midi(mono_channel_inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
        api->on_midi(mono_channel_inst, (const uint8_t[]){ 0x91, 64, 100 }, 3, 0);
        api->on_midi(mono_channel_inst, (const uint8_t[]){ 0x80, 60, 0 }, 3, 0);
        CHECK(mci->v[0].gate && mci->v[0].root_note == 64 &&
              mci->v[0].midi_ch == 1,
              "Mono note stack restores the held note from its channel");
        api->on_midi(mono_channel_inst, (const uint8_t[]){ 0x81, 64, 0 }, 3, 0);
        CHECK(!mci->v[0].gate, "Mono note-off releases matching channel key");
        api->destroy_instance(mono_channel_inst);
    }

    /* CC123 cancels a stolen-but-not-yet-started note instead of resurrecting
     * it after the old voice's anti-click fade. */
    void *all_off_inst = api->create_instance("", 0);
    CHECK(all_off_inst != NULL, "all-notes-off regression instance");
    if (all_off_inst) {
        AminoInstance *ai = (AminoInstance *)all_off_inst;
        api->set_param(all_off_inst, "voices", "1");
        api->set_param(all_off_inst, "l1_e3_r", "1");
        api->on_midi(all_off_inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
        render_rms(api, all_off_inst, 1, 0, 0);
        api->on_midi(all_off_inst, (const uint8_t[]){ 0x90, 62, 100 }, 3, 0);
        CHECK(ai->v[0].pending_layer == 0, "voice steal queues retrigger");
        api->on_midi(all_off_inst, (const uint8_t[]){ 0xB0, 123, 0 }, 3, 0);
        render_rms(api, all_off_inst, 3, 0, 0);
        CHECK(ai->v[0].pending_layer < 0 && !ai->v[0].gate,
              "CC123 clears pending starts and releases the channel");
        api->destroy_instance(all_off_inst);
    }

    /* MPE channel pressure and poly pressure are resolved per MIDI channel. */
    void *mpe_inst = api->create_instance("", 0);
    CHECK(mpe_inst != NULL, "MPE pressure regression instance");
    if (mpe_inst) {
        AminoInstance *mi = (AminoInstance *)mpe_inst;
        api->set_param(mpe_inst, "mpe", "On");
        api->on_midi(mpe_inst, (const uint8_t[]){ 0x92, 60, 100 }, 3, 0);
        api->on_midi(mpe_inst, (const uint8_t[]){ 0x92, 64, 100 }, 3, 0);
        api->on_midi(mpe_inst, (const uint8_t[]){ 0x93, 60, 100 }, 3, 0);
        api->on_midi(mpe_inst, (const uint8_t[]){ 0xA2, 60, 127 }, 3, 0);
        int16_t mpe_out[256];
        api->render_block(mpe_inst, mpe_out, 128);
        float p2 = -1.0f, p2_other = -1.0f, p3 = -1.0f;
        for (int i = 0; i < AMINO_MAX_VOICES; i++) {
            AminoVoice *v = &mi->v[i];
            if (!v->active) continue;
            if (v->root_note == 60 && v->midi_ch == 2) p2 = v->pressure;
            if (v->root_note == 64 && v->midi_ch == 2) p2_other = v->pressure;
            if (v->root_note == 60 && v->midi_ch == 3) p3 = v->pressure;
        }
        CHECK(p2 > 0.99f && p2_other < 0.01f && p3 < 0.01f,
              "poly aftertouch affects only the matching note and channel");
        api->on_midi(mpe_inst, (const uint8_t[]){ 0xD3, 127 }, 2, 0);
        api->render_block(mpe_inst, mpe_out, 128);
        p3 = -1.0f;
        for (int i = 0; i < AMINO_MAX_VOICES; i++)
            if (mi->v[i].active && mi->v[i].root_note == 60 &&
                mi->v[i].midi_ch == 3) p3 = mi->v[i].pressure;
        CHECK(p3 > 0.99f, "MPE channel pressure persists through rendering");
        api->destroy_instance(mpe_inst);
    }

    /* Every accepted pitch/modulation combination must keep table phases in
     * range, even when the resulting frequency is far above the sample rate. */
    void *pitch_edge_inst = api->create_instance("", 0);
    CHECK(pitch_edge_inst != NULL, "pitch bounds regression instance");
    if (pitch_edge_inst) {
        AminoInstance *pi = (AminoInstance *)pitch_edge_inst;
        api->set_param(pitch_edge_inst, "l1_osc_mode", "Clean");
        api->set_param(pitch_edge_inst, "l1_octave", "3");
        api->set_param(pitch_edge_inst, "l1_osc1_semi", "48");
        api->set_param(pitch_edge_inst, "l1_m1_src", "Constant");
        api->set_param(pitch_edge_inst, "l1_m1_dst", "Osc1 Pitch");
        api->set_param(pitch_edge_inst, "l1_m1_amt", "1");
        api->on_midi(pitch_edge_inst, (const uint8_t[]){ 0x90, 127, 127 }, 3, 0);
        int16_t edge_out[256];
        api->render_block(pitch_edge_inst, edge_out, 128);
        CHECK(pi->v[0].p1 >= 0.0f && pi->v[0].p1 < 128.0f &&
              pi->v[0].p2 >= 0.0f && pi->v[0].p2 < 128.0f,
              "extreme pitch keeps oscillator phases inside wavetable");
        api->destroy_instance(pitch_edge_inst);
    }

    /* Scale=Set follows Move's root/scale instead of always assuming C minor.
     * In an A-minor set, A3 stays A3; with the documented fallback C-minor
     * set, the same MIDI note is quantized down to Ab3. */
    void *scale_set_inst = api->create_instance("", 0);
    CHECK(scale_set_inst != NULL, "Move scale-set regression instance");
    if (scale_set_inst) {
        AminoInstance *ssi = (AminoInstance *)scale_set_inst;
        api->set_param(scale_set_inst, "l1_scale", "Set");
        api->on_midi(scale_set_inst, (const uint8_t[]){ 0x90, 57, 100 }, 3, 0);
        CHECK(ssi->v[0].note == 56, "Scale=Set fallback remains C minor without Move info");
        amino_move_scale_update(ssi, 9, "Minor");
        api->on_midi(scale_set_inst, (const uint8_t[]){ 0x90, 57, 100 }, 3, 0);
        CHECK(ssi->v[1].note == 57, "Scale=Set honors Move root and scale");
        api->on_midi(scale_set_inst, (const uint8_t[]){ 0x90, 58, 100 }, 3, 0);
        CHECK(ssi->v[2].note == 57, "Move A-minor quantization uses the selected root");
        api->destroy_instance(scale_set_inst);
    }

    /* Loading a preset resets allocation history so voice-specific random
     * streams are repeatable after prior playback on the same instance. */
    void *alloc_inst = api->create_instance("", 0);
    CHECK(alloc_inst != NULL, "preset allocator regression instance");
    if (alloc_inst) {
        AminoInstance *ai = (AminoInstance *)alloc_inst;
        api->on_midi(alloc_inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
        CHECK(ai->alloc_cursor == 1, "note allocation advances voice cursor");
        api->set_param(alloc_inst, "preset", "79");
        CHECK(ai->alloc_cursor == 0, "preset load resets voice allocation cursor");
        api->destroy_instance(alloc_inst);
    }

    /* Randomise / init triggers. */
    api->set_param(inst, "randomize", "1");
    n = api->get_param(inst, "l1_cutoff", buf, sizeof buf);
    printf("randomised cutoff: %s\n", buf);
    api->set_param(inst, "init_layer", "1");
    n = api->get_param(inst, "l1_cutoff", buf, sizeof buf);
    CHECK(near_str(buf, 127.0), "init layer cutoff");

    /* Load Layer. */
    api->set_param(inst, "layer_load_src", "1");
    n = api->get_param(inst, "l1_osc1_table", buf, sizeof buf);
    printf("loaded layer table: %s\n", buf);

    /* Garbage state must not crash. */
    api->set_param(inst, "state", "{\"master_vol\":\"0.5\",\"bogus\":\"1\",\"l2_cutoff\":\"80\"}");
    n = api->get_param(inst, "master_vol", buf, sizeof buf);
    CHECK(strcmp(buf, "0.500") == 0, "partial state applies");
    n = api->get_param(inst, "l2_cutoff", buf, sizeof buf);
    CHECK(near_str(buf, 80.0), "partial layer state applies");
    api->set_param(inst, "state", "not json at all {{{ ,,,");
    api->set_param(inst, "state", "");

    /* Realtime-ish stress: random CCs and notes for 200 blocks. */
    for (int b = 0; b < 200; b++) {
        int note = 36 + (b * 7) % 48;
        api->on_midi(inst, (const uint8_t[]){ 0x90, (uint8_t)note, 100 }, 3, 0);
        api->on_midi(inst, (const uint8_t[]){ 0xE0, (uint8_t)(b & 0x7F), 64 }, 3, 0);
        api->on_midi(inst, (const uint8_t[]){ 0xB0, 1, (uint8_t)(b & 0x7F) }, 3, 0);
        render_rms(api, inst, 1, 0, &peak);
    }
    CHECK(peak <= 32767, "stress peak");

    /* Silence: no output one second after all notes off (no FX tails). */
    api->set_param(inst, "preset", "0");
    api->set_param(inst, "fx1_type", "Off");
    api->set_param(inst, "fx2_type", "Off");
    api->set_param(inst, "l1_e3_r", "0.05");
    api->on_midi(inst, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
    render_rms(api, inst, 10, 0, 0);
    api->on_midi(inst, (const uint8_t[]){ 0xB0, 123, 0 }, 3, 0);
    render_rms(api, inst, 700, 0, 0);
    peak = 0;
    rms = render_rms(api, inst, 100, 0, &peak);
    printf("silence rms %.3f peak %d\n", rms, peak);
    CHECK(peak <= 1 && rms < 1.0, "silence after note off");

    api->destroy_instance(inst);
    inst = 0;

    /* Review regressions (2026-10-23). Each uses a fresh Init instance. */
    {
        /* Voice steal: after the 2 ms steal fade the old note stays silent
         * for the rest of the block instead of returning at full level. */
        void *ri = api->create_instance("", 0);
        api->set_param(ri, "fx1_type", "Off");
        api->set_param(ri, "fx2_type", "Off");
        api->set_param(ri, "voices", "1");
        api->set_param(ri, "l1_e3_a", "0");
        api->set_param(ri, "l1_e3_s", "1");
        api->on_midi(ri, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
        render_rms(api, ri, 40, 0, 0);
        api->on_midi(ri, (const uint8_t[]){ 0x90, 60, 100 }, 3, 0);
        int16_t out[256];
        api->render_block(ri, out, 128);
        int fade = (int)(0.002f * 44100.0f);
        int after = 0;
        for (int i = fade; i < 128; i++) {
            int a = abs(out[i * 2]);
            if (a > after) after = a;
        }
        CHECK(after <= 1, "stolen voice stays silent after its fade");
        double next = render_rms(api, ri, 4, 0, 0);
        CHECK(next > 500.0, "queued note starts after a steal");
        api->destroy_instance(ri);
    }
    {
        /* A direct FX type change loads the new type's defaults; a state
         * load keeps its stored p-values. */
        void *ri = api->create_instance("", 0);
        api->set_param(ri, "fx1_type", "Reverb");
        render_rms(api, ri, 30, 0, 0);
        api->set_param(ri, "fx1_type", "Delay");
        render_rms(api, ri, 30, 0, 0);
        const float *def = amino_fx_defaults(8);
        int ok = 1;
        for (int i = 0; i < 6; i++) {
            char key[16];
            snprintf(key, sizeof key, "fx1_p%d", i + 1);
            api->get_param(ri, key, buf, sizeof buf);
            if (!near_str(buf, def[i])) ok = 0;
        }
        CHECK(ok, "FX type knob change loads that type's defaults");
        CHECK(((AminoInstance *)ri)->fx[0].type == 8, "FX type change completes");
        api->set_param(ri, "state", "{\"fx1_type\":\"9\",\"fx1_p1\":\"0.200\"}");
        render_rms(api, ri, 30, 0, 0);
        api->get_param(ri, "fx1_p1", buf, sizeof buf);
        CHECK(near_str(buf, 0.2), "state load keeps its FX p-values");
        api->destroy_instance(ri);
    }
    {
        /* Via = LFO1 on a Constant route follows the LFO exactly like a
         * direct LFO1 route at the same amount. */
        double direct[10], via[10];
        for (int pass = 0; pass < 2; pass++) {
            void *ri = api->create_instance("", 0);
            api->set_param(ri, "fx1_type", "Off");
            api->set_param(ri, "fx2_type", "Off");
            api->set_param(ri, "l1_e3_a", "0");
            api->set_param(ri, "l1_e3_s", "1");
            api->set_param(ri, "l1_lfo1_shape", "Pulse");
            api->set_param(ri, "l1_lfo1_rate", "0.35");
            api->set_param(ri, "l1_m1_src", pass ? "Constant" : "LFO1");
            api->set_param(ri, "l1_m1_dst", "Amp Level");
            api->set_param(ri, "l1_m1_amt", "-1");
            if (pass) {
                api->set_param(ri, "l1_m1_via", "LFO1");
                api->set_param(ri, "l1_m1_via_depth", "1");
            }
            api->on_midi(ri, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
            for (int w = 0; w < 10; w++)
                (pass ? via : direct)[w] = render_rms(api, ri, 172, 0, 0);
            api->destroy_instance(ri);
        }
        double lo = 1e9, hi = 0.0, diff = 0.0;
        for (int w = 0; w < 10; w++) {
            if (via[w] < lo) lo = via[w];
            if (via[w] > hi) hi = via[w];
            diff = fmax(diff, fabs(via[w] - direct[w]));
        }
        CHECK(lo < 50.0 && hi > 3000.0, "Via = LFO1 modulates a non-LFO route");
        CHECK(diff < 1.0, "Via = LFO1 matches the equivalent direct LFO route");
    }
    {
        /* EQ: a band crossing 0 dB must not hand its filter state to the
         * next band (that produced a large click). */
        void *ri = api->create_instance("", 0);
        AminoInstance *ai = (AminoInstance *)ri;
        api->set_param(ri, "fx1_type", "EQ");
        api->set_param(ri, "fx1_mix", "1");
        api->set_param(ri, "fx1_p1", "0.9");
        api->set_param(ri, "fx1_p2", "0.9");
        api->set_param(ri, "fx1_p3", "0.2");
        api->set_param(ri, "fx1_p4", "0.5");
        api->set_param(ri, "fx1_p6", "0");
        float l[128], r[128], prev = 0.0f, prev2 = 0.0f, worst = 0.0f;
        double ph = 0.0;
        for (int b = 0; b < 400; b++) {
            if (b == 200) api->set_param(ri, "fx1_p1", "0.5");
            for (int i = 0; i < 128; i++) {
                l[i] = r[i] = 0.3f * (float)sin(ph);
                ph += 2.0 * M_PI * 300.0 / 44100.0;
            }
            amino_fx_process(ai, 0, l, r, 128);
            for (int i = 0; i < 128; i++) {
                float d2 = fabsf(l[i] - 2.0f * prev + prev2);
                if (b > 150 && d2 > worst) worst = d2;
                prev2 = prev;
                prev = l[i];
            }
        }
        CHECK(worst < 0.01f, "EQ band crossing 0 dB is click-free");
        api->destroy_instance(ri);
    }
    {
        /* Lowering Voices releases notes above the limit instead of cutting
         * them; they finish their release and free the voice. */
        void *ri = api->create_instance("", 0);
        AminoInstance *ai = (AminoInstance *)ri;
        api->set_param(ri, "fx1_type", "Off");
        api->set_param(ri, "fx2_type", "Off");
        api->set_param(ri, "l1_e3_r", "0.3");
        for (int k = 0; k < 4; k++)
            api->on_midi(ri, (const uint8_t[]){ 0x90, (uint8_t)(48 + k * 4), 100 }, 3, 0);
        double before = render_rms(api, ri, 20, 0, 0);
        api->set_param(ri, "voices", "1");
        double just_after = render_rms(api, ri, 1, 0, 0);
        CHECK(just_after > before * 0.5, "lowering Voices does not cut notes");
        int gated = 0;
        for (int i = 1; i < AMINO_MAX_VOICES; i++) gated += ai->v[i].gate;
        CHECK(gated == 0, "voices above the limit are released");
        render_rms(api, ri, 400, 0, 0);
        int active = 0;
        for (int i = 1; i < AMINO_MAX_VOICES; i++) active += ai->v[i].active;
        CHECK(active == 0, "released voices above the limit finish");
        api->destroy_instance(ri);
    }
    {
        /* LFO fade-in restarts with the envelopes: on Mono retriggers, but
         * not on Legato notes. */
        for (int legato = 0; legato < 2; legato++) {
            void *ri = api->create_instance("", 0);
            AminoInstance *ai = (AminoInstance *)ri;
            api->set_param(ri, "l1_poly", legato ? "Legato" : "Mono");
            api->set_param(ri, "l1_lfo1_fade", "0.5");
            api->on_midi(ri, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
            render_rms(api, ri, 20, 0, 0);
            AminoVoice *v = 0;
            for (int i = 0; i < AMINO_MAX_VOICES; i++)
                if (ai->v[i].active && ai->v[i].gate) v = &ai->v[i];
            uint64_t t0 = v ? v->lfo_t0 : 0;
            api->on_midi(ri, (const uint8_t[]){ 0x90, 55, 100 }, 3, 0);
            render_rms(api, ri, 1, 0, 0);
            CHECK(v && v->active && v->gate, "mono/legato voice keeps playing");
            if (legato) CHECK(v && v->lfo_t0 == t0, "Legato note keeps the LFO fade running");
            else CHECK(v && v->lfo_t0 > t0, "Mono retrigger restarts the LFO fade");
            api->destroy_instance(ri);
        }
    }
    {
        /* FX Send modulation only scales FX sends; a dry layer is unaffected. */
        double rms_mod[2];
        for (int pass = 0; pass < 2; pass++) {
            void *ri = api->create_instance("", 0);
            api->set_param(ri, "fx1_type", "Off");
            api->set_param(ri, "fx2_type", "Off");
            api->set_param(ri, "l1_route", "Dry");
            if (pass) {
                api->set_param(ri, "l1_m1_src", "Constant");
                api->set_param(ri, "l1_m1_dst", "FX Send");
                api->set_param(ri, "l1_m1_amt", "-1");
            }
            api->on_midi(ri, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
            rms_mod[pass] = render_rms(api, ri, 40, 0, 0);
            api->destroy_instance(ri);
        }
        CHECK(fabs(rms_mod[1] - rms_mod[0]) < 1.0, "FX Send modulation leaves dry layers alone");
    }
    {
        /* am_tanh is bounded and continuous at its clamp. */
        float peak = 0.0f;
        for (int i = 0; i <= 400; i++) {
            float y = am_tanh((float)i * 0.01f);
            if (y > peak) peak = y;
        }
        CHECK(peak <= 1.0f + 1e-6f, "am_tanh never exceeds 1 (to float rounding)");
        CHECK(fabsf(am_tanh(2.9999f) - am_tanh(3.0001f)) < 1e-4f, "am_tanh is continuous at its clamp");
    }

    /* Remote UI live telemetry and audition (Phase 5). */
    {
        void *li = api->create_instance("", 0);
        char lb[256];
        api->set_param(li, "fx1_type", "Off");
        api->set_param(li, "fx2_type", "Off");
        int ln = api->get_param(li, "rui_live", lb, sizeof lb);
        CHECK(ln > 0 && strncmp(lb, "v=0,0,0,0,0;l=-;e=-;p=0.000;", 28) == 0, "rui_live idle");
        char before_state[16];
        api->get_param(li, "rui_poll", before_state, sizeof before_state);

        api->set_param(li, "audition", "60:100");
        render_rms(api, li, 20, 0, 0);
        api->get_param(li, "rui_live", lb, sizeof lb);
        int tot = -1, c1 = -1, st0 = -1, st2 = -1;
        float peak = 0.0f, lv2 = 0.0f;
        CHECK(sscanf(lb, "v=%d,%d,", &tot, &c1) == 2 && tot == 1 && c1 == 1, "audition starts one voice on L1");
        const char *e = strstr(lb, ";e=");
        CHECK(e && sscanf(e, ";e=%d,%*f,%*d,%*f,%d,%f", &st0, &st2, &lv2) == 3 && st2 >= 1 && st2 <= 2 && lv2 > 0.1f,
              "rui_live reports the amp envelope of the newest voice");
        const char *pk = strstr(lb, ";p=");
        CHECK(pk && sscanf(pk, ";p=%f", &peak) == 1 && peak > 0.01f && peak <= 1.0f, "rui_live output peak");
        CHECK(strstr(lb, ";l=0.") != NULL || strstr(lb, ";l=1.") != NULL, "rui_live LFO phases");
        char after_state[16];
        api->get_param(li, "rui_poll", after_state, sizeof after_state);
        CHECK(strncmp(before_state, after_state, 2) == 0, "audition is not a sound edit (rev unchanged)");

        api->set_param(li, "audition", "60:0");
        api->get_param(li, "rui_live", lb, sizeof lb);
        e = strstr(lb, ";e=");
        CHECK(e && sscanf(e, ";e=%*d,%*f,%*d,%*f,%d", &st2) == 1 && st2 == 3, "audition release enters the release stage");

        /* "off" releases everything; the timeout releases forgotten notes. */
        api->set_param(li, "l1_e3_s", "1");
        api->set_param(li, "l1_e3_r", "0.1");      /* ~20 ms release */
        render_rms(api, li, 200, 0, 0);            /* let note 60's tail end */
        api->set_param(li, "audition", "64:90");
        api->set_param(li, "audition", "67:90");
        render_rms(api, li, 20, 0, 0);
        api->get_param(li, "rui_live", lb, sizeof lb);
        CHECK(strncmp(lb, "v=2,2,", 6) == 0, "two audition notes sound");
        api->set_param(li, "audition", "off");
        render_rms(api, li, 40, 0, 0);
        api->get_param(li, "rui_live", lb, sizeof lb);
        CHECK(strncmp(lb, "v=0,", 4) == 0, "audition off releases held notes");
        api->set_param(li, "audition", "72:100");
        render_rms(api, li, (AMINO_AUDITION_TIMEOUT_S - 1) * 44100 / 128, 0, 0);
        api->get_param(li, "rui_live", lb, sizeof lb);
        CHECK(strncmp(lb, "v=1,", 4) == 0, "an audition note holds until the timeout");
        render_rms(api, li, 2 * 44100 / 128, 0, 0);
        api->get_param(li, "rui_live", lb, sizeof lb);
        CHECK(strncmp(lb, "v=0,", 4) == 0, "a forgotten audition note is released after the timeout");

        /* Malformed values are ignored. */
        api->set_param(li, "audition", "200:100");
        api->set_param(li, "audition", "abc");
        api->set_param(li, "audition", "60");
        CHECK(api->get_param(li, "rui_live", lb, sizeof lb) > 0, "rui_live after malformed audition");
        CHECK(api->get_param(li, "rui_live", lb, 8) < 0, "rui_live refuses a short buffer");
        api->destroy_instance(li);
    }

    /* Aux envelope (4th) and the Moog ladder filter. */
    {
        char lb[256];
        void *ai = api->create_instance("", 0);
        api->set_param(ai, "fx1_type", "Off");
        api->set_param(ai, "fx2_type", "Off");
        api->set_param(ai, "l1_e4_a", "0");
        api->set_param(ai, "l1_e4_d", "0.5");
        api->set_param(ai, "l1_e4_s", "0.7");
        api->set_param(ai, "l1_e4_r", "0.3");
        api->on_midi(ai, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
        render_rms(api, ai, 20, 0, 0);
        api->get_param(ai, "rui_live", lb, sizeof lb);
        const char *e = strstr(lb, ";e=");
        int st4 = -1;
        float lv4 = -1.0f;
        CHECK(e && sscanf(e, ";e=%*d,%*f,%*d,%*f,%*d,%*f,%d,%f", &st4, &lv4) == 2 &&
              st4 >= 1 && lv4 > 0.5f, "aux envelope reports stage and level");
        api->on_midi(ai, (const uint8_t[]){ 0x80, 48, 0 }, 3, 0);
        api->destroy_instance(ai);

        /* Aux Env as a mod source: routing it to Cutoff changes the sound. */
        double r_off, r_mod;
        for (int pass = 0; pass < 2; pass++) {
            void *mi = api->create_instance("", 0);
            api->set_param(mi, "fx1_type", "Off");
            api->set_param(mi, "fx2_type", "Off");
            api->set_param(mi, "l1_cutoff", "50");
            api->set_param(mi, "l1_e4_a", "0");
            api->set_param(mi, "l1_e4_d", "1");
            api->set_param(mi, "l1_e4_s", "1");
            if (pass) {
                api->set_param(mi, "l1_m1_src", "Aux Env");
                api->set_param(mi, "l1_m1_dst", "Cutoff");
                api->set_param(mi, "l1_m1_amt", "0.8");
            }
            api->on_midi(mi, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
            render_rms(api, mi, 10, 0, 0);
            double r = render_rms(api, mi, 40, 0, 0);
            if (pass) r_mod = r; else r_off = r;
            api->destroy_instance(mi);
        }
        printf("aux env cutoff mod rms %.1f -> %.1f\n", r_off, r_mod);
        CHECK(r_mod > r_off * 1.2, "Aux Env routes through the matrix");

        /* Dedicated Aux Env routes: either slot reaches Cutoff without the
         * matrix, and a route with no target does nothing. */
        double r_route[3];
        for (int pass = 0; pass < 3; pass++) {
            void *mi = api->create_instance("", 0);
            api->set_param(mi, "fx1_type", "Off");
            api->set_param(mi, "fx2_type", "Off");
            api->set_param(mi, "l1_cutoff", "50");
            api->set_param(mi, "l1_e4_a", "0");
            api->set_param(mi, "l1_e4_d", "1");
            api->set_param(mi, "l1_e4_s", "1");
            api->set_param(mi, pass == 2 ? "l1_e4_amt2" : "l1_e4_amt1", "0.8");
            if (pass == 1) api->set_param(mi, "l1_e4_dst1", "Cutoff");
            if (pass == 2) api->set_param(mi, "l1_e4_dst2", "Cutoff");
            api->on_midi(mi, (const uint8_t[]){ 0x90, 48, 100 }, 3, 0);
            render_rms(api, mi, 10, 0, 0);
            r_route[pass] = render_rms(api, mi, 40, 0, 0);
            api->destroy_instance(mi);
        }
        printf("aux routes rms %.1f -> %.1f / %.1f\n", r_route[0], r_route[1], r_route[2]);
        CHECK(fabs(r_route[0] - r_off) < 1.0, "Aux route without a target is silent");
        CHECK(r_route[1] > r_off * 1.2, "Aux Env route 1 reaches its target");
        CHECK(r_route[2] > r_off * 1.2, "Aux Env route 2 reaches its target");

        /* LFO to Pitch: a square LFO at full depth swings +/-12 semitones,
         * which moves the zero-crossing rate; at 0 it leaves pitch alone. */
        long zc[2];
        for (int pass = 0; pass < 2; pass++) {
            void *vi = api->create_instance("", 0);
            api->set_param(vi, "fx1_type", "Off");
            api->set_param(vi, "fx2_type", "Off");
            api->set_param(vi, "l1_osc1_wave", "61");
            api->set_param(vi, "l1_lfo1_shape", "Pulse");
            api->set_param(vi, "l1_lfo1_rate", "0.3");
            api->set_param(vi, "l1_lfo1_pitch", pass ? "1" : "0");
            api->on_midi(vi, (const uint8_t[]){ 0x90, 57, 100 }, 3, 0);
            int16_t buf[256];
            int prev = 0;
            zc[pass] = 0;
            for (int b = 0; b < 600; b++) {
                api->render_block(vi, buf, 128);
                for (int i = 0; i < 256; i += 2) {
                    int s = buf[i] >= 0;
                    if (b && s != prev) zc[pass]++;
                    prev = s;
                }
            }
            api->destroy_instance(vi);
        }
        printf("lfo pitch zero crossings %ld -> %ld\n", zc[0], zc[1]);
        CHECK(zc[1] > zc[0] * 1.1, "LFO to Pitch modulates oscillator pitch");

        /* Moog ladder: bright source, low cutoff.  Removes highs like LP24,
         * stays finite and bounded at full resonance, rings on its own. */
        double r_lp24 = 0.0, r_moog = 0.0, r_self = 0.0;
        int pk_self = 0;
        for (int pass = 0; pass < 4; pass++) {
            void *fi = api->create_instance("", 0);
            api->set_param(fi, "fx1_type", "Off");
            api->set_param(fi, "fx2_type", "Off");
            api->set_param(fi, "l1_cutoff", pass == 3 ? "60" : "50");
            api->set_param(fi, "l1_ftype", pass == 0 ? "LP24" : "Ladder 24dB");
            api->set_param(fi, "l1_res", pass >= 2 ? "1" : "0");
            if (pass == 3) api->set_param(fi, "l1_osc1_level", "0");
            api->on_midi(fi, (const uint8_t[]){ 0x90, 36, 110 }, 3, 0);
            render_rms(api, fi, 10, 0, 0);
            int pk = 0;
            double r = render_rms(api, fi, 100, 0, &pk);
            if (pass == 0) r_lp24 = r;
            else if (pass == 1) r_moog = r;
            else if (pass == 3) { r_self = r; pk_self = pk; }
            CHECK(pk <= 32767 && r == r, "Moog output is finite");
            api->destroy_instance(fi);
        }
        printf("filter rms lp24 %.1f moog %.1f self-osc %.1f peak %d\n",
               r_lp24, r_moog, r_self, pk_self);
        CHECK(r_moog > 1.0 && r_moog < 12000.0, "Moog passes signal");
        CHECK(r_self > 10.0, "Moog self-oscillates with no input at res 1");

        /* State v1 stored Filter Type in the old order (Moog last); v2 moved
         * Ladder 24dB after LP12.  Old blobs remap, current ones round-trip. */
        void *mi = api->create_instance("", 0);
        static const char *const v1_vals[7] = { "0", "1", "2", "3", "4", "5", "6" };
        static const char *const v2_want[7] = { "0", "1", "3", "4", "5", "6", "2" };
        for (int t = 0; t < 7; t++) {
            char blob[96], got[16];
            snprintf(blob, sizeof blob, "{\"fmt\":\"amino\",\"v\":\"1\",\"l1_ftype\":\"%s\"}",
                     v1_vals[t]);
            api->set_param(mi, "state", blob);
            api->get_param(mi, "l1_ftype", got, sizeof got);
            CHECK(strcmp(got, v2_want[t]) == 0, "v1 Filter Type migrates to v2 order");
        }
        api->set_param(mi, "l1_ftype", "Ladder 24dB");
        static char st[65536];
        api->get_param(mi, "state", st, sizeof st);
        CHECK(strstr(st, "\"v\":\"2\"") != NULL, "state blob is v2");
        api->set_param(mi, "l1_ftype", "LP24");
        api->set_param(mi, "state", st);
        char ft[16];
        api->get_param(mi, "l1_ftype", ft, sizeof ft);
        CHECK(strcmp(ft, "2") == 0, "v2 Filter Type round-trips unchanged");
        api->destroy_instance(mi);
    }

    regression_tests(api);
    bank_polish_tests(api);

    inst = api->create_instance("", 0);
    CHECK(inst != NULL, "recreate");
    api->destroy_instance(inst);

    printf(g_fail ? "FAILED (%d)\n" : "OK\n", g_fail);
    return g_fail ? 1 : 0;
}

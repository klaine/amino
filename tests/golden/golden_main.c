/* Golden-audio reference renderer (docs/08 3.2).  Not shipped.
 *
 * Renders one fixed MIDI phrase through every factory preset and prints one
 * JSON object per line:
 *
 *   {"p":0,"rms":123.4,"peak":4567,"cent":789.0,"zcr":0.0123}
 *
 * `tests/golden/check_golden.py` regenerates these metrics and compares them
 * with the committed golden file using RMS/spectral tolerances.
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "factory_presets.h"
#include "plugin_api.h"

extern plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

#define N 2048
#define BINS 24

static float g_bpm = 120.0f;
static float host_bpm(void) { return g_bpm; }
static int host_clock(void) { return MOVE_CLOCK_STATUS_RUNNING; }
static double g_beats;
static double host_beats(void) { return g_beats; }

static float g_l[N];
static int g_n;

static void collect(plugin_api_v2_t *api, void *inst, int blocks) {
    int16_t out[128 * 2];
    for (int b = 0; b < blocks; b++) {
        api->render_block(inst, out, 128);
        g_beats += 128.0 * (double)g_bpm / (60.0 * 44100.0);
        for (int i = 0; i < 128 && g_n < N; i++) g_l[g_n++] = (float)out[i * 2];
    }
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
    if (!api) return 1;

    for (int p = 0; p < amino_factory_preset_count; p++) {
        g_bpm = amino_factory_presets[p].bpm;
        g_beats = 0.0;
        void *inst = api->create_instance("modules/sound_generators/amino", 0);
        if (!inst) return 1;

        char ps[16];
        snprintf(ps, sizeof ps, "%d", p);
        api->set_param(inst, "preset", ps);

        /* Let incremental FX arena clearing and the type fade-in finish before
         * the measured phrase, matching a fresh Python FX instance. */
        collect(api, inst, 32);
        api->on_midi(inst, (const uint8_t[]){ 0x90, 48, 110 }, 3, 0);
        api->on_midi(inst, (const uint8_t[]){ 0x90, 55, 110 }, 3, 0);
        api->on_midi(inst, (const uint8_t[]){ 0x90, 60, 110 }, 3, 0);
        collect(api, inst, 24);          /* attack / transient */
        g_n = 0;
        collect(api, inst, 16);          /* 2048 samples */

        double sum = 0.0, zc = 0.0;
        int peak = 0, last = 0;
        for (int i = 0; i < g_n; i++) {
            float v = g_l[i];
            int iv = (int)v;
            if (iv < 0) iv = -iv;
            if (iv > peak) peak = iv;
            sum += (double)v * v;
            if (i && ((v >= 0.0f) != (last >= 0))) zc += 1.0;
            last = (int)v;
        }
        double rms = sqrt(sum / (double)g_n);
        double zcr = zc / (double)g_n;

        /* Log-spaced magnitude bins 40 Hz .. 16 kHz, Hann-windowed. */
        double wsum = 0.0, cent = 0.0;
        for (int k = 0; k < BINS; k++) {
            double freq = 40.0 * pow(400.0, (double)k / (double)(BINS - 1));
            double wr = 0.0, wi = 0.0, ws = 0.0;
            double w = 2.0 * 3.14159265358979323846 * freq / 44100.0;
            for (int i = 0; i < g_n; i++) {
                double hann = 0.5 - 0.5 * cos(2.0 * 3.14159265358979323846 *
                                              (double)i / (double)(g_n - 1));
                wr += (double)g_l[i] * hann * cos(w * (double)i);
                wi += (double)g_l[i] * hann * sin(w * (double)i);
                ws += hann;
            }
            double mag = sqrt(wr * wr + wi * wi) / (ws > 0.0 ? ws : 1.0);
            wsum += mag;
            cent += freq * mag;
        }
        if (wsum > 1e-9) cent /= wsum;

        printf("{\"p\":%d,\"rms\":%.3f,\"peak\":%d,\"cent\":%.2f,\"zcr\":%.5f}\n",
               p, rms, peak, cent, zcr);

        api->on_midi(inst, (const uint8_t[]){ 0xB0, 123, 0 }, 3, 0);
        collect(api, inst, 4);
        api->destroy_instance(inst);
    }

    return 0;
}

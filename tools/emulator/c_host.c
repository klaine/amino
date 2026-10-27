/* Minimal Schwung-like host around the compiled Amino engine, for offline
 * rendering from Python (tools/emulator/c_engine.py loads it with ctypes).
 * Not shipped.
 *
 * Built by scripts/build.sh into build/libamino_host.{dylib,so} together with
 * every src/dsp source, so previews come from exactly the code that runs on Move.  The
 * host has one transport (tempo, running flag, beat position), which is fine
 * for the renderer: each worker process renders one preset at a time.
 */
#include <stdint.h>
#include <string.h>

#include "plugin_api.h"

extern plugin_api_v2_t *move_plugin_init_v2(const host_api_v1_t *host);

#define SR 44100

static host_api_v1_t g_host;
static plugin_api_v2_t *g_api;
static float g_bpm = 120.0f;
static int g_running;
static double g_beats;

static float host_bpm(void) { return g_bpm; }
static int host_clock(void) {
    return g_running ? MOVE_CLOCK_STATUS_RUNNING : MOVE_CLOCK_STATUS_STOPPED;
}
static double host_beats(void) { return g_running ? g_beats : -1.0; }

static plugin_api_v2_t *api(void) {
    if (!g_api) {
        memset(&g_host, 0, sizeof g_host);
        g_host.api_version = MOVE_PLUGIN_API_VERSION;
        g_host.sample_rate = SR;
        g_host.frames_per_block = 128;
        g_host.get_bpm = host_bpm;
        g_host.get_clock_status = host_clock;
        g_host.get_beat_position = host_beats;
        g_api = move_plugin_init_v2(&g_host);
    }
    return g_api;
}

int ar_sample_rate(void) { return SR; }

void *ar_create(float bpm) {
    g_bpm = bpm;
    g_running = 0;
    g_beats = 0.0;
    return api()->create_instance("", 0);
}

void ar_destroy(void *inst) {
    if (inst) api()->destroy_instance(inst);
}

/* Start (or stop) the transport; a started transport counts beats from 0. */
void ar_transport(int running) {
    g_running = running ? 1 : 0;
    g_beats = 0.0;
}

void ar_set(void *inst, const char *key, const char *val) {
    api()->set_param(inst, key, val);
}

int ar_get(void *inst, const char *key, char *buf, int len) {
    return api()->get_param(inst, key, buf, len);
}

void ar_midi(void *inst, int status, int d1, int d2) {
    uint8_t msg[3] = { (uint8_t)status, (uint8_t)d1, (uint8_t)d2 };
    int len = (status & 0xF0) == 0xD0 ? 2 : 3;
    api()->on_midi(inst, msg, len, 0);
}

/* Render up to 128 interleaved stereo frames and advance the transport. */
void ar_render(void *inst, int16_t *out, int frames) {
    api()->render_block(inst, out, frames);
    if (g_running) g_beats += (double)frames * (double)g_bpm / (60.0 * SR);
}

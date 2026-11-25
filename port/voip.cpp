// voip.cpp: the C interface in voip.h over libtgvoip's VoIPController, with the phone's audio.
// libtgvoip (callback audio build) asks for 20 ms blocks of 48 kHz mono 16-bit audio on its own threads:
// the input callback reads them from the microphone, the output callback writes them to the speaker. Both
// block on the sound device, so the device clock paces the call. Audio goes through BB10's audio manager as
// the "voice" type: earpiece by default, headset when plugged, loudspeaker on request.
#include "voip.h"
#include "VoIPController.h"
#include "VoIPServerConfig.h"
#include <sys/asoundlib.h>
#include <audio/audio_manager_routing.h>
#include <audio/audio_manager_device.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <string>
#include <vector>

using namespace tgvoip;

#define RATE 48000
#define BLOCK 960                                   // samples per 20 ms

// ---------------------------------------------------------------- one direction of the phone's audio
struct Pcm {
    snd_pcm_t *pcm; unsigned int am; int channel; bool ok, failed;
    int speaker;                                    // -1 default routing, 0 earpiece, 1 loudspeaker
    Pcm(int ch) : pcm(0), am(0), channel(ch), ok(false), failed(false), speaker(-1) {}
    ~Pcm() { close(); }
    void close() {
        if (pcm) { if (channel == SND_PCM_CHANNEL_PLAYBACK) snd_pcm_plugin_flush(pcm, channel); snd_pcm_close(pcm); pcm = 0; }
        if (am) { audio_manager_free_handle(am); am = 0; }
        ok = false;
    }
    bool open() {
        if (ok) return true;
        if (failed) return false;
        int card = 0, dev = 0;
        int mode = channel == SND_PCM_CHANNEL_PLAYBACK ? SND_PCM_OPEN_PLAYBACK : SND_PCM_OPEN_CAPTURE;
        if (audio_manager_snd_pcm_open_preferred(AUDIO_TYPE_VOICE, &pcm, &am, &card, &dev, mode) < 0) {
            fprintf(stderr, "voip: cannot open voice %s\n", channel ? "input" : "output"); failed = true; pcm = 0; am = 0; return false;
        }
        audio_manager_set_handle_routing_conditions(am, SETTINGS_RESET_ON_DEVICE_CONNECTION | SETTINGS_RESET_ON_DEVICE_DISCONNECTION);
        snd_pcm_channel_info_t pi; memset(&pi, 0, sizeof pi); pi.channel = channel;
        snd_pcm_plugin_info(pcm, &pi);
        snd_pcm_channel_params_t pp; memset(&pp, 0, sizeof pp);
        pp.mode = SND_PCM_MODE_BLOCK; pp.channel = channel;
        pp.start_mode = channel == SND_PCM_CHANNEL_PLAYBACK ? SND_PCM_START_FULL : SND_PCM_START_DATA;
        pp.stop_mode = SND_PCM_STOP_STOP;
        pp.buf.block.frag_size = BLOCK * 2;
        pp.buf.block.frags_min = 1;
        pp.buf.block.frags_max = channel == SND_PCM_CHANNEL_PLAYBACK ? 4 : 8;      // ~80 ms output buffer
        pp.format.interleave = 1; pp.format.rate = RATE; pp.format.voices = 1; pp.format.format = SND_PCM_SFMT_S16_LE;
        strcpy(pp.sw_mixer_subchn_name, "Gram call");
        int r = snd_pcm_plugin_params(pcm, &pp);
        if (r < 0) { fprintf(stderr, "voip: params %d why %u\n", r, pp.why_failed); close(); failed = true; return false; }
        snd_pcm_plugin_prepare(pcm, channel);
        ok = true;
        if (speaker >= 0) route(speaker);
        return true;
    }
    void route(int sp) {
        speaker = sp;
        if (!am) return;
        audio_manager_device_t d = sp ? AUDIO_DEVICE_SPEAKER : AUDIO_DEVICE_HANDSET;
        if (channel == SND_PCM_CHANNEL_PLAYBACK) audio_manager_set_handle_type(am, AUDIO_TYPE_UNCHANGED, d, AUDIO_DEVICE_UNCHANGED);
        else audio_manager_set_handle_type(am, AUDIO_TYPE_UNCHANGED, AUDIO_DEVICE_UNCHANGED, d);
    }
    void recover() {
        snd_pcm_channel_status_t st; memset(&st, 0, sizeof st); st.channel = channel;
        if (snd_pcm_plugin_status(pcm, &st) < 0) { close(); return; }
        if (st.status == SND_PCM_STATUS_CHANGE) { close(); return; }        // route changed: open again
        if (st.status == SND_PCM_STATUS_UNDERRUN || st.status == SND_PCM_STATUS_OVERRUN || st.status == SND_PCM_STATUS_READY
            || st.status == SND_PCM_STATUS_ERROR || st.status == SND_PCM_STATUS_PREEMPTED)
            snd_pcm_plugin_prepare(pcm, channel);
    }
    void write(const int16_t *s, size_t n) {
        if (!open()) { usleep(20000); return; }
        size_t bytes = n * 2, done = 0; int tries = 0;
        while (done < bytes && tries++ < 3) {
            ssize_t w = snd_pcm_plugin_write(pcm, (const char *)s + done, bytes - done);
            if (w > 0) done += w;
            if (done < bytes) { recover(); if (!pcm) return; }
        }
    }
    void read(int16_t *s, size_t n) {
        if (!open()) { memset(s, 0, n * 2); usleep(20000); return; }
        size_t bytes = n * 2, done = 0; int tries = 0;
        while (done < bytes && tries++ < 3) {
            ssize_t r = snd_pcm_plugin_read(pcm, (char *)s + done, bytes - done);
            if (r > 0) done += r;
            if (done < bytes) { recover(); if (!pcm) break; }
        }
        if (done < bytes) memset((char *)s + done, 0, bytes - done);
    }
};

struct gv_call {
    VoIPController *ctl;
    Pcm out, in;
    pthread_mutex_t lock;
    gv_call() : ctl(0), out(SND_PCM_CHANNEL_PLAYBACK), in(SND_PCM_CHANNEL_CAPTURE) { pthread_mutex_init(&lock, 0); }
};

static int netType(int net) {
    return net == 1 ? NET_TYPE_WIFI : net == 2 ? NET_TYPE_LTE : NET_TYPE_UNKNOWN;
}

// ---------------------------------------------------------------- tones
static pthread_mutex_t toneLock = PTHREAD_MUTEX_INITIALIZER;
static pthread_t toneThread; static bool toneRunning = false;
static volatile int toneKind = 0, toneGen = 0, toneSpeaker = 0;
static void *toneRun(void *arg) {
    int gen = (int)(long)arg, kind = toneKind;
    Pcm out(SND_PCM_CHANNEL_PLAYBACK);
    out.speaker = toneSpeaker;
    int16_t buf[BLOCK];
    long t = 0;                                        // samples since start
    const double PI2 = 6.283185307179586;
    long period = kind == 1 ? RATE * 5 : kind == 2 ? RATE : RATE * 2 / 5;
    long on = kind == 1 ? RATE : kind == 2 ? RATE / 2 : RATE * 3 / 20;
    long total = kind == 1 ? RATE * 60 : kind == 2 ? RATE * 3 : RATE * 4 / 5;
    while (toneGen == gen && t < total) {
        for (int i = 0; i < BLOCK; i++, t++) {
            long p = t % period;
            double v = 0;
            if (p < on) {
                double env = p < 480 ? p / 480.0 : on - p < 480 ? (on - p) / 480.0 : 1;   // 10 ms fades: no clicks
                v = sin(PI2 * 425 * t / RATE) * 0.22 * env;
            }
            buf[i] = (int16_t)(v * 32767);
        }
        out.write(buf, BLOCK);
    }
    return 0;
}

extern "C" {
__attribute__((visibility("default"))) void gv_tone(int kind, int speaker) {
    pthread_mutex_lock(&toneLock);
    toneGen++;                                         // the running tone stops at its next block
    if (toneRunning) { pthread_join(toneThread, 0); toneRunning = false; }
    if (kind) {
        toneKind = kind; toneSpeaker = speaker ? 1 : 0;
        toneRunning = pthread_create(&toneThread, 0, toneRun, (void *)(long)toneGen) == 0;
    }
    pthread_mutex_unlock(&toneLock);
}

__attribute__((visibility("default"))) const char *gv_version(void) { return VoIPController::GetVersion(); }
__attribute__((visibility("default"))) int gv_max_layer(void) { return VoIPController::GetConnectionMaxLayer(); }

__attribute__((visibility("default"))) gv_call *gv_start(const char *serverConfig, const unsigned char *key, int outgoing,
        const gv_endpoint *eps, int n, int allowP2p, int maxLayer, int net, const char *logPath) {
    gv_tone(0, 0);
    if (serverConfig && *serverConfig) ServerConfig::GetSharedInstance()->Update(serverConfig);
    gv_call *c = new gv_call();
    c->ctl = new VoIPController();
    VoIPController::Config cfg(30.0, 20.0, DATA_SAVING_NEVER, true, true, true);   // AEC, noise suppression, gain control
    if (logPath) cfg.logFilePath = logPath;
    c->ctl->SetConfig(cfg);
    c->ctl->SetAudioDataCallbacks(
        [c](int16_t *buf, size_t len) { c->in.read(buf, len); },
        [c](int16_t *buf, size_t len) { c->out.write(buf, len); });
    std::vector<Endpoint> list;
    for (int i = 0; i < n; i++) {
        NetworkAddress v4 = eps[i].ipv4 && *eps[i].ipv4 ? NetworkAddress::IPv4(eps[i].ipv4) : NetworkAddress::Empty();
        NetworkAddress v6 = eps[i].ipv6 && *eps[i].ipv6 ? NetworkAddress::IPv6(eps[i].ipv6) : NetworkAddress::Empty();
        unsigned char tag[16]; memcpy(tag, eps[i].peer_tag, 16);
        list.push_back(Endpoint(eps[i].id, (uint16_t)eps[i].port, v4, v6, eps[i].tcp ? Endpoint::Type::TCP_RELAY : Endpoint::Type::UDP_RELAY, tag));
    }
    c->ctl->SetRemoteEndpoints(list, allowP2p != 0, maxLayer);
    char k[256]; memcpy(k, key, 256);
    c->ctl->SetEncryptionKey(k, outgoing != 0);
    c->ctl->SetNetworkType(netType(net));
    c->ctl->Start();
    c->ctl->Connect();
    return c;
}
__attribute__((visibility("default"))) int gv_state(gv_call *c) { return c ? c->ctl->GetConnectionState() : GV_FAILED; }
__attribute__((visibility("default"))) int gv_bars(gv_call *c) { return c ? c->ctl->GetSignalBarsCount() : 0; }
__attribute__((visibility("default"))) int gv_error(gv_call *c) { return c ? c->ctl->GetLastError() : 0; }
__attribute__((visibility("default"))) void gv_mute(gv_call *c, int on) { if (c) c->ctl->SetMicMute(on != 0); }
__attribute__((visibility("default"))) void gv_speaker(gv_call *c, int on) {
    if (!c) return;
    c->out.route(on ? 1 : 0); c->in.route(on ? 1 : 0);
}
__attribute__((visibility("default"))) void gv_network(gv_call *c, int net) { if (c) c->ctl->SetNetworkType(netType(net)); }
__attribute__((visibility("default"))) void gv_stop(gv_call *c) {
    if (!c) return;
    c->ctl->Stop();                                 // joins libtgvoip's threads, so the audio callbacks are done
    delete c->ctl;
    delete c;                                       // closes the sound devices
}
__attribute__((visibility("default"))) int gv_audio_check(int ms, char *report, int len) {
    Pcm out(SND_PCM_CHANNEL_PLAYBACK), in(SND_PCM_CHANNEL_CAPTURE);
    bool o = out.open(), i = in.open();
    int16_t buf[BLOCK]; memset(buf, 0, sizeof buf);
    long got = 0;
    for (int t = 0; t < ms / 20; t++) {
        if (o) out.write(buf, BLOCK);
        if (i) { in.read(buf, BLOCK); got += BLOCK; memset(buf, 0, sizeof buf); }
    }
    snprintf(report, len, "output %s, input %s, read %ld samples", o ? "ok" : "FAILED", i ? "ok" : "FAILED", got);
    return o && i ? 0 : -1;
}
}

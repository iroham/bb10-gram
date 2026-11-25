/* voip.h: Telegram voice calls for BB10 (libgramvoip.so = libtgvoip + the phone's audio).
 * The app does the signalling through TDLib (createCall / acceptCall / discardCall); once TDLib reports
 * callStateReady it starts the media here with the key, the servers and the server config it got. */
#ifndef GRAM_VOIP_H
#define GRAM_VOIP_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct gv_call gv_call;
typedef struct {
    long long id;
    const char *ipv4, *ipv6;        /* "" when missing */
    int port;
    unsigned char peer_tag[16];
    int tcp;                        /* reflector over TCP */
} gv_endpoint;

enum { GV_WAIT_INIT = 1, GV_WAIT_INIT_ACK, GV_ESTABLISHED, GV_FAILED, GV_RECONNECTING };

const char *gv_version(void);       /* library version for callProtocol.library_versions */
int gv_max_layer(void);             /* callProtocol.max_layer */
/* key: 256 bytes; net: 0 unknown, 1 Wi-Fi, 2 mobile.  log_path may be NULL. */
gv_call *gv_start(const char *server_config, const unsigned char *key, int outgoing, const gv_endpoint *eps, int n,
                  int allow_p2p, int max_layer, int net, const char *log_path);
int gv_state(gv_call *c);           /* GV_* */
int gv_bars(gv_call *c);            /* signal 0..4 */
int gv_error(gv_call *c);           /* libtgvoip error code when GV_FAILED */
void gv_mute(gv_call *c, int on);
void gv_speaker(gv_call *c, int on); /* loudspeaker instead of the earpiece (headset wins when plugged) */
void gv_network(gv_call *c, int net);
void gv_stop(gv_call *c);           /* ends the media and frees everything */
/* call tones on the voice output (earpiece, or loudspeaker when speaker=1), until gv_tone(0, ..):
 * 1 ringback (425 Hz, 1 s on / 4 s off), 2 busy (0.5 s on / 0.5 s off, 3 times), 3 call ended (two short beeps) */
void gv_tone(int kind, int speaker);
/* audio check without a call: opens the voice output and input, plays/reads silence for ms, 0 = ok */
int gv_audio_check(int ms, char *report, int len);
#ifdef __cplusplus
}
#endif
#endif

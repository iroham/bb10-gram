// The only symbols libtdjson.so exports: TDLib's JSON interface under tg_ names. Everything else in the library
// (TDLib, OpenSSL, libc++) stays hidden, so it cannot clash with the libstdc++ the Cascades side uses.
#include "td/telegram/td_json_client.h"
#define EXPORT extern "C" __attribute__((visibility("default")))
EXPORT int tg_create_client_id(void) { return td_create_client_id(); }
EXPORT void tg_send(int client_id, const char *request) { td_send(client_id, request); }
EXPORT const char *tg_receive(double timeout) { return td_receive(timeout); }
EXPORT const char *tg_execute(const char *request) { return td_execute(request); }

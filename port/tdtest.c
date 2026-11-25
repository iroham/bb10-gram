/* tdtest: checks on the phone that the cross-built Telegram library runs (threads, polling, sqlite, crypto).
 * Uses no account: it only asks for the first updates and runs two local functions.  tdtest <dir> */
#include <stdio.h>
#include <string.h>
#include <time.h>
int tg_create_client_id(void);
void tg_send(int, const char *);
const char *tg_receive(double);
const char *tg_execute(const char *);
int main(int argc, char **argv) {
    char req[1024]; int i, n = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("start\n");
    const char *r = tg_execute("{\"@type\":\"setLogVerbosityLevel\",\"new_verbosity_level\":1}");
    printf("execute: %s\n", r ? r : "(null)");
    r = tg_execute("{\"@type\":\"getTextEntities\",\"text\":\"@telegram /test_command https://telegram.org telegram.me\"}");
    printf("execute: %.160s\n", r ? r : "(null)");
    int id = tg_create_client_id();
    tg_send(id, "{\"@type\":\"getOption\",\"name\":\"version\",\"@extra\":1}");
    snprintf(req, sizeof req, "{\"@type\":\"setTdlibParameters\",\"database_directory\":\"%s/db\",\"files_directory\":\"%s/files\",\"use_message_database\":true,"
             "\"use_chat_info_database\":true,\"use_file_database\":true,\"api_id\":%s,\"api_hash\":\"%s\",\"system_language_code\":\"en\",\"device_model\":\"BlackBerry Passport\","
             "\"system_version\":\"10.3\",\"application_version\":\"0.1\",\"@extra\":2}", argc > 1 ? argv[1] : ".", argc > 1 ? argv[1] : ".", argc > 2 ? argv[2] : "1", argc > 3 ? argv[3] : "x");
    time_t t0 = time(NULL);
    for (i = 0; i < 400 && time(NULL) - t0 < 12; i++) {
        r = tg_receive(1.0);
        if (!r) continue;
        printf("update %d: %.200s\n", ++n, r);
        if (strstr(r, "authorizationStateWaitTdlibParameters")) tg_send(id, req);
        if (strstr(r, "authorizationStateWaitPhoneNumber")) { printf("OK: the library asked for a phone number (it reached Telegram's servers' setup stage)\n"); break; }
    }
    printf("done, %d updates\n", n);
    return 0;
}

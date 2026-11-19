// Known-answer functional test for the rdpapplist server channel's receive path (normal flags).
#include <stdio.h>
#include <string.h>
#include <winpr/wlog.h>
#include "wts_channel.h"

typedef struct { int calls; UINT16 version; char lang[RDPAPPLIST_LANG_SIZE + 1]; } Seen;

static UINT on_caps(RdpAppListServerContext* context, const RDPAPPLIST_CLIENT_CAPS_PDU* caps)
{
    Seen* s = (Seen*)context->custom;
    s->calls++;
    s->version = caps->version;
    memcpy(s->lang, caps->clientLanguageId, RDPAPPLIST_LANG_SIZE);
    s->lang[RDPAPPLIST_LANG_SIZE] = 0;
    return CHANNEL_RC_OK;
}

static int pass, fail;
static void check(int ok, const char* name)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (ok) pass++; else fail++;
}

static size_t caps_pdu(uint8_t* b, uint32_t cmd, uint32_t len, uint16_t ver, const char* lang)
{
    memset(b, 0, 42);
    memcpy(b, &cmd, 4);
    memcpy(b + 4, &len, 4);
    memcpy(b + 8, &ver, 2);
    strncpy((char*)b + 10, lang, 32);
    return 42;
}

int main(void)
{
    WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
    uint8_t b[128];
    Seen s;

    memset(&s, 0, sizeof s);
    size_t n = caps_pdu(b, 1, 42, 4, "ja_JP");
    UINT rc = rdpapplist_feed(b, n, on_caps, &s);
    check(rc == 0 && s.calls == 1 && s.version == 4 && strcmp(s.lang, "ja_JP") == 0, "client_caps_delivered");

    memset(&s, 0, sizeof s);
    n = caps_pdu(b, 1, 42, 4, "en_US");
    memcpy(b + 42, b, 42);
    b[42 + 8] = 3; // second PDU: version 3, same language
    rdpapplist_feed(b, 84, on_caps, &s);
    check(s.calls == 2 && s.version == 3 && strcmp(s.lang, "en_US") == 0, "two_caps_pdus_in_one_read");

    memset(&s, 0, sizeof s);
    n = caps_pdu(b, 2, 42, 4, "en_US"); // UPDATE_APPLIST is server->client only
    rdpapplist_feed(b, n, on_caps, &s);
    check(s.calls == 0, "unknown_cmd_ignored");

    memset(&s, 0, sizeof s);
    n = caps_pdu(b, 1, 42, 4, "en_US");
    rdpapplist_feed(b, 20, on_caps, &s); // truncated before the 32-byte language id
    check(s.calls == 0, "truncated_caps_rejected");

    printf("RESULT passed=%d failed=%d\n", pass, fail);
    return fail ? 1 : 0;
}

// Fuzz the WSLg rdpapplist server channel plugin (runs in the Linux guest's RDP server) with the
// bytes a client sends on the "Microsoft::Windows::RDS::RemoteApplicationList" dynamic channel:
// rdpapplist_server_handle_messages -> rdpapplist_server_receive_pdu -> rdpapplist_read_header /
// rdpapplist_recv_caps_pdu -> ApplicationListClientCaps callback.
#include <stdint.h>
#include <string.h>
#include <winpr/wlog.h>
#include "wts_channel.h"

static UINT on_caps(RdpAppListServerContext* context, const RDPAPPLIST_CLIENT_CAPS_PDU* caps)
{
    (void)context;
    // What the weston RDP backend does with it: read the version and the language id.
    volatile size_t n = strnlen(caps->clientLanguageId, sizeof(caps->clientLanguageId));
    (void)n;
    return caps->version >= 1 ? CHANNEL_RC_OK : ERROR_INVALID_DATA;
}

int LLVMFuzzerInitialize(int* argc, char*** argv)
{
    (void)argc; (void)argv;
    WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size > 0xFFFFFFFFu) return -1; // a channel read reports a ULONG size
    rdpapplist_feed(data, size, on_caps, NULL);
    return 0;
}

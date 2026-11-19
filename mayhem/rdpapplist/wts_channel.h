// In-memory stand-in for the FreeRDP server's virtual channel transport (the WTS* API), so the
// rdpapplist server plugin (rdpapplist/server/rdpapplist_main.c) can be driven through its public
// API: rdpapplist_server_context_new() -> Open() -> receive thread -> Close() -> _free().
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "rdpapplist_server.h"

// Opens a server context, delivers `data` as one channel read (what a client sends), waits until
// the receive thread has consumed it, then closes and frees the context. Returns Open()'s result.
UINT rdpapplist_feed(const uint8_t* data, size_t size, psRdpAppListClientCaps on_caps, void* custom);

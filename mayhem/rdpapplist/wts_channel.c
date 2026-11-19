#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/wtsapi.h>
#include <freerdp/channels/wtsvc.h>

#include <winpr/stream.h>
#include "wts_channel.h"
#include "rdpapplist_main.h" /* RdpAppListServerPrivate: to know whether Open() started the thread */

// One channel at a time (the harness and the test are single-session).
static struct {
    const uint8_t* data;
    size_t size;
    int reads;            // number of WTSVirtualChannelRead calls so far
    int started;          // the receive thread reached its first read
    HANDLE event;         // the channel's event handle (winpr manual-reset event)
    pthread_mutex_t mu;
    pthread_cond_t cv;
} g_ch = { .mu = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER };

static DWORD g_session_id = 1;

BOOL WINAPI WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass,
                                        LPSTR* ppBuffer, DWORD* pBytesReturned)
{
    (void)hServer; (void)SessionId;
    if (WTSInfoClass != WTSSessionId) return FALSE;
    ULONG* id = malloc(sizeof(ULONG));
    *id = g_session_id;
    *ppBuffer = (LPSTR)id;
    *pBytesReturned = sizeof(ULONG);
    return TRUE;
}

VOID WINAPI WTSFreeMemory(PVOID pMemory) { free(pMemory); }

HANDLE WINAPI WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags)
{
    (void)SessionId; (void)pVirtualName; (void)flags;
    g_ch.event = CreateEvent(NULL, TRUE, TRUE, NULL); // data is pending right away
    return g_ch.event ? (HANDLE)&g_ch : NULL;
}

BOOL WINAPI WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass,
                                   PVOID* ppBuffer, DWORD* pBytesReturned)
{
    (void)hChannelHandle;
    if (WtsVirtualClass == WTSVirtualEventHandle) {
        HANDLE* h = malloc(sizeof(HANDLE));
        *h = g_ch.event;
        *ppBuffer = h;
        *pBytesReturned = sizeof(HANDLE);
        return TRUE;
    }
    if (WtsVirtualClass == WTSVirtualChannelReady) {
        BOOL* b = malloc(sizeof(BOOL));
        *b = TRUE;
        *ppBuffer = b;
        *pBytesReturned = sizeof(BOOL);
        return TRUE;
    }
    return FALSE;
}

BOOL WINAPI WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize,
                                  PULONG pBytesRead)
{
    (void)hChannelHandle; (void)TimeOut;
    pthread_mutex_lock(&g_ch.mu);
    int n = g_ch.reads++;
    if (!g_ch.started) { g_ch.started = 1; pthread_cond_broadcast(&g_ch.cv); }
    pthread_mutex_unlock(&g_ch.mu);
    if (n == 0) { // size query (Buffer == NULL)
        *pBytesRead = (ULONG)g_ch.size;
        return TRUE;
    }
    if (n == 1) {
        size_t c = g_ch.size < BufferSize ? g_ch.size : BufferSize;
        if (c) memcpy(Buffer, g_ch.data, c);
        *pBytesRead = (ULONG)c;
        ResetEvent(g_ch.event); // nothing more is pending
        return TRUE;
    }
    *pBytesRead = 0;
    SetLastError(ERROR_NO_DATA);
    return FALSE;
}

BOOL WINAPI WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten)
{
    (void)hChannelHandle; (void)Buffer;
    if (pBytesWritten) *pBytesWritten = Length;
    return TRUE;
}

BOOL WINAPI WTSVirtualChannelClose(HANDLE hChannelHandle)
{
    if (hChannelHandle == (HANDLE)&g_ch && g_ch.event) {
        CloseHandle(g_ch.event);
        g_ch.event = NULL;
    }
    return TRUE;
}

UINT rdpapplist_feed(const uint8_t* data, size_t size, psRdpAppListClientCaps on_caps, void* custom)
{
    g_ch.data = data;
    g_ch.size = size;
    g_ch.reads = 0;
    g_ch.started = 0;

    RdpAppListServerContext* ctx = rdpapplist_server_context_new((HANDLE)0x1);
    if (!ctx) return ERROR_INTERNAL_ERROR;
    ctx->custom = custom;
    ctx->ApplicationListClientCaps = on_caps;
    UINT rc = ctx->Open(ctx);
    if (rc == CHANNEL_RC_OK && ctx->priv->thread) {
        // The whole input is parsed inside the thread's first handle_messages() call, which begins
        // with the first read; Close() then waits for that call to return before stopping.
        pthread_mutex_lock(&g_ch.mu);
        while (!g_ch.started) pthread_cond_wait(&g_ch.cv, &g_ch.mu);
        pthread_mutex_unlock(&g_ch.mu);
    }
    rdpapplist_server_context_free(ctx);
    return rc;
}

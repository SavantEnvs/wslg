// Fuzz the WSLg RDP client plugin's "rdpapplist" dynamic-virtual-channel receive path: the bytes the
// Linux guest (WSLg) sends to the Windows client. Entry is the plugin's own public COM entry:
// WSLDVCCallback_CreateInstance() -> IWTSVirtualChannelCallback::OnDataReceived() -> OnClose().
// A fresh callback (and so a fresh WSLDVCFileDB) is created per input; nothing persists.
#include "pch.h"
#include "WSLDVCCallback.h"

namespace {
class NullChannel : public IWTSVirtualChannel {
public:
    HRESULT QueryInterface(REFIID, void** ppv) override { *ppv = nullptr; return E_NOINTERFACE; }
    ULONG AddRef() override { return 1; }
    ULONG Release() override { return 1; }
    HRESULT Write(ULONG cbSize, BYTE* pBuffer, IUnknown*) override
    {
        volatile BYTE sink = 0;
        for (ULONG i = 0; i < cbSize; i++) sink ^= pBuffer[i];
        (void)sink;
        return S_OK;
    }
    HRESULT Close() override { return S_OK; }
};
} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size > 0xFFFFFFFFu) return -1; // OnDataReceived takes a ULONG size
    NullChannel channel;
    IWTSVirtualChannelCallback* cb = nullptr;
    if (FAILED(WSLDVCCallback_CreateInstance(&channel, &cb)) || !cb) return 0;
    // Exact-size heap copy so ASan sees any read past the received buffer.
    BYTE* buf = (BYTE*)malloc(size ? size : 1);
    if (size) memcpy(buf, data, size);
    cb->OnDataReceived((ULONG)size, buf);
    cb->OnClose();
    cb->Release();
    free(buf);
    return 0;
}

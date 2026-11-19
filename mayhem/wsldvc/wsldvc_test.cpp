// Known-answer functional test for the WSLDVCPlugin rdpapplist client (built with normal flags).
// Drives WSLDVCCallback through its COM entry and asserts the protocol outcomes: the caps reply
// bytes, accepted/rejected PDUs, the .lnk/.ico that would be created and the icon file bytes.
#include "pch.h"
#include "WSLDVCCallback.h"
#include "shim_hooks.h"
#include "pdu.h"
#include <stdio.h>

namespace {
class RecChannel : public IWTSVirtualChannel {
public:
    std::vector<std::vector<uint8_t>> writes;
    HRESULT QueryInterface(REFIID, void** ppv) override { *ppv = nullptr; return E_NOINTERFACE; }
    ULONG AddRef() override { return 1; }
    ULONG Release() override { return 1; }
    HRESULT Write(ULONG cb, BYTE* p, IUnknown*) override { writes.emplace_back(p, p + cb); return S_OK; }
    HRESULT Close() override { return S_OK; }
};

int g_pass, g_fail;
void check(bool ok, const char* name)
{
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    (ok ? g_pass : g_fail)++;
}

struct Session {
    RecChannel ch;
    IWTSVirtualChannelCallback* cb = nullptr;
    Session() { ShimReset(); WSLDVCCallback_CreateInstance(&ch, &cb); }
    ~Session() { if (cb) { cb->OnClose(); cb->Release(); } }
    HRESULT feed(std::vector<uint8_t> v) { return cb->OnDataReceived((ULONG)v.size(), v.data()); }
};

const std::wstring G = L"0bf1a8bd-3a4b-4e7c-9b0f-1a2b3c4d5e6f";
const std::wstring MENU = L"C:\\Users\\user\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Ubuntu";
const std::wstring ICONDIR = L"C:\\Users\\user\\AppData\\Local\\Temp\\WSLDVCPlugin\\Ubuntu";
} // namespace

int main()
{
    {
        Session s;
        check(s.cb != nullptr, "create_instance");
        HRESULT hr = s.feed(capsPdu(4, L"Ubuntu", G));
        check(hr == S_OK, "caps_v4_accepted");
        static const uint8_t expect[42] = {1, 0, 0, 0, 42, 0, 0, 0, 4, 0, 'e', 'n', '_', 'U', 'S'};
        check(s.ch.writes.size() == 1 && s.ch.writes[0].size() == 42 &&
                  memcmp(s.ch.writes[0].data(), expect, 42) == 0,
              "caps_reply_bytes");

        hr = s.feed(updatePdu(0x7F, L"firefox", L"Internet", L"/usr/bin/firefox", L"/home/u", L"Firefox", 16, 16, 32, 1024, 0xAB));
        check(hr == S_OK, "update_with_icon_accepted");
        check(g_shimLastLink.calls == 1 && g_shimLastLink.link == MENU + L"\\Internet\\Firefox.lnk", "update_link_path");
        check(g_shimLastLink.args == L"-d Ubuntu --cd \"/home/u\" -- /usr/bin/firefox", "update_link_args");
        check(g_shimLastLink.obj == L"C:\\Program Files\\WSL\\wslg.exe" && g_shimLastLink.workingDir == L"C:\\Windows\\system32",
              "update_link_target");
        check(g_shimLastIcon.calls == 1 && g_shimLastIcon.path == ICONDIR + L"\\Internet\\firefox.ico", "update_icon_path");
        const auto& ico = g_shimLastIcon.data;
        bool icoOk = ico.size() == 22 + 40 + 1024 && ico[2] == 1 && ico[4] == 1 && ico[6] == 16 && ico[7] == 16 &&
                     ico[12] == 32 && ico[22] == 40 && ico[30] == 32 /* biHeight = 2*16 */ && ico[22 + 40] == 0xAB &&
                     ico.back() == 0xAB;
        check(icoOk, "update_icon_file_bytes");

        hr = s.feed(deletePdu(0x03, L"firefox", L"Internet"));
        check(hr == S_OK, "delete_known_accepted");
        hr = s.feed(deletePdu(0x03, L"firefox", L"Internet"));
        check(FAILED(hr), "delete_unknown_rejected");
        hr = s.feed(updatePdu(0x1D, L"x", L"", L"/x", L"", L"X", 1, 1, 32, 0xFFFFFFF0u));
        check(FAILED(hr), "icon_size_overflow_rejected");
        hr = s.feed(updatePdu(0x0D, L"a..b", L"", L"/x", L"", L"X"));
        check(FAILED(hr), "dotdot_appid_rejected");
        hr = s.feed(updatePdu(0x0C, L"", L"", L"/x", L"", L"X"));
        check(FAILED(hr), "missing_required_flag_rejected");
    }
    {
        Session s;
        HRESULT hr = s.feed(updatePdu(0x0D, L"a", L"", L"/a", L"", L"A"));
        check(FAILED(hr) && g_shimLastLink.calls == 0, "update_before_caps_rejected");
    }
    {
        Session s;
        HRESULT hr = s.feed(capsPdu(3, L"Ubuntu", L""));
        check(FAILED(hr) && s.ch.writes.empty(), "caps_v3_rejected");
    }
    {
        Session s;
        HRESULT hr = s.feed(capsPdu(4, L"bad name", G));
        check(FAILED(hr) && s.ch.writes.empty(), "caps_space_in_provider_rejected");
    }
    {
        Session s;
        HRESULT hr = s.feed(capsPdu(4, L"Ubuntu", L"not-a-guid"));
        check(FAILED(hr) && s.ch.writes.empty(), "caps_bad_guid_rejected");
    }
    printf("RESULT passed=%d failed=%d\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}

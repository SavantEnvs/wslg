// Writes the committed seed corpus (mayhem/wsldvc_ondatareceived/testsuite). Run by hand only:
// clang++ -std=c++17 mayhem/wsldvc/gen_seeds.cpp -o /tmp/gen && /tmp/gen <dir>
#include "pdu.h"
#include <stdio.h>

static const std::wstring G = L"0bf1a8bd-3a4b-4e7c-9b0f-1a2b3c4d5e6f";

static void put(const char* dir, const char* name, const std::vector<uint8_t>& v)
{
    std::string p = std::string(dir) + "/" + name;
    FILE* f = fopen(p.c_str(), "wb");
    fwrite(v.data(), 1, v.size(), f);
    fclose(f);
}

int main(int argc, char** argv)
{
    const char* d = argv[1];
    auto caps = capsPdu(4, L"Ubuntu", G);
    put(d, "caps", caps);
    put(d, "caps_update", cat({caps, updatePdu(0x0D, L"org.gnome.gedit", L"", L"/usr/bin/gedit", L"", L"Text Editor")}));
    put(d, "caps_update_icon_group", cat({caps, updatePdu(0x7F, L"firefox", L"Internet", L"/usr/bin/firefox %u", L"/home/user", L"Firefox", 16, 16, 32, 16 * 16 * 4)}));
    put(d, "caps_update_delete", cat({caps, updatePdu(0x0F, L"xterm", L"System", L"/usr/bin/xterm", L"", L"XTerm"), deletePdu(0x03, L"xterm", L"System")}));
    put(d, "caps_sync", cat({caps,
        updatePdu(0x0D | 0x00010000 | 0x00100000 | 0x00200000, L"a", L"", L"/bin/a", L"", L"A"),
        updatePdu(0x1D | 0x00010000 | 0x00100000, L"b", L"", L"/bin/b", L"", L"B", 2, 2, 32, 16),
        updatePdu(0x0D | 0x00010000 | 0x00100000 | 0x00400000, L"c", L"", L"/bin/c", L"", L"C")}));
    put(d, "caps_update_associate", cat({caps, updatePdu(0x0D, L"gedit", L"", L"/usr/bin/gedit", L"", L"Gedit"),
        associatePdu(0x8D, 7, L"gedit", L"", L"/usr/bin/gedit", L"Gedit"), deletePdu(1, L"gedit", L""), Pdu{}.b}));
    put(d, "caps_provider_delete", cat({caps, withHeader(4, Pdu{})}));
    return 0;
}

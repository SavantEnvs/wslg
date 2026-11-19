// Little helpers to build rdpapplist PDUs (server -> client) for the test and the seed generator.
#pragma once
#include <stdint.h>
#include <string>
#include <vector>

struct Pdu {
    std::vector<uint8_t> b;
    void u16(uint16_t v) { b.push_back(v & 0xff); b.push_back(v >> 8); }
    void u32(uint32_t v) { for (int i = 0; i < 4; i++) b.push_back((v >> (8 * i)) & 0xff); }
    // u16 byte length + string in the build's WCHAR encoding, no terminator
    void str(const std::wstring& s)
    {
        u16((uint16_t)(s.size() * sizeof(wchar_t)));
        for (wchar_t c : s)
            for (size_t i = 0; i < sizeof(wchar_t); i++) b.push_back(((uint32_t)c >> (8 * i)) & 0xff);
    }
    void raw(const std::vector<uint8_t>& v) { b.insert(b.end(), v.begin(), v.end()); }
};

inline std::vector<uint8_t> withHeader(uint32_t cmd, const Pdu& body)
{
    Pdu p;
    p.u32(cmd);
    p.u32((uint32_t)(8 + body.b.size()));
    p.raw(body.b);
    return p.b;
}

inline std::vector<uint8_t> capsPdu(uint16_t version, const std::wstring& provider, const std::wstring& guid)
{
    Pdu body;
    body.u16(version);
    body.str(provider);
    if (version >= 4) body.str(guid);
    return withHeader(1, body);
}

inline std::vector<uint8_t> updatePdu(uint32_t flags, const std::wstring& id, const std::wstring& group,
    const std::wstring& exec, const std::wstring& wd, const std::wstring& desc,
    uint32_t iconW = 0, uint32_t iconH = 0, uint32_t bpp = 32, uint32_t bitsLen = 0, uint8_t fill = 0xAB)
{
    Pdu body;
    body.u32(flags);
    if (flags & 0x01) body.str(id);
    if (flags & 0x02) body.str(group);
    if (flags & 0x04) body.str(exec);
    if (flags & 0x40) body.str(wd);
    if (flags & 0x08) body.str(desc);
    if (flags & 0x10) {
        body.u32(0); body.u32(iconW); body.u32(iconH); body.u32(iconW * 4); body.u32(bpp); body.u32(0);
        body.u32(bitsLen);
        if (bitsLen < 0x100000) body.raw(std::vector<uint8_t>(bitsLen, fill));
    }
    return withHeader(2, body);
}

inline std::vector<uint8_t> deletePdu(uint32_t flags, const std::wstring& id, const std::wstring& group)
{
    Pdu body;
    body.u32(flags);
    if (flags & 0x01) body.str(id);
    if (flags & 0x02) body.str(group);
    return withHeader(3, body);
}

inline std::vector<uint8_t> associatePdu(uint32_t flags, uint32_t windowId, const std::wstring& id,
    const std::wstring& group, const std::wstring& exec, const std::wstring& desc)
{
    Pdu body;
    body.u32(flags);
    body.u32(windowId);
    if (flags & 0x01) body.str(id);
    if (flags & 0x02) body.str(group);
    if (flags & 0x04) body.str(exec);
    if (flags & 0x08) body.str(desc);
    return withHeader(5, body);
}

inline std::vector<uint8_t> cat(std::initializer_list<std::vector<uint8_t>> parts)
{
    std::vector<uint8_t> out;
    for (auto& p : parts) out.insert(out.end(), p.begin(), p.end());
    return out;
}

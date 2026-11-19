// Observation hooks into the shim, used by the functional test (wsldvc_test.cpp) only.
#pragma once
#include <windows.h>
#include <vector>

struct ShimLastIcon {
    std::vector<BYTE> data;   // bytes handed to CreateIconFile
    std::wstring path;        // icon path it would have been written to
    int calls = 0;
};
struct ShimLastLink {
    std::wstring link, obj, args, workingDir, desc, icon;
    int calls = 0;
};
extern ShimLastIcon g_shimLastIcon;
extern ShimLastLink g_shimLastLink;
void ShimReset();

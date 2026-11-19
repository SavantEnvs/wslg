// Linux stand-ins for the Win32 APIs used by WSLDVCCallback.cpp / WSLDVCFileDB.cpp, and for the
// Windows-shell glue in WSLDVCPlugin/utils.cpp (which needs IShellLink/IPropertyStore and cannot be
// built here). No real file system, registry or window is touched: directories "exist", shortcut
// and icon writes are recorded in memory (shim_hooks.h), so fuzz inputs do not leak state.
#include "pch.h"
#include "utils.h"
#include "shim_hooks.h"

#include <errno.h>
#include <string>

ShimLastIcon g_shimLastIcon;
ShimLastLink g_shimLastLink;
static thread_local DWORD g_lastError;

void ShimReset()
{
    g_shimLastIcon = ShimLastIcon();
    g_shimLastLink = ShimLastLink();
    g_lastError = 0;
}

void* LocalAlloc(UINT flags, size_t bytes)
{
    return (flags & LPTR) ? calloc(1, bytes ? bytes : 1) : malloc(bytes ? bytes : 1);
}
void* LocalFree(void* p) { free(p); return nullptr; }

HRESULT UIntAdd(UINT a, UINT b, UINT* result)
{
    if (a + b < a) { *result = 0xFFFFFFFFu; return INTSAFE_E_ARITHMETIC_OVERFLOW; }
    *result = a + b;
    return S_OK;
}

void InitializeCriticalSection(CRITICAL_SECTION* cs)
{
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&cs->m, &a);
    pthread_mutexattr_destroy(&a);
}
void EnterCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_lock(&cs->m); }
void LeaveCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_unlock(&cs->m); }
void DeleteCriticalSection(CRITICAL_SECTION* cs) { pthread_mutex_destroy(&cs->m); }
DWORD GetLastError(void) { return g_lastError; }

int lstrlenW(LPCWSTR s) { return s ? (int)wcslen(s) : 0; }

int wcscpy_s(WCHAR* dst, size_t n, const WCHAR* src)
{
    if (!dst || !n) return EINVAL;
    if (!src) { dst[0] = 0; return EINVAL; }
    size_t l = wcslen(src);
    if (l + 1 > n) { dst[0] = 0; return ERANGE; }
    memcpy(dst, src, (l + 1) * sizeof(WCHAR));
    return 0;
}
int wcscat_s(WCHAR* dst, size_t n, const WCHAR* src)
{
    if (!dst || !n) return EINVAL;
    size_t d = wcsnlen(dst, n);
    if (d == n) { dst[0] = 0; return EINVAL; } // unterminated destination
    if (!src) { dst[0] = 0; return EINVAL; }
    size_t l = wcslen(src);
    if (d + l + 1 > n) { dst[0] = 0; return ERANGE; }
    memcpy(dst + d, src, (l + 1) * sizeof(WCHAR));
    return 0;
}
int strcpy_s(char* dst, size_t n, const char* src)
{
    if (!dst || !n) return EINVAL;
    if (!src) { dst[0] = 0; return EINVAL; }
    size_t l = strlen(src);
    if (l + 1 > n) { dst[0] = 0; return ERANGE; }
    memcpy(dst, src, l + 1);
    return 0;
}
int strncpy_s(char* dst, size_t n, const char* src, size_t count)
{
    if (!dst || !n) return EINVAL;
    if (!src) { dst[0] = 0; return EINVAL; }
    size_t l = strnlen(src, count);
    if (l + 1 > n) { dst[0] = 0; return ERANGE; }
    memcpy(dst, src, l);
    dst[l] = 0;
    return 0;
}
int swprintf_s(WCHAR* buf, size_t n, const WCHAR* fmt, ...)
{
    if (!buf || !n || !fmt) return -1;
    std::wstring f;
    for (const WCHAR* p = fmt; *p; ++p) {
        f += *p;
        if (*p == L'%' && p[1] == L'%') { f += L'%'; ++p; }
        else if (*p == L'%' && p[1] == L's') { f += L'l'; }
    }
    va_list ap;
    va_start(ap, fmt);
    int r = vswprintf(buf, n, f.c_str(), ap);
    va_end(ap);
    if (r < 0) buf[0] = 0;
    return r;
}

static const WCHAR c_installLocation[] = L"C:\\Program Files\\WSL";
LONG RegOpenKeyExW(HKEY, LPCWSTR, DWORD, DWORD, HKEY* result)
{
    *result = (HKEY)(uintptr_t)0x1234;
    return ERROR_SUCCESS;
}
LONG RegGetValueW(HKEY, LPCWSTR, LPCWSTR, DWORD, DWORD* type, void* data, DWORD* size)
{
    if (type) *type = RRF_RT_REG_SZ;
    if (!data || *size < sizeof(c_installLocation)) { *size = sizeof(c_installLocation); return ERROR_MORE_DATA; }
    memcpy(data, c_installLocation, sizeof(c_installLocation));
    *size = sizeof(c_installLocation);
    return ERROR_SUCCESS;
}
LONG RegCloseKey(HKEY) { return ERROR_SUCCESS; }

DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD size)
{
    std::wstring s = src, out;
    for (size_t i = 0; i < s.size();) {
        if (s.compare(i, 8, L"%windir%") == 0) { out += L"C:\\Windows"; i += 8; }
        else out += s[i++];
    }
    if (out.size() + 1 > size) return (DWORD)out.size() + 1;
    memcpy(dst, out.c_str(), (out.size() + 1) * sizeof(WCHAR));
    return (DWORD)out.size() + 1;
}

BOOL CreateDirectoryW(LPCWSTR path, void*)
{
    (void)wcslen(path);
    g_lastError = ERROR_ALREADY_EXISTS;
    return FALSE;
}
BOOL DeleteFileW(LPCWSTR path) { (void)wcslen(path); return TRUE; }
BOOL RemoveDirectory(LPCWSTR path) { (void)wcslen(path); return TRUE; }
BOOL PathIsDirectoryEmptyW(LPCWSTR path) { (void)wcslen(path); return FALSE; }
HANDLE FindFirstFile(LPCWSTR pattern, WIN32_FIND_DATA*)
{
    (void)wcslen(pattern);
    g_lastError = ERROR_FILE_NOT_FOUND;
    return INVALID_HANDLE_VALUE;
}
BOOL FindNextFile(HANDLE, WIN32_FIND_DATA*) { return FALSE; }
BOOL FindClose(HANDLE) { return TRUE; }
LPCWSTR PathFindExtensionW(LPCWSTR path)
{
    LPCWSTR ext = nullptr;
    for (LPCWSTR p = path; *p; ++p) {
        if (*p == L'\\' || *p == L' ') ext = nullptr;
        else if (*p == L'.') ext = p;
    }
    return ext ? ext : path + wcslen(path);
}

// One top-level window, carrying a WSLg window id that never matches (ids are provider-GUID based).
HANDLE GetPropW(HWND, LPCWSTR name) { (void)wcslen(name); return (HANDLE)(uintptr_t)0x1; }
BOOL EnumWindows(WNDENUMPROC cb, LPARAM lparam) { cb((HWND)(uintptr_t)0x10, lparam); return TRUE; }

static int hexval(WCHAR c)
{
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    return -1;
}
static bool readhex(LPCWSTR& p, int digits, uint64_t& v)
{
    v = 0;
    for (int i = 0; i < digits; i++) {
        int h = hexval(p[i]);
        if (h < 0) return false;
        v = (v << 4) | (uint64_t)h;
    }
    p += digits;
    return true;
}
HRESULT CLSIDFromString(LPCWSTR s, CLSID* clsid)
{
    // "{xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx}"
    uint64_t v;
    CLSID g = {};
    if (!s || wcslen(s) != 38 || *s++ != L'{') return CO_E_CLASSSTRING;
    if (!readhex(s, 8, v)) return CO_E_CLASSSTRING; g.Data1 = (uint32_t)v;
    if (*s++ != L'-' || !readhex(s, 4, v)) return CO_E_CLASSSTRING; g.Data2 = (uint16_t)v;
    if (*s++ != L'-' || !readhex(s, 4, v)) return CO_E_CLASSSTRING; g.Data3 = (uint16_t)v;
    if (*s++ != L'-') return CO_E_CLASSSTRING;
    for (int i = 0; i < 8; i++) {
        if (i == 2 && *s++ != L'-') return CO_E_CLASSSTRING;
        if (!readhex(s, 2, v)) return CO_E_CLASSSTRING;
        g.Data4[i] = (uint8_t)v;
    }
    if (*s != L'}') return CO_E_CLASSSTRING;
    *clsid = g;
    return S_OK;
}

// ---- utils.h (Windows-shell glue) ----
void DebugPrint(const wchar_t*, ...) {}

BOOL IsDirectoryPresent(LPCWSTR lpszPath) { (void)wcslen(lpszPath); return TRUE; }

HRESULT CreateShellLink(LPCWSTR lpszPathLink, LPCWSTR lpszPathObj, LPCWSTR lpszArgs,
    LPCWSTR lpszWorkingDir, LPCWSTR lpszDesc, LPCWSTR lpszPathIcon)
{
    g_shimLastLink.link = lpszPathLink;
    g_shimLastLink.obj = lpszPathObj;
    g_shimLastLink.args = lpszArgs;
    g_shimLastLink.workingDir = lpszWorkingDir;
    g_shimLastLink.desc = lpszDesc;
    g_shimLastLink.icon = lpszPathIcon ? lpszPathIcon : L"";
    g_shimLastLink.calls++;
    return S_OK;
}

HRESULT GetIconFileFromShellLink(UINT32 iconPathSize, LPWSTR iconPath, LPCWSTR lnkPath)
{
    (void)wcslen(lnkPath);
    if (iconPathSize) iconPath[0] = 0;
    return E_FAIL;
}

HRESULT CreateIconFile(BYTE* pBuffer, UINT32 cbSize, LPCWSTR lpszIconFile)
{
    // WriteFile(hFile, pBuffer, cbSize): touch every byte, as the real write would.
    g_shimLastIcon.data.assign(pBuffer, pBuffer + cbSize);
    g_shimLastIcon.path = lpszIconFile;
    g_shimLastIcon.calls++;
    return S_OK;
}

BOOL GetLocaleName(char* localeName, int localeNameSize)
{
    return strcpy_s(localeName, localeNameSize, "en_US") == 0;
}

// Same logic as utils.cpp, with fixed known-folder / temp paths.
HRESULT BuildMenuPath(UINT32 appMenuPathSize, LPWSTR appMenuPath, LPCWSTR appProvider, bool isCreateDir)
{
    const WCHAR* knownFolderPath = L"C:\\Users\\user\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu";
    int ret = swprintf_s(appMenuPath, appMenuPathSize, L"%s\\Programs\\%s", knownFolderPath, appProvider);
    if (ret < 0) return E_FAIL;
    if (isCreateDir && !CreateDirectoryW(appMenuPath, NULL) && ERROR_ALREADY_EXISTS != GetLastError())
        return E_FAIL;
    return S_OK;
}

HRESULT BuildIconPath(UINT32 iconPathSize, LPWSTR iconPath, LPCWSTR appProvider, bool isCreateDir)
{
    WCHAR prefix[] = L"WSLDVCPlugin\\";
    const WCHAR tempPath[] = L"C:\\Users\\user\\AppData\\Local\\Temp\\";
    UINT32 lenTempPath = (UINT32)wcslen(tempPath);
    if (lenTempPath + 1 > iconPathSize) return E_FAIL;
    memcpy(iconPath, tempPath, sizeof(tempPath));
    if ((lenTempPath + ARRAYSIZE(prefix) + wcslen(appProvider)) > iconPathSize) return E_FAIL;
    if (wcscat_s(iconPath, iconPathSize, prefix) != 0) return E_FAIL;
    if (isCreateDir && !CreateDirectoryW(iconPath, NULL) && ERROR_ALREADY_EXISTS != GetLastError())
        return E_FAIL;
    if (wcscat_s(iconPath, iconPathSize, appProvider) != 0) return E_FAIL;
    if (isCreateDir && !CreateDirectoryW(iconPath, NULL) && ERROR_ALREADY_EXISTS != GetLastError())
        return E_FAIL;
    return S_OK;
}

HRESULT UpdateTaskBarInfo(HWND, LPCWSTR relaunchCmdline, LPCWSTR displayName, LPCWSTR iconPath)
{
    if (relaunchCmdline) (void)wcslen(relaunchCmdline);
    if (displayName) (void)wcslen(displayName);
    if (iconPath) (void)wcslen(iconPath);
    return S_OK;
}

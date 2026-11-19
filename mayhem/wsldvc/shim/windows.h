// Minimal Win32 / COM / WRL shim so the WSLDVCPlugin protocol code (WSLDVCCallback.cpp,
// WSLDVCFileDB.cpp) compiles and runs unmodified on Linux for fuzzing. Everything that would
// touch the Windows shell, registry or file system is an in-memory stub (shim.cpp), so each
// fuzz input's result depends only on that input.
//
// WCHAR is the native Linux wchar_t (32-bit): string payloads in the rdpapplist PDUs are
// therefore UTF-32LE in this build instead of UTF-16LE. The byte-level parsing (lengths,
// bounds checks, memcpy sizes, struct layouts in bytes) is unchanged.
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <pthread.h>

typedef int BOOL;
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef unsigned int UINT;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef char CHAR;
typedef wchar_t WCHAR;
typedef WCHAR* LPWSTR;
typedef WCHAR* PWSTR;
typedef const WCHAR* LPCWSTR;
typedef int32_t HRESULT;
typedef void* HANDLE;
typedef void* HWND;
typedef void* HKEY;
typedef intptr_t LPARAM;
typedef DWORD LCID;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define S_OK ((HRESULT)0)
#define E_FAIL ((HRESULT)0x80004005)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_POINTER ((HRESULT)0x80004003)
#define CO_E_CLASSSTRING ((HRESULT)0x800401F3)
#define INTSAFE_E_ARITHMETIC_OVERFLOW ((HRESULT)0x80070216)
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define HRESULT_FROM_WIN32(x) \
    ((HRESULT)(x) <= 0 ? ((HRESULT)(x)) : ((HRESULT)(((x) & 0x0000FFFF) | 0x80070000)))

#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define ERROR_ALREADY_EXISTS 183L
#define ERROR_MORE_DATA 234L

#define MAX_PATH 260
#define LPTR 0x0040
#define BI_RGB 0L
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#define ZeroMemory(p, n) memset((p), 0, (n))
#define UNREFERENCED_PARAMETER(p) (void)(p)
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10

#define HKEY_LOCAL_MACHINE ((HKEY)(uintptr_t)0x80000002)
#define KEY_READ 0x20019
#define RRF_RT_REG_SZ 0x00000002
#define RRF_RT_REG_EXPAND_SZ 0x00000004

#define STDMETHODCALLTYPE
#define STDMETHODIMP HRESULT
#define STDMETHODIMP_(t) t
#define CALLBACK
#define MIDL_INTERFACE(x) struct
#define __RPC__in_ecount_full(x)

// SAL annotations
#define _In_
#define _In_z_
#define _In_opt_
#define _In_opt_z_
#define _Out_
#define _Inout_
#define _Out_writes_z_(x)
#define _In_reads_bytes_(x)
#define _Use_decl_annotations_

typedef struct _GUID {
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} GUID, IID, CLSID;
typedef const IID& REFIID;

typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER;

typedef struct _WIN32_FIND_DATAW {
    DWORD dwFileAttributes;
    WCHAR cFileName[MAX_PATH];
} WIN32_FIND_DATA;

typedef struct _CRITICAL_SECTION {
    pthread_mutex_t m;
} CRITICAL_SECTION;

typedef BOOL (*WNDENUMPROC)(HWND, LPARAM);

// --- kernel32 / advapi32 / user32 / shell stubs (shim.cpp) ---
void* LocalAlloc(UINT flags, size_t bytes);
void* LocalFree(void* p);
HRESULT UIntAdd(UINT a, UINT b, UINT* result);
void InitializeCriticalSection(CRITICAL_SECTION* cs);
void EnterCriticalSection(CRITICAL_SECTION* cs);
void LeaveCriticalSection(CRITICAL_SECTION* cs);
void DeleteCriticalSection(CRITICAL_SECTION* cs);
DWORD GetLastError(void);
LONG RegOpenKeyExW(HKEY key, LPCWSTR subKey, DWORD options, DWORD sam, HKEY* result);
LONG RegGetValueW(HKEY key, LPCWSTR subKey, LPCWSTR value, DWORD flags, DWORD* type, void* data, DWORD* size);
LONG RegCloseKey(HKEY key);
DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD size);
BOOL CreateDirectoryW(LPCWSTR path, void* sa);
BOOL DeleteFileW(LPCWSTR path);
BOOL RemoveDirectory(LPCWSTR path);
BOOL PathIsDirectoryEmptyW(LPCWSTR path);
HANDLE FindFirstFile(LPCWSTR pattern, WIN32_FIND_DATA* data);
BOOL FindNextFile(HANDLE h, WIN32_FIND_DATA* data);
BOOL FindClose(HANDLE h);
LPCWSTR PathFindExtensionW(LPCWSTR path);
HANDLE GetPropW(HWND hwnd, LPCWSTR name);
BOOL EnumWindows(WNDENUMPROC cb, LPARAM lparam);
HRESULT CLSIDFromString(LPCWSTR str, CLSID* clsid);
int lstrlenW(LPCWSTR s);

// --- MSVC CRT "secure" string functions (shim.cpp). On a too-small destination they return
// an error (as the CRT does once the invalid-parameter handler returns) instead of terminating.
int wcscpy_s(WCHAR* dst, size_t n, const WCHAR* src);
int wcscat_s(WCHAR* dst, size_t n, const WCHAR* src);
int strcpy_s(char* dst, size_t n, const char* src);
int strncpy_s(char* dst, size_t n, const char* src, size_t count);
template <size_t N>
inline int strncpy_s(char (&dst)[N], const char* src, size_t count)
{
    return strncpy_s(dst, N, src, count);
}
// MSVC swprintf_s: "%s" in a wide format means a WIDE string (glibc: narrow), so the shim
// rewrites %s to %ls before calling vswprintf.
int swprintf_s(WCHAR* buf, size_t n, const WCHAR* fmt, ...);

// --- COM ---
struct IUnknown {
    virtual HRESULT QueryInterface(REFIID riid, void** ppv) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
};

struct IWTSVirtualChannel : public IUnknown {
    virtual HRESULT Write(ULONG cbSize, BYTE* pBuffer, IUnknown* pReserved) = 0;
    virtual HRESULT Close() = 0;
};

struct IWTSVirtualChannelCallback : public IUnknown {
    virtual HRESULT OnDataReceived(ULONG cbSize, BYTE* pBuffer) = 0;
    virtual HRESULT OnClose() = 0;
};

// --- WRL subset ---
namespace Microsoft {
namespace WRL {

enum RuntimeClassType { WinRt = 1, ClassicCom = 2 };
template <unsigned int flags> struct RuntimeClassFlags {};

template <class Flags, class I>
class RuntimeClass : public I {
public:
    HRESULT QueryInterface(REFIID, void** ppv) override
    {
        if (ppv) *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG AddRef() override { return ++m_ref; }
    ULONG Release() override
    {
        ULONG r = --m_ref;
        if (r == 0) delete this;
        return r;
    }

protected:
    virtual ~RuntimeClass() {}

private:
    ULONG m_ref = 1;
};

template <class T>
class ComPtr {
public:
    ComPtr() : m_p(nullptr) {}
    ComPtr(decltype(nullptr)) : m_p(nullptr) {}
    ComPtr(const ComPtr&) = delete;
    ~ComPtr() { reset(); }
    ComPtr& operator=(T* p)
    {
        if (p) p->AddRef();
        reset();
        m_p = p;
        return *this;
    }
    ComPtr& operator=(decltype(nullptr))
    {
        reset();
        return *this;
    }
    T* Get() const { return m_p; }
    T* operator->() const { return m_p; }
    T** operator&()
    {
        reset();
        return &m_p;
    }

private:
    void reset()
    {
        if (m_p) {
            T* p = m_p;
            m_p = nullptr;
            p->Release();
        }
    }
    T* m_p;
};

template <class T, class I, class... Args>
HRESULT MakeAndInitialize(I** pp, Args&&... args)
{
    *pp = nullptr;
    T* obj = new T();
    HRESULT hr = obj->RuntimeClassInitialize(args...);
    if (FAILED(hr)) {
        obj->Release();
        return hr;
    }
    *pp = obj;
    return S_OK;
}

} // namespace WRL
} // namespace Microsoft

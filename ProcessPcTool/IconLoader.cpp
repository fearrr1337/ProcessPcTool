#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <unordered_map>
#include <vector>
#include <string>
#include "IconLoader.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")

struct CachedIcon {
    int width;
    int height;
    std::vector<unsigned char> pixels;
};

static std::unordered_map<std::string, CachedIcon> g_pixelsCache;


// системную иконку HICON в RGBA
static bool IconToPixels(HICON hIcon, int size, std::vector<unsigned char>& outPixels)
{
    HDC hdc = GetDC(NULL);

    BITMAPV5HEADER bi{};
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = size;
    bi.bV5Height = -size;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_RGB;

    void* bits = nullptr;

    HBITMAP hBitmap = CreateDIBSection(
        hdc,
        (BITMAPINFO*)&bi,
        DIB_RGB_COLORS,
        &bits,
        NULL,
        0);

    if (!hBitmap) {
        ReleaseDC(NULL, hdc);
        return false;
    }

    HDC mem = CreateCompatibleDC(hdc);
    SelectObject(mem, hBitmap);

    DrawIconEx(mem, 0, 0, hIcon, size, size, 0, NULL, DI_NORMAL);

    outPixels.resize(size * size * 4);

    unsigned char* src = (unsigned char*)bits;
    unsigned char* dst = outPixels.data();

    for (int i = 0; i < size * size; i++)
    {
        dst[i * 4 + 0] = src[i * 4 + 2];
        dst[i * 4 + 1] = src[i * 4 + 1];
        dst[i * 4 + 2] = src[i * 4 + 0];
        dst[i * 4 + 3] = src[i * 4 + 3];
    }

    DeleteDC(mem);
    DeleteObject(hBitmap);
    ReleaseDC(NULL, hdc);

    return true;
}

// получает иконку .exe с кэшированием пикселей RGBA
bool GetProcessIconPixels(const std::string& path, int& width, int& height, const unsigned char*& pixels, int size)
{
    std::string key = path + "#" + std::to_string(size);

    auto it = g_pixelsCache.find(key);
    if (it != g_pixelsCache.end()) {
        width = it->second.width;
        height = it->second.height;
        pixels = it->second.pixels.data();
        return true;
    }

    SHFILEINFOA sfi{};

    if (!SHGetFileInfoA(
        path.c_str(),
        FILE_ATTRIBUTE_NORMAL,
        &sfi,
        sizeof(sfi),
        SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
    {
        return GetDefaultExeIconPixels(width, height, pixels, size);
    }

    HICON hIcon = sfi.hIcon;
    if (!hIcon)
        return GetDefaultExeIconPixels(width, height, pixels, size);

    CachedIcon cached;
    cached.width = size;
    cached.height = size;

    if (!IconToPixels(hIcon, size, cached.pixels)) {
        DestroyIcon(hIcon);
        return GetDefaultExeIconPixels(width, height, pixels, size);
    }

    DestroyIcon(hIcon);

    width = cached.width;
    height = cached.height;

    pixels = cached.pixels.data();

    g_pixelsCache[key] = std::move(cached);

    return true;
}

// получает стандартную иконку .exe
bool GetDefaultExeIconPixels(int& width, int& height, const unsigned char*& pixels, int size)
{
    std::string key = "DEFAULT_EXE#" + std::to_string(size);

    auto it = g_pixelsCache.find(key);
    if (it != g_pixelsCache.end()) {
        width = it->second.width;
        height = it->second.height;
        pixels = it->second.pixels.data();
        return true;
    }

    SHFILEINFOA sfi{};

    if (!SHGetFileInfoA(
        ".exe",
        FILE_ATTRIBUTE_NORMAL,
        &sfi,
        sizeof(sfi),
        SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES))
    {
        return false;
    }

    HICON hIcon = sfi.hIcon;
    if (!hIcon) return false;

    CachedIcon cached;
    cached.width = size;
    cached.height = size;

    if (!IconToPixels(hIcon, size, cached.pixels)) {
        DestroyIcon(hIcon);
        return false;
    }

    DestroyIcon(hIcon);

    width = cached.width;
    height = cached.height;

    pixels = cached.pixels.data();

    g_pixelsCache[key] = std::move(cached);

    return true;
}


// чистит кэш
void CleanupIconCache()
{
    g_pixelsCache.clear();
}
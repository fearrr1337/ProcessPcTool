#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <unordered_map>
#include <vector>
#include <string>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "gdi32.lib")

struct CachedIcon {
    int width;
    int height;
    std::vector<unsigned char> pixels;
};

static std::unordered_map<std::string, CachedIcon> g_pixelsCache;

static bool IconToPixels(HICON hIcon, int targetSize, std::vector<unsigned char>& outPixels) {
    HDC hdcScreen = GetDC(NULL);
    BITMAPV5HEADER bi = { 0 };
    bi.bV5Size = sizeof(BITMAPV5HEADER);
    bi.bV5Width = targetSize;
    bi.bV5Height = -targetSize;
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_RGB;
    bi.bV5AlphaMask = 0xFF000000;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hBitmap) {
        ReleaseDC(NULL, hdcScreen);
        return false;
    }

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    SelectObject(hdcMem, hBitmap);
    DrawIconEx(hdcMem, 0, 0, hIcon, targetSize, targetSize, 0, NULL, DI_NORMAL);

    outPixels.resize(targetSize * targetSize * 4);
    unsigned char* src = (unsigned char*)bits;
    unsigned char* dst = outPixels.data();
    for (int i = 0; i < targetSize * targetSize; ++i) {
        dst[i * 4 + 0] = src[i * 4 + 2];
        dst[i * 4 + 1] = src[i * 4 + 1];
        dst[i * 4 + 2] = src[i * 4 + 0];
        dst[i * 4 + 3] = src[i * 4 + 3];
    }

    DeleteDC(hdcMem);
    DeleteObject(hBitmap);
    ReleaseDC(NULL, hdcScreen);
    return true;
}

bool GetProcessIconPixels(const std::string& path, int& width, int& height, const unsigned char*& pixels, int targetSize) {
    std::string cacheKey = path + "#" + std::to_string(targetSize);
    auto it = g_pixelsCache.find(cacheKey);
    if (it != g_pixelsCache.end()) {
        width = it->second.width;
        height = it->second.height;
        pixels = it->second.pixels.data();
        return true;
    }

    HICON hIcon = NULL;
    UINT num = ExtractIconExA(path.c_str(), 0, &hIcon, NULL, 1);
    if (num == 0 || hIcon == NULL) {
        SHFILEINFOA sfi;
        if (SHGetFileInfoA(path.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON)) {
            hIcon = sfi.hIcon;
        }
        else {
            return false;
        }
    }

    CachedIcon cached;
    cached.width = targetSize;
    cached.height = targetSize;
    if (!IconToPixels(hIcon, targetSize, cached.pixels)) {
        DestroyIcon(hIcon);
        return false;
    }

    DestroyIcon(hIcon);
    width = cached.width;
    height = cached.height;
    pixels = cached.pixels.data();
    g_pixelsCache[cacheKey] = std::move(cached);
    return true;
}

bool GetDefaultExeIconPixels(int& width, int& height, const unsigned char*& pixels, int targetSize) {
    std::string cacheKey = "DEFAULT_EXE#" + std::to_string(targetSize);
    auto it = g_pixelsCache.find(cacheKey);
    if (it != g_pixelsCache.end()) {
        width = it->second.width;
        height = it->second.height;
        pixels = it->second.pixels.data();
        return true;
    }

    SHFILEINFOA sfi = { 0 };
    if (!SHGetFileInfoA("*.exe", FILE_ATTRIBUTE_NORMAL, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES)) {
        return false;
    }
    HICON hIcon = sfi.hIcon;
    if (!hIcon) return false;

    CachedIcon cached;
    cached.width = targetSize;
    cached.height = targetSize;
    bool ok = IconToPixels(hIcon, targetSize, cached.pixels);
    DestroyIcon(hIcon);
    if (!ok) return false;

    width = cached.width;
    height = cached.height;
    pixels = cached.pixels.data();
    g_pixelsCache[cacheKey] = std::move(cached);
    return true;
}

void CleanupIconCache() {
    g_pixelsCache.clear();
}
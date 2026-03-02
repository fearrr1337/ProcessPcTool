#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "ProcessIcons.h"

std::vector<Image> g_processIcons;

// Вспомогательная функция: создает Image из HICON с заданным размером.
static Image ImageFromHICON(HICON hIcon, int width, int height) {
    Image image = { 0 };
    if (!hIcon) return image;

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    // Настройка DIB-секции (32 бита на пиксель, top-down)
    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // отрицательная высота для порядка строк сверху вниз
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hBitmap) {
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        return image;
    }

    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Заливка прозрачным чёрным фоном
    RECT rect = { 0, 0, width, height };
    HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdcMem, &rect, hBrush);
    DeleteObject(hBrush);

    // Рисуем иконку с масштабированием до нужного размера
    DrawIconEx(hdcMem, 0, 0, hIcon, width, height, 0, NULL, DI_NORMAL);

    // Пиксели в bits лежат в формате BGRA. Преобразуем в RGBA для Raylib.
    unsigned char* src = (unsigned char*)bits;
    unsigned char* dst = (unsigned char*)RL_MALLOC(width * height * 4 * sizeof(unsigned char));

    for (int i = 0; i < width * height; ++i) {
        dst[i * 4 + 0] = src[i * 4 + 2]; // R
        dst[i * 4 + 1] = src[i * 4 + 1]; // G
        dst[i * 4 + 2] = src[i * 4 + 0]; // B
        dst[i * 4 + 3] = src[i * 4 + 3]; // A
    }

    // Создаём структуру Image Raylib
    image.data = dst;
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    // Очистка GDI-ресурсов
    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return image;
}

Image LoadIconFromExe(const std::string& filePath, int iconSize) {
    Image result = { 0 };
    if (filePath.empty()) return result;

    // Конвертация пути в широкую строку
    int wlen = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, NULL, 0);
    if (wlen == 0) return result;
    std::wstring wpath(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, &wpath[0], wlen);
    wpath.pop_back(); // удаляем лишний нулевой символ

    // Попытка извлечения иконки через ExtractIconEx
    HICON hIconLarge = NULL, hIconSmall = NULL;
    UINT numIcons = ExtractIconExW(wpath.c_str(), 0, &hIconLarge, &hIconSmall, 1);
    HICON hIcon = NULL;
    if (numIcons > 0) {
        if (hIconLarge) hIcon = hIconLarge;
        else if (hIconSmall) hIcon = hIconSmall;
    }

    // Если не удалось, пробуем через SHGetFileInfo
    if (!hIcon) {
        SHFILEINFOW sfi = { 0 };
        if (SHGetFileInfoW(wpath.c_str(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON)) {
            hIcon = sfi.hIcon;
        }
    }

    if (hIcon) {
        result = ImageFromHICON(hIcon, iconSize, iconSize);
        DestroyIcon(hIcon);
    }

    return result;
}

void UpdateProcessIcons(const std::vector<std::string>& paths, int iconSize) {
    UnloadProcessIcons();                     // освобождаем старые иконки
    g_processIcons.reserve(paths.size());

    for (const auto& path : paths) {
        if (path.empty()) {
            g_processIcons.push_back({ 0 });   // пустое изображение
        }
        else {
            Image img = LoadIconFromExe(path, iconSize);
            g_processIcons.push_back(img);
        }
    }
}

void UnloadProcessIcons() {
    for (auto& icon : g_processIcons) {
        if (icon.data != nullptr) {
            UnloadImage(icon);                 // Raylib освобождает память
        }
    }
    g_processIcons.clear();
}
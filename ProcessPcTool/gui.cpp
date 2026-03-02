#define _CRT_SECURE_NO_WARNINGS
#include "gui.h"
#include "IconLoader.h"
#include <unordered_map>
#include <thread>

static std::unordered_map<std::string, Texture2D> g_textureCache;
static Texture2D g_defaultIconTexture = { 0 };

static void CreateDefaultIcon() {
    if (g_defaultIconTexture.id != 0) return;
    int width, height;
    const unsigned char* pixels = nullptr;
    if (!GetDefaultExeIconPixels(width, height, pixels, 16)) {
        const int size = 16;
        unsigned char* fallback = (unsigned char*)malloc(size * size * 4);
        for (int i = 0; i < size * size; ++i) {
            fallback[i * 4 + 0] = 192;
            fallback[i * 4 + 1] = 192;
            fallback[i * 4 + 2] = 192;
            fallback[i * 4 + 3] = 255;
        }
        Image image = { 0 };
        image.data = fallback;
        image.width = size;
        image.height = size;
        image.mipmaps = 1;
        image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
        g_defaultIconTexture = LoadTextureFromImage(image);
        free(fallback);
        return;
    }

    Image image = { 0 };
    image.data = const_cast<unsigned char*>(pixels);
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    g_defaultIconTexture = LoadTextureFromImage(image);
}

static void CleanupTextureCache() {
    for (auto& pair : g_textureCache) {
        UnloadTexture(pair.second);
    }
    g_textureCache.clear();
    if (g_defaultIconTexture.id != 0) {
        UnloadTexture(g_defaultIconTexture);
        g_defaultIconTexture.id = 0;
    }
}

static Texture2D GetProcessTexture(const ProcessInfo& proc) {
    const std::string& path = proc.imagePath;
    if (path.empty()) {
        CreateDefaultIcon();
        return g_defaultIconTexture;
    }

    auto it = g_textureCache.find(path);
    if (it != g_textureCache.end()) return it->second;

    int width, height;
    const unsigned char* pixels = nullptr;
    if (!GetProcessIconPixels(path, width, height, pixels, 16)) {
        CreateDefaultIcon();
        return g_defaultIconTexture;
    }

    Image image = { 0 };
    image.data = const_cast<unsigned char*>(pixels);
    image.width = width;
    image.height = height;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;

    Texture2D texture = LoadTextureFromImage(image);
    if (texture.id != 0) {
        g_textureCache[path] = texture;
    }
    else {
        CreateDefaultIcon();
        texture = g_defaultIconTexture;
    }
    return texture;
}

static void PreloadIconsAsync() {
    for (const auto& proc : g_processes) {
        if (!proc.imagePath.empty()) {
            GetProcessTexture(proc);
        }
    }
}

int screenWidth = 1920;
int screenHeight = 1080;

void mainWindow() {
    // Создаём окно с возможностью изменения размера
    InitWindow(1280, 720, "ProcessMonitor");
    SetWindowState(FLAG_WINDOW_RESIZABLE);  // разрешаем растягивание мышкой

    InitPerformanceCounters();
    ListProcesses();
    CollectProcessPaths();

    // Загружаем иконки в фоне
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawText("Loading icons...", 10, 30, 20, DARKGRAY);
    EndDrawing();

    std::thread preloadThread(PreloadIconsAsync);
    preloadThread.detach();

    SetTargetFPS(60);

    // Переменные для вкладок
    int currentTab = 0;              // 0 - процессы, 1 - вторая страница
    const int tabCount = 2;
    const char* tabNames[2] = { "Processes", "Second" };
    const int tabHeight = 30;
    const int tabWidth = 100;

    const int itemHeight = 20;       // высота строки таблицы
    int scrollOffset = 0;             // смещение скролла

    while (!WindowShouldClose()) {
        // Получаем текущие размеры окна
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // Обновление данных процессов раз в секунду
        static double lastUpdate = GetTime();
        if (GetTime() - lastUpdate >= 1.0) {
            UpdateProcessesStats();
            std::sort(g_processes.begin(), g_processes.end(),
                [](const ProcessInfo& a, const ProcessInfo& b) {
                    return a.cpuUsage > b.cpuUsage;
                });
            lastUpdate = GetTime();
        }

        // Обработка ввода мыши для вкладок
        Vector2 mousePoint = GetMousePosition();
        bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        if (mousePressed) {
            // Проверяем клик по вкладкам
            for (int i = 0; i < tabCount; i++) {
                Rectangle tabRect = { 10.0f + i * (tabWidth + 5), 40.0f, (float)tabWidth, (float)tabHeight };
                if (CheckCollisionPointRec(mousePoint, tabRect)) {
                    currentTab = i;
                    // Сбрасываем скролл при смене вкладки (опционально)
                    scrollOffset = 0;
                }
            }
        }

        // Обработка скролла колёсиком только для вкладки процессов
        if (currentTab == 0) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                // Вычисляем область таблицы, чтобы скроллить только когда мышь внутри неё
                Rectangle listRect = {
                    10.0f,
                    40.0f + tabHeight + 10,
                    (float)screenWidth - 20,
                    (float)screenHeight - (40.0f + tabHeight + 10 + 10)  // отступ снизу
                };
                if (CheckCollisionPointRec(mousePoint, listRect)) {
                    scrollOffset -= static_cast<int>(wheel * itemHeight * 3);
                    if (scrollOffset < 0) scrollOffset = 0;
                    int maxScroll = static_cast<int>(g_processes.size()) * itemHeight
                                    - static_cast<int>(listRect.height - 20); // высота заголовка таблицы ~20
                    if (maxScroll < 0) maxScroll = 0;
                    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
                }
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        // Заголовок окна
        DrawText("Process Monitor", 10, 10, 20, DARKGRAY);

        // Отрисовка вкладок
        for (int i = 0; i < tabCount; i++) {
            Rectangle tabRect = { 10.0f + i * (tabWidth + 5), 40.0f, (float)tabWidth, (float)tabHeight };
            Color bgColor = (i == currentTab) ? WHITE : LIGHTGRAY;
            DrawRectangleRec(tabRect, bgColor);
            DrawRectangleLinesEx(tabRect, 1, GRAY);
            // Центрируем текст вкладки
            int textWidth = MeasureText(tabNames[i], 10);
            DrawText(tabNames[i],
                     tabRect.x + (tabRect.width - textWidth) / 2,
                     tabRect.y + (tabRect.height - 10) / 2,
                     10, BLACK);
        }

        // Отображаем содержимое в зависимости от выбранной вкладки
        if (currentTab == 0) {
            // Вкладка процессов
            // Область таблицы: отступы 10px со всех сторон, учитывая заголовок и вкладки
            Rectangle listRect = {
                10.0f,
                40.0f + tabHeight + 10,
                (float)screenWidth - 20,
                (float)screenHeight - (40.0f + tabHeight + 10 + 10)  // нижний отступ 10px
            };

            // Заголовок таблицы
            Rectangle headerRect = { listRect.x, listRect.y, listRect.width, 20 };
            DrawRectangleRec(headerRect, LIGHTGRAY);
            DrawRectangleLinesEx(headerRect, 1, GRAY);

            // Рассчитываем позиции колонок в зависимости от ширины окна
            const int colIconW = 20;
            const int colPidW = 50;
            const int colCpuW = 70;
            const int colMemW = 90;
            int colNameW = listRect.width - (colIconW + colPidW + colCpuW + colMemW + 20); // +20 на отступы

            int colIconX = listRect.x + 5;
            int colPidX = colIconX + colIconW + 5;
            int colNameX = colPidX + colPidW + 5;
            int colCpuX = colNameX + colNameW + 5;
            int colMemX = colCpuX + colCpuW + 5;

            // Текст заголовков
            DrawText("PID", colPidX, listRect.y + 4, 10, DARKGRAY);
            DrawText("Name", colNameX, listRect.y + 4, 10, DARKGRAY);
            DrawText("CPU%", colCpuX, listRect.y + 4, 10, DARKGRAY);
            DrawText("Memory (KB)", colMemX, listRect.y + 4, 10, DARKGRAY);

            // Область прокрутки списка процессов
            BeginScissorMode(listRect.x, listRect.y + 20,
                             listRect.width, listRect.height - 20);

            int yPos = listRect.y + 20 - scrollOffset;
            for (const auto& p : g_processes) {
                if (yPos + itemHeight > listRect.y + 20 &&
                    yPos < listRect.y + listRect.height) {

                    // Иконка процесса
                    Texture2D tex = GetProcessTexture(p);
                    if (tex.id != 0) {
                        DrawTexture(tex, colIconX, yPos + (itemHeight - tex.height) / 2, WHITE);
                    }

                    // PID
                    char pidStr[16];
                    sprintf(pidStr, "%5d", p.pid);
                    DrawText(pidStr, colPidX, yPos + 2, 10, BLACK);

                    // Имя процесса (обрезаем, если слишком длинное)
                    std::string nameStr(p.name.begin(), p.name.end());
                    if (MeasureText(nameStr.c_str(), 10) > colNameW) {
                        // Простое обрезание по символам (можно улучшить)
                        while (!nameStr.empty() && MeasureText(nameStr.c_str(), 10) > colNameW - 10) {
                            nameStr.pop_back();
                        }
                        nameStr += "...";
                    }
                    DrawText(nameStr.c_str(), colNameX, yPos + 2, 10, BLACK);

                    // CPU
                    char cpuStr[16];
                    sprintf(cpuStr, "%5.1f%%", p.cpuUsage);
                    DrawText(cpuStr, colCpuX, yPos + 2, 10, BLACK);

                    // Память
                    char memStr[32];
                    sprintf(memStr, "%7.0f", p.workingSetSize / 1024.0);
                    DrawText(memStr, colMemX, yPos + 2, 10, BLACK);
                }
                yPos += itemHeight;
            }

            EndScissorMode();

            // Дополнительно: полоса прокрутки (опционально)
            // Можно нарисовать простой индикатор
            int totalHeight = g_processes.size() * itemHeight;
            int visibleHeight = listRect.height - 20;
            if (totalHeight > visibleHeight) {
                float thumbHeight = (float)visibleHeight / totalHeight * visibleHeight;
                float thumbPos = (float)scrollOffset / totalHeight * visibleHeight;
                Rectangle scrollBar = {
                    listRect.x + listRect.width - 8,
                    listRect.y + 20 + thumbPos,
                    6,
                    thumbHeight
                };
                DrawRectangleRec(scrollBar, GRAY);
            }
        } else {
            // Вторая вкладка (пустая)
            DrawText("Second Tab - Empty", 20, 100, 20, DARKGRAY);
        }

        EndDrawing();
    }

    // Очистка ресурсов
    CleanupTextureCache();
    CleanupIconCache();
    ClosePerformanceCounters();
    CloseWindow();
}
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI

#include "gui.h"

int screenWidth = 1920;
int screenHeight = 1080;

void mainWindow() {
    InitWindow(screenWidth, screenHeight, "ProcessMonitor");
    InitPerformanceCounters();
    ListProcesses();
    SetTargetFPS(60);

    const int itemHeight = 20;           // высота строки процесса
    const int headerHeight = 20;          // высота строки заголовков
    Rectangle listRect = { 10, 40, 780, 500 };
    int scrollOffset = 0;

    // Координаты колонок (подобраны для оптимального использования ширины)
    const int colPidX = listRect.x + 5;
    const int colNameX = colPidX + 50;   // ширина PID
    const int colCpuX = colNameX + 250; // ширина Name
    const int colMemX = colCpuX + 80;   // ширина CPU
    // Оставшееся место (до listRect.x + listRect.width) используется для колонки Memory

    while (!WindowShouldClose()) {
        static double lastUpdate = GetTime();
        if (GetTime() - lastUpdate >= 1.0) {
            UpdateProcessesStats();

            // Сортировка по убыванию загрузки CPU
            std::sort(g_processes.begin(), g_processes.end(),
                [](const ProcessInfo& a, const ProcessInfo& b) {
                    return a.cpuUsage > b.cpuUsage;
                });

            lastUpdate = GetTime();
        }

        // Обработка прокрутки колёсиком мыши
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            scrollOffset -= static_cast<int>(wheel * itemHeight * 3);
            if (scrollOffset < 0) scrollOffset = 0;
            int maxScroll = static_cast<int>(g_processes.size()) * itemHeight
                - static_cast<int>(listRect.height - headerHeight);
            if (maxScroll < 0) maxScroll = 0;
            if (scrollOffset > maxScroll) scrollOffset = maxScroll;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Process Monitor", 10, 10, 20, DARKGRAY);
        DrawRectangleLines(listRect.x, listRect.y, listRect.width, listRect.height, GRAY);

        // --- Заголовки (всегда видимы) ---
        Rectangle headerRect = { listRect.x, listRect.y, listRect.width, headerHeight };
        DrawRectangleRec(headerRect, LIGHTGRAY);
        DrawRectangleLines(headerRect.x, headerRect.y, headerRect.width, headerRect.height, GRAY);

        DrawText("PID", colPidX, listRect.y + 2, 10, DARKGRAY);
        DrawText("Name", colNameX, listRect.y + 2, 10, DARKGRAY);
        DrawText("CPU%", colCpuX, listRect.y + 2, 10, DARKGRAY);
        DrawText("Memory (KB)", colMemX, listRect.y + 2, 10, DARKGRAY);

        // --- Список процессов с прокруткой ---
        BeginScissorMode(listRect.x, listRect.y + headerHeight,
            listRect.width, listRect.height - headerHeight);

        int yPos = listRect.y + headerHeight - scrollOffset;
        for (size_t i = 0; i < g_processes.size(); ++i) {
            if (yPos + itemHeight > listRect.y + headerHeight &&
                yPos < listRect.y + listRect.height) {

                const auto& p = g_processes[i];

                // Обрезаем слишком длинные имена
                std::string nameStr(p.name.begin(), p.name.end());
                if (nameStr.length() > 30) nameStr = nameStr.substr(0, 27) + "...";

                // PID
                char pidStr[16];
                sprintf(pidStr, "%5d", p.pid);
                DrawText(pidStr, colPidX, yPos + 2, 10, BLACK);

                // Name
                DrawText(nameStr.c_str(), colNameX, yPos + 2, 10, BLACK);

                // CPU
                char cpuStr[16];
                sprintf(cpuStr, "%5.1f%%", p.cpuUsage);
                DrawText(cpuStr, colCpuX, yPos + 2, 10, BLACK);

                // Memory
                char memStr[32];
                sprintf(memStr, "%7.0f", p.workingSetSize / 1024.0);
                DrawText(memStr, colMemX, yPos + 2, 10, BLACK);
            }
            yPos += itemHeight;
        }

        EndScissorMode();
        EndDrawing();
    }

    ClosePerformanceCounters();
    CloseWindow();
}
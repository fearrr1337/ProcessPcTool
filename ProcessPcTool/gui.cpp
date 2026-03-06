#define _CRT_SECURE_NO_WARNINGS
#include "gui.h"
#include "IconLoader.h"
#include "ProcessInfo.h"
#include <unordered_map>
#include <thread>
#include <cstdio>   
#include "SystemInfoWin.h"

static std::unordered_map<std::string, Texture2D> g_textureCache;
static Texture2D g_defaultIconTexture = { 0 };

static double g_cachedSystemCpu = 0.0;


// дефолтная иконка в текстуру
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


// чистит кэш
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

// возвращает текстуру процесса
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

// форматирование б/с в другие виды
static void FormatSpeed(char* buffer, size_t size, double bytesPerSec) {
    if (bytesPerSec < 1024.0)
        snprintf(buffer, size, "%.0f B/s", bytesPerSec);
    else if (bytesPerSec < 1024.0 * 1024.0)
        snprintf(buffer, size, "%.1f KB/s", bytesPerSec / 1024.0);
    else if (bytesPerSec < 1024.0 * 1024.0 * 1024.0)
        snprintf(buffer, size, "%.1f MB/s", bytesPerSec / (1024.0 * 1024.0));
    else
        snprintf(buffer, size, "%.2f GB/s", bytesPerSec / (1024.0 * 1024.0 * 1024.0));
}

void mainWindow() {
    InitWindow(1600, 800, "ProcessMonitor");
    SetWindowState(FLAG_WINDOW_RESIZABLE);
    Texture2D LoadingIcon = LoadTexture("icon.png");
    Font gFont = LoadFontEx("C:\\Windows\\Fonts\\arial.ttf", 20, 0, 0);
    Image iconImg = LoadImage("icon.png");
    SetWindowIcon(iconImg);
    InitPerformanceCounters();
    ListProcesses();
    CollectProcessPaths();


    // Предварительная загрузка иконок и процессов
    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawTexturePro(LoadingIcon,
        { 0, 0, (float)LoadingIcon.width, (float)LoadingIcon.height },
        { 1600 / 2 - 128 / 2, 800 / 2 - 128 / 2, 128, 128 },
        { 0, 0 },
        0.0f,
        WHITE);
    EndDrawing();
    static SystemInfoData sysInfo = GetSystemInfoData();


    SetTargetFPS(60);

    int currentTab = 0;
    const int tabCount = 2;
    const char* tabNames[2] = { "Processes", "PC Info" };
    const int tabHeight = 40;
    const int tabWidth = 120;
    const int itemHeight = 30;
    int scrollOffset = 0;

    int rightClickedIndex = -1;
    bool showContextMenu = false;
    Rectangle contextMenuRect = { 0 };

    while (!WindowShouldClose()) {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();
        

        // обновление статистики

        static bool pauseProcess = false;
        static std::vector<ProcessInfo> snapshotProcesses;
        static double lastUpdate = GetTime();

        if (currentTab == 0 && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) { 
            pauseProcess = !pauseProcess;

            if (pauseProcess) {
                snapshotProcesses = g_processes;
            }
        }

        if (!pauseProcess && (GetTime() - lastUpdate >= 1.0)) {
            g_cachedSystemCpu = GetSystemCpuUsage();
            UpdateProcessesStats();
            std::sort(g_processes.begin(), g_processes.end(),
                [](const ProcessInfo& a, const ProcessInfo& b) { return a.cpuUsage > b.cpuUsage; });
            lastUpdate = GetTime();
        }

        if (pauseProcess) {
            DrawText("PAUSED (Press RMB to resume)", 300, 15, 20, RED);
            g_processes = snapshotProcesses;
        }
        else {
            DrawText("RUNNING (Press RMB to pause)", 300, 15, 20, GREEN);
        }

        // системные метрики
        double sysCpu = g_cachedSystemCpu;
        DWORDLONG totalPhys, availPhys;
        GetSystemMemoryInfo(totalPhys, availPhys);
        double diskRead = GetDiskReadSpeed();
        double diskWrite = GetDiskWriteSpeed();
        double netUpload = GetNetworkUploadSpeed();
        double netDownload = GetNetworkDownloadSpeed();

        Vector2 mousePoint = GetMousePosition();
        bool mousePressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
        bool mouseRightPressed = IsMouseButtonPressed(MOUSE_RIGHT_BUTTON);

        // переключение вкладок при клике
        if (mousePressed) {
            for (int i = 0; i < tabCount; i++) {
                Rectangle tabRect = { 10 + i * (tabWidth + 5), 5, (float)tabWidth, (float)tabHeight };
                if (CheckCollisionPointRec(mousePoint, tabRect)) {
                    currentTab = i;
                    scrollOffset = 0;
                }
            }
        }

        // прокрутка списка процессов
        if (currentTab == 0) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                Rectangle listRect = { 10, 40 + tabHeight + 10 + 60, (float)screenWidth - 20, (float)screenHeight - (40 + tabHeight + 10 + 60 + 10) };
                if (CheckCollisionPointRec(mousePoint, listRect)) {
                    scrollOffset -= (int)(wheel * itemHeight * 3);
                    if (scrollOffset < 0) scrollOffset = 0;
                    int maxScroll = (int)g_processes.size() * itemHeight - (int)(listRect.height - 20);
                    if (maxScroll < 0) maxScroll = 0;
                    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
                }
            }
        }

        // меню процесса(закрыть процесс) при пкм
        if (currentTab == 0 && mouseRightPressed) {
            Rectangle listRect = { 10, 40 + tabHeight + 10 + 60, (float)screenWidth - 20, (float)screenHeight - (40 + tabHeight + 10 + 60 + 10) };
            if (CheckCollisionPointRec(mousePoint, listRect)) {
                int y = (int)mousePoint.y - (int)(listRect.y + 20) + scrollOffset;
                int index = y / itemHeight;
                if (index >= 0 && index < (int)g_processes.size()) {
                    rightClickedIndex = index;
                    showContextMenu = true;
                    contextMenuRect = { mousePoint.x, mousePoint.y, 140, 35 };
                }
            }
        }

        
        // закрытие процесса при нажатии кнопки
        if (showContextMenu && mousePressed) {
            if (CheckCollisionPointRec(mousePoint, contextMenuRect)) {
                if (rightClickedIndex >= 0 && rightClickedIndex < (int)g_processes.size()) {
                    DWORD pid = g_processes[rightClickedIndex].pid;
                    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProcess) {
                        TerminateProcess(hProcess, 1);
                        auto it = std::remove_if(g_processes.begin(), g_processes.end(),
                            [pid](const ProcessInfo& proc) { return proc.pid == pid; });
                        g_processes.erase(it, g_processes.end());
                        CloseHandle(hProcess);
                    }
                }
            }
            showContextMenu = false;
            rightClickedIndex = -1;
        }


        BeginDrawing();
        ClearBackground(RAYWHITE);

        // 2 вкладки
        for (int i = 0; i < tabCount; i++) {
            Rectangle tabRect = { 10 + i * (tabWidth + 5), 5, (float)tabWidth, (float)tabHeight };
            Color bgColor = (i == currentTab) ? WHITE : LIGHTGRAY;
            DrawRectangleRec(tabRect, bgColor);
            DrawRectangleLinesEx(tabRect, 2, GRAY);
            int textW = MeasureTextEx(gFont, tabNames[i], 20, 2).x;
            DrawTextEx(gFont, tabNames[i], { tabRect.x + (tabRect.width - textW) / 2, tabRect.y + (tabRect.height - 20) / 2 }, 20, 2, BLACK);
        }

        if (currentTab == 0) {
            Rectangle summaryRect = { 10, 5 + tabHeight + 10, (float)screenWidth - 20, 60 };
            DrawRectangleRec(summaryRect, LIGHTGRAY);
            DrawRectangleLinesEx(summaryRect, 2, GRAY);

            char buffer[128];
            int yPos = summaryRect.y + 20;
            snprintf(buffer, sizeof(buffer), "CPU: %.1f%%", sysCpu);
            DrawTextEx(gFont, buffer, { (float)summaryRect.x + 5, (float)yPos }, 20, 2, BLACK);

            double memUsedPercent = (totalPhys - availPhys) * 100.0 / totalPhys;
            snprintf(buffer, sizeof(buffer), "Memory: %.1f%% (%.1f/%.1f GB)", memUsedPercent, (totalPhys - availPhys) / 1024.0 / 1024.0 / 1024.0, totalPhys / 1024.0 / 1024.0 / 1024.0);
            DrawTextEx(gFont, buffer, { (float)summaryRect.x + 220, (float)yPos }, 20, 2, BLACK);

            // Таблица процессов
            Rectangle listRect = { 10, summaryRect.y + summaryRect.height + 5, (float)screenWidth - 20, (float)screenHeight - (summaryRect.y + summaryRect.height + 5 + 10) };

            const int colIconW = 30, colPidW = 60, colCpuW = 80, colMemW = 100, colDiskRW = 150, colNetW = 120;
            int colNameW = listRect.width - (colIconW + colPidW + colCpuW + colMemW + colDiskRW + colNetW + 40);
            int colIconX = listRect.x + 5, colPidX = colIconX + colIconW + 5, colNameX = colPidX + colPidW + 5;
            int colCpuX = colNameX + colNameW + 5, colMemX = colCpuX + colCpuW + 5, colDiskX = colMemX + colMemW + 5, colNetX = colDiskX + colDiskRW + 5;

            DrawTextEx(gFont, "PID", { (float)colPidX,listRect.y + 4 }, 18, 1, DARKGRAY);
            DrawTextEx(gFont, "Name", { (float)colNameX,listRect.y + 4 }, 18, 1, DARKGRAY);
            DrawTextEx(gFont, "CPU%", { (float)colCpuX,listRect.y + 4 }, 18, 1, DARKGRAY);
            DrawTextEx(gFont, "Memory (KB)", { (float)colMemX,listRect.y + 4 }, 18, 1, DARKGRAY);
            DrawTextEx(gFont, "Disk R/W (B/s)", { (float)colDiskX,listRect.y + 4 }, 18, 1, DARKGRAY);
            DrawTextEx(gFont, "Network (B/s)", { (float)colNetX,listRect.y + 4 }, 18, 1, DARKGRAY);

            BeginScissorMode(listRect.x, listRect.y + 20, listRect.width, listRect.height - 20);
            int yDraw = listRect.y + 20 - scrollOffset;
            for (size_t i = 0; i < g_processes.size(); ++i) {
                const auto& p = g_processes[i];
                if (yDraw + itemHeight > listRect.y + 20 && yDraw < listRect.y + listRect.height) {
                    // Подсветка строки
                    if (mousePoint.y > yDraw && mousePoint.y < yDraw + itemHeight)
                        DrawRectangleRec({ listRect.x,(float)yDraw,listRect.width,(float)itemHeight }, Fade(LIGHTGRAY, 0.3f));

                    // Иконка
                    Texture2D tex = GetProcessTexture(p);
                    if (tex.id != 0) DrawTexture(tex, colIconX, yDraw + (itemHeight - tex.height) / 2, WHITE);

                    // PID
                    snprintf(buffer, sizeof(buffer), "%5d", p.pid);
                    DrawTextEx(gFont, buffer, { (float)colPidX,(float)yDraw + 4 }, 18, 1, BLACK);

                    // Name
                    std::string nameStr(p.name.begin(), p.name.end());
                    if (MeasureTextEx(gFont, nameStr.c_str(), 18, 1).x > colNameW) {
                        while (!nameStr.empty() && MeasureTextEx(gFont, nameStr.c_str(), 18, 1).x > colNameW - 10) nameStr.pop_back();
                        nameStr += "...";
                    }
                    DrawTextEx(gFont, nameStr.c_str(), { (float)colNameX,(float)yDraw + 4 }, 18, 1, BLACK);

                    // CPU
                    snprintf(buffer, sizeof(buffer), "%5.1f%%", p.cpuUsage);
                    DrawTextEx(gFont, buffer, { (float)colCpuX,(float)yDraw + 4 }, 18, 1, BLACK);

                    // Memory
                    snprintf(buffer, sizeof(buffer), "%7.0f", p.workingSetSize / 1024.0);
                    DrawTextEx(gFont, buffer, { (float)colMemX,(float)yDraw + 4 }, 18, 1, BLACK);

                    // Disk
                    char diskReadSpd[16], diskWriteSpd[16];
                    FormatSpeed(diskReadSpd, sizeof(diskReadSpd), p.ioReadSpeed);
                    FormatSpeed(diskWriteSpd, sizeof(diskWriteSpd), p.ioWriteSpeed);
                    snprintf(buffer, sizeof(buffer), "%s / %s", diskReadSpd, diskWriteSpd);
                    DrawTextEx(gFont, buffer, { (float)colDiskX,(float)yDraw + 4 }, 18, 1, BLACK);

                    // Network
                    char netSpd[16];
                    FormatSpeed(netSpd, sizeof(netSpd), p.ioNetworkSpeed);
                    DrawTextEx(gFont, netSpd, { (float)colNetX,(float)yDraw + 4 }, 18, 1, BLACK);
                }
                yDraw += itemHeight;
            }
            EndScissorMode();

            // Полоса прокрутки
            int totalHeight = (int)g_processes.size() * itemHeight;
            int visibleHeight = (int)(listRect.height - 20);
            if (totalHeight > visibleHeight) {
                float thumbHeight = (float)visibleHeight / totalHeight * visibleHeight;
                float thumbPos = (float)scrollOffset / totalHeight * visibleHeight;
                DrawRectangleRec({ listRect.x + listRect.width - 8,listRect.y + 20 + thumbPos,6,thumbHeight }, DARKGRAY);
            }

            // Контекстное меню
            if (showContextMenu) {
                DrawRectangleRec(contextMenuRect, DARKBLUE);
                DrawRectangleLinesEx(contextMenuRect, 2, BLUE);
                int textW = MeasureTextEx(gFont, "Terminate", 16, 1).x;
                DrawTextEx(gFont, "Terminate", { contextMenuRect.x + (contextMenuRect.width - textW) / 2,contextMenuRect.y + (contextMenuRect.height - 16) / 2 }, 16, 1, WHITE);
            }
        }
        else {
            int y = 60;
            char buffer[256];

            snprintf(buffer, sizeof(buffer), "Processor: %s", sysInfo.processorName.c_str());
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            snprintf(buffer, sizeof(buffer), "Cores: %u", sysInfo.cores);
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            snprintf(buffer, sizeof(buffer), "Logical Processors: %u", sysInfo.logicalProcessors);
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            snprintf(buffer, sizeof(buffer), "Memory: %.1f / %.1f GB", sysInfo.usedMemoryGB, sysInfo.totalMemoryGB);
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            snprintf(buffer, sizeof(buffer), "GPU: %s", sysInfo.gpuName.c_str());
            DrawTextEx(gFont, buffer, { 20, (float)y }, 20, 2, BLACK); y += 30;

            char computerName[256];
            DWORD compSize = sizeof(computerName);
            GetComputerNameA(computerName, &compSize);

            snprintf(buffer, sizeof(buffer), "Computer Name: %s", computerName);
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            char username[256];
            DWORD userSize = sizeof(username);
            GetUserNameA(username, &userSize);

            snprintf(buffer, sizeof(buffer), "User: %s", username);
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            ULONGLONG uptime = GetTickCount64() / 1000;

            snprintf(buffer, sizeof(buffer), "System Uptime: %llu sec", uptime);
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            SYSTEM_INFO si;
            GetSystemInfo(&si);

            snprintf(buffer, sizeof(buffer), "CPU Architecture: %u", si.wProcessorArchitecture);
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            snprintf(buffer, sizeof(buffer), "Page Size: %u", si.dwPageSize);
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

            snprintf(buffer, sizeof(buffer), "Processes Running: %zu", g_processes.size());
            DrawTextEx(gFont, buffer, { 20,(float)y }, 20, 2, BLACK); y += 30;

        }

        EndDrawing();
    }


    UnloadFont(gFont);
    CleanupTextureCache();
    CleanupIconCache();
    ClosePerformanceCounters();
    CloseWindow();
}

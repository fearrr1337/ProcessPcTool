#pragma once

// Отключаем конфликтующие части Windows API
#define WIN32_LEAN_AND_MEAN   // минимизируем включения
#define NOMINMAX              // отключаем макросы min/max
#define NOUSER                // исключаем функции USER (CloseWindow, ShowCursor и др.)
#define NOGDI                  // исключаем GDI (DrawText и т.д.)

// Теперь безопасно включаем windows.h
#include <windows.h>
#include <tlhelp32.h>
#include <pdh.h>
#include <psapi.h>

// Стандартные библиотеки
#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

// Ваши объявления (как и раньше)
struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    double cpuUsage;
    SIZE_T workingSetSize;
    SIZE_T privateUsage;
    ULONGLONG ioReadBytesDelta;
    ULONGLONG ioWriteBytesDelta;
    ULONGLONG prevKernelTime;
    ULONGLONG prevUserTime;
    ULONGLONG prevIoRead;
    ULONGLONG prevIoWrite;
    ULONGLONG prevTimeStamp;
};

extern std::vector<ProcessInfo> g_processes;

void ListProcesses();
void UpdateProcessesStats();
double GetSystemCpuUsage();
void GetSystemMemoryInfo(DWORDLONG& totalPhys, DWORDLONG& availPhys);
double GetDiskReadSpeed();
double GetDiskWriteSpeed();
double GetNetworkUploadSpeed();
double GetNetworkDownloadSpeed();
void InitPerformanceCounters();
void ClosePerformanceCounters();
#pragma once

// Отключаем конфликтующие части Windows API
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOUSER
#define NOGDI

#include <windows.h>
#include <tlhelp32.h>
#include <pdh.h>
#include <psapi.h>

#include <iostream>
#include <string>
#include <vector>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    std::string imagePath;      // полный путь к исполняемому файлу
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
void CollectProcessPaths();   // заполняет imagePath для всех процессов
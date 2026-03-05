#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOUSER
#define NOGDI

#include <windows.h>
#include <tlhelp32.h>
#include <pdh.h>
#include <psapi.h>

#include <string>
#include <vector>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

struct ProcessInfo {
    DWORD pid = 0; // PID
    std::wstring name; // название процесса
    std::string imagePath; // путь до картинки

    double cpuUsage = 0.0; // нагрузка на процессор

    ULONGLONG prevKernelTime = 0; // предыдущее суммарное время работы процесса в режиме ядра
    ULONGLONG prevUserTime = 0; // предыдущее суммарное время работы процесса в пользовательском режиме
    ULONGLONG prevTimeStamp = 0; // момент времени предыдущего замера (временная метка)

    SIZE_T workingSetSize = 0; // Сколько RAM занято процессом

    ULONGLONG prevIoRead = 0; // прошлое суммарное количество прочитанных байт
    ULONGLONG prevIoWrite = 0; // сколько записал
    ULONGLONG prevIoOther = 0; // предыдущее значение счётчика прочих операций ввода-вывода (I/O)

    double ioReadSpeed = 0.0; // скорость чтения
    double ioWriteSpeed = 0.0; // скорость записи
    double ioNetworkSpeed = 0.0; // сеть
};

extern std::vector<ProcessInfo> g_processes;

void ListProcesses();
void UpdateProcessesStats();
void CollectProcessPaths();

double GetSystemCpuUsage();
void GetSystemMemoryInfo(DWORDLONG& totalPhys, DWORDLONG& availPhys);

double GetDiskReadSpeed();
double GetDiskWriteSpeed();
double GetNetworkUploadSpeed();
double GetNetworkDownloadSpeed();

void InitPerformanceCounters();
void ClosePerformanceCounters();

ULONGLONG FileTimeToUll(const FILETIME& ft);
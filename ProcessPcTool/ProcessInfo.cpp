#include "ProcessInfo.h"

std::vector<ProcessInfo> g_processes;

// преобразует FILETIME в 64-битное число
ULONGLONG FileTimeToUll(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

// Заполяет g_processes процессами
void ListProcesses() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    g_processes.clear();

    if (Process32FirstW(snap, &pe)) {
        do {
            ProcessInfo pi;
            pi.pid = pe.th32ProcessID;
            pi.name = pe.szExeFile;
            g_processes.push_back(pi);
        } while (Process32NextW(snap, &pe));
    }

    CloseHandle(snap);
}


// Обновляет процессы
void UpdateProcessesStats() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULONGLONG now = FileTimeToUll(ft);

    // Кол-во ядер процессора
    static DWORD cpuCount = [] {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwNumberOfProcessors;
        }();

    for (auto& pi : g_processes) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
        if (!hProcess) continue;

        FILETIME create, exit, kernel, user;
        ULONGLONG k = 0, u = 0;

        if (GetProcessTimes(hProcess, &create, &exit, &kernel, &user)) {
            k = FileTimeToUll(kernel);
            u = FileTimeToUll(user);
        }

        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            pi.workingSetSize = pmc.WorkingSetSize;
        }

        IO_COUNTERS io;
        ULONGLONG read = 0, write = 0, other = 0;
        if (GetProcessIoCounters(hProcess, &io)) {
            read = io.ReadTransferCount;
            write = io.WriteTransferCount;
            other = io.OtherTransferCount;
        }

        CloseHandle(hProcess);

        if (pi.prevTimeStamp != 0) {
            double seconds = (now - pi.prevTimeStamp) * 1e-7;
            if (seconds > 0) {
                ULONGLONG cpuDelta = (k - pi.prevKernelTime) + (u - pi.prevUserTime);
                pi.cpuUsage = (cpuDelta * 100.0) / ((now - pi.prevTimeStamp) * cpuCount);
                if (pi.cpuUsage > 100.0) pi.cpuUsage = 100.0;

                pi.ioReadSpeed = (read - pi.prevIoRead) / seconds;
                pi.ioWriteSpeed = (write - pi.prevIoWrite) / seconds;
                pi.ioNetworkSpeed = (other - pi.prevIoOther) / seconds;
            }
        }

        pi.prevKernelTime = k;
        pi.prevUserTime = u;
        pi.prevIoRead = read;
        pi.prevIoWrite = write;
        pi.prevIoOther = other;
        pi.prevTimeStamp = now;
    }
}


// Дескрипторы для мониторинга дисковой\сетевой активности
static PDH_HQUERY g_query = nullptr;
static PDH_HCOUNTER g_diskRead = nullptr;
static PDH_HCOUNTER g_diskWrite = nullptr;
static PDH_HCOUNTER g_netSend = nullptr;
static PDH_HCOUNTER g_netRecv = nullptr;

// инициализирует PDH для мониторинга дисков и сети
void InitPerformanceCounters() {
    if (g_query) return;

    PdhOpenQuery(nullptr, 0, &g_query);

    PdhAddCounter(g_query, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &g_diskRead);
    PdhAddCounter(g_query, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &g_diskWrite);
    PdhAddCounter(g_query, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &g_netSend);
    PdhAddCounter(g_query, L"\\Network Interface(*)\\Bytes Received/sec", 0, &g_netRecv);

    
    PdhCollectQueryData(g_query);
}

// получает текущее значение счётчика производительности (PDH)
static double GetCounter(PDH_HCOUNTER c) {
    if (!c) return 0.0;

    PDH_FMT_COUNTERVALUE v;
    if (PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &v) == ERROR_SUCCESS)
        return v.doubleValue;

    return 0.0;
}

double GetDiskReadSpeed() { return GetCounter(g_diskRead); }
double GetDiskWriteSpeed() { return GetCounter(g_diskWrite); }
double GetNetworkUploadSpeed() { return GetCounter(g_netSend); }
double GetNetworkDownloadSpeed() { return GetCounter(g_netRecv); }

// закрывает PDH-запрос и чистит ресурсы счётчиков производительности
void ClosePerformanceCounters() {
    if (g_query) {
        PdhCloseQuery(g_query);
        g_query = nullptr;
    }
}

// Общий процент загрузки CPU
double GetSystemCpuUsage() {
    static ULONGLONG prevIdle = 0, prevKernel = 0, prevUser = 0;

    FILETIME idle, kernel, user;
    GetSystemTimes(&idle, &kernel, &user);

    ULONGLONG i = FileTimeToUll(idle);
    ULONGLONG k = FileTimeToUll(kernel);
    ULONGLONG u = FileTimeToUll(user);

    if (!prevIdle) {
        prevIdle = i; prevKernel = k; prevUser = u;
        return 0;
    }

    ULONGLONG idleDelta = i - prevIdle; // простой
    ULONGLONG totalDelta = (k - prevKernel) + (u - prevUser); // работа

    prevIdle = i; 
    prevKernel = k; 
    prevUser = u;

    return 100.0 - (100.0 * idleDelta / totalDelta);
}

// получает объём общей и доступной физической памяти системы
void GetSystemMemoryInfo(DWORDLONG& total, DWORDLONG& avail) {
    MEMORYSTATUSEX m;
    m.dwLength = sizeof(m);
    GlobalMemoryStatusEx(&m);
    total = m.ullTotalPhys;
    avail = m.ullAvailPhys;
}

// Получение полного пути к .exe
void CollectProcessPaths() {
    for (auto& pi : g_processes) {
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
        if (!h) continue;

        char path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameA(h, 0, path, &size))
            pi.imagePath = path;

        CloseHandle(h);
    }
}


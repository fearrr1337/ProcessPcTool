#include "ProcessInfo.h"

std::vector<ProcessInfo> g_processes;

// Вспомогательная функция для преобразования FILETIME в 64-битное целое
static ULONGLONG FileTimeToUll(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

void ListProcesses() {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    g_processes.clear();

    if (Process32First(hSnapshot, &pe32)) {
        do {
            ProcessInfo pi;
            pi.pid = pe32.th32ProcessID;
            pi.name = pe32.szExeFile;
            pi.cpuUsage = 0.0;
            pi.workingSetSize = 0;
            pi.privateUsage = 0;
            pi.ioReadBytesDelta = 0;
            pi.ioWriteBytesDelta = 0;
            pi.prevKernelTime = 0;
            pi.prevUserTime = 0;
            pi.prevIoRead = 0;
            pi.prevIoWrite = 0;
            pi.prevTimeStamp = 0;
            g_processes.push_back(pi);
        } while (Process32Next(hSnapshot, &pe32));
    }
    CloseHandle(hSnapshot);
}

void UpdateProcessesStats() {
    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULONGLONG now = FileTimeToUll(nowFt);

    for (auto& pi : g_processes) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
        if (!hProcess) continue;

        FILETIME createTime, exitTime, kernelTime, userTime;
        ULONGLONG kernel = 0, user = 0;
        if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
            kernel = FileTimeToUll(kernelTime);
            user = FileTimeToUll(userTime);
        }

        PROCESS_MEMORY_COUNTERS_EX pmc;
        pmc.cb = sizeof(pmc);
        if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            pi.workingSetSize = pmc.WorkingSetSize;
            pi.privateUsage = pmc.PrivateUsage;
        }

        IO_COUNTERS ioCounters;
        ULONGLONG read = 0, write = 0;
        if (GetProcessIoCounters(hProcess, &ioCounters)) {
            read = ioCounters.ReadTransferCount;
            write = ioCounters.WriteTransferCount;
        }

        CloseHandle(hProcess);

        if (pi.prevTimeStamp != 0) {
            ULONGLONG timeDelta = now - pi.prevTimeStamp;
            if (timeDelta > 0) {
                ULONGLONG cpuDelta = (kernel - pi.prevKernelTime) + (user - pi.prevUserTime);
                static DWORD numProcessors = [] {
                    SYSTEM_INFO si;
                    GetSystemInfo(&si);
                    return si.dwNumberOfProcessors;
                    }();
                pi.cpuUsage = (cpuDelta * 100.0) / (timeDelta * numProcessors);
                if (pi.cpuUsage > 100.0) pi.cpuUsage = 100.0;

                double seconds = static_cast<double>(timeDelta) * 1e-7;
                pi.ioReadBytesDelta = static_cast<ULONGLONG>((read - pi.prevIoRead) / seconds);
                pi.ioWriteBytesDelta = static_cast<ULONGLONG>((write - pi.prevIoWrite) / seconds);
            }
        }

        pi.prevKernelTime = kernel;
        pi.prevUserTime = user;
        pi.prevIoRead = read;
        pi.prevIoWrite = write;
        pi.prevTimeStamp = now;
    }
}

double GetSystemCpuUsage() {
    static ULONGLONG prevIdle = 0, prevKernel = 0, prevUser = 0;
    static ULONGLONG prevTime = 0;

    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user))
        return 0.0;

    ULONGLONG idleNow = FileTimeToUll(idle);
    ULONGLONG kernelNow = FileTimeToUll(kernel);
    ULONGLONG userNow = FileTimeToUll(user);
    ULONGLONG now = FileTimeToUll(idle);

    if (prevTime == 0) {
        prevIdle = idleNow;
        prevKernel = kernelNow;
        prevUser = userNow;
        prevTime = now;
        return 0.0;
    }

    ULONGLONG idleDelta = idleNow - prevIdle;
    ULONGLONG kernelDelta = kernelNow - prevKernel;
    ULONGLONG userDelta = userNow - prevUser;
    ULONGLONG totalDelta = kernelDelta + userDelta;
    ULONGLONG timeDelta = now - prevTime;

    prevIdle = idleNow;
    prevKernel = kernelNow;
    prevUser = userNow;
    prevTime = now;

    if (totalDelta == 0 || timeDelta == 0) return 0.0;

    double cpuUsage = 100.0 - (100.0 * idleDelta / totalDelta);
    return cpuUsage;
}

void GetSystemMemoryInfo(DWORDLONG& totalPhys, DWORDLONG& availPhys) {
    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus)) {
        totalPhys = memStatus.ullTotalPhys;
        availPhys = memStatus.ullAvailPhys;
    }
    else {
        totalPhys = availPhys = 0;
    }
}

static PDH_HQUERY g_hQuery = nullptr;
static PDH_HCOUNTER g_hCounterDiskRead = nullptr;
static PDH_HCOUNTER g_hCounterDiskWrite = nullptr;
static PDH_HCOUNTER g_hCounterNetSend = nullptr;
static PDH_HCOUNTER g_hCounterNetRecv = nullptr;

void InitPerformanceCounters() {
    if (g_hQuery) return;
    PdhOpenQuery(nullptr, 0, &g_hQuery);
    if (g_hQuery) {
        PdhAddCounter(g_hQuery, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", 0, &g_hCounterDiskRead);
        PdhAddCounter(g_hQuery, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", 0, &g_hCounterDiskWrite);
        PdhAddCounter(g_hQuery, L"\\Network Interface(*)\\Bytes Sent/sec", 0, &g_hCounterNetSend);
        PdhAddCounter(g_hQuery, L"\\Network Interface(*)\\Bytes Received/sec", 0, &g_hCounterNetRecv);
    }
}

void ClosePerformanceCounters() {
    if (g_hQuery) {
        PdhCloseQuery(g_hQuery);
        g_hQuery = nullptr;
        g_hCounterDiskRead = g_hCounterDiskWrite = g_hCounterNetSend = g_hCounterNetRecv = nullptr;
    }
}

static double GetPdhCounterValue(PDH_HCOUNTER counter) {
    if (!counter) return 0.0;
    PdhCollectQueryData(g_hQuery);
    Sleep(1000);
    PdhCollectQueryData(g_hQuery);
    PDH_FMT_COUNTERVALUE value;
    if (PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, nullptr, &value) == ERROR_SUCCESS) {
        return value.doubleValue;
    }
    return 0.0;
}

double GetDiskReadSpeed() {
    return GetPdhCounterValue(g_hCounterDiskRead);
}

double GetDiskWriteSpeed() {
    return GetPdhCounterValue(g_hCounterDiskWrite);
}

double GetNetworkUploadSpeed() {
    return GetPdhCounterValue(g_hCounterNetSend);
}

double GetNetworkDownloadSpeed() {
    return GetPdhCounterValue(g_hCounterNetRecv);
}

void CollectProcessPaths() {
    for (auto& pi : g_processes) {
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
        if (!hProcess) {
            pi.imagePath.clear();
            continue;
        }

        char path[MAX_PATH];
        DWORD size = MAX_PATH;
        if (QueryFullProcessImageNameA(hProcess, 0, path, &size)) {
            pi.imagePath.assign(path, size);
        }
        else {
            pi.imagePath.clear();
        }
        CloseHandle(hProcess);
    }
}
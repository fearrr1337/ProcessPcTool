#include "SystemInfoWin.h"
#include <windows.h>
#include <fstream>
#include <cstdlib>


// название процессора
static std::string GetGpuName() {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string tempFile = std::string(tempPath) + "dxdiag.txt";

    // dxdiag(сбор подробной информации directx)
    system(("dxdiag /t " + tempFile).c_str());

    std::ifstream file(tempFile);
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("Card name:") != std::string::npos) {
            size_t pos = line.find(":");
            if (pos != std::string::npos)
                return line.substr(pos + 2);
        }
    }
    return "Unknown";
}


// получение системных компонентов
SystemInfoData GetSystemInfoData() {
    SystemInfoData info = {};


    SYSTEM_INFO si;
    GetSystemInfo(&si);
    info.cores = si.dwNumberOfProcessors;
    info.logicalProcessors = si.dwNumberOfProcessors;

    // название процессора через перем.окружения PROCESSOR_IDENTIFIER от win
    char cpuName[128] = { 0 };
    if (GetEnvironmentVariableA("PROCESSOR_IDENTIFIER", cpuName, sizeof(cpuName)) > 0) {
        info.processorName = cpuName;
    }
    else {
        info.processorName = "Unknown CPU";
    }

    // оператива
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        info.totalMemoryGB = mem.ullTotalPhys / 1024.0 / 1024.0 / 1024.0;
        info.usedMemoryGB = (mem.ullTotalPhys - mem.ullAvailPhys) / 1024.0 / 1024.0 / 1024.0;
    }

    // видюха
    info.gpuName = GetGpuName();

    return info;
}
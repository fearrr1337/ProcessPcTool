#pragma once
#include <string>

struct SystemInfoData {
    std::string processorName; // название процессора
    unsigned int cores; // физические ядра
    unsigned int logicalProcessors; // потоки
    std::string gpuName; // видеокарта
    double totalMemoryGB; // общий объем ram
    double usedMemoryGB; // занятая ram
};


SystemInfoData GetSystemInfoData();
#pragma once
#ifndef PROCESSICONS_H
#define PROCESSICONS_H

#include <vector>
#include <string>
#include <windows.h>
#include <shellapi.h>   // для ExtractIconEx

#include "include/raylib.h"

// Глобальный вектор иконок процессов (индексы соответствуют g_processPaths)
extern std::vector<Image> g_processIcons;

// Загружает иконку из указанного exe-файла заданного размера.
// Возвращает пустой Image при ошибке.
Image LoadIconFromExe(const std::string& filePath, int iconSize = 32);

// Обновляет вектор иконок, загружая их для каждого пути из списка.
// Предыдущие иконки автоматически выгружаются.
void UpdateProcessIcons(const std::vector<std::string>& paths, int iconSize = 32);

// Выгружает все иконки из вектора и очищает его.
void UnloadProcessIcons();

#endif
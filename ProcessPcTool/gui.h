#pragma once
#include "include/raylib.h"
#include "ProcessInfo.h"
#include <string>
#include <vector>
#include <cstdio>
#include <algorithm>

static void CreateDefaultIcon();
static void CleanupTextureCache();
static Texture2D GetProcessTexture(const ProcessInfo& proc);
static void FormatSpeed(char* buffer, size_t size, double bytesPerSec);
void mainWindow();
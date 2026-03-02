#pragma once
#include <string>

bool GetProcessIconPixels(const std::string& path, int& width, int& height, const unsigned char*& pixels, int targetSize = 16);
bool GetDefaultExeIconPixels(int& width, int& height, const unsigned char*& pixels, int targetSize = 16);
void CleanupIconCache();
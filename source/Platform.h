#pragma once

#include <cstdint>
#include <string>
#include <vector>

bool PlatformInit();        // runs at program start
void PlatformShutdown();    // cleanup
bool PlatformRunning();     // master loop condition
uint64_t PlatformTicks();   // milliseconds
std::string PlatformDataPath(); // "romfs:/data/" or "data/"
bool PlatformReadFile(const char* path, std::vector<unsigned char>& outData);

#include "Platform.h"

#include <SDL2/SDL.h>
#include <cstdio>

#ifdef __SWITCH__
#include <switch.h>
#endif

#ifndef __SWITCH__
// Desktop state for SDL_QUIT handling
static bool g_running = true;
#endif

bool PlatformInit() {
#ifdef __SWITCH__
    socketInitializeDefault();
    nxlinkStdio();
    if (romfsInit() != 0) {
        std::printf("romfsInit failed\n");
        return false;
    }
    setvbuf(stdout, NULL, _IONBF, 0);
    return true;
#else
    g_running = true;
    return true;
#endif
}

void PlatformShutdown() {
#ifdef __SWITCH__
    romfsExit();
    socketExit();
#endif
}

bool PlatformRunning() {
#ifdef __SWITCH__
    return appletMainLoop();
#else
    if (!g_running)
        return false;

    SDL_PumpEvents();
    SDL_Event e;
    // Peek for quit events without consuming the rest of the queue.
    while (SDL_PeepEvents(&e, 1, SDL_PEEKEVENT, SDL_QUIT, SDL_QUIT) > 0) {
        if (e.type == SDL_QUIT) {
            g_running = false;
        }
    }
    return g_running;
#endif
}

uint64_t PlatformTicks() {
#ifdef __SWITCH__
    return SDL_GetTicks();
#else
    return SDL_GetTicks64();
#endif
}

std::string PlatformDataPath() {
#ifdef __SWITCH__
    return "romfs:/data/";
#else
    return "data/";
#endif
}

// main.cpp
#include <switch.h>       // for appletMainLoop / console logging if you want
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>

#include "RendererGL.h"

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    // Initialize NxLink / debug print if you want to see logs in nxlink
    consoleDebugInit(debugDevice_SVC);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    // Attributes for GLES2
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE,   8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,  8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    int winW = 1280;
    int winH = 720;

    SDL_Window* window = SDL_CreateWindow(
        "Switch Doom Editor",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        winW, winH,
        SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN
    );

    if (!window) {
        printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) {
        printf("SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_GL_SetSwapInterval(1); // vsync

    RendererGL renderer;
    if (!renderer.init(window)) {
        printf("Renderer init failed\n");
        SDL_GL_DeleteContext(glCtx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    bool running = true;

    // Main loop; on Switch you can also wrap in appletMainLoop()
    while (running && appletMainLoop()) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    if (ev.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        renderer.resize(ev.window.data1, ev.window.data2);
                    }
                    break;
                case SDL_CONTROLLERBUTTONDOWN:
                    // Quick exit on + button, for example
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
                        running = false;
                    }
                    break;
            }
        }

        renderer.beginFrame();
        // Here is where the editor will later draw:
        // - grid
        // - vertices, lines, sectors
        renderer.drawTestTriangle();
        renderer.endFrame(window);
    }

    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    consoleExit(NULL);
    return 0;
}

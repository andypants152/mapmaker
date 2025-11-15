// main.cpp
#include <switch.h>       // for appletMainLoop / console logging if you want
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "RendererGL.h"

struct EditorState {
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    float cursorRawX = 0.0f;
    float cursorRawY = 0.0f;
    bool snapEnabled = true;
    float snapSize = 1.0f;
    std::vector<std::pair<float, float>> vertices;
};

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

    Camera2D camera;
    camera.zoom = 32.0f;
    camera.offsetX = 0.0f;
    camera.offsetY = 0.0f;

    SDL_GameController* controller = nullptr;
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) {
                break;
            }
        }
    }

    EditorState state;

    bool running = true;
    uint32_t lastTicks = SDL_GetTicks();

    // Main loop; on Switch you can also wrap in appletMainLoop()
    while (running && appletMainLoop()) {
        uint32_t now = SDL_GetTicks();
        float dt = (now - lastTicks) / 1000.0f;
        lastTicks = now;

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
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        const float eps = 0.0001f;
                        bool exists = false;
                        for (const auto& v : state.vertices) {
                            if (std::fabs(v.first - state.cursorX) < eps &&
                                std::fabs(v.second - state.cursorY) < eps) {
                                exists = true;
                                break;
                            }
                        }
                        if (!exists) {
                            state.vertices.emplace_back(state.cursorX, state.cursorY);
                        }
                    }
                    break;
            }
        }

        if (controller) {
            const int16_t deadZone = 8000;
            const float invMax = 1.0f / 32767.0f;
            const float panSpeed = 8.0f; // world units per second

            int16_t rawX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t rawY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

            float axisX = (std::abs(rawX) > deadZone) ? rawX * invMax : 0.0f;
            float axisY = (std::abs(rawY) > deadZone) ? rawY * invMax : 0.0f;

            camera.offsetX -= axisX * panSpeed * dt;
            camera.offsetY -= axisY * panSpeed * dt;

            float zoomInput = 0.0f;
            if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) {
                zoomInput += 1.0f;
            }
            if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)) {
                zoomInput -= 1.0f;
            }

            if (zoomInput != 0.0f) {
                const float zoomSpeed = 1.5f;
                camera.zoom *= (1.0f + zoomInput * zoomSpeed * dt);
                if (camera.zoom < 0.1f) camera.zoom = 0.1f;
                if (camera.zoom > 32.0f) camera.zoom = 32.0f;
            }

            int16_t rawCursorX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
            int16_t rawCursorY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

            float axisCursorX = (std::abs(rawCursorX) > deadZone) ? rawCursorX * invMax : 0.0f;
            float axisCursorY = (std::abs(rawCursorY) > deadZone) ? rawCursorY * invMax : 0.0f;

            const float baseCursorSpeed = 160.0f;
            const float cursorSpeed = baseCursorSpeed / std::max(camera.zoom, 0.001f);

            float newCursorRawX = state.cursorRawX + axisCursorX * cursorSpeed * dt;
            float newCursorRawY = state.cursorRawY - axisCursorY * cursorSpeed * dt;

            if (state.snapEnabled && state.snapSize > 0.0f) {
                state.cursorX = std::round(newCursorRawX / state.snapSize) * state.snapSize;
                state.cursorY = std::round(newCursorRawY / state.snapSize) * state.snapSize;
            } else {
                state.cursorX = newCursorRawX;
                state.cursorY = newCursorRawY;
            }

            state.cursorRawX = newCursorRawX;
            state.cursorRawY = newCursorRawY;
        }

        renderer.beginFrame();
        renderer.drawGrid(camera, 1.0f);    // 1 unit = 1 grid cell
        renderer.drawPoint2D(state.cursorX, state.cursorY, 0.2f, 0.1f, 0.4f, 1.0f);
        for (const auto& vert : state.vertices) {
            renderer.drawPoint2D(vert.first, vert.second, 0.12f, 0.0f, 1.0f, 1.0f);
        }
        renderer.endFrame(window);
    }

    if (controller) {
        SDL_GameControllerClose(controller);
    }

    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    consoleExit(NULL);
    return 0;
}

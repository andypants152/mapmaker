// main.cpp
#include <switch.h>       // for appletMainLoop / console logging if you want
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "RendererGL.h"

struct LineDef {
    int v1 = -1;
    int v2 = -1;
};

struct EditorState {
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    float cursorRawX = 0.0f;
    float cursorRawY = 0.0f;
    bool snapEnabled = true;
    float snapSize = 1.0f;
    std::vector<std::pair<float, float>> vertices;
    std::vector<LineDef> lines;
    int hoveredVertex = -1;
    int selectedVertex = -1;
    bool wallMode = false;
};

static int findVertexAt(const EditorState& state, float x, float y, float eps = 0.0001f) {
    for (size_t i = 0; i < state.vertices.size(); ++i) {
        if (std::fabs(state.vertices[i].first - x) < eps &&
            std::fabs(state.vertices[i].second - y) < eps) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static int findLineAt(const EditorState& state, float x, float y, float eps = 0.0001f) {
    for (size_t i = 0; i < state.lines.size(); ++i) {
        const LineDef& line = state.lines[i];
        if (line.v1 < 0 || line.v2 < 0 ||
            line.v1 >= static_cast<int>(state.vertices.size()) ||
            line.v2 >= static_cast<int>(state.vertices.size())) {
            continue;
        }
        const auto& v1 = state.vertices[line.v1];
        const auto& v2 = state.vertices[line.v2];

        float dx = v2.first - v1.first;
        float dy = v2.second - v1.second;
        float px = x - v1.first;
        float py = y - v1.second;

        float cross = dx * py - dy * px;
        if (std::fabs(cross) > eps)
            continue;

        float minX = std::min(v1.first, v2.first) - eps;
        float maxX = std::max(v1.first, v2.first) + eps;
        float minY = std::min(v1.second, v2.second) - eps;
        float maxY = std::max(v1.second, v2.second) + eps;

        if (x < minX || x > maxX)
            continue;
        if (y < minY || y > maxY)
            continue;

        return static_cast<int>(i);
    }
    return -1;
}

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

        bool selectPressed = false; // logical "A" action (wall mode)
        bool placePressed = false;  // logical "B" action (place vertex)
        bool deletePressed = false;

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
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                        selectPressed = true;
                    }
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
                        placePressed = true;
                    }
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_X) {
                        deletePressed = true;
                    }
                    break;
            }
        }

        if (controller) {
            const int16_t deadZone = 8000;
            const float invMax = 1.0f / 32767.0f;
            const float panSpeed = 8.0f; // world units per second

            int16_t camRawX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
            int16_t camRawY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

            float camAxisX = (std::abs(camRawX) > deadZone) ? camRawX * invMax : 0.0f;
            float camAxisY = (std::abs(camRawY) > deadZone) ? camRawY * invMax : 0.0f;

            camera.offsetX += camAxisX * panSpeed * dt;
            camera.offsetY -= camAxisY * panSpeed * dt;

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

            int16_t rawCursorX = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t rawCursorY = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

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

        state.hoveredVertex = -1;
        const float hoverRadius = 0.4f;
        float bestDist2 = hoverRadius * hoverRadius;
        for (size_t i = 0; i < state.vertices.size(); ++i) {
            float dx = state.cursorX - state.vertices[i].first;
            float dy = state.cursorY - state.vertices[i].second;
            float dist2 = dx * dx + dy * dy;
            if (dist2 <= bestDist2) {
                bestDist2 = dist2;
                state.hoveredVertex = static_cast<int>(i);
            }
        }

        if (deletePressed) {
            int deleteVertex = findVertexAt(state, state.cursorX, state.cursorY);
            if (deleteVertex != -1) {
                state.vertices.erase(state.vertices.begin() + deleteVertex);

                for (auto it = state.lines.begin(); it != state.lines.end();) {
                    if (it->v1 == deleteVertex || it->v2 == deleteVertex) {
                        it = state.lines.erase(it);
                        continue;
                    }
                    if (it->v1 > deleteVertex) --(it->v1);
                    if (it->v2 > deleteVertex) --(it->v2);
                    ++it;
                }

                if (state.selectedVertex == deleteVertex) {
                    state.selectedVertex = -1;
                    state.wallMode = false;
                } else if (state.selectedVertex > deleteVertex) {
                    --state.selectedVertex;
                }

                if (state.hoveredVertex == deleteVertex) {
                    state.hoveredVertex = -1;
                } else if (state.hoveredVertex > deleteVertex) {
                    --state.hoveredVertex;
                }
            } else {
                int deleteLine = findLineAt(state, state.cursorX, state.cursorY);
                if (deleteLine != -1) {
                    state.lines.erase(state.lines.begin() + deleteLine);
                }
            }
        }

        int placedVertexIndex = -1;
        if (placePressed) {
            placedVertexIndex = findVertexAt(state, state.cursorX, state.cursorY);
            if (placedVertexIndex == -1) {
                state.vertices.emplace_back(state.cursorX, state.cursorY);
                placedVertexIndex = static_cast<int>(state.vertices.size() - 1);
            }

            if (state.wallMode && state.selectedVertex >= 0 && placedVertexIndex >= 0 &&
                placedVertexIndex != state.selectedVertex) {
                LineDef newLine;
                newLine.v1 = state.selectedVertex;
                newLine.v2 = placedVertexIndex;
                state.lines.push_back(newLine);
                state.selectedVertex = placedVertexIndex;
            }
        }

        if (selectPressed) {
            if (state.hoveredVertex != -1) {
                if (!state.wallMode) {
                    state.wallMode = true;
                    state.selectedVertex = state.hoveredVertex;
                } else if (state.selectedVertex != state.hoveredVertex) {
                    LineDef newLine;
                    newLine.v1 = state.selectedVertex;
                    newLine.v2 = state.hoveredVertex;
                    state.lines.push_back(newLine);
                    state.selectedVertex = state.hoveredVertex;
                }
            } else {
                state.wallMode = false;
                state.selectedVertex = -1;
            }
        }

        renderer.beginFrame();
        renderer.drawGrid(camera, 1.0f);    // 1 unit = 1 grid cell

        for (const auto& line : state.lines) {
            if (line.v1 < 0 || line.v2 < 0 ||
                line.v1 >= static_cast<int>(state.vertices.size()) ||
                line.v2 >= static_cast<int>(state.vertices.size())) {
                continue;
            }
            const auto& v1 = state.vertices[line.v1];
            const auto& v2 = state.vertices[line.v2];
            renderer.drawLine2D(v1.first, v1.second, v2.first, v2.second, 1.0f, 1.0f, 1.0f);
        }

        for (const auto& vert : state.vertices) {
            renderer.drawPoint2D(vert.first, vert.second, 0.12f, 0.0f, 1.0f, 1.0f);
        }

        if (state.hoveredVertex >= 0 && state.hoveredVertex < static_cast<int>(state.vertices.size())) {
            const auto& hv = state.vertices[state.hoveredVertex];
            renderer.drawPoint2D(hv.first, hv.second, 0.16f, 1.0f, 1.0f, 0.0f);
        }

        if (state.wallMode && state.selectedVertex >= 0 &&
            state.selectedVertex < static_cast<int>(state.vertices.size())) {
            const auto& sv = state.vertices[state.selectedVertex];
            renderer.drawPoint2D(sv.first, sv.second, 0.2f, 1.0f, 0.5f, 0.0f);
        }

        if (state.wallMode &&
            state.selectedVertex >= 0 &&
            state.hoveredVertex >= 0 &&
            state.selectedVertex < static_cast<int>(state.vertices.size()) &&
            state.hoveredVertex < static_cast<int>(state.vertices.size()) &&
            state.selectedVertex != state.hoveredVertex) {
            const auto& sv = state.vertices[state.selectedVertex];
            const auto& hv = state.vertices[state.hoveredVertex];
            renderer.drawLine2D(sv.first, sv.second, hv.first, hv.second, 1.0f, 1.0f, 0.0f);
        }

        const float cursorSize = 0.2f;
        renderer.drawLine2D(state.cursorX - cursorSize, state.cursorY,
                            state.cursorX + cursorSize, state.cursorY,
                            0.1f, 0.4f, 1.0f);
        renderer.drawLine2D(state.cursorX, state.cursorY - cursorSize,
                            state.cursorX, state.cursorY + cursorSize,
                            0.1f, 0.4f, 1.0f);
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

// main.cpp
#include <switch.h>       // for appletMainLoop / console logging if you want
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>
#include <unistd.h>
#include <cstdio>

#ifdef __cplusplus
extern "C" {
    void userAppInit(void);
    void userAppExit(void);
}
#endif

static int g_nxlinkSock = -1;
static inline void initNxLink() { userAppInit(); }

#ifdef __cplusplus
#endif

#include "RendererGL.h"
#include "EditorState.h"

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

static bool loopsEqual(const std::vector<int>& a, const std::vector<int>& b) {
    if (a.size() != b.size())
        return false;
    if (a.empty())
        return true;
    const size_t count = a.size();
    for (size_t offset = 0; offset < count; ++offset) {
        bool match = true;
        for (size_t i = 0; i < count; ++i) {
            if (a[i] != b[(i + offset) % count]) {
                match = false;
                break;
            }
        }
        if (match)
            return true;
    }
    return false;
}

static void buildDefaultMap(EditorState& state) {
    if (!state.vertices.empty() || !state.lines.empty())
        return;

    state.vertices.push_back({0.0f, 0.0f});
    state.vertices.push_back({8.0f, 0.0f});
    state.vertices.push_back({8.0f, 6.0f});
    state.vertices.push_back({0.0f, 6.0f});

    state.lines.push_back({0, 1});
    state.lines.push_back({1, 2});
    state.lines.push_back({2, 3});
    state.lines.push_back({3, 0});
}

static void rebuildWorldMesh(const EditorState& state, Mesh3D& mesh,
                             float floorHeight = 0.0f, float ceilingHeight = 3.0f) {
    mesh.vertices.clear();
    mesh.normals.clear();
    mesh.colors.clear();
    mesh.indices.clear();

    uint16_t baseIndex = 0;

    auto addVertex = [&](float x, float y, float z, float nx, float ny, float nz, float r, float g, float b) -> uint16_t {
        mesh.vertices.push_back(x);
        mesh.vertices.push_back(y);
        mesh.vertices.push_back(z);
        mesh.normals.push_back(nx);
        mesh.normals.push_back(ny);
        mesh.normals.push_back(nz);
        mesh.colors.push_back(r);
        mesh.colors.push_back(g);
        mesh.colors.push_back(b);
        return baseIndex++;
    };

    for (const auto& sector : state.sectors) {
        if (sector.vertices.size() < 3)
            continue;

        std::vector<uint16_t> fanFloor;
        std::vector<uint16_t> fanCeil;
        for (int idx : sector.vertices) {
            if (idx < 0 || idx >= static_cast<int>(state.vertices.size()))
                continue;
            const auto& v = state.vertices[idx];
            fanFloor.push_back(addVertex(v.first, v.second, floorHeight, 0.0f, 0.0f, 1.0f, 0.5f, 0.35f, 0.2f));
            fanCeil.push_back(addVertex(v.first, v.second, ceilingHeight, 0.0f, 0.0f, -1.0f, 0.65f, 0.65f, 0.7f));
        }

        for (size_t i = 1; i + 1 < fanFloor.size(); ++i) {
            mesh.indices.push_back(fanFloor[0]);
            mesh.indices.push_back(fanFloor[i]);
            mesh.indices.push_back(fanFloor[i + 1]);
        }

        for (size_t i = 1; i + 1 < fanCeil.size(); ++i) {
            mesh.indices.push_back(fanCeil[0]);
            mesh.indices.push_back(fanCeil[i + 1]);
            mesh.indices.push_back(fanCeil[i]);
        }

        for (size_t i = 0; i < sector.vertices.size(); ++i) {
            int idxA = sector.vertices[i];
            int idxB = sector.vertices[(i + 1) % sector.vertices.size()];
            if (idxA < 0 || idxA >= static_cast<int>(state.vertices.size()) ||
                idxB < 0 || idxB >= static_cast<int>(state.vertices.size()))
                continue;
            const auto& vA = state.vertices[idxA];
            const auto& vB = state.vertices[idxB];
            float edgeX = vB.first - vA.first;
            float edgeY = vB.second - vA.second;
            float len = std::sqrt(edgeX * edgeX + edgeY * edgeY);
            float nx = 0.0f;
            float ny = 0.0f;
            if (len > 0.0001f) {
                nx = edgeY / len;
                ny = -edgeX / len;
            }

            uint16_t a0 = addVertex(vA.first, vA.second, floorHeight, nx, ny, 0.0f, 0.6f, 0.6f, 0.6f);
            uint16_t b0 = addVertex(vB.first, vB.second, floorHeight, nx, ny, 0.0f, 0.6f, 0.6f, 0.6f);
            uint16_t b1 = addVertex(vB.first, vB.second, ceilingHeight, nx, ny, 0.0f, 0.6f, 0.6f, 0.6f);
            uint16_t a1 = addVertex(vA.first, vA.second, ceilingHeight, nx, ny, 0.0f, 0.6f, 0.6f, 0.6f);

            mesh.indices.push_back(a0);
            mesh.indices.push_back(b0);
            mesh.indices.push_back(b1);
            mesh.indices.push_back(a0);
            mesh.indices.push_back(b1);
            mesh.indices.push_back(a1);
        }
    }

    std::printf("world mesh: %zu verts, %zu tris\n",
                mesh.vertices.size() / 3,
                mesh.indices.size() / 3);
}
static void rebuildSectors(EditorState& state) {
    state.sectors.clear();
    if (state.vertices.size() < 3 || state.lines.size() < 3)
        return;

    std::vector<std::vector<int>> adjacency(state.vertices.size());
    std::map<std::pair<int, int>, bool> edgeUsed;

    for (const auto& line : state.lines) {
        if (line.v1 < 0 || line.v2 < 0 ||
            line.v1 >= static_cast<int>(state.vertices.size()) ||
            line.v2 >= static_cast<int>(state.vertices.size()) ||
            line.v1 == line.v2) {
            continue;
        }
        adjacency[line.v1].push_back(line.v2);
        adjacency[line.v2].push_back(line.v1);
        edgeUsed[{line.v1, line.v2}] = false;
        edgeUsed[{line.v2, line.v1}] = false;
    }

    for (size_t start = 0; start < adjacency.size(); ++start) {
        for (int neighbor : adjacency[start]) {
            auto key = std::make_pair(static_cast<int>(start), neighbor);
            if (edgeUsed[key])
                continue;

            std::vector<int> loop;
            std::vector<std::pair<int, int>> usedEdges;
            loop.push_back(static_cast<int>(start));

            edgeUsed[key] = true;
            usedEdges.push_back(key);
            int prev = static_cast<int>(start);
            int curr = neighbor;
            loop.push_back(curr);

            bool closed = false;
            while (true) {
                if (curr == static_cast<int>(start)) {
                    closed = true;
                    break;
                }

                bool advanced = false;
                for (int next : adjacency[curr]) {
                    if (next == prev)
                        continue;
                    auto nextKey = std::make_pair(curr, next);
                    if (edgeUsed.find(nextKey) == edgeUsed.end() || edgeUsed[nextKey])
                        continue;
                    edgeUsed[nextKey] = true;
                    usedEdges.push_back(nextKey);
                    prev = curr;
                    curr = next;
                    loop.push_back(curr);
                    advanced = true;
                    break;
                }

                if (!advanced) {
                    break;
                }
            }

            if (!closed)
            {
                for (const auto& e : usedEdges) {
                    edgeUsed[e] = false;
                }
                continue;
            }

            loop.pop_back(); // remove duplicated start

            if (loop.size() < 3) {
                for (const auto& e : usedEdges) {
                    edgeUsed[e] = false;
                }
                continue;
            }

            float area = 0.0f;
            for (size_t i = 0; i < loop.size(); ++i) {
                const auto& a = state.vertices[loop[i]];
                const auto& b = state.vertices[loop[(i + 1) % loop.size()]];
                area += (b.first - a.first) * (b.second + a.second);
            }
            if (area > 0.0f) {
                std::reverse(loop.begin(), loop.end());
            }

            bool duplicate = false;
            for (const auto& existing : state.sectors) {
                if (loopsEqual(loop, existing.vertices)) {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate) {
                Sector sector;
                sector.vertices = loop;
                state.sectors.push_back(std::move(sector));
            }
            else {
                for (const auto& e : usedEdges) {
                    edgeUsed[e] = false;
                }
            }
        }
    }
}

extern "C" {

void userAppInit(void) {
    nxlinkStdio();
    socketInitializeDefault();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("userAppInit(): nxlink ready\n");
}

void userAppExit(void) {
    socketExit();
}

}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    initNxLink();
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
    camera.offsetX = 4.0f;
    camera.offsetY = 3.0f;

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
    buildDefaultMap(state);
    state.cursorRawX = 4.0f;
    state.cursorRawY = 3.0f;
    state.cursorX    = 4.0f;
    state.cursorY    = 3.0f;
    rebuildSectors(state);
    rebuildWorldMesh(state, state.worldMesh);
    Camera3D fpsCamera;

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
        bool togglePlayPressed = false;
        bool needRebuild = false;

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
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
                        togglePlayPressed = true;
                    }
                    break;
            }
        }

        if (togglePlayPressed) {
            state.playMode = !state.playMode;
            state.wallMode = false;
            state.selectedVertex = -1;
            state.hoveredVertex = -1;
            if (state.playMode) {
                fpsCamera = Camera3D{};
                fpsCamera.x = 4.0f;
                fpsCamera.y = 3.0f;
                fpsCamera.z = 1.7f;
            }
        }

        if (!state.playMode && controller) {
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
        } else if (state.playMode && controller) {
            const int16_t deadZone = 8000;
            const float invMax = 1.0f / 32767.0f;

            int16_t moveXRaw = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
            int16_t moveYRaw = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);
            int16_t lookXRaw = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTX);
            int16_t lookYRaw = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_RIGHTY);

            float moveX = (std::abs(moveXRaw) > deadZone) ? moveXRaw * invMax : 0.0f;
            float moveY = (std::abs(moveYRaw) > deadZone) ? moveYRaw * invMax : 0.0f;
            float lookX = (std::abs(lookXRaw) > deadZone) ? lookXRaw * invMax : 0.0f;
            float lookY = (std::abs(lookYRaw) > deadZone) ? lookYRaw * invMax : 0.0f;

            const float moveSpeed = 5.0f;
            const float lookSpeed = 2.5f;

            fpsCamera.yaw += lookX * lookSpeed * dt;
            fpsCamera.pitch -= lookY * lookSpeed * dt;
            if (fpsCamera.pitch > 1.2f) fpsCamera.pitch = 1.2f;
            if (fpsCamera.pitch < -1.2f) fpsCamera.pitch = -1.2f;

            const float forwardX = std::sin(fpsCamera.yaw);
            const float forwardY = std::cos(fpsCamera.yaw);
            const float rightX = std::cos(fpsCamera.yaw);
            const float rightY = -std::sin(fpsCamera.yaw);

            float moveForward = -moveY;
            float moveStrafe = moveX;

            fpsCamera.x += (forwardX * moveForward + rightX * moveStrafe) * moveSpeed * dt;
            fpsCamera.y += (forwardY * moveForward + rightY * moveStrafe) * moveSpeed * dt;
        }

        if (!state.playMode) {
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
        } else {
            state.hoveredVertex = -1;
        }

        if (!state.playMode && deletePressed) {
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
                needRebuild = true;
            } else {
                int deleteLine = findLineAt(state, state.cursorX, state.cursorY);
                if (deleteLine != -1) {
                    state.lines.erase(state.lines.begin() + deleteLine);
                    needRebuild = true;
                }
            }
        }

        int placedVertexIndex = -1;
        if (!state.playMode && placePressed) {
            placedVertexIndex = findVertexAt(state, state.cursorX, state.cursorY);
            if (placedVertexIndex == -1) {
                state.vertices.emplace_back(state.cursorX, state.cursorY);
                placedVertexIndex = static_cast<int>(state.vertices.size() - 1);
                needRebuild = true;
            }

            if (state.wallMode && state.selectedVertex >= 0 && placedVertexIndex >= 0 &&
                placedVertexIndex != state.selectedVertex) {
                LineDef newLine;
                newLine.v1 = state.selectedVertex;
                newLine.v2 = placedVertexIndex;
                state.lines.push_back(newLine);
                state.selectedVertex = placedVertexIndex;
                needRebuild = true;
            }
        }

        if (!state.playMode && selectPressed) {
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
                    needRebuild = true;
                }
            } else {
                state.wallMode = false;
                state.selectedVertex = -1;
            }
        }

        if (!state.playMode) {
            if (needRebuild) {
                rebuildSectors(state);
                rebuildWorldMesh(state, state.worldMesh);
                needRebuild = false;
            }

            renderer.beginFrame();
            renderer.drawGrid(camera, 1.0f);    // 1 unit = 1 grid cell

            for (const auto& sector : state.sectors) {
                renderer.drawSectorFill(sector, state, 0.2f, 0.4f, 0.9f, 0.25f);
            }

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
        } else {
            renderer.drawMesh3D(state.worldMesh, fpsCamera);
            renderer.endFrame(window);
        }
    }

    if (controller) {
        SDL_GameControllerClose(controller);
    }

    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (g_nxlinkSock >= 0) {
        close(g_nxlinkSock);
        socketExit();
        g_nxlinkSock = -1;
    }
    consoleExit(NULL);
    return 0;
}

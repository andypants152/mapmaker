// main.cpp
#include <switch.h>       // for appletMainLoop / console logging if you want
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
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

struct Vec2 {
    float x;
    float y;
};

static Vec2 closestPointOnSegment(float px, float py, float ax, float ay, float bx, float by) {
    float vx = bx - ax;
    float vy = by - ay;
    float wx = px - ax;
    float wy = py - ay;
    float len2 = vx * vx + vy * vy;
    float t = (len2 > 0.0f) ? ((wx * vx + wy * vy) / len2) : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return { ax + vx * t, ay + vy * t };
}

static float distance2D(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

static bool segmentsIntersect(const Vec2& a1, const Vec2& a2, const Vec2& b1, const Vec2& b2) {
    auto cross = [](float x1, float y1, float x2, float y2) { return x1 * y2 - y1 * x2; };
    float dxa = a2.x - a1.x, dya = a2.y - a1.y;
    float dxb = b2.x - b1.x, dyb = b2.y - b1.y;
    float c1 = cross(dxb, dyb, b1.x - a1.x, b1.y - a1.y);
    float c2 = cross(dxb, dyb, b2.x - a1.x, b2.y - a1.y);
    float c3 = cross(dxa, dya, a1.x - b1.x, a1.y - b1.y);
    float c4 = cross(dxa, dya, a2.x - b1.x, a2.y - b1.y);
    return ( (c1 * c2) < 0.0f ) && ( (c3 * c4) < 0.0f );
}

static bool polygonSelfIntersects(const std::vector<Vec2>& verts) {
    const size_t n = verts.size();
    if (n < 4) return false;
    for (size_t i = 0; i < n; ++i) {
        Vec2 a1 = verts[i];
        Vec2 a2 = verts[(i + 1) % n];
        for (size_t j = i + 1; j < n; ++j) {
            if (j == i || (j + 1) % n == i) continue;
            if ((i == 0 && j == n - 1)) continue;
            Vec2 b1 = verts[j];
            Vec2 b2 = verts[(j + 1) % n];
            if (segmentsIntersect(a1, a2, b1, b2)) {
                return true;
            }
        }
    }
    return false;
}

static EntityType nextEntityBrush(EntityType t) {
    switch (t) {
        case EntityType::PlayerStart: return EntityType::EnemyWizard;
        case EntityType::EnemyWizard: return EntityType::EnemySpawn;
        case EntityType::EnemySpawn:  return EntityType::ItemPickup;
        case EntityType::ItemPickup:  return EntityType::PlayerStart;
    }
    return EntityType::PlayerStart;
}

static const char* entityTypeName(EntityType t) {
    switch (t) {
        case EntityType::PlayerStart: return "PlayerStart";
        case EntityType::EnemyWizard: return "EnemyWizard";
        case EntityType::EnemySpawn:  return "EnemySpawn";
        case EntityType::ItemPickup:  return "ItemPickup";
    }
    return "Unknown";
}

static void spawnProjectile(EditorState& state, float x, float y, float z,
                            float vx, float vy, float vz, bool fromPlayer) {
    Projectile p{ x, y, z, vx, vy, vz, fromPlayer, true };
    state.projectiles.active.push_back(p);
}

static void buildDefaultMap(EditorState& state) {
    state.vertices.clear();
    state.lines.clear();
    state.entities.clear();

    auto addLine = [&](uint16_t a, uint16_t b) { state.lines.push_back({a, b}); };

    // Room1 (gap on right y 4..6)
    uint16_t v0 = state.vertices.size(); state.vertices.push_back({0.0f, 0.0f});
    uint16_t v1 = state.vertices.size(); state.vertices.push_back({10.0f, 0.0f});
    uint16_t v2 = state.vertices.size(); state.vertices.push_back({10.0f, 4.0f});
    uint16_t v3 = state.vertices.size(); state.vertices.push_back({10.0f, 6.0f});
    uint16_t v4 = state.vertices.size(); state.vertices.push_back({10.0f,10.0f});
    uint16_t v5 = state.vertices.size(); state.vertices.push_back({0.0f,10.0f});
    addLine(v0, v1); addLine(v1, v2); addLine(v3, v4); addLine(v4, v5); addLine(v5, v0);

    // Corridor1 (10..12,4..6)
    uint16_t c1a = v2;
    uint16_t c1b = state.vertices.size(); state.vertices.push_back({12.0f, 4.0f});
    uint16_t c1c = state.vertices.size(); state.vertices.push_back({12.0f, 6.0f});
    uint16_t c1d = v3;
    addLine(c1a, c1b); addLine(c1b, c1c); addLine(c1c, c1d);

    // Room2 (gap on left/right y 4..6)
    uint16_t r2a = state.vertices.size(); state.vertices.push_back({12.0f, 0.0f});
    uint16_t r2b = state.vertices.size(); state.vertices.push_back({22.0f, 0.0f});
    uint16_t r2c = state.vertices.size(); state.vertices.push_back({22.0f, 4.0f});
    uint16_t r2d = state.vertices.size(); state.vertices.push_back({22.0f, 6.0f});
    uint16_t r2e = state.vertices.size(); state.vertices.push_back({22.0f,10.0f});
    uint16_t r2f = state.vertices.size(); state.vertices.push_back({12.0f,10.0f});
    addLine(r2a, r2b); addLine(r2b, r2c); addLine(r2d, r2e); addLine(r2e, r2f); addLine(r2f, r2a);
    addLine(r2c, c1b); addLine(c1c, r2d);

    // Corridor2 (22..24,4..6)
    uint16_t c2a = r2c;
    uint16_t c2b = state.vertices.size(); state.vertices.push_back({24.0f, 4.0f});
    uint16_t c2c = state.vertices.size(); state.vertices.push_back({24.0f, 6.0f});
    uint16_t c2d = r2d;
    addLine(c2a, c2b); addLine(c2b, c2c); addLine(c2c, c2d);

    // Room3 (gap on left/right y 4..6)
    uint16_t r3a = state.vertices.size(); state.vertices.push_back({24.0f, 0.0f});
    uint16_t r3b = state.vertices.size(); state.vertices.push_back({34.0f, 0.0f});
    uint16_t r3c = state.vertices.size(); state.vertices.push_back({34.0f, 4.0f});
    uint16_t r3d = state.vertices.size(); state.vertices.push_back({34.0f, 6.0f});
    uint16_t r3e = state.vertices.size(); state.vertices.push_back({34.0f,10.0f});
    uint16_t r3f = state.vertices.size(); state.vertices.push_back({24.0f,10.0f});
    addLine(r3a, r3b); addLine(r3b, r3c); addLine(r3d, r3e); addLine(r3e, r3f); addLine(r3f, r3a);
    addLine(r3c, c2b); addLine(c2c, r3d);

    // Corridor3 (34..36,4..6)
    uint16_t c3a = r3c;
    uint16_t c3b = state.vertices.size(); state.vertices.push_back({36.0f, 4.0f});
    uint16_t c3c = state.vertices.size(); state.vertices.push_back({36.0f, 6.0f});
    uint16_t c3d = r3d;
    addLine(c3a, c3b); addLine(c3b, c3c); addLine(c3c, c3d);

    // Tie corridor3 top/bottom edges across width to avoid diagonal fills
    addLine(c3a, c3d);

    // Room4 (gap on left y4..6)
    uint16_t r4a = state.vertices.size(); state.vertices.push_back({36.0f,-1.0f});
    uint16_t r4b = state.vertices.size(); state.vertices.push_back({48.0f,-1.0f});
    uint16_t r4c = state.vertices.size(); state.vertices.push_back({48.0f,11.0f});
    uint16_t r4d = state.vertices.size(); state.vertices.push_back({36.0f,11.0f});
    addLine(r4a, r4b); addLine(r4b, r4c); addLine(r4c, r4d);
    addLine(r4a, c3b); addLine(c3c, r4d);
    // Close the right wall fully to avoid diagonals
    addLine(c3b, c3c);

    // Entities
    state.entities.push_back({5.0f, 5.0f, EntityType::PlayerStart});
    state.entities.push_back({17.0f, 5.0f, EntityType::EnemyWizard});
    state.entities.push_back({29.0f, 5.0f, EntityType::ItemPickup});
    state.entities.push_back({39.0f, 5.0f, EntityType::EnemyWizard});
    state.entities.push_back({43.0f, 3.0f, EntityType::EnemyWizard});
    state.entities.push_back({43.0f, 7.0f, EntityType::EnemyWizard});
}

static bool earClip(const std::vector<Vec2>& poly, const std::vector<uint16_t>& localIdx, std::vector<uint16_t>& out) {
    if (poly.size() < 3) return false;
    float area = 0.0f;
    for (size_t i = 0; i < poly.size(); ++i) {
        const auto& a = poly[i];
        const auto& b = poly[(i + 1) % poly.size()];
        area += (a.x * b.y - b.x * a.y);
    }
    bool clockwise = area < 0.0f;

    std::vector<uint16_t> idx = localIdx;
    int guard = 0;
    while (idx.size() >= 3 && guard < 1000) {
        guard++;
        bool clipped = false;
        for (size_t i = 0; i < idx.size(); ++i) {
            uint16_t i0 = idx[(i + idx.size() - 1) % idx.size()];
            uint16_t i1 = idx[i];
            uint16_t i2 = idx[(i + 1) % idx.size()];
            const Vec2& a = poly[i0];
            const Vec2& b = poly[i1];
            const Vec2& c = poly[i2];
            float cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
            if (clockwise ? cross > -1e-5f : cross < 1e-5f) continue;
            bool ear = true;
            for (uint16_t k : idx) {
                if (k == i0 || k == i1 || k == i2) continue;
                const Vec2& p = poly[k];
                float w1 = ( (a.x - p.x) * (b.y - a.y) - (a.y - p.y)*(b.x - a.x) );
                float w2 = ( (b.x - p.x) * (c.y - b.y) - (b.y - p.y)*(c.x - b.x) );
                float w3 = ( (c.x - p.x) * (a.y - c.y) - (c.y - p.y)*(a.x - c.x) );
                if ( (clockwise ? (w1<=0 && w2<=0 && w3<=0) : (w1>=0 && w2>=0 && w3>=0)) ) {
                    ear = false; break;
                }
            }
            if (ear) {
                out.push_back(i0);
                out.push_back(i1);
                out.push_back(i2);
                idx.erase(idx.begin() + i);
                clipped = true;
                break;
            }
        }
        if (!clipped) return false;
    }
    return out.size() % 3 == 0;
}

static void rebuildWorldMesh(const EditorState& state, Mesh3D& mesh,
                             float floorHeight = 0.0f, float ceilingHeight = 3.0f) {
    mesh.vertices.clear();
    mesh.normals.clear();
    mesh.colors.clear();
    mesh.uvs.clear();
    mesh.indices.clear();
    mesh.floorIndexStart = mesh.floorIndexCount = 0;
    mesh.ceilingIndexStart = mesh.ceilingIndexCount = 0;
    mesh.wallIndexStart = mesh.wallIndexCount = 0;

    uint16_t baseIndex = 0;

    auto addVertex = [&](float x, float y, float z, float nx, float ny, float nz, float r, float g, float b, float u, float v) -> uint16_t {
        mesh.vertices.push_back(x);
        mesh.vertices.push_back(y);
        mesh.vertices.push_back(z);
        mesh.normals.push_back(nx);
        mesh.normals.push_back(ny);
        mesh.normals.push_back(nz);
        mesh.colors.push_back(r);
        mesh.colors.push_back(g);
        mesh.colors.push_back(b);
        mesh.uvs.push_back(u);
        mesh.uvs.push_back(v);
        return baseIndex++;
    };

    for (const auto& sector : state.sectors) {
        if (sector.vertices.size() < 3)
            continue;

        std::vector<uint16_t> floorLocal;
        std::vector<uint16_t> ceilLocal;
        std::vector<Vec2> poly2d;
        for (int idx : sector.vertices) {
            if (idx < 0 || idx >= static_cast<int>(state.vertices.size()))
                continue;
            const auto& v = state.vertices[idx];
            float u = v.first * 0.25f;
            float vv = v.second * 0.25f;
            floorLocal.push_back(addVertex(v.first, v.second, floorHeight, 0.0f, 0.0f, 1.0f, 0.5f, 0.35f, 0.2f, u, vv));
            ceilLocal.push_back(addVertex(v.first, v.second, ceilingHeight, 0.0f, 0.0f, -1.0f, 0.65f, 0.65f, 0.7f, u, vv));
            poly2d.push_back({v.first, v.second});
        }

        std::vector<uint16_t> triFloor;
        if (!earClip(poly2d, floorLocal, triFloor)) {
            // fallback skip
        }
        size_t floorStart = mesh.indices.size();
        for (uint16_t t : triFloor) mesh.indices.push_back(t);
        size_t addedFloor = mesh.indices.size() - floorStart;
        if (addedFloor > 0) {
            if (mesh.floorIndexCount == 0) mesh.floorIndexStart = floorStart;
            mesh.floorIndexCount += addedFloor;
        }

        std::vector<uint16_t> triCeil;
        if (!earClip(poly2d, ceilLocal, triCeil)) {
            // fallback skip
        }
        size_t ceilStart = mesh.indices.size();
        // reverse winding for ceiling
        for (size_t i = 0; i + 2 < triCeil.size(); i += 3) {
            mesh.indices.push_back(triCeil[i]);
            mesh.indices.push_back(triCeil[i + 2]);
            mesh.indices.push_back(triCeil[i + 1]);
        }
        size_t addedCeil = mesh.indices.size() - ceilStart;
        if (addedCeil > 0) {
            if (mesh.ceilingIndexCount == 0) mesh.ceilingIndexStart = ceilStart;
            mesh.ceilingIndexCount += addedCeil;
        }

        size_t wallStart = mesh.indices.size();
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
            float uA = 0.0f;
            float uB = len;
            float vLower = floorHeight / ceilingHeight;
            float vUpper = 1.0f;

            uint16_t a0 = addVertex(vA.first, vA.second, floorHeight, nx, ny, 0.0f, 0.6f, 0.6f, 0.6f, uA, vLower);
            uint16_t b0 = addVertex(vB.first, vB.second, floorHeight, nx, ny, 0.0f, 0.6f, 0.6f, 0.6f, uB, vLower);
            uint16_t b1 = addVertex(vB.first, vB.second, ceilingHeight, nx, ny, 0.0f, 0.6f, 0.6f, 0.6f, uB, vUpper);
            uint16_t a1 = addVertex(vA.first, vA.second, ceilingHeight, nx, ny, 0.0f, 0.6f, 0.6f, 0.6f, uA, vUpper);

            mesh.indices.push_back(a0);
            mesh.indices.push_back(b0);
            mesh.indices.push_back(b1);
            mesh.indices.push_back(a0);
            mesh.indices.push_back(b1);
            mesh.indices.push_back(a1);
        }
        size_t addedWall = mesh.indices.size() - wallStart;
        if (addedWall > 0) {
            if (mesh.wallIndexCount == 0) mesh.wallIndexStart = wallStart;
            mesh.wallIndexCount += addedWall;
        }
    }

    std::printf("world mesh: %zu verts, %zu tris\n",
                mesh.vertices.size() / 3,
                mesh.indices.size() / 3);
}
static void buildSectorsFromLinedefs(EditorState& state) {
    state.sectors.clear();
    if (state.vertices.size() < 3 || state.lines.size() < 3)
        return;

    std::vector<std::vector<int>> adjacency(state.vertices.size());
    std::map<std::pair<int, int>, bool> edgeUsed;

    auto normKey = [](int a, int b) {
        return std::make_pair(std::min(a, b), std::max(a, b));
    };

    for (const auto& line : state.lines) {
        if (line.v1 < 0 || line.v2 < 0 ||
            line.v1 >= static_cast<int>(state.vertices.size()) ||
            line.v2 >= static_cast<int>(state.vertices.size()) ||
            line.v1 == line.v2) {
            continue;
        }
        adjacency[line.v1].push_back(line.v2);
        adjacency[line.v2].push_back(line.v1);
        edgeUsed[normKey(line.v1, line.v2)] = false;
    }

    for (const auto& kv : edgeUsed) {
        int start = kv.first.first;
        int next = kv.first.second;
        if (edgeUsed[kv.first])
            continue;

        std::vector<int> loop;
        std::vector<std::pair<int, int>> usedEdges;
        loop.push_back(start);
        loop.push_back(next);
        edgeUsed[kv.first] = true;
        usedEdges.push_back(kv.first);
        int prev = start;
        int curr = next;
        bool closed = false;

        while (true) {
            if (curr == start) { closed = true; break; }
            bool advanced = false;
            for (int neigh : adjacency[curr]) {
                if (neigh == prev) continue;
                auto k = normKey(curr, neigh);
                if (edgeUsed.find(k) == edgeUsed.end() || edgeUsed[k]) continue;
                edgeUsed[k] = true;
                usedEdges.push_back(k);
                loop.push_back(neigh);
                prev = curr;
                curr = neigh;
                advanced = true;
                break;
            }
            if (!advanced) break;
        }

        if (!closed || loop.size() < 3) {
            for (auto& e : usedEdges) edgeUsed[e] = false;
            continue;
        }

        // dedup closing vertex if present
        if (loop.front() == loop.back()) loop.pop_back();
        // unique check
        std::set<int> uniq(loop.begin(), loop.end());
        if (uniq.size() < 3) {
            for (auto& e : usedEdges) edgeUsed[e] = false;
            continue;
        }

        // Preserve loop order; ensure clockwise orientation
        std::vector<int> ordered = loop;
        float area = 0.0f;
        for (size_t i = 0; i < ordered.size(); ++i) {
            const auto& a = state.vertices[ordered[i]];
            const auto& b = state.vertices[ordered[(i + 1) % ordered.size()]];
            area += a.first * b.second - b.first * a.second;
        }
        if (area > 0.0f) {
            std::reverse(ordered.begin(), ordered.end());
        }

        // self-intersection check
        std::vector<Vec2> poly;
        for (int idx : ordered) poly.push_back({state.vertices[idx].first, state.vertices[idx].second});
        if (polygonSelfIntersects(poly)) {
            std::printf("Skipping self-intersecting sector\n");
            continue;
        }

        Sector s;
        s.vertices = ordered;
        state.sectors.push_back(std::move(s));
    }
}

extern "C" {

void userAppInit(void) {
    nxlinkStdio();
    socketInitializeDefault();
    romfsInit();
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("userAppInit(): nxlink ready\n");
}

void userAppExit(void) {
    romfsExit();
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
    buildSectorsFromLinedefs(state);
    rebuildWorldMesh(state, state.worldMesh);
    Camera3D fpsCamera;
    GLuint texWall = loadTextureFromPNG("romfs:/data/wall.png");
    GLuint texFloor = loadTextureFromPNG("romfs:/data/floor.png");
    GLuint texCeil = loadTextureFromPNG("romfs:/data/ceiling.png");
    renderer.setTextures(texFloor, texWall, texCeil);
    GLuint texEnemySprite = loadTextureFromPNG("romfs:/data/enemy_wizard.png");
    GLuint texProjSprite = loadTextureFromPNG("romfs:/data/projectile_orb.png");
    GLuint texItemHealth = loadTextureFromPNG("romfs:/data/item_health.png");
    GLuint texItemMana   = loadTextureFromPNG("romfs:/data/item_mana.png");
    GLuint texBlockFlash = loadTextureFromPNG("romfs:/data/block_flash.png");
    renderer.setBillboardTextures(texEnemySprite, texProjSprite);
    renderer.setItemTextures(texItemHealth, texItemMana);
    renderer.setEffectTextures(texBlockFlash);

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
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP) {
                        if (!state.entityMode) {
                            state.entityMode = true;
                        } else {
                            state.entityBrush = nextEntityBrush(state.entityBrush);
                        }
                        std::printf("Entity brush: %s\n", entityTypeName(state.entityBrush));
                    }
                    if (ev.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
                        state.entityMode = false;
                        state.selectedEntity = -1;
                        state.hoveredEntity = -1;
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
                bool foundStart = false;
                for (const auto& e : state.entities) {
                    if (e.type == EntityType::PlayerStart) {
                        fpsCamera.x = e.x;
                        fpsCamera.y = e.y;
                        fpsCamera.z = 1.7f;
                        foundStart = true;
                        break;
                    }
                }
                if (!foundStart) {
                    fpsCamera.x = 4.0f;
                    fpsCamera.y = 3.0f;
                    fpsCamera.z = 1.7f;
                }
                state.enemies.clear();
                for (const auto& e : state.entities) {
                    if (e.type == EntityType::EnemyWizard || e.type == EntityType::EnemySpawn) {
                        state.enemies.push_back({ e.x, e.y, 1.4f, 0.0f, true });
                    }
                }
                state.items.clear();
                for (const auto& e : state.entities) {
                    if (e.type == EntityType::ItemPickup) {
                        state.items.push_back({ e.x, e.y, 1.0f, e.type, true });
                    }
                }
                state.projectiles.active.clear();
                state.blocking = false;
                state.blockFlashTimer = 0.0f;
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

            state.blocking = SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);

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

            float newX = fpsCamera.x;
            float newY = fpsCamera.y;
            if (!state.blocking) {
                newX += (forwardX * moveForward + rightX * moveStrafe) * moveSpeed * dt;
                newY += (forwardY * moveForward + rightY * moveStrafe) * moveSpeed * dt;
            }

            const float radius = fpsCamera.radius;
            for (const auto& line : state.lines) {
                if (line.v1 < 0 || line.v2 < 0 ||
                    line.v1 >= static_cast<int>(state.vertices.size()) ||
                    line.v2 >= static_cast<int>(state.vertices.size())) {
                    continue;
                }
                const auto& a = state.vertices[line.v1];
                const auto& b = state.vertices[line.v2];
                Vec2 nearest = closestPointOnSegment(newX, newY, a.first, a.second, b.first, b.second);
                float dist = distance2D(newX, newY, nearest.x, nearest.y);
                if (dist < radius && dist > 0.0001f) {
                    float dx = newX - nearest.x;
                    float dy = newY - nearest.y;
                    float len = std::sqrt(dx * dx + dy * dy);
                    dx /= len;
                    dy /= len;
                    newX = nearest.x + dx * radius;
                    newY = nearest.y + dy * radius;
                }
            }

            fpsCamera.x = newX;
            fpsCamera.y = newY;
            fpsCamera.z = std::clamp(fpsCamera.z, 0.0f + 1.6f, 3.0f - 0.1f);
        }

        if (state.playMode) {
            // Enemy AI
            for (auto& e : state.enemies) {
                if (!e.alive) continue;
                e.cooldown -= dt;
                if (e.cooldown <= 0.0f) {
                    float dx = fpsCamera.x - e.x;
                    float dy = fpsCamera.y - e.y;
                    float len = std::sqrt(dx * dx + dy * dy);
                    if (len > 0.0001f) {
                        dx /= len;
                        dy /= len;
                        spawnProjectile(state, e.x, e.y, e.z, dx * 4.0f, dy * 4.0f, 0.0f, false);
                    }
                    e.cooldown = 2.0f;
                }
            }

            // Projectiles
            const float projectileRadius = 0.12f;
            for (auto& p : state.projectiles.active) {
                if (!p.alive) continue;
                p.x += p.vx * dt;
                p.y += p.vy * dt;
                p.z += p.vz * dt;

                // wall collision
                for (const auto& line : state.lines) {
                    if (line.v1 < 0 || line.v2 < 0 ||
                        line.v1 >= static_cast<int>(state.vertices.size()) ||
                        line.v2 >= static_cast<int>(state.vertices.size())) {
                        continue;
                    }
                    const auto& a = state.vertices[line.v1];
                    const auto& b = state.vertices[line.v2];
                    Vec2 nearest = closestPointOnSegment(p.x, p.y, a.first, a.second, b.first, b.second);
                    float dist = distance2D(p.x, p.y, nearest.x, nearest.y);
                    if (dist < projectileRadius) {
                        float nx = b.second - a.second;
                        float ny = -(b.first - a.first);
                        float nlen = std::sqrt(nx * nx + ny * ny);
                        if (nlen > 0.0001f) {
                            nx /= nlen; ny /= nlen;
                        } else {
                            nx = 0.0f; ny = 1.0f;
                        }
                        if (p.fromPlayer || state.blocking) {
                            p.fromPlayer = true;
                            float dot = p.vx * nx + p.vy * ny;
                            p.vx = p.vx - 2.0f * dot * nx;
                            p.vy = p.vy - 2.0f * dot * ny;
                            float push = projectileRadius - dist;
                            p.x += nx * push;
                            p.y += ny * push;
                        } else {
                            p.alive = false;
                        }
                        break;
                    }
                }

                if (!p.alive) continue;

                // hit player
                float dp = distance2D(p.x, p.y, fpsCamera.x, fpsCamera.y);
                if (dp < projectileRadius + fpsCamera.radius) {
                    if (state.blocking) {
                        p.fromPlayer = true;
                        float nx = (p.x - fpsCamera.x);
                        float ny = (p.y - fpsCamera.y);
                        float nlen = std::sqrt(nx * nx + ny * ny);
                        if (nlen > 0.0001f) { nx /= nlen; ny /= nlen; }
                        float dot = p.vx * nx + p.vy * ny;
                        p.vx = (p.vx - 2.0f * dot * nx) * 1.2f;
                        p.vy = (p.vy - 2.0f * dot * ny) * 1.2f;
                        p.vz = -p.vz * 1.2f;
                        float push = (projectileRadius + fpsCamera.radius) - dp;
                        p.x += nx * push;
                        p.y += ny * push;
                        state.blockFlashTimer = 0.15f;
                    } else {
                        std::printf("Player hit! Returning to editor.\n");
                        state.playMode = false;
                        state.projectiles.active.clear();
                        state.enemies.clear();
                        state.blocking = false;
                        break;
                    }
                }

                // hit enemy
                if (p.fromPlayer) {
                    for (auto& e : state.enemies) {
                        if (!e.alive) continue;
                        float de = distance2D(p.x, p.y, e.x, e.y);
                        if (de < projectileRadius + 0.25f) {
                            e.alive = false;
                            p.alive = false;
                            break;
                        }
                    }
                }
            }

            state.projectiles.active.erase(
                std::remove_if(state.projectiles.active.begin(), state.projectiles.active.end(),
                               [](const Projectile& p) { return !p.alive; }),
                state.projectiles.active.end());
            state.enemies.erase(
                std::remove_if(state.enemies.begin(), state.enemies.end(),
                               [](const EnemyWizard& e) { return !e.alive; }),
                state.enemies.end());
            for (auto& it : state.items) {
                if (!it.alive) continue;
                float d = distance2D(fpsCamera.x, fpsCamera.y, it.x, it.y);
                if (d < 0.6f) {
                    it.alive = false;
                    std::printf("Picked up item!\n");
                }
            }
            if (state.blockFlashTimer > 0.0f) {
                state.blockFlashTimer -= dt;
                if (state.blockFlashTimer < 0.0f) state.blockFlashTimer = 0.0f;
            }
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
            if (state.entityMode) {
                if (state.hoveredEntity >= 0 && state.hoveredEntity < static_cast<int>(state.entities.size())) {
                    state.entities.erase(state.entities.begin() + state.hoveredEntity);
                    state.hoveredEntity = -1;
                    state.selectedEntity = -1;
                }
            } else {
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
        }

        int placedVertexIndex = -1;
        if (!state.playMode && placePressed) {
            if (state.entityMode) {
                Entity newEnt;
                newEnt.x = state.cursorX;
                newEnt.y = state.cursorY;
                newEnt.type = state.entityBrush;
                if (newEnt.type == EntityType::PlayerStart) {
                    for (auto it = state.entities.begin(); it != state.entities.end();) {
                        if (it->type == EntityType::PlayerStart)
                            it = state.entities.erase(it);
                        else
                            ++it;
                    }
                }
                state.entities.push_back(newEnt);
                state.selectedEntity = static_cast<int>(state.entities.size() - 1);
            } else {
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
        }

        if (!state.playMode && selectPressed) {
            if (state.entityMode) {
                if (state.hoveredEntity >= 0) {
                    state.selectedEntity = state.hoveredEntity;
                } else {
                    state.selectedEntity = -1;
                }
            } else {
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
        }

        if (!state.playMode) {
            if (needRebuild) {
                buildSectorsFromLinedefs(state);
                rebuildWorldMesh(state, state.worldMesh);
                needRebuild = false;
            }

            renderer.beginFrame();
            renderer.drawGrid(camera, 1.0f);    // 1 unit = 1 grid cell

            // Entities
            state.hoveredEntity = -1;
            const float entityHoverRadius = 0.3f;
            float bestEDist2 = entityHoverRadius * entityHoverRadius;
            for (size_t i = 0; i < state.entities.size(); ++i) {
                float dx = state.cursorX - state.entities[i].x;
                float dy = state.cursorY - state.entities[i].y;
                float d2 = dx * dx + dy * dy;
                if (d2 <= bestEDist2) {
                    bestEDist2 = d2;
                    state.hoveredEntity = static_cast<int>(i);
                }
            }

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

            auto drawEntity2D = [&](const Entity& e, bool hovered, bool selected) {
                float size = 0.18f;
                float r = 0.0f, g = 0.0f, b = 0.0f;
                switch (e.type) {
                    case EntityType::PlayerStart: r = 0.1f; g = 0.8f; b = 0.1f; break;
                    case EntityType::EnemyWizard: r = 0.7f; g = 0.2f; b = 0.9f; break;
                    case EntityType::EnemySpawn:  r = 0.9f; g = 0.1f; b = 0.1f; break;
                    case EntityType::ItemPickup:  r = 0.1f; g = 0.4f; b = 0.9f; break;
                }
                if (selected) { r = 1.0f; g = 0.5f; b = 0.0f; size *= 1.1f; }
                else if (hovered) { r = 1.0f; g = 1.0f; b = 0.0f; size *= 1.05f; }

                switch (e.type) {
                    case EntityType::PlayerStart:
                        renderer.drawPoint2D(e.x, e.y, size, r, g, b);
                        break;
                    case EntityType::EnemyWizard: {
                        renderer.drawLine2D(e.x - size, e.y, e.x, e.y + size, r, g, b);
                        renderer.drawLine2D(e.x, e.y + size, e.x + size, e.y, r, g, b);
                        renderer.drawLine2D(e.x + size, e.y, e.x, e.y - size, r, g, b);
                        renderer.drawLine2D(e.x, e.y - size, e.x - size, e.y, r, g, b);
                        break;
                    }
                    case EntityType::EnemySpawn: {
                        float h = size;
                        renderer.drawLine2D(e.x - size, e.y - size, e.x, e.y + h, r, g, b);
                        renderer.drawLine2D(e.x, e.y + h, e.x + size, e.y - size, r, g, b);
                        renderer.drawLine2D(e.x + size, e.y - size, e.x - size, e.y - size, r, g, b);
                        break;
                    }
                    case EntityType::ItemPickup: {
                        renderer.drawLine2D(e.x, e.y + size, e.x + size, e.y, r, g, b);
                        renderer.drawLine2D(e.x + size, e.y, e.x, e.y - size, r, g, b);
                        renderer.drawLine2D(e.x, e.y - size, e.x - size, e.y, r, g, b);
                        renderer.drawLine2D(e.x - size, e.y, e.x, e.y + size, r, g, b);
                        break;
                    }
                }
            };

            for (size_t i = 0; i < state.entities.size(); ++i) {
                bool hovered = (static_cast<int>(i) == state.hoveredEntity);
                bool selected = (static_cast<int>(i) == state.selectedEntity);
                drawEntity2D(state.entities[i], hovered, selected);
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
                                1.0f, 0.2f, 0.8f);
            renderer.drawLine2D(state.cursorX, state.cursorY - cursorSize,
                                state.cursorX, state.cursorY + cursorSize,
                                1.0f, 0.2f, 0.8f);
            renderer.endFrame(window);
        } else {
            renderer.drawMesh3D(state.worldMesh, fpsCamera);
            for (const auto& p : state.projectiles.active) {
                if (!p.alive) continue;
                renderer.drawBillboard3D(fpsCamera, p.x, p.y, p.z, 0.35f, texProjSprite,
                                         p.fromPlayer ? 0.8f : 0.2f,
                                         p.fromPlayer ? 0.9f : 0.2f,
                                         p.fromPlayer ? 1.0f : 0.1f);
            }
            for (const auto& e : state.enemies) {
                if (!e.alive) continue;
                renderer.drawBillboard3D(fpsCamera, e.x, e.y, e.z, 1.2f, texEnemySprite,
                                         1.0f, 1.0f, 1.0f);
            }
            for (const auto& it : state.items) {
                if (!it.alive) continue;
                GLuint tex = texItemMana;
                renderer.drawBillboard3D(fpsCamera, it.x, it.y, it.z, 0.6f, tex, 1.0f, 1.0f, 1.0f);
            }
            if (state.blockFlashTimer > 0.0f) {
                renderer.drawBillboard3D(fpsCamera, fpsCamera.x, fpsCamera.y, fpsCamera.z + 0.2f, 1.3f, texBlockFlash, 1.0f, 1.0f, 1.0f);
            }
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

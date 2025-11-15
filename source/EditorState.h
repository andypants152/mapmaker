// EditorState.h
#pragma once

#include <utility>
#include <vector>
#include "Mesh3D.h"

struct LineDef {
    int v1 = -1;
    int v2 = -1;
};

struct Sector {
    std::vector<int> vertices;
};

struct Camera3D {
    float x = 0.0f;
    float y = 0.0f;
    float z = 1.7f;
    float yaw = 0.0f;
    float pitch = 0.0f;
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
    std::vector<Sector> sectors;
    Mesh3D worldMesh;
    int hoveredVertex = -1;
    int selectedVertex = -1;
    bool wallMode = false;
    bool playMode = false;
};

// EditorState.h
#pragma once

#include <utility>
#include <vector>

struct LineDef {
    int v1 = -1;
    int v2 = -1;
};

struct Sector {
    std::vector<int> vertices;
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
    int hoveredVertex = -1;
    int selectedVertex = -1;
    bool wallMode = false;
};

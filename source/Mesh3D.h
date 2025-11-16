// Mesh3D.h
#pragma once

#include <vector>

struct Mesh3D {
    std::vector<float> vertices; // x,y,z
    std::vector<float> normals;  // x,y,z
    std::vector<float> colors;   // r,g,b
    std::vector<float> uvs;      // u,v
    std::vector<uint16_t> indices;
    size_t floorIndexStart = 0;
    size_t floorIndexCount = 0;
    size_t ceilingIndexStart = 0;
    size_t ceilingIndexCount = 0;
    size_t wallIndexStart = 0;
    size_t wallIndexCount = 0;
};

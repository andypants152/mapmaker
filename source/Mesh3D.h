// Mesh3D.h
#pragma once

#include <vector>

struct Mesh3D {
    std::vector<float> vertices; // x,y,z
    std::vector<float> normals;  // x,y,z
    std::vector<float> colors;   // r,g,b
    std::vector<uint16_t> indices;
};

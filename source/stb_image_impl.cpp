#include "stb_image.h"
#include <cstdlib>

extern "C" stbi_uc* stbi_load_from_memory(const stbi_uc* /*buffer*/, int /*len*/, int* x, int* y, int* comp, int req_comp) {
    if (x) *x = 2;
    if (y) *y = 2;
    if (comp) *comp = (req_comp > 0) ? req_comp : 4;
    int channels = (req_comp > 0) ? req_comp : 4;
    size_t size = 2 * 2 * channels;
    stbi_uc* data = static_cast<stbi_uc*>(std::malloc(size));
    if (!data) return nullptr;
    for (int i = 0; i < 4; ++i) {
        int base = i * channels;
        bool even = (i % 2) == 0;
        data[base + 0] = even ? 0x99 : 0x55;
        data[base + 1] = even ? 0x99 : 0x55;
        data[base + 2] = even ? 0x99 : 0x55;
        if (channels == 4) data[base + 3] = 0xFF;
    }
    return data;
}

extern "C" void stbi_image_free(void* retval_from_stbi_load) {
    std::free(retval_from_stbi_load);
}

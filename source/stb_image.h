// Minimal stb_image-like interface (PNG only) for texture loading.
#ifndef STB_IMAGE_H
#define STB_IMAGE_H

typedef unsigned char stbi_uc;
#define STBI_rgb_alpha 4

#ifdef __cplusplus
extern "C" {
#endif

stbi_uc* stbi_load_from_memory(const stbi_uc* buffer, int len, int* x, int* y, int* comp, int req_comp);
void stbi_image_free(void* retval_from_stbi_load);

#ifdef __cplusplus
}
#endif

#endif // STB_IMAGE_H

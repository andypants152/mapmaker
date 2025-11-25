// RendererGL.cpp
#include "RendererGL.h"
#include "EditorState.h"
#include "Mesh3D.h"
#include "Platform.h"
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_NO_SIMD
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_LINEAR
#include "stb_image.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <array>
#include <limits>
#include <cctype>
#include <cstring>
#include <cstdint>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
// Load core GL entry points dynamically (Windows opengl32 exports only 1.1)
static PFNGLDELETEBUFFERSPROC       p_glDeleteBuffers       = nullptr;
static PFNGLDELETEPROGRAMPROC       p_glDeleteProgram       = nullptr;
static PFNGLUSEPROGRAMPROC          p_glUseProgram          = nullptr;
static PFNGLUNIFORM4FPROC           p_glUniform4f           = nullptr;
static PFNGLBINDBUFFERPROC          p_glBindBuffer          = nullptr;
static PFNGLBUFFERDATAPROC          p_glBufferData          = nullptr;
static PFNGLENABLEVERTEXATTRIBARRAYPROC p_glEnableVertexAttribArray = nullptr;
static PFNGLVERTEXATTRIBPOINTERPROC p_glVertexAttribPointer = nullptr;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC p_glDisableVertexAttribArray = nullptr;
static PFNGLUNIFORMMATRIX4FVPROC    p_glUniformMatrix4fv    = nullptr;
static PFNGLUNIFORM1IPROC           p_glUniform1i           = nullptr;
static PFNGLACTIVETEXTUREPROC       p_glActiveTexture       = nullptr;
static PFNGLCREATESHADERPROC        p_glCreateShader        = nullptr;
static PFNGLSHADERSOURCEPROC        p_glShaderSource        = nullptr;
static PFNGLCOMPILESHADERPROC       p_glCompileShader       = nullptr;
static PFNGLGETSHADERIVPROC         p_glGetShaderiv         = nullptr;
static PFNGLGETSHADERINFOLOGPROC    p_glGetShaderInfoLog    = nullptr;
static PFNGLDELETESHADERPROC        p_glDeleteShader        = nullptr;
static PFNGLCREATEPROGRAMPROC       p_glCreateProgram       = nullptr;
static PFNGLATTACHSHADERPROC        p_glAttachShader        = nullptr;
static PFNGLLINKPROGRAMPROC         p_glLinkProgram         = nullptr;
static PFNGLGETPROGRAMIVPROC        p_glGetProgramiv        = nullptr;
static PFNGLGETPROGRAMINFOLOGPROC   p_glGetProgramInfoLog   = nullptr;
static PFNGLGETATTRIBLOCATIONPROC   p_glGetAttribLocation   = nullptr;
static PFNGLGETUNIFORMLOCATIONPROC  p_glGetUniformLocation  = nullptr;
static PFNGLGENBUFFERSPROC          p_glGenBuffers          = nullptr;

#define glDeleteBuffers p_glDeleteBuffers
#define glDeleteProgram p_glDeleteProgram
#define glUseProgram p_glUseProgram
#define glUniform4f p_glUniform4f
#define glBindBuffer p_glBindBuffer
#define glBufferData p_glBufferData
#define glEnableVertexAttribArray p_glEnableVertexAttribArray
#define glVertexAttribPointer p_glVertexAttribPointer
#define glDisableVertexAttribArray p_glDisableVertexAttribArray
#define glUniformMatrix4fv p_glUniformMatrix4fv
#define glUniform1i p_glUniform1i
#define glActiveTexture p_glActiveTexture
#define glCreateShader p_glCreateShader
#define glShaderSource p_glShaderSource
#define glCompileShader p_glCompileShader
#define glGetShaderiv p_glGetShaderiv
#define glGetShaderInfoLog p_glGetShaderInfoLog
#define glDeleteShader p_glDeleteShader
#define glCreateProgram p_glCreateProgram
#define glAttachShader p_glAttachShader
#define glLinkProgram p_glLinkProgram
#define glGetProgramiv p_glGetProgramiv
#define glGetProgramInfoLog p_glGetProgramInfoLog
#define glGetAttribLocation p_glGetAttribLocation
#define glGetUniformLocation p_glGetUniformLocation
#define glGenBuffers p_glGenBuffers

static bool loadGLFunctions() {
    p_glDeleteBuffers        = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(SDL_GL_GetProcAddress("glDeleteBuffers"));
    p_glDeleteProgram        = reinterpret_cast<PFNGLDELETEPROGRAMPROC>(SDL_GL_GetProcAddress("glDeleteProgram"));
    p_glUseProgram           = reinterpret_cast<PFNGLUSEPROGRAMPROC>(SDL_GL_GetProcAddress("glUseProgram"));
    p_glUniform4f            = reinterpret_cast<PFNGLUNIFORM4FPROC>(SDL_GL_GetProcAddress("glUniform4f"));
    p_glBindBuffer           = reinterpret_cast<PFNGLBINDBUFFERPROC>(SDL_GL_GetProcAddress("glBindBuffer"));
    p_glBufferData           = reinterpret_cast<PFNGLBUFFERDATAPROC>(SDL_GL_GetProcAddress("glBufferData"));
    p_glEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glEnableVertexAttribArray"));
    p_glVertexAttribPointer  = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(SDL_GL_GetProcAddress("glVertexAttribPointer"));
    p_glDisableVertexAttribArray = reinterpret_cast<PFNGLDISABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glDisableVertexAttribArray"));
    p_glUniformMatrix4fv     = reinterpret_cast<PFNGLUNIFORMMATRIX4FVPROC>(SDL_GL_GetProcAddress("glUniformMatrix4fv"));
    p_glUniform1i            = reinterpret_cast<PFNGLUNIFORM1IPROC>(SDL_GL_GetProcAddress("glUniform1i"));
    p_glActiveTexture        = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(SDL_GL_GetProcAddress("glActiveTexture"));
    p_glCreateShader         = reinterpret_cast<PFNGLCREATESHADERPROC>(SDL_GL_GetProcAddress("glCreateShader"));
    p_glShaderSource         = reinterpret_cast<PFNGLSHADERSOURCEPROC>(SDL_GL_GetProcAddress("glShaderSource"));
    p_glCompileShader        = reinterpret_cast<PFNGLCOMPILESHADERPROC>(SDL_GL_GetProcAddress("glCompileShader"));
    p_glGetShaderiv          = reinterpret_cast<PFNGLGETSHADERIVPROC>(SDL_GL_GetProcAddress("glGetShaderiv"));
    p_glGetShaderInfoLog     = reinterpret_cast<PFNGLGETSHADERINFOLOGPROC>(SDL_GL_GetProcAddress("glGetShaderInfoLog"));
    p_glDeleteShader         = reinterpret_cast<PFNGLDELETESHADERPROC>(SDL_GL_GetProcAddress("glDeleteShader"));
    p_glCreateProgram        = reinterpret_cast<PFNGLCREATEPROGRAMPROC>(SDL_GL_GetProcAddress("glCreateProgram"));
    p_glAttachShader         = reinterpret_cast<PFNGLATTACHSHADERPROC>(SDL_GL_GetProcAddress("glAttachShader"));
    p_glLinkProgram          = reinterpret_cast<PFNGLLINKPROGRAMPROC>(SDL_GL_GetProcAddress("glLinkProgram"));
    p_glGetProgramiv         = reinterpret_cast<PFNGLGETPROGRAMIVPROC>(SDL_GL_GetProcAddress("glGetProgramiv"));
    p_glGetProgramInfoLog    = reinterpret_cast<PFNGLGETPROGRAMINFOLOGPROC>(SDL_GL_GetProcAddress("glGetProgramInfoLog"));
    p_glGetAttribLocation    = reinterpret_cast<PFNGLGETATTRIBLOCATIONPROC>(SDL_GL_GetProcAddress("glGetAttribLocation"));
    p_glGetUniformLocation   = reinterpret_cast<PFNGLGETUNIFORMLOCATIONPROC>(SDL_GL_GetProcAddress("glGetUniformLocation"));
    p_glGenBuffers           = reinterpret_cast<PFNGLGENBUFFERSPROC>(SDL_GL_GetProcAddress("glGenBuffers"));

    return p_glDeleteBuffers && p_glDeleteProgram && p_glUseProgram && p_glUniform4f &&
           p_glBindBuffer && p_glBufferData && p_glEnableVertexAttribArray && p_glVertexAttribPointer &&
           p_glDisableVertexAttribArray && p_glUniformMatrix4fv && p_glUniform1i && p_glActiveTexture &&
           p_glCreateShader && p_glShaderSource && p_glCompileShader && p_glGetShaderiv &&
           p_glGetShaderInfoLog && p_glDeleteShader && p_glCreateProgram && p_glAttachShader &&
           p_glLinkProgram && p_glGetProgramiv && p_glGetProgramInfoLog && p_glGetAttribLocation &&
           p_glGetUniformLocation && p_glGenBuffers;
}
#else
static bool loadGLFunctions() { return true; }
#endif

struct ClipVertex {
    float x;
    float y;
};

static bool earClip2D(const std::vector<ClipVertex>& poly, const std::vector<uint16_t>& localIdx, std::vector<uint16_t>& out) {
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
            const ClipVertex& a = poly[i0];
            const ClipVertex& b = poly[i1];
            const ClipVertex& c = poly[i2];
            float cross = (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x);
            if (clockwise ? cross > -1e-5f : cross < 1e-5f) continue;
            bool ear = true;
            for (uint16_t k : idx) {
                if (k == i0 || k == i1 || k == i2) continue;
                const ClipVertex& p = poly[k];
                float w1 = ((a.x - p.x) * (b.y - a.y) - (a.y - p.y) * (b.x - a.x));
                float w2 = ((b.x - p.x) * (c.y - b.y) - (b.y - p.y) * (c.x - b.x));
                float w3 = ((c.x - p.x) * (a.y - c.y) - (c.y - p.y) * (a.x - c.x));
                if (clockwise ? (w1 <= 0 && w2 <= 0 && w3 <= 0) : (w1 >= 0 && w2 >= 0 && w3 >= 0)) {
                    ear = false;
                    break;
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

static bool getGlyph(char c, std::array<uint8_t, 5>& out) {
    switch (std::toupper(static_cast<unsigned char>(c))) {
        case 'A': out = {0x7E,0x11,0x11,0x11,0x7E}; return true;
        case 'B': out = {0x7F,0x49,0x49,0x49,0x36}; return true;
        case 'C': out = {0x3E,0x41,0x41,0x41,0x22}; return true;
        case 'D': out = {0x7F,0x41,0x41,0x22,0x1C}; return true;
        case 'E': out = {0x7F,0x49,0x49,0x49,0x41}; return true;
        case 'F': out = {0x7F,0x09,0x09,0x09,0x01}; return true;
        case 'G': out = {0x3E,0x41,0x49,0x49,0x7A}; return true;
        case 'H': out = {0x7F,0x08,0x08,0x08,0x7F}; return true;
        case 'I': out = {0x00,0x41,0x7F,0x41,0x00}; return true;
        case 'J': out = {0x20,0x40,0x41,0x3F,0x01}; return true;
        case 'K': out = {0x7F,0x08,0x14,0x22,0x41}; return true;
        case 'L': out = {0x7F,0x40,0x40,0x40,0x40}; return true;
        case 'M': out = {0x7F,0x02,0x0C,0x02,0x7F}; return true;
        case 'N': out = {0x7F,0x04,0x08,0x10,0x7F}; return true;
        case 'O': out = {0x3E,0x41,0x41,0x41,0x3E}; return true;
        case 'P': out = {0x7F,0x09,0x09,0x09,0x06}; return true;
        case 'Q': out = {0x3E,0x41,0x51,0x21,0x5E}; return true;
        case 'R': out = {0x7F,0x09,0x19,0x29,0x46}; return true;
        case 'S': out = {0x26,0x49,0x49,0x49,0x32}; return true;
        case 'T': out = {0x01,0x01,0x7F,0x01,0x01}; return true;
        case 'U': out = {0x3F,0x40,0x40,0x40,0x3F}; return true;
        case 'V': out = {0x1F,0x20,0x40,0x20,0x1F}; return true;
        case 'W': out = {0x7F,0x20,0x18,0x20,0x7F}; return true;
        case 'X': out = {0x63,0x14,0x08,0x14,0x63}; return true;
        case 'Y': out = {0x03,0x04,0x78,0x04,0x03}; return true;
        case 'Z': out = {0x61,0x51,0x49,0x45,0x43}; return true;
        case '0': out = {0x3E,0x45,0x49,0x51,0x3E}; return true;
        case '1': out = {0x00,0x42,0x7F,0x40,0x00}; return true;
        case '2': out = {0x62,0x51,0x49,0x45,0x42}; return true;
        case '3': out = {0x22,0x41,0x49,0x49,0x36}; return true;
        case '4': out = {0x1C,0x12,0x7F,0x10,0x10}; return true;
        case '5': out = {0x27,0x45,0x45,0x45,0x39}; return true;
        case '6': out = {0x3E,0x49,0x49,0x49,0x32}; return true;
        case '7': out = {0x01,0x01,0x7D,0x03,0x01}; return true;
        case '8': out = {0x36,0x49,0x49,0x49,0x36}; return true;
        case '9': out = {0x26,0x49,0x49,0x49,0x3E}; return true;
        case ':': out = {0x00,0x36,0x36,0x00,0x00}; return true;
        case '.': out = {0x00,0x60,0x60,0x00,0x00}; return true;
        case ',': out = {0x00,0x40,0x60,0x00,0x00}; return true;
        case '(': out = {0x00,0x1C,0x22,0x41,0x00}; return true;
        case ')': out = {0x00,0x41,0x22,0x1C,0x00}; return true;
        case '-': out = {0x08,0x08,0x08,0x08,0x08}; return true;
        case '+': out = {0x08,0x08,0x3E,0x08,0x08}; return true;
        case '/': out = {0x40,0x30,0x0C,0x03,0x00}; return true;
        case ' ': out = {0x00,0x00,0x00,0x00,0x00}; return true;
        default: out = {0x00,0x00,0x00,0x00,0x00}; return true;
    }
}

static GLuint createFallbackTexture() {
    static const unsigned char fallback[16] = {
        0x99,0x99,0x99,0xFF, 0x55,0x55,0x55,0xFF,
        0x55,0x55,0x55,0xFF, 0x99,0x99,0x99,0xFF
    };
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint loadTextureFromPNG(const char* path) {
    std::vector<unsigned char> fileData;
    if (!PlatformReadFile(path, fileData) || fileData.empty()) {
        std::printf("Failed to read texture: %s (using fallback)\n", path);
        return createFallbackTexture();
    }

    int width = 0;
    int height = 0;
    int comp = 0;
    if (fileData.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        std::printf("Texture too large to load: %s (using fallback)\n", path);
        return createFallbackTexture();
    }

    stbi_uc* pixels = stbi_load_from_memory(fileData.data(), static_cast<int>(fileData.size()), &width, &height, &comp, STBI_rgb_alpha);
    if (!pixels) {
        const char* reason = stbi_failure_reason();
        std::printf("stbi_load_from_memory failed for %s: %s (using fallback)\n", path, reason ? reason : "unknown");
        return createFallbackTexture();
    }

    auto isPowerOfTwo = [](int n) { return (n & (n - 1)) == 0; };
    bool pow2 = width > 0 && height > 0 && isPowerOfTwo(width) && isPowerOfTwo(height);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // handle NPOT widths cleanly across GL/WebGL
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // WebGL forbids REPEAT on NPOT textures; clamp when not power-of-two.
    GLint wrapMode = (pow2) ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);

    std::printf("Loaded texture %s (%dx%d)\n", path, width, height);
    return tex;
}

RendererGL::RendererGL()
    : m_width(1280)
    , m_height(720)
    , m_camera{}
    , m_program(0)
    , m_attrPos(-1)
    , m_uniformColor(-1)
    , m_program3D(0)
    , m_attrPos3D(-1)
    , m_attrUV3D(-1)
    , m_uniformMVP(-1)
    , m_uniformTex(-1)
    , m_vbo(0)
    , m_vbo3DPos(0)
    , m_vbo3DUV(0)
    , m_ibo3D(0)
{
    m_camera.zoom = 1.0f;
}

RendererGL::~RendererGL() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vbo3DPos) glDeleteBuffers(1, &m_vbo3DPos);
    if (m_vbo3DUV) glDeleteBuffers(1, &m_vbo3DUV);
    if (m_ibo3D) glDeleteBuffers(1, &m_ibo3D);
    if (m_program) glDeleteProgram(m_program);
    if (m_program3D) glDeleteProgram(m_program3D);
}

bool RendererGL::init(SDL_Window* window) {
    if (!window) {
        std::printf("RendererGL::init called with null window\n");
        return false;
    }

#if !defined(__SWITCH__) && !defined(__EMSCRIPTEN__)
    if (!loadGLFunctions()) {
        std::printf("Failed to load desktop GL functions\n");
        return false;
    }
#endif

    if (!initGL())
        return false;

    glGenBuffers(1, &m_vbo);
    if (!m_vbo) {
        std::printf("Failed to create VBO for line rendering\n");
        return false;
    }

    glGenBuffers(1, &m_vbo3DPos);
    glGenBuffers(1, &m_vbo3DUV);
    glGenBuffers(1, &m_ibo3D);
    if (!m_vbo3DPos || !m_vbo3DUV || !m_ibo3D) {
        std::printf("Failed to create buffers for 3D rendering\n");
        return false;
    }

    glViewport(0, 0, m_width, m_height);
    return true;
}

void RendererGL::resize(int width, int height) {
    m_width = width;
    m_height = height;
    glViewport(0, 0, width, height);
}

void RendererGL::beginFrame() {
    glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
    glDisable(GL_DEPTH_TEST); // 2D editor rendering does not need depth
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void RendererGL::drawLine2D(float x1, float y1, float x2, float y2, float r, float g, float b) {
    if (m_camera.zoom <= 0.0f)
        return;

    const float verts[4] = {
        worldToClipX(x1), worldToClipY(y1),
        worldToClipX(x2), worldToClipY(y2),
    };

    glUseProgram(m_program);
    glUniform4f(m_uniformColor, r, g, b, 1.0f);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(m_attrPos);
    glVertexAttribPointer(m_attrPos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (const void*)0);

    glDrawArrays(GL_LINES, 0, 2);

    glDisableVertexAttribArray(m_attrPos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RendererGL::drawPoint2D(float x, float y, float size, float r, float g, float b) {
    if (size <= 0.0f)
        return;
    if (m_camera.zoom <= 0.0f)
        return;

    const float cx = worldToClipX(x);
    const float cy = worldToClipY(y);
    const float halfWidth = static_cast<float>(m_width) * 0.5f;
    const float halfHeight = static_cast<float>(m_height) * 0.5f;
    if (halfWidth <= 0.0f || halfHeight <= 0.0f)
        return;

    const float radiusX = size * m_camera.zoom / halfWidth;
    const float radiusY = size * m_camera.zoom / halfHeight;

    constexpr int segments = 20;
    float verts[(segments + 2) * 2];
    verts[0] = cx;
    verts[1] = cy;

    for (int i = 0; i <= segments; ++i) {
        float angle = (static_cast<float>(i) / segments) * 6.28318530718f;
        float xPos = cx + std::cos(angle) * radiusX;
        float yPos = cy + std::sin(angle) * radiusY;
        verts[(i + 1) * 2 + 0] = xPos;
        verts[(i + 1) * 2 + 1] = yPos;
    }

    glUseProgram(m_program);
    glUniform4f(m_uniformColor, r, g, b, 1.0f);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(m_attrPos);
    glVertexAttribPointer(m_attrPos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (const void*)0);

    glDrawArrays(GL_TRIANGLE_FAN, 0, segments + 2);

    glDisableVertexAttribArray(m_attrPos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RendererGL::drawSectorFill(const Sector& sector, const EditorState& state,
                                float r, float g, float b, float a) {
    if (sector.vertices.size() < 3)
        return;

    std::vector<ClipVertex> poly;
    poly.reserve(sector.vertices.size());
    std::vector<uint16_t> localIdx;
    localIdx.reserve(sector.vertices.size());

    for (int idx : sector.vertices) {
        if (idx < 0 || idx >= static_cast<int>(state.vertices.size()))
            return;
        const auto& v = state.vertices[idx];
        poly.push_back({ worldToClipX(v.first), worldToClipY(v.second) });
        localIdx.push_back(static_cast<uint16_t>(localIdx.size()));
    }

    std::vector<uint16_t> triIdx;
    bool triangulated = earClip2D(poly, localIdx, triIdx);

    std::vector<float> verts;
    GLenum primitive = GL_TRIANGLES;
    if (triangulated && !triIdx.empty()) {
        verts.reserve(triIdx.size() * 2);
        for (uint16_t i : triIdx) {
            const ClipVertex& p = poly[i];
            verts.push_back(p.x);
            verts.push_back(p.y);
        }
    } else {
        primitive = GL_TRIANGLE_FAN;
        verts.reserve(poly.size() * 2);
        for (const auto& p : poly) {
            verts.push_back(p.x);
            verts.push_back(p.y);
        }
    }

    if (verts.empty())
        return;

    glUseProgram(m_program);
    glUniform4f(m_uniformColor, r, g, b, a);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * verts.size(), verts.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(m_attrPos);
    glVertexAttribPointer(m_attrPos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (const void*)0);

    glDrawArrays(primitive, 0, static_cast<GLsizei>(verts.size() / 2));

    glDisableVertexAttribArray(m_attrPos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RendererGL::drawBillboard3D(const Camera3D& cam, float x, float y, float z, float size, GLuint tex, float r, float g, float b) {
    // Billboard uses camera-facing basis (yaw + pitch) so it stays anchored when looking up/down.
    glEnable(GL_DEPTH_TEST);

    const float cosYaw = std::cos(cam.yaw);
    const float sinYaw = std::sin(cam.yaw);
    const float cosPitch = std::cos(cam.pitch);
    const float sinPitch = std::sin(cam.pitch);
    float forwardX = cosPitch * sinYaw;
    float forwardY = cosPitch * cosYaw;
    float forwardZ = sinPitch;
    float rightX = cosYaw;
    float rightY = -sinYaw;
    float rightZ = 0.0f;
    float upX = -sinPitch * sinYaw;
    float upY = -sinPitch * cosYaw;
    float upZ = cosPitch;

    float hs = size * 0.5f;
    float positions[12] = {
        x - rightX * hs + upX * hs, y - rightY * hs + upY * hs, z + upZ * hs, // top-left
        x + rightX * hs + upX * hs, y + rightY * hs + upY * hs, z + upZ * hs, // top-right
        x + rightX * hs - upX * hs, y + rightY * hs - upY * hs, z - upZ * hs, // bottom-right
        x - rightX * hs - upX * hs, y - rightY * hs - upY * hs, z - upZ * hs, // bottom-left
    };
    float uvs[8] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
        0.0f, 0.0f
    };
    uint16_t indices[6] = {0,1,2, 0,2,3};

    float aspect = (m_height != 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    const float fov = 70.0f * 3.1415926535f / 180.0f;
    float f = 1.0f / std::tan(fov * 0.5f);
    const float nearPlane = 0.05f;
    const float farPlane = 500.0f;

    float proj[16] = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (farPlane + nearPlane) / (nearPlane - farPlane), -1,
        0, 0, (2 * farPlane * nearPlane) / (nearPlane - farPlane), 0
    };

    float view[16] = {
        rightX, upX, -forwardX, 0,
        rightY, upY, -forwardY, 0,
        rightZ, upZ, -forwardZ, 0,
        0,      0,    0,        1
    };

    view[12] = -(cam.x * rightX + cam.y * rightY + cam.z * rightZ);
    view[13] = -(cam.x * upX + cam.y * upY + cam.z * upZ);
    view[14] =  (cam.x * forwardX + cam.y * forwardY + cam.z * forwardZ);

    float mvp[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            mvp[col * 4 + row] =
                proj[0 * 4 + row] * view[col * 4 + 0] +
                proj[1 * 4 + row] * view[col * 4 + 1] +
                proj[2 * 4 + row] * view[col * 4 + 2] +
                proj[3 * 4 + row] * view[col * 4 + 3];
        }
    }

    glUseProgram(m_program3D);
    glUniformMatrix4fv(m_uniformMVP, 1, GL_FALSE, mvp);
    glUniform1i(m_uniformTex, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex ? tex : m_texProjectileSprite);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo3DPos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(m_attrPos3D);
    glVertexAttribPointer(m_attrPos3D, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo3DUV);
    glBufferData(GL_ARRAY_BUFFER, sizeof(uvs), uvs, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(m_attrUV3D);
    glVertexAttribPointer(m_attrUV3D, 2, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo3D);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);

    glDisableVertexAttribArray(m_attrPos3D);
    glDisableVertexAttribArray(m_attrUV3D);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_BLEND);
}

void RendererGL::drawMesh3D(const Mesh3D& mesh, const Camera3D& cam) {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = (m_height != 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    const float fov = 70.0f * 3.1415926535f / 180.0f;
    float f = 1.0f / std::tan(fov * 0.5f);
    const float nearPlane = 0.05f;
    const float farPlane = 500.0f;

    float proj[16] = {
        f / aspect, 0, 0, 0,
        0, f, 0, 0,
        0, 0, (farPlane + nearPlane) / (nearPlane - farPlane), -1,
        0, 0, (2 * farPlane * nearPlane) / (nearPlane - farPlane), 0
    };

    const float cosYaw = std::cos(cam.yaw);
    const float sinYaw = std::sin(cam.yaw);
    const float cosPitch = std::cos(cam.pitch);
    const float sinPitch = std::sin(cam.pitch);
    float forwardX = cosPitch * sinYaw;
    float forwardY = cosPitch * cosYaw;
    float forwardZ = sinPitch;

    float rightX = cosYaw;
    float rightY = -sinYaw;
    float rightZ = 0.0f;

    float upX = -sinPitch * sinYaw;
    float upY = -sinPitch * cosYaw;
    float upZ = cosPitch;

    float view[16] = {
        rightX, upX, -forwardX, 0,
        rightY, upY, -forwardY, 0,
        rightZ, upZ, -forwardZ, 0,
        0,      0,    0,        1
    };

    view[12] = -(cam.x * rightX + cam.y * rightY + cam.z * rightZ);
    view[13] = -(cam.x * upX + cam.y * upY + cam.z * upZ);
    view[14] =  (cam.x * forwardX + cam.y * forwardY + cam.z * forwardZ);

    float mvp[16];
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            mvp[c * 4 + r] =
                proj[0 * 4 + r] * view[c * 4 + 0] +
                proj[1 * 4 + r] * view[c * 4 + 1] +
                proj[2 * 4 + r] * view[c * 4 + 2] +
                proj[3 * 4 + r] * view[c * 4 + 3];
        }
    }

    glUseProgram(m_program3D);
    glUniformMatrix4fv(m_uniformMVP, 1, GL_FALSE, mvp);
    glUniform1i(m_uniformTex, 0);
    glActiveTexture(GL_TEXTURE0);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo3DPos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.vertices.size(), mesh.vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(m_attrPos3D);
    glVertexAttribPointer(m_attrPos3D, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo3DUV);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.uvs.size(), mesh.uvs.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(m_attrUV3D);
    glVertexAttribPointer(m_attrUV3D, 2, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo3D);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * mesh.indices.size(), mesh.indices.data(), GL_DYNAMIC_DRAW);

    auto drawRange = [&](size_t start, size_t count, GLuint tex) {
        if (count == 0 || tex == 0)
            return;
        glBindTexture(GL_TEXTURE_2D, tex);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_SHORT, (const void*)(start * sizeof(uint16_t)));
    };

    drawRange(mesh.floorIndexStart, mesh.floorIndexCount, m_texFloor);
    drawRange(mesh.ceilingIndexStart, mesh.ceilingIndexCount, m_texCeil);
    drawRange(mesh.wallIndexStart, mesh.wallIndexCount, m_texWall);

    glDisableVertexAttribArray(m_attrPos3D);
    glDisableVertexAttribArray(m_attrUV3D);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RendererGL::drawGrid(const Camera2D& cam, float gridSize) {
    if (gridSize <= 0.0f)
        return;

    setCamera(cam);

    const float halfWidthWorld = (m_width * 0.5f) / m_camera.zoom;
    const float halfHeightWorld = (m_height * 0.5f) / m_camera.zoom;

    const float minX = m_camera.offsetX - halfWidthWorld;
    const float maxX = m_camera.offsetX + halfWidthWorld;
    const float minY = m_camera.offsetY - halfHeightWorld;
    const float maxY = m_camera.offsetY + halfHeightWorld;

    const float minPixelSpacing = 4.0f;
    float fineStep = gridSize;
    while (fineStep * m_camera.zoom < minPixelSpacing) {
        fineStep *= 2.0f;
    }

    float coarseStep = fineStep * 4.0f;
    if (coarseStep < fineStep) {
        coarseStep = fineStep;
    }

    auto drawSpacing = [&](float spacing, float r, float g, float b) {
        const float startX = std::floor(minX / spacing) * spacing;
        const float endX = std::ceil(maxX / spacing) * spacing;
        const float startY = std::floor(minY / spacing) * spacing;
        const float endY = std::ceil(maxY / spacing) * spacing;

        for (float x = startX; x <= endX; x += spacing) {
            drawLine2D(x, minY, x, maxY, r, g, b);
        }

        for (float y = startY; y <= endY; y += spacing) {
            drawLine2D(minX, y, maxX, y, r, g, b);
        }
    };

    drawSpacing(coarseStep, 0.30f, 0.30f, 0.30f);
    drawSpacing(fineStep, 0.18f, 0.18f, 0.18f);

    if (minX <= 0.0f && maxX >= 0.0f) {
        drawLine2D(0.0f, minY, 0.0f, maxY, 0.6f, 0.2f, 0.2f);
    }
    if (minY <= 0.0f && maxY >= 0.0f) {
        drawLine2D(minX, 0.0f, maxX, 0.0f, 0.2f, 0.6f, 0.2f);
    }
}

void RendererGL::setCamera(const Camera2D& cam) {
    m_camera = cam;
    if (m_camera.zoom <= 0.0001f)
        m_camera.zoom = 0.0001f;
}

void RendererGL::setTextures(GLuint floorTex, GLuint wallTex, GLuint ceilTex) {
    m_texFloor = floorTex;
    m_texWall = wallTex;
    m_texCeil = ceilTex;
}

void RendererGL::setBillboardTextures(GLuint enemyTex, GLuint projectileTex) {
    m_texEnemySprite = enemyTex;
    m_texProjectileSprite = projectileTex;
}

void RendererGL::setItemTextures(GLuint healthTex, GLuint manaTex) {
    m_texItemHealth = healthTex;
    m_texItemMana = manaTex;
}

void RendererGL::setEffectTextures(GLuint blockFlashTex) {
    m_texBlockFlash = blockFlashTex;
}

float RendererGL::worldToClipX(float worldX) const {
    const float halfWidth = static_cast<float>(m_width) * 0.5f;
    if (halfWidth <= 0.0f)
        return 0.0f;
    const float screenX = (worldX - m_camera.offsetX) * m_camera.zoom;
    return screenX / halfWidth;
}

float RendererGL::worldToClipY(float worldY) const {
    const float halfHeight = static_cast<float>(m_height) * 0.5f;
    if (halfHeight <= 0.0f)
        return 0.0f;
    const float screenY = (worldY - m_camera.offsetY) * m_camera.zoom;
    return screenY / halfHeight;
}

bool RendererGL::projectPoint3D(const Camera3D& cam, float x, float y, float z,
                                float& outX, float& outY) const {
    float dx = x - cam.x;
    float dy = y - cam.y;
    float dz = z - cam.z;

    const float cosYaw = std::cos(cam.yaw);
    const float sinYaw = std::sin(cam.yaw);
    const float cosPitch = std::cos(cam.pitch);
    const float sinPitch = std::sin(cam.pitch);

    const float forwardX = cosPitch * sinYaw;
    const float forwardY = cosPitch * cosYaw;
    const float forwardZ = sinPitch;

    const float rightX = cosYaw;
    const float rightY = -sinYaw;
    const float rightZ = 0.0f;

    const float upX = -sinPitch * sinYaw;
    const float upY = -sinPitch * cosYaw;
    const float upZ = cosPitch;

    float camX = dx * rightX + dy * rightY + dz * rightZ;
    float camY = dx * upX + dy * upY + dz * upZ;
    float camZ = dx * forwardX + dy * forwardY + dz * forwardZ;

    const float nearPlane = 0.05f;
    const float farPlane = 200.0f;
    if (camZ <= nearPlane || camZ >= farPlane)
        return false;

    float aspect = (m_height != 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    const float fov = 70.0f * 3.1415926535f / 180.0f;
    float f = 1.0f / std::tan(fov * 0.5f);

    outX = (camX * f / aspect) / camZ;
    outY = (camY * f) / camZ;
    return true;
}

void RendererGL::endFrame(SDL_Window* window) {
    SDL_GL_SwapWindow(window);
}

// ===== internal helpers =====

bool RendererGL::initGL() {
    glDisable(GL_DEPTH_TEST); // 2D path disables; 3D path will enable as needed
    glDisable(GL_CULL_FACE);

    const char* vsSrc =
        "attribute vec2 aPos;\n"
        "void main() {\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char* fsSrc =
        "precision mediump float;\n"
        "uniform vec4 uColor;\n"
        "void main() {\n"
        "    gl_FragColor = uColor;\n"
        "}\n";

    m_program = createProgram(vsSrc, fsSrc);
    if (!m_program) {
        std::printf("Failed to create GL program\n");
        return false;
    }

    m_attrPos = glGetAttribLocation(m_program, "aPos");
    m_uniformColor = glGetUniformLocation(m_program, "uColor");

    if (m_attrPos < 0 || m_uniformColor < 0) {
        std::printf("Failed to get shader locations\n");
        return false;
    }

    const char* vsSrc3D =
        "uniform mat4 uMVP;\n"
        "attribute vec3 aPos;\n"
        "attribute vec2 aUV;\n"
        "varying vec2 vUV;\n"
        "void main() {\n"
        "    vUV = aUV;\n"
        "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "}\n";

    const char* fsSrc3D =
        "precision mediump float;\n"
        "varying vec2 vUV;\n"
        "uniform sampler2D uTex;\n"
        "void main() {\n"
        "    vec2 uv = fract(vUV);\n"
        "    gl_FragColor = texture2D(uTex, uv);\n"
        "}\n";

    m_program3D = createProgram(vsSrc3D, fsSrc3D);
    if (!m_program3D) {
        std::printf("Failed to create 3D GL program\n");
        return false;
    }

    m_attrPos3D   = glGetAttribLocation(m_program3D, "aPos");
    m_attrUV3D    = glGetAttribLocation(m_program3D, "aUV");
    m_uniformMVP  = glGetUniformLocation(m_program3D, "uMVP");
    m_uniformTex  = glGetUniformLocation(m_program3D, "uTex");

    if (m_attrPos3D < 0 || m_attrUV3D < 0 || m_uniformMVP < 0 || m_uniformTex < 0) {
        std::printf("Failed to get 3D shader locations\n");
        return false;
    }

    return true;
}

GLuint RendererGL::compileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::printf("Shader compile error: %s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint RendererGL::createProgram(const char* vsSrc, const char* fsSrc) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSrc);
    if (!vs) return 0;

    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!fs) {
        glDeleteShader(vs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        std::printf("Program link error: %s\n", log);
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

void RendererGL::drawQuad2D(float x, float y, float w, float h, float r, float g, float b, float a, int screenW, int screenH) {
    if (w <= 0.0f || h <= 0.0f || screenW <= 0 || screenH <= 0)
        return;

    // Convert from screen space (origin top-left) to clip space
    float x0 = (x / (static_cast<float>(screenW) * 0.5f)) - 1.0f;
    float y0 = 1.0f - (y / (static_cast<float>(screenH) * 0.5f));
    float x1 = ((x + w) / (static_cast<float>(screenW) * 0.5f)) - 1.0f;
    float y1 = 1.0f - ((y + h) / (static_cast<float>(screenH) * 0.5f));

    const float verts[8] = {
        x0, y0,
        x1, y0,
        x1, y1,
        x0, y1
    };

    glDisable(GL_DEPTH_TEST);
    glUseProgram(m_program);
    glUniform4f(m_uniformColor, r, g, b, a);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(m_attrPos);
    glVertexAttribPointer(m_attrPos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (const void*)0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableVertexAttribArray(m_attrPos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void RendererGL::drawText2D(const std::string& text, float x, float y, float scale, float r, float g, float b, float a, int screenW, int screenH) {
    float cursorX = x;
    const float advance = 6.0f * scale;
    for (char c : text) {
        std::array<uint8_t, 5> glyph{};
        if (!getGlyph(c, glyph))
            continue;
        for (int col = 0; col < 5; ++col) {
            uint8_t bits = glyph[col];
            for (int row = 0; row < 7; ++row) {
                if (bits & (1u << row)) {
                    float gx = cursorX + static_cast<float>(col) * scale;
                    float gy = y + static_cast<float>(row) * scale;
                    drawQuad2D(gx, gy, scale, scale, r, g, b, a, screenW, screenH);
                }
            }
        }
        cursorX += advance;
    }
}

static const char* hudEntityName(EntityType t) {
    switch (t) {
        case EntityType::PlayerStart: return "PlayerStart";
        case EntityType::EnemyWizard: return "EnemyWizard";
        case EntityType::ItemPickup:  return "ItemPickup";
    }
    return "Unknown";
}

void RendererGL::drawEditorHUD(const EditorState& state, int screenW, int screenH) {
    if (screenW <= 0 || screenH <= 0)
        return;

    auto modeColor = [&](float& r, float& g, float& b) {
        if (state.playMode) { r = 1.0f; g = 0.6f; b = 0.1f; return; }
        if (state.entityMode) {
            switch (state.entityBrush) {
                case EntityType::PlayerStart: r = 0.1f; g = 0.8f; b = 0.1f; return;
                case EntityType::EnemyWizard: r = 0.7f; g = 0.2f; b = 0.9f; return;
                case EntityType::ItemPickup:  r = 1.0f; g = 0.5f; b = 0.1f; return;
            }
        }
        if (state.wallMode) { r = 1.0f; g = 0.9f; b = 0.2f; return; }
        r = 0.0f; g = 0.7f; b = 1.0f;
    };

    float br = 0.0f, bg = 0.7f, bb = 1.0f;
    modeColor(br, bg, bb);

    const float border = 10.0f;
    const float ba = 1.0f;
    drawQuad2D(0.0f, 0.0f, static_cast<float>(screenW), border, br, bg, bb, ba, screenW, screenH);
    drawQuad2D(0.0f, static_cast<float>(screenH) - border, static_cast<float>(screenW), border, br, bg, bb, ba, screenW, screenH);
    drawQuad2D(0.0f, 0.0f, border, static_cast<float>(screenH), br, bg, bb, ba, screenW, screenH);
    drawQuad2D(static_cast<float>(screenW) - border, 0.0f, border, static_cast<float>(screenH), br, bg, bb, ba, screenW, screenH);

    const float boxW = 980.0f;
    const float boxH = 120.0f;
    const float boxX = 10.0f;
    const float boxY = static_cast<float>(screenH) - boxH - 10.0f;
    drawQuad2D(boxX, boxY, boxW, boxH, br, bg, bb, 0.6f, screenW, screenH);

    std::string mode;
    if (state.playMode) {
        mode = "MODE: PLAY MODE";
    } else if (state.entityMode) {
        mode = "MODE: ENTITY EDITING (" + std::string(hudEntityName(state.entityBrush)) + ")";
    } else if (state.wallMode) {
        mode = "MODE: WALL EDITING";
    } else {
        mode = "MODE: VERTEX EDITING";
    }

    char snapBuf[64];
    char cursorBuf[96];
    std::snprintf(cursorBuf, sizeof(cursorBuf), "CURSOR: (X: %.2f, Y: %.2f)", state.cursorX, state.cursorY);

    float textX = boxX + 12.0f;
    float textY = boxY + 12.0f;
    float scale = 2.0f;
    drawText2D(mode, textX, textY, scale, 1.0f, 1.0f, 1.0f, 1.0f, screenW, screenH);
    drawText2D(cursorBuf, textX, textY + 16.0f, scale, 1.0f, 1.0f, 1.0f, 1.0f, screenW, screenH);

    // Quick reference controls for Switch (controller)
    std::string controls1;
    std::string controls2;
    if (state.playMode) {
        controls1 = "Play: Left Stick move  |  Right Stick look  |  L block";
        controls2 = "Minus toggle playtest  |  Plus exit game";
    } else if (state.entityMode) {
        controls1 = "Entity (" + std::string(hudEntityName(state.entityBrush)) + "): A place  |  X delete  |  D-Pad Up cycle  |  D-Pad Down exit";
        controls2 = "Left Stick move cursor  |  Right Stick pan view  |  L/R zoom  |  Minus playtest";
    } else if (state.wallMode) {
        controls1 = "Walls: B select/extend  |  A place vertex  |  X delete";
        controls2 = "Left Stick move cursor  |  Right Stick pan view  |  L/R zoom  |  D-Pad Up entity mode  |  Minus playtest";
    } else {
        controls1 = "Vertices: A place/move  |  B select  |  X delete  |  D-Pad Up entity mode";
        controls2 = "Left Stick move cursor  |  Right Stick pan view  |  L/R zoom  |  Minus playtest";
    }

    drawText2D(controls1, textX, textY + 32.0f, scale, 1.0f, 1.0f, 1.0f, 1.0f, screenW, screenH);
    drawText2D(controls2, textX, textY + 48.0f, scale, 1.0f, 1.0f, 1.0f, 1.0f, screenW, screenH);
}

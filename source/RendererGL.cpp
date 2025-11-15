// RendererGL.cpp
#include "RendererGL.h"
#include "EditorState.h"
#include "Mesh3D.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include <vector>

RendererGL::RendererGL()
    : m_width(1280)
    , m_height(720)
    , m_camera{}
    , m_program(0)
    , m_attrPos(-1)
    , m_uniformColor(-1)
    , m_program3D(0)
    , m_attrPos3D(-1)
    , m_attrColor3D(-1)
    , m_uniformMVP(-1)
    , m_vbo(0)
    , m_vbo3DPos(0)
    , m_vbo3DColor(0)
    , m_ibo3D(0)
{
    m_camera.zoom = 1.0f;
}

RendererGL::~RendererGL() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_vbo3DPos) glDeleteBuffers(1, &m_vbo3DPos);
    if (m_vbo3DColor) glDeleteBuffers(1, &m_vbo3DColor);
    if (m_ibo3D) glDeleteBuffers(1, &m_ibo3D);
    if (m_program) glDeleteProgram(m_program);
    if (m_program3D) glDeleteProgram(m_program3D);
}

bool RendererGL::init(SDL_Window* window) {
    if (!window) {
        std::printf("RendererGL::init called with null window\n");
        return false;
    }

    static bool s_gladInitialized = false;
    if (!s_gladInitialized) {
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
            std::printf("Failed to initialize GLAD via SDL\n");
            return false;
        }
        s_gladInitialized = true;
    }

    if (!initGL())
        return false;

    glGenBuffers(1, &m_vbo);
    if (!m_vbo) {
        std::printf("Failed to create VBO for line rendering\n");
        return false;
    }

    glGenBuffers(1, &m_vbo3DPos);
    glGenBuffers(1, &m_vbo3DColor);
    glGenBuffers(1, &m_ibo3D);
    if (!m_vbo3DPos || !m_vbo3DColor || !m_ibo3D) {
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

    std::vector<float> verts;
    verts.reserve(sector.vertices.size() * 2);

    for (int idx : sector.vertices) {
        if (idx < 0 || idx >= static_cast<int>(state.vertices.size()))
            return;
        const auto& v = state.vertices[idx];
        verts.push_back(worldToClipX(v.first));
        verts.push_back(worldToClipY(v.second));
    }

    glUseProgram(m_program);
    glUniform4f(m_uniformColor, r, g, b, a);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * verts.size(), verts.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(m_attrPos);
    glVertexAttribPointer(m_attrPos, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, (const void*)0);

    glDrawArrays(GL_TRIANGLE_FAN, 0, static_cast<GLsizei>(sector.vertices.size()));

    glDisableVertexAttribArray(m_attrPos);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
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

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo3DPos);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.vertices.size(), mesh.vertices.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(m_attrPos3D);
    glVertexAttribPointer(m_attrPos3D, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo3DColor);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * mesh.colors.size(), mesh.colors.data(), GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(m_attrColor3D);
    glVertexAttribPointer(m_attrColor3D, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo3D);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * mesh.indices.size(), mesh.indices.data(), GL_DYNAMIC_DRAW);

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_SHORT, 0);

    glDisableVertexAttribArray(m_attrPos3D);
    glDisableVertexAttribArray(m_attrColor3D);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable(GL_DEPTH_TEST);
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
        "attribute vec3 aColor;\n"
        "varying vec3 vColor;\n"
        "void main() {\n"
        "    vColor = aColor;\n"
        "    gl_Position = uMVP * vec4(aPos, 1.0);\n"
        "}\n";

    const char* fsSrc3D =
        "precision mediump float;\n"
        "varying vec3 vColor;\n"
        "void main() {\n"
        "    gl_FragColor = vec4(vColor, 1.0);\n"
        "}\n";

    m_program3D = createProgram(vsSrc3D, fsSrc3D);
    if (!m_program3D) {
        std::printf("Failed to create 3D GL program\n");
        return false;
    }

    m_attrPos3D   = glGetAttribLocation(m_program3D, "aPos");
    m_attrColor3D = glGetAttribLocation(m_program3D, "aColor");
    m_uniformMVP  = glGetUniformLocation(m_program3D, "uMVP");

    if (m_attrPos3D < 0 || m_attrColor3D < 0 || m_uniformMVP < 0) {
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

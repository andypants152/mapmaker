// RendererGL.cpp
#include "RendererGL.h"
#include <cstdio>

RendererGL::RendererGL()
    : m_width(1280)
    , m_height(720)
    , m_program(0)
    , m_attrPos(-1)
    , m_attrColor(-1)
    , m_vbo(0)
    , m_ibo(0)
{
}

RendererGL::~RendererGL() {
    if (m_vbo) glDeleteBuffers(1, &m_vbo);
    if (m_ibo) glDeleteBuffers(1, &m_ibo);
    if (m_program) glDeleteProgram(m_program);
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

    if (!createTestTriangle())
        return false;

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

void RendererGL::drawTestTriangle() {
    glUseProgram(m_program);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);

    glEnableVertexAttribArray(m_attrPos);
    glEnableVertexAttribArray(m_attrColor);

    const GLsizei stride = sizeof(float) * 5; // vec2 pos + vec3 color

    glVertexAttribPointer(
        m_attrPos, 2, GL_FLOAT, GL_FALSE,
        stride, (const void*)0
    );

    glVertexAttribPointer(
        m_attrColor, 3, GL_FLOAT, GL_FALSE,
        stride, (const void*)(sizeof(float) * 2)
    );

    glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, 0);

    glDisableVertexAttribArray(m_attrPos);
    glDisableVertexAttribArray(m_attrColor);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RendererGL::endFrame(SDL_Window* window) {
    SDL_GL_SwapWindow(window);
}

// ===== internal helpers =====

bool RendererGL::initGL() {
    glDisable(GL_DEPTH_TEST); // 2D for now
    glDisable(GL_CULL_FACE);

    const char* vsSrc =
        "attribute vec2 aPos;\n"
        "attribute vec3 aColor;\n"
        "varying vec3 vColor;\n"
        "void main() {\n"
        "    vColor = aColor;\n"
        "    gl_Position = vec4(aPos, 0.0, 1.0);\n"
        "}\n";

    const char* fsSrc =
        "precision mediump float;\n"
        "varying vec3 vColor;\n"
        "void main() {\n"
        "    gl_FragColor = vec4(vColor, 1.0);\n"
        "}\n";

    m_program = createProgram(vsSrc, fsSrc);
    if (!m_program) {
        std::printf("Failed to create GL program\n");
        return false;
    }

    m_attrPos   = glGetAttribLocation(m_program, "aPos");
    m_attrColor = glGetAttribLocation(m_program, "aColor");

    if (m_attrPos < 0 || m_attrColor < 0) {
        std::printf("Failed to get attribute locations\n");
        return false;
    }

    return true;
}

bool RendererGL::createTestTriangle() {
    struct Vertex {
        float x, y;
        float r, g, b;
    };

    Vertex vertices[3] = {
        { -0.5f, -0.5f, 1.f, 0.f, 0.f },
        {  0.5f, -0.5f, 0.f, 1.f, 0.f },
        {  0.0f,  0.5f, 0.f, 0.f, 1.f },
    };

    GLushort indices[3] = { 0, 1, 2 };

    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &m_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

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

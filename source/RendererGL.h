// RendererGL.h
#pragma once

#include <SDL2/SDL.h>
#include <glad/glad.h>

class RendererGL {
public:
    RendererGL();
    ~RendererGL();

    bool init(SDL_Window* window);
    void resize(int width, int height);
    void beginFrame();
    void drawTestTriangle();
    void endFrame(SDL_Window* window);

private:
    bool initGL();
    bool createTestTriangle();

    GLuint compileShader(GLenum type, const char* src);
    GLuint createProgram(const char* vsSrc, const char* fsSrc);

    int m_width;
    int m_height;

    GLuint m_program;
    GLint  m_attrPos;
    GLint  m_attrColor;

    GLuint m_vbo;
    GLuint m_ibo;
};

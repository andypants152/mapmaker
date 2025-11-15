// RendererGL.h
#pragma once

#include <SDL2/SDL.h>
#include <glad/glad.h>

struct Camera2D {
    float zoom = 1.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
};

class RendererGL {
public:
    RendererGL();
    ~RendererGL();

    bool init(SDL_Window* window);
    void resize(int width, int height);
    void beginFrame();
    void drawLine2D(float x1, float y1, float x2, float y2, float r, float g, float b);
    void drawPoint2D(float x, float y, float size, float r, float g, float b);
    void drawGrid(const Camera2D& cam, float gridSize);
    void endFrame(SDL_Window* window);

    void setCamera(const Camera2D& cam);

private:
    bool initGL();

    GLuint compileShader(GLenum type, const char* src);
    GLuint createProgram(const char* vsSrc, const char* fsSrc);

    float worldToClipX(float worldX) const;
    float worldToClipY(float worldY) const;

    int m_width;
    int m_height;

    Camera2D m_camera;

    GLuint m_program;
    GLint  m_attrPos;
    GLint  m_uniformColor;

    GLuint m_vbo;
};

// RendererGL.h
#pragma once

#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#elif __has_include(<SDL3/SDL.h>)
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif
#ifdef __SWITCH__
#include <GLES2/gl2.h>
#else
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#if __has_include(<SDL2/SDL_opengl.h>)
#include <SDL2/SDL_opengl.h>
#elif __has_include(<SDL3/SDL_opengl.h>)
#include <SDL3/SDL_opengl.h>
#else
#include <SDL_opengl.h>
#endif
#endif
#include <string>

struct Sector;
struct EditorState;
struct Camera3D;
struct Mesh3D;
GLuint loadTextureFromPNG(const char* path);

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
    void drawSectorFill(const Sector& sector, const EditorState& state,
                        float r, float g, float b, float a);
    void drawMesh3D(const Mesh3D& mesh, const Camera3D& cam);
    void drawBillboard3D(const Camera3D& cam, float x, float y, float z, float size, GLuint tex, float r, float g, float b);
    void drawGrid(const Camera2D& cam, float gridSize);
    void endFrame(SDL_Window* window);

    void setTextures(GLuint floorTex, GLuint wallTex, GLuint ceilTex);
    void setBillboardTextures(GLuint enemyTex, GLuint projectileTex);
    void setItemTextures(GLuint healthTex, GLuint manaTex);
    void setEffectTextures(GLuint blockFlashTex);

    void setCamera(const Camera2D& cam);

    void drawQuad2D(float x, float y, float w, float h, float r, float g, float b, float a, int screenW, int screenH);
    void drawText2D(const std::string& text, float x, float y, float scale, float r, float g, float b, float a, int screenW, int screenH);
    void drawEditorHUD(const EditorState& state, int screenW, int screenH);

private:
    bool initGL();

    GLuint compileShader(GLenum type, const char* src);
    GLuint createProgram(const char* vsSrc, const char* fsSrc);

    float worldToClipX(float worldX) const;
    float worldToClipY(float worldY) const;
    bool projectPoint3D(const Camera3D& cam, float x, float y, float z,
                        float& outX, float& outY) const;

    int m_width;
    int m_height;

    Camera2D m_camera;

    GLuint m_program;
    GLint  m_attrPos;
    GLint  m_uniformColor;

    GLuint m_program3D;
    GLint  m_attrPos3D;
    GLint  m_attrUV3D;
    GLint  m_uniformMVP;
    GLint  m_uniformTex;

    GLuint m_vbo;
    GLuint m_vbo3DPos;
    GLuint m_vbo3DUV;
    GLuint m_ibo3D;

    GLuint m_texFloor = 0;
    GLuint m_texWall = 0;
    GLuint m_texCeil = 0;
    GLuint m_texEnemySprite = 0;
    GLuint m_texProjectileSprite = 0;
    GLuint m_texItemHealth = 0;
    GLuint m_texItemMana = 0;
    GLuint m_texBlockFlash = 0;
};

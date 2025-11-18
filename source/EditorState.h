// EditorState.h
#pragma once

#include <utility>
#include <vector>
#include "Mesh3D.h"
#include "Projectiles.h"

struct LineDef {
    int v1 = -1;
    int v2 = -1;
};

struct Sector {
    std::vector<int> vertices;
    bool clockwise = false;
};

struct Camera3D {
    float x = 0.0f;
    float y = 0.0f;
    float z = 1.7f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float radius = 0.25f;
    float height = 1.7f;
};

enum class EntityType {
    PlayerStart,
    EnemyWizard,
    ItemPickup,
    Door
};

struct Entity {
    float x = 0.0f;
    float y = 0.0f;
    EntityType type = EntityType::PlayerStart;
};

struct DoorState {
    float x = 0.0f;
    float y = 0.0f;
    float width = 2.0f;
    float height = 3.0f;
    float progress = 0.0f; // 0 = closed, 1 = fully open
    bool opening = false;
    bool locked = false;
    bool active = true;
};

struct ItemWorld {
    float x = 0.0f;
    float y = 0.0f;
    float z = 1.0f;
    EntityType type = EntityType::ItemPickup;
    bool alive = true;
};

struct EditorState {
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    float cursorRawX = 0.0f;
    float cursorRawY = 0.0f;
    bool snapEnabled = true;
    float snapSize = 1.0f;
    std::vector<std::pair<float, float>> vertices;
    std::vector<LineDef> lines;
    std::vector<Sector> sectors;
    Mesh3D worldMesh;
    std::vector<Entity> entities;
    std::vector<EnemyWizard> enemies;
    ProjectileSystem projectiles;
    std::vector<ItemWorld> items;
    std::vector<DoorState> doors;
    int hoveredVertex = -1;
    int selectedVertex = -1;
    int hoveredEntity = -1;
    int selectedEntity = -1;
    bool wallMode = false;
    bool playMode = false;
    bool entityMode = false;
    EntityType entityBrush = EntityType::PlayerStart;
    bool blocking = false;
    float blockFlashTimer = 0.0f;
};

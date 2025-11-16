// Projectiles.h
#pragma once

#include <vector>

struct Projectile {
    float x, y, z;
    float vx, vy, vz;
    bool fromPlayer;
    bool alive;
};

struct ProjectileSystem {
    std::vector<Projectile> active;
};

struct EnemyWizard {
    float x, y, z;
    float cooldown;
    bool alive;
};

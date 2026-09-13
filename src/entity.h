#pragma once
#include <cstdint>
#include "boss/oam.h"

class Entity {
public:
    virtual ~Entity() = default;

    float x = 0.0f;
    float y = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float width = 16.0f;
    float height = 16.0f;
    bool active = true;

    virtual void update(float dt) = 0;
    virtual void render(boss::OAM& oam) = 0;
};

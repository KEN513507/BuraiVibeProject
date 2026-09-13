#pragma once
#include <cstdint>
#include "oam.h"

class MultiPartEntity;

namespace boss {

class Part {
public:
    virtual ~Part() = default;

    MultiPartEntity* owner = nullptr;
    float local_x = 0.0f;
    float local_y = 0.0f;
    float world_x = 0.0f;
    float world_y = 0.0f;
    float angle = 0.0f;

    int hp = 10;
    int max_hp = 10;
    bool destroyed = false;
    bool active = true;

    virtual void update(float dt) = 0;
    virtual void emit_to(OAM& oam) = 0;
};

}

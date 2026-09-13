#pragma once
#include "part.h"

namespace boss {

struct Emitter {
    float local_x = 0.0f;
    float local_y = 0.0f;
    Part* attach_to = nullptr;
    float angle_offset = 0.0f;
    int cooldown_frames = 0;

    void get_world_pos(float entity_x, float entity_y, float& out_x, float& out_y) const {
        if (attach_to) {
            out_x = attach_to->world_x + local_x;
            out_y = attach_to->world_y + local_y;
        } else {
            out_x = entity_x + local_x;
            out_y = entity_y + local_y;
        }
    }
};

}

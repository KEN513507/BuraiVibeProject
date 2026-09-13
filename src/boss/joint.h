#pragma once
#include <cmath>
#include "composite_part.h"
#include "chain_node.h"

namespace boss {

class Joint : public CompositePart, public ChainNode {
public:
    void solve_fk(float parent_x, float parent_y, float parent_angle) override {
        angle = parent_angle + joint_angle;
        world_x = parent_x + std::cos(angle) * base_length;
        world_y = parent_y + std::sin(angle) * base_length;
        if (next) {
            next->solve_fk(world_x, world_y, angle);
        }
    }
};

class Segment : public CompositePart, public ChainNode {
public:
    void solve_fk(float parent_x, float parent_y, float parent_angle) override {
        angle = parent_angle + joint_angle;
        world_x = parent_x + std::cos(angle) * base_length;
        world_y = parent_y + std::sin(angle) * base_length;
        if (next) {
            next->solve_fk(world_x, world_y, angle);
        }
    }
};

}

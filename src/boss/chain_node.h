#pragma once

namespace boss {

class ChainNode {
public:
    virtual ~ChainNode() = default;

    ChainNode* prev = nullptr;
    ChainNode* next = nullptr;
    float base_length = 16.0f;
    float joint_angle = 0.0f;

    virtual void solve_fk(float parent_x, float parent_y, float parent_angle) = 0;
};

}

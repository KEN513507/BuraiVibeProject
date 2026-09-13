#pragma once
#include "single_part.h"
#include "dir_sprite.h"

namespace boss {

class Tip : public SinglePart {
public:
    uint16_t dir_tile_table[8] = {0}; // 8方向スワップ用テーブル

    void update(float dt) override {
        uint8_t dir = DirSpriteResolver::RadianTo8Dir(angle);
        unit.tile_id = dir_tile_table[dir];
    }
};

}

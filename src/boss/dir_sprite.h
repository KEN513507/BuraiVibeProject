#pragma once
#include <cmath>
#include <cstdint>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace boss {

// 8方位インデックス (0: 上, 1: 右上, 2: 右, 3: 右下, 4: 下, 5: 左下, 6: 左, 7: 左上)
struct DirSpriteResolver {
    static uint8_t RadianTo8Dir(float angle_rad) {
        // -PI〜+PI を 0〜2PI に正規化し、上を基準 (0) に合わせる
        float a = angle_rad + static_cast<float>(M_PI * 0.5);
        while (a < 0.0f) a += static_cast<float>(M_PI * 2.0);
        while (a >= static_cast<float>(M_PI * 2.0)) a -= static_cast<float>(M_PI * 2.0);

        int dir = static_cast<int>(std::floor((a + (M_PI / 8.0f)) / (M_PI / 4.0f))) % 8;
        return static_cast<uint8_t>(dir);
    }
};

}

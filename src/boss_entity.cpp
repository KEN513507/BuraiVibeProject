#include "boss_entity.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void BossEntity::update(float dt) {
    if (!active || phase == BossPhase::DEAD) return;

    boss_timer++;

    // 生存部位数から発狂度 (frenzy_rate) を動的計算
    if (parts.size() > 1) {
        int alive = count_alive_parts();
        frenzy_rate = 1.0f - std::clamp(static_cast<float>(alive - 1) / static_cast<float>(parts.size() - 1), 0.0f, 1.0f);
    }

    // 呼吸サイン波の周波数更新 (発狂に伴い加速)
    float freq = 0.05f + frenzy_rate * 0.10f;
    breathe_timer += freq;
    breathe_scale = 1.0f + std::sin(breathe_timer) * (0.08f + frenzy_rate * 0.12f);

    // 基底クラス (MultiPartEntity) のFKチェーン・Emitter更新
    MultiPartEntity::update(dt);
}

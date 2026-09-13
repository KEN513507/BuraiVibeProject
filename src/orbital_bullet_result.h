#pragma once

// entity.h (class Entity) を含まないので、main.cpp の struct Entity と衝突せずに使える
enum class OrbitalBulletResult {
    MISS,              // どれにも当たっていない
    BLOCKED_BY_SHIELD, // 弾だけ消える。Shield も Core も変化なし
    CORE_DAMAGED,      // Core の HP が減った
    CORE_DESTROYED     // Core の HP が 0 以下になり、敵全体が死亡した
};

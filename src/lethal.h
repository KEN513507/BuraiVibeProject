#pragma once

#include <algorithm>

// 接触した瞬間にプレイヤーを死亡させるもの (回転障害物・敵キャラ) に共通の円形判定。
// 静的な壁 (tileGrid の障害物) は触れても死なない別カテゴリで、こちらには含めない。
struct LethalCircle {
    float x, y;
    float radius;
};

// 円と矩形の当たり判定。中心距離ではなく、矩形上で円の中心に最も近い点との距離で判定する。
// hazard と矩形は同じ座標系 (画面座標どうし、またはマップ座標どうし) で渡すこと。
inline bool CheckLethalCollision(const LethalCircle& hazard, float px, float py, float pw, float ph) {
    float closestX = std::clamp(hazard.x, px, px + pw);
    float closestY = std::clamp(hazard.y, py, py + ph);
    float dx = hazard.x - closestX;
    float dy = hazard.y - closestY;
    return (dx * dx + dy * dy) <= (hazard.radius * hazard.radius);
}

// 矩形の当たり判定を持つ敵キャラを円で近似する (中心 = 矩形の中心、半径 = 幅と高さの小さい方の半分)
inline LethalCircle LethalCircleFromRect(float x, float y, float w, float h) {
    return { x + w * 0.5f, y + h * 0.5f, std::min(w, h) * 0.5f };
}

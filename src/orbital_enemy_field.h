#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "orbital_bullet_result.h"

struct SDL_Renderer;

// ゲーム内の OrbitalShieldEnemy を持つ入れ物。
// entity.h (class Entity) をこのヘッダに出さないので、main.cpp の struct Entity と衝突しない。
class OrbitalEnemyField {
public:
    OrbitalEnemyField();
    ~OrbitalEnemyField();
    OrbitalEnemyField(const OrbitalEnemyField&) = delete;
    OrbitalEnemyField& operator=(const OrbitalEnemyField&) = delete;

    void Clear();
    void Spawn(float centerX, float centerY);

    // playerCenterX / Y は Player 中心の画面座標。dt は秒
    void Update(float dt, float playerCenterX, float playerCenterY, int viewWidth, int viewHeight);

    // 最初に当たった 1 体の結果を返す。CORE_DESTROYED のとき scoreOut に得点を入れる
    OrbitalBulletResult ResolvePlayerBullet(float bx, float by, float bw, float bh, int damage, int& scoreOut);

    // OAM に積み、flicker を解決してから SDL ブリッジで描く
    void Render(SDL_Renderer* renderer, uint32_t frameCount) const;

    size_t AliveCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "orbital_bullet_result.h"

struct SDL_Renderer;
class OamTileBank;

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

    // 最初に当たった 1 体の結果を返す。CORE_DESTROYED のとき scoreOut に得点を入れ、
    // coreXOut / coreYOut が非 null ならその Core の中心座標 (画面座標) も入れる。それ以外の結果では書かない
    OrbitalBulletResult ResolvePlayerBullet(float bx, float by, float bw, float bh, int damage, int& scoreOut,
                                            float* coreXOut = nullptr, float* coreYOut = nullptr);

    bool HitsPlayer(float px, float py, float pw, float ph) const;

    // OAM に積み、flicker を解決してから SDL ブリッジで描く
    void Render(SDL_Renderer* renderer, uint32_t frameCount, const OamTileBank& tiles) const;

    size_t AliveCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

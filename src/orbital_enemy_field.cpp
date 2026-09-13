#include "orbital_enemy_field.h"

#include <algorithm>
#include <vector>

#include "oam_sdl_bridge.h"
#include "orbital_shield_enemy.h"

struct OrbitalEnemyField::Impl {
    // Part::owner が Entity のアドレスを指すので、Entity 自体は動かさず unique_ptr で持つ
    std::vector<std::unique_ptr<OrbitalShieldEnemy>> enemies;
};

OrbitalEnemyField::OrbitalEnemyField() : impl_(std::make_unique<Impl>()) {}
OrbitalEnemyField::~OrbitalEnemyField() = default;

void OrbitalEnemyField::Clear() { impl_->enemies.clear(); }

void OrbitalEnemyField::Spawn(float centerX, float centerY) {
    impl_->enemies.push_back(std::make_unique<OrbitalShieldEnemy>(centerX, centerY));
}

void OrbitalEnemyField::Update(float dt, float playerCenterX, float playerCenterY, int viewWidth, int viewHeight) {
    for (auto& enemy : impl_->enemies) {
        enemy->set_player_position(playerCenterX, playerCenterY);
        enemy->update(dt);
        enemy->cull_if_offscreen(0, 0, viewWidth, viewHeight);
    }
    auto& list = impl_->enemies;
    list.erase(std::remove_if(list.begin(), list.end(), [](const auto& e) { return !e->active; }), list.end());
}

OrbitalBulletResult OrbitalEnemyField::ResolvePlayerBullet(float bx, float by, float bw, float bh, int damage,
                                                           int& scoreOut, float* coreXOut, float* coreYOut) {
    scoreOut = 0;
    for (auto& enemy : impl_->enemies) {
        const OrbitalBulletResult result = enemy->resolve_player_bullet(bx, by, bw, bh, damage);
        if (result == OrbitalBulletResult::MISS) continue;
        if (result == OrbitalBulletResult::CORE_DESTROYED) {
            scoreOut = enemy->tuning().scoreValue;
            // 死亡直後でも Core の部位は次の Update で取り除かれるまで有効
            if (coreXOut) *coreXOut = enemy->core().world_x;
            if (coreYOut) *coreYOut = enemy->core().world_y;
        }
        return result;
    }
    return OrbitalBulletResult::MISS;
}

bool OrbitalEnemyField::HitsPlayer(float px, float py, float pw, float ph) const {
    for (const auto& enemy : impl_->enemies) {
        if (enemy->overlaps_player(px, py, pw, ph)) return true;
    }
    return false;
}

void OrbitalEnemyField::Render(SDL_Renderer* renderer, uint32_t frameCount) const {
    if (impl_->enemies.empty()) return;
    boss::OAM oam;
    for (const auto& enemy : impl_->enemies) enemy->render(oam);
    oam.resolve_flicker(frameCount);
    // 色と大きさは各部位の RenderUnit が持ち、OAMEntry 経由で届く (開発用プレースホルダー)
    DrawOamPlaceholders(renderer, oam);
}

size_t OrbitalEnemyField::AliveCount() const { return impl_->enemies.size(); }

#include "orbital_shield_enemy.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = kPi * 2.0f;

// current を target へ最大 maxDelta だけ近づける
float Approach(float current, float target, float maxDelta) {
    if (current < target) return std::min(current + maxDelta, target);
    return std::max(current - maxDelta, target);
}

bool OverlapsCentered(float bx, float by, float bw, float bh,
                      float cx, float cy, float w, float h) {
    const float left = cx - w * 0.5f;
    const float top = cy - h * 0.5f;
    return bx < left + w && bx + bw > left && by < top + h && by + bh > top;
}

// 開発用プレースホルダーの単色 (NES 2C02 パレット)。製品仕様ではない
constexpr boss::Rgb kCorePlaceholder{0xE4, 0x00, 0x58};   // $15
constexpr boss::Rgb kShieldPlaceholder{0xBC, 0xBC, 0xBC}; // $10

// 中心座標の部位を OAM 左上座標で出すためのオフセットと、プレースホルダーの色・大きさを設定する
std::unique_ptr<boss::Part> MakePart(int cellSize, uint16_t tile, boss::Rgb placeholder) {
    auto part = std::make_unique<boss::SinglePart>();
    part->unit.rel_x = -cellSize * 0.5f;
    part->unit.rel_y = -cellSize * 0.5f;
    part->unit.tile_id = tile;
    part->unit.placeholder_color = placeholder;
    part->unit.placeholder_w = cellSize;
    part->unit.placeholder_h = cellSize;
    return part;
}

} // namespace

OrbitalShieldEnemy::OrbitalShieldEnemy(float centerX, float centerY, const OrbitalShieldTuning& tuning)
    : tuning_(tuning) {
    x = centerX;
    y = centerY;
    width = static_cast<float>(tuning_.coreCellSize);
    height = static_cast<float>(tuning_.coreCellSize);

    core_ = static_cast<boss::SinglePart*>(add_part(MakePart(tuning_.coreCellSize, CORE_TILE_BASE, kCorePlaceholder)));
    core_->hp = tuning_.coreHp;
    core_->max_hp = tuning_.coreHp;

    // 十字の上 / 左 / 下 (画面座標は +y が下なので 上 = 270 度)。右 (0 度) は空き枠
    constexpr std::array<float, SHIELD_COUNT> kCrossPhases{kPi * 1.5f, kPi, kPi * 0.5f};
    for (int i = 0; i < SHIELD_COUNT; ++i) {
        shieldPhaseOffsets_[i] = kCrossPhases[i];
        shields_[i] = static_cast<boss::SinglePart*>(add_part(MakePart(tuning_.shieldCellSize, SHIELD_TILE_BASE, kShieldPlaceholder)));
    }

    // spawn 直後の描画で Shield が (0,0) に出ないよう即時計算する
    recompute_rigid_parts();
}

void OrbitalShieldEnemy::set_player_position(float playerX, float playerY) {
    playerX_ = playerX;
    playerY_ = playerY;
    hasPlayerPosition_ = true;
}

void OrbitalShieldEnemy::update(float dt) {
    if (!active) return;
    update_chase(dt);   // A: vx / vy を決める (位置の積分は MultiPartEntity::update)
    update_orbit(dt);   // B
    update_animation(dt); // C, D (タイル番号だけ。座標には触れない)
    RigidMultiPartEntity::update(dt); // 位置 → recompute_rigid_parts → Part::update → Emitter
}

void OrbitalShieldEnemy::update_chase(float dt) {
    vx = 0.0f;
    vy = 0.0f;
    if (!hasPlayerPosition_ || dt <= 0.0f) return;
    const float dx = playerX_ - x;
    const float dy = playerY_ - y;
    const float distance = std::hypot(dx, dy);
    if (distance <= tuning_.stopDistance) return;
    // stopDistance を越えて食い込まないよう、1 フレームの移動量を残り距離で頭打ちにする
    const float step = std::min(tuning_.chaseSpeed * dt, distance - tuning_.stopDistance);
    vx = dx / distance * step / dt;
    vy = dy / distance * step / dt;
}

void OrbitalShieldEnemy::update_orbit(float dt) {
    orbitReverseTimer_ += dt;
    if (orbitReverseTimer_ >= tuning_.orbitReverseInterval) {
        orbitReverseTimer_ -= tuning_.orbitReverseInterval;
        orbitDirection_ = -orbitDirection_;
    }
    // 目標角速度へ加速度制限つきで近づける: +ω → 減速 → 0 → -ω
    const float target = tuning_.orbitAngularSpeed * orbitDirection_;
    orbitAngularVelocity_ = Approach(orbitAngularVelocity_, target, tuning_.orbitAcceleration * dt);
    orbitAngle_ = std::fmod(orbitAngle_ + orbitAngularVelocity_ * dt, kTwoPi);
    if (orbitAngle_ < 0.0f) orbitAngle_ += kTwoPi;
}

void OrbitalShieldEnemy::update_animation(float dt) {
    // 公転とは無関係な独自タイマー。1 周期ごとに巻き戻して float の肥大化を防ぐ
    coreAnimTimer_ = std::fmod(coreAnimTimer_ + dt, CORE_FRAME_COUNT / tuning_.coreAnimFps);
    shieldAnimTimer_ = std::fmod(shieldAnimTimer_ + dt, SHIELD_FRAME_COUNT / tuning_.shieldAnimFps);
    coreFrame_ = static_cast<int>(coreAnimTimer_ * tuning_.coreAnimFps) % CORE_FRAME_COUNT;
    shieldFrame_ = static_cast<int>(shieldAnimTimer_ * tuning_.shieldAnimFps) % SHIELD_FRAME_COUNT;
    core_->unit.tile_id = static_cast<uint16_t>(CORE_TILE_BASE + coreFrame_);
    for (auto* shield : shields_) shield->unit.tile_id = static_cast<uint16_t>(SHIELD_TILE_BASE + shieldFrame_);
}

void OrbitalShieldEnemy::recompute_rigid_parts() {
    // 前フレームの Shield 座標は使わず、毎回 Parent 中心 + 軌道オフセットで作り直す (drift 防止)
    core_->world_x = x;
    core_->world_y = y;
    for (int i = 0; i < SHIELD_COUNT; ++i) {
        const float angle = orbitAngle_ + shieldPhaseOffsets_[i];
        shields_[i]->world_x = x + std::cos(angle) * tuning_.orbitRadiusX;
        shields_[i]->world_y = y + std::sin(angle) * tuning_.orbitRadiusY;
    }
}

OrbitalBulletResult OrbitalShieldEnemy::resolve_player_bullet(float bx, float by, float bw, float bh,
                                                              int damage) {
    if (!active) return OrbitalBulletResult::MISS;

    // FIRST: Shield。当たったら弾だけ止める。Shield 側の状態は何も変えない (無敵)
    for (const auto* shield : shields_) {
        if (OverlapsCentered(bx, by, bw, bh, shield->world_x, shield->world_y,
                             tuning_.shieldHitboxWidth, tuning_.shieldHitboxHeight)) {
            return OrbitalBulletResult::BLOCKED_BY_SHIELD;
        }
    }

    // SECOND: Core。Shield に止められなかった弾だけがここに届く
    if (!OverlapsCentered(bx, by, bw, bh, core_->world_x, core_->world_y,
                          tuning_.coreHitboxWidth, tuning_.coreHitboxHeight)) {
        return OrbitalBulletResult::MISS;
    }
    core_->hp -= damage;
    if (core_->hp <= 0) {
        core_->hp = 0;
        core_->destroyed = true;
        kill();
        return OrbitalBulletResult::CORE_DESTROYED;
    }
    return OrbitalBulletResult::CORE_DAMAGED;
}

bool OrbitalShieldEnemy::overlaps_player(float px, float py, float pw, float ph) const {
    if (!active) return false;
    for (const auto* shield : shields_) {
        if (OverlapsCentered(px, py, pw, ph, shield->world_x, shield->world_y,
                             tuning_.shieldHitboxWidth, tuning_.shieldHitboxHeight)) return true;
    }
    return OverlapsCentered(px, py, pw, ph, core_->world_x, core_->world_y,
                            tuning_.coreHitboxWidth, tuning_.coreHitboxHeight);
}

void OrbitalShieldEnemy::cull_if_offscreen(int cameraX, int cameraY, int viewWidth, int viewHeight) {
    if (!active) return;
    const float halfW = tuning_.coreHitboxWidth * 0.5f;
    const float halfH = tuning_.coreHitboxHeight * 0.5f;
    const bool visible = x + halfW > cameraX && x - halfW < cameraX + viewWidth &&
                         y + halfH > cameraY && y - halfH < cameraY + viewHeight;
    enteredView_ = enteredView_ || visible;
    const float m = tuning_.despawnMargin;
    if (enteredView_ && (x + halfW < cameraX - m || x - halfW > cameraX + viewWidth + m ||
                         y + halfH < cameraY - m || y - halfH > cameraY + viewHeight + m)) {
        kill();
    }
}

void OrbitalShieldEnemy::kill() {
    // Entity 全体を止める。MultiPartEntity::render は !active で何も出さないので 4 部位まとめて消える
    active = false;
}

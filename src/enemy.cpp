#include "enemy.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kGravity = 0.35f;
constexpr float kJumpSpeed = -5.2f;
constexpr float kTriggerDistance = 64.0f;
}

Enemy::Enemy(float posX, float posY, const EnemyTypeConfig& typeCfg,
             float patrolLeft, float patrolRight)
    : x(posX), y(posY), startX(posX), startY(posY), hp(typeCfg.maxHp),
      config(typeCfg), patrolLeft_(patrolLeft), patrolRight_(patrolRight) {}

void Enemy::Update(float playerX, float playerY) {
    if (!active) return;
    ++frameTimer;
    if (hitCooldown > 0) --hitCooldown;

    switch (config.pattern) {
    case MovementPattern::STRAIGHT_LEFT:
        x -= config.speed;
        break;
    case MovementPattern::SIN_WAVE:
        x -= config.speed;
        y = startY + std::sin(frameTimer * 0.08f) * 32.0f;
        break;
    case MovementPattern::HOMING: {
        const float dx = playerX - (x + Width() / 2.0f);
        const float dy = playerY - (y + Height() / 2.0f);
        const float distance = std::hypot(dx, dy);
        if (distance > 0.001f) {
            const float step = std::min(config.speed, distance);
            x += dx / distance * step;
            y += dy / distance * step;
        }
        break;
    }
    case MovementPattern::STOP_AND_SHOOT:
        x = std::max(startX - 80.0f, x - config.speed);
        break;
    case MovementPattern::CIRCLE_ROUND:
        x = startX + std::cos(frameTimer * 0.05f) * 40.0f;
        y = startY + std::sin(frameTimer * 0.05f) * 40.0f;
        break;
    case MovementPattern::PATROL_JUMP: {
        // Keep the sprite inside the supporting obstacle's top face.
        if (patrolRight_ > patrolLeft_ + Width()) {
            x += direction_ * config.speed;
            if (x <= patrolLeft_) {
                x = patrolLeft_;
                direction_ = 1;
            } else if (x + Width() >= patrolRight_) {
                x = patrolRight_ - Width();
                direction_ = -1;
            }
        }
        if (jumpCooldown_ > 0) --jumpCooldown_;
        const float dx = playerX - (x + Width() / 2.0f);
        const float dy = playerY - (y + Height() / 2.0f);
        if (verticalSpeed_ == 0.0f && y == startY && jumpCooldown_ == 0 &&
            dx * dx + dy * dy <= kTriggerDistance * kTriggerDistance) {
            verticalSpeed_ = kJumpSpeed;
            jumpCooldown_ = 45;
        }
        if (verticalSpeed_ != 0.0f || y < startY) {
            y += verticalSpeed_;
            verticalSpeed_ += kGravity;
            if (y >= startY) {
                y = startY;
                verticalSpeed_ = 0.0f;
            }
        }
        break;
    }
    }
}

bool Enemy::ShouldShoot() const {
    return active && config.shootInterval > 0 &&
           frameTimer % config.shootInterval == 0 &&
           (config.pattern != MovementPattern::STOP_AND_SHOOT || x <= startX - 80.0f);
}

void Enemy::Render(SDL_Renderer* renderer, SDL_Texture* spriteSheet,
                   int cameraX, int cameraY) const {
    if (!active) return;
    SDL_Rect dst{static_cast<int>(std::round(x - cameraX)),
                 static_cast<int>(std::round(y - cameraY)),
                 config.spriteSrcRect.w, config.spriteSrcRect.h};
    if (spriteSheet) {
        SDL_RenderCopy(renderer, spriteSheet, &config.spriteSrcRect, &dst);
    } else {
        SDL_SetRenderDrawColor(renderer, 0x28, 0x8C, 0x84, 255);
        SDL_RenderFillRect(renderer, &dst);
    }
}

bool EnemyManager::RegisterEnemyType(const EnemyTypeConfig& config) {
    if (config.maxHp <= 0 || config.speed < 0 || config.collisionRadius <= 0 ||
        config.shootInterval < 0 || config.spriteSrcRect.w <= 0 ||
        config.spriteSrcRect.h <= 0) return false;
    const auto existing = std::find_if(typeRegistry_.begin(), typeRegistry_.end(),
        [&](const EnemyTypeConfig& type) { return type.typeId == config.typeId; });
    if (existing != typeRegistry_.end()) return false;
    typeRegistry_.push_back(config);
    return true;
}

bool EnemyManager::AddSpawnEvent(int frame, int typeId, float x, float y,
                                 float patrolLeft, float patrolRight) {
    if (frame < 1 || frame <= currentFrame_) return false;
    const auto type = std::find_if(typeRegistry_.begin(), typeRegistry_.end(),
        [&](const EnemyTypeConfig& config) { return config.typeId == typeId; });
    if (type == typeRegistry_.end()) return false;
    if (type->pattern == MovementPattern::PATROL_JUMP &&
        patrolRight <= patrolLeft + type->spriteSrcRect.w) return false;
    spawnTimeline_.push_back({frame, typeId, x, y, patrolLeft, patrolRight});
    return true;
}

void EnemyManager::Reset() {
    currentFrame_ = 0;
    activeEnemies_.clear();
    shots_.clear();
}

void EnemyManager::Update(float playerX, float playerY, int cameraX, int cameraY,
                          int viewWidth, int viewHeight) {
    ++currentFrame_;
    for (const auto& event : spawnTimeline_) {
        if (event.spawnFrame != currentFrame_) continue;
        const auto type = std::find_if(typeRegistry_.begin(), typeRegistry_.end(),
            [&](const EnemyTypeConfig& config) { return config.typeId == event.typeId; });
        if (type != typeRegistry_.end()) {
            activeEnemies_.emplace_back(event.spawnX, event.spawnY, *type,
                                        event.patrolLeft, event.patrolRight);
        }
    }
    for (auto& enemy : activeEnemies_) {
        enemy.Update(playerX, playerY);
        if (!enemy.active) continue;
        if (enemy.ShouldShoot()) {
            const float sx = enemy.x + enemy.Width() / 2.0f;
            const float sy = enemy.y + enemy.Height() / 2.0f;
            const float dx = playerX - sx, dy = playerY - sy;
            const float distance = std::hypot(dx, dy);
            if (distance > 0.001f) shots_.push_back({sx, sy, dx / distance * 1.8f, dy / distance * 1.8f});
        }
        // Offscreen spawn events may approach the camera. Cull only after
        // the enemy has actually appeared in the visible area.
        const bool visible = enemy.x + enemy.Width() > cameraX &&
            enemy.x < cameraX + viewWidth &&
            enemy.y + enemy.Height() > cameraY &&
            enemy.y < cameraY + viewHeight;
        enemy.enteredView = enemy.enteredView || visible;
        if (enemy.enteredView && enemy.config.pattern != MovementPattern::PATROL_JUMP &&
            (enemy.x + enemy.Width() < cameraX - 64 ||
             enemy.x > cameraX + viewWidth + 64 ||
             enemy.y + enemy.Height() < cameraY - 64 ||
             enemy.y > cameraY + viewHeight + 64)) {
            enemy.active = false;
        }
    }
    for (auto& shot : shots_) {
        shot.x += shot.vx;
        shot.y += shot.vy;
        if (shot.x < cameraX - 4 || shot.x > cameraX + viewWidth + 4 ||
            shot.y < cameraY - 4 || shot.y > cameraY + viewHeight + 4) shot.active = false;
    }
    activeEnemies_.erase(std::remove_if(activeEnemies_.begin(), activeEnemies_.end(),
        [](const Enemy& enemy) { return !enemy.active; }), activeEnemies_.end());
    shots_.erase(std::remove_if(shots_.begin(), shots_.end(),
        [](const EnemyShot& shot) { return !shot.active; }), shots_.end());
}

void EnemyManager::Render(SDL_Renderer* renderer, SDL_Texture* spriteSheet,
                          int cameraX, int cameraY) const {
    for (const auto& enemy : activeEnemies_) enemy.Render(renderer, spriteSheet, cameraX, cameraY);
}

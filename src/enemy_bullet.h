#pragma once
#include <SDL2/SDL.h>
#include <vector>
#include <cmath>
#include <array>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// 発射アルゴリズムパターン
enum class BulletPatternType {
    SINGLE_AIM, // 自機狙撃（1-WAY）
    N_WAY_FAN,  // 3-WAY 扇状拡散
    SPIRAL      // 全方位スパイラル
};

// 敵弾データオブジェクト
struct EnemyBullet {
    float x = 0.0f, y = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float radius = 3.0f;
    bool active = false;
};

// 敵弾マネージャー（64発固定オブジェクトプール）
class EnemyBulletManager {
public:
    static constexpr size_t MAX_BULLETS = 64;
    std::array<EnemyBullet, MAX_BULLETS> pool;

    void Clear() {
        for (auto& b : pool) b.active = false;
    }

    void Spawn(float x, float y, float vx, float vy, float radius = 3.0f) {
        for (auto& b : pool) {
            if (!b.active) {
                b.x = x;
                b.y = y;
                b.vx = vx;
                b.vy = vy;
                b.radius = radius;
                b.active = true;
                return;
            }
        }
    }

    void FirePattern(BulletPatternType pattern, float enemyX, float enemyY, float playerX, float playerY, float speed = 2.0f) {
        float dx = playerX - enemyX;
        float dy = playerY - enemyY;
        float dist = std::sqrt(dx * dx + dy * dy);
        float baseAngle = (dist > 0.0f) ? std::atan2(dy, dx) : 0.0f;

        switch (pattern) {
        case BulletPatternType::SINGLE_AIM: {
            float vx = std::cos(baseAngle) * speed;
            float vy = std::sin(baseAngle) * speed;
            Spawn(enemyX, enemyY, vx, vy);
            break;
        }
        case BulletPatternType::N_WAY_FAN: {
            float offsets[] = { -0.3f, 0.0f, 0.3f };
            for (float offset : offsets) {
                float angle = baseAngle + offset;
                Spawn(enemyX, enemyY, std::cos(angle) * speed, std::sin(angle) * speed);
            }
            break;
        }
        case BulletPatternType::SPIRAL: {
            static float spiralAngle = 0.0f;
            spiralAngle += 0.5f;
            for (int i = 0; i < 4; ++i) {
                float angle = spiralAngle + (i * (M_PI / 2.0));
                Spawn(enemyX, enemyY, std::cos(angle) * speed, std::sin(angle) * speed);
            }
            break;
        }
        }
    }

    void Update(int playWidth = 320, int playHeight = 204) {
        for (auto& b : pool) {
            if (!b.active) continue;
            b.x += b.vx;
            b.y += b.vy;
            if (b.x < -8.0f || b.x > playWidth + 8.0f || b.y < -8.0f || b.y > playHeight + 8.0f) {
                b.active = false;
            }
        }
    }

    void Render(SDL_Renderer* renderer, float scrollX = 0.0f, float scrollY = 0.0f) {
        SDL_SetRenderDrawColor(renderer, 255, 128, 0, 255); // NES オレンジ
        for (const auto& b : pool) {
            if (!b.active) continue;
            int sx = static_cast<int>(b.x - scrollX);
            int sy = static_cast<int>(b.y - scrollY);
            SDL_Rect r = { sx - (int)b.radius, sy - (int)b.radius, (int)(b.radius * 2), (int)(b.radius * 2) };
            SDL_RenderFillRect(renderer, &r);
        }
    }
};

// 結合モジュール（Mediator: 弾 vs 自機・地形衝突 & SEトリガー）
class BulletIntegrationModule {
public:
    static bool CheckPlayerHit(EnemyBulletManager& bulletMgr, float px, float py, float pradius) {
        for (auto& b : bulletMgr.pool) {
            if (!b.active) continue;
            float dx = b.x - px;
            float dy = b.y - py;
            float distSq = dx * dx + dy * dy;
            float minDist = b.radius + pradius;
            if (distSq <= minDist * minDist) {
                b.active = false; // 命中した弾を消失
                return true;     // 自機被弾検知
            }
        }
        return false;
    }
};

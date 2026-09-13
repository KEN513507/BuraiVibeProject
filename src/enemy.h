#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>

enum class MovementPattern {
    STRAIGHT_LEFT,
    SIN_WAVE,
    HOMING,
    CIRCLE_ROUND,
    STOP_AND_SHOOT,
    PATROL_JUMP
};

struct EnemyTypeConfig {
    int typeId;
    std::string name;
    int maxHp;
    float speed;
    int collisionRadius;
    MovementPattern pattern;
    int scoreValue;
    int shootInterval; // 0 means no shots
    SDL_Rect spriteSrcRect;
};

struct SpawnEvent {
    int spawnFrame;
    int typeId;
    float spawnX, spawnY; // World coordinates, sprite top-left
    float patrolLeft = 0.0f;
    float patrolRight = 0.0f; // Right edge of the patrol area
};

struct EnemyShot {
    float x, y, vx, vy; // World coordinates
    bool active = true;
};

class Enemy {
public:
    float x, y;
    float startX, startY;
    int hp;
    int frameTimer = 0;
    int hitCooldown = 0;
    bool active = true;
    bool enteredView = false;
    EnemyTypeConfig config;

    Enemy(float posX, float posY, const EnemyTypeConfig& typeCfg,
          float patrolLeft = 0.0f, float patrolRight = 0.0f);

    void Update(float playerX, float playerY);
    void Render(SDL_Renderer* renderer, SDL_Texture* spriteSheet,
                int cameraX = 0, int cameraY = 0) const;
    bool ShouldShoot() const;
    float Width() const { return static_cast<float>(config.spriteSrcRect.w); }
    float Height() const { return static_cast<float>(config.spriteSrcRect.h); }

private:
    float patrolLeft_;
    float patrolRight_;
    float verticalSpeed_ = 0.0f;
    int direction_ = -1;
    int jumpCooldown_ = 0;
};

class EnemyManager {
public:
    bool RegisterEnemyType(const EnemyTypeConfig& config);
    bool AddSpawnEvent(int frame, int typeId, float x, float y,
                       float patrolLeft = 0.0f, float patrolRight = 0.0f);
    void Reset();
    void Update(float playerX, float playerY, int cameraX, int cameraY,
                int viewWidth, int viewHeight);
    void Render(SDL_Renderer* renderer, SDL_Texture* spriteSheet,
                int cameraX, int cameraY) const;
    std::vector<Enemy>& Enemies() { return activeEnemies_; }
    const std::vector<Enemy>& Enemies() const { return activeEnemies_; }
    std::vector<EnemyShot>& Shots() { return shots_; }
    const std::vector<EnemyShot>& Shots() const { return shots_; }
    int CurrentFrame() const { return currentFrame_; }

private:
    std::vector<Enemy> activeEnemies_;
    std::vector<EnemyShot> shots_;
    std::vector<SpawnEvent> spawnTimeline_;
    std::vector<EnemyTypeConfig> typeRegistry_;
    int currentFrame_ = 0;
};

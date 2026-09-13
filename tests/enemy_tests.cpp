#include "../src/enemy.h"

#include <cmath>
#include <iostream>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}
}

int main(int, char**) {
    const EnemyTypeConfig crawler{1, "Crawler", 2, 0.55f, 12,
        MovementPattern::PATROL_JUMP, 100, 0, {0, 0, 32, 32}};
    EnemyManager manager;
    Check(manager.RegisterEnemyType(crawler), "register crawler");
    Check(!manager.RegisterEnemyType(crawler), "reject duplicate type");
    Check(!manager.AddSpawnEvent(1, 99, 208, 128), "reject unknown type");
    Check(!manager.AddSpawnEvent(1, 1, 208, 128, 160, 180), "reject narrow platform");
    Check(manager.AddSpawnEvent(1, 1, 208, 128, 160, 272), "add first-frame spawn");

    manager.Update(50, 100, 0, 0, 320, 204);
    Check(manager.CurrentFrame() == 1 && manager.Enemies().size() == 1,
          "spawn on the scheduled frame");
    const float initialY = manager.Enemies().front().y;
    Check(initialY == 128.0f, "distant player does not trigger a jump");
    bool jumped = false;
    bool movedLeft = false, movedRight = false;
    float previousX = manager.Enemies().front().x;
    for (int i = 0; i < 400; ++i) {
        manager.Update(195, 125, 0, 0, 320, 204);
        const Enemy& enemy = manager.Enemies().front();
        Check(enemy.x >= 160 && enemy.x + enemy.Width() <= 272,
              "crawler stays on supporting platform");
        if (enemy.y < initialY) jumped = true;
        if (enemy.x < previousX) movedLeft = true;
        if (enemy.x > previousX) movedRight = true;
        previousX = enemy.x;
    }
    Check(jumped, "crawler jumps when player approaches");
    Check(movedLeft && movedRight, "crawler patrols in both directions");
    for (int i = 0; i < 100; ++i) manager.Update(20, 20, 0, 0, 320, 204);
    Check(std::abs(manager.Enemies().front().y - initialY) < 0.001f,
          "crawler lands on the obstacle top");

    const EnemyTypeConfig shooter{2, "Shooter", 1, 1.0f, 8,
        MovementPattern::STOP_AND_SHOOT, 50, 10, {0, 0, 16, 16}};
    Check(manager.RegisterEnemyType(shooter), "register shooter");
    Check(manager.AddSpawnEvent(manager.CurrentFrame() + 1, 2, 200, 50),
          "schedule future shooter");
    manager.Update(100, 60, 0, 0, 320, 204);
    for (int i = 0; i < 100; ++i) manager.Update(100, 60, 0, 0, 320, 204);
    Check(!manager.Shots().empty(), "stopped shooter fires on its interval");

    manager.Reset();
    Check(manager.CurrentFrame() == 0 && manager.Enemies().empty() && manager.Shots().empty(),
          "reset clears runtime state");
    manager.Update(50, 100, 0, 0, 320, 204);
    Check(manager.Enemies().size() == 1, "reset replays the original timeline");
    return failures ? 1 : 0;
}


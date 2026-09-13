// OrbitalShieldEnemy = 1 Core + 3 Shields = 4 independent objects = 1 Enemy
// SDL を使わずにビルドできる (OAM までを検査する)
#include "../src/orbital_shield_enemy.h"
#include "../src/boss/joint.h"

#include <cmath>
#include <iostream>
#include <set>
#include <string>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

constexpr float kDt = 1.0f / 60.0f;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kEps = 0.01f;

float Distance(float ax, float ay, float bx, float by) { return std::hypot(ax - bx, ay - by); }

// 十字の位相 (上 / 左 / 下)。画面座標は +y が下なので 上 = 270 度。右 (0 度) は空き枠
constexpr float kCrossPhases[3] = {kPi * 1.5f, kPi, kPi * 0.5f};

// 3 Shield が Core 中心の軌道上で十字配置 (上 / 左 / 下) を保っているか (毎回数式から期待値を作って比べる)
bool ShieldsOnOrbit(const OrbitalShieldEnemy& enemy) {
    const OrbitalShieldTuning& t = enemy.tuning();
    const auto& core = enemy.core();
    if (std::abs(core.world_x - enemy.x) > kEps || std::abs(core.world_y - enemy.y) > kEps) return false;
    for (int i = 0; i < OrbitalShieldEnemy::SHIELD_COUNT; ++i) {
        const float angle = enemy.orbit_angle() + kCrossPhases[i];
        if (std::abs(enemy.shield(i).world_x - (enemy.x + std::cos(angle) * t.orbitRadiusX)) > kEps ||
            std::abs(enemy.shield(i).world_y - (enemy.y + std::sin(angle) * t.orbitRadiusY)) > kEps) return false;
    }
    return true;
}

// 位置へ 4x4 弾を中心合わせで置く
OrbitalBulletResult ShootAt(OrbitalShieldEnemy& enemy, float cx, float cy) {
    return enemy.resolve_player_bullet(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f, 1);
}

void StepToward(OrbitalShieldEnemy& enemy, float px, float py, float dt = kDt) {
    enemy.set_player_position(px, py);
    enemy.update(dt);
}
} // namespace

int main(int, char**) {
    const OrbitalShieldTuning tuning;

    // TEST 1 / 2 / 3: 部位数と所有権
    {
        OrbitalShieldEnemy enemy(200, 100, tuning);
        Check(OrbitalShieldEnemy::SHIELD_COUNT == 3, "SHIELD_COUNT is 3");
        Check(enemy.parts.size() == 4, "TEST1 parts.size() == 4");
        int coreCount = 0, shieldCount = 0;
        std::set<const boss::Part*> shieldPtrs;
        for (const auto& p : enemy.parts) {
            Check(p && p->owner == &enemy, "every part is owned by the enemy via unique_ptr");
            if (p.get() == &enemy.core()) ++coreCount;
            for (int i = 0; i < OrbitalShieldEnemy::SHIELD_COUNT; ++i) {
                if (p.get() == &enemy.shield(i)) { ++shieldCount; shieldPtrs.insert(p.get()); }
            }
        }
        Check(coreCount == 1, "TEST2 Core == 1");
        Check(shieldCount == 3 && shieldPtrs.size() == 3, "TEST3 Shield == 3 (distinct parts)");
        Check(enemy.core().hp == tuning.coreHp && enemy.core().max_hp == tuning.coreHp,
              "Core HP lives in core Part::hp / max_hp");
    }

    // TEST 4: 十字配置 (上 / 左 / 下、右が空き)、spawn 直後から軌道上
    {
        OrbitalShieldEnemy enemy(200, 100, tuning);
        Check(std::abs(enemy.shield_phase_offset(0) - kPi * 1.5f) < 1e-5f &&
              std::abs(enemy.shield_phase_offset(1) - kPi) < 1e-5f &&
              std::abs(enemy.shield_phase_offset(2) - kPi * 0.5f) < 1e-5f,
              "TEST4 phase offsets are up (270), left (180), down (90) degrees");
        Check(ShieldsOnOrbit(enemy), "TEST4 shields on orbit at spawn (not at 0,0)");
        const float d = tuning.coreCellSize * 0.5f + tuning.shieldCellSize * 0.5f + 1.0f;
        Check(tuning.orbitRadiusX == d && tuning.orbitRadiusY == d, "TEST4 orbit radius = core half + shield half + 1");
        Check(std::abs(enemy.shield(0).world_x - 200) < kEps && std::abs(enemy.shield(0).world_y - (100 - d)) < kEps,
              "TEST4 shield 0 starts above core");
        Check(std::abs(enemy.shield(1).world_x - (200 - d)) < kEps && std::abs(enemy.shield(1).world_y - 100) < kEps,
              "TEST4 shield 1 starts left of core");
        Check(std::abs(enemy.shield(2).world_x - 200) < kEps && std::abs(enemy.shield(2).world_y - (100 + d)) < kEps,
              "TEST4 shield 2 starts below core");
        bool rightEmpty = true;
        for (int i = 0; i < 3; ++i) {
            if (enemy.shield(i).world_x > 200 + kEps) rightEmpty = false;
        }
        Check(rightEmpty, "TEST4 no shield on the right of core at spawn");
        // 十字で並べても Shield のセル (16) と Core のセル (32) が重ならない
        Check(d - tuning.shieldCellSize * 0.5f > tuning.coreCellSize * 0.5f, "TEST4 shield cells do not overlap core cell");
    }

    // Chase: 接近と stopDistance
    {
        OrbitalShieldEnemy enemy(250, 100, tuning);
        const float before = Distance(enemy.x, enemy.y, 50, 100);
        StepToward(enemy, 50, 100, 1.0f);
        Check(std::abs((before - Distance(enemy.x, enemy.y, 50, 100)) - tuning.chaseSpeed) < kEps,
              "CHASE parent approaches at chaseSpeed px/sec");
        bool neverInside = true;
        for (int i = 0; i < 60 * 30; ++i) {
            StepToward(enemy, 50, 100);
            if (Distance(enemy.x, enemy.y, 50, 100) < tuning.stopDistance - kEps) neverInside = false;
        }
        Check(neverInside, "CHASE never closer than stopDistance");
        Check(std::abs(Distance(enemy.x, enemy.y, 50, 100) - tuning.stopDistance) < kEps,
              "CHASE settles at stopDistance");
        OrbitalShieldEnemy idle(160, 100, tuning);
        idle.update(kDt);
        Check(idle.x == 160 && idle.y == 100, "CHASE no player position -> no movement");
    }

    // TEST 5 / 6 / 7 / 8: Core 移動中の軌道維持・drift なし・反転・角速度の連続性
    {
        OrbitalShieldEnemy enemy(160, 100, tuning);
        bool structureKept = true, noJump = true, reversed = false, velocityContinuous = true;
        bool crossedZero = false;
        int firstReverseFrame = -1;
        float prevVelocity = 0.0f;
        const float maxStep = tuning.orbitAngularSpeed * kDt * std::max(tuning.orbitRadiusX, tuning.orbitRadiusY)
                              + tuning.chaseSpeed * kDt + kEps;
        const int frames = 60 * 60 * 30; // 30 分
        for (int f = 0; f < frames; ++f) {
            const float px = 160 + std::cos(f * 0.01f) * 120; // Player を動かし続けて Core も動かす
            const float py = 100 + std::sin(f * 0.013f) * 80;
            std::array<std::pair<float, float>, 3> prev;
            for (int i = 0; i < 3; ++i) prev[i] = {enemy.shield(i).world_x, enemy.shield(i).world_y};
            const int prevDir = enemy.orbit_direction();
            StepToward(enemy, px, py);
            if (!ShieldsOnOrbit(enemy)) structureKept = false;
            for (int i = 0; i < 3; ++i) {
                if (Distance(prev[i].first, prev[i].second, enemy.shield(i).world_x, enemy.shield(i).world_y) > maxStep)
                    noJump = false;
            }
            const float v = enemy.orbit_angular_velocity();
            if (std::abs(v - prevVelocity) > tuning.orbitAcceleration * kDt + 1e-4f) velocityContinuous = false;
            if (prevVelocity > 0.0f && v <= 0.0f && std::abs(v) < tuning.orbitAngularSpeed) crossedZero = true;
            prevVelocity = v;
            if (enemy.orbit_direction() != prevDir && !reversed) { reversed = true; firstReverseFrame = f + 1; }
        }
        Check(structureKept, "TEST5 3 shields keep core-centered orbit while core moves (30 min)");
        Check(noJump, "TEST6 no shield position jump across frames (no drift, incl. reversal)");
        Check(enemy.orbit_angle() >= 0.0f && enemy.orbit_angle() < 2 * kPi, "TEST6 orbit angle normalized");
        Check(reversed && std::abs(firstReverseFrame * kDt - tuning.orbitReverseInterval) < 2 * kDt,
              "TEST7 direction reverses after orbitReverseInterval");
        Check(velocityContinuous && crossedZero, "TEST8 angular velocity crosses 0 smoothly (+w -> 0 -> -w)");
    }
    {
        OrbitalShieldEnemy enemy(160, 100, tuning);
        for (int i = 0; i < 120; ++i) enemy.update(kDt);
        Check(enemy.orbit_angular_velocity() > 0.0f, "TEST7 clockwise before reversal");
        for (int i = 0; i < 120; ++i) enemy.update(kDt);
        Check(enemy.orbit_angular_velocity() < 0.0f, "TEST7 counter-clockwise after reversal");
    }

    // TEST 9 / 10: Shield は弾を止め、ダメージを受けない
    {
        OrbitalShieldEnemy enemy(160, 100, tuning);
        Check(ShootAt(enemy, enemy.shield(0).world_x, enemy.shield(0).world_y) ==
              OrbitalBulletResult::BLOCKED_BY_SHIELD, "TEST9 bullet on shield is blocked");
        for (int i = 0; i < 1000; ++i) ShootAt(enemy, enemy.shield(i % 3).world_x, enemy.shield(i % 3).world_y);
        bool shieldsIntact = true;
        for (int i = 0; i < 3; ++i) {
            const auto& s = enemy.shield(i);
            if (s.destroyed || !s.active || s.hp != s.max_hp) shieldsIntact = false;
        }
        Check(shieldsIntact, "TEST10 shields: hp unchanged, destroyed=false, active=true");
        Check(enemy.active && enemy.core().hp == tuning.coreHp, "TEST10 shield hits never reach core hp");

        // Shield と Core の両方に重なる弾でも Shield 優先
        OrbitalShieldTuning tight = tuning;
        tight.orbitRadiusX = tight.orbitRadiusY = 8.0f;
        OrbitalShieldEnemy overlapped(160, 100, tight);
        Check(ShootAt(overlapped, 155, 100) == OrbitalBulletResult::BLOCKED_BY_SHIELD &&
              overlapped.core().hp == tight.coreHp, "TEST9 shield checked before core");
    }

    // TEST 11: Shield の隙間を抜けて Core に当たると HP が減る
    {
        OrbitalShieldEnemy enemy(160, 100, tuning);
        // Shield は上 / 左 / 下。空き枠の右側は Shield に掛からない
        Check(ShootAt(enemy, 160 + 40, 100) == OrbitalBulletResult::MISS, "empty right slot outside core is a miss");
        Check(ShootAt(enemy, 160 - 25, 100) == OrbitalBulletResult::BLOCKED_BY_SHIELD &&
              enemy.core().hp == tuning.coreHp, "left shield blocks a bullet from the left");
        Check(ShootAt(enemy, 160 + 6, 100) == OrbitalBulletResult::CORE_DAMAGED &&
              enemy.core().hp == tuning.coreHp - 1, "TEST11 core hit through the empty right slot reduces core hp");
    }

    // TEST 12 / 13 / 14: Core 死亡で Entity 全体が止まり、OAM に何も出さない
    {
        OrbitalShieldEnemy enemy(160, 100, tuning);
        boss::OAM oam;
        enemy.render(oam);
        Check(oam.entries.size() == 4, "TEST14 alive enemy emits 4 OAM entries");
        int core32 = 0, shield16 = 0;
        for (const auto& e : oam.entries) {
            if (e.tile_id == OrbitalShieldEnemy::CORE_TILE_BASE && e.x == 160 - 16 && e.y == 100 - 16) ++core32;
            if (e.tile_id == OrbitalShieldEnemy::SHIELD_TILE_BASE) ++shield16;
        }
        Check(core32 == 1 && shield16 == 3, "TEST14 OAM = 1 core tile (top-left) + 3 shield tiles");
        int corePlaceholder = 0, shieldPlaceholder = 0;
        for (const auto& e : oam.entries) {
            if (e.placeholder_w == 32 && e.placeholder_h == 32 && e.placeholder_color.r == 0xE4 &&
                e.placeholder_color.g == 0x00 && e.placeholder_color.b == 0x58) ++corePlaceholder;
            if (e.placeholder_w == 16 && e.placeholder_h == 16 && e.placeholder_color.r == 0xBC &&
                e.placeholder_color.g == 0xBC && e.placeholder_color.b == 0xBC) ++shieldPlaceholder;
        }
        Check(corePlaceholder == 1 && shieldPlaceholder == 3,
              "TEST14 OAM entries carry placeholder color/size (core 32 $15, shield 16 $10)");

        for (int i = 0; i < tuning.coreHp - 1; ++i) ShootAt(enemy, 154, 100);
        Check(enemy.active, "core alive until hp reaches 0");
        Check(ShootAt(enemy, 154, 100) == OrbitalBulletResult::CORE_DESTROYED && !enemy.active &&
              enemy.core().hp == 0, "TEST12 core death sets entity active=false");
        Check(ShootAt(enemy, enemy.shield(0).world_x, enemy.shield(0).world_y) == OrbitalBulletResult::MISS,
              "TEST12 dead enemy's shields no longer block");
        const float sx = enemy.x;
        StepToward(enemy, 0, 0);
        Check(enemy.x == sx, "TEST12 dead enemy no longer updates");
        oam.clear();
        enemy.render(oam);
        Check(oam.entries.empty(), "TEST13 dead enemy emits 0 OAM entries");
    }

    // Player body: all four live parts are lethal, using tuned hitboxes rather than sprite cells.
    {
        OrbitalShieldEnemy enemy(160, 100, tuning);
        const int hpBefore = enemy.core().hp;
        Check(enemy.overlaps_player(159, 99, 2, 2), "player AABB hits Core");
        for (int i = 0; i < OrbitalShieldEnemy::SHIELD_COUNT; ++i) {
            const auto& shield = enemy.shield(i);
            const bool hit = enemy.overlaps_player(shield.world_x - 1, shield.world_y - 1, 2, 2);
            if (i == 0) Check(hit, "player AABB hits Shield0");
            if (i == 1) Check(hit, "player AABB hits Shield1");
            if (i == 2) Check(hit, "player AABB hits Shield2");
        }
        Check(!enemy.overlaps_player(10, 10, 2, 2), "player away from all four parts misses");
        Check(!enemy.overlaps_player(170, 99, 1, 2), "Core contact uses tuned 20px hitbox, not 32px sprite");
        Check(enemy.core().hp == hpBefore && enemy.active, "player overlap query does not damage Orbital");
        for (int i = 0; i < tuning.coreHp; ++i) ShootAt(enemy, 166, 100);
        Check(!enemy.active && !enemy.overlaps_player(159, 99, 2, 2),
              "player collision misses after Orbital death");
        Check(!enemy.overlaps_player(enemy.shield(0).world_x - 1, enemy.shield(0).world_y - 1, 2, 2),
              "dead Shield no longer hits player");
    }

    // Offscreen: Core だけが判断基準
    {
        OrbitalShieldEnemy enemy(10, 100, tuning); // 左側の Shield は画面外
        enemy.cull_if_offscreen(0, 0, 320, 204);
        Check(enemy.active, "shield offscreen alone does not despawn");
        enemy.x = -100;
        enemy.cull_if_offscreen(0, 0, 320, 204);
        Check(!enemy.active && !enemy.core().destroyed, "core far offscreen despawns whole enemy (not destroyed)");
    }

    // Animation: 公転速度を変えてもアニメは同じ。タイル番号だけが変わる
    {
        OrbitalShieldTuning fast = tuning;
        fast.orbitAngularSpeed *= 5.0f;
        OrbitalShieldEnemy a(160, 100, tuning), b(160, 100, fast);
        bool same = true, coreAnimated = false, shieldAnimated = false;
        for (int i = 0; i < 600; ++i) {
            a.update(kDt);
            b.update(kDt);
            if (a.core_frame() != b.core_frame() || a.shield_frame() != b.shield_frame()) same = false;
            if (a.core_frame() != 0) coreAnimated = true;
            if (a.shield_frame() != 0) shieldAnimated = true;
            if (a.core().unit.tile_id != OrbitalShieldEnemy::CORE_TILE_BASE + a.core_frame()) same = false;
        }
        Check(same && a.orbit_angle() != b.orbit_angle(), "animation independent of orbit speed");
        Check(coreAnimated && shieldAnimated, "core and shield animate on their own clocks");
    }

    // 既存の FK 経路 (BossEntity 等) が recompute_part_transforms の既定実装で動き続ける
    {
        MultiPartEntity entity;
        entity.x = 50;
        entity.y = 20;
        auto* joint = static_cast<boss::Joint*>(entity.add_part(std::make_unique<boss::Joint>()));
        entity.root_chains.push_back(joint);
        entity.update(kDt);
        Check(std::abs(joint->world_x - 66) < kEps && std::abs(joint->world_y - 20) < kEps,
              "MultiPartEntity default transform still solves FK chains");
    }

    std::cout << (failures ? "FAILED: " : "ALL PASS") << (failures ? std::to_string(failures) : "") << '\n';
    return failures ? 1 : 0;
}

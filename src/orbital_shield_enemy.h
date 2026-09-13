#pragma once

#include <array>

#include "boss/single_part.h"
#include "orbital_bullet_result.h"
#include "rigid_multi_part_entity.h"

// -------------------------------------------------------------
// OrbitalShieldEnemy (仮称) — 通常敵 / 特殊複合敵。Boss ではない (BossEntity は使わない)
// -------------------------------------------------------------
// OrbitalShieldEnemy = 1 Core + 3 Shields = 4 independent objects = 1 Enemy
//   - Core (Parent) だけが HP を持つ。HP の正本は core の Part::hp / Part::max_hp だけ
//   - Shield (Child) ×3 は無敵の Player 弾ブロッカー。HP・死亡・スコアなし
//   - Shield 座標は毎フレーム Core 中心 + 軌道から作り直す。積分しない
//   - 配置は十字の 3 方向 (上 / 左 / 下、90 度間隔)。右が空き枠。spawn 時はこの十字で、公転すると十字ごと回る
//   - どのスプライトも回転させない。回るのは Shield の「座標」だけ
//
// REFERENCE 画像の 4 コマは「アニメーションのコマ」であり、ゲーム内の玉の数ではない。
//
// 独立した 4 つの時計:
//   A: Core のワールド移動 (Player へ低速接近)
//   B: Shield の公転 (orbit angle / angular velocity / reverse timer)
//   C: Core のアニメ   D: Shield のアニメ
//
// 座標は Core / Shield とも「中心」座標。時間の単位は秒。
// 描画は Part::emit_to → OAM だけ。このクラスは SDL の描画関数も PNG も使わない。

// TUNING PLACEHOLDER: ここの値はすべて暫定値で、ゲームデザインの確定値ではない。
struct OrbitalShieldTuning {
    float chaseSpeed = 20.0f;           // px/sec
    float stopDistance = 48.0f;         // px。Core 中心と Player 中心がこれより近ければ止まる
    int coreHp = 12;

    // px。X == Y なら円、違えば楕円。25 = Core セル半分 16 + Shield セル半分 8 + 隙間 1 (十字で重ならない最小距離)
    float orbitRadiusX = 25.0f;
    float orbitRadiusY = 25.0f;
    float orbitAngularSpeed = 1.2f;     // rad/sec
    float orbitAcceleration = 3.0f;     // rad/sec^2。反転時の減速→逆回転の滑らかさ
    float orbitReverseInterval = 2.5f;  // sec

    float coreHitboxWidth = 20.0f;      // px。Core 中心に置く AABB
    float coreHitboxHeight = 20.0f;
    float shieldHitboxWidth = 12.0f;    // px。各 Shield 中心に置く AABB
    float shieldHitboxHeight = 12.0f;

    float coreAnimFps = 8.0f;
    float shieldAnimFps = 12.0f;

    int scoreValue = 1000;
    float despawnMargin = 64.0f;        // px。Core が画面外にこれ以上出たら全体を消す

    // セルの大きさ (NES の 32x32 / 16x16 グリッド)。OAM の左上座標を中心から求めるのに使う
    int coreCellSize = 32;
    int shieldCellSize = 16;
};

class OrbitalShieldEnemy : public RigidMultiPartEntity {
public:
    static constexpr int SHIELD_COUNT = 3;
    static constexpr int PART_COUNT = 1 + SHIELD_COUNT;

    // REFERENCE 画像から読み取ったコマ数 (各 4 コマ)。Canonical アセット完成時に見直す
    static constexpr int CORE_FRAME_COUNT = 4;
    static constexpr int SHIELD_FRAME_COUNT = 4;
    // OAM に載せるタイル番号 = base + アニメのコマ番号
    static constexpr uint16_t CORE_TILE_BASE = 0x40;
    static constexpr uint16_t SHIELD_TILE_BASE = 0x50;

    OrbitalShieldEnemy(float centerX, float centerY, const OrbitalShieldTuning& tuning = {});
    // Part::owner と core_/shields_ が this と部位を指すのでコピー・ムーブしない
    OrbitalShieldEnemy(const OrbitalShieldEnemy&) = delete;
    OrbitalShieldEnemy& operator=(const OrbitalShieldEnemy&) = delete;

    // Player 中心座標。移動ベクトルにだけ使い、向き・描画角度には使わない
    void set_player_position(float playerX, float playerY);

    // chase → orbit → MultiPartEntity::update (位置 → 部位座標 → Part::update → Emitter)
    void update(float dt) override;

    // Shield ×3 → Core の順で判定する。弾の消去と得点加算は呼び出し側が結果を見て行う。
    // bx, by は弾の左上、bw, bh は弾の大きさ
    OrbitalBulletResult resolve_player_bullet(float bx, float by, float bw, float bh, int damage);

    // Read-only Player AABB query against the Core and all three Shields.
    bool overlaps_player(float px, float py, float pw, float ph) const;

    // Parent (Core) だけを基準に画面外判定する (Shield 単独では判定しない)
    void cull_if_offscreen(int cameraX, int cameraY, int viewWidth, int viewHeight);

    const boss::SinglePart& core() const { return *core_; }
    const boss::SinglePart& shield(int index) const { return *shields_[index]; }
    float shield_phase_offset(int index) const { return shieldPhaseOffsets_[index]; }

    float orbit_angle() const { return orbitAngle_; }
    float orbit_angular_velocity() const { return orbitAngularVelocity_; }
    int orbit_direction() const { return orbitDirection_; }
    int core_frame() const { return coreFrame_; }
    int shield_frame() const { return shieldFrame_; }
    const OrbitalShieldTuning& tuning() const { return tuning_; }

protected:
    void recompute_rigid_parts() override;

private:
    void update_chase(float dt);
    void update_orbit(float dt);
    void update_animation(float dt);
    void kill();

    OrbitalShieldTuning tuning_;

    // 非所有ポインタ。所有権は MultiPartEntity::parts の unique_ptr
    boss::SinglePart* core_ = nullptr;
    std::array<boss::SinglePart*, SHIELD_COUNT> shields_{};
    std::array<float, SHIELD_COUNT> shieldPhaseOffsets_{};

    float playerX_ = 0.0f;
    float playerY_ = 0.0f;
    bool hasPlayerPosition_ = false;
    bool enteredView_ = false;

    float orbitAngle_ = 0.0f;
    float orbitAngularVelocity_ = 0.0f; // rad/sec。+ = 時計回り (画面座標は +y が下)
    int orbitDirection_ = 1;            // +1 = clockwise, -1 = counter-clockwise
    float orbitReverseTimer_ = 0.0f;

    float coreAnimTimer_ = 0.0f;
    float shieldAnimTimer_ = 0.0f;
    int coreFrame_ = 0;
    int shieldFrame_ = 0;
};

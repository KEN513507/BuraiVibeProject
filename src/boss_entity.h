#pragma once
#include "multi_part_entity.h"

enum class BossPhase {
    INTRO,          // 出現・配置
    PHASE_1_ARMORED,// 第一形態 (重装甲・全身防御)
    PHASE_2_EXPOSED,// 第二形態 (外殻パージ・アーム乱舞)
    PHASE_3_BERSERK,// 最終形態 (核単体暴走)
    DYING,          // 爆発連鎖・撃破
    DEAD
};

class BossEntity : public MultiPartEntity {
public:
    BossPhase phase = BossPhase::INTRO;
    float frenzy_rate = 0.0f;       // 0.0(通常) 〜 1.0(最大発狂)
    float breathe_timer = 0.0f;     // 呼吸サイン波タイマー
    float breathe_scale = 1.0f;     // 呼吸による拡縮率
    int boss_timer = 0;

    int total_max_hp = 100;
    int current_hp = 100;

    void update(float dt) override;
    virtual void on_phase_transition(BossPhase new_phase) = 0;
    virtual void on_part_destroyed(boss::Part* destroyed_part) = 0;
};

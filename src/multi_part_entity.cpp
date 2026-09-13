#include "multi_part_entity.h"

void MultiPartEntity::update(float dt) {
    if (!active) return;

    // ルート座標の更新
    x += vx * dt;
    y += vy * dt;

    // 部位座標の再計算 (Part::update より前)
    recompute_part_transforms();

    // 各部位の個別状態更新
    for (auto& p : parts) {
        if (p && p->active && !p->destroyed) {
            p->update(dt);
        }
    }

    // Emitterのクールダウン減算
    for (auto& em : emitters) {
        if (em.cooldown_frames > 0) {
            em.cooldown_frames--;
        }
    }
}

void MultiPartEntity::render(boss::OAM& oam) {
    if (!active) return;

    // 全部位のOAMスプライト要求を発行
    for (auto& p : parts) {
        if (p && p->active && !p->destroyed) {
            p->emit_to(oam);
        }
    }
}

void MultiPartEntity::recompute_part_transforms() {
    // FKチェーンの更新 (基点から順次先端へ角度・座標を伝播)
    for (auto* chain : root_chains) {
        if (chain) {
            chain->solve_fk(x, y, 0.0f);
        }
    }
}

boss::Part* MultiPartEntity::add_part(std::unique_ptr<boss::Part> part) {
    part->owner = this;
    boss::Part* raw = part.get();
    parts.push_back(std::move(part));
    return raw;
}

int MultiPartEntity::count_alive_parts() const {
    int count = 0;
    for (const auto& p : parts) {
        if (p && p->active && !p->destroyed) {
            count++;
        }
    }
    return count;
}

#pragma once
#include "multi_part_entity.h"

// FK を使わず「親の位置 + 数式 (パラメトリック変換)」で子の座標を毎フレーム作り直す MultiPartEntity。
// 全部位が同じ座標に固まるという意味ではない。前フレームの子座標は使わない (drift させない)。
class RigidMultiPartEntity : public MultiPartEntity {
protected:
    // 呼び出し順: ルート座標更新 → FK (あれば) → recompute_rigid_parts → Part::update
    void recompute_part_transforms() override {
        MultiPartEntity::recompute_part_transforms();
        recompute_rigid_parts();
    }

    virtual void recompute_rigid_parts() = 0;
};

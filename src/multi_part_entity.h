#pragma once
#include <vector>
#include <memory>
#include "entity.h"
#include "boss/part.h"
#include "boss/chain_node.h"
#include "boss/emitter.h"

class MultiPartEntity : public Entity {
public:
    // 1. 集約軸 (所有・ライフサイクル・破壊判定)
    std::vector<std::unique_ptr<boss::Part>> parts;

    // 2. チェーン軸 (FK走査用・生ポインタ参照)
    std::vector<boss::ChainNode*> root_chains;

    // 3. 発射点 (描画なし・別管理)
    std::vector<boss::Emitter> emitters;

    void update(float dt) override;
    void render(boss::OAM& oam) override;

    // ヘルパー: 部位追加とオーナー設定
    boss::Part* add_part(std::unique_ptr<boss::Part> part);

    // 生存部位数のカウント
    int count_alive_parts() const;

protected:
    // update() の中で「ルート座標更新の後・Part::update の前」に呼ばれる部位座標の再計算。
    // 既定は FK チェーンの伝播。数式で子座標を決める派生クラスはここを拡張する
    virtual void recompute_part_transforms();
};

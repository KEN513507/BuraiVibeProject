#pragma once

// assets/sprites/player/player.json (contract_version 1) の読み取り。SDL に依存しない。
// 画像サイズ・色数上限・向きごとのファイル名はすべて JSON から読み、ここでは固定しない。
// 書式の正本は tools/player_asset_validator.py。ランタイムはそのうち読み込みに必要な項目だけ扱う。

#include <array>
#include <string>

#include "direction8.h"

struct PlayerSpriteContract {
    int contractVersion = 0;
    int cellWidth = 0;
    int cellHeight = 0;
    int maxOpaqueColorsPerSprite = 0;
    std::array<std::string, 8> files;  // Direction8 の値で引く (UP=0 から時計回り)
};

// player.json の directions キー名 (Direction8 の並び順)
extern const std::array<const char*, 8> PLAYER_DIRECTION_KEYS;

// 成功時 true。失敗時は error に理由を入れ、out は変更しない
bool ParsePlayerSpriteContract(const std::string& json, PlayerSpriteContract& out, std::string& error);

// 照準ベクトル (画面座標: +y が下) を最も近い 8 方向に丸める。ゼロベクトルは fallback を返す
Direction8 Direction8FromVector(float x, float y, Direction8 fallback);

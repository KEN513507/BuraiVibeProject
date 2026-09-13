#pragma once

// 自機スプライトの読み込みと描画。
// 画像は差し替え前提: assets/sprites/player/player.json と、そこに書かれた 8 枚の PNG を同じ名前で上書きすれば反映される。
// 不正な画像 (サイズ違い・半透明・色数超過) は縮小や減色をせずに拒否し、その向きはプレースホルダー描画に戻す。

#include <SDL2/SDL.h>

#include <array>
#include <string>

#include "direction8.h"
#include "player_sprite_contract.h"

struct PlayerSprites {
    bool contractLoaded = false;
    PlayerSpriteContract contract;
    std::array<SDL_Texture*, 8> textures{};  // Direction8 の値で引く。nullptr はプレースホルダー
    int loadedCount = 0;
};

PlayerSprites LoadPlayerSprites(SDL_Renderer* renderer);
void FreePlayerSprites(PlayerSprites& sprites);

// 自機の当たり判定矩形の中心にセルを合わせて描く (等倍・回転なし)。
// その向きの画像が無ければ何も描かず false を返すので、呼び出し側でプレースホルダーを描くこと
bool DrawPlayerSprite(SDL_Renderer* renderer, const PlayerSprites& sprites, Direction8 dir,
                      float hitX, float hitY, float hitW, float hitH);

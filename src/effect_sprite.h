#pragma once

#include <SDL2/SDL.h>
#include <array>
#include <string>
#include <vector>

// 爆発エフェクト 16x16 の 4 フレーム。assets/sprites/effects/effects.json
// と同名の PNG を差し替えれば反映される。
struct EffectSprites {
    bool contractLoaded = false;
    int cellWidth = 0;
    int cellHeight = 0;
    std::vector<SDL_Texture*> frames;   // 順番は effects.json の frames 通り
    int loadedCount = 0;
};

EffectSprites LoadEffectSprites(SDL_Renderer* renderer);
void FreeEffectSprites(EffectSprites& sprites);

// x, y are the top-left of the destination rect in screen coordinates. scale >= 1.
// The frame is drawn at cellWidth*scale x cellHeight*scale using nearest-neighbor.
// (中心合わせは呼び出し側で行う。回転なし。scale < 1 は 1 として扱う)
// frame が範囲外、またはテクスチャが無ければ false を返す。
bool DrawEffectFrame(SDL_Renderer* renderer, const EffectSprites& sprites,
                     int frame, float x, float y, int scale);

#pragma once

#include <SDL2/SDL.h>

#include "boss/oam.h"

// OAM → 画面。Entity / Part は SDL を触らず OAM に積むだけで、SDL 描画はこのブリッジだけが行う。
// CHR タイル描画はまだ無いので、各 OAMEntry が持つ placeholder_color / placeholder_w / placeholder_h の
// 単色矩形 (開発用プレースホルダー) で描く。
// OAMEntry の x, y はセル左上。整数ピクセルにスナップし、拡縮・回転はしない。
void DrawOamPlaceholders(SDL_Renderer* renderer, const boss::OAM& oam, int cameraX = 0, int cameraY = 0);

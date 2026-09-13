#pragma once
#include <cstdint>

namespace boss {

// SDL非依存のRGB（プレースホルダ塗りつけ用）
struct Rgb {
    uint8_t r = 200;
    uint8_t g = 200;
    uint8_t b = 200;
};

// tile_id == この値なら CHRバンクを使わず placeholder_color で矩形塗り
constexpr uint16_t PLACEHOLDER_TILE_ID = 0xFFFF;

struct RenderUnit {
    float rel_x = 0.0f;
    float rel_y = 0.0f;
    uint16_t tile_id = 0;
    uint8_t palette_id = 0;
    bool flip_h = false;
    bool flip_v = false;
    bool visible = true;

    Rgb placeholder_color{200, 200, 200};
    int placeholder_w = 8;
    int placeholder_h = 8;
};

} // namespace boss

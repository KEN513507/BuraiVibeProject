#include "oam_sdl_bridge.h"

#include <cmath>

void DrawOamPlaceholders(SDL_Renderer* renderer, const boss::OAM& oam, int cameraX, int cameraY) {
    for (const auto& entry : oam.entries) {
        if (entry.placeholder_w <= 0 || entry.placeholder_h <= 0) continue;
        SDL_Rect dst{static_cast<int>(std::lround(entry.x)) - cameraX,
                     static_cast<int>(std::lround(entry.y)) - cameraY,
                     entry.placeholder_w, entry.placeholder_h};
        const boss::Rgb& c = entry.placeholder_color;
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(renderer, &dst);
    }
}

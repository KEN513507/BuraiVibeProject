#include "oam_sdl_bridge.h"

#include <cmath>

OamTileBank::~OamTileBank() { Clear(); }

void OamTileBank::Install(uint16_t tileId, SDL_Texture* texture, int width, int height) {
    if (!texture || tileId == boss::PLACEHOLDER_TILE_ID) return;
    auto& tile = tiles_[tileId];
    if (tile.texture) SDL_DestroyTexture(tile.texture);
    tile = {texture, width, height};
}

const OamTile* OamTileBank::Find(uint16_t tileId) const {
    const auto it = tiles_.find(tileId);
    return it == tiles_.end() ? nullptr : &it->second;
}

void OamTileBank::Clear() {
    for (auto& [id, tile] : tiles_) {
        if (tile.texture) SDL_DestroyTexture(tile.texture);
    }
    tiles_.clear();
}

void DrawOamTiles(SDL_Renderer* renderer, const boss::OAM& oam, const OamTileBank& tiles,
                  int cameraX, int cameraY) {
    if (!renderer) return;
    for (const auto& entry : oam.entries) {
        const int x = static_cast<int>(std::lround(entry.x)) - cameraX;
        const int y = static_cast<int>(std::lround(entry.y)) - cameraY;
        const OamTile* tile = entry.tile_id == boss::PLACEHOLDER_TILE_ID ? nullptr : tiles.Find(entry.tile_id);
        if (tile && tile->texture) {
            SDL_Rect dst{x, y, tile->width, tile->height};
            if (entry.flip_h || entry.flip_v) {
                const auto flip = static_cast<SDL_RendererFlip>(
                    (entry.flip_h ? SDL_FLIP_HORIZONTAL : 0) | (entry.flip_v ? SDL_FLIP_VERTICAL : 0));
                SDL_RenderCopyEx(renderer, tile->texture, nullptr, &dst, 0.0, nullptr, flip);
            } else {
                SDL_RenderCopy(renderer, tile->texture, nullptr, &dst);
            }
            continue;
        }
        if (entry.placeholder_w <= 0 || entry.placeholder_h <= 0) continue;
        SDL_Rect dst{x, y, entry.placeholder_w, entry.placeholder_h};
        const boss::Rgb& c = entry.placeholder_color;
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_RenderFillRect(renderer, &dst);
    }
}

#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <unordered_map>

#include "boss/oam.h"

struct OamTile {
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
};

// Owns loaded textures; OAM itself stays SDL-independent.
class OamTileBank {
public:
    OamTileBank() = default;
    ~OamTileBank();
    OamTileBank(const OamTileBank&) = delete;
    OamTileBank& operator=(const OamTileBank&) = delete;

    void Install(uint16_t tileId, SDL_Texture* texture, int width, int height);
    const OamTile* Find(uint16_t tileId) const;
    void Clear();

private:
    std::unordered_map<uint16_t, OamTile> tiles_;
};

// Render a real texture for a mapped tile_id. The explicit placeholder ID and
// missing tiles use the OAM entry's placeholder color/size as fallback.
void DrawOamTiles(SDL_Renderer* renderer, const boss::OAM& oam, const OamTileBank& tiles,
                  int cameraX = 0, int cameraY = 0);

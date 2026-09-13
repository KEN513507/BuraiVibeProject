#include <SDL2/SDL.h>

#include <cstdint>
#include <cmath>
#include <iostream>
#include <vector>

#include "../src/oam_sdl_bridge.h"
#include "../src/orbital_enemy_field.h"
#include "../src/orbital_shield_enemy.h"
#include "../src/orbital_sprite.h"

namespace {
int failures = 0;
void Check(bool ok, const char* message) {
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; ++failures; }
}
}

int main(int, char**) {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL init: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, 320, 240, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!renderer) {
        std::cerr << "SDL software renderer: " << SDL_GetError() << '\n';
        if (surface) SDL_FreeSurface(surface);
        SDL_Quit();
        return 1;
    }

    OamTileBank tiles;
    Check(LoadOrbitalTiles(renderer, tiles) == 8, "eight canonical Orbital textures load");
    for (int frame = 0; frame < 4; ++frame) {
        const OamTile* core = tiles.Find(OrbitalShieldEnemy::CORE_TILE_BASE + frame);
        const OamTile* shield = tiles.Find(OrbitalShieldEnemy::SHIELD_TILE_BASE + frame);
        Check(core && core->width == 32 && core->height == 32, "Core tile maps to a 32x32 texture");
        Check(shield && shield->width == 16 && shield->height == 16, "Shield tile maps to a 16x16 texture");
    }

    OrbitalShieldEnemy enemy(100, 100);
    boss::OAM oam;
    enemy.render(oam);
    boss::OAMEntry placeholder;
    placeholder.x = 250;
    placeholder.y = 50;
    placeholder.tile_id = boss::PLACEHOLDER_TILE_ID;
    placeholder.placeholder_color = {255, 0, 0};
    placeholder.placeholder_w = placeholder.placeholder_h = 6;
    oam.submit(placeholder);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    DrawOamTiles(renderer, oam, tiles);
    std::vector<uint8_t> pixels(320 * 240 * 4);
    Check(SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, pixels.data(), 320 * 4) == 0,
          "software renderer readback");
    auto isColor = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        const size_t offset = (y * 320 + x) * 4;
        return pixels[offset] == r && pixels[offset + 1] == g && pixels[offset + 2] == b;
    };
    if (!isColor(100, 100, 0xFC, 0xFC, 0xFC)) {
        const size_t offset = (100 * 320 + 100) * 4;
        std::cerr << "Core center RGB=" << int(pixels[offset]) << "," << int(pixels[offset + 1])
                  << "," << int(pixels[offset + 2]) << '\n';
    }
    Check(isColor(100, 100, 0xFC, 0xFC, 0xFC), "Core OAM tile renders a white center");
    Check(isColor(84, 84, 0, 0, 0), "transparent Core corner is not a filled rectangle");
    const int shieldBlueX = static_cast<int>(std::lround(enemy.shield(0).world_x - 8.0f)) + 8;
    const int shieldBlueY = static_cast<int>(std::lround(enemy.shield(0).world_y - 8.0f)) + 6;
    Check(isColor(shieldBlueX, shieldBlueY, 0x00, 0x78, 0xF8), "Shield OAM tile renders its blue center");
    Check(isColor(250, 50, 255, 0, 0), "explicit placeholder tile uses fallback rectangle");

    OrbitalEnemyField field;
    field.Spawn(160, 100);
    Check(field.HitsPlayer(159, 99, 2, 2), "OrbitalEnemyField exposes Core Player collision");
    int score = 0;
    for (int hit = 0; hit < OrbitalShieldTuning{}.coreHp; ++hit)
        field.ResolvePlayerBullet(164, 98, 4, 4, 1, score);
    Check(!field.HitsPlayer(159, 99, 2, 2), "field ignores an Orbital after Core death");
    field.Clear();
    Check(!field.HitsPlayer(159, 99, 2, 2), "cleared field misses Player");

    tiles.Clear();
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
    std::cout << (failures ? "FAILED: " : "ALL PASS") << (failures ? std::to_string(failures) : "") << '\n';
    return failures ? 1 : 0;
}

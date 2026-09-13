#include "orbital_sprite.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>

#include "oam_sdl_bridge.h"
#include "orbital_shield_enemy.h"

namespace {
constexpr const char* kAssetDir = "assets/sprites/orbital";
constexpr std::array<uint32_t, 4> kCorePalette{0x000000, 0xE40058, 0xF85898, 0xFCFCFC};
constexpr std::array<uint32_t, 4> kShieldPalette{0x000000, 0xFCA044, 0x0078F8, 0x3CBCFC};

std::filesystem::path ResolveAssetPath(const std::string& relativePath) {
    std::filesystem::path base;
    if (char* basePath = SDL_GetBasePath()) {
        base = std::filesystem::u8path(basePath);
        SDL_free(basePath);
    }
    return base / std::filesystem::u8path(relativePath);
}

bool ValidSurface(SDL_Surface* surface, int width, int height,
                  const std::array<uint32_t, 4>& palette) {
    if (surface->w != width || surface->h != height) return false;
    bool opaque = false;
    if (SDL_LockSurface(surface) != 0) return false;
    bool valid = true;
    for (int y = 0; y < height && valid; ++y) {
        const auto* row = static_cast<const uint8_t*>(surface->pixels) + y * surface->pitch;
        for (int x = 0; x < width; ++x) {
            const auto* pixel = row + x * 4;  // SDL_PIXELFORMAT_RGBA32 byte order
            if (pixel[3] == 0) {
                if (pixel[0] || pixel[1] || pixel[2]) valid = false;
                continue;
            }
            if (pixel[3] != 255) { valid = false; continue; }
            opaque = true;
            const uint32_t rgb = (uint32_t(pixel[0]) << 16) | (uint32_t(pixel[1]) << 8) | pixel[2];
            bool inPalette = false;
            for (uint32_t color : palette) {
                if (rgb == color) { inPalette = true; break; }
            }
            if (!inPalette) valid = false;
        }
    }
    SDL_UnlockSurface(surface);
    return valid && opaque;
}

bool LoadFrame(SDL_Renderer* renderer, OamTileBank& tiles, uint16_t tileId,
               const std::string& fileName, int size, const std::array<uint32_t, 4>& palette) {
    const std::string path = ResolveAssetPath(std::string(kAssetDir) + "/" + fileName).u8string();
    SDL_Surface* loaded = IMG_Load(path.c_str());
    if (!loaded) {
        std::cerr << "[orbital] missing/invalid " << path << ": " << IMG_GetError() << '\n';
        return false;
    }
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loaded);
    if (!rgba) {
        std::cerr << "[orbital] conversion failed " << path << ": " << SDL_GetError() << '\n';
        return false;
    }
    if (!ValidSurface(rgba, size, size, palette)) {
        std::cerr << "[orbital] rejected " << path << " (size, alpha, or NES palette)\n";
        SDL_FreeSurface(rgba);
        return false;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, rgba);
    SDL_FreeSurface(rgba);
    if (!texture) {
        std::cerr << "[orbital] texture failed " << path << ": " << SDL_GetError() << '\n';
        return false;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    tiles.Install(tileId, texture, size, size);
    return true;
}
} // namespace

int LoadOrbitalTiles(SDL_Renderer* renderer, OamTileBank& tiles) {
    if (!renderer) return 0;
    int loaded = 0;
    for (int frame = 0; frame < OrbitalShieldEnemy::CORE_FRAME_COUNT; ++frame) {
        loaded += LoadFrame(renderer, tiles, OrbitalShieldEnemy::CORE_TILE_BASE + frame,
                            "core_" + std::to_string(frame) + ".png", 32, kCorePalette);
    }
    for (int frame = 0; frame < OrbitalShieldEnemy::SHIELD_FRAME_COUNT; ++frame) {
        loaded += LoadFrame(renderer, tiles, OrbitalShieldEnemy::SHIELD_TILE_BASE + frame,
                            "shield_" + std::to_string(frame) + ".png", 16, kShieldPalette);
    }
    std::cout << "[orbital] canonical tiles loaded " << loaded << "/8\n";
    return loaded;
}

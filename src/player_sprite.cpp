#include "player_sprite.h"

#include <SDL2/SDL_image.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>

namespace {

// アセットパスはここだけに置く
constexpr const char* kPlayerAssetDir = "assets/sprites/player";
constexpr const char* kPlayerContractFile = "player.json";

std::filesystem::path ResolveAssetPath(const std::string& relativePath) {
    std::filesystem::path base;
    if (char* basePath = SDL_GetBasePath()) {
        base = std::filesystem::u8path(basePath);
        SDL_free(basePath);
    }
    return base / std::filesystem::u8path(relativePath);
}

// ランタイムでできる範囲のゲートチェック。NES パレット照合など完全な検査は tools/player_asset_validator.py で行う
bool CheckSurface(SDL_Surface* rgba, const PlayerSpriteContract& contract, std::string& error) {
    if (rgba->w != contract.cellWidth || rgba->h != contract.cellHeight) {
        error = "size " + std::to_string(rgba->w) + "x" + std::to_string(rgba->h) + ", contract requires " +
                std::to_string(contract.cellWidth) + "x" + std::to_string(contract.cellHeight);
        return false;
    }
    std::set<Uint32> opaqueColors;
    int partialAlpha = 0;
    SDL_LockSurface(rgba);
    for (int y = 0; y < rgba->h; ++y) {
        const Uint8* row = static_cast<Uint8*>(rgba->pixels) + y * rgba->pitch;
        for (int x = 0; x < rgba->w; ++x) {
            const Uint8* px = row + x * 4;  // SDL_PIXELFORMAT_RGBA32 はバイト順 R,G,B,A
            if (px[3] == 255) opaqueColors.insert((Uint32(px[0]) << 16) | (Uint32(px[1]) << 8) | px[2]);
            else if (px[3] != 0) ++partialAlpha;
        }
    }
    SDL_UnlockSurface(rgba);
    if (partialAlpha > 0) {
        error = std::to_string(partialAlpha) + " pixel(s) with partial alpha";
        return false;
    }
    if (opaqueColors.empty()) {
        error = "image is empty";
        return false;
    }
    if (static_cast<int>(opaqueColors.size()) > contract.maxOpaqueColorsPerSprite) {
        error = std::to_string(opaqueColors.size()) + " opaque colors, limit is " +
                std::to_string(contract.maxOpaqueColorsPerSprite);
        return false;
    }
    return true;
}

SDL_Texture* LoadDirectionTexture(SDL_Renderer* renderer, const PlayerSpriteContract& contract,
                                  const std::string& fileName) {
    const std::filesystem::path path = ResolveAssetPath(std::string(kPlayerAssetDir) + "/" + fileName);
    const std::string pathUtf8 = path.u8string();
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || std::filesystem::file_size(path, ec) == 0) {
        std::cout << "[player] sprite missing, using placeholder: " << pathUtf8 << std::endl;
        return nullptr;
    }

    SDL_Surface* loaded = IMG_Load(pathUtf8.c_str());
    if (!loaded) {
        std::cerr << "[player] load failed: " << pathUtf8 << " : " << IMG_GetError() << std::endl;
        return nullptr;
    }
    SDL_Surface* rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(loaded);
    if (!rgba) {
        std::cerr << "[player] convert failed: " << pathUtf8 << " : " << SDL_GetError() << std::endl;
        return nullptr;
    }

    std::string error;
    SDL_Texture* texture = nullptr;
    if (!CheckSurface(rgba, contract, error)) {
        // 縮小・減色のフォールバックはしない
        std::cerr << "[player] rejected " << pathUtf8 << " : " << error << std::endl;
    } else {
        texture = SDL_CreateTextureFromSurface(renderer, rgba);
        if (!texture) std::cerr << "[player] texture failed: " << pathUtf8 << " : " << SDL_GetError() << std::endl;
        else SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    SDL_FreeSurface(rgba);
    return texture;
}

} // namespace

PlayerSprites LoadPlayerSprites(SDL_Renderer* renderer) {
    PlayerSprites sprites;
    const std::filesystem::path contractPath =
        ResolveAssetPath(std::string(kPlayerAssetDir) + "/" + kPlayerContractFile);

    std::ifstream file(contractPath, std::ios::binary);
    if (!file) {
        std::cerr << "[player] contract not found, using placeholder: " << contractPath.u8string() << std::endl;
        return sprites;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string error;
    if (!ParsePlayerSpriteContract(buffer.str(), sprites.contract, error)) {
        std::cerr << "[player] invalid contract, using placeholder: " << contractPath.u8string() << " : " << error
                  << std::endl;
        return sprites;
    }
    sprites.contractLoaded = true;

    for (size_t i = 0; i < sprites.textures.size(); ++i) {
        sprites.textures[i] = LoadDirectionTexture(renderer, sprites.contract, sprites.contract.files[i]);
        if (sprites.textures[i]) ++sprites.loadedCount;
    }
    std::cout << "[player] sprites loaded " << sprites.loadedCount << "/8 (cell " << sprites.contract.cellWidth << "x"
              << sprites.contract.cellHeight << ")" << std::endl;
    return sprites;
}

void FreePlayerSprites(PlayerSprites& sprites) {
    for (SDL_Texture*& texture : sprites.textures) {
        if (texture) SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    sprites.loadedCount = 0;
    sprites.contractLoaded = false;
}

bool DrawPlayerSprite(SDL_Renderer* renderer, const PlayerSprites& sprites, Direction8 dir,
                      float hitX, float hitY, float hitW, float hitH) {
    SDL_Texture* texture = sprites.textures[static_cast<size_t>(dir)];
    if (!texture) return false;
    // 整数ピクセルにスナップしてから左上を求める (サブピクセル表示・拡縮・回転をしない)
    const int cx = static_cast<int>(std::lround(hitX + hitW * 0.5f));
    const int cy = static_cast<int>(std::lround(hitY + hitH * 0.5f));
    const int w = sprites.contract.cellWidth;
    const int h = sprites.contract.cellHeight;
    SDL_Rect dst{cx - w / 2, cy - h / 2, w, h};
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    return true;
}

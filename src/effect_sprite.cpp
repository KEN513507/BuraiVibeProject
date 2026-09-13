#include "effect_sprite.h"

#include <SDL2/SDL_image.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

// アセットパスはここだけに置く
constexpr const char* kEffectAssetDir = "assets/sprites/effects";
constexpr const char* kEffectContractFile = "effects.json";

std::filesystem::path ResolveAssetPath(const std::string& relativePath) {
    std::filesystem::path base;
    if (char* basePath = SDL_GetBasePath()) {
        base = std::filesystem::u8path(basePath);
        SDL_free(basePath);
    }
    return base / std::filesystem::u8path(relativePath);
}

struct EffectContract {
    int contractVersion = 0;
    int cellWidth = 0;
    int cellHeight = 0;
    int maxOpaqueColors = 0;
    std::vector<std::string> frames;
};

// effects.json 専用の最小 JSON リーダー。平坦なオブジェクト (int / 文字列配列) だけを読む
class ContractReader {
public:
    explicit ContractReader(const std::string& text) : s_(text) {}

    bool Parse(EffectContract& out, std::string& error) {
        if (!Expect('{', error)) return false;
        SkipWs();
        if (Peek() == '}') { ++pos_; return true; }
        while (true) {
            std::string key;
            if (!ReadString(key, error) || !Expect(':', error)) return false;
            SkipWs();
            if (Peek() == '[') {
                std::vector<std::string> values;
                if (!ReadStringArray(values, error)) return false;
                if (key == "frames") out.frames = std::move(values);  // palette は読み捨てる
            } else {
                int value = 0;
                if (!ReadInt(value, error)) return false;
                if (key == "contract_version") out.contractVersion = value;
                else if (key == "cell_width") out.cellWidth = value;
                else if (key == "cell_height") out.cellHeight = value;
                else if (key == "max_opaque_colors_per_sprite") out.maxOpaqueColors = value;
            }
            SkipWs();
            if (Peek() == ',') { ++pos_; continue; }
            return Expect('}', error);
        }
    }

private:
    char Peek() const { return pos_ < s_.size() ? s_[pos_] : '\0'; }
    void SkipWs() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }
    bool Expect(char c, std::string& error) {
        SkipWs();
        if (Peek() != c) {
            error = std::string("expected '") + c + "' at offset " + std::to_string(pos_);
            return false;
        }
        ++pos_;
        return true;
    }
    bool ReadString(std::string& out, std::string& error) {
        if (!Expect('"', error)) return false;
        const size_t end = s_.find('"', pos_);
        if (end == std::string::npos) {
            error = "unterminated string";
            return false;
        }
        out = s_.substr(pos_, end - pos_);
        pos_ = end + 1;
        return true;
    }
    bool ReadInt(int& out, std::string& error) {
        SkipWs();
        size_t used = 0;
        try {
            out = std::stoi(s_.substr(pos_), &used);
        } catch (...) {
            error = "expected integer at offset " + std::to_string(pos_);
            return false;
        }
        pos_ += used;
        return true;
    }
    bool ReadStringArray(std::vector<std::string>& out, std::string& error) {
        if (!Expect('[', error)) return false;
        SkipWs();
        if (Peek() == ']') { ++pos_; return true; }
        while (true) {
            std::string value;
            if (!ReadString(value, error)) return false;
            out.push_back(value);
            SkipWs();
            if (Peek() == ',') { ++pos_; continue; }
            return Expect(']', error);
        }
    }

    const std::string& s_;
    size_t pos_ = 0;
};

} // namespace

EffectSprites LoadEffectSprites(SDL_Renderer* renderer) {
    EffectSprites sprites;
    const std::filesystem::path contractPath =
        ResolveAssetPath(std::string(kEffectAssetDir) + "/" + kEffectContractFile);

    std::ifstream file(contractPath, std::ios::binary);
    if (!file) {
        SDL_Log("[effect] contract not found: %s", contractPath.u8string().c_str());
        return sprites;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    EffectContract contract;
    std::string error;
    if (!ContractReader(text).Parse(contract, error)) {
        SDL_Log("[effect] invalid contract %s : %s", contractPath.u8string().c_str(), error.c_str());
        return sprites;
    }
    if (contract.contractVersion != 1 || contract.cellWidth <= 0 || contract.cellHeight <= 0 ||
        contract.frames.empty()) {
        SDL_Log("[effect] invalid contract %s : version/cell/frames", contractPath.u8string().c_str());
        return sprites;
    }
    sprites.contractLoaded = true;
    sprites.cellWidth = contract.cellWidth;
    sprites.cellHeight = contract.cellHeight;

    for (const std::string& name : contract.frames) {
        const std::string pathUtf8 = ResolveAssetPath(std::string(kEffectAssetDir) + "/" + name).u8string();
        SDL_Surface* loaded = IMG_Load(pathUtf8.c_str());
        if (!loaded) {
            SDL_Log("[effect] load failed: %s : %s", pathUtf8.c_str(), IMG_GetError());
            continue;
        }
        SDL_Surface* rgba = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_RGBA32, 0);
        SDL_FreeSurface(loaded);
        if (!rgba) {
            SDL_Log("[effect] convert failed: %s : %s", pathUtf8.c_str(), SDL_GetError());
            continue;
        }
        if (rgba->w != sprites.cellWidth || rgba->h != sprites.cellHeight) {
            // 縮小・拡大のフォールバックはしない
            SDL_Log("[effect] rejected %s : size %dx%d, contract requires %dx%d", pathUtf8.c_str(), rgba->w,
                    rgba->h, sprites.cellWidth, sprites.cellHeight);
            SDL_FreeSurface(rgba);
            continue;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, rgba);
        SDL_FreeSurface(rgba);
        if (!texture) {
            SDL_Log("[effect] texture failed: %s : %s", pathUtf8.c_str(), SDL_GetError());
            continue;
        }
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        sprites.frames.push_back(texture);
        ++sprites.loadedCount;
    }
    std::cout << "[effect] frames loaded " << sprites.loadedCount << "/" << contract.frames.size() << " (cell "
              << sprites.cellWidth << "x" << sprites.cellHeight << ")" << std::endl;
    return sprites;
}

void FreeEffectSprites(EffectSprites& sprites) {
    for (SDL_Texture*& texture : sprites.frames) {
        if (texture) SDL_DestroyTexture(texture);
        texture = nullptr;
    }
    sprites.frames.clear();
    sprites.loadedCount = 0;
}

bool DrawEffectFrame(SDL_Renderer* renderer, const EffectSprites& sprites,
                     int frame, float x, float y, int scale) {
    if (frame < 0 || frame >= static_cast<int>(sprites.frames.size())) return false;
    SDL_Texture* texture = sprites.frames[static_cast<size_t>(frame)];
    if (!texture) return false;
    if (scale < 1) scale = 1;
    // 整数倍の最近傍拡大。フィルタは main.cpp が SDL_HINT_RENDER_SCALE_QUALITY="0" をテクスチャ生成前に
    // 設定済みなので、ここではグローバル状態を変えない
    SDL_Rect dst = { static_cast<int>(x), static_cast<int>(y), sprites.cellWidth * scale, sprites.cellHeight * scale };
    SDL_RenderCopy(renderer, texture, nullptr, &dst);
    return true;
}

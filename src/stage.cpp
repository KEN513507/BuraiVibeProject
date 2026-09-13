#include "stage.h"

#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <system_error>

StageAssetSet g_stageAssets[STAGE_COUNT] = {
    { StageTheme::ALIEN_BASE_ORGANIC_MECH,
      "assets/tilesets/stage1_bg_16.png", "assets/tilesets/stage1_obstacle_16.png" },
    { StageTheme::FRONTLINE_METAL_BASE,
      "assets/tilesets/stage2_bg_16.png", "assets/tilesets/stage2_obstacle_16.png" },
    { StageTheme::HIVE_FLESH,
      "assets/tilesets/stage3_bg_16.png", "assets/tilesets/stage3_obstacle_16.png" },
    { StageTheme::ASTEROID_FIELD,
      "assets/tilesets/stage4_bg_16.png", "assets/tilesets/stage4_obstacle_16.png" },
};

namespace {

// プレースホルダーの単色。NES (2C02) パレットから選び、ステージと用途が見分けられる組み合わせにしている
struct PlaceholderColors {
    SDL_Color background;
    SDL_Color obstacle;
};
const PlaceholderColors PLACEHOLDER_COLORS[STAGE_COUNT] = {
    { { 0x00, 0x68, 0x00, 255 }, { 0x58, 0xD8, 0x54, 255 } }, // ステージ1: 暗い緑 $0A / 明るい緑 $2A
    { { 0x00, 0x00, 0xBC, 255 }, { 0x3C, 0xBC, 0xFC, 255 } }, // ステージ2: 暗い青 $02 / 明るい青 $21
    { { 0x88, 0x14, 0x00, 255 }, { 0xF8, 0x78, 0x58, 255 } }, // ステージ3: 暗い赤 $07 / 明るい肉色 $26
    { { 0x50, 0x30, 0x00, 255 }, { 0xAC, 0x7C, 0x00, 255 } }, // ステージ4: 暗い茶 $08 / 明るい茶 $18
};

std::filesystem::path ResolveAssetPath(const std::string& relativePath) {
    std::filesystem::path base;
    if (char* basePath = SDL_GetBasePath()) {
        base = std::filesystem::u8path(basePath);
        SDL_free(basePath);
    }
    return base / std::filesystem::u8path(relativePath);
}

SDL_Surface* CreateSolidTileSurface(SDL_Color color) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, TILE_SIZE, TILE_SIZE, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return nullptr;
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a));
    return surface;
}

// 画像が無ければ単色 PNG を生成して保存し、テクスチャとして読み込む
SDL_Texture* LoadTileTexture(SDL_Renderer* renderer, const std::string& relativePath, SDL_Color placeholderColor) {
    const std::filesystem::path path = ResolveAssetPath(relativePath);
    const std::string pathUtf8 = path.u8string();

    std::error_code ec;
    const bool missing = !std::filesystem::exists(path, ec) || std::filesystem::file_size(path, ec) == 0;
    if (missing) {
        std::filesystem::create_directories(path.parent_path(), ec);
        SDL_Surface* placeholder = CreateSolidTileSurface(placeholderColor);
        if (placeholder && IMG_SavePNG(placeholder, pathUtf8.c_str()) == 0) {
            std::cout << "[stage] placeholder generated: " << pathUtf8 << std::endl;
        } else {
            std::cerr << "[stage] placeholder save failed: " << pathUtf8 << " : " << IMG_GetError() << std::endl;
        }
        SDL_FreeSurface(placeholder);
    }

    SDL_Surface* surface = IMG_Load(pathUtf8.c_str());
    if (!surface) {
        // 保存も読み込みもできない場合でも、メモリ上の単色タイルで検証を続けられるようにする
        std::cerr << "[stage] load failed: " << pathUtf8 << " : " << IMG_GetError() << std::endl;
        surface = CreateSolidTileSurface(placeholderColor);
        if (!surface) return nullptr;
    }
    if (surface->w != TILE_SIZE || surface->h != TILE_SIZE) {
        std::cerr << "[stage] warning: " << pathUtf8 << " is " << surface->w << "x" << surface->h
                  << " (tile must be " << TILE_SIZE << "x" << TILE_SIZE << ")" << std::endl;
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
}

int FloorDiv(int a, int b) {
    return (a >= 0) ? a / b : -((-a + b - 1) / b);
}

uint8_t TileAt(const StageMap& map, int tx, int ty) {
    if (tx < 0 || ty < 0 || tx >= map.widthTiles || ty >= map.heightTiles) return 0; // マップ外は通行可
    return map.tileGrid[ty * map.widthTiles + tx];
}

} // namespace

StageTextures LoadStageTextures(SDL_Renderer* renderer, int stageIndex) {
    const StageAssetSet& assets = g_stageAssets[stageIndex];
    const PlaceholderColors& colors = PLACEHOLDER_COLORS[stageIndex];
    StageTextures textures;
    textures.backgroundTile = LoadTileTexture(renderer, assets.backgroundTileImagePath, colors.background);
    textures.obstacleTile = LoadTileTexture(renderer, assets.obstacleTileImagePath, colors.obstacle);
    return textures;
}

void DestroyStageTextures(StageTextures& textures) {
    if (textures.backgroundTile) SDL_DestroyTexture(textures.backgroundTile);
    if (textures.obstacleTile) SDL_DestroyTexture(textures.obstacleTile);
    textures = StageTextures{};
}

StageMap BuildTestStage() {
    StageMap map;
    map.widthTiles = 64;
    map.heightTiles = 26;
    map.tileGrid.assign(map.widthTiles * map.heightTiles, 0);

    auto setObstacle = [&](int tx, int ty) {
        if (tx >= 0 && ty >= 0 && tx < map.widthTiles && ty < map.heightTiles) {
            map.tileGrid[ty * map.widthTiles + tx] = 1;
        }
    };

    // 天井と床
    for (int tx = 0; tx < map.widthTiles; ++tx) {
        setObstacle(tx, 0);
        setObstacle(tx, map.heightTiles - 1);
    }

    // 巡回する虫の足場。上面 y=160、幅112px。敵の座標はマップ基準。
    for (int tx = 10; tx <= 16; ++tx) setObstacle(tx, 10);

    // 開始地点付近に足場を置き、その先に隙間付きの柱と浮き障害物を並べる
    for (int px = 24; px < map.widthTiles; px += 10) {
        int gapTop = 2 + ((px / 10) * 3) % 6; // 初期スクロール位置でも見える高さに 5 タイルの隙間
        for (int ty = 1; ty < map.heightTiles - 1; ++ty) {
            if (ty >= gapTop && ty < gapTop + 5) continue;
            setObstacle(px, ty);
            setObstacle(px + 1, ty);
        }
        int blockY = 2 + ((px / 10) * 5) % 9;
        setObstacle(px + 5, blockY);
        setObstacle(px + 6, blockY);
        setObstacle(px + 5, blockY + 1);
        setObstacle(px + 6, blockY + 1);
    }
    return map;
}

bool AdvanceScroll(StageMap& map, int viewWidth, int viewHeight) {
    const int maxX = std::max(0, map.widthTiles * TILE_SIZE - viewWidth);
    const int maxY = std::max(0, map.heightTiles * TILE_SIZE - viewHeight);
    int nx = map.scrollX, ny = map.scrollY;
    switch (map.currentDirection) {
    case ScrollDirection::RIGHT: nx++; break;
    case ScrollDirection::LEFT:  nx--; break;
    case ScrollDirection::DOWN:  ny++; break;
    case ScrollDirection::UP:    ny--; break;
    }
    nx = std::clamp(nx, 0, maxX);
    ny = std::clamp(ny, 0, maxY);
    const bool moved = (nx != map.scrollX || ny != map.scrollY);
    map.scrollX = nx;
    map.scrollY = ny;
    return moved;
}

std::pair<float, float> ScrollPushVector(ScrollDirection dir) {
    switch (dir) {
    case ScrollDirection::RIGHT: return { -1.0f, 0.0f }; // 地形が左へ流れる
    case ScrollDirection::LEFT:  return { 1.0f, 0.0f };
    case ScrollDirection::DOWN:  return { 0.0f, -1.0f }; // 地形が上へ流れる
    case ScrollDirection::UP:    return { 0.0f, 1.0f };
    }
    return { 0.0f, 0.0f };
}

void RenderStage(SDL_Renderer* renderer, const StageMap& map,
                 const StageTextures& textures,
                 int logicalWidth, int logicalHeight, int scale) {
    const int tilePx = TILE_SIZE * scale;
    const int firstCol = FloorDiv(map.scrollX, TILE_SIZE);
    const int firstRow = FloorDiv(map.scrollY, TILE_SIZE);
    const int offsetX = -(map.scrollX - firstCol * TILE_SIZE) * scale;
    const int offsetY = -(map.scrollY - firstRow * TILE_SIZE) * scale;
    // 画面に入るタイル数 (端数切り上げ) + 1 タイルのバッファ
    const int cols = (logicalWidth + tilePx - 1) / tilePx + 1;
    const int rows = (logicalHeight + tilePx - 1) / tilePx + 1;

    for (int r = 0; r < rows; ++r) {
        const int ty = firstRow + r;
        if (ty < 0 || ty >= map.heightTiles) continue;
        for (int c = 0; c < cols; ++c) {
            const int tx = firstCol + c;
            if (tx < 0 || tx >= map.widthTiles) continue;
            SDL_Texture* texture = (map.tileGrid[ty * map.widthTiles + tx] == 1)
                ? textures.obstacleTile : textures.backgroundTile;
            if (!texture) continue;
            SDL_Rect dst = { offsetX + c * tilePx, offsetY + r * tilePx, tilePx, tilePx };
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
        }
    }
}

bool CheckStageCollision(const StageMap& map, float x, float y, float w, float h) {
    if (w <= 0.0f || h <= 0.0f) return false;
    // 矩形は右端・下端を含まない半開区間 [x, x+w) × [y, y+h) として扱う
    const float mapX = x + map.scrollX;
    const float mapY = y + map.scrollY;
    const int left   = (int)std::floor(mapX / TILE_SIZE);
    const int right  = (int)std::floor((mapX + w - 0.001f) / TILE_SIZE);
    const int top    = (int)std::floor(mapY / TILE_SIZE);
    const int bottom = (int)std::floor((mapY + h - 0.001f) / TILE_SIZE);
    for (int ty = top; ty <= bottom; ++ty) {
        for (int tx = left; tx <= right; ++tx) {
            if (TileAt(map, tx, ty) == 1) return true;
        }
    }
    return false;
}

// 押し潰し判定
// 障害物に触れているだけでは死なない。スクロールで迫ってくる画面端に CRUSH_MARGIN 以内まで
// 追い詰められ、かつ反対側 (スクロールで障害物が押し寄せてくる側) の面が障害物に接している
// 「画面端と障害物に挟まれた」状態のときだけ true。
//   RIGHT スクロール: 左端が迫る  → 左端との距離 <= MARGIN かつ 右面が障害物に接触
//   LEFT  スクロール: 右端が迫る  → 右端との距離 <= MARGIN かつ 左面が障害物に接触
//   DOWN  スクロール: 上端が迫る  → 上端との距離 <= MARGIN かつ 下面が障害物に接触
//   UP    スクロール: 下端が迫る  → 下端との距離 <= MARGIN かつ 上面が障害物に接触
// 接触は、面の外側 1px の帯が障害物と重なるか (めり込み中も含む) で調べる。
bool IsPlayerCrushed(const StageMap& map, float px, float py, float pw, float ph,
                     int logicalWidth, int logicalHeight, int scrollDirectionAsInt) {
    switch (scrollDirectionAsInt) {
    case static_cast<int>(ScrollDirection::RIGHT):
        return px <= CRUSH_MARGIN
            && CheckStageCollision(map, px + pw, py, 1.0f, ph);
    case static_cast<int>(ScrollDirection::LEFT):
        return (logicalWidth - (px + pw)) <= CRUSH_MARGIN
            && CheckStageCollision(map, px - 1.0f, py, 1.0f, ph);
    case static_cast<int>(ScrollDirection::DOWN):
        return py <= CRUSH_MARGIN
            && CheckStageCollision(map, px, py + ph, pw, 1.0f);
    case static_cast<int>(ScrollDirection::UP):
        return (logicalHeight - (py + ph)) <= CRUSH_MARGIN
            && CheckStageCollision(map, px, py - 1.0f, pw, 1.0f);
    default:
        return false;
    }
}

namespace {
const float TWO_PI = 6.28318530718f;

RotatingChainObstacle MakeChain(float pivotX, float pivotY, float innerRadius, float radiusStep,
                                float angularSpeed, float ballRadius) {
    RotatingChainObstacle chain;
    chain.pivotX = pivotX;
    chain.pivotY = pivotY;
    chain.angularSpeed = angularSpeed;
    chain.baseAngle = 0.0f;
    chain.ballCount = CHAIN_BALL_COUNT;
    chain.innerRadius = innerRadius;
    chain.radiusStep = radiusStep;
    chain.ballRadius = ballRadius;
    return chain;
}
} // namespace

void UpdateRotatingChain(RotatingChainObstacle& chain) {
    chain.baseAngle += chain.angularSpeed;
    // 角度が大きくなりすぎないよう2πで正規化(浮動小数点誤差の蓄積防止)
    if (chain.baseAngle > TWO_PI) chain.baseAngle -= TWO_PI;
    if (chain.baseAngle < -TWO_PI) chain.baseAngle += TWO_PI;
}

std::pair<float, float> GetChainBallPosition(const RotatingChainObstacle& chain, int i) {
    // 角度は全球共通 (1 本の針)。ピボットからの距離だけが i で変わる
    float radius = chain.innerRadius + i * chain.radiusStep;
    float x = chain.pivotX + radius * std::cos(chain.baseAngle);
    // 画面座標は +y が下向きなので、sin を引いて「角度が増える = 画面上で反時計回り」にそろえる
    float y = chain.pivotY - radius * std::sin(chain.baseAngle);
    return { x, y };
}

std::vector<LethalCircle> GetChainLethalCircles(const RotatingChainObstacle& chain) {
    std::vector<LethalCircle> circles;
    circles.reserve(chain.ballCount);
    for (int i = 0; i < chain.ballCount; ++i) {
        auto [bx, by] = GetChainBallPosition(chain, i);
        circles.push_back({ bx, by, chain.ballRadius });
    }
    return circles;
}

void InitHazardsForStage(StageMap& map, StageTheme theme) {
    map.hazardChains.clear();

    switch (theme) {
    // 引数: pivotX, pivotY, innerRadius, radiusStep, angularSpeed, ballRadius
    // 先端の球の中心 = innerRadius + 4 * radiusStep
    // radiusStep は球の直径 (16px) 以上にして、5 個の球が重ならず見分けられるようにする
    case StageTheme::ALIEN_BASE_ORGANIC_MECH: // ステージ1: デバッグ用 (先端 16 + 4*16 = 80px)
        map.hazardChains.push_back(MakeChain(200.0f, 100.0f, 16.0f, 16.0f, 0.02f, 8.0f));
        break;

    case StageTheme::FRONTLINE_METAL_BASE: // ステージ2
    case StageTheme::HIVE_FLESH:           // ステージ3
    case StageTheme::ASTEROID_FIELD:       // ステージ4 (先端 26 + 4*16 = 90px)
        map.hazardChains.push_back(MakeChain(200.0f, 80.0f, 26.0f, 16.0f, 0.02f, 8.0f));   // 左上: 反時計回り
        map.hazardChains.push_back(MakeChain(600.0f, 400.0f, 26.0f, 16.0f, -0.02f, 8.0f)); // 右下: 時計回り
        break;
    }
}

void RenderHazardChains(SDL_Renderer* renderer, const StageMap& map) {
    for (const auto& chain : map.hazardChains) {
        const float pivotX = chain.pivotX - map.scrollX;
        const float pivotY = chain.pivotY - map.scrollY;

        // ピボットから最も内側の球までのアーム (4px 間隔の 2x2 ドット)
        const float ux = std::cos(chain.baseAngle), uy = -std::sin(chain.baseAngle); // GetChainBallPosition と同じ向き
        SDL_SetRenderDrawColor(renderer, 0xBC, 0xBC, 0xBC, 255); // NES $10
        for (float t = 4.0f; t < chain.innerRadius - chain.ballRadius; t += 4.0f) {
            SDL_Rect link = { (int)(pivotX + ux * t) - 1, (int)(pivotY + uy * t) - 1, 2, 2 };
            SDL_RenderFillRect(renderer, &link);
        }

        for (int i = 0; i < chain.ballCount; ++i) {
            auto [bx, by] = GetChainBallPosition(chain, i);
            const float cx = bx - map.scrollX;
            const float cy = by - map.scrollY;

            // 球 (ベタ塗りの円。水平線で塗りつぶし、アンチエイリアスなし)。
            // 直径 = 2*ballRadius px に収める (半径8なら 16x16 のスプライト枠内)
            SDL_SetRenderDrawColor(renderer, 0xF8, 0x38, 0x00, 255); // NES $16
            const float r = chain.ballRadius;
            const int top = (int)std::lround(cy - r);
            for (int j = 0; j < (int)(2 * r); ++j) {
                const float dyc = j - r + 0.5f;
                const float half = std::sqrt(std::max(0.0f, r * r - dyc * dyc));
                const int x0 = (int)std::lround(cx - half);
                const int x1 = (int)std::lround(cx + half) - 1;
                if (x1 >= x0) SDL_RenderDrawLine(renderer, x0, top + j, x1, top + j);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0x7C, 0x7C, 0x7C, 255); // NES $00
        SDL_Rect pivot = { (int)pivotX - 2, (int)pivotY - 2, 4, 4 };
        SDL_RenderFillRect(renderer, &pivot);
    }
}

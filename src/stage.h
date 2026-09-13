#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "lethal.h"

// タイルサイズは 16x16px 固定。NES 実機のメタタイル (8x8 タイルを 2x2 並べた基本ブロック) に
// 対応する、本プロジェクトのファミコン準拠要件のコア条件なので変更しないこと。
constexpr int TILE_SIZE = 16;

// 押し潰し判定で「画面端に追い詰められた」とみなす距離 (px)。16px タイルのうち 5 ドット分
constexpr float CRUSH_MARGIN = TILE_SIZE * 5.0f / 16.0f;

enum class ScrollDirection : uint8_t { RIGHT = 0, LEFT = 1, UP = 2, DOWN = 3 };

// 回転即死障害物。時計の針のように、ピボットから伸びる 1 本のアームに球が等間隔で並び、
// アーム全体がピボットを中心に常に同じ方向へ全周回転する (往復なし)。
// 全ての球は同じ角度 (baseAngle) を共有し、ピボットからの距離だけが球ごとに異なる。
// 座標はマップ座標なので、地形と一緒にスクロールする。
struct RotatingChainObstacle {
    float pivotX = 0.0f, pivotY = 0.0f;
    float angularSpeed = 0.0f; // rad/フレーム。正=反時計回り、負=時計回り (画面上の見た目の向き)
    float baseAngle = 0.0f;    // アーム全体の現在角度 (全球共通)
    int ballCount = 0;         // CHAIN_BALL_COUNT 固定
    float innerRadius = 0.0f;  // ピボットから最も近い球までの距離
    float radiusStep = 0.0f;   // 球と球の間隔 (半径方向)
    float ballRadius = 0.0f;   // 各球の当たり判定半径
};

constexpr int CHAIN_BALL_COUNT = 5;

struct StageMap {
    int widthTiles, heightTiles;
    std::vector<uint8_t> tileGrid; // 0=背景(通行可), 1=障害物(当たり判定あり)
    int scrollX = 0, scrollY = 0;  // カメラ左上のマップ内ピクセル座標
    ScrollDirection currentDirection = ScrollDirection::RIGHT;
    std::vector<RotatingChainObstacle> hazardChains;
    // 復活ポイント（スクロール座標、進行方向のプライマリ軸 = 現状は X）
    std::vector<int> checkpoints;
    int lastCheckpointIndex = 0;
};

struct StageTextures {
    SDL_Texture* backgroundTile = nullptr;
    SDL_Texture* obstacleTile   = nullptr;
};

enum class StageTheme : uint8_t {
    ALIEN_BASE_ORGANIC_MECH = 0, // ステージ1: 有機×機械混合のエイリアン基地
    FRONTLINE_METAL_BASE    = 1, // ステージ2: 金属質な前線基地内部
    HIVE_FLESH              = 2, // ステージ3: エイリアンの巣窟、肉壁
    ASTEROID_FIELD          = 3, // ステージ4: 隕石帯
};

struct StageAssetSet {
    StageTheme theme;
    std::string backgroundTileImagePath;
    std::string obstacleTileImagePath;
};

constexpr int STAGE_COUNT = 4;
extern StageAssetSet g_stageAssets[STAGE_COUNT];

// タイル画像を読み込む。パスは実行ファイルのディレクトリ基準。
// 画像が無い (または 0 バイトの) 場合は 16x16 単色のプレースホルダー PNG を生成して保存してから読み込む。
// IMG_Init(IMG_INIT_PNG) 済みであること。
StageTextures LoadStageTextures(SDL_Renderer* renderer, int stageIndex);
void DestroyStageTextures(StageTextures& textures);

// スクロール・当たり判定の検証用マップ
StageMap BuildTestStage();

// currentDirection へ 1px スクロールする。マップ端で止まり、動いたときだけ true
bool AdvanceScroll(StageMap& map, int viewWidth, int viewHeight);

// スクロールで障害物が自機を押す向き (画面座標)
std::pair<float, float> ScrollPushVector(ScrollDirection dir);

// 画面範囲 + 1タイル分のバッファでタイルを描画する。
// logicalWidth / logicalHeight は描画先の範囲 (描画座標)、scale はタイル1枚の拡大率。
// SDL_RenderSetLogicalSize で拡大している場合は scale = 1 を渡す。
void RenderStage(SDL_Renderer* renderer, const StageMap& map,
                 const StageTextures& textures,
                 int logicalWidth, int logicalHeight, int scale);

// 以下の判定関数の x, y は画面座標 (内部で scrollX / scrollY を足してマップ座標に変換する)

// 矩形が障害物 (値1) と重なっているか
bool CheckStageCollision(const StageMap& map, float x, float y, float w, float h);

// スクロールで迫ってくる画面端と障害物に挟まれて押し潰されたか
bool IsPlayerCrushed(const StageMap& map, float px, float py, float pw, float ph,
                     int logicalWidth, int logicalHeight, int scrollDirectionAsInt);

// ---- 回転即死障害物 ----

// baseAngle += angularSpeed。2πで正規化して誤差の蓄積を防ぐ
void UpdateRotatingChain(RotatingChainObstacle& chain);

// i 番目の球の中心 (マップ座標)。角度は全球共通の baseAngle、距離 = innerRadius + i * radiusStep
std::pair<float, float> GetChainBallPosition(const RotatingChainObstacle& chain, int i);

// チェーン内の全球を LethalCircle (マップ座標) に変換
std::vector<LethalCircle> GetChainLethalCircles(const RotatingChainObstacle& chain);

// ステージ1: デバッグ用に 1 本 (反時計回り、アームの先端 = ピボットから 80px)
// ステージ2〜4: 左上に反時計回り・右下に時計回り (各 アームの先端 = 90px)
// 球はどのステージも 5 個
void InitHazardsForStage(StageMap& map, StageTheme theme);

// 回転障害物を描画 (スクロール分ずらして画面座標に変換する)
void RenderHazardChains(SDL_Renderer* renderer, const StageMap& map);

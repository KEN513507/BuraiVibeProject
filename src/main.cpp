#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <string>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include "stage1_score.h"
#include "stage.h"
#include "direction8.h"
#include "enemy_bullet.h"
#include "lethal.h"
#include "player_sprite.h"
#include "effect_sprite.h"
#include "orbital_enemy_field.h"
#include "orbital_sprite.h"
#include "oam_sdl_bridge.h"

const int LOGICAL_WIDTH = 320;
const int LOGICAL_HEIGHT = 240;
const int WINDOW_WIDTH = 960;
const int WINDOW_HEIGHT = 720;
const int PLAY_AREA_HEIGHT = 204;

enum GameState { TITLE, PLAYING, PAUSED_DEATH, GAME_OVER, SOUND_TEST };
enum WeaponType { WEAPON_LASER, WEAPON_RING, WEAPON_MISSILE };

// ============================================================
// Sound Test 状態管理
// ============================================================
struct SoundTestState {
    static constexpr int TRACK_COUNT = 10;
    int  selected    = 1;      // 初期選択は Stage 1
    bool playing     = false;
    int  step        = 0;      // 現在の16分音符ステップ
    int  frameInStep = 0;      // 0..4 (STAGE1_FRAMES_PER_TICK 依存)

    static const char* TrackName(int i) {
        static const char* names[TRACK_COUNT] = {
            "TITLE",
            "STAGE 1",
            "STAGE 2",
            "MAZE",
            "STAGE 3",
            "BOSS",
            "STAGE 4",
            "STAGE 5",
            "LAST STAGE",
            "GAME OVER"
        };
        return (i >= 0 && i < TRACK_COUNT) ? names[i] : "???";
    }

    bool IsPlayable(int i) const { return i == 1; } // Phase 1 は Stage 1 のみ再生可能
};

struct PlayerBullet {
    float x, y;
    float vx, vy;
    WeaponType type;
    bool active;
};

struct Entity {
    float x, y, width, height, speed;
    bool active;
};

// -------------------------------------------------------------
// NES 2A03 APU 音源 Engine
// -------------------------------------------------------------
struct APU {
    float bgmPulse1Freq = 0.0f;
    float bgmPulse2Freq = 0.0f;
    float bgmTriangleFreq = 0.0f;
    bool  bgmNoiseTrigger = false;

    int sePulseTimer = 0;
    float sePulseFreq = 0.0f;
    int seNoiseTimer = 0;

    float phasePulse1 = 0.0f;
    float phasePulse2 = 0.0f;
    float phaseTriangle = 0.0f;
} g_apu;

void PlaySE_Shot() {
    g_apu.sePulseFreq = 880.0f;
    g_apu.sePulseTimer = (int)(44100 * 0.08f);
}

void PlaySE_Explosion() {
    g_apu.seNoiseTimer = (int)(44100 * 0.25f);
}

void AudioCallback(void* userdata, Uint8* stream, int len) {
    int16_t* buffer = (int16_t*)stream;
    int samples = len / sizeof(int16_t);
    const float sampleRate = 44100.0f;

    for (int i = 0; i < samples; ++i) {
        float mix = 0.0f;

        if (g_apu.sePulseTimer > 0) {
            g_apu.phasePulse1 += (2.0f * M_PI * g_apu.sePulseFreq) / sampleRate;
            mix += (std::sin(g_apu.phasePulse1) > 0.0f ? 0.15f : -0.15f);
            g_apu.sePulseTimer--;
            g_apu.sePulseFreq *= 0.9995f;
        } else if (g_apu.bgmPulse1Freq > 0.0f) {
            g_apu.phasePulse1 += (2.0f * M_PI * g_apu.bgmPulse1Freq) / sampleRate;
            mix += (std::sin(g_apu.phasePulse1) > 0.0f ? 0.08f : -0.08f);
        }

        if (g_apu.bgmPulse2Freq > 0.0f) {
            g_apu.phasePulse2 += (2.0f * M_PI * g_apu.bgmPulse2Freq) / sampleRate;
            mix += (std::sin(g_apu.phasePulse2) > 0.0f ? 0.05f : -0.05f);
        }

        if (g_apu.bgmTriangleFreq > 0.0f) {
            g_apu.phaseTriangle += (2.0f * M_PI * g_apu.bgmTriangleFreq) / sampleRate;
            float tri = std::abs(std::fmod(g_apu.phaseTriangle / M_PI, 2.0f) - 1.0f) * 2.0f - 1.0f;
            mix += tri * 0.12f;
        }

        if (g_apu.seNoiseTimer > 0) {
            float noise = ((rand() % 2000) / 1000.0f) - 1.0f;
            mix += noise * 0.18f;
            g_apu.seNoiseTimer--;
        } else if (g_apu.bgmNoiseTrigger) {
            float noise = ((rand() % 2000) / 1000.0f) - 1.0f;
            mix += noise * 0.04f;
        }

        buffer[i] = (int16_t)(mix * 32767.0f);
    }
}

// 5x5 フォント
static const unsigned char font5x5[][5] = {
    {0x1E, 0x05, 0x05, 0x05, 0x1E}, {0x1F, 0x15, 0x15, 0x15, 0x0A},
    {0x0E, 0x11, 0x11, 0x11, 0x0A}, {0x1F, 0x11, 0x11, 0x11, 0x0E},
    {0x1F, 0x15, 0x15, 0x15, 0x11}, {0x1F, 0x05, 0x05, 0x05, 0x01},
    {0x0E, 0x11, 0x15, 0x15, 0x1D}, {0x1F, 0x04, 0x04, 0x04, 0x1F},
    {0x11, 0x11, 0x1F, 0x11, 0x11}, {0x08, 0x10, 0x10, 0x10, 0x0F},
    {0x1F, 0x04, 0x0A, 0x11, 0x00}, {0x1F, 0x10, 0x10, 0x10, 0x10},
    {0x1F, 0x02, 0x0C, 0x02, 0x1F}, {0x1F, 0x02, 0x04, 0x08, 0x1F},
    {0x0E, 0x11, 0x11, 0x11, 0x0E}, {0x1F, 0x05, 0x05, 0x05, 0x02},
    {0x0E, 0x11, 0x15, 0x09, 0x16}, {0x1F, 0x05, 0x05, 0x0D, 0x12},
    {0x12, 0x15, 0x15, 0x15, 0x09}, {0x01, 0x01, 0x1F, 0x01, 0x01},
    {0x0F, 0x10, 0x10, 0x10, 0x0F}, {0x07, 0x08, 0x10, 0x08, 0x07},
    {0x1F, 0x08, 0x06, 0x08, 0x1F}, {0x11, 0x0A, 0x04, 0x0A, 0x11},
    {0x01, 0x02, 0x1C, 0x02, 0x01}, {0x11, 0x13, 0x15, 0x19, 0x11},
    {0x0E, 0x11, 0x11, 0x11, 0x0E}, {0x00, 0x12, 0x1F, 0x10, 0x00},
    {0x12, 0x19, 0x15, 0x13, 0x10}, {0x11, 0x15, 0x15, 0x15, 0x0A},
    {0x07, 0x04, 0x04, 0x1F, 0x04}, {0x17, 0x15, 0x15, 0x15, 0x09},
    {0x0E, 0x15, 0x15, 0x15, 0x08}, {0x01, 0x1D, 0x03, 0x01, 0x01},
    {0x0A, 0x15, 0x15, 0x15, 0x0A}, {0x02, 0x15, 0x15, 0x15, 0x0E},
    {0x00, 0x00, 0x00, 0x00, 0x00}
};

void DrawText(SDL_Renderer* renderer, const std::string& text, int startX, int startY, int scale, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int currentX = startX;
    for (char c : text) {
        c = std::toupper(c);
        int index = 36;
        if (c >= 'A' && c <= 'Z') index = c - 'A';
        else if (c >= '0' && c <= '9') index = 26 + (c - '0');

        for (int col = 0; col < 5; ++col) {
            unsigned char bitCol = font5x5[index][col];
            for (int row = 0; row < 5; ++row) {
                if (bitCol & (1 << row)) {
                    SDL_Rect pixel = { currentX + col * scale, startY + row * scale, scale, scale };
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        currentX += 6 * scale;
    }
}

std::string FormatScore(int score) {
    std::ostringstream ss;
    ss << std::setw(6) << std::setfill('0') << score;
    return ss.str();
}

char FormatHexLevel(int lvl) {
    if (lvl >= 10) return 'A';
    if (lvl < 0) return '0';
    return '0' + lvl;
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) return 1;

    SDL_AudioSpec wantedSpec;
    SDL_zero(wantedSpec);
    wantedSpec.freq = 44100;
    wantedSpec.format = AUDIO_S16SYS;
    wantedSpec.channels = 1;
    wantedSpec.samples = 1024;
    wantedSpec.callback = AudioCallback;

    if (SDL_OpenAudio(&wantedSpec, NULL) < 0) return 1;
    SDL_PauseAudio(0);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_Window* window = SDL_CreateWindow("Burai Vibe - Native Stage Collision", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderSetLogicalSize(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT);

    StageMap stageMap = BuildTestStage();
    // 復活ポイントを10個配置（進行方向プライマリ軸 = 現状スクロールX）
    {
        const int mapLen = stageMap.widthTiles * TILE_SIZE;
        for (int i = 0; i < 10; ++i) {
            stageMap.checkpoints.push_back(mapLen * i / 10);
        }
    }
    InitHazardsForStage(stageMap, StageTheme::ALIEN_BASE_ORGANIC_MECH);
    StageTextures stageTextures = LoadStageTextures(renderer, 0);
    PlayerSprites playerSprites = LoadPlayerSprites(renderer);  // 画像差し替え後は F5 で再読み込み
    EffectSprites effectSprites = LoadEffectSprites(renderer);  // 爆発 4 フレーム (F5 で再読み込み)
    OamTileBank orbitalTiles;
    LoadOrbitalTiles(renderer, orbitalTiles);
    struct EffectInstance { float x, y; int frame; int timer; };
    std::vector<EffectInstance> effects;

    GameState state = TITLE;
    SoundTestState soundTest;
    WeaponType currentWeapon = WEAPON_RING;

    int score = 0;
    int highScore = 152900;
    int lives = 5;
    bool debugInvincible = false;  // デバッグ用: F1 で ON/OFF (通常プレイは一発接触で死亡)
    int cobaltMeter = 4;
    int laserLevel = 1;
    int ringLevel = 1;
    int missileLevel = 1;

    Entity player = { 20, 100, 16, 16, 2.0f, true };
    float aimDirX = 1.0f;
    float aimDirY = 0.0f;

    std::vector<Entity> enemies;
    OrbitalEnemyField orbitalEnemies;  // 1 Core + 3 Shields = 4 objects = 1 Enemy (OAM 描画)
    std::vector<PlayerBullet> pBullets;

    bool running = true;
    SDL_Event e;
    Uint32 frameCount = 0;
    int bgmStep = 0;

    auto spawnEnemies = [&](int count) {
        enemies.clear();
        for (int i = 0; i < count; ++i) {
            enemies.push_back({ (float)(220 + (i % 4) * 22), (float)(30 + (i / 4) * 35), 14, 14, 0.8f, true });
        }
        orbitalEnemies.Clear();
        orbitalEnemies.Spawn(260.0f, 120.0f);  // 中心座標
    };

    // 安全な復帰座標を探索（鉄球・敵を避ける）。
    // スクロール維持仕様のため、進行度によっては (20,100) が鉄球の位置と重なる。
    // 左端から右へ候補を順に試し、衝突しない最初の場所に置く。
    auto safeRespawn = [&]() {
        const float ry = 100.0f;
        // 候補を右に広げる（全滅時は 260 まで試す）
        const float candX[] = {
            20.0f, 40.0f, 60.0f, 80.0f, 100.0f, 120.0f, 140.0f,
            160.0f, 180.0f, 200.0f, 220.0f, 240.0f, 260.0f
        };

        auto isSafeAt = [&](float cx, float cy) -> bool {
            // 1) 敵との重なり
            for (const auto& en : enemies) {
                if (!en.active) continue;
                if (cx < en.x + en.width && cx + player.width > en.x &&
                    cy < en.y + en.height && cy + player.height > en.y) {
                    return false;
                }
            }
            if (orbitalEnemies.HitsPlayer(cx, cy, player.width, player.height)) return false;
            // 2) 地形（壁）との重なり
            if (CheckStageCollision(stageMap, cx, cy, player.width, player.height)) return false;

            // 3) 鉄球（4px マージン込み）
            for (const auto& chain : stageMap.hazardChains) {
                auto circles = GetChainLethalCircles(chain);
                for (auto lc : circles) {
                    lc.x -= (float)stageMap.scrollX;
                    lc.y -= (float)stageMap.scrollY;
                    LethalCircle lc2 = { lc.x, lc.y, lc.radius + 4.0f };
                    if (CheckLethalCollision(lc2, cx, cy, player.width, player.height)) return false;
                }
            }
            return true;
        };

        // 候補を順に試す
        for (float cx : candX) {
            if (isSafeAt(cx, ry)) { player.x = cx; player.y = ry; return; }
        }

        // 全候補が危険：鉄球の角度をずらして安全地帯を作る（一時退避）
        // 4px マージンでも全滅するほど密な状況は現状の配置では起きない想定だが、保険として実行。
        for (auto& chain : stageMap.hazardChains) {
            chain.baseAngle += (float)M_PI;      // 180度回して反対側へ
            if (chain.baseAngle > 2.0f * (float)M_PI) chain.baseAngle -= 2.0f * (float)M_PI;
        }
        // 退避後にもう一度だけ候補を試す
        for (float cx : candX) {
            if (isSafeAt(cx, ry)) { player.x = cx; player.y = ry; return; }
        }

        // それでもダメなら最後の手段：画面左端に置く（4px マージン無しの素の判定で再確認）
        player.x = 20.0f; player.y = ry;
    };

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                if (state == TITLE) {
                    if (e.key.keysym.sym == SDLK_z || e.key.keysym.sym == SDLK_RETURN) {
                        player.x = 20; player.y = 100;
                        aimDirX = 1.0f; aimDirY = 0.0f;
                        spawnEnemies(4);
                        bgmStep = 0;
                        state = PLAYING;
                    } else if (e.key.keysym.sym == SDLK_s) {
                        state = SOUND_TEST;
                        soundTest.selected = 1;
                        soundTest.playing = false;
                        soundTest.step = 0;
                        soundTest.frameInStep = 0;
                        bgmStep = 0;
                    }
                } else if (state == SOUND_TEST) {
                    switch (e.key.keysym.sym) {
                        case SDLK_UP:
                            soundTest.selected = (soundTest.selected + SoundTestState::TRACK_COUNT - 1) % SoundTestState::TRACK_COUNT;
                            break;
                        case SDLK_DOWN:
                            soundTest.selected = (soundTest.selected + 1) % SoundTestState::TRACK_COUNT;
                            break;
                        case SDLK_z:
                            if (soundTest.IsPlayable(soundTest.selected)) {
                                soundTest.playing     = true;
                                soundTest.step        = 0;
                                soundTest.frameInStep = 0;
                                bgmStep               = 0;
                            }
                            break;
                        case SDLK_x:
                            soundTest.playing = false;
                            g_apu.bgmPulse1Freq = 0.0f;
                            g_apu.bgmPulse2Freq = 0.0f;
                            g_apu.bgmTriangleFreq = 0.0f;
                            g_apu.bgmNoiseTrigger = false;
                            break;
                        case SDLK_ESCAPE:
                            state             = TITLE;
                            soundTest.playing = false;
                            g_apu.bgmPulse1Freq = 0.0f;
                            g_apu.bgmPulse2Freq = 0.0f;
                            g_apu.bgmTriangleFreq = 0.0f;
                            g_apu.bgmNoiseTrigger = false;
                            break;
                        default: break;
                    }
                } else if (state == PLAYING) {
                    if (e.key.keysym.sym == SDLK_1) currentWeapon = WEAPON_LASER;
                    if (e.key.keysym.sym == SDLK_2) currentWeapon = WEAPON_RING;
                    if (e.key.keysym.sym == SDLK_3) currentWeapon = WEAPON_MISSILE;
                    if (e.key.keysym.sym == SDLK_F1) debugInvincible = !debugInvincible;
                    if (e.key.keysym.sym == SDLK_F5) {
                        FreePlayerSprites(playerSprites);
                        playerSprites = LoadPlayerSprites(renderer);
                        FreeEffectSprites(effectSprites);
                        effectSprites = LoadEffectSprites(renderer);
                        orbitalTiles.Clear();
                        LoadOrbitalTiles(renderer, orbitalTiles);
                    }

                    if (e.key.keysym.sym == SDLK_z) {
                        PlaySE_Shot();
                        float speed = 4.5f;
                        float baseAngle = std::atan2(aimDirY, aimDirX);

                        if (currentWeapon == WEAPON_RING) {
                            float angles[4] = {
                                baseAngle,
                                baseAngle - 0.35f,
                                baseAngle + 0.35f,
                                baseAngle + (float)M_PI
                            };
                            for (int i = 0; i < 4; ++i) {
                                float vx = std::cos(angles[i]) * speed;
                                float vy = std::sin(angles[i]) * speed;
                                pBullets.push_back({ player.x + 6, player.y + 6, vx, vy, WEAPON_RING, true });
                            }
                        } else {
                            float vx = aimDirX * speed;
                            float vy = aimDirY * speed;
                            pBullets.push_back({ player.x + 6, player.y + 6, vx, vy, currentWeapon, true });
                        }
                    }
                } else if (state == PAUSED_DEATH || state == GAME_OVER) {
                    if (e.key.keysym.sym == SDLK_z || e.key.keysym.sym == SDLK_RETURN) {
                        player.x = 20; player.y = 100;
                        aimDirX = 1.0f; aimDirY = 0.0f;
                        pBullets.clear();

                        // 敵を完全リセット（spawnEnemies 内でも clear するが明示）
                        enemies.clear();
                        spawnEnemies(4);

                        // 直前のチェックポイントまでスクロールを巻き戻す（敵がいない地点へ）
                        if (!stageMap.checkpoints.empty()) {
                            stageMap.scrollX = stageMap.checkpoints[stageMap.lastCheckpointIndex];
                        }

                        // 安全な復帰座標を探索（鉄球・敵を避ける）
                        safeRespawn();

                        // BGM を曲頭から再開
                        bgmStep = 0;
                        g_apu.bgmPulse1Freq = 0.0f;
                        g_apu.bgmPulse2Freq = 0.0f;
                        g_apu.bgmTriangleFreq = 0.0f;
                        g_apu.bgmNoiseTrigger = false;

                        // ゲームオーバーからの復帰は完全リセット
                        if (state == GAME_OVER) {
                            score = 0;
                            lives = 5;
                        }
                        state = PLAYING;
                    }
                }
            }
        }

        const Uint8* keys = SDL_GetKeyboardState(NULL);
        frameCount++;

        if (state == PLAYING && (frameCount % 6 == 0)) {
            const NoteStep& step = STAGE1_SCORE[bgmStep];
            g_apu.bgmPulse1Freq = step.pulse1Freq;
            g_apu.bgmPulse2Freq = step.pulse2Freq;
            g_apu.bgmTriangleFreq = step.triFreq;
            g_apu.bgmNoiseTrigger = (step.drumType != 0);
            bgmStep = (bgmStep + 1) % 128;
        }

        // SOUND TEST 時の BGM 再生
        if (state == SOUND_TEST && soundTest.playing) {
            soundTest.frameInStep++;
            if (soundTest.frameInStep >= 6) { // STAGE1_FRAMES_PER_TICK 相当
                soundTest.frameInStep = 0;
                const NoteStep& s = STAGE1_SCORE[soundTest.step];
                g_apu.bgmPulse1Freq   = s.pulse1Freq;
                g_apu.bgmPulse2Freq   = s.pulse2Freq;
                g_apu.bgmTriangleFreq = s.triFreq;
                g_apu.bgmNoiseTrigger = (s.drumType != 0);
                soundTest.step = (soundTest.step + 1) % 128;
            }
        }

        if (state == PLAYING) {
            // 1. スクロール処理と回転障害物の更新
            AdvanceScroll(stageMap, LOGICAL_WIDTH, PLAY_AREA_HEIGHT);
            // 通過済みチェックポイントを更新（スクロール座標で判定）
            if (!stageMap.checkpoints.empty()) {
                for (int i = stageMap.lastCheckpointIndex; i < (int)stageMap.checkpoints.size(); ++i) {
                    if (stageMap.scrollX >= stageMap.checkpoints[i]) {
                        stageMap.lastCheckpointIndex = i;
                    } else break;
                }
            }
            for (auto& chain : stageMap.hazardChains) {
                UpdateRotatingChain(chain);
            }

            // 2. 地形スクロールによる自機の押し出し処理
            if (CheckStageCollision(stageMap, player.x, player.y, player.width, player.height)) {
                auto [pushX, pushY] = ScrollPushVector(stageMap.currentDirection);
                player.x -= pushX * 1.0f;
                player.y -= pushY * 1.0f;
            }

            // 3. 自機キー移動入力 ＆ CheckStageCollision による移動制限 (軸別分離)
            float moveX = 0.0f, moveY = 0.0f;
            if (keys[SDL_SCANCODE_UP]) moveY -= 1.0f;
            if (keys[SDL_SCANCODE_DOWN]) moveY += 1.0f;
            if (keys[SDL_SCANCODE_LEFT]) moveX -= 1.0f;
            if (keys[SDL_SCANCODE_RIGHT]) moveX += 1.0f;

            if (moveX != 0.0f || moveY != 0.0f) {
                float len = std::sqrt(moveX * moveX + moveY * moveY);
                aimDirX = moveX / len;
                aimDirY = moveY / len;

                float stepX = (moveX / len) * player.speed;
                float stepY = (moveY / len) * player.speed;

                if (!CheckStageCollision(stageMap, player.x + stepX, player.y, player.width, player.height)) {
                    player.x += stepX;
                }
                if (!CheckStageCollision(stageMap, player.x, player.y + stepY, player.width, player.height)) {
                    player.y += stepY;
                }
            }

            // 画面範囲クランプ
            if (player.x < 0) player.x = 0;
            if (player.x > LOGICAL_WIDTH - player.width) player.x = LOGICAL_WIDTH - player.width;
            if (player.y < 0) player.y = 0;
            if (player.y > PLAY_AREA_HEIGHT - player.height) player.y = PLAY_AREA_HEIGHT - player.height;

            // 4〜6. 被弾判定（一発接触で死亡・damaged フラグ1本に集約して1フレーム多段ヒットを防ぐ）
            if (!debugInvincible) {
                bool damaged = false;

                // 4. スクロール挟み込み即死判定
                if (IsPlayerCrushed(stageMap, player.x, player.y, player.width, player.height, LOGICAL_WIDTH, PLAY_AREA_HEIGHT, static_cast<int>(stageMap.currentDirection))) {
                    damaged = true;
                }

                // 5. 回転即死障害物（マップ座標 → 画面座標に変換して判定）
                if (!damaged) {
                    for (const auto& chain : stageMap.hazardChains) {
                        for (auto lc : GetChainLethalCircles(chain)) {
                            lc.x -= (float)stageMap.scrollX;
                            lc.y -= (float)stageMap.scrollY;
                            if (CheckLethalCollision(lc, player.x, player.y, player.width, player.height)) {
                                damaged = true;
                                break;
                            }
                        }
                        if (damaged) break;
                    }
                }

                // 6. 自機 vs 敵（どちらも画面座標）
                if (!damaged) {
                    for (auto& en : enemies) {
                        if (!en.active) continue;
                        if (player.x < en.x + en.width && player.x + player.width > en.x &&
                            player.y < en.y + en.height && player.y + player.height > en.y) {
                            en.active = false;
                            damaged = true;
                            break;
                        }
                    }
                }

                if (!damaged && orbitalEnemies.HitsPlayer(
                        player.x, player.y, player.width, player.height)) {
                    damaged = true;
                }

                // 被弾確定 → ライフ減算・無敵付与・リスポーン
                if (damaged) {
                    lives--;
                    PlaySE_Explosion();
                    pBullets.clear();
                    // BGM 完全停止（爆発SEのみ残す）
                    g_apu.bgmPulse1Freq = 0.0f;
                    g_apu.bgmPulse2Freq = 0.0f;
                    g_apu.bgmTriangleFreq = 0.0f;
                    g_apu.bgmNoiseTrigger = false;
                    if (lives <= 0) {
                        state = GAME_OVER;
                    } else {
                        state = PAUSED_DEATH;
                    }
                }
            }

            // 敵の移動
            for (auto& en : enemies) {
                if (!en.active) continue;
                en.x -= en.speed;
                if (en.x < -16) en.x = LOGICAL_WIDTH;
            }
            orbitalEnemies.Update(1.0f / 60.0f, player.x + player.width * 0.5f, player.y + player.height * 0.5f,
                                  LOGICAL_WIDTH, PLAY_AREA_HEIGHT);

            // 爆発エフェクトのコマ送り (6 ゲームフレームで 1 コマ、4 コマで消える)
            for (auto& fx : effects) {
                if (--fx.timer <= 0) {
                    fx.timer = 6;
                    ++fx.frame;
                }
            }
            effects.erase(
                std::remove_if(effects.begin(), effects.end(),
                               [](const EffectInstance& e){ return e.frame >= 4; }),
                effects.end());

            // 自機弾の移動および地形衝突 (リング(WEAPON_RING)以外は障害物で消滅)
            for (auto& b : pBullets) {
                if (!b.active) continue;
                b.x += b.vx;
                b.y += b.vy;

                if (b.x < 0 || b.x > LOGICAL_WIDTH || b.y < 0 || b.y > PLAY_AREA_HEIGHT) {
                    b.active = false;
                    continue;
                }

                if (b.type != WEAPON_RING && CheckStageCollision(stageMap, b.x, b.y, 4.0f, 4.0f)) {
                    b.active = false;
                    continue;
                }

                // Orbital Enemy: Shield ×3 → Core の順で判定 (Shield に当たった弾は消えるだけ)
                int orbitalScore = 0;
                float coreX = 0.0f, coreY = 0.0f;
                const OrbitalBulletResult orbitalHit =
                    orbitalEnemies.ResolvePlayerBullet(
                        b.x, b.y, 4.0f, 4.0f, 1, orbitalScore, &coreX, &coreY);
                if (orbitalHit != OrbitalBulletResult::MISS) {
                    b.active = false;
                    if (orbitalHit == OrbitalBulletResult::CORE_DESTROYED) {
                        score += orbitalScore;
                        if (score > highScore) highScore = score;
                        PlaySE_Explosion();
                        {
                            const float ex = coreX - effectSprites.cellWidth;
                            const float ey = coreY - effectSprites.cellHeight;
                            effects.push_back({ ex, ey, 0, 6 });
                        }
                    }
                    continue;
                }

                for (auto& en : enemies) {
                    if (!en.active) continue;
                    if (b.x < en.x + en.width && b.x + 4 > en.x &&
                        b.y < en.y + en.height && b.y + 4 > en.y) {
                        b.active = false;
                        // spawn a 4-frame explosion centered on this enemy
                        {
                            const float ex = en.x + en.width  * 0.5f - effectSprites.cellWidth;   // cellWidth*scale/2 with scale=2
                            const float ey = en.y + en.height * 0.5f - effectSprites.cellHeight;
                            effects.push_back({ ex, ey, 0, 6 });
                        }
                        en.active = false;
                        score += 100;
                        if (score > highScore) highScore = score;
                        PlaySE_Explosion();
                    }
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        if (state == TITLE) {
            DrawText(renderer, "BURAI VIBE APU", 65, 50, 3, {0, 255, 255, 255});
            DrawText(renderer, "STAGE 1 INTEGRATION", 55, 90, 1, {255, 255, 255, 255});
            DrawText(renderer, "PRESS S FOR SOUND TEST", 55, 160, 1, {180, 180, 180, 255});
            if ((frameCount / 30) % 2 == 0) {
                DrawText(renderer, "PRESS Z TO START", 80, 140, 1, {255, 255, 0, 255});
            }
        } else if (state == SOUND_TEST) {
            DrawText(renderer, "SOUND TESTMODE", 75, 15, 2, {0, 255, 255, 255});

            for (int i = 0; i < SoundTestState::TRACK_COUNT; ++i) {
                const int y = 45 + i * 14;
                const bool sel = (i == soundTest.selected);
                const bool playable = soundTest.IsPlayable(i);

                if (sel) {
                    SDL_SetRenderDrawColor(renderer, 30, 40, 90, 255);
                    SDL_Rect bar = { 40, y - 1, 240, 11 };
                    SDL_RenderFillRect(renderer, &bar);
                }

                std::string line = (sel ? ">" : " ");
                line += (i < 9 ? "0" : "") + std::to_string(i + 1) + " ";
                line += SoundTestState::TrackName(i);

                if (playable) {
                    if (soundTest.playing && sel) line += " [PLAY]";
                } else {
                    line += " [---]";
                }

                SDL_Color col = sel ? SDL_Color{255, 255, 0, 255} : (playable ? SDL_Color{255, 255, 255, 255} : SDL_Color{100, 100, 100, 255});
                DrawText(renderer, line, 50, y, 1, col);
            }

            DrawText(renderer, "UP/DN:SEL Z:PLAY X:STOP ESC:EXIT", 15, 205, 1, {180, 180, 180, 255});
        } else if (state == PLAYING) {
            RenderStage(renderer, stageMap, stageTextures, LOGICAL_WIDTH, PLAY_AREA_HEIGHT, 1);
            RenderHazardChains(renderer, stageMap);

            const Direction8 playerDir = Direction8FromVector(aimDirX, aimDirY, Direction8::RIGHT);
            if (!DrawPlayerSprite(renderer, playerSprites, playerDir, player.x, player.y, player.width, player.height)) {
                // その向きの画像が無い・拒否された場合のプレースホルダー (開発用)
                SDL_Rect pRect = { (int)player.x, (int)player.y, (int)player.width, (int)player.height };
                SDL_SetRenderDrawColor(renderer, 0, 220, 255, 255);
                SDL_RenderFillRect(renderer, &pRect);
            }

            SDL_Rect aimPointer = { (int)(player.x + 6 + aimDirX * 10), (int)(player.y + 6 + aimDirY * 10), 4, 4 };
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderFillRect(renderer, &aimPointer);

            for (const auto& b : pBullets) {
                if (b.active) {
                    SDL_Rect bRect = { (int)b.x, (int)b.y, 4, 4 };
                    if (b.type == WEAPON_RING) SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
                    else SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                    SDL_RenderFillRect(renderer, &bRect);
                }
            }

            SDL_SetRenderDrawColor(renderer, 255, 50, 50, 255);
            for (const auto& en : enemies) {
                if (en.active) {
                    SDL_Rect eRect = { (int)en.x, (int)en.y, (int)en.width, (int)en.height };
                    SDL_RenderFillRect(renderer, &eRect);
                }
            }
            orbitalEnemies.Render(renderer, frameCount, orbitalTiles);
            for (const auto& fx : effects) {
                DrawEffectFrame(renderer, effectSprites, fx.frame, fx.x, fx.y, 2);
            }

            DrawText(renderer, "ARROWS: MOVE & AIM  Z: SHOOT", 10, 10, 1, {255, 255, 255, 255});
            DrawText(renderer, "1: LASER  2: RING  3: MISSILE", 10, 22, 1, {220, 220, 220, 255});
            {
                std::ostringstream dbg;
                dbg << "P:" << (int)player.x << "," << (int)player.y
                    << " S:" << stageMap.scrollX << " CP:" << stageMap.lastCheckpointIndex;
                DrawText(renderer, dbg.str(), 10, 34, 1, {255, 255, 0, 255});
            }
        } else if (state == PAUSED_DEATH) {
            DrawText(renderer, "PRESS Z TO CONTINUE", 45, 90, 1, {255, 255, 0, 255});
        } else if (state == GAME_OVER) {
            DrawText(renderer, "GAME OVER", 85, 60, 2, {255, 0, 0, 255});
            if ((frameCount / 30) % 2 == 0) {
                DrawText(renderer, "PRESS Z TO RESTART", 55, 120, 1, {255, 255, 255, 255});
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 128, 128, 255);
        SDL_RenderDrawLine(renderer, 0, 205, LOGICAL_WIDTH, 205);

        DrawText(renderer, FormatScore(score), 35, 210, 1, {255, 255, 255, 255});
        DrawText(renderer, "TOP", 110, 210, 1, {255, 0, 255, 255});
        DrawText(renderer, FormatScore(highScore), 175, 210, 1, {255, 255, 255, 255});

        SDL_Rect iconRect = { 268, 209, 8, 7 };
        SDL_SetRenderDrawColor(renderer, 0, 220, 255, 255);
        SDL_RenderFillRect(renderer, &iconRect);
        DrawText(renderer, std::to_string(lives), 285, 210, 1, {255, 255, 255, 255});
        if (debugInvincible) DrawText(renderer, "DBG INV", 2, 2, 1, {255, 255, 0, 255});

        for (int i = 0; i < 6; ++i) {
            SDL_Rect barBox = { 10 + i * 8, 224, 6, 6 };
            if (i < cobaltMeter) {
                SDL_SetRenderDrawColor(renderer, 255, 100, 255, 255);
                SDL_RenderFillRect(renderer, &barBox);
            } else {
                SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
                SDL_RenderDrawRect(renderer, &barBox);
            }
        }

        DrawText(renderer, "EXTRA", 62, 224, 1, {255, 255, 255, 255});
        DrawText(renderer, "LASER", 100, 224, 1, {255, 100, 255, 255});
        DrawText(renderer, std::string(1, FormatHexLevel(laserLevel)), 132, 224, 1, {255, 255, 255, 255});
        DrawText(renderer, "RING", 155, 224, 1, {255, 100, 255, 255});
        DrawText(renderer, std::string(1, FormatHexLevel(ringLevel)), 180, 224, 1, {255, 255, 255, 255});
        DrawText(renderer, "MISSILE", 200, 224, 1, {255, 100, 255, 255});
        DrawText(renderer, std::string(1, FormatHexLevel(missileLevel)), 245, 224, 1, {255, 255, 255, 255});

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    FreePlayerSprites(playerSprites);
    FreeEffectSprites(effectSprites);
    orbitalTiles.Clear();
    DestroyStageTextures(stageTextures);
    SDL_CloseAudio();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

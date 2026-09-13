#pragma once

#include <cstdint>
#include <utility>

// 8方向。UP を 0 として画面上で時計回りに並ぶ
enum class Direction8 : uint8_t {
    UP = 0, UP_RIGHT, RIGHT, DOWN_RIGHT, DOWN, DOWN_LEFT, LEFT, UP_LEFT
};

struct PlayerState {
    float x, y;
    Direction8 moveDir;
    Direction8 aimDir; // moveDirとは独立して管理する攻撃方向
};

// Direction8 の並び順に対応する単位ベクトル (画面座標: +y が下)
const std::pair<float, float> DIR_VECTOR[8] = {
    {  0.0f, -1.0f }, {  0.707f, -0.707f }, {  1.0f, 0.0f }, {  0.707f, 0.707f },
    {  0.0f,  1.0f }, { -0.707f,  0.707f }, { -1.0f, 0.0f }, { -0.707f, -0.707f },
};

// -------------------------------------------------------------
// Direction8 と sprite-ai MCP の sprite_rotate の向きインデックス対応表
// -------------------------------------------------------------
// sprite_rotate が返す並び: 0:S, 1:SE, 2:E, 3:NE, 4:N, 5:NW, 6:W, 7:SW
//   (S=画面下向き から始まり、画面上で反時計回り)
// Direction8 の並び:       0:UP(N), 1:UP_RIGHT(NE), 2:RIGHT(E), 3:DOWN_RIGHT(SE),
//                          4:DOWN(S), 5:DOWN_LEFT(SW), 6:LEFT(W), 7:UP_LEFT(NW)
//   (N=画面上向き から始まり、画面上で時計回り)
//
// 開始点が 180 度ずれ、回る向きも逆なので、単純なオフセット加算では合わない。
// 対応は spriteIndex = (4 - dir + 8) % 8 (この式は逆方向の変換にもそのまま使える)。
//
//   Direction8      | 方位 | sprite_rotate index
//   ----------------+------+--------------------
//   UP         (0)  |  N   | 4
//   UP_RIGHT   (1)  |  NE  | 3
//   RIGHT      (2)  |  E   | 2
//   DOWN_RIGHT (3)  |  SE  | 1
//   DOWN       (4)  |  S   | 0
//   DOWN_LEFT  (5)  |  SW  | 7
//   LEFT       (6)  |  W   | 6
//   UP_LEFT    (7)  |  NW  | 5
//
// 自機スプライトの向き表示は aimDir をこの表で変換したインデックスのコマを使うこと。
// (コマの切り出し寸法・グリッド数はシート側の設定値から読み、ここでは固定しない)
const uint8_t DIR8_TO_SPRITE_ROTATE_INDEX[8] = { 4, 3, 2, 1, 0, 7, 6, 5 };

inline uint8_t SpriteRotateIndex(Direction8 dir) {
    return DIR8_TO_SPRITE_ROTATE_INDEX[static_cast<uint8_t>(dir)];
}

inline Direction8 RotateClockwise(Direction8 dir) {
    return static_cast<Direction8>((static_cast<uint8_t>(dir) + 1) % 8);
}

// 入力 (-1/0/+1) から 8方向を求める。無入力なら false
inline bool DirectionFromInput(int dx, int dy, Direction8& out) {
    static const Direction8 table[3][3] = {
        // dx = -1               dx = 0           dx = +1
        { Direction8::UP_LEFT,   Direction8::UP,   Direction8::UP_RIGHT },   // dy = -1
        { Direction8::LEFT,      Direction8::UP,   Direction8::RIGHT },      // dy = 0 (中央は未使用)
        { Direction8::DOWN_LEFT, Direction8::DOWN, Direction8::DOWN_RIGHT }, // dy = +1
    };
    if (dx == 0 && dy == 0) return false;
    out = table[dy + 1][dx + 1];
    return true;
}

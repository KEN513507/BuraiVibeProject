#include "../src/player_sprite_contract.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace {
int failures = 0;
void Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

const char* kValid = R"({
  "contract_version": 1,
  "cell_width": 32,
  "cell_height": 32,
  "max_opaque_colors_per_sprite": 3,
  "palette": [],
  "directions": {
    "right": "player_right.png", "up_right": "player_up_right.png", "up": "player_up.png",
    "up_left": "player_up_left.png", "left": "player_left.png", "down_left": "player_down_left.png",
    "down": "player_down.png", "down_right": "player_down_right.png"
  },
  "reference_only": ["sprite_output/sheet_8dir_47b88223.png"]
})";

std::string Replace(std::string s, const std::string& from, const std::string& to) {
    s.replace(s.find(from), from.size(), to);
    return s;
}

bool Parses(const std::string& json) {
    PlayerSpriteContract c;
    std::string error;
    return ParsePlayerSpriteContract(json, c, error);
}
}

int main(int argc, char** argv) {
    PlayerSpriteContract c;
    std::string error;

    // TEST1 正しい契約を読み、向きが Direction8 の並びで引ける
    Check(ParsePlayerSpriteContract(kValid, c, error), "TEST1 valid contract parses");
    Check(c.cellWidth == 32 && c.cellHeight == 32, "TEST1 cell size 32x32");
    Check(c.maxOpaqueColorsPerSprite == 3, "TEST1 max colors 3");
    Check(c.files[static_cast<int>(Direction8::UP)] == "player_up.png", "TEST1 UP file");
    Check(c.files[static_cast<int>(Direction8::DOWN_LEFT)] == "player_down_left.png", "TEST1 DOWN_LEFT file");
    Check(c.files[static_cast<int>(Direction8::UP_LEFT)] == "player_up_left.png", "TEST1 UP_LEFT file");

    // TEST2 16x16 も許可、それ以外のサイズ・非正方形は拒否 (縮小前提のサイズを通さない)
    Check(Parses(Replace(Replace(kValid, "\"cell_width\": 32", "\"cell_width\": 16"), "\"cell_height\": 32", "\"cell_height\": 16")),
          "TEST2 16x16 accepted");
    Check(!Parses(Replace(kValid, "\"cell_width\": 32", "\"cell_width\": 16")), "TEST2 16x32 rejected");
    Check(!Parses(Replace(Replace(kValid, "\"cell_width\": 32", "\"cell_width\": 64"), "\"cell_height\": 32", "\"cell_height\": 64")),
          "TEST2 64x64 rejected");

    // TEST3 色数上限は 1..4
    Check(!Parses(Replace(kValid, "\"max_opaque_colors_per_sprite\": 3", "\"max_opaque_colors_per_sprite\": 5")), "TEST3 5 colors rejected");
    Check(!Parses(Replace(kValid, "\"max_opaque_colors_per_sprite\": 3", "\"max_opaque_colors_per_sprite\": 0")), "TEST3 0 colors rejected");

    // TEST4 向きの欠落・パス付きファイル名・重複ファイルは拒否
    Check(!Parses(Replace(kValid, "\"down\": \"player_down.png\",", "")), "TEST4 missing direction rejected");
    Check(!Parses(Replace(kValid, "\"player_up.png\"", "\"../player_up.png\"")), "TEST4 path traversal rejected");
    Check(!Parses(Replace(kValid, "\"player_up.png\"", "\"player_right.png\"")), "TEST4 duplicate file rejected");
    Check(!Parses(Replace(kValid, "\"player_up.png\"", "\"player_up.bmp\"")), "TEST4 non-png rejected");

    // TEST5 壊れた JSON・未知の contract_version は拒否し、out を書き換えない
    PlayerSpriteContract untouched;
    untouched.cellWidth = 123;
    Check(!ParsePlayerSpriteContract("{ \"contract_version\": 1, ", untouched, error), "TEST5 truncated JSON rejected");
    Check(untouched.cellWidth == 123, "TEST5 out untouched on failure");
    Check(!Parses(Replace(kValid, "\"contract_version\": 1", "\"contract_version\": 2")), "TEST5 version 2 rejected");
    Check(!Parses(Replace(kValid, "\"cell_width\": 32", "\"cell_width\": 32.5")), "TEST5 non-integer rejected");

    // TEST6 照準ベクトル → 8方向 (画面座標 +y が下)
    Check(Direction8FromVector(0, -1, Direction8::RIGHT) == Direction8::UP, "TEST6 up");
    Check(Direction8FromVector(1, 0, Direction8::UP) == Direction8::RIGHT, "TEST6 right");
    Check(Direction8FromVector(0, 1, Direction8::UP) == Direction8::DOWN, "TEST6 down");
    Check(Direction8FromVector(-1, 0, Direction8::UP) == Direction8::LEFT, "TEST6 left");
    Check(Direction8FromVector(0.707f, -0.707f, Direction8::UP) == Direction8::UP_RIGHT, "TEST6 up_right");
    Check(Direction8FromVector(-0.707f, 0.707f, Direction8::UP) == Direction8::DOWN_LEFT, "TEST6 down_left");
    Check(Direction8FromVector(-0.707f, -0.707f, Direction8::UP) == Direction8::UP_LEFT, "TEST6 up_left");
    Check(Direction8FromVector(0, 0, Direction8::DOWN_RIGHT) == Direction8::DOWN_RIGHT, "TEST6 zero -> fallback");

    // TEST7 実ファイル assets/sprites/player/player.json が読める (引数でパス指定)
    if (argc > 1) {
        std::ifstream f(argv[1], std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        PlayerSpriteContract real;
        const bool ok = ParsePlayerSpriteContract(ss.str(), real, error);
        if (!ok) std::cerr << "  real contract error: " << error << '\n';
        Check(ok, "TEST7 real player.json parses");
    }

    if (failures == 0) std::cout << "ALL PLAYER SPRITE CONTRACT TESTS PASSED\n";
    return failures == 0 ? 0 : 1;
}

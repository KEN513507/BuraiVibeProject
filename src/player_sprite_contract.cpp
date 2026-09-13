#include "player_sprite_contract.h"

#include <cmath>
#include <cstdlib>
#include <map>
#include <memory>

const std::array<const char*, 8> PLAYER_DIRECTION_KEYS = {
    "up", "up_right", "right", "down_right", "down", "down_left", "left", "up_left",
};

namespace {

// player.json 用の最小 JSON リーダ。オブジェクト・配列・文字列・数値・true/false/null を読む
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    double number = 0.0;
    std::string text;
    std::map<std::string, std::unique_ptr<JsonValue>> members;
};

class JsonReader {
public:
    explicit JsonReader(const std::string& src) : s_(src) {}

    bool ReadDocument(JsonValue& out) {
        if (!ReadValue(out, 0)) return false;
        SkipSpace();
        return pos_ == s_.size() || Fail("trailing characters");
    }

    const std::string& Error() const { return error_; }

private:
    const std::string& s_;
    size_t pos_ = 0;
    std::string error_;

    bool Fail(const std::string& message) {
        if (error_.empty()) error_ = message + " at offset " + std::to_string(pos_);
        return false;
    }

    void SkipSpace() {
        while (pos_ < s_.size() && (s_[pos_] == ' ' || s_[pos_] == '\t' || s_[pos_] == '\n' || s_[pos_] == '\r')) ++pos_;
    }

    bool Consume(char c) {
        SkipSpace();
        if (pos_ < s_.size() && s_[pos_] == c) { ++pos_; return true; }
        return false;
    }

    bool ReadLiteral(const char* word) {
        const std::string w(word);
        if (s_.compare(pos_, w.size(), w) != 0) return Fail("invalid literal");
        pos_ += w.size();
        return true;
    }

    bool ReadString(std::string& out) {
        if (!Consume('"')) return Fail("expected string");
        out.clear();
        while (pos_ < s_.size()) {
            char c = s_[pos_++];
            if (c == '"') return true;
            if (c != '\\') { out += c; continue; }
            if (pos_ >= s_.size()) break;
            char esc = s_[pos_++];
            switch (esc) {
                case '"': case '\\': case '/': out += esc; break;
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u':
                    // player.json のファイル名は ASCII 前提。\u は読み飛ばして '?' にする
                    if (pos_ + 4 > s_.size()) return Fail("bad \\u escape");
                    pos_ += 4;
                    out += '?';
                    break;
                default: return Fail("bad escape");
            }
        }
        return Fail("unterminated string");
    }

    bool ReadValue(JsonValue& out, int depth) {
        if (depth > 32) return Fail("nesting too deep");
        SkipSpace();
        if (pos_ >= s_.size()) return Fail("unexpected end");
        const char c = s_[pos_];
        if (c == '{') return ReadObject(out, depth);
        if (c == '[') return ReadArray(out, depth);
        if (c == '"') { out.type = JsonValue::Type::String; return ReadString(out.text); }
        if (c == 't') { out.type = JsonValue::Type::Bool; out.number = 1; return ReadLiteral("true"); }
        if (c == 'f') { out.type = JsonValue::Type::Bool; out.number = 0; return ReadLiteral("false"); }
        if (c == 'n') { out.type = JsonValue::Type::Null; return ReadLiteral("null"); }
        const char* begin = s_.c_str() + pos_;
        char* end = nullptr;
        out.number = std::strtod(begin, &end);
        if (end == begin) return Fail("unexpected character");
        out.type = JsonValue::Type::Number;
        pos_ += static_cast<size_t>(end - begin);
        return true;
    }

    bool ReadObject(JsonValue& out, int depth) {
        out.type = JsonValue::Type::Object;
        ++pos_;  // '{'
        if (Consume('}')) return true;
        do {
            std::string key;
            if (!ReadString(key)) return false;
            if (!Consume(':')) return Fail("expected ':'");
            auto value = std::make_unique<JsonValue>();
            if (!ReadValue(*value, depth + 1)) return false;
            out.members[key] = std::move(value);
        } while (Consume(','));
        return Consume('}') || Fail("expected '}'");
    }

    bool ReadArray(JsonValue& out, int depth) {
        out.type = JsonValue::Type::Array;  // 要素はランタイムで使わないので読み捨てる
        ++pos_;  // '['
        if (Consume(']')) return true;
        do {
            JsonValue element;
            if (!ReadValue(element, depth + 1)) return false;
        } while (Consume(','));
        return Consume(']') || Fail("expected ']'");
    }
};

const JsonValue* Member(const JsonValue& obj, const char* key) {
    const auto it = obj.members.find(key);
    return it == obj.members.end() ? nullptr : it->second.get();
}

bool ReadInt(const JsonValue& obj, const char* key, int& out, std::string& error) {
    const JsonValue* v = Member(obj, key);
    if (!v || v->type != JsonValue::Type::Number || v->number != std::floor(v->number)) {
        error = std::string("\"") + key + "\" must be an integer";
        return false;
    }
    out = static_cast<int>(v->number);
    return true;
}

bool IsBarePngName(const std::string& name) {
    if (name.size() <= 4 || name.compare(name.size() - 4, 4, ".png") != 0) return false;
    return name.find('/') == std::string::npos && name.find('\\') == std::string::npos && name.find("..") == std::string::npos;
}

} // namespace

bool ParsePlayerSpriteContract(const std::string& json, PlayerSpriteContract& out, std::string& error) {
    JsonValue root;
    JsonReader reader(json);
    if (!reader.ReadDocument(root)) { error = "invalid JSON: " + reader.Error(); return false; }
    if (root.type != JsonValue::Type::Object) { error = "root must be an object"; return false; }

    PlayerSpriteContract parsed;
    if (!ReadInt(root, "contract_version", parsed.contractVersion, error)) return false;
    if (parsed.contractVersion != 1) {
        error = "unsupported contract_version " + std::to_string(parsed.contractVersion);
        return false;
    }
    if (!ReadInt(root, "cell_width", parsed.cellWidth, error) ||
        !ReadInt(root, "cell_height", parsed.cellHeight, error) ||
        !ReadInt(root, "max_opaque_colors_per_sprite", parsed.maxOpaqueColorsPerSprite, error)) return false;

    // CLAUDE.md: スプライトは 32x32 か 16x16 の正方形のみ。色数は NES の 1 スプライト 4 色まで
    if (parsed.cellWidth != parsed.cellHeight || (parsed.cellWidth != 16 && parsed.cellWidth != 32)) {
        error = "cell size " + std::to_string(parsed.cellWidth) + "x" + std::to_string(parsed.cellHeight) +
                " must be 32x32 or 16x16";
        return false;
    }
    if (parsed.maxOpaqueColorsPerSprite < 1 || parsed.maxOpaqueColorsPerSprite > 4) {
        error = "max_opaque_colors_per_sprite must be 1..4";
        return false;
    }

    const JsonValue* directions = Member(root, "directions");
    if (!directions || directions->type != JsonValue::Type::Object) {
        error = "\"directions\" must be an object";
        return false;
    }
    for (size_t i = 0; i < PLAYER_DIRECTION_KEYS.size(); ++i) {
        const JsonValue* file = Member(*directions, PLAYER_DIRECTION_KEYS[i]);
        if (!file || file->type != JsonValue::Type::String || !IsBarePngName(file->text)) {
            error = std::string("directions.") + PLAYER_DIRECTION_KEYS[i] + " must be a bare .png file name";
            return false;
        }
        parsed.files[i] = file->text;
    }
    for (size_t i = 0; i < parsed.files.size(); ++i) {
        for (size_t j = i + 1; j < parsed.files.size(); ++j) {
            if (parsed.files[i] == parsed.files[j]) {
                error = "each direction must use its own file (" + parsed.files[i] + ")";
                return false;
            }
        }
    }

    out = parsed;
    return true;
}

Direction8 Direction8FromVector(float x, float y, Direction8 fallback) {
    if (x == 0.0f && y == 0.0f) return fallback;
    // UP を 0 とした時計回りの角度 (画面座標なので +y が下)
    const double angle = std::atan2(static_cast<double>(x), static_cast<double>(-y));
    constexpr double kQuarterPi = 0.78539816339744830962;
    int sector = static_cast<int>(std::lround(angle / kQuarterPi));
    sector = ((sector % 8) + 8) % 8;
    return static_cast<Direction8>(sector);
}

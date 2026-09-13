#pragma once
#include <cstdint>
#include <vector>
#include <algorithm>
#include "render_unit.h"

namespace boss {

struct OAMEntry {
    float x = 0.0f;
    float y = 0.0f;
    uint16_t tile_id = 0;
    uint8_t palette_id = 0;
    bool flip_h = false;
    bool flip_v = false;
    uint8_t priority = 0;

    Rgb placeholder_color{200, 200, 200};
    int placeholder_w = 8;
    int placeholder_h = 8;
};

class OAM {
public:
    static constexpr size_t MAX_SPRITES = 64;
    static constexpr int SCANLINE_LIMIT = 8;

    std::vector<OAMEntry> entries;

    void clear() { entries.clear(); }

    void submit(const OAMEntry& entry) {
        if (entries.size() < MAX_SPRITES) {
            entries.push_back(entry);
        }
    }

    void resolve_flicker(uint32_t frame_count) {
        if (!entries.empty()) {
            size_t shift = frame_count % entries.size();
            std::rotate(entries.begin(), entries.begin() + shift, entries.end());
        }
    }
};

} // namespace boss

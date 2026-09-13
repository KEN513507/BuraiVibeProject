#pragma once
#include "part.h"
#include "render_unit.h"
#include "small_vector.h"

namespace boss {

class CompositePart : public Part {
public:
    SmallVector<RenderUnit, 4> units;

    void update(float dt) override {}

    void emit_to(OAM& oam) override {
        if (!active || destroyed) return;
        for (auto& u : units) {
            if (!u.visible) continue;
            OAMEntry e;
            e.x = world_x + u.rel_x;
            e.y = world_y + u.rel_y;
            e.tile_id = u.tile_id;
            e.palette_id = u.palette_id;
            e.flip_h = u.flip_h;
            e.flip_v = u.flip_v;
            e.placeholder_color = u.placeholder_color;
            e.placeholder_w = u.placeholder_w;
            e.placeholder_h = u.placeholder_h;
            oam.submit(e);
        }
    }
};

} // namespace boss

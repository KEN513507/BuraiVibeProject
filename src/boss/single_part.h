#pragma once
#include "part.h"
#include "render_unit.h"

namespace boss {

class SinglePart : public Part {
public:
    RenderUnit unit;

    void update(float dt) override {}

    void emit_to(OAM& oam) override {
        if (!active || destroyed || !unit.visible) return;
        OAMEntry e;
        e.x = world_x + unit.rel_x;
        e.y = world_y + unit.rel_y;
        e.tile_id = unit.tile_id;
        e.palette_id = unit.palette_id;
        e.flip_h = unit.flip_h;
        e.flip_v = unit.flip_v;
        e.placeholder_color = unit.placeholder_color;
        e.placeholder_w = unit.placeholder_w;
        e.placeholder_h = unit.placeholder_h;
        oam.submit(e);
    }
};

} // namespace boss

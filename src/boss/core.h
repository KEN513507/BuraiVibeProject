#pragma once
#include "single_part.h"

namespace boss {

class Core : public SinglePart {
public:
    Core() {
        hp = 120;
        max_hp = 120;
    }
};

}

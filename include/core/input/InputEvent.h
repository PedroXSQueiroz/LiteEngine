#pragma once
#include <unordered_map>
#include <glm/glm.hpp>
#include <core/input/InputEnums.h>

namespace lite {

struct InputEvent {
    std::unordered_map<INPUT_KEYS, INPUT_KEY_STATES> keys;
    std::unordered_map<INPUT_ANALOGS, glm::vec2> analogs;
};

} // namespace lite

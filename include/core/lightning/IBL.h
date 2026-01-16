#pragma once

#include <string>

namespace lite {

class IBL {
public:
    virtual ~IBL() = default;

    // Load IBL from path (file or directory)
    virtual bool load(const std::string& path) = 0;
};

} // namespace lite

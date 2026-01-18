#pragma once

#include <string>
#include <vector>

#include <core/data/assets/Asset3dData.h>
#include <core/data/assets/MaterialData.h>

namespace lite {

// Abstract interface for 3D asset importers
// Implementations: AssimpImporter, etc.
class Asset3dImporter {
public:
    virtual ~Asset3dImporter() = default;

    // Import a 3D asset file
    // Populates rootNode tree and materials vector by reference
    // Returns true on success
    virtual bool import(
        const std::string& filePath,
        Asset3dData& rootNode,
        std::vector<MaterialData>& materials
    ) = 0;

    // Check if this mporter can handle the given file extension
    virtual bool canImport(const std::string& extension) const = 0;

    // Get supported file extensions
    virtual std::vector<std::string> getSupportedExtensions() const = 0;
};

} // namespace lite

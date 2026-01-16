#pragma once

#include <string>
#include <memory>
#include <vector>
#include <core/data/assets/Asset3dData.h>

namespace lite {

    // Abstract interface for 3D asset importers
    // Implementations: AssimpImporter, etc.
    class Asset3dImporter {
    public:
        virtual ~Asset3dImporter() = default;

        // Import a 3D asset file and return renderer-agnostic data
        virtual std::unique_ptr<Asset3dData> import(const std::string& filePath) = 0;

        // Check if this importer can handle the given file extension
        virtual bool canImport(const std::string& extension) const = 0;

        // Get supported file extensions
        virtual std::vector<std::string> getSupportedExtensions() const = 0;
    };

} // namespace lite

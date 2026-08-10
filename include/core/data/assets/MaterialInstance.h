#pragma once

#include <string>

namespace lite {

// Material materializado no backend — o par de MaterialData do lado instância,
// como MeshAsset3dInstance é o par de MeshAsset3dData.
//
// Ao contrário de Asset3dInstance/MeshAsset3dInstance, NÃO é
// template<TransformConcept>: material não é nó de cena e não tem transform.
//
// Os parâmetros de cada modelo de shading ficam nas filhas (hoje só
// MPBRLitMaterialInstance), espelhando a divisão do lado Data.
class MaterialInstance {
public:
    virtual ~MaterialInstance() = default;

    const std::string& getName() const { return m_name; }

protected:
    std::string m_name;
};

} // namespace lite

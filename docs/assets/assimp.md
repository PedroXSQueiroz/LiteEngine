# LiteEngine — Módulo de Assets: Assimp

> Parte da [documentação de arquitetura](../ARCHITECTURE.md). Interfaces do core: [core.md §4.6](../core.md).

Implementação concreta de importação de modelos 3D usando **Assimp** (a lib vem do vendoring do Filament: `3rd_party/filament/third_party/libassimp`). Código em `assimp/assets/importer/AssimpImporter.cpp` + `include/assimp/assets/importer/AssimpImporter.h`. Namespace `lite`.

Este módulo trabalha **exclusivamente com tipos CPU do core** (`Asset3dData`, `MeshAsset3dData`, `MaterialData`, `TextureInfo` + GLM) — não conhece Filament nem GPU. É a metade "de entrada" do pipeline de assets; a metade GPU é o [`FilamentInstanceFactory`](../rendering/filament.md).

## 1. Mapa de implementação — interface do core → classe Assimp

| Interface do core | Implementação | Arquivos |
|---|---|---|
| `lite::Asset3dImporter` | `lite::AssimpImporter` | `include/assimp/assets/importer/AssimpImporter.h` + `assimp/assets/importer/AssimpImporter.cpp` |

Contrato implementado (os 3 métodos puros de `Asset3dImporter`):

| Método do core | Implementação aqui |
|---|---|
| `import(filePath, rootNode&, materials&) → bool` | carrega via `Assimp::Importer::ReadFile` e popula as referências (§2) |
| `canImport(extension) → bool` | normaliza (lowercase, remove `.`) e busca na lista de extensões |
| `getSupportedExtensions()` | `fbx, obj, gltf, glb, dae, 3ds, blend, ase, ifc, xgl, zgl, ply, lwo, lws, lxo, stl, x, ac, ms3d` |

## 2. Fluxo do `import()`

Flags de pós-processamento do Assimp: `aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace | aiProcess_FlipUVs` — ou seja, o resto do pipeline pode assumir **triângulos**, normais presentes e UVs no padrão esperado pelo Filament.

1. `ReadFile`; falha (ou cena sem meshes) → log em `cerr` e `false`.
2. Extrai o **diretório base** do arquivo (para resolver texturas relativas).
3. **Materiais primeiro**: `materials.clear()` e um `MaterialData` por `aiMaterial` (§3) — a ordem não importa, pois o lookup posterior é **por nome**.
4. Reseta o `rootNode` recebido (limpa filhos, `name="root"`, transform identidade) — o chamador pode reusar o mesmo `Asset3dData` em vários imports (a main faz isso).
5. `processNode` recursivo (§4).

**Estado interno**: `m_importer` (o `Assimp::Importer` — a cena `aiScene` pertence a ele e morre no próximo `ReadFile`; nada dela sobrevive fora do `import`) e `m_currentScene` (ponteiro temporário durante o import, zerado no fim). Isso torna a classe **não reentrante/não thread-safe** — um import por vez por instância.

## 3. Mapeamento de materiais (Assimp → `MaterialData`)

O importer traduz materiais legados e PBR para o modelo PBR do core:

| Campo `MaterialData` | Fonte Assimp (em ordem de preferência) |
|---|---|
| `name` | `AI_MATKEY_NAME` — **é a chave** que o `MeshAsset3dData::materialName` referencia |
| `baseColorFactor` | `AI_MATKEY_COLOR_DIFFUSE` (alpha fixo 1.0) |
| `metallicFactor` | chave glTF `$mat.gltf.pbrMetallicRoughness.metallicFactor` → fallback `AI_MATKEY_REFLECTIVITY` |
| `roughnessFactor` | chave glTF `...roughnessFactor` → fallback `1 - min(shininess/100, 1)` |
| `emissiveFactor` | `AI_MATKEY_COLOR_EMISSIVE` |
| `baseColorTexture` | `aiTextureType_DIFFUSE` → `BASE_COLOR` (sRGB = true) |
| `normalTexture` | `NORMALS` → `HEIGHT` (sRGB = false) |
| `metallicRoughnessTexture` | `METALNESS` (sRGB = false) |
| `occlusionTexture` | `AMBIENT_OCCLUSION` → `LIGHTMAP` (sRGB = false) |
| `emissiveTexture` | `EMISSIVE` (sRGB = true) |

Paths de textura (`getTexturePath`): relativos são resolvidos contra o diretório do arquivo; separadores normalizados para `/`. **Somente `TextureInfo::path` é preenchido** — texturas embutidas (`embeddedData`) ainda não são extraídas (dívida, §6).

## 4. Mapeamento de hierarquia (aiNode → `Asset3dData`)

`processNode` espelha a árvore do Assimp na árvore do core:

- Cada `aiNode` (exceto o root, que reaproveita o `rootNode` do chamador) vira um `Asset3dData` **container** com `name` e `localTransform` (conversão coluna-major em `toGlmMatrix`).
- Cada mesh referenciado pelo nó vira um `MeshAsset3dData` **filho** do container, com transform identidade (herda do pai). Consequência: um `aiNode` com N meshes vira 1 container + N filhos mesh — o factory Filament achata containers vazios depois.
- `materialName` do mesh = nome do `aiMaterial` correspondente (lookup por índice na cena Assimp, gravado como nome).

`populateMeshData` preenche a geometria: positions (calculando bounds min/max/center/radius no caminho), normals (fallback `(0,1,0)` se ausentes — raro, dado `GenSmoothNormals`), UVs do canal 0 (fallback `(0,0)`), e índices apenas de faces com exatamente 3 índices (não-triângulos são descartados silenciosamente — inofensivo com `Triangulate` ativo).

## 5. Uso típico (como a main usa hoje)

```cpp
auto importer = std::make_unique<AssimpImporter>();
Asset3dData rootNode;                       // reutilizável entre imports
std::vector<MaterialData> materials;
if (importer->import(path, rootNode, materials)) {
    int id = currentScene->create(rootNode, materials,
                 TransformUtils<FilamentAsset3dTransform>::build());
    // create() CLONA rootNode → rootNode/materials podem ser reusados no próximo import
}
```

O `import` pode rodar em **qualquer thread** (só CPU); é o `Scene::create` que faz o handoff seguro para a render thread.

## 6. Dívidas específicas do módulo

- Texturas **embutidas** (FBX/GLB com texturas no próprio arquivo, paths `*0`) não são suportadas — `TextureInfo::embeddedData` existe no core mas nunca é preenchido.
- Não há suporte a: cores de vértice, múltiplos canais de UV, bones/skinning e animações (relevante para a futura integração ozz), câmeras/luzes da cena importada.
- `canImport`/`getSupportedExtensions` existem mas **ninguém os chama** ainda (não há dispatcher de importers; a main instancia `AssimpImporter` direto).
- Classe não thread-safe (estado `m_importer`/`m_currentScene`) — uma instância por thread se houver import paralelo no futuro.

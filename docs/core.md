# LiteEngine — Core (camada agnóstica)

> Parte da [documentação de arquitetura](ARCHITECTURE.md). Implementações concretas: [rendering/filament.md](rendering/filament.md) · [ui/cef.md](ui/cef.md) · [assets/assimp.md](assets/assimp.md).

O core (`include/core/**`, `core/src/**`, mais `include/editor/**`) define **todos os contratos** da engine: interfaces abstratas, templates parametrizados por concepts e tipos de dados agnósticos. Nenhum arquivo de `include/core/**` inclui Filament, CEF, SDL ou Assimp — a única dependência externa é **GLM** (matemática) e a STL.

> ⚠️ **Exceção real (2026-08/09), em `include/editor/**`**: `GizmoSystem.h` e `systems/EditorNavigationSystem.h` incluem `<SDL.h>` e recebem `SDL_Event` cru; `EditorSceneConfigurer.h` cita `FilamentAsset3dTransform`. `include/core/**` continua limpo.

## 1. Regra de camadas

```
┌─────────────────────────────────────────────────────┐
│                       core                          │
│  concepts · dados · Scene · UI abstrata · input     │
│  (depende só de GLM + STL)                          │
└────────────▲──────────────▲──────────────▲──────────┘
             │              │              │
      filament/        CEF/          assimp/
   (render concreto) (UI concreta) (import concreto)
```

- A direção de dependência é **sempre** das implementações para o core.
- `include/core/concepts/EngineConcepts.h` (umbrella) **nunca** pode referenciar Filament/CEF — os concepts só citam classes abstratas do core.
- O acoplamento entre core e implementações acontece em **um único ponto por combinação**: a classe de "amarração" (ex.: `FilamentScene`, que instancia o template `Scene` com os tipos concretos — documentada em [rendering/filament.md](rendering/filament.md)).

## 2. Concepts — o mecanismo de plugabilidade

Local: `include/core/concepts/`. Umbrella header: `EngineConcepts.h` — hoje inclui **sete**: os cinco originais mais `SceneConcept` e `CameraConcept` (2026-09). Os dois concepts de DTO ficam **fora** do umbrella (§2.8). Histórico: foram extraídos de ~9 concepts duplicados espalhados pelo código (plano em `extraindo_concepts.txt`, na raiz).

Os concepts são a "cola" que permite ao core ser genérico sobre a tecnologia: em vez de herança com ponteiros para interfaces em todos os lugares, os templates do core (ex.: `Scene`, `UIInstance`, `WireframeSystem`) recebem **tipos concretos** como parâmetros e os concepts garantem em tempo de compilação que esses tipos honram os contratos. Isso dá despacho estático (sem custo de vtable nos hot paths dos templates) mantendo o desacoplamento.

### 2.1 `TransformConcept` — `concepts/TransformConcept.h`

```cpp
template<typename T>
concept TransformConcept = std::derived_from<T, Asset3dTransform>;
```

- **Semântica**: T é um transform concreto — implementa a interface abstrata [`Asset3dTransform`](#41-asset3dtransform) (position/rotation/scale/matrizes em GLM).
- **É o concept mais fundamental**: quase todos os outros templates são parametrizados direta ou indiretamente por ele, porque a hierarquia de instâncias (`Asset3dInstance<Transform>`) carrega o tipo do transform.
- **Quem exige**: `Asset3dInstance<T>`, `MeshAsset3dInstance<T>`, `CameraAsset3dInstance<T>`, `Asset3dInstanceFactory<A, T>`, `Scene<..., T, ...>`, `TransformUtils<T>`.
- **Satisfeito por**: `lite::FilamentAsset3dTransform`.

### 2.2 `Asset3dConcept` — `concepts/Asset3dConcept.h`

```cpp
template<typename A>
concept Asset3dConcept =
    requires { typename A::TransformType; } &&
    std::derived_from<A, Asset3dInstance<typename A::TransformType>>;
```

- **Semântica**: A é um nó de instância 3D concreto. O contrato tem duas partes: (1) A **exporta o alias `TransformType`** (definido na base `Asset3dInstance` como `using TransformType = Transform`), e (2) A deriva de `Asset3dInstance` instanciado com *esse mesmo* transform. Isso amarra o asset ao seu transform sem precisar de um segundo parâmetro de template em quem consome.
- **Padrão importante**: o idioma `requires { typename A::TransformType; }` + `derived_from` aparece também em `MeshAsset3dConcept` e `Asset3dInstanceFactoryConcept` — é a forma do projeto de expressar "tipo associado" (mini traits via aliases membros).
- **Quem exige**: `Scene<Asset, ...>` (1º parâmetro), `Asset3dInstanceFactory<Asset, Transform>` (1º parâmetro).
- **Satisfeito por**: `lite::FilamentAsset3dInstance` (e, transitivamente, `FilamentMeshAsset3dInstance` — mas a `Scene` concreta usa o tipo raiz).

### 2.3 `MeshAsset3dConcept` — `concepts/MeshAsset3dConcept.h`

```cpp
template<typename M>
concept MeshAsset3dConcept =
    requires { typename M::TransformType; } &&
    std::derived_from<M, MeshAsset3dInstance<typename M::TransformType>>;
```

- **Semântica**: refinamento de `Asset3dConcept` para meshes — M deriva de `MeshAsset3dInstance<M::TransformType>` (que por sua vez deriva de `Asset3dInstance`). Usado quando o consumidor precisa de geometria, não de um nó qualquer.
- **Quem exige**: `WireframeSystem<MeshType>` (editor).
- **Satisfeito por**: `lite::FilamentMeshAsset3dInstance`.

### 2.4 `Asset3dInstanceFactoryConcept` — `concepts/Asset3dInstanceFactoryConcept.h`

```cpp
template<typename ISF>
concept Asset3dInstanceFactoryConcept =
    requires { typename ISF::AssetType; typename ISF::TransformType; } &&
    std::derived_from<ISF,
        Asset3dInstanceFactory<typename ISF::AssetType, typename ISF::TransformType>>;
```

- **Semântica**: ISF é uma fábrica de instâncias GPU. Exporta **dois** tipos associados (`AssetType`, `TransformType`, definidos como aliases na base `Asset3dInstanceFactory`) e deriva da base instanciada com eles. Assim a `Scene` consegue validar que o factory produz exatamente o `AssetType`/`TransformType` que ela mesma usa.
- **Quem exige**: `Scene<..., InstanceFactory, ...>` (3º parâmetro).
- **Satisfeito por**: `lite::FilamentInstanceFactory` (`AssetType = FilamentAsset3dInstance`, `TransformType = FilamentAsset3dTransform`).

### 2.5 `UIRendererConcept` — `concepts/UIRendererConcept.h`

```cpp
template<typename T>
concept UIRendererConcept = std::derived_from<T, UIRenderer<typename T::RendererType>>;
```

- **Semântica**: T é um renderer de UI concreto. O tipo associado `RendererType` (alias na base `UIRenderer<R>`) é o **tipo do renderer gráfico da engine host** sobre o qual a UI desenha (no caso concreto, `filament::Renderer`). Ou seja: o concept desacopla a UI do core, mas o renderer de UI concreto se declara compatível com um renderer gráfico específico.
- **Quem exige**: `Scene<..., UIRenderer>` (4º parâmetro), `UIInstance<URI>`, e **toda** a família `UIElement<URT>` e derivados.
- **Satisfeito por**: `lite::CEF_Filament_UIRendererThreaded` (`RendererType = filament::Renderer`).

### 2.6 `SceneConcept` — `concepts/SceneConcept.h` (2026-09)

```cpp
template<typename S>
concept SceneConcept =
    requires { typename S::SceneAsset3d; typename S::SceneTransform;
               typename S::SceneAsset3dFactory; typename S::SceneUIRenderer; } &&
    std::derived_from<S, lite::Scene<
        typename S::SceneAsset3d, typename S::SceneTransform,
        typename S::SceneAsset3dFactory, typename S::SceneUIRenderer>>;
```

- **Semântica**: S é uma cena concreta. Os **quatro** aliases exigidos já eram publicados pela `Scene` desde antes (`using SceneAsset3d = AssetType;` etc.) — este concept é o que finalmente os consome. É a "evolução futura" que o §4.9 previa: `SceneRenderer`, `SceneConfigurer` e `SceneFactory` deixaram de receber `typename SceneType` solto e passaram a ser constrainados.
- **Detalhe de implementação**: o header **forward-declara** `lite::Scene` em vez de incluir `Scene.h` — incluir fecharia um ciclo, porque `Scene.h` inclui o umbrella `EngineConcepts.h`. As constraints do forward declare precisam ser **idênticas** às do `Scene.h`; mudar uma sem a outra dá erro de redeclaração.
- **Quem exige**: `SceneRenderer<SceneType, CameraType>`, `SceneConfigurer<SceneType>`, `SceneFactory<SceneType>`, `EditorSceneConfigurer<...>`.
- **Satisfeito por**: `FilamentScene` e `FilamentOverlayScene`.

### 2.7 `CameraConcept` — `concepts/CameraConcept.h` (2026-09)

```cpp
template<typename C>
concept CameraConcept =
    requires { typename C::TransformType; } &&
    std::derived_from<C, CameraAsset3dInstance<typename C::TransformType>>;
```

- **Semântica**: mesmo idioma de tipo associado do `Asset3dConcept`, aplicado à câmera. Também usa forward declare pelo mesmo motivo de ciclo.
- **Quem exige**: `SceneRenderer<SceneType, CameraType>` (2º parâmetro) — o que permitiu subir `getCurrentCamera()` para o contrato do core (§4.9).
- **Satisfeito por**: `lite::FilamentCameraAsset3dInstance`.

### 2.8 Concepts de DTO — fora do umbrella

`InstanceDTOConcept` e `SceneDTOConcept` (`concepts/InstanceDTOConcept.h` / `SceneDTOConcept.h`) são `std::derived_from<D, Asset3dInstanceDTO>` e `std::derived_from<D, SceneDTO>` — aceitam a própria base ou qualquer filha. **Não estão em `EngineConcepts.h`**: pertencem à camada de serialização (§8), não ao contrato de plugabilidade da engine.

### 2.9 Como os concepts se amarram na prática

A instanciação concreta única do projeto hoje é (definida em `include/filament/scene/FilamentScene.h`):

```cpp
class FilamentScene : public lite::Scene<
    lite::FilamentAsset3dInstance,          // Asset3dConcept
    lite::FilamentAsset3dTransform,         // TransformConcept
    lite::FilamentInstanceFactory,          // Asset3dInstanceFactoryConcept
    lite::CEF_Filament_UIRendererThreaded   // UIRendererConcept
> { ... };
```

Os tipos associados garantem a coerência interna: `FilamentInstanceFactory::AssetType == FilamentAsset3dInstance` e `FilamentAsset3dInstance::TransformType == FilamentAsset3dTransform`. Trocar o renderer significa escrever novas implementações desses 4 contratos e uma nova classe de amarração — o core não muda.

## 3. Hierarquias de classes — mapa completo

Legenda: `[A]` = abstrata/interface (métodos puros), `[T]` = template, `(F)` = implementação Filament, `(C)` = implementação CEF, `(As)` = implementação Assimp. Implementações concretas detalhadas nos docs de módulo.

### 3.1 Dados CPU (importação, sem GPU)

```
Asset3dData                          # nó de cena CPU (hierarquia + transform GLM)
└── MeshAsset3dData                  # + geometria, bounds, materialName

MaterialData [A*]                    # base polimórfica: só name + clone() (2026-08-10)
└── MPBRLitMaterialData              # + fatores PBR e as 5 texturas opcionais
TextureInfo                          # struct de textura (path ou dados embutidos)

MaterialInstance [A]                 # par do MaterialData do lado GPU; NÃO é nó (sem transform)
└── MPBRLitMaterialInstance [A]      # get/set dos 4 fatores PBR (sem set de textura)
    └── FilamentMPBRLitMaterialInstance (F)
```

`[A*]` = não tem método puro, mas é usada polimorficamente. **Mudança de 2026-08-10**: `MaterialData` deixou de ser um struct PBR fechado e virou base de modelo de shading; os parâmetros desceram para `MPBRLitMaterialData` ("Metallic-PBR Lit" — o que o `lit.filamat` implementa). Consequência que atravessa o pipeline inteiro: **material agora anda por ponteiro** (`std::vector<std::unique_ptr<MaterialData>>` em `Asset3dImporter::import`, `Scene::create` e `Asset3dInstanceFactory::instantiateAsset`), porque vetor de valores faria slicing. `clone()` virtual existe pelo mesmo motivo do `Asset3dData::clone()`: a fila de criação da `Scene` guarda cópia própria e `unique_ptr` não é copiável.

### 3.2 Transform

```
Asset3dTransform [A]                 # interface pura, 100% GLM
└── FilamentAsset3dTransform (F)     # facade sobre filament::TransformManager
```

### 3.3 Instâncias (GPU, espelham a árvore de dados)

```
Node                                              # base NÃO-template: só parent/children/addChild
└── Asset3dInstance<Transform> [T]                # nó instanciado; exporta TransformType
    ├── MeshAsset3dInstance<Transform> [T]        # + materialName; isMesh()==true; contratos de geometria CPU (getVertex/getIndex/getUVS/boundingBox)
    │   └── FilamentMeshAsset3dInstance (F)       # <FilamentAsset3dTransform> + VB/IB/entity
    ├── CameraAsset3dInstance<Transform> [T][A]   # câmera abstrata (view/proj/lookAt)
    │   └── FilamentCameraAsset3dInstance (F)     # <FilamentAsset3dTransform> + filament::Camera
    └── FilamentAsset3dInstance (F)               # nó raiz/intermediário Filament + entity
```

**`lite::Node` (`include/core/data/assets/Node.h`, 2026-08-12)** é a mudança estrutural desta hierarquia. Existe **só para dar um tipo comum aos elos da árvore**: `Asset3dInstance` é template, e duas instanciações com argumentos diferentes **não têm relação de herança** (invariância de template) — sem uma base não-template, um mesmo vetor de filhos não conseguiria guardar nós de tipos parametrizados diferentes. Consequências práticas:

- `parent` (raw, não-dono), `children` (`vector<unique_ptr<Node>>`) e `addChild(Node*)` **moraram para `Node`** — saíram do `Asset3dInstance`;
- o destrutor de `Node` é `virtual` **obrigatoriamente**: a destruição acontece por `Node*`, e sem isso os destrutores das derivadas (que liberam recursos de backend) não rodam;
- quem percorre a árvore recebe `Node*` e **precisa de `dynamic_cast`** para chegar a id/transform/isMesh — é o que `Scene::getNode`, `Scene::assignChildIds` e os mappers de DTO fazem;
- `addChild` continua **tomando posse do ponteiro cru** (o duplo-dono com a `Scene` segue em aberto).

> ⚠️ O comentário no topo de `Node.h` diz que `Asset3dInstance` é "template em `<Transform, DTO>`" — **não é**: em `Asset3dInstance.h` ele tem um parâmetro só (`template<TransformConcept Transform>`). O comentário descreve um desenho que não chegou ao código.

### 3.4 Importação e instanciação

```
Asset3dImporter [A]                               # arquivo → Asset3dData + MaterialData
└── AssimpImporter (As)

Asset3dInstanceFactory<Asset, Transform> [T][A]   # Asset3dData → Asset (GPU); exporta AssetType/TransformType
└── FilamentInstanceFactory (F)
```

### 3.5 Cena e sistemas

```
Scene<Asset, Transform, Factory, UIRenderer> [T]  # dona das instâncias E dos systems; ciclo de frame
└── FilamentScene (F)                             # + Renderer/Scene/View/SwapChain do Filament
    └── FilamentOverlayScene (F)                  # cena de overlay: NÃO abre frame nem se desenha (gizmo)

SceneRenderer<SceneConcept, CameraConcept> [T][A] # facade da render thread (Template Method:
└── FilamentSceneRenderer (F)                     #   setup/renderFrame/cleanup virtuais)

SceneConfigurer<SceneConcept> [T][A]              # configure(scene) — §10
└── EditorSceneConfigurer<7 params> [T][A]        # systems + UI + IBL + asset inicial do editor
    └── FilamentEditorSceneConfigurer (F)         # fixa os 7 tipos concretos

SceneFactory<SceneConcept> [T][A]                 # build() → cena; SEM implementação ainda

View [A]                                          # janela/superfície — §9
└── SDLFilamentView (F)                           # SDL_Init + janela + handle nativo

SceneScopeSystem [A]                              # 6 hooks de frame (onFrameBegin..onFrameEnd)
├── WireframeSystem<MeshAsset3dConcept> [T][A]    # overlay wireframe agnóstico (editor)
│   └── FilamentWireframeSystem (F)
├── ObjectSelectorSystem<SceneType, Transform> [T] # picking por raio, 100% GLM (editor)
│   └── FilamentObjectSelectorSystem (F)          # só fixa os parâmetros de template
├── GizmoSystem<SceneType, Transform> [T][A]      # gizmo de transformação (dono de cena de overlay)
│   └── FilamentGizmoSystem (F)
└── EditorNavigationSystem                        # câmera FPS (NÃO-template; consome SDL_Event cru)

IBL [A]                                           # iluminação baseada em imagem
└── FilamentIBL (F)
```

### 3.6 UI

```
UIRenderer<R> [T][A]                              # renderer de UI; exporta RendererType=R
└── CEF_Filament_UIRendererThreaded (C)           # R = filament::Renderer (+ 6 bases CEF)

UIInstance<URI:UIRendererConcept> [T][A]          # "documento" de UI; factory method createRoot()
└── CEF_Filament_UIInstance (C)

UIElement<URT:UIRendererConcept> [T]              # base de widget: id, draw(), eventos por nome
├── UIPanelElement<URT> [T][A]                    # container em grid; drawContainer() puro
│   └── CEF_UIPanelElement (C)
├── UITextElement<URT> [T][A]
│   └── CEF_UITextElement (C)
├── UITextInputElement<URT> [T][A]
│   └── CEF_UITextInputElement (C)
├── UICheckBoxElement<URT> [T][A]
│   └── CEF_UICheckBoxElement (C)
├── UIComboBoxInputElement<URT> [T][A]
│   └── CEF_UIComboBoxInputElement (C)
├── UIButtonElement<URT> [T]
│   └── CEF_UIButtonElement (C)
└── UITabElement<URT> [T]                         # vazio (placeholder)

UIElementHandler                                  # type-erasure (void* + type_index)
```

### 3.7 Facades

Classes que escondem subsistemas inteiros (padrão Facade). O facade da render thread agora **tem** contraparte abstrata no core (`SceneRenderer<SceneType>`, §4.9); os demais ainda não:

| Facade | Interface no core | Esconde | Doc |
|---|---|---|---|
| `FilamentSceneRenderer` | `SceneRenderer<FilamentScene, FilamentCameraAsset3dInstance>` | render thread + `filament::Engine` + SwapChain + ciclo de vida da `FilamentScene` e da câmera | [rendering/filament.md §2](rendering/filament.md) |
| `SDLFilamentView` | `View` | `SDL_Init`, janela e handle nativo | [rendering/filament.md §10](rendering/filament.md) |
| `FilamentAsset3dTransform` | `Asset3dTransform` (é simultaneamente implementação e facade) | `filament::TransformManager` | [rendering/filament.md §4](rendering/filament.md) |
| `FilamentUtils` | — | singleton global do `filament::Engine*` | [rendering/filament.md §8](rendering/filament.md) |
| `CEF_Filament_UIRendererThreaded` | `UIRenderer<filament::Renderer>` | thread CEF, browser offscreen, ponte JS↔C++, textura/quad de composição | [ui/cef.md §3](ui/cef.md) |

A intenção do projeto (ver `create_sceneRenderer.txt`) é que a `main` só converse com facades — nunca com Filament/CEF diretamente. **Progresso em 2026-09-19**: os pontos que violavam isso (`getFilamentScene()`, `FilamentUtils::getEngine()` no setup do wireframe e do gizmo) **saíram da `main`** — mas foram para o `FilamentEditorSceneConfigurer`, que é o lugar certo para eles (é a classe de amarração do editor com o backend). O que sobra de concreto na `main` é a construção do renderer/view, o `DummySceneDTOMapper` e a lógica de clique, que ainda cita `FilamentMeshAsset3dInstance`.

## 4. Classes do core em detalhe

### 4.1 `Asset3dTransform` — `include/core/data/assets/Asset3dTransform.h`

Interface pura de transform, 100% GLM. Contrato:

| Método | Assinatura | Obrigatório |
|---|---|---|
| Posição | `setPosition(vec3, bool isWorldSpace=false)` / `getPosition(bool isWorldSpace=false)` | puro |
| Rotação | `setRotation(quat, bool isWorldSpace=false)` / `getRotation(bool isWorldSpace=false)` | puro |
| Escala | `setScale(vec3, bool isWorldSpace=false)` / `getScale(bool isWorldSpace=false)` | puro |
| Euler | `setEulerAngles(vec3 graus, bool isWorldSpace=false)` / `getEulerAngles(bool isWorldSpace=false)` | virtual com default (converte via quat) — impl. em `core/src/data/assets/Asset3dTransform.cpp` |
| Matrizes | `setLocalMatrix(mat4)` / `setWorldMatrix(mat4)` / `getLocalMatrix()` / `getWorldMatrix()` | puros |

O flag `isWorldSpace` (default `false` = local, adicionado em 2026-07-25) permite ler/escrever em espaço de mundo; a impl Filament ainda tem essa via **em implementação (WIP)** — ver [rendering/filament.md §4.3](rendering/filament.md). `setWorldMatrix` **subiu para o contrato base em 2026-09-20** (antes existia só em `FilamentAsset3dTransform`): é por ele que o `GizmoSystem` escreve o resultado do drag, já que a transformação de um conjunto é calculada como matriz e não como T/R/S separados (§4.15).

Note que a interface **não** define hierarquia — parent/child é responsabilidade do `Asset3dInstance` (e, no Filament, do `TransformManager` por baixo).

### 4.2 `Asset3dData` / `MeshAsset3dData` — dados CPU

`Asset3dData` **é** o nó da cena (comentário no código: "This IS the node - no separate SceneNode concept"):
- `name`, `localTransform` (`glm::mat4`, relativo ao pai);
- `parent` (raw pointer, **não-dono**) + `children` (`vector<unique_ptr<Asset3dData>>` — a árvore é dona dos filhos);
- `addChild<T>(args...)` — cria filho tipado e seta parent automaticamente;
- `getWorldTransform()` — recursivo até a raiz (`core/src/data/assets/Asset3dData.cpp`);
- `clone()` **virtual e profundo** — essencial: `Scene::create` clona a árvore para a fila de criação, então o chamador pode descartar/reusar a original;
- `isMesh()` virtual (RTTI manual usado em todo o pipeline em vez de `dynamic_cast` no hot path).

`MeshAsset3dData` acrescenta: `positions/normals/uvs` (`vector<glm::vec3/vec2>`), `indices` (`vector<uint32_t>`), bounding box (`boundsMin/boundsMax/center/radius`) e **`materialName`** — materiais são referenciados **por nome, não por índice** (o importer nomeia; o factory resolve num mapa nome→instância).

### 4.3 `MaterialData` / `TextureInfo`

PBR agnóstico: `baseColorFactor` (vec4), `metallicFactor`, `roughnessFactor`, `emissiveFactor` (vec3), e cinco texturas opcionais (`std::optional<TextureInfo>`): baseColor, normal, metallicRoughness, occlusion, emissive. `TextureInfo` carrega `path` **ou** `embeddedData`, dimensões e flag `sRGB`. `name` do material é a chave de lookup.

### 4.4 `Asset3dInstance<Transform>` — nó instanciado

Template parametrizado por `TransformConcept`. Exporta `using TransformType = Transform` (base do idioma de tipos associados dos concepts). Estado e API:
- **`getId()/setId()` — id no espaço de numeração da Scene** (`-1` = sem id). A raiz recebe o mesmo id pré-alocado pelo `create()` (chave de `m_3dInstances`); filhos só recebem ids se o asset foi criado com `deepIds=true`. Atribuição é exclusiva da `Scene` (`instantiate()`); nós fora de cena (ex.: câmera do renderer) ficam com `-1`;
- `m_transform` (`unique_ptr<Transform>`) — acessível via `getTransform()`, que devolve `Transform*`, o **tipo concreto do template** (sobrecarga const devolve `const Transform*`);
- conveniências `getLocalMatrix()/getWorldMatrix()/setLocalMatrix()` delegando ao transform (com fallback identidade se nulo);
- **hierarquia não mora mais aqui** (2026-08-12): `parent`, `children` e `addChild` foram para a base não-template `lite::Node` (§3.3). A semântica é a mesma — `parent` raw não-dono, `children` de `unique_ptr<Node>`, **`addChild(ptr)` toma posse do ponteiro cru** (quem chama não pode deletar) — mas o vetor agora é de `Node*`, então percorrer a árvore exige `dynamic_cast` para chegar a id/transform. Há um `TODO` no `Node::addChild` para torná-lo virtual e deixar o filho concreto parentear também as entities do backend;
- `isMesh()` virtual, `setVisible/isVisible`.

**Invariante do pipeline**: a árvore de `Asset3dInstance` espelha a árvore de `Asset3dData` que a originou (com nós vazios possivelmente achatados pelo factory).

### 4.5 `MeshAsset3dInstance<Transform>` / `CameraAsset3dInstance<Transform>`

- `MeshAsset3dInstance`: acrescenta `materialName`, força `isMesh()==true` e define os **contratos virtuais puros de geometria CPU** (2026-07): `getVertex() → vector<vec3>`, `getIndex() → vector<int64_t>`, `getUVS(int canal) → vector<vec2>` e o trio de bounding box **por mesh** `{min, max}` — `getBoundingBox()` (lazy: calcula via `calcBoundingBox()` na 1ª chamada se não setado; **espaço LOCAL do mesh**), `setBoundingBox()`, `calcBoundingBox()`. São a base do picking agnóstico (`ObjectSelectorSystem`, §4.12). Os recursos GPU e o armazenamento de fato ficam na subclasse concreta. Além do bound por mesh, expõe **`getCompleteBoundingBox()`** (não-virtual, na base): AABB `{min, max}` **agregada** deste nó + todos os meshes descendentes, no **espaço LOCAL deste nó** (cada mesh entra pela transform relativa `inverse(this.world) * mesh.world`, então o resultado independe da world transform do próprio nó); traversal igual ao do picking (nó sem geometria não contribui, mas a recursão desce pelos filhos); **sem cache** (recalcula a cada chamada). Usado pelo gizmo para dimensionar a escala em tela.
- `CameraAsset3dInstance`: câmera **é um nó da cena** (deriva de `Asset3dInstance`). Contrato puro: `getViewMatrix()`, `getProjectionMatrix()`, `getFieldOfViewInRadians()` (abertura vertical da lente, em radianos), `lookAt(center)` (eye vem do transform, up é mundo (0,1,0)), `setProjection(fovDeg, aspect, near, far)`.

### 4.6 `Asset3dImporter` — `include/core/assets/importer/Asset3dImporter.h`

Interface de importação (não-template — trabalha só com tipos CPU agnósticos):

```cpp
virtual bool import(const std::string& filePath,
                    Asset3dData& rootNode,                 // preenchido por referência
                    std::vector<std::unique_ptr<MaterialData>>& materials) = 0;
virtual bool canImport(const std::string& extension) const = 0;
virtual std::vector<std::string> getSupportedExtensions() const = 0;
```

Desenhada para múltiplos importers coexistirem (dispatch por extensão via `canImport`). Implementação atual: [`AssimpImporter`](assets/assimp.md).

### 4.7 `Asset3dInstanceFactory<Asset, Transform>` — `include/core/assets/instanceFactory/Asset3dInstanceFactory.h`

Interface da fábrica de instâncias GPU. Exporta `AssetType`/`TransformType` (exigidos pelo concept). Contrato:

```cpp
virtual std::unique_ptr<Asset> instantiateAsset(
    const Asset3dData& rootNode, Transform transform,
    const std::vector<std::unique_ptr<MaterialData>>& materials) = 0;
virtual bool destroyAsset(Asset* instance) = 0;
```

O vetor passou a ser de `unique_ptr` em 2026-08-10, quando `MaterialData` virou base polimórfica (§3.1) — a implementação precisa fazer `dynamic_cast` para `MPBRLitMaterialData` antes de ler os fatores PBR.

**Semântica de `destroyAsset` (importante)**: o contrato permite destruição adiada — a implementação Filament apenas *marca* e enfileira; a liberação GPU real acontece pós-frame (ver [rendering/filament.md §5](rendering/filament.md)). Consumidores não devem assumir que o recurso morreu ao retornar `true`.

### 4.8 `Scene<Asset, Transform, Factory, UIRenderer>` — `include/core/scene/Scene.h`

O coração do core. Parametrizada pelos 4 concepts. **Possui** (unique_ptr) o factory e o uiRenderer, recebidos no construtor.

**Estado**:
- `m_3dInstances : map<int, unique_ptr<Asset>>` — instâncias vivas, chaveadas por id crescente (`m_lastId`);
- `m_creatingObjects : vector<CreationEntry>` — fila de criação (`{id, clone dos dados, materiais, transform}`);
- `m_instancesMutex` + `m_instantiatedCV` — protegem ambos e acordam quem espera em `get()`;
- `m_systems : vector<unique_ptr<SceneScopeSystem>>` (**dono** desde 2026-09-19, sem lock) — **único mecanismo de extensão do frame** (§4.10); `addSystem`/`removeSystem` antes do `start()` ou via `postCommand`.

**API thread-safe de assets**:
- `create(data, materials, transform, deepIds=false) → int` — `materials` é `const vector<unique_ptr<MaterialData>>&` e a fila leva uma **cópia própria** dele (`clone()` polimórfico por elemento, mesmo tratamento que `data.clone()` dá à árvore); clona `data`, enfileira, retorna id **imediatamente** (a instanciação real acontece na render thread, dentro de `update`). O id retornado é estampado na raiz da instância (`getId()`) no `instantiate()`; com `deepIds=true`, **todos** os nós da árvore recebem ids do mesmo espaço de numeração (`m_lastId`, sob o mesmo mutex), em ordem determinística de percurso — base para endereçar subobjetos (usado pelo picking do `ObjectSelectorSystem` e pelo gizmo). **Nota**: o índice definitivo de subobjetos por id segue adiado (dupla posse e tipo impedem filhos em `m_3dInstances`; ver memória do projeto) — o paliativo é o `getNode` abaixo;
- `get(id) → Asset*` — empréstimo (dono continua sendo a Scene). Se o id ainda está na fila, **bloqueia** na CV até ser instanciado; se não existe, `nullptr`;
- `getNode(id) → Asset3dInstance<T>*` — busca por id em **toda a hierarquia** (raízes e filhos com `deepIds`). Retorna o tipo **base** dos nós (filhos, ex.: meshes, não são `Asset`). Fast-path no mapa + DFS linear O(nós) — paliativo até o índice. Ao contrário de `get()`, **não bloqueia**: ids de filhos só nascem no `instantiate()`, então id desconhecido → `nullptr`;
- `find(predicate) → vector<Asset*>` — filtro sobre as instâncias vivas;
- `getAll() → vector<Asset*>` (2026-08) — todas as raízes vivas, **sem filtro e sem marcar deletados**. ⚠️ **Não tranca `m_instancesMutex`**, ao contrário de todos os outros acessores — é o único ponto da API com essa assimetria, e é justamente o que o `SceneDTOMapper` usa para serializar (§8);
- `destroy(id)` / `destroy(unique_ptr)` — delega a `factory->destroyAsset()`. O `erase` do mapa está **comentado** (deleção adiada; ver dívidas em [ARCHITECTURE.md §5](ARCHITECTURE.md));
- `addSystem(std::unique_ptr<SceneScopeSystem>)` / `removeSystem(SceneScopeSystem*)` / `getSystemOfType<T>() → T*`.

**Posse dos systems mudou em 2026-09-19** (commit do configurer): `addSystem` passou a receber `unique_ptr` e a **Scene é dona**. Isso tem três consequências que valem para qualquer código novo:
1. quem constrói um system **entrega** e não guarda ponteiro dono — o `SceneConfigurer` devolve `unique_ptr` por isso;
2. `removeSystem` **destrói** o system, na thread de quem chamar — o antigo padrão de teardown (`postCommand([&]{ removeSystem(x); x.reset(); })`) deixou de fazer sentido e hoje está comentado na `main`, sem substituto (dívida 10 do [ARCHITECTURE.md §5](ARCHITECTURE.md));
3. para reaver um system registrado existe `getSystemOfType<SystemType>()`, restrito por `SystemConcept` (`derived_from<SceneScopeSystem>`) e implementado com **`dynamic_cast`** — ao contrário de `typeid`, aceita perguntar pela **interface** (ex.: `GizmoSystem<...>`), não só pela classe concreta. Devolve já no tipo pedido (`SystemType*`, desde 2026-09-20). É assim que a `main` recupera navegação, gizmo, seletor e wireframe depois que o configurer os registrou, e é como o callback de seleção do configurer alcança o gizmo (§4.12). Retorna o **primeiro** que casar.

> **Por que `dynamic_cast` e não um trait**: o tipo **estático** de todo elemento de `m_systems` é `SceneScopeSystem` — é o que o polimorfismo apaga. `is_base_of`/`is_same` respondem em compilação sobre tipos declarados e, por isso, não conseguem distinguir um elemento do outro (dariam a mesma resposta para os quatro). A informação "este objeto é um `FilamentGizmoSystem`" só existe em runtime, na vtable, e `dynamic_cast` é o operador que a consulta.

**Ciclo de frame — `update(dt)`** (chamado pela render thread; hooks virtuais `prepareRender`/`renderScene`/`renderUI`/`finishRender` são o que a subclasse concreta implementa; sistemas são despachados em cada fase, na ordem de registro):

```
instantiate()                    // drena fila → factory->instantiateAsset → m_3dInstances (+notify CV)
systems[].onFrameBegin(dt)       // assets criados neste frame já visíveis
m_uiRenderer->update()           // UI é responsabilidade INTERNA da Scene (ela possui o renderer)
if (prepareRender())             // virtual — ex.: beginFrame
  systems[].onRenderPrepared(dt)
  systems[].preRenderScene(dt)
  renderScene()                  // virtual PURO — ex.: render(view)
  systems[].postRenderScene(dt)
  renderUI()                     // virtual — ex.: uiRenderer->render(filamentRenderer)
  systems[].onSceneRendered(dt)
  finishRender()                 // virtual — ex.: endFrame + flush de deleções
systems[].onFrameEnd(dt)         // roda SEMPRE (mesmo com frame pulado), após o flush
```

`instantiate()` (privado) drena a fila em batch fora do lock, instancia um a um e insere no mapa sob lock, notificando a CV a cada inserção — é isso que desbloqueia `get()`.

Um TODO extenso no código indica que o **main loop completo** (input → lógica por asset/componente → render) deve migrar para cá; a integração da UI já é interna.

### 4.9 `SceneRenderer<SceneType, CameraType>` — `include/core/scene/SceneRenderer.h`

Facade abstrato da **render thread** (header-only). Possui a thread, a fila de comandos, o handshake de inicialização e o esqueleto do loop; implementações concretas (ex.: `FilamentSceneRenderer`) fornecem só as fases específicas. É um **Template Method**:

```
renderThreadMain() [privado, entry point da thread]:
    setup()        → virtual puro (bool): engine/cena/recursos; false → direto pro cleanup
    waitStart()    → concreto: spin-wait por start(), processando comandos
    renderLoop()   → concreto: while(m_running) { dt; processCommands(); renderFrame(dt); }
    [drena a fila] → comandos de teardown postados no shutdown rodam aqui (padrão:
                     "postCommand(removeSystem+reset) + stop()" para sistemas com GPU)
    cleanup()      → virtual puro: teardown completo (roda SEMPRE, mesmo com setup falho)
```

**API pública concreta** (thread-safe): `waitReady()` (promise/future — destravada com setup completo *ou* falho; `getScene()` nulo indica falha), `start()`/`stop()` (atomics + join, idempotente), `postCommand(fn)` (fila+mutex, drenada a cada frame e no spin-wait), `setCameraState(eye, target)` (estado pendente GLM; a filha consome via helper protegido `takePendingCamera` dentro de `renderFrame`), `getScene() → SceneType*`.

**API pública virtual** (assinaturas agnósticas — GLM/string; implementações devem postar à fila): `setIBL(path, intensity)`, `addDirectionalLight(color, intensity, dir, shadows)`, `resize(w, h)` e **`getCurrentCamera() → CameraType*`** (subiu para o contrato em 2026-09; antes só existia na `FilamentSceneRenderer`).

**Construção**: `SceneRenderer(lite::View* view, int width, int height)`. Desde 2026-08-31 a base recebe a **`View`** (§9), não mais o handle nativo da janela — o ponteiro fica em `m_view` (não-dono; há um `TODO` de virar `unique_ptr`) e é a implementação concreta que sabe extrair dele o que o backend precisa. Quem chama `View::Init()` é a `main`, antes de construir o renderer.

**Contrato de threading com a filha** (comentário `THREADING` no header — consequência de misturar herança virtual com thread própria):
1. A base **não** inicia a thread no construtor (a thread despacharia virtuais de um objeto em construção → *pure virtual call*). A filha chama o protegido `launchRenderThread()` como **última instrução do próprio construtor**.
2. A filha chama `stop()` no **próprio destrutor** (o join precisa completar antes da parte derivada ser destruída; o `stop()` do destrutor da base é só cinto de segurança).
3. `setup()`, `renderFrame()` e `cleanup()` executam **na render thread** — é aí que recursos GPU podem ser criados/destruídos.

Os parâmetros resolvem o problema de `Scene` e `CameraAsset3dInstance` serem templates sem base comum: `getScene()` e `getCurrentCamera()` ficam tipados sem o core conhecer o backend. **A "evolução futura" que este parágrafo previa aconteceu em 2026-09**: os dois parâmetros deixaram de ser `typename` solto e hoje são constrainados por `SceneConcept` e `CameraConcept` (§2.6 e §2.7), que consomem os aliases que a `Scene` já publicava.

### 4.10 `SceneScopeSystem` — `include/core/SceneScopeSystem.h`

**Único ponto de extensão do frame** (padrão Interceptor / lifecycle hooks — os antigos 4 vetores públicos de callbacks foram absorvidos aqui). Seis hooks com corpo vazio default — implemente só o que precisar:

| Hook | Quando dispara | Roda com frame pulado? |
|---|---|---|
| `onFrameBegin(dt)` | após `instantiate()`, antes do `prepareRender` | ✅ |
| `onRenderPrepared(dt)` | após `beginFrame` | ❌ |
| `preRenderScene(dt)` | antes de `renderScene()` | ❌ |
| `postRenderScene(dt)` | depois de `renderScene()` | ❌ |
| `onSceneRendered(dt)` | após a composição da UI (`renderUI`), antes do `endFrame` | ❌ |
| `onFrameEnd(dt)` | após `finishRender()` (deleções GPU já flushadas) | ✅ |

Registrado por **`unique_ptr`** via `Scene::addSystem` — a `Scene` é a dona desde 2026-09-19 (antes era ponteiro não-dono); `removeSystem(ptr)` **destrói** o system. **Regras** (documentadas no header): hooks executam na render thread; registro/remoção antes do `start()` ou via `postCommand`; hook nunca chama `Scene::get()` de id ainda na fila (deadlock — `instantiate()` roda na mesma thread). Exemplo completo de uso das fases: `FilamentWireframeSystem` ([rendering/filament.md §8](rendering/filament.md)).

> ⚠️ A troca de posse deixou o **teardown de systems com GPU sem dono claro** e a regra de "registrar antes do `start()`" **sendo violada pela `main`** — ver dívidas 9 e 10 do [ARCHITECTURE.md §5](ARCHITECTURE.md).

### 4.11 `WireframeSystem<MeshType>` — `include/editor/WireframeSystem.h`

Sistema de editor agnóstico (overlay de wireframe), parametrizado por `MeshAsset3dConcept`. Deriva de `SceneScopeSystem` e liga `preRenderScene → update()`. Contrato puro: `initialize(w,h)`, `resize(w,h)`, `setWireframeColor(vec4)`, `setWireframeWidth(float)`, `addWireframeMesh(MeshType*)`, `removeWireframeMesh`, `clearWireframeMeshes`, `update()`. Estado protegido na base: cor, largura, dimensões e `unordered_set<MeshType*>` dos meshes rastreados. Implementação: [FilamentWireframeSystem](rendering/filament.md).

### 4.12 `ObjectSelectorSystem<SceneType, TransformType>` — `include/editor/ObjectSelectorSystem.h`

Sistema de editor agnóstico de **picking por raio**, no mesmo molde do `WireframeSystem` (deriva de `SceneScopeSystem`, liga-se à cena via `attachTo`), **header-only** — toda a lógica é GLM sobre os contratos do core: meshes são detectados por `isMesh()` + `dynamic_cast` para a classe **pai** `MeshAsset3dInstance<TransformType>` (cujos virtuais `getVertex()/getIndex()/getBoundingBox()` fornecem a geometria), então qualquer subclasse de mesh é selecionável sem reinstanciar o template. Não implementa nenhum hook hoje (serviço passivo de consulta); derivar de `SceneScopeSystem` o mantém plugável no ciclo do frame. `SceneType` segue o idioma do `SceneRenderer<SceneType>` (Scene é template sem base comum); `TransformType` existe porque `Asset3dInstance` também é template — quando a `Scene` exportar aliases (§4.9), pode colapsar para um parâmetro só. A subclasse `FilamentObjectSelectorSystem` (`include/filament/editor/FilamentObjectSelectorSystem.h`, header-only) apenas fixa `<FilamentScene, FilamentAsset3dTransform>` — não adiciona lógica nem toca recursos Filament.

API: `setCamera(CameraAsset3dInstance*)` (nó de cena da câmera — posição e view vêm da world matrix do nó, **nunca** da view do renderer; só a projeção usa o contrato agnóstico `getProjectionMatrix()`); `getCameraPosition()`; `getCameraRay(pixel, viewportSize, length)` (unprojection do pixel clicado — inverte viewport → NDC → clip → mundo via `inverse(proj * inverse(world))`, robusta às convenções de depth NO/ZO, direção escalada pelo alcance); e **duas sobrecargas de `intersect`**:
- `intersect(origin, ray, coneHalfAngle)` — **broad phase por cone**: percorre as raízes vivas da cena (`Scene::find`, filtrando `isDeleted()`) e mantém só objetos cuja esfera envolvente (agregada dos meshes descendentes) cai dentro do cone de meio-ângulo `coneHalfAngle` em torno da direção do raio e ao alcance do segmento. O `coneHalfAngle` é calculado pelo chamador a partir do FOV da câmera (`CameraAsset3dInstance::getFieldOfViewInRadians()`).
- `intersect(origin, ray, roots)` — pública, recebe um **conjunto de nós já escolhido** (sem broad phase); absorve toda a lógica posterior ao `find()`. Usada pelo gizmo, que a chama com as peças do overlay antes de cair no picking da cena principal (ver [rendering/filament.md §9](rendering/filament.md)).

Ambas fazem, por mesh: leva o segmento ao espaço local (direção não normalizada → t∈[0,1] válido mesmo com escala não uniforme), broad phase segmento×AABB (slab test sobre `getBoundingBox()`) e narrow phase **Möller–Trumbore** por triângulo; varrem TODOS os candidatos e retornam o `getId()` do mesh de **menor t** (mais próximo da origem), ou −1.

**Estado da seleção e notificação** (2026-09-20): o sistema guarda a seleção corrente em `m_selectedObjects` (`set<Asset3dInstance<TransformType>*>`) e expõe `addSelected(node)`, `clearSelected()` e `getSelectionMedianPoint()` — este último delega a `MathUtils::centroid` (§4.17), o mesmo ponto de cálculo que o gizmo usa como pivô, sobre posições lidas em **world** (`getPosition(true)`).

Mudanças de seleção são publicadas por **callbacks**: `m_onSelectedObjectsChange` é um `vector<function<void(set<Asset3dInstance<TransformType>*>)>>` percorrido por `broadcastSelectionChange()`, chamado por `addSelected` e `clearSelected`. É assim que o `EditorSceneConfigurer` liga seleção → `GizmoSystem::setOperatingAssets` sem que o selector conheça o gizmo.

Duas características do disparo que importam para quem registra callback:
- `addSelected` só insere **e só notifica** quando o objeto ainda não está na seleção (`if(!contains)`);
- `clearSelected` notifica **sempre**, então um clique simples (que limpa e seleciona) emite duas notificações: primeiro com o conjunto vazio, depois com o novo. Até 2026-09-20 o `clearSelected` chamava `empty()` no lugar de `clear()` — só perguntava se o conjunto estava vazio e descartava a resposta, de modo que a seleção **nunca era esvaziada**; combinado com o guard do `addSelected`, isso fazia as notificações cessarem assim que o usuário reclicava num objeto já selecionado.

Limitações documentadas no header: escolhe o hit de **menor t** (mais próximo — implementado 2026-07-20); ainda **sem backface culling** (faces de costas contam como hit); filhos criados sem `deepIds` têm id −1; `getUVS()` da implementação Filament ainda retorna vazio (decisão pendente).

### 4.13 `IBL` — `include/core/lightning/IBL.h`

Interface mínima de iluminação por imagem: `virtual bool load(const std::string& path) = 0` (arquivo ou diretório). Implementação: [FilamentIBL](rendering/filament.md).

### 4.14 `TransformUtils<TransformType>` — `include/core/utils/TransformUtils.h`

Fábrica estática de transforms por especialização de template: o core declara `static TransformType build()` **sem corpo genérico** e cada módulo concreto fornece a especialização (`TransformUtils<FilamentAsset3dTransform>::build()` em `filament/utils/FilamentTransformUtils.cpp`). É como a `main` constrói transforms sem saber o tipo concreto por trás.

**Armadilha**: `buildWithPosition/Rotation/Scale/build(p,r,s)` existem mas **ignoram os argumentos** (retornam `build()` puro) — não implementados.

### 4.15 `GizmoSystem<SceneType, TransformType>` — `include/editor/GizmoSystem.h`

Sistema de editor do **gizmo de transformação** (mover/rotacionar/escalar), agnóstico, derivado de `SceneScopeSystem`. Ao contrário dos outros systems, **é dono de uma cena de overlay** (cena + view separadas, composta por cima da cena 3D) — por isso o gizmo nunca é ocluído pela geometria e fica fora do picking da cena principal. A base cuida do ciclo e da matemática (tudo GLM); a implementação concreta (`FilamentGizmoSystem`, [rendering/filament.md §9](rendering/filament.md)) fornece **6 virtuais**: `initializeOverlay()`, `createRoot()`, `createPart(data, materials)`, `updateOverlay(dt)`, `renderOverlay()`, `attachPartToRoot(id)`.

- **Composição**: 9 peças (um `GizmoPart` = `Asset3dData` + materiais, por eixo × modo — enum `GizmoAction` MOVE/ROTATE/SCALE × X/Y/Z), entregues no construtor (`GizmoParts`, move-only). A base cria a cena de overlay e um **root** (nó dono do sistema) no primeiro `onFrameBegin`, enfileira as 9 peças e as prende ao root. Funções livres no namespace: `toString(GizmoAction)`, `operator<<`, `axisOf(GizmoAction)` (eixo de mundo unitário).
- **Ciclo**: `onFrameBegin` (fora do frame GPU) instancia e mede; `postRenderScene` (dentro do frame) desenha o overlay. Instanciar fora do frame é **obrigatório** (o commit das MaterialInstance do Filament acontece no `beginFrame`).
- **Picking dos eixos**: `intersectGizmo(camPos, ray, coneHalfAngle) → optional<GizmoAction>`. Possui um `ObjectSelectorSystem` interno atado à cena de overlay; o id da folha (mesh) atingida é casado **direto** num mapa `id→GizmoAction` (`m_meshToAction`, construído após a instanciação) — sem subir a hierarquia, então é imune à estrutura da árvore.
- **Escala em tela**: `calcGizmoScaleFactor(size)` — altura de mundo para o gizmo ocupar a fração `size` da viewport (escala linear pela distância câmera↔root; câmera injetada por `setCamera`, agnóstica).
- **Drag**: `dragDistanceOnAxis(axis, camPos, ray, objPos)` — deslocamento assinado ao longo do eixo pelo algoritmo do ponto mais próximo entre duas retas (raio × eixo infinito). Retorno **absoluto** (o chamador guarda o valor do mouse-down como grab offset).
- **Input** (2026-08-17): `digestInput(SDL_Event, glm::vec2 pixel)` encapsula o ciclo de arrastar do gizmo, chamado pela `main` dentro do `while(SDL_PollEvent)`. ⚠️ É por causa deste método que `GizmoSystem.h` inclui `<SDL.h>` — o sistema é agnóstico em todo o resto (§1). `getCurrentGizmoAction() → optional<GizmoAction>` diz se um arrasto está em curso; a `main` usa isso para não disparar o picking da cena enquanto o gizmo está ativo.
- **Alvos**: `setOperatingAssets(vector<Asset3dInstance*>)` define sobre quem o gizmo opera — indexados por id em `m_operatingAssets`. Quem chama é o callback de seleção que o `EditorSceneConfigurer` registra no `ObjectSelectorSystem` (§4.12).

**Aplicação da transformação — múltiplos objetos** (2026-09-20). `applyGizmoTransforms` deixou de manipular posição/rotação/escala em separado (onde rotação e escala atingiam só `m_operatingAssets.begin()`) e passou a compor **uma matriz de delta em torno de um pivô**:

```
Mi      = T(pi) · Δ · T(-pi)
world'i = Mi · world0i
```

- `Δ` é o delta do drag: `translate(eixo · distância)`, `mat4_cast(angleAxis(ângulo, eixo))` ou `scale(fator no eixo)` — cada operação só monta `Δ`, e **um único loop** aplica em todos os alvos via `setWorldMatrix` (§3).
- `world0i` é o **snapshot** do mouse-down: `m_initialWorldMatricesOperatingAssets` (`map<int, mat4>`) guarda a world matrix inteira, o que dispensa snapshots separados de rotação e escala.
- `pi` é o pivô, e **é só ele que muda entre os dois modos** — a flag `m_indiviualTransformApply` (`get/setIndiviualTransformApply`, default `false`, ainda sem input ligado): `false` = pivô comum (o centroide da seleção, em `m_groupPivot`), `true` = cada objeto é seu próprio pivô (a translação de `world0i`). No modo comum os objetos **orbitam** o centro na rotação e se afastam/aproximam dele na escala; no individual, cada um gira/incha no lugar.
- O pivô e o snapshot ficam **congelados** até o mouse-up: recalcular o pivô a cada frame realimentaria o drag (o grupo fugiria do cursor).
- Para translação os dois modos coincidem, e não por aproximação: `T(p) · T(d) · T(-p) = T(d)`, o pivô se cancela.
- ⚠️ **A escala virou multiplicativa**: `scale' = scale0 · (1 + d/d0)`, contra a soma a um valor absoluto (`scale0 + d/d0`) de antes. É o que a composição de matrizes impõe, e muda a sensibilidade do arrasto conforme o tamanho do objeto.
- Escrever em matriz é também o que permite **escala não-uniforme sobre objetos rotacionados** sem perda: o resultado pode conter *shear* (ângulos internos deixam de ser retos), que não tem representação em T/R/S e se perderia numa decomposição.
- Sobras: `startingTurningObjectRotation` e `startingSizeObjectScale` continuam preenchidos no mouse-down (a partir do primeiro alvo) mas **não são mais lidos** por ninguém.

### 4.16 `EditorNavigationSystem` — `include/editor/systems/EditorNavigationSystem.h`

Câmera FPS do editor, extraída da `main` em 2026-08-16. É o **único system não-template** e o único que consome input hoje.

- **Duas metades, duas threads**: `digestInputEvent(SDL_Event)` é chamado pela **main thread** dentro do `while(SDL_PollEvent)` e só atualiza flags (`mov_front/back/right/left/up/down`, `left_button_down`) e o yaw/pitch; o movimento de fato acontece no hook **`onFrameBegin`**, na **render thread**, que integra o delta e chama o callback. Os campos são públicos e não têm sincronização — é mais um item da família de races de [threading](../ARCHITECTURE.md#4-modelo-de-threads-visão-transversal).
- **Saída por callback**: o construtor recebe `function<vec3(vec3 center, vec3 offsetCenter, vec3 offsetEye)>`, invocado a cada movimento; a implementação (hoje montada pelo `FilamentEditorSceneConfigurer`) chama `renderer->setCameraState(...)` e devolve a posição resultante. É o que mantém o system sem conhecer renderer nem câmera concreta.
- **Dívidas**: inclui `<SDL.h>` (§1); o `deltaTime` do hook é **ignorado** — o sistema recalcula o seu com `SDL_GetPerformanceCounter` e um `static` local; a orbitação só acontece com o **botão direito** pressionado, apesar do nome `left_button_down`; boa parte do corpo é código comentado da época em que vivia na `main`.

### 4.17 `MathUtils` — `include/core/utils/MathUtils.h`

Utilitários geométricos estáticos, header-only, sem estado — o núcleo matemático que editor e systems compartilham em vez de reimplementar.

- `calcScreenPixelRay<TransformType>(camera, pixel, viewportSize, length) → vec3` — unprojection do pixel: viewport → NDC → clip → mundo por `inverse(proj · inverse(world))`, usando **dois depths** do mesmo pixel para ser imune à convenção de z da projeção (NO/ZO); orienta o resultado para a frente da câmera e escala pelo alcance. A câmera entra pelo contrato agnóstico (`CameraAsset3dInstance`), nunca pela view do renderer.
- `centroid(const vector<vec3>&) → vec3` (2026-09-20) — média aritmética dos pontos (o "median point" do vocabulário de editores, não a mediana estatística). Conjunto **vazio devolve a origem**, sem dividir por zero. Ponto único de cálculo do pivô de um conjunto: usado pelo `ObjectSelectorSystem::getSelectionMedianPoint` (§4.12) e pelo `GizmoSystem` ao congelar `m_groupPivot` no início do drag (§4.15).

## 5. UI abstrata — `include/core/ui/`

A UI do core é totalmente template sobre `UIRendererConcept`, então os widgets abstratos não conhecem CEF nem Filament.

### 5.1 `UIRenderer<R>`

Contrato do renderer de UI. `R` é o **renderer gráfico da engine host** (`RendererType = R`) — a UI final precisa desenhar "dentro" da engine, e esse parâmetro formaliza isso.

| Membro | Papel |
|---|---|
| `start()/stop()` | ciclo de vida (puros) |
| `update()` | por frame, fora do render (ex.: upload de textura) (puro) |
| `render(R*)` | desenha a UI usando o renderer da engine (puro) |
| `sendInputEvent(const InputEvent&)` | entrega input agnóstico (default vazio) |
| `nextElementId()` | gera ids sequenciais de elementos |
| `registerElement(id, UIElementHandler)` / `getElement(id)` | registro id→handler usado para rotear eventos vindos da UI concreta de volta aos objetos C++ |

**Armadilha**: `registerElement` usa `map::emplace` — registrar duas vezes o mesmo id não substitui o handler.

### 5.2 `UIElement<URT>` e o sistema de eventos

Base de todo widget. Estado: `m_uiRenderer` (raw, não-dono), `m_parentId`, `m_currentId` (inicia `EMPTY_ELEMENT_ID = -1`).

- `draw(parentId, line, column, lineSpan, columnSpan) → id` — virtual; a base obtém id do renderer e **se registra** (`registerElement(id, UIElementHandler(this))`). Subclasses concretas sobrescrevem para também materializar o widget na tecnologia de UI (ex.: gerar JSON→JS no CEF) e **devem chamar a base**.
- **Eventos por nome** (stringly-typed): `registerEvent(nome, cb)` guarda `function<void(URT*, int id, string value)>` em um mapa; `invokeEvent(nome, value)` dispara. Nomes usados hoje: `"click"`, `"changeValue"`. O caminho de volta (UI concreta → `invokeEvent`) passa pelo `UIElementHandler`.
- `isFoccused()` puro (sic — typo mantido no código).

**Ownership**: elementos são criados com `new` na main e registrados por ponteiro; hoje **ninguém os deleta** (leak consciente do sandbox).

### 5.3 `UIPanelElement<URT>` — layout em grid

Container com filhos posicionados em grade: cada filho entra como `PanelGridCell{element, line, column, lineSpan, columnSpan}` via `addChildComponent(...)`. O `draw()` da base: chama `drawContainer(...)` (puro — a subclasse cria o container concreto) e então desenha cada filho passando `m_currentId` como `parentId` do filho — é assim que a árvore de UI se forma. O frontend interpreta line/column/spans como CSS grid (ver [ui/cef.md §5](ui/cef.md)).

### 5.4 Widgets abstratos

| Classe | Contrato puro | Lógica na base |
|---|---|---|
| `UITextElement` | `getText()`, `setText(string)` | — |
| `UITextInputElement(label)` | `getText()`, `updateInput(text)` (protegido) | `setText()` = `updateInput` + `notifyChange`; lista `onTextChange` de callbacks |
| `UICheckBoxElement` | `isChecked()`, `setChecked(bool)` | lista `onCheckValueChange` (não usada pela base ainda) |
| `UIComboBoxInputElement` | `addOption(key,label)`, `getSelectedOption()`, `updateInput(key)` (protegido) | `setSelectedOption()` = `updateInput` + `notifyChange`; lista `onSelectValueChange` |
| `UIButtonElement` | — (não-abstrata) | `onClick()` percorre `onClickCallbacks` — mas o fluxo real usa `registerEvent("click", ...)` |
| `UITabElement` | vazio (placeholder) | — |

### 5.5 `UIInstance<URI>` — o "documento" de UI

Representa uma UI montada sobre um renderer. `start()`: chama `uiRenderer->start()`, cria a raiz via **factory method** `createRoot()` (puro — a implementação concreta decide o painel raiz) e chama `root->draw()`. Mantém (parcialmente implementado) um registro `id → UIElement*` (`getElementById`, `registerComponent` — este com bug latente: usa `map::insert(k, v)` em vez de `emplace`, e nada o chama hoje).

### 5.6 `UIElementHandler` — type erasure

Guarda `void* m_elementPtr` + `std::type_index`. O template `invokeEvents<T>(evento, valor)` (definido no fim de `UIElements.h`) faz `static_cast` de volta para `UIElement<T>*` e chama `invokeEvent`. Permite que o `UIRenderer` (que não conhece os tipos dos widgets) roteie eventos vindos da tecnologia de UI para os objetos certos. **Não há verificação do `type_index` no cast** — o chamador é responsável por usar o `T` correto.

## 6. Input — `include/core/input/`

- **`INPUT_KEYS`** (`uint16_t`): letras/números usam o próprio valor ASCII (parse trivial); especiais ≥ 256; mouse ≥ 400 (`MOUSE_LEFT/RIGHT/MIDDLE`); gamepad ≥ 500. `KEY_UNKNOWN = 0`.
- **`INPUT_KEY_STATES`**: `NONE / DOWN / PRESSED / UP`. Convenção atual: ausência da tecla no mapa = "sem mudança neste frame" (o rastreio de modificadores do CEF depende disso).
- **`INPUT_ANALOGS`**: `MOUSE`, `MOUSE_WHEEL`, sticks — valores `glm::vec2`.
- **`InputEvent`**: snapshot por frame — `unordered_map<INPUT_KEYS, INPUT_KEY_STATES> keys` + `unordered_map<INPUT_ANALOGS, vec2> analogs`.
- Conversões livres em `core/src/input/InputEnums.cpp`: `inputKeyToChar` e `inputKeyToVirtualKey` (Windows VK).

**Fluxo atual** (ainda não abstraído) — o laço `while(SDL_PollEvent)` da `main` alimenta **três destinos diferentes**, e só um deles usa o tipo agnóstico:

| Destino | O que recebe |
|---|---|
| `uiRenderer->sendInputEvent()` | `InputEvent` traduzido pela função local `sdlKeyToInputKey` — **o único caminho agnóstico** |
| `navigation->digestInputEvent(ev)` | o `SDL_Event` **cru** (§4.16) |
| `gizmoSystem->digestInput(ev, mousePos)` | o `SDL_Event` **cru** |

Não existe ainda um "InputSystem" do core. O TODO no `Scene::update` indica que o processamento de input deve migrar para o ciclo da cena; o desenho do futuro módulo (evento × nível, actions, thread da bomba de eventos) ainda está em aberto. **O `InputEvent` é recriado a cada frame**, então uma tecla ausente do mapa significa "sem mudança neste frame" — e bordas perdidas no mesmo frame colapsam (é a causa conhecida do drag do gizmo ficar preso).

## 7. Stubs do core (não usar sem consertar)

- `include/core/scene/SceneFactory.h` — **não está mais quebrado** (2026-09): hoje é `template<SceneConcept SceneType> class SceneFactory` com `virtual SceneType* build() = 0`, sintaxe válida. Continua **sem nenhuma implementação** e **sem `#pragma once`**. A `main` já o inclui. Papel: **construir** a cena (complementar ao `SceneConfigurer` do §10, que só ajusta uma cena pronta).
- `include/core/ui/UIEvenetsManager.h` — stub vazio (typo no nome do arquivo/classe `UIEventsManager`).
- `core/src/ui/elements/UIElements.cpp` e `UIElementHandler.cpp` — vazios/1 linha, fora do build.
- `core/src/scene/Scene.cpp` — 3 linhas, praticamente vazio (a Scene é template/header-only); está no `target_sources`.
- `filament/data/assets/IO/FilamentSceneDTOMapper.cpp` — **stub novo e quebrado** (2026-08): `.cpp` com `#pragma once` contendo um template, `typname` em vez de `typename`, corpo de classe sem `;` e uso de `FilamentInstanceFactory` sem include. Fora do build.
- `include/core/data/DTOs/DTO.h` — **órfão** (2026-08): struct `DTO` com `std::type_index m_entityTypeId` e `vector<DTO> children`. **Ninguém a inclui** e nenhum DTO real deriva dela; além disso `std::type_index` não é default-construtível (a struct não teria construtor default) e um `vector` de valores faria slicing — exatamente o que a hierarquia real evitou usando `vector<unique_ptr<...>>`. É um desenho abandonado, não a base dos DTOs.

## 8. Persistência de cena — `include/core/data/assets/IO/` e `include/core/data/DTOs/`

Camada adicionada em 2026-08 (commits `498e468`→`3f20c2f`). A separação de papéis é o ponto central: **três peças, três responsabilidades, nenhuma conhece as outras duas por inteiro**.

```
Scene ──► SceneDTOMapper ──► SceneDTO ──► SceneSerializer ──► arquivo
       (conhece Scene e DTO)         (só DTO e path; NÃO conhece Scene)
```

### 8.1 Os DTOs (`core/data/DTOs/`)

Espelham as entidades, mas **sem template e sem backend** — só dados e GLM:

| DTO | Espelha | Campos próprios |
|---|---|---|
| `SceneDTO` | a cena inteira (raiz do arquivo) | `schemaVersion` (default 1), `id`, `IblDTO{path, intensity}`, `vector<unique_ptr<MaterialDTO>> materials`, `vector<unique_ptr<Asset3dInstanceDTO>> instances` |
| `Asset3dInstanceDTO` | `Asset3dInstance` | `id`, `name`, `localTransform` (**local**, não world — a hierarquia está em `children`), `visible`, `children` |
| `MeshAsset3dInstanceDTO` | `MeshAsset3dInstance` | `positions`, `normals`, `uvs`, `indices`, `boundsMin/Max`, `materialName` |
| `CameraAsset3dInstanceDTO` | `CameraAsset3dInstance` | `eye`, `target` |
| `MaterialDTO` → `MPBRLitMaterialDTO` | `MaterialData` → `MPBRLitMaterialData` | `name`; fatores PBR + 5 `optional<TextureInfoDTO>` |

Decisões registradas nos próprios headers: os filhos entram por **ponteiro** (vetor de valores faria slicing, já que a hierarquia é polimórfica); o **`embeddedData`** de `TextureInfo` ficou **fora** do `TextureInfoDTO` (textura embutida não sobrevive ao round-trip); a **câmera entra em `SceneDTO::instances`**, não em campo próprio, apesar de hoje não ser nó da cena; **luzes ainda não entram** em lugar nenhum.

**Atalho consciente**: a geometria está embutida em cada `MeshAsset3dInstanceDTO`. Quando existir um cache/manager de `Asset3dData`, isso vira uma referência por id e a geometria sai do DTO de instância.

### 8.2 `SceneSerializer` — o facade de formato

```cpp
virtual bool save(const SceneDTO& scene, const std::string& path) = 0;
virtual std::optional<SceneDTO> load(const std::string& path) = 0;
```

**Fala apenas em DTO e path.** Nenhum tipo da lib concreta aparece na interface — nem no header da implementação. Duas implementações:

- **`DummySceneSerializer`** (`core/src/data/assets/IO/`) — verificação: `save()` despeja o DTO inteiro no stdout e devolve `true`; `load()` devolve sempre `std::nullopt`. Serve para conferir o que os mappers produzem, sem tocar disco.
- **`FlatBuffersSceneSerializer`** (`flatbuffers/`) — o schema (`flatbuffers/resources/scene.fbs`), o `scene_generated.h` e o runtime ficam **todos dentro do `.cpp`**. O formato tem `file_identifier` `"LSCN"` e um `schema_version` gravado: conteúdo que não seja nosso, ou de geração que a build não saiba ler, **falha o load** em vez de ser interpretado errado.

### 8.3 Os mappers — Scene ↔ DTO

`SceneDTOMapper<Asset, Transform, Factory, UIRenderer>` (mesmos 4 parâmetros da `Scene`) é um **registry de mappers por tipo**:

- `registerMapper(Asset3dDTOMapper*)` — rejeita (`false`) se já houver mapper com o mesmo `getEntityTypeIndex()` **ou** o mesmo `getDTOTypeIndex()`. Guarda **ponteiro cru, não-dono**;
- `toDto(scene)` — chama o virtual puro **`buildBaseSceneDto(scene)`** (a parte que depende da cena concreta: id, IBL, materiais) e depois converte as raízes vindas de `Scene::getAll()`;
- `instanceToDto(Node*)` — resolve o mapper por **`typeid(*node)`** → `std::type_index` e desce recursivamente pelos `children`. Nó sem mapper registrado vira `nullptr` e é **silenciosamente descartado**;
- `Asset3dDTOMapper` (não-template) é o contrato por tipo de nó: `getEntityTypeIndex()`, `getDTOTypeIndex()`, `nodeToDto(Node*)` e `fromDto(const Asset3dInstanceDTO&) → Node*`.

**Estado real da funcionalidade** (verificado em 2026-09-19):

| Item | Situação |
|---|---|
| Escrita (save) | funciona ponta a ponta com o mapper dummy da `main` |
| Leitura (load) | **não existe na prática** — `fromDto` devolve `nullptr` em todo mapper implementado (sem factory/engine, não há como materializar um nó) |
| `buildBaseSceneDto` | o `DummySceneDTOMapper` da `main` **repete literais** do IBL: a `Scene` não tem id, não conhece o IBL (é do `SceneRenderer`) e não guarda os `MaterialData` após instanciar |
| `materials` do `SceneDTO` | sai **vazio** — sem fonte de dados hoje |
| `uvs` do mesh | sai **vazio** — não há `cpuUvs` na instância Filament |
| Thread | `toDto` chama `Scene::getAll()`, que **não tranca o mutex** (§4.8) |
| Mapper Filament | só existe como stub quebrado (§7) — quem existe de verdade é o `DummySceneDTOMapper` dentro da `main.cpp` |

## 9. `View` — `include/core/view/View.h`

Abstração de janela/superfície, introduzida em 2026-08-31. Contrato mínimo:

```cpp
virtual bool Init() = 0;
virtual glm::vec2 getDimensions() = 0;
```

- **`Init()` não é chamado no construtor da base** — durante a construção da base o tipo dinâmico ainda é `View`, então a chamada seria desvirtualizada para a pura e nunca chegaria ao override. Quem chama é a `main`, com o objeto completo.
- **`getDimensions()` é a fonte da verdade do tamanho**, não os literais passados ao construtor: com `FULLSCREEN_DESKTOP` a janela assume a resolução do desktop e ignora o pedido. Esses valores alimentam o viewport, a aspect da projeção **e** o pixel→ray do picking — se divergirem, o clique erra o alvo.
- Implementação: `SDLFilamentView` (§[filament.md](rendering/filament.md)), dona do `SDL_Init(SDL_INIT_VIDEO)`, da janela e do `getNativeWindow()`.
- **O que a `View` ainda NÃO cobre**: a bomba de eventos. `SDL_PollEvent` continua na `main` — janela e input foram separados, e só a janela ficou atrás da interface.

## 10. `SceneConfigurer<SceneType>` — `include/core/scene/SceneConfigurer.h`

Contrato de **configuração** de cena, introduzido em 2026-09-19. Uma linha:

```cpp
template<SceneConcept SceneType>
class SceneConfigurer { public: virtual SceneType* configure(SceneType* scene) = 0; };
```

A ideia é que "cena de editor" seja uma **configuração aplicada** a uma cena, não um tipo de cena — evitando a herança cruzada (um `EditorScene` teria de derivar da cena concreta do backend, amarrando editor a Filament). Complementar ao `SceneFactory` (§7), que **constrói**; o configurer só **ajusta** o que já existe.

`EditorSceneConfigurer<Scene, OverlayScene, Transform, Asset, Mesh, Camera, UIRenderer>` (em `include/editor/`) é a versão de editor: o `configure()` concreto registra os quatro systems (gizmo, wireframe, seletor, navegação), monta a UI, carrega o asset inicial e posta IBL + luz direcional. Os **factory methods** por peça (`getGizmoSystem`, `getWireframeSystem`, `getObjectSelectorSystem`, `getNavigationSystem`, `createPanel/createText/createTextInput/createButton`, `getUIRenderer`, `getUIInstance`) são puros — quem os implementa é `FilamentEditorSceneConfigurer`.

> ⚠️ **Estado em 2026-09-19 — não funcional**: (a) `configureUI()` referencia seis identificadores que não existem no escopo (sobras do recorte da `main`) e captura locais por `[&]` em lambdas que sobrevivem à função; (b) o header cita `FilamentAsset3dTransform`, furando a camada; (c) a `main` chama `configure()` **depois** de `sceneRenderer.start()`, violando a regra de registro de systems; (d) IBL, caminho do modelo inicial e os 9 FBX do gizmo estão **hardcoded** dentro dos configurers — inclusive paths em `C:/Users/pixqu/Downloads/`. Ver [ARCHITECTURE.md §5](ARCHITECTURE.md), itens 9 a 12.

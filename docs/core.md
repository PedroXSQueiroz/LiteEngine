# LiteEngine — Core (camada agnóstica)

> Parte da [documentação de arquitetura](ARCHITECTURE.md). Implementações concretas: [rendering/filament.md](rendering/filament.md) · [ui/cef.md](ui/cef.md) · [assets/assimp.md](assets/assimp.md).

O core (`include/core/**`, `core/src/**`, mais `include/editor/**`) define **todos os contratos** da engine: interfaces abstratas, templates parametrizados por concepts e tipos de dados agnósticos. Nenhum arquivo do core inclui Filament, CEF, SDL ou Assimp — a única dependência externa é **GLM** (matemática) e a STL.

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

Local: `include/core/concepts/`. Umbrella header: `EngineConcepts.h` (inclui todos). Histórico: foram extraídos de ~9 concepts duplicados espalhados pelo código (plano em `extraindo_concepts.txt`, na raiz).

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

### 2.6 Como os concepts se amarram na prática

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

MaterialData                         # struct PBR (sem hierarquia)
TextureInfo                          # struct de textura (path ou dados embutidos)
```

### 3.2 Transform

```
Asset3dTransform [A]                 # interface pura, 100% GLM
└── FilamentAsset3dTransform (F)     # facade sobre filament::TransformManager
```

### 3.3 Instâncias (GPU, espelham a árvore de dados)

```
Asset3dInstance<Transform> [T]                    # nó instanciado; exporta TransformType
├── MeshAsset3dInstance<Transform> [T]            # + materialName; isMesh()==true; contratos de geometria CPU (getVertex/getIndex/getUVS/boundingBox)
│   └── FilamentMeshAsset3dInstance (F)           # <FilamentAsset3dTransform> + VB/IB/entity
├── CameraAsset3dInstance<Transform> [T][A]       # câmera abstrata (view/proj/lookAt)
│   └── FilamentCameraAsset3dInstance (F)         # <FilamentAsset3dTransform> + filament::Camera
└── FilamentAsset3dInstance (F)                   # nó raiz/intermediário Filament + entity
```

### 3.4 Importação e instanciação

```
Asset3dImporter [A]                               # arquivo → Asset3dData + MaterialData
└── AssimpImporter (As)

Asset3dInstanceFactory<Asset, Transform> [T][A]   # Asset3dData → Asset (GPU); exporta AssetType/TransformType
└── FilamentInstanceFactory (F)
```

### 3.5 Cena e sistemas

```
Scene<Asset, Transform, Factory, UIRenderer> [T]  # dona das instâncias; ciclo de frame
└── FilamentScene (F)                             # + Renderer/Scene/View/SwapChain do Filament
    └── FilamentOverlayScene (F)                  # cena de overlay: NÃO abre frame nem se desenha (gizmo)

SceneRenderer<SceneType> [T][A]                   # facade da render thread (Template Method:
└── FilamentSceneRenderer (F)                     #   setup/renderFrame/cleanup virtuais)

SceneScopeSystem [A]                              # 6 hooks de frame (onFrameBegin..onFrameEnd)
├── WireframeSystem<MeshAsset3dConcept> [T][A]    # overlay wireframe agnóstico (editor)
│   └── FilamentWireframeSystem (F)
├── ObjectSelectorSystem<SceneType, Transform> [T] # picking por raio, 100% GLM (editor)
│   └── FilamentObjectSelectorSystem (F)          # só fixa os parâmetros de template
└── GizmoSystem<SceneType, Transform> [T][A]      # gizmo de transformação (dono de cena de overlay)
    └── FilamentGizmoSystem (F)

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
| `FilamentSceneRenderer` | `SceneRenderer<FilamentScene>` | render thread + `filament::Engine` + SwapChain + ciclo de vida da `FilamentScene` e da câmera | [rendering/filament.md §2](rendering/filament.md) |
| `FilamentAsset3dTransform` | `Asset3dTransform` (é simultaneamente implementação e facade) | `filament::TransformManager` | [rendering/filament.md §4](rendering/filament.md) |
| `FilamentUtils` | — | singleton global do `filament::Engine*` | [rendering/filament.md §8](rendering/filament.md) |
| `CEF_Filament_UIRendererThreaded` | `UIRenderer<filament::Renderer>` | thread CEF, browser offscreen, ponte JS↔C++, textura/quad de composição | [ui/cef.md §3](ui/cef.md) |

A intenção do projeto (ver `create_sceneRenderer.txt`) é que a `main` só converse com facades — nunca com Filament/CEF diretamente. Hoje a `main.cpp` ainda viola isso em alguns pontos (ex.: `getFilamentScene()`, `FilamentUtils::getEngine()` no setup do wireframe).

## 4. Classes do core em detalhe

### 4.1 `Asset3dTransform` — `include/core/data/assets/Asset3dTransform.h`

Interface pura de transform, 100% GLM. Contrato:

| Método | Assinatura | Obrigatório |
|---|---|---|
| Posição | `setPosition(vec3, bool isWorldSpace=false)` / `getPosition(bool isWorldSpace=false)` | puro |
| Rotação | `setRotation(quat, bool isWorldSpace=false)` / `getRotation(bool isWorldSpace=false)` | puro |
| Escala | `setScale(vec3, bool isWorldSpace=false)` / `getScale(bool isWorldSpace=false)` | puro |
| Euler | `setEulerAngles(vec3 graus, bool isWorldSpace=false)` / `getEulerAngles(bool isWorldSpace=false)` | virtual com default (converte via quat) — impl. em `core/src/data/assets/Asset3dTransform.cpp` |
| Matrizes | `setLocalMatrix(mat4)` / `getLocalMatrix()` / `getWorldMatrix()` | puro |

O flag `isWorldSpace` (default `false` = local, adicionado em 2026-07-25) permite ler/escrever em espaço de mundo; a impl Filament ainda tem essa via **em implementação (WIP)** — ver [rendering/filament.md §4.3](rendering/filament.md). `setWorldMatrix` existe **só na impl concreta** `FilamentAsset3dTransform` (ainda não no contrato base).

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
- hierarquia idêntica à de `Asset3dData`: `parent` raw não-dono, `children` de `unique_ptr` — **`addChild(ptr)` toma posse do ponteiro cru** passado (atenção: quem chama não pode deletar);
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
                    std::vector<MaterialData>& materials) = 0;
virtual bool canImport(const std::string& extension) const = 0;
virtual std::vector<std::string> getSupportedExtensions() const = 0;
```

Desenhada para múltiplos importers coexistirem (dispatch por extensão via `canImport`). Implementação atual: [`AssimpImporter`](assets/assimp.md).

### 4.7 `Asset3dInstanceFactory<Asset, Transform>` — `include/core/assets/instanceFactory/Asset3dInstanceFactory.h`

Interface da fábrica de instâncias GPU. Exporta `AssetType`/`TransformType` (exigidos pelo concept). Contrato:

```cpp
virtual std::unique_ptr<Asset> instantiateAsset(
    const Asset3dData& rootNode, Transform transform,
    const std::vector<MaterialData>& materials) = 0;
virtual bool destroyAsset(Asset* instance) = 0;
```

**Semântica de `destroyAsset` (importante)**: o contrato permite destruição adiada — a implementação Filament apenas *marca* e enfileira; a liberação GPU real acontece pós-frame (ver [rendering/filament.md §5](rendering/filament.md)). Consumidores não devem assumir que o recurso morreu ao retornar `true`.

### 4.8 `Scene<Asset, Transform, Factory, UIRenderer>` — `include/core/scene/Scene.h`

O coração do core. Parametrizada pelos 4 concepts. **Possui** (unique_ptr) o factory e o uiRenderer, recebidos no construtor.

**Estado**:
- `m_3dInstances : map<int, unique_ptr<Asset>>` — instâncias vivas, chaveadas por id crescente (`m_lastId`);
- `m_creatingObjects : vector<CreationEntry>` — fila de criação (`{id, clone dos dados, materiais, transform}`);
- `m_instancesMutex` + `m_instantiatedCV` — protegem ambos e acordam quem espera em `get()`;
- `m_systems : vector<SceneScopeSystem*>` (não-dono, sem lock) — **único mecanismo de extensão do frame** (§4.10); `addSystem`/`removeSystem` antes do `start()` ou via `postCommand`.

**API thread-safe de assets**:
- `create(data, materials, transform, deepIds=false) → int` — clona `data`, enfileira, retorna id **imediatamente** (a instanciação real acontece na render thread, dentro de `update`). O id retornado é estampado na raiz da instância (`getId()`) no `instantiate()`; com `deepIds=true`, **todos** os nós da árvore recebem ids do mesmo espaço de numeração (`m_lastId`, sob o mesmo mutex), em ordem determinística de percurso — base para endereçar subobjetos (usado pelo picking do `ObjectSelectorSystem` e pelo gizmo). **Nota**: o índice definitivo de subobjetos por id segue adiado (dupla posse e tipo impedem filhos em `m_3dInstances`; ver memória do projeto) — o paliativo é o `getNode` abaixo;
- `get(id) → Asset*` — empréstimo (dono continua sendo a Scene). Se o id ainda está na fila, **bloqueia** na CV até ser instanciado; se não existe, `nullptr`;
- `getNode(id) → Asset3dInstance<T>*` — busca por id em **toda a hierarquia** (raízes e filhos com `deepIds`). Retorna o tipo **base** dos nós (filhos, ex.: meshes, não são `Asset`). Fast-path no mapa + DFS linear O(nós) — paliativo até o índice. Ao contrário de `get()`, **não bloqueia**: ids de filhos só nascem no `instantiate()`, então id desconhecido → `nullptr`;
- `find(predicate) → vector<Asset*>` — filtro sobre as instâncias vivas;
- `destroy(id)` / `destroy(unique_ptr)` — delega a `factory->destroyAsset()`. O `erase` do mapa está **comentado** (deleção adiada; ver dívidas em [ARCHITECTURE.md §5](ARCHITECTURE.md));
- `addSystem(SceneScopeSystem*)` / `removeSystem(SceneScopeSystem*)`.

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

### 4.9 `SceneRenderer<SceneType>` — `include/core/scene/SceneRenderer.h`

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

**API pública virtual** (assinaturas agnósticas — GLM/string; implementações devem postar à fila): `setIBL(path, intensity)`, `addDirectionalLight(color, intensity, dir, shadows)`, `resize(w, h)`.

**Contrato de threading com a filha** (comentário `THREADING` no header — consequência de misturar herança virtual com thread própria):
1. A base **não** inicia a thread no construtor (a thread despacharia virtuais de um objeto em construção → *pure virtual call*). A filha chama o protegido `launchRenderThread()` como **última instrução do próprio construtor**.
2. A filha chama `stop()` no **próprio destrutor** (o join precisa completar antes da parte derivada ser destruída; o `stop()` do destrutor da base é só cinto de segurança).
3. `setup()`, `renderFrame()` e `cleanup()` executam **na render thread** — é aí que recursos GPU podem ser criados/destruídos.

O parâmetro `SceneType` resolve o problema de `Scene` ser template sem base comum: `getScene()` fica tipado sem o core conhecer o renderer. (Evolução futura: exportar aliases na `Scene` e constrainar com um `SceneConcept`, no idioma dos demais concepts.)

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

Registrado por ponteiro **não-dono** via `Scene::addSystem`; removido via `removeSystem`. **Regras** (documentadas no header): hooks executam na render thread; registro/remoção antes do `start()` ou via `postCommand`; hook nunca chama `Scene::get()` de id ainda na fila (deadlock — `instantiate()` roda na mesma thread). Exemplo completo de uso das fases: `FilamentWireframeSystem` ([rendering/filament.md §8](rendering/filament.md)).

### 4.11 `WireframeSystem<MeshType>` — `include/editor/WireframeSystem.h`

Sistema de editor agnóstico (overlay de wireframe), parametrizado por `MeshAsset3dConcept`. Deriva de `SceneScopeSystem` e liga `preRenderScene → update()`. Contrato puro: `initialize(w,h)`, `resize(w,h)`, `setWireframeColor(vec4)`, `setWireframeWidth(float)`, `addWireframeMesh(MeshType*)`, `removeWireframeMesh`, `clearWireframeMeshes`, `update()`. Estado protegido na base: cor, largura, dimensões e `unordered_set<MeshType*>` dos meshes rastreados. Implementação: [FilamentWireframeSystem](rendering/filament.md).

### 4.12 `ObjectSelectorSystem<SceneType, TransformType>` — `include/editor/ObjectSelectorSystem.h`

Sistema de editor agnóstico de **picking por raio**, no mesmo molde do `WireframeSystem` (deriva de `SceneScopeSystem`, liga-se à cena via `attachTo`), **header-only** — toda a lógica é GLM sobre os contratos do core: meshes são detectados por `isMesh()` + `dynamic_cast` para a classe **pai** `MeshAsset3dInstance<TransformType>` (cujos virtuais `getVertex()/getIndex()/getBoundingBox()` fornecem a geometria), então qualquer subclasse de mesh é selecionável sem reinstanciar o template. Não implementa nenhum hook hoje (serviço passivo de consulta); derivar de `SceneScopeSystem` o mantém plugável no ciclo do frame. `SceneType` segue o idioma do `SceneRenderer<SceneType>` (Scene é template sem base comum); `TransformType` existe porque `Asset3dInstance` também é template — quando a `Scene` exportar aliases (§4.9), pode colapsar para um parâmetro só. A subclasse `FilamentObjectSelectorSystem` (`include/filament/editor/FilamentObjectSelectorSystem.h`, header-only) apenas fixa `<FilamentScene, FilamentAsset3dTransform>` — não adiciona lógica nem toca recursos Filament.

API: `setCamera(CameraAsset3dInstance*)` (nó de cena da câmera — posição e view vêm da world matrix do nó, **nunca** da view do renderer; só a projeção usa o contrato agnóstico `getProjectionMatrix()`); `getCameraPosition()`; `getCameraRay(pixel, viewportSize, length)` (unprojection do pixel clicado — inverte viewport → NDC → clip → mundo via `inverse(proj * inverse(world))`, robusta às convenções de depth NO/ZO, direção escalada pelo alcance); e **duas sobrecargas de `intersect`**:
- `intersect(origin, ray, coneHalfAngle)` — **broad phase por cone**: percorre as raízes vivas da cena (`Scene::find`, filtrando `isDeleted()`) e mantém só objetos cuja esfera envolvente (agregada dos meshes descendentes) cai dentro do cone de meio-ângulo `coneHalfAngle` em torno da direção do raio e ao alcance do segmento. O `coneHalfAngle` é calculado pelo chamador a partir do FOV da câmera (`CameraAsset3dInstance::getFieldOfViewInRadians()`).
- `intersect(origin, ray, roots)` — pública, recebe um **conjunto de nós já escolhido** (sem broad phase); absorve toda a lógica posterior ao `find()`. Usada pelo gizmo, que a chama com as peças do overlay antes de cair no picking da cena principal (ver [rendering/filament.md §9](rendering/filament.md)).

Ambas fazem, por mesh: leva o segmento ao espaço local (direção não normalizada → t∈[0,1] válido mesmo com escala não uniforme), broad phase segmento×AABB (slab test sobre `getBoundingBox()`) e narrow phase **Möller–Trumbore** por triângulo; varrem TODOS os candidatos e retornam o `getId()` do mesh de **menor t** (mais próximo da origem), ou −1.

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

**Fluxo atual** (ainda não abstraído): `main.cpp` traduz eventos SDL → `InputEvent` (função local `sdlKeyToInputKey`) e entrega a `uiRenderer->sendInputEvent()`. Não existe ainda um "InputSystem" do core; o TODO no `Scene::update` indica que o processamento de input deve migrar para o ciclo da cena.

## 7. Stubs do core (não usar sem consertar)

- `include/core/scene/SceneFactory.h` — rascunho com sintaxe inválida; a ideia (montar cenas sem conhecer tipos concretos) está no roadmap.
- `include/core/ui/UIEvenetsManager.h` — stub vazio (typo no nome do arquivo/classe `UIEventsManager`).
- `core/src/ui/elements/UIElements.cpp` e `UIElementHandler.cpp` — vazios/1 linha, fora do build.
- `core/src/scene/Scene.cpp` — 3 linhas, praticamente vazio (a Scene é template/header-only); está no `target_sources`.

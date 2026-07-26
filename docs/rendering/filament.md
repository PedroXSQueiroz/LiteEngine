# LiteEngine — Módulo de Renderização: Filament

> Parte da [documentação de arquitetura](../ARCHITECTURE.md). Interfaces do core: [core.md](../core.md).

Implementação concreta de renderização usando **Google Filament** (backend Vulkan com fallback OpenGL). Código em `filament/**` (fontes) e `include/filament/**` (headers). Tudo no namespace `lite`, exceto `FilamentScene` e `FilamentUtils` (namespace global).

## 1. Mapa de implementação — interface do core → classe Filament

Esta tabela é o contrato do módulo. Toda classe daqui ou **implementa uma interface do core** ou é **facade/amarração** sem contraparte abstrata:

| Interface / template do core | Implementação Filament | Concept satisfeito | Arquivos |
|---|---|---|---|
| `lite::Asset3dTransform` | `lite::FilamentAsset3dTransform` | `TransformConcept` | `include/filament/data/assets/FilamentAsset3dTransform.h` + `filament/data/assets/FilamentAsset3dTransform.cpp` |
| `lite::Asset3dInstance<T>` | `lite::FilamentAsset3dInstance` | `Asset3dConcept` | `include/filament/data/assets/FilamentAsset3dInstance.h` + `filament/assets/instanceFactory/FilamentAsset3dInstance.cpp` |
| `lite::MeshAsset3dInstance<T>` | `lite::FilamentMeshAsset3dInstance` | `MeshAsset3dConcept` | `include/filament/data/assets/FilamentMeshAsset3dInstance.h` + `filament/assets/instanceFactory/FilamentMeshAsset3dInstance.cpp` |
| `lite::CameraAsset3dInstance<T>` | `lite::FilamentCameraAsset3dInstance` | (via `Asset3dConcept`) | `include/filament/data/assets/FilamentCameraAsset3dInstance.h` + `filament/data/assets/FilamentCameraAsset3dInstance.cpp` |
| `lite::Asset3dInstanceFactory<A,T>` | `lite::FilamentInstanceFactory` | `Asset3dInstanceFactoryConcept` | `include/filament/assets/instanceFactory/FilamentInstanceFactory.h` + `filament/assets/instanceFactory/FilamentInstanceFactory.cpp` |
| `lite::Scene<A,T,F,U>` | `FilamentScene` (classe de **amarração**) | — | `include/filament/scene/FilamentScene.h` (header-only) |
| `FilamentScene` (cena de overlay) | `FilamentOverlayScene` (não abre frame nem se desenha) | — | `include/filament/scene/FilamentOverlayScene.h` (header-only) |
| `lite::IBL` | `lite::FilamentIBL` | — | `include/filament/lightning/FilamentIBL.h` + `filament/lightning/FilamentIBL.cpp` |
| `lite::WireframeSystem<M>` (→ `SceneScopeSystem`) | `lite::FilamentWireframeSystem` | consome `MeshAsset3dConcept` | `include/filament/editor/FilamentWireframeSystem.h` + `filament/editor/FilamentWireframeSystem.cpp` |
| `lite::ObjectSelectorSystem<S,T>` (→ `SceneScopeSystem`) | `lite::FilamentObjectSelectorSystem` (só fixa `<FilamentScene, FilamentAsset3dTransform>`) | — | `include/filament/editor/FilamentObjectSelectorSystem.h` (header-only) |
| `lite::GizmoSystem<S,T>` (→ `SceneScopeSystem`) | `lite::FilamentGizmoSystem` (fixa `<FilamentOverlayScene, FilamentAsset3dTransform>`) | — | `include/filament/editor/FilamentGizmoSystem.h` + `filament/editor/FilamentGizmoSystem.cpp` |
| `lite::TransformUtils<T>::build()` (especialização) | `TransformUtils<FilamentAsset3dTransform>::build()` | — | `filament/utils/FilamentTransformUtils.cpp` |
| `lite::SceneRenderer<SceneType>` | `lite::FilamentSceneRenderer` (= `SceneRenderer<FilamentScene>`) | — | `include/filament/scene/FilamentSceneRenderer.h` + `filament/scene/FilamentSceneRenderer.cpp` |
| — (utilitário global) | `FilamentUtils` | — | `include/filament/utils/FilamentUtils.h` + `filament/utils/FilamentUtils.cpp` |

Em todo o módulo, a fronteira de tipos é: **GLM na assinatura (lado core), tipos `filament::math` só internamente** — conversões `toFilament()` privadas no factory.

## 2. `FilamentSceneRenderer` — implementa `lite::SceneRenderer<FilamentScene>`

É a implementação Filament do facade abstrato **`lite::SceneRenderer<SceneType>`** ([core.md §4.9](../core.md)), que esconde da `main` tudo do Filament: engine, swapchain, render thread, câmera e o ciclo de vida da `FilamentScene`. Motivação e alternativas descartadas documentadas em `create_sceneRenderer.txt` (opção **B2** implementada: render loop independente, sem sync por frame).

**Divisão de responsabilidades com a base**: a base (core) fornece a render thread, a fila de comandos, o handshake `waitReady/start/stop`, o estado pendente de câmera e o **esqueleto do loop**; esta classe fornece apenas as fases virtuais — todas executando na render thread:

| Hook da base | Implementação Filament |
|---|---|
| `setup() → bool` | engine (VULKAN→OPENGL fallback) + `FilamentUtils::setEngine`; swapchain; `FilamentScene` (injetando factory + UI renderer); **`uiRenderer->createFilamentResources()`** (recursos GPU da UI, na thread do Engine); câmera; view (projeção 45°, near 0.1, far 2000). `false` em falha → base pula direto para `cleanup()` |
| `renderFrame(dt)` | `takePendingCamera` → aplica na `FilamentCameraAsset3dInstance`; `m_scene->update(dt)` (o ciclo de frame da Scene) |
| `cleanup()` | `uiRenderer->stop()`, destrói scene/câmera/luz/renderer/view/scene-Filament/swapchain e o engine; zera `FilamentUtils`. Tolerante a setup parcial (guards de nullptr) |

**Por que a thread affinity manda no desenho**: o Filament exige que toda chamada ao `filament::Engine` venha da thread que o criou. Logo o engine é criado *dentro* de `setup()` (render thread), e todo trabalho GPU externo entra via fila de comandos da base.

**Contrato de construção/destruição** (imposto pela base, ver comentário `THREADING` em `SceneRenderer.h`): o construtor desta classe chama `launchRenderThread()` como **última instrução** (virtuais só podem ser despachados com o objeto completo) e o destrutor chama `stop()` (o join precisa completar antes da parte derivada ser destruída).

### Ciclo de vida

```cpp
FilamentSceneRenderer renderer(nativeWindowHandle, w, h); // spawna a render thread
renderer.waitReady();      // bloqueia até engine/scene/camera existirem (promise/future)
// ... setup via postCommand/setIBL/addDirectionalLight; addSystem na Scene ...
renderer.start();          // libera o loop de frames (atomic m_started)
// ... aplicação roda ...
// ~FilamentSceneRenderer() → stop(): m_running=false, join; cleanup GPU roda NA render thread
```

Nota: é em `setup()` que o módulo de UI é instanciado (a `FilamentScene` recebe o `CEF_Filament_UIRendererThreaded` — ver [ui/cef.md](../ui/cef.md)); o `start()` da UI, chamado pela main, não toca GPU ([ui/cef.md §3](../ui/cef.md)).

### API pública (todas thread-safe)

| Método | Origem | Mecanismo |
|---|---|---|
| `waitReady()` / `start()` / `stop()` | herdado da base | promise/future + atomics |
| `getScene() → FilamentScene*` | herdado da base | ponteiro estável após `waitReady()` (nulo se setup falhou) |
| `setCameraState(eye, target)` | herdado da base | struct pendente + mutex; consumida 1×/frame via `takePendingCamera` (última escrita vence) |
| `postCommand(std::function<void()>)` | herdado da base | fila + mutex; drenada em batch a cada frame — **é o jeito canônico de rodar qualquer coisa que toque GPU** |
| `getCurrentCamera() → FilamentCameraAsset3dInstance*` | daqui | ponteiro estável após `waitReady()` (câmera criada no `setup()`). **Caveat**: os dados por trás (TransformManager/`filament::Camera`) são escritos pela render thread a cada frame — leituras de outra thread (ex.: picking na main) são race conhecido; o refino planejado é rotear o uso via `postCommand` |
| `setIBL(path, intensity)` | override daqui | vira comando: cria `FilamentIBL` e ajusta intensidade |
| `addDirectionalLight(color, intensity, dir, shadows)` | override daqui | vira comando: `LightManager::Builder(SUN)` |
| `resize(w, h)` | override daqui | vira comando: viewport + reprojeção da câmera |

## 3. `FilamentScene` — amarração `Scene` + Filament

`include/filament/scene/FilamentScene.h` (header-only). É a **instanciação concreta única** do template `Scene` do core:

```cpp
class FilamentScene : public lite::Scene<
    lite::FilamentAsset3dInstance,          // Asset3dConcept
    lite::FilamentAsset3dTransform,         // TransformConcept
    lite::FilamentInstanceFactory,          // Asset3dInstanceFactoryConcept
    lite::CEF_Filament_UIRendererThreaded>  // UIRendererConcept
```

Guarda ponteiros crus (não-donos — o dono é o `FilamentSceneRenderer`) para `filament::Renderer/Scene/View/SwapChain` e implementa os **3 hooks virtuais do ciclo de frame** da `Scene`:

| Hook do core | Implementação |
|---|---|
| `prepareRender()` | `renderer->beginFrame(swapChain)` (retorno `false` pula o frame — comportamento previsto pelo core) |
| `renderScene()` | `renderer->render(view)` (a view 3D) |
| `renderUI()` | `getCurrentUI()->render(m_filamentRenderer)` — compõe a UI por cima, dentro do frame aberto |
| `finishRender()` | `renderer->endFrame()` **e** `m_asset3dFactory->flushDeletedFilament3dAssets()` — a deleção GPU adiada acontece aqui, pós-frame, na render thread |

Acessores Filament-specific (`getFilamentRenderer/Scene/View`) existem para os pontos ainda não abstraídos (ex.: construção do wireframe na main). Nota: este header inclui `CEF_Filament_UIRendererThreaded.h`, criando a única dependência rendering→UI do módulo — é a classe de amarração, não o padrão a seguir.

## 4. `FilamentAsset3dTransform` — implementa `Asset3dTransform`

**Facade puro sobre `filament::TransformManager`** (comentário no código). Não guarda estado de transform próprio: cada get/set lê/escreve a matriz local da entity no TransformManager, decompondo/recompondo position/rotation/scale (método privado `modifyComponent`).

- Construtor: `(TransformManager&, optional<Entity>)` — pode nascer **sem entity**; `of(entity)` liga depois (padrão usado pelo factory: o transform raiz é construído pela main via `TransformUtils::build()` sem entity, e o factory chama `of(rootEntity)` na instanciação).
- `getWorldMatrix()` percorre os pais no TransformManager.
- **Armadilha**: qualquer operação sem entity ligada lança `const char*` (`assertEntity`) — não é `std::exception`.

Contrato implementado (tudo de `Asset3dTransform`): `setPosition/getPosition`, `setRotation/getRotation` (quat), `setScale/getScale`, `setLocalMatrix/getLocalMatrix/getWorldMatrix` (herda os Euler default da base).

### 4.1 Propagação: mexer num nó move a subárvore inteira

Este é o comportamento mais importante do transform e **não está implementado em nenhuma linha de código `lite`** — é herdado do `filament::TransformManager`, que mantém a hierarquia de entities e calcula o world transform subindo a cadeia de pais.

**Como a hierarquia nasce.** O `FilamentInstanceFactory` cria cada entity já apontando para o pai:

```cpp
// raiz do asset: sem pai
transformManager.create(rootEntity);
// cada nó filho (mesh ou intermediário): com o instance do pai + transform local
auto parentTransformInstance = transformManager.getInstance(parentEntity);
transformManager.create(meshEntity, parentTransformInstance, toFilament(mesh.localTransform));
```

Ou seja, a árvore de `Asset3dInstance` (lado `lite`) e a árvore de transforms (lado Filament) são **espelhadas**, montadas no mesmo percurso do `processNode`. Fora do factory, o mesmo efeito se obtém com `TransformManager::setParent(childInstance, parentInstance)` — é assim que o `GizmoSystem` prende as 9 peças ao root do gizmo, já que cada peça é uma raiz separada na cena de overlay.

**O que acontece ao escrever.** `setPosition/setRotation/setScale/setLocalMatrix` escrevem **só a matriz local daquele nó** (`TransformManager::setTransform`). Nenhum filho é tocado, nenhum valor é recalculado na hora. A propagação é *lazy* e acontece na leitura: `getWorldMatrix()` chama `TransformManager::getWorldTransform(instance)`, que compõe a cadeia `raiz → … → nó`. Consequências práticas:

- mover a raiz de um asset **move todos os meshes descendentes** — inclusive os que já foram instanciados, sem nenhum trabalho extra;
- mover um nó intermediário move só a subárvore dele; os irmãos ficam parados;
- a escala e a rotação também compõem — escalar a raiz escala as posições dos filhos (é o mecanismo que o gizmo usará para escala constante em tela, mexendo só no root);
- não há evento/callback de "meu pai mudou": quem depende de world (picking via `getWorldMatrix`, bounds agregados, wireframe) simplesmente lê de novo no frame seguinte e enxerga o valor novo;
- **quem cacheia world precisa invalidar por conta própria.** O `FilamentWireframeSystem` copia o world transform do mesh para a entity do wireframe a cada frame (`update()` no hook `preRenderScene`) exatamente por isso.

**Bounding boxes não são afetados**: `getBoundingBox()` do mesh é em **espaço local** (cache lazy sobre `cpuPositions`), então mover/rotacionar/escalar não invalida nada — o picking transforma o raio para o espaço local do mesh usando `inverse(getWorldMatrix())` a cada consulta.

### 4.2 Armadilhas de `modifyComponent` (decompose/recompose)

`setPosition`, `setRotation` e `setScale` não escrevem "só aquele componente": os três passam por `modifyComponent`, que **lê a matriz local, decompõe, troca o componente pedido e recompõe** — e **só grava se o decompose suceder**:

```cpp
if (glm::decompose(current, scale, rotation, position, skew, perspective)) {
    if (newPosition) position = *newPosition;   // (idem rotation/scale)
    glm::mat4 result = glm::translate(I, position) * glm::mat4_cast(rotation) * glm::scale(I, scale);
    setLocalMatrix(result);
}   // decompose falha (matriz singular) → não escreve nada, mantém o transform anterior
```

O guard `if (glm::decompose(...))` foi adicionado depois de um bug real: sem ele, quando o decompose **falhava** (matriz (quase) singular — ex.: escala ~0), `position`/`rotation` ficavam com **lixo de stack (`0xCCCCCCCC`)** e eram gravados, corrompendo o transform (o gizmo, ao aplicar escalas pequenas, entrava em cascata até o Filament abortar). **Atenção**: os getters `getScale()`/`getRotation()` ainda ignoram o retorno do decompose (mesmo risco, não corrigido).

Duas consequências que persistem mesmo quando o decompose sucede:

- **Perda de informação**: `skew` e `perspective` saem da decomposição e **são descartados** na recomposição. Uma matriz importada com shear (cisalhamento) perde o shear no primeiro `setPosition` — mesmo que a intenção fosse só transladar.
- **Ordem imposta**: o resultado é sempre `T * R * S`. Se a matriz original tinha outra ordem de composição (ex.: escala aplicada depois da rotação), ela é normalizada silenciosamente para essa forma.

Quem precisa preservar a matriz exata (ou evitar o decompose de vez) deve usar `setLocalMatrix()` com a matriz montada pelo chamador, que escreve direto no TransformManager. É o que o gizmo faz para a escala do seu root (`setLocalMatrix(glm::scale(I, escala))`), fugindo do decompose por completo.

### 4.3 Local × world: só a raiz pode tratar os dois como iguais

Todos os setters do contrato são **locais** (relativos ao pai). O `getPosition()` devolve `m[3]` da **matriz local**, e o único acesso a world é o `getWorldMatrix()` — **não existe setter em world**.

- **Raiz de asset** (criada com `transformManager.create(rootEntity)`, sem pai): local == world. Escrever `setPosition(p)` põe o nó em `p` no mundo.
- **Sub-nó** (mesh ou intermediário, que é o que o picking devolve quando o asset foi criado com `deepIds`): local ≠ world. Para aplicar um deslocamento `delta` calculado em world é preciso levá-lo para o espaço do pai antes:

```cpp
const glm::mat4 parentWorld = node->parent->getWorldMatrix();
const glm::vec3 deltaLocal  = glm::vec3(glm::inverse(parentWorld) * glm::vec4(delta, 0.0f));
node->getTransform()->setPosition(node->getTransform()->getPosition() + deltaLocal);
```

(`vec4(delta, 0)` porque é direção, não ponto.) Não há helper para isso hoje.

## 5. Instâncias: `FilamentAsset3dInstance` / `FilamentMeshAsset3dInstance` / `FilamentCameraAsset3dInstance`

### `FilamentAsset3dInstance` — implementa `Asset3dInstance<FilamentAsset3dTransform>`
Nó raiz ou intermediário. Além do contrato do core:
- `getEntity()` — a `utils::Entity` do nó (base da hierarquia de transform no TransformManager);
- `materialInstances : vector<MaterialInstance*>` — instâncias de material **compartilhadas pela árvore**; a raiz é dona (destruídas pelo factory no flush). *TODO no código: mover para um serviço de Materiais*;
- `textures : vector<Texture*>` — referências ao cache do factory (**não-dono**). *TODO: serviço de Assets*;
- `markAsDeleted()` / `isDeleted()` — flag do fluxo de deleção adiada (consultada por `Scene::find` para limpezas pós-deleção, ex.: wireframes);
- `setVisible(bool)` override — mostra/esconde os renderables da subárvore.

### `FilamentMeshAsset3dInstance` — implementa `MeshAsset3dInstance<FilamentAsset3dTransform>`
Recursos GPU de um mesh: `vertexBuffer`, `indexBuffer`, `materialInstance` (aponta para os compartilhados da raiz), entity própria + `initializeTransform(entity)` (liga o transform wrapper após criar o renderable). **Mantém cópia CPU da geometria** (`cpuPositions/cpuNormals/cpuIndices`, bounds) consumida pelo `FilamentWireframeSystem` e pelos **contratos de geometria da base** (`getVertex/getIndex/getBoundingBox` — picking do `ObjectSelectorSystem`). Overrides de geometria: `getVertex()` devolve `cpuPositions`; `getIndex()` converte `cpuIndices` (uint32→int64); `getUVS()` retorna **vazio** (não há `cpuUvs` — decisão pendente); bounding box com **cache lazy** (`m_boundsSet`: `getBoundingBox()` chama `calcBoundingBox()` sobre `cpuPositions` na 1ª chamada; `setBoundingBox()` sobrescreve e marca a flag). Expõe `getEngine()/getScene()` para sistemas que precisam operar sobre ele.

### `FilamentCameraAsset3dInstance` — implementa `CameraAsset3dInstance<FilamentAsset3dTransform>`
Envolve `filament::Camera` + entity própria. Implementa o contrato puro do core: `getViewMatrix/getProjectionMatrix/lookAt(center)/setProjection(fov, aspect, near, far)`. O eye do `lookAt` vem do transform (setado via `getTransform()->setPosition`, como faz o `FilamentSceneRenderer` ao aplicar `setCameraState`).

## 6. `FilamentInstanceFactory` — implementa `Asset3dInstanceFactory<FilamentAsset3dInstance, FilamentAsset3dTransform>`

O coração da instanciação GPU. Tipos associados exigidos pelo concept: `AssetType = FilamentAsset3dInstance`, `TransformType = FilamentAsset3dTransform`.

### `instantiateAsset(rootNode, transform, materials)` — contrato do core
Executa **na render thread** (chamado por `Scene::instantiate()` dentro de `update`):
1. Cria entity raiz + transform no TransformManager; `transform.of(rootEntity)`.
2. Cria `MaterialInstance` para cada `MaterialData` (mapa **nome → instância**; a raiz guarda todos em `materialInstances`).
3. `processNode` recursivo sobre a árvore de `Asset3dData`:
   - **Nó mesh** (`isMesh()`): cria `FilamentMeshAsset3dInstance` filho; copia dados CPU; cria `VertexBuffer` (atributos POSITION/TANGENTS/UV0, dados copiados para vetores heap liberados no callback do BufferDescriptor) e `IndexBuffer` (UINT32); resolve material **por nome** com fallback (primeiro material da raiz → instância nova do material base); constrói o renderable (`culling(false)`, `castShadows(false)`, `receiveShadows(true)`); cria transform **com parent** no TransformManager; adiciona a entity à `filament::Scene`.
   - **Nó vazio com filhos/transform**: cria `FilamentAsset3dInstance` intermediário (entity + transform hierárquico).
   - **Nó vazio sem transform**: é **achatado** — filhos processados direto no pai (a árvore de instâncias pode ser mais rasa que a de dados).
4. Material base: `lit.filamat` carregado no construtor (**path hardcoded** `D:/Workspace/LiteEngine/core/resources/filament/materials/lit.filamat`). Parâmetros setados por material: `baseColorFactor/metallicFactor/roughnessFactor/emissiveFactor` + samplers `baseColorMap/normalMap/metallicRoughnessMap/occlusionMap/emissiveMap`.
5. Texturas: cache por path (`m_textureCache`), decodificadas com `image::ImageDecoder` do Filament (sRGB ou linear conforme `TextureInfo::sRGB`). O cache é do factory e vive até `cleanup()` (destrutor).

### `destroyAsset(instance)` — contrato do core (deleção em DUAS fases)
**Fase 1 (qualquer thread, via `Scene::destroy`)**: apenas `markAsDeleted()` + push em `m_deleted3dInstances`. Retorna `true` sem tocar GPU.

**Fase 2 — `flushDeletedFilament3dAssets()`** (método Filament-specific, **fora do contrato do core**): chamado por `FilamentScene::finishRender()` após `endFrame()`, na render thread. Para cada instância enfileirada, `destroyFilament3dAsset`: percorre a árvore bottom-up destruindo (meshes: remove da cena, destrói VB/IB, transform e entity; nós: transform e entity) e por fim destrói os `MaterialInstance` compartilhados da raiz.

**Atenção**: o flush **não** remove a entrada de `Scene::m_3dInstances` (o `erase` na Scene está comentado) — a memória CPU da instância permanece, marcada como deleted. Ver dívidas em [ARCHITECTURE.md §5](../ARCHITECTURE.md).

## 7. `FilamentIBL` — implementa `lite::IBL`

Contrato do core: `bool load(const std::string& path)`. Detecta o formato e delega:
- `loadFromEquirect` — HDR equirectangular;
- `loadFromKtx` — par `<prefix>_ibl.ktx` / `<prefix>_skybox.ktx`;
- `loadFromDirectory` — diretório com faces do cubemap (ex.: os IBLs de sample do Filament, como `lightroom_14b` usado na main).

Cria `IndirectLight` + `Skybox` na `filament::Scene`, com suporte a spherical harmonics (`getSphericalHarmonics`). Getters Filament-specific para uso avançado (`getIndirectLight/getSkybox/getFogTexture`). Destrutor libera texturas/luz/skybox. Instanciado pelo `FilamentSceneRenderer::setIBL` (na render thread, via comando).

## 8. `FilamentWireframeSystem` — implementa `WireframeSystem<FilamentMeshAsset3dInstance>`

Sistema de editor (overlay wireframe). Cadeia de herança completa: `SceneScopeSystem → WireframeSystem<FilamentMeshAsset3dInstance> → FilamentWireframeSystem`. É o **exemplo canônico do lifecycle de sistemas** ([core.md §4.10](../core.md)) — usa três fases:

| Fase | O que faz |
|---|---|
| `onFrameBegin(dt)` | `ensureGpuResources()` (materialização preguiçosa do material — 1ª execução) e **auto-track**: varre os assets vivos da cena (`find(!isDeleted)`) e registra meshes ainda não rastreados; rebuild único por frame |
| `preRenderScene(dt)` | herdado da base: `update()` sincroniza transforms das entidades wireframe com os meshes fonte |
| `onFrameEnd(dt)` | varre assets `isDeleted()` e remove seus wireframes (roda após o flush de deleções GPU — as entidades wireframe têm buffers próprios) |

**Configuração é 100% main-thread** (só dados): construtor, `initialize(w,h)`, `setMaterialPath(path)` (guarda a string; o load GPU acontece no primeiro hook, na render thread), `setWireframeColor/Width` (guardam; aplicados ao material quando ele existir), `attachTo(FilamentScene*)` (liga o sistema à cena que ele observa) e `addSystem` **antes** do `start()`. `loadMaterial(path)` continua público, mas só pode ser chamado na render thread.

**Técnica**: para cada mesh rastreado cria uma **entidade duplicada** (`WireframeEntity`) com vértices expandidos por triângulo — cada triângulo ganha 3 vértices únicos com coordenadas baricêntricas `(1,0,0),(0,1,0),(0,0,1)` no atributo COLOR — e o material desenha só as arestas (distância baricêntrica). Usa a geometria CPU mantida pelo `FilamentMeshAsset3dInstance` (`cpuPositions/cpuIndices`).

**Teardown**: o destrutor destrói recursos GPU ⇒ o `reset()` deve rodar na render thread. Padrão usado pela main no shutdown: `postCommand([&]{ scene->removeSystem(ws.get()); ws.reset(); })` seguido de `sceneRenderer.stop()` — a base drena a fila de comandos após o loop, antes do `cleanup()`, garantindo a execução.

## 9. Gizmo — `FilamentGizmoSystem` + `FilamentOverlayScene`

Implementação Filament do gizmo de transformação ([core.md §4.15](../core.md)). Cadeia: `SceneScopeSystem → GizmoSystem<FilamentOverlayScene, FilamentAsset3dTransform> → FilamentGizmoSystem`.

**Padrão de overlay — o gizmo é dono de uma SEGUNDA cena.** Ao contrário do wireframe (que injeta entidades na cena 3D), o gizmo tem `filament::Scene` + `View` próprias, compostas **por cima** da cena 3D:
- A view do overlay usa a **mesma câmera** da cena 3D (nada a sincronizar por frame), `BlendMode::TRANSLUCENT`, `setPostProcessingEnabled(false)`, `setShadowingEnabled(false)`, `setScreenSpaceRefractionEnabled(false)`.
- Um render pass extra: `renderOverlay()` chama `renderer->render(m_view)` no hook `postRenderScene`, **dentro** do frame já aberto pela cena principal.
- A `FilamentOverlayScene` (subclasse de `FilamentScene`) **neutraliza as fases de frame**: `prepareRender()`/`renderScene()` são no-op e `finishRender()` só faz o flush de deleções — ela NÃO abre `beginFrame`/`endFrame` (o frame é da cena principal) e NÃO se desenha (quem desenha é o sistema). Não recebe `swapChain` nem `UIRenderer`. Seu `update(dt)` roda **fora** do frame (no `onFrameBegin`), porque instanciar dentro do frame quebraria o commit das `MaterialInstance` do Filament (feito no `beginFrame`).

**As 9 peças e o root.** Cada peça é criada pela cena de overlay (`create(..., deepIds=true)`) — a factory própria do overlay aponta para a `filament::Scene` do gizmo, então tudo nasce lá. O `createRoot()` cria um `FilamentMeshAsset3dInstance` **sem geometria** (só para agregar bounds via `getCompleteBoundingBox`), com `setLocalMatrix(identidade)` explícito no `createRoot` (senão o world transform nasce não inicializado). O `attachPartToRoot` liga cada peça ao root em **duas hierarquias**: `TransformManager::setParent` (Filament, propaga transform) **e** `m_root->addChild(part)` (lite, para a agregação de bounds) — este último cria **duplo dono** do ponteiro (`//FIXME` no código, deleção adiada).

**Picking dos eixos.** O `intersectGizmo` da base usa um `ObjectSelectorSystem` interno (atado à cena de overlay); o id da folha (mesh) atingida é casado num mapa `id→GizmoAction` construído após a instanciação — **sem subir a hierarquia**, imune à estrutura da árvore.

**Escala em tela** (`calcGizmoScaleFactor`) escreve a escala do root via `setLocalMatrix(glm::scale(...))` direto, **sem passar pelo `modifyComponent`/decompose** (ver §4.2).

**Teardown**: o destrutor destrói recursos GPU (view, scene, factory do overlay) ⇒ mesmo padrão do wireframe (`postCommand([&]{ removeSystem; reset; })` antes do `stop()`).

## 10. Utilitários

- **`FilamentUtils`** (namespace global) — singleton estático do `filament::Engine*`. Setado pela render thread na criação do engine; usado onde ainda não há injeção de dependência (ex.: setup do wireframe na main). Candidato a remoção quando as facades cobrirem esses casos.
- **`TransformUtils<FilamentAsset3dTransform>::build()`** (`filament/utils/FilamentTransformUtils.cpp`) — especialização exigida pelo core: constrói um `FilamentAsset3dTransform` a partir do TransformManager do engine global (via `FilamentUtils::getEngine()`), **sem entity** (o factory liga depois com `of()`). É o que permite à main escrever `TransformUtils<FilamentAsset3dTransform>::build()` sem tocar no engine.

## 11. Recursos (materiais)

Materiais fonte `.mat` são compilados para `.filamat` com o `matc` do Filament. Local: `core/resources/filament/`:

| Arquivo | Uso |
|---|---|
| `materials/lit.filamat` | material PBR base de todos os assets (carregado pelo factory) |
| `materials/ui_overlay.filamat` | quad de composição da UI (usado pelo módulo CEF) |
| `editor/materials/wireframe.filamat` | overlay de wireframe (baricêntrico) |

## 12. Dívidas específicas do módulo

- Paths hardcoded: `lit.filamat` (ctor do factory), IBL e wireframe (main).
- A `main` ainda instancia `FilamentSceneRenderer` pelo tipo concreto (inevitável em algum ponto de composição), mas poderia programar contra `lite::SceneRenderer<FilamentScene>` no restante.
- `FilamentScene` expõe getters crus do Filament usados pela main/UI — pontos de vazamento da abstração a fechar.
- `instantiateAsset` tem FIXME ("should receive the transform") e código morto comentado (contagem de meshes).
- `FilamentAsset3dTransform` lança `const char*` em vez de exceção tipada.

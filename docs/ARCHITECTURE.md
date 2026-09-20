# LiteEngine — Visão Geral e Índice da Documentação

> Documento de referência para agentes e desenvolvedores. Gerado a partir da análise do código em julho/2026 e **auditado contra o código em 2026-09-19** (branch `abstract_scene`, commit `468dd1d`). Se o código divergir deste documento, o código vence — atualize este arquivo.

## Índice da documentação

| Documento | Conteúdo |
|---|---|
| **este arquivo** | Visão geral, estrutura de diretórios, build, modelo de threads, dívidas técnicas globais, roadmap |
| [core.md](core.md) | A camada agnóstica: **concepts em detalhe**, hierarquias completas de classes, `Scene`, UI abstrata, input, sistemas |
| [rendering/filament.md](rendering/filament.md) | Implementação de renderização com Google Filament — quais interfaces do core cada classe implementa |
| [ui/cef.md](ui/cef.md) | Implementação de UI com CEF + React — quais interfaces do core cada classe implementa |
| [assets/assimp.md](assets/assimp.md) | Implementação de importação de assets com Assimp |

## 1. Visão geral

LiteEngine é uma game engine em **C++20** cujo princípio central é a **modularidade extrema**: um `core/` totalmente agnóstico (sem dependência de nenhuma biblioteca de render/UI/física) que coordena renderização, física, IO, input e UI através de **interfaces abstratas e C++20 concepts**. Cada tecnologia concreta vive em seu próprio diretório de integração e se pluga ao core implementando essas interfaces.

| Papel | Tecnologia atual | Diretório | Documentação |
|---|---|---|---|
| Renderização 3D | Google Filament (Vulkan, fallback OpenGL) | `filament/` | [rendering/filament.md](rendering/filament.md) |
| UI | CEF (Chromium offscreen) + React | `CEF/` | [ui/cef.md](ui/cef.md) |
| Importação de assets | Assimp | `assimp/` | [assets/assimp.md](assets/assimp.md) |
| Serialização de cena | FlatBuffers | `flatbuffers/` | [core.md §8](core.md) |
| Janela | SDL2 atrás de `lite::View` | `include/filament/view/SDL/` | [core.md §9](core.md) |
| Input | SDL2 — **ainda não abstraído** (inline na `main.cpp` e no `EditorNavigationSystem`) | `core/src/main.cpp`, `include/editor/systems/` | [core.md §6](core.md) |
| Matemática agnóstica | GLM (header-only) | todo o `core/` | — |
| Física (planejado) | JoltPhysics | `3rd_party/JoltPhysics` (não integrado) | — |
| Animação (planejado) | ozz-animation | `3rd_party/ozz-animation` (não integrado) | — |
| JSON | nlohmann/json | ponte CEF↔C++ | [ui/cef.md](ui/cef.md) |

**Estado do projeto**: o processo de abstração/modularização está **em andamento**. Já validados em runtime (2026-07-20): cena + UI + wireframe, **picking por clique** (`ObjectSelectorSystem`) e o **gizmo de transformação** (`GizmoSystem` — render em overlay, picking dos eixos e drag de translação; escala em tela e os 3 modos ainda em refino). Em **2026-09-20** o gizmo passou a operar **múltiplos objetos** nas três operações, por delta de matriz em torno de um pivô, com os modos "conjunto" e "individual" já implementados (a flag existe, o input que a alterna ainda não) — ver [core.md §4.15](core.md).

**O que mudou entre 2026-08 e 2026-09** (8 commits, nesta ordem):

| Commit | Mudança estrutural |
|---|---|
| `6f2f25d` | `MaterialData` vira **base polimórfica** com `clone()`; parâmetros PBR descem para `MPBRLitMaterialData`. Todo o pipeline passa a transportar `vector<unique_ptr<MaterialData>>`. Nasce o lado instância (`MaterialInstance`/`MPBRLitMaterialInstance`) |
| `498e468`→`3f20c2f` | **Camada de serialização**: DTOs (`core/data/DTOs/`), facade `SceneSerializer`, mappers (`SceneDTOMapper`/`Asset3dDTOMapper`), impl `DummySceneSerializer` e `FlatBuffersSceneSerializer` (+ schema `scene.fbs`) |
| `041b880` | **Refactor de hierarquia**: nasce `lite::Node`, base **não-template** dos elos da árvore; `Asset3dInstance` passa a derivar dela e `parent`/`children`/`addChild` saem para lá |
| `7abaae5`→`7aa5236` | `GizmoSystem` e `EditorNavigationSystem` extraídos da `main` |
| `6705f1f`, `f9d38a7` | **Janela abstraída**: nasce `lite::View`; `SDLFilamentView` passa a ser dono do `SDL_Init` e da janela; `SceneRenderer` recebe `View*` em vez do handle nativo |
| `468dd1d` | **`SceneConfigurer`**: a montagem da cena de editor (systems + UI + IBL + asset inicial) sai da `main` para `EditorSceneConfigurer`/`FilamentEditorSceneConfigurer`. A `Scene` passa a ser **dona** dos systems (`addSystem(unique_ptr)`), ganha `getSystemOfType<T>()` e `getAll()`. `SceneRenderer` vira `template<SceneConcept, CameraConcept>` com `getCurrentCamera()` no contrato |

A `main.cpp` ainda contém: bootstrap do CEF, criação da `View` e do renderer, câmera inicial, serializer + mappers dummy, o laço de eventos SDL e a lógica de clique→seleção→wireframe→gizmo. **Atenção**: parte do que o configurer monta está quebrada — ver §5.

## 2. Estrutura de diretórios

```
LiteEngine/
├── CMakeLists.txt            # Build único (alvo `app`)
├── include/                  # TODOS os headers, espelhando a estrutura dos fontes
│   ├── core/                 # Camada agnóstica (nunca inclui Filament/CEF/SDL)
│   │   ├── concepts/         # Os contratos de plugabilidade (umbrella: EngineConcepts.h)
│   │   ├── data/assets/      # Dados CPU, instâncias, materiais + IO/ (mappers e facade)
│   │   ├── data/DTOs/        # DTOs de persistência (agnósticos de formato)
│   │   ├── scene/            # Scene, SceneRenderer, SceneConfigurer, SceneFactory
│   │   └── view/             # View — janela/superfície abstrata
│   ├── filament/             # Headers das implementações Filament (inclui view/SDL/)
│   ├── CEF/                  # Headers das implementações CEF
│   ├── assimp/               # Header do AssimpImporter
│   ├── flatbuffers/          # Header do FlatBuffersSceneSerializer
│   └── editor/               # Camada de editor agnóstica (Wireframe, ObjectSelector,
│                             #   Gizmo, EditorSceneConfigurer, systems/)
├── core/src/                 # .cpp da camada core + main.cpp
├── filament/                 # .cpp das implementações Filament
├── CEF/                      # .cpp das implementações CEF + frontend web
│   └── ui/resources/cef-ui/  # App React+Vite+TS (a UI de fato)
├── assimp/                   # .cpp do AssimpImporter
├── flatbuffers/              # .cpp do serializer + scene_generated.h + resources/scene.fbs
├── core/resources/filament/  # Materiais (.mat fonte, .filamat compilado)
├── 3rd_party/                # filament, cef, SDL, glm, flatbuffers, JoltPhysics, ozz, nlohmann
├── docs/                     # Esta documentação
├── build/, out/              # Diretórios de build
└── *.txt                     # Notas de planejamento/decisões (ver §7)
```

**Regra de camadas (invariante do projeto)**: `include/core/**` não pode incluir nada de Filament, CEF, SDL ou Assimp. A direção de dependência é sempre `filament | CEF | assimp | flatbuffers → core`, nunca o contrário. Detalhes em [core.md §1](core.md).

> ⚠️ **O invariante está furado em `include/editor/**`** (que a doc trata como parte da camada agnóstica): `editor/GizmoSystem.h` e `editor/systems/EditorNavigationSystem.h` incluem `<SDL.h>`, e `editor/EditorSceneConfigurer.h` referencia `FilamentAsset3dTransform`. Ver §5.11.

**Convenções**:
- Namespace `lite` para quase tudo (exceções no namespace global: `FilamentScene`, `FilamentUtils`, `CEF_UIApp`, `CEF_UIRenderProcessHandler`).
- Implementações concretas prefixam a tecnologia: `FilamentXxx`, `CEF_Xxx`, `AssimpXxx`.
- Headers em `include/<camada>/...` espelhando o caminho do `.cpp`.
- Comentários em português e inglês misturados; `TODO:`/`FIXME:` marcam dívidas reais.

## 3. Build

- CMake ≥ 3.15, C++20, MSVC no Windows, runtime **estático** (`MultiThreadedDebug` /MTd — consistência exigida entre `app` e `libcef_dll_wrapper`).
- Alvo único: `app` (executável). Não há testes automatizados.
- `target_compile_definitions(app PRIVATE _HAS_STD_BYTE=0)` — evita conflito `std::byte` vs `byte` do Windows SDK (CEF + Filament + MSVC).
- Filament é consumido **pré-compilado** de `3rd_party/filament/out/` (build Debug). SDL idem (`3rd_party/SDL-release-2.32.10/build/Debug`). CEF via `find_package(CEF)` com `CEF_ROOT=3rd_party/cef`.
- **FlatBuffers é header-only** no build (`3rd_party/flatbuffers/include`); o compilador de schema é um executável à parte. O `scene_generated.h` é **versionado**, não gerado pelo build — regerar manualmente após mexer no schema:
  `3rd_party/flatbuffers/bin/flatc.exe --cpp -o flatbuffers/data/assets/IO flatbuffers/resources/scene.fbs`
- **Todo novo `.cpp` precisa ser adicionado manualmente em `target_sources(app ...)` no CMakeLists.txt** — não há glob. `add_executable(app ...)` lista só `core/src/main.cpp`; todo o resto entra pelo `target_sources`.
- Vários **headers** estão listados em `target_sources` de propósito: é só para aparecerem na solução do Visual Studio (header não vira unidade de compilação por estar ali).
- O frontend da UI precisa de build próprio: `cd CEF/ui/resources/cef-ui && npm install && npm run build` (o C++ carrega `dist/index.html` via `file://`).

Compilar: `cmake -B build && cmake --build build --config Debug` (ou abrir a solução gerada no Visual Studio).

## 4. Modelo de threads (visão transversal)

O modelo implementado é o **"B2"** do documento `create_sceneRenderer.txt`: render loop independente, sem sincronização por frame com a main thread.

```
┌────────────── Main thread ──────────────┐   ┌──────────── Render thread ─────────────┐
│ SDL_PollEvent → InputEvent               │   │ SceneRenderer::renderThreadMain():      │
│ navigation->digestInputEvent(ev) ───────▶│   │  setup()    → Engine (VULKAN→OPENGL),   │
│ gizmoSystem->digestInput(ev)    ───────▶ │   │    SwapChain, FilamentScene, UI GPU,    │
│ clique → picking → seleção/wireframe     │   │    câmera; set m_readyPromise           │
│ sceneRenderer.setCameraState() ──mutex──▶│   │  waitStart()→ spin-wait processando fila│
│ sceneRenderer.postCommand()  ──queue───▶ │   │  renderLoop()→ processCommands → câmera │
│ uiRenderer->sendInputEvent() ──► CEF     │   │    → m_scene->update(dt) → sleep 1ms    │
│ scene->create()/destroy()   ──mutex────▶ │   │      └─ systems[].onFrameBegin(dt):     │
└──────────────────────────────────────────┘   │         navegação move a câmera AQUI    │
                                               │  drena fila → cleanup() GPU na thread   │
┌────────────── CEF thread ────────────────┐   └─────────────────────────────────────────┘
│ CefInitialize (multi_threaded_msg_loop)  │
│ OnPaint (thread do CEF) → double buffer  │──▶ consumido por uiRenderer->update() na
│ OnQuery (eventos JS→C++)                 │    render thread (upload de textura)
└──────────────────────────────────────────┘
   (CEF também cria subprocessos: ver ui/cef.md §2)
```

**Restrição fundamental do Filament (thread affinity)**: toda operação no `filament::Engine` (create/destroy/render) deve acontecer **na mesma thread que criou o engine**. Por isso o engine é criado *dentro* da render thread e qualquer trabalho GPU vindo de fora entra via `FilamentSceneRenderer::postCommand()`. Detalhes em [rendering/filament.md §2](rendering/filament.md).

**Pontes entre threads** (resumo; detalhes nos docs de cada módulo):

| Ponte | Mecanismo | Onde |
|---|---|---|
| main → render: comandos GPU | fila `postCommand` + mutex | `lite::SceneRenderer` (base) |
| main → render: câmera | estado pendente + mutex, consumido 1×/frame | `lite::SceneRenderer` (base) |
| qualquer → render: criação de assets | fila `m_creatingObjects` + mutex + CV | `lite::Scene` |
| qualquer → render: deleção de assets | marcação `isDeleted` + flush pós-frame | `FilamentInstanceFactory` |
| CEF → render: pixels da UI | double buffer + swap de índices sob mutex | `CEF_Filament_UIRendererThreaded` |
| CEF → main/lógica: eventos de UI | `OnQuery` JSON → `invokeEvents` (callbacks C++) | `CEF_Filament_UIRendererThreaded` |

**Ordem de inicialização atual (ver `core/src/main.cpp`)**:
1. `CefExecuteProcess` (PRIMEIRA coisa no main — subprocessos CEF retornam aqui).
2. `View* view = new SDLFilamentView(w, h); view->Init();` — a `View` é dona do `SDL_Init` e da janela; `getDimensions()` devolve o tamanho REAL (o modo fullscreen-desktop ignora os literais pedidos).
3. `FilamentSceneRenderer renderer(view, fbW, fbH)` + `waitReady()`.
4. Câmera inicial (`setCameraState`) e serializer + mappers.
5. `uiInstance = new CEF_Filament_UIInstance(uiRenderer)` — construído, **não** iniciado aqui.
6. `renderer.start()`.
7. `configurer->configure(scene)` — registra os 4 systems, monta a UI, carrega o asset inicial e posta IBL + luz direcional.
8. Laço principal: eventos SDL → navegação/gizmo/picking.
9. Shutdown: hoje só `renderer.stop()` — o teardown explícito dos systems está **comentado** (a `Scene` passou a ser dona deles; ver §5.10).

> ⚠️ **Regressão de ordem (§5.9)**: os passos 6 e 7 estão **invertidos em relação ao contrato**. `addSystem` só pode acontecer antes do `start()` ou via `postCommand`, porque o vetor de systems não tem lock — hoje o configurer registra os systems com a render thread já rodando.

## 5. Dívidas técnicas e armadilhas conhecidas (IMPORTANTE para agentes)

1. **`Scene::get()` retorna EMPRÉSTIMO** (o dono é o mapa da Scene) — nunca envolver em `unique_ptr`. O double-ownership que existia na `main.cpp` (unique_ptr sobre o ponteiro de `get()` + `destroy(std::move(...))` no shutdown → double-free) **foi corrigido**: hoje o shutdown destrói por id.
2. **`Scene::destroy` não remove do mapa**: o `erase` está comentado (deliberadamente, pois a deleção GPU é adiada), então instâncias deletadas permanecem em `m_3dInstances` com `isDeleted()==true` e o `unique_ptr` nunca libera a memória CPU. O `onFrameEnd` do `FilamentWireframeSystem` depende disso para limpar wireframes. Um FIXME pede solução mais eficiente para `destroy(unique_ptr)` (busca linear).
3. **`Scene::get` é bloqueante**: se chamado na própria render thread antes de `instantiate()`, deadlock potencial. Chame `get()` apenas de fora da render thread, ou após a instanciação.
4. **Paths absolutos hardcoded** (`D:/Workspace/LiteEngine/...`): material base (`FilamentInstanceFactory` ctor), material da UI (`createMaterial`), URL do HTML (`start`), IBL e modelo de exemplo (`main.cpp`), material do wireframe (`main.cpp`). Portabilidade exige um sistema de resource paths.
5. **Arquivos stub/quebrados** (não compilados, não incluir no build sem consertar):
   - `filament/data/assets/IO/FilamentSceneDTOMapper.cpp` — **novo (2026-08)**: `.cpp` com `#pragma once` contendo um template; tem `typname` (typo de `typename`), corpo de classe sem `;` final e usa `FilamentInstanceFactory` sem incluí-lo. Fora do `target_sources`.
   - `include/core/scene/SceneFactory.h` — **não está mais quebrado** (2026-09): hoje é `template<SceneConcept SceneType> class SceneFactory` com `build()` puro e sintaxe válida. Só falta `#pragma once`. Ninguém o implementa ainda; a `main` o inclui.
   - `include/core/ui/UIEvenetsManager.h` — stub vazio (typo no nome).
   - `include/CEF/ui/CEF_UIEditor.h` — referencia headers inexistentes (`editor/UI/UIEditor.h`, `core/ui/CEFUIRendererThreaded.h`).
   - `core/src/ui/elements/UIElements.cpp` (vazio), `core/src/ui/elements/UIElementHandler.cpp` (1 linha), `filament/ui/elements/CEF_UIElements.cpp` (2 linhas) — órfãos, fora do `target_sources`.
   - `CEF/ui/CEF_Filament_UIInstance.cpp` — existe mas está fora do build (a classe é header-only hoje).
   - `CEF/ui/resources/index.html|script.js|styles.css` — UI antiga pré-React, substituída pelo app `cef-ui/`.
6. **`TransformUtils`**: só `build()` tem especialização real; os `buildWithPosition/Rotation/Scale` **ignoram os argumentos**.
7. **Tratamento de erro frágil**: `FilamentAsset3dTransform::assertEntity` lança `const char*` (não `std::exception`); vários retornos de erro só logam em `std::cout/cerr`.
8. **`UIRenderer::registerElement`** usa `map::emplace` — re-draw de um elemento com id existente não atualiza o handler.
9. **O vetor de sistemas da Scene não tem lock** — `addSystem`/`removeSystem` só antes de `sceneRenderer.start()` (main thread) ou via `postCommand` (render thread). Hooks de sistema nunca devem chamar `Scene::get()` de um id ainda na fila de criação (deadlock). **Violado hoje**: a `main` chama `sceneRenderer.start()` ANTES de `configurer->configure(scene)`, que é quem faz os `addSystem` — quatro inserções no vetor com a render thread já iterando sobre ele.
10. **Teardown de sistemas com GPU está desativado** (2026-09). A `Scene` virou **dona** dos systems (`addSystem(unique_ptr)`, `removeSystem` destrói), então o antigo padrão `postCommand([&]{ removeSystem(x); x.reset(); })` deixou de fazer sentido e foi **comentado na `main`** — sem substituto. Hoje os destrutores do wireframe e do gizmo (que destroem recursos GPU) rodam quando a `Scene` morre, e não há garantia de que isso aconteça na render thread. Ver [rendering/filament.md §8](rendering/filament.md).
11. **A camada `editor/` furou o invariante de camadas** (2026-08/09) — dois headers incluem `<SDL.h>`:
    - `include/editor/GizmoSystem.h` — `digestInput(SDL_Event, glm::vec2)`;
    - `include/editor/systems/EditorNavigationSystem.h` — `digestInputEvent(SDL_Event)`.
    Os dois sistemas são agnósticos em tudo **menos no input**; some quando o módulo de Input existir. Além disso:
    - `include/editor/EditorSceneConfigurer.h` referencia `FilamentAsset3dTransform` dentro de `configureUI()` (camada agnóstica citando classe concreta).
12. **`EditorSceneConfigurer`: callbacks dos botões continuam vazios** (2026-09-20). O que impedia a compilação foi resolvido — os corpos que usavam `importer`, `rootNode`, `currentInstanceId`, `currentScene` e `assetPtr` (sobras do recorte da `main`) estão **comentados**, e `configureUI` ganhou o `return scene` que faltava. Ficam duas pendências: Carregar/Deletar/Salvar não fazem nada, e o header ainda inclui `filament/data/assets/FilamentAsset3dTransform.h` (§5.11) por causa de uma citação dentro do bloco comentado.
    > **Armadilha registrada**: a ausência daquele `return` num método que devolve `SceneType*` fazia `configure()` seguir com o valor residual do registrador — `loadScene3dInstances` recebia um ponteiro lixo e o `create()` estourava ao trancar `m_instancesMutex` num endereço inválido. Vale para qualquer caminho sem `return` nesta camada: o sintoma aparece longe da causa.
13. **`Scene::getAll()` não tranca o mutex**, ao contrário de `find`/`getNode`/`destroy` — iterar `m_3dInstances` enquanto a render thread instancia é race. É o método que o `SceneDTOMapper` usa para serializar.
14. **CMake**: blocos de `target_sources` comentados referenciam serviços antigos (`core/src/services/*`) já removidos da árvore.
15. **Callbacks armazenados não podem capturar por referência** (2026-09-20). O callback de seleção que o `configure()` registra no selector sobrevive ao escopo de quem o criou — ele é copiado para dentro da `std::function` do system, que pertence à `Scene`. Capturar `[&]` um `unique_ptr` local, ainda por cima logo antes de um `std::move` para a `Scene`, deixa a captura pendurada e estoura no primeiro disparo (o `unique_ptr` vira nulo no move e o frame morre no `return`). A forma correta é capturar por **valor** o que o callback precisa: hoje ele captura a `scene` e reobtém o gizmo por `getSystemOfType`.

## 6. Roadmap implícito (TODOs no código)

- Main loop deve migrar para dentro de `Scene::update` (comentário TODO no header): input → lógica de game (loop por assets/componentes) → UI → limpeza → render.
- Serviços dedicados de Materiais e Assets (TODOs em `FilamentAsset3dInstance`). **Andou em 2026-08-10**: existe o lado instância do material (`MaterialInstance` → `MPBRLitMaterialInstance` → `FilamentMPBRLitMaterialInstance`), mas quem possui as `MaterialInstance` continua sendo a raiz do asset, não um serviço.
- Views da cena 3D e da UI overlay poderiam compartilhar a mesma `filament::View` (TODO em `createFilamentResources`).
- Integração de JoltPhysics (física) e ozz-animation (animação) — libs já vendorizadas.
- **Janela: FEITO** (2026-08-31) — `lite::View` + `SDLFilamentView`. **Input: pendente** — é o último ponto em que o SDL aparece acima da camada concreta (`main.cpp` e `EditorNavigationSystem`). Planejado virar módulo agnóstico no molde da UI.
- **Câmera FPS fora da main: FEITO** (2026-08-16) — `EditorNavigationSystem`, registrado como `SceneScopeSystem`.
- **Montagem da cena fora da main: PARCIAL** (atualizado 2026-09-20) — `SceneConfigurer`/`EditorSceneConfigurer` já recebem systems, UI, IBL e asset inicial, e o que foi movido **compila e roda**; o configurer também já liga seleção → gizmo por callback. Continuam na `main`: o laço de eventos, o clique→seleção→wireframe e os callbacks dos botões vazios (§5.12).
- `SceneFactory` para **construir** cenas sem conhecer os tipos concretos — contrato já existe (`build()` puro), sem implementação. Complementar ao `SceneConfigurer`, que **ajusta** uma cena já construída.
- **Persistência: FEITO em escrita** (2026-08-13) — facade `SceneSerializer` + DTOs + mappers, com `FlatBuffersSceneSerializer`. Buracos conhecidos: UVs saem vazias, materiais do `SceneDTO` ficam vazios, `fromDto` (load) devolve `nullptr` em todos os mappers. Ver [core.md §8](core.md).
- **Decisão adiada (2026-07): índice de nós por id** — `create(deepIds=true)` já atribui ids a toda a árvore; a consulta de subobjeto por id existe como **paliativo** (`Scene::getNode(id)`, busca linear recursiva O(nós), retorna o tipo base, não bloqueia — adicionado em 2026-07-20 para o picking), mas o índice definitivo segue pendente. Bloqueadores identificados: dupla posse (filhos já são donos do pai; `m_3dInstances` é mapa de `unique_ptr`) e tipo (meshes não derivam de `AssetType`). Opções na mesa: índice separado não-dono vs alargar o mapa principal — e a escolha afeta `get`/`find`/`destroy` em conjunto (`destroy` de id de filho é o caso espinhoso). Motivação: picking/seleção. Nome da flag `deepIds` é provisório.

## 7. Documentos de decisão na raiz (*.txt)

Notas de planejamento escritas durante o desenvolvimento — úteis para entender o "porquê":
- `create_sceneRenderer.txt` — análise das opções A/B1/B2 para o facade `SceneRenderer` e o problema de thread affinity do Filament. **A opção B2 (render thread independente) foi a implementada.**
- `extraindo_concepts.txt` — plano (executado) de deduplicação dos concepts para `include/core/concepts/`.
- `refactor.txt` — plano (executado) de mover implementações de headers para `.cpp` (o que pode e o que não pode por ser template).
- `deletion_assets.txt` — plano de deleção cross-thread com Treiber stack lock-free (**não implementado assim**; a solução atual é marcação + flush pós-frame no factory).

# LiteEngine — Visão Geral e Índice da Documentação

> Documento de referência para agentes e desenvolvedores. Gerado a partir da análise do código em julho/2026 (branch `abstract_scene`). Se o código divergir deste documento, o código vence — atualize este arquivo.

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
| Janela + input | SDL2 | inline em `core/src/main.cpp` (**ainda não abstraído**) | [core.md §8](core.md) |
| Matemática agnóstica | GLM (header-only) | todo o `core/` | — |
| Física (planejado) | JoltPhysics | `3rd_party/JoltPhysics` (não integrado) | — |
| Animação (planejado) | ozz-animation | `3rd_party/ozz-animation` (não integrado) | — |
| JSON | nlohmann/json | ponte CEF↔C++ | [ui/cef.md](ui/cef.md) |

**Estado do projeto**: o processo de abstração/modularização está **em andamento**. A `main.cpp` ainda é um sandbox que carrega um IBL e um modelo 3D hardcoded; a intenção declarada é mover tudo para trás de facades e interfaces. Já validados em runtime: cena + UI + wireframe, **picking por clique** (`ObjectSelectorSystem`) e o **gizmo de transformação** (`GizmoSystem` — render em overlay, picking dos eixos e drag de translação; escala em tela e os 3 modos ainda em refino). Input ainda é SDL inline na main (planejado virar módulo agnóstico no molde da UI).

## 2. Estrutura de diretórios

```
LiteEngine/
├── CMakeLists.txt            # Build único (alvo `app`)
├── include/                  # TODOS os headers, espelhando a estrutura dos fontes
│   ├── core/                 # Camada agnóstica (nunca inclui Filament/CEF/SDL)
│   ├── filament/             # Headers das implementações Filament
│   ├── CEF/                  # Headers das implementações CEF
│   ├── assimp/               # Header do AssimpImporter
│   └── editor/               # Sistemas de editor agnósticos (WireframeSystem, ObjectSelectorSystem, GizmoSystem)
├── core/src/                 # .cpp da camada core + main.cpp
├── filament/                 # .cpp das implementações Filament
├── CEF/                      # .cpp das implementações CEF + frontend web
│   └── ui/resources/cef-ui/  # App React+Vite+TS (a UI de fato)
├── assimp/                   # .cpp do AssimpImporter
├── core/resources/filament/  # Materiais (.mat fonte, .filamat compilado)
├── 3rd_party/                # filament, cef, SDL, glm, JoltPhysics, ozz, nlohmann
├── docs/                     # Esta documentação
├── build/, out/              # Diretórios de build
└── *.txt                     # Notas de planejamento/decisões (ver §7)
```

**Regra de camadas (invariante do projeto)**: `include/core/**` não pode incluir nada de Filament, CEF, SDL ou Assimp. A direção de dependência é sempre `filament | CEF | assimp → core`, nunca o contrário. Detalhes em [core.md §1](core.md).

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
- **Todo novo `.cpp` precisa ser adicionado manualmente em `target_sources(app ...)` no CMakeLists.txt** — não há glob.
- O frontend da UI precisa de build próprio: `cd CEF/ui/resources/cef-ui && npm install && npm run build` (o C++ carrega `dist/index.html` via `file://`).

Compilar: `cmake -B build && cmake --build build --config Debug` (ou abrir a solução gerada no Visual Studio).

## 4. Modelo de threads (visão transversal)

O modelo implementado é o **"B2"** do documento `create_sceneRenderer.txt`: render loop independente, sem sincronização por frame com a main thread.

```
┌────────────── Main thread ──────────────┐   ┌──────────── Render thread ─────────────┐
│ SDL_PollEvent → InputEvent               │   │ SceneRenderer::renderThreadMain():      │
│ câmera FPS (WASD+mouse)                  │   │  setup()    → Engine (VULKAN→OPENGL),   │
│ sceneRenderer.setCameraState() ──mutex──▶│   │    SwapChain, FilamentScene, UI GPU,    │
│ sceneRenderer.postCommand()  ──queue───▶ │   │    câmera; set m_readyPromise           │
│ uiRenderer->sendInputEvent() ──► CEF     │   │  waitStart()→ spin-wait processando fila│
│ scene->create()/destroy()   ──mutex────▶ │   │  renderLoop()→ processCommands → câmera │
└──────────────────────────────────────────┘   │    → m_scene->update(dt) → sleep 1ms    │
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

**Ordem de inicialização correta (ver `core/src/main.cpp`)**:
1. `CefExecuteProcess` (PRIMEIRA coisa no main — subprocessos CEF retornam aqui).
2. SDL cria janela → native handle.
3. `FilamentSceneRenderer renderer(handle, w, h)` + `waitReady()`.
4. Comandos de conteúdo: `setIBL`, `addDirectionalLight` (viram comandos na fila).
5. Sistemas: construção/configuração **na main** (só dados — o GPU é lazy no 1º hook) + `addSystem` — tudo **antes** do start (vetor sem lock). Ex.: wireframe.
6. Setup da UI (`uiInstance->start()` — só CEF, sem GPU; elementos; `root->draw()`).
7. `renderer.start()` → só então `scene->create(...)` de assets (o sandbox carrega o FBX inicial com `deepIds=true`).
8. Shutdown: teardown de sistemas com GPU via `postCommand` (`removeSystem` + reset no mesmo comando) seguido de `stop()` explícito — a base drena a fila antes do `cleanup()`.

## 5. Dívidas técnicas e armadilhas conhecidas (IMPORTANTE para agentes)

1. **`Scene::get()` retorna EMPRÉSTIMO** (o dono é o mapa da Scene) — nunca envolver em `unique_ptr`. O double-ownership que existia na `main.cpp` (unique_ptr sobre o ponteiro de `get()` + `destroy(std::move(...))` no shutdown → double-free) **foi corrigido**: hoje o shutdown destrói por id.
2. **`Scene::destroy` não remove do mapa**: o `erase` está comentado (deliberadamente, pois a deleção GPU é adiada), então instâncias deletadas permanecem em `m_3dInstances` com `isDeleted()==true` e o `unique_ptr` nunca libera a memória CPU. O `onFrameEnd` do `FilamentWireframeSystem` depende disso para limpar wireframes. Um FIXME pede solução mais eficiente para `destroy(unique_ptr)` (busca linear).
3. **`Scene::get` é bloqueante**: se chamado na própria render thread antes de `instantiate()`, deadlock potencial. Chame `get()` apenas de fora da render thread, ou após a instanciação.
4. **Paths absolutos hardcoded** (`D:/Workspace/LiteEngine/...`): material base (`FilamentInstanceFactory` ctor), material da UI (`createMaterial`), URL do HTML (`start`), IBL e modelo de exemplo (`main.cpp`), material do wireframe (`main.cpp`). Portabilidade exige um sistema de resource paths.
5. **Arquivos stub/quebrados** (não compilados, não incluir no build sem consertar):
   - `include/core/scene/SceneFactory.h` — sintaxe inválida (template<> sem parâmetros, sem `;` final).
   - `include/core/ui/UIEvenetsManager.h` — stub vazio (typo no nome).
   - `include/CEF/ui/CEF_UIEditor.h` — referencia headers inexistentes (`editor/UI/UIEditor.h`, `core/ui/CEFUIRendererThreaded.h`).
   - `core/src/ui/elements/UIElements.cpp` (vazio), `core/src/ui/elements/UIElementHandler.cpp` (1 linha), `filament/ui/elements/CEF_UIElements.cpp` (2 linhas) — órfãos, fora do `target_sources`.
   - `CEF/ui/CEF_Filament_UIInstance.cpp` — existe mas está fora do build (a classe é header-only hoje).
   - `CEF/ui/resources/index.html|script.js|styles.css` — UI antiga pré-React, substituída pelo app `cef-ui/`.
6. **`TransformUtils`**: só `build()` tem especialização real; os `buildWithPosition/Rotation/Scale` **ignoram os argumentos**.
7. **Tratamento de erro frágil**: `FilamentAsset3dTransform::assertEntity` lança `const char*` (não `std::exception`); vários retornos de erro só logam em `std::cout/cerr`.
8. **`UIRenderer::registerElement`** usa `map::emplace` — re-draw de um elemento com id existente não atualiza o handler.
9. **O vetor de sistemas da Scene não tem lock** — `addSystem`/`removeSystem` só antes de `sceneRenderer.start()` (main thread) ou via `postCommand` (render thread). Hooks de sistema nunca devem chamar `Scene::get()` de um id ainda na fila de criação (deadlock).
10. **CMake**: blocos de `target_sources` comentados referenciam serviços antigos (`core/src/services/*`) já removidos da árvore.

## 6. Roadmap implícito (TODOs no código)

- Main loop deve migrar para dentro de `Scene::update` (comentário TODO no header): input → lógica de game (loop por assets/componentes) → UI → limpeza → render.
- Serviços dedicados de Materiais e Assets (TODOs em `FilamentAsset3dInstance`).
- Views da cena 3D e da UI overlay poderiam compartilhar a mesma `filament::View` (TODO em `createFilamentResources`).
- Integração de JoltPhysics (física) e ozz-animation (animação) — libs já vendorizadas.
- Abstrair SDL (janela/input) atrás de interface própria; câmera FPS fora da main.
- `SceneFactory` para montar cenas sem conhecer os tipos concretos.
- **Decisão adiada (2026-07): índice de nós por id** — `create(deepIds=true)` já atribui ids a toda a árvore; a consulta de subobjeto por id existe como **paliativo** (`Scene::getNode(id)`, busca linear recursiva O(nós), retorna o tipo base, não bloqueia — adicionado em 2026-07-20 para o picking), mas o índice definitivo segue pendente. Bloqueadores identificados: dupla posse (filhos já são donos do pai; `m_3dInstances` é mapa de `unique_ptr`) e tipo (meshes não derivam de `AssetType`). Opções na mesa: índice separado não-dono vs alargar o mapa principal — e a escolha afeta `get`/`find`/`destroy` em conjunto (`destroy` de id de filho é o caso espinhoso). Motivação: picking/seleção. Nome da flag `deepIds` é provisório.

## 7. Documentos de decisão na raiz (*.txt)

Notas de planejamento escritas durante o desenvolvimento — úteis para entender o "porquê":
- `create_sceneRenderer.txt` — análise das opções A/B1/B2 para o facade `SceneRenderer` e o problema de thread affinity do Filament. **A opção B2 (render thread independente) foi a implementada.**
- `extraindo_concepts.txt` — plano (executado) de deduplicação dos concepts para `include/core/concepts/`.
- `refactor.txt` — plano (executado) de mover implementações de headers para `.cpp` (o que pode e o que não pode por ser template).
- `deletion_assets.txt` — plano de deleção cross-thread com Treiber stack lock-free (**não implementado assim**; a solução atual é marcação + flush pós-frame no factory).

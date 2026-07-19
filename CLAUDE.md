# LiteEngine

Game engine em **C++20** com foco em **modularidade extrema**: um `core/` agnóstico (interfaces + C++20 concepts, matemática em GLM) coordena renderização, UI, input e assets; cada tecnologia concreta vive em seu diretório de integração.

> **Documentação completa em `docs/`** — leia antes de mexer em Scene, threading, UI ou no fluxo de assets:
> - [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — visão geral, build, modelo de threads, dívidas técnicas, roadmap (índice de tudo)
> - [docs/core.md](docs/core.md) — camada agnóstica: **concepts detalhados**, hierarquias completas de classes e facades, `Scene`, UI abstrata, input
> - [docs/rendering/filament.md](docs/rendering/filament.md) — render Filament, com o mapa "interface do core → classe concreta"
> - [docs/ui/cef.md](docs/ui/cef.md) — UI CEF+React, protocolo C++↔JS, idem mapa de interfaces
> - [docs/assets/assimp.md](docs/assets/assimp.md) — importação Assimp

## Mapa rápido

- `include/` — todos os headers, espelhando os fontes. `include/core/**` **nunca** inclui Filament/CEF/SDL/Assimp (invariante do projeto).
- `core/src/` — camada agnóstica + `main.cpp` (exemplo/sandbox atual).
- `filament/` — render (Google Filament, Vulkan→OpenGL). Classes `FilamentXxx`.
- `CEF/` — UI via Chromium offscreen; frontend React+Vite em `CEF/ui/resources/cef-ui/`. Classes `CEF_Xxx`.
- `assimp/` — importação de modelos 3D.
- `3rd_party/` — deps vendorizadas (filament, cef, SDL2, glm, nlohmann; Jolt e ozz ainda não integrados).
- `*.txt` na raiz — notas de decisão de design (ver ARCHITECTURE.md §13).

## Peças centrais

- `lite::Scene<Asset, Transform, Factory, UIRenderer>` (`include/core/scene/Scene.h`) — dono das instâncias; criação de assets é **enfileirada** (`create()` retorna id; instanciação ocorre na render thread), deleção é **em duas fases** (marca + flush pós-frame).
- `lite::SceneRenderer<SceneType>` (`include/core/scene/SceneRenderer.h`) — facade abstrato que possui a **render thread** (Template Method: `setup`/`renderFrame`/`cleanup` virtuais). `FilamentSceneRenderer` é a implementação. O `filament::Engine` só pode ser usado na thread que o criou; trabalho GPU de fora entra por `postCommand()`.
- `CEF_Filament_UIRendererThreaded` — CEF roda em thread própria; pixels via double-buffer → textura Filament num quad translúcido. C++→JS: `window.liteUI.addElement/updateElement`; JS→C++: `cefQuery` com JSON `{id, type, value}`.
- Concepts em `include/core/concepts/` (umbrella `EngineConcepts.h`) definem os contratos entre core e implementações.

## Build

- CMake ≥ 3.15, MSVC, runtime estático `/MTd`; alvo único `app`.
- `cmake -B build && cmake --build build --config Debug`.
- **Novo `.cpp` deve ser adicionado manualmente em `target_sources(app ...)`** no CMakeLists.txt (não há glob).
- Frontend da UI: `cd CEF/ui/resources/cef-ui && npm run build` (C++ carrega `dist/index.html` via `file://`).
- Filament/SDL/CEF são consumidos pré-compilados de `3rd_party/`.

## Cuidados ao editar (resumo — detalhes em ARCHITECTURE.md §12)

1. `Scene::get()` retorna ponteiro **emprestado** (dono é a Scene) e pode **bloquear** esperando a render thread — nunca envolver em `unique_ptr`, nunca chamar da render thread antes da instanciação.
2. Registrar callbacks/sistemas da Scene **antes** de `sceneRenderer.start()` (vetores sem lock).
3. Recursos GPU (materiais, wireframe, buffers) só podem ser criados/destruídos na render thread — usar `postCommand()`.
4. `CefExecuteProcess` deve permanecer a **primeira** instrução do `main()` (subprocessos CEF).
5. Há paths absolutos `D:/Workspace/LiteEngine/...` hardcoded (materiais, IBL, HTML) — não espalhar mais; existe intenção de criar sistema de resource paths.
6. Stubs quebrados que não devem entrar no build sem conserto: `SceneFactory.h`, `UIEvenetsManager.h`, `CEF_UIEditor.h` (e cpps órfãos listados na doc).

## Estado atual

Branch `abstract_scene`: abstração em andamento. `main.cpp` ainda é um sandbox que carrega IBL + modelo FBX hardcoded, monta uma UI de teste (input de path + botões Carregar/Deletar) e roda câmera FPS inline. Objetivo declarado: mover tudo para facades/interfaces (main loop dentro da Scene, serviços de materiais/assets, física Jolt, animação ozz).

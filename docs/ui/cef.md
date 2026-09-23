# LiteEngine — Módulo de UI: CEF (+ React)

> Parte da [documentação de arquitetura](../ARCHITECTURE.md). Interfaces do core: [core.md §5](../core.md).

Implementação concreta da UI usando **CEF (Chromium Embedded Framework)** em modo offscreen (windowless): o Chromium renderiza uma página React em memória e o resultado é composto por cima da cena 3D como textura Filament. Código em `CEF/**` (fontes + frontend web) e `include/CEF/**` (headers). Classes no namespace `lite`, exceto `CEF_UIApp` e `CEF_UIRenderProcessHandler` (global).

**Nota de acoplamento**: este módulo é deliberadamente "CEF **+ Filament**" (os nomes dizem isso: `CEF_Filament_...`). A parte CEF (browser, IPC, input) seria reutilizável com outro renderer, mas a composição final (textura/quad/view) é Filament — um novo renderer exigiria outra classe `CEF_<Renderer>_UIRenderer...`.

## 1. Mapa de implementação — interface do core → classe CEF

| Interface / template do core | Implementação CEF | Papel | Arquivos |
|---|---|---|---|
| `lite::UIRenderer<filament::Renderer>` | `lite::CEF_Filament_UIRendererThreaded` | renderer de UI (satisfaz `UIRendererConcept`, `RendererType = filament::Renderer`) | `include/CEF/ui/CEF_Filament_UIRendererThreaded.h` + `CEF/ui/CEF_Filament_UIRendererThreaded.cpp` |
| `lite::UIInstance<CEF_Filament_UIRendererThreaded>` | `lite::CEF_Filament_UIInstance` | documento de UI; implementa o factory method `createRoot()` | `include/CEF/ui/CEF_Filament_UIInstance.h` (header-only) |
| `lite::UIPanelElement<URT>` | `lite::CEF_UIPanelElement` | container grid — implementa `drawContainer()` | `include/CEF/ui/elements/CEF_UIElements.h` + `CEF/ui/elements/CEF_UIElements.cpp` |
| `lite::UITextElement<URT>` | `lite::CEF_UITextElement` | texto — implementa `draw/setText/getText` | idem |
| `lite::UITextInputElement<URT>` | `lite::CEF_UITextInputElement` | input de texto — implementa `draw/getText/updateInput` | idem |
| `lite::UICheckBoxElement<URT>` | `lite::CEF_UICheckBoxElement` | checkbox — implementa `draw/isChecked/setChecked` | idem |
| `lite::UIComboBoxInputElement<URT>` | `lite::CEF_UIComboBoxInputElement` | combo — implementa `draw/addOption/getSelectedOption/updateInput` | idem |
| `lite::UIButtonElement<URT>` | `lite::CEF_UIButtonElement` | botão — implementa `draw` (com label) | idem |
| `lite::UITreeElement<URT, DataType>` | `lite::CEF_UITreeElement<DataType>` | treeview — implementa `draw` e o hook `redrawTree()` (a base do core guarda os nós e gera os ids) | `include/CEF/ui/elements/CEF_UIElements.h` (header-only: é template) |
| — (infra CEF, sem contraparte no core) | `CEF_UIApp` (`CefApp`) | bootstrap dos processos CEF | `include/CEF/CEF_UIApp.h` + `CEF/CEF_UIApp.cpp` |
| — (infra CEF) | `CEF_UIRenderProcessHandler` (`CefRenderProcessHandler`) | lado renderer-process do MessageRouter | `include/CEF/ui/CEF_UIRenderProcessHandler.h` + `CEF/ui/CEF_UIRenderProcessHandler.cpp` |
| — (frontend) | app React `cef-ui` | os widgets DOM de fato | `CEF/ui/resources/cef-ui/` |

Em todas as linhas acima, `URT = CEF_Filament_UIRendererThreaded`. Todo widget concreto **deve chamar `UIElement::draw()` da base** dentro do seu `draw()` (é a base que gera o id e registra o `UIElementHandler` no renderer — sem isso os eventos JS→C++ não roteiam).

`CEF_UITreeElement` é template em `DataType`, então vive inteiro no header, fora do `CEF_UIElements.cpp`. Por isso o `CEF_UIElements.h` inclui o nlohmann/json entre `#pragma push_macro`/`#pragma pop_macro` de `assert_invariant` e `UTILS_VERY_LIKELY` (macros do Filament que colidem com o nlohmann): o `#undef` vale só durante o include, e quem inclui o header continua com as macros do Filament.

## 2. Arquitetura de processos e threads do CEF

CEF é **multiprocesso**: o executável `app` é reexecutado como subprocessos (renderer, GPU, utility). Por isso:

- `main()` chama **`CefExecuteProcess` como primeira instrução**, passando `CEF_UIApp`. Nos subprocessos essa chamada só retorna no fim (exit code ≥ 0 → `main` retorna imediatamente); no processo principal (browser process) retorna -1 e o programa segue.
- `CEF_UIApp` (usado em ambos os lados) fornece:
  - `OnBeforeCommandLineProcessing` — flags de linha de comando do Chromium;
  - `GetRenderProcessHandler()` → `CEF_UIRenderProcessHandler` — **roda no subprocesso renderer**; cria o `CefMessageRouterRendererSide` que injeta `window.cefQuery` no JS (`OnWebKitInitialized/OnContextCreated/OnContextReleased/OnProcessMessageReceived` apenas delegam ao router).
- No browser process, `CEF_Filament_UIRendererThreaded::start()` spawna a **thread CEF** (`cefThreadFunc`): `CefInitialize` com `windowless_rendering_enabled=true`, `no_sandbox=true`, **`multi_threaded_message_loop=true`** (o CEF gerencia seu próprio message loop — a thread só cria o browser e dorme até `m_running=false`, então fecha o browser e chama `CefShutdown`).

Threads que tocam esta classe: **thread CEF/UI** (callbacks `OnPaint`, `OnQuery`, etc., vindas do message loop interno do CEF), **render thread** (`update()`, `render()` — via callbacks da `Scene`), **main thread** (`sendInputEvent`, criação de widgets). A sincronização é o double buffer (§4) e atomics.

## 3. `CEF_Filament_UIRendererThreaded` — a classe central

Herança múltipla (é ao mesmo tempo o cliente CEF e o renderer de UI do core):

```
CefClient ─┬─ CefRenderHandler        (OnPaint, GetViewRect)
           ├─ CefLifeSpanHandler      (OnAfterCreated, OnBeforeClose)
           ├─ CefDisplayHandler       (OnConsoleMessage → repassa console JS ao stdout)
           ├─ CefLoadHandler          (OnLoadEnd → m_pageLoaded)
           ├─ CefMessageRouterBrowserSide::Handler  (OnQuery — eventos JS→C++)
           └─ lite::UIRenderer<filament::Renderer>  ★ contrato do core
```

### Implementação do contrato `UIRenderer<filament::Renderer>` (o que o core enxerga)

| Método do core | O que faz aqui |
|---|---|
| `start()` | registra evento interno `ui_ready`; spawna thread CEF; espera `m_cefReady` **e** `m_uiAppReady` (sinal `ui_ready` vindo do React) por até ~5 s. **Thread-agnóstico** — não toca GPU (a main chama) |
| `stop()` | encerra a thread CEF (fecha browser + `CefShutdown`) e chama `destroyFilamentResources()` — invocado pelo `FilamentSceneRenderer` no `cleanup()` da render thread. A parte GPU roda mesmo se `start()` nunca aconteceu |
| `update()` | se `m_needsTextureUpdate`: copia read-buffer → upload-buffer (pré-alocado) e `Texture::setImage` (upload GPU). Chamado **pela própria `Scene`** no início do `update(dt)` (a UI é responsabilidade interna dela) |
| `render(filament::Renderer*)` | `renderer->render(m_view)` — desenha a view da UI **por cima** da cena 3D. Chamado pelo hook `FilamentScene::renderUI()`, dentro do frame aberto |
| `sendInputEvent(const InputEvent&)` | traduz o input agnóstico do core para eventos CEF (§6) |
| `nextElementId` / `registerElement` / `getElement` | herdados da base — registro id→`UIElementHandler` usado pelo `OnQuery` |

### API própria (fora do contrato do core)
- **`createFilamentResources()` / `destroyFilamentResources()`** — criação/destruição dos recursos GPU da UI (§4). **Contrato de threading**: só podem rodar na thread que criou o `Engine`; quem chama é o `FilamentSceneRenderer` (`setup()` e, via `stop()`, o `cleanup()`), nunca a main. Idempotentes (guard `m_filamentResourcesCreated`).
- `loadUrl/loadHtml`, `executeJavaScript(code)`, `executeJavaScriptThrottled(code, minIntervalMs=16)` (throttle para setters frequentes), `resize(w,h)` (realoca buffers + textura + `WasResized` — **atenção**: toca GPU, mesma restrição de thread), `isPageLoaded()`, `getView()`.

URL inicial **hardcoded**: `file:///D:/Workspace/LiteEngine/CEF/ui/resources/cef-ui/dist/index.html` (build Vite do frontend).

## 4. Pipeline de pixels (CEF → tela)

Composição da UI sobre a cena 3D — recursos Filament próprios criados em `createFilamentResources()`:
- `filament::Scene` + `View` **separadas** da cena 3D, com câmera ortográfica (0..1), `BlendMode::TRANSLUCENT`, sem pós-processamento (TODO no código: compartilhar a view da cena e economizar um render pass);
- quad fullscreen (4 vértices POSITION+UV0) com material `ui_overlay.filamat` (path hardcoded) e textura RGBA8 do tamanho da janela.

Fluxo por frame (otimizações numeradas no código):

```
[thread CEF]  OnPaint(buffer BGRA)
   └─ convertBGRAtoRGBA → write-buffer          (sem lock; loop desenrolado 4 px/iteração)
   └─ swap de índices read/write sob m_swapMutex (lock mínimo — double buffering, OTIM. 1)
   └─ m_needsTextureUpdate = true
[render thread]  update()   ← chamado pela Scene no início do frame
   └─ memcpy read-buffer → m_uploadBuffer (pré-alocado, OTIM. 2) sob o mesmo mutex
   └─ Texture::setImage (o Filament copia internamente)
[render thread]  render(renderer)  ← FilamentScene::renderUI() (após a cena 3D)
   └─ renderer->render(m_view)  → quad translúcido por cima do frame
```

A composição é automática: a `Scene` possui o `m_uiRenderer` e o integra ao próprio ciclo de frame (`update()` no início, `renderUI()` após `renderScene`) — a main não registra nada.

## 5. Protocolo C++ ↔ JavaScript

### C++ → JS (construir/atualizar widgets)
Cada widget concreto serializa um **descriptor JSON** (nlohmann/json) e injeta via `executeJavaScript`:

```js
window.liteUI.addElement({ id, type, parentId, line, column, lineSpan, columnSpan, ...props })
window.liteUI.updateElement(id, { ...propsParciais })
// também expostos: removeElement(id), clearElements()
```

`type` ∈ `panel | text | textInput | checkbox | combobox | button | tree`. Props por tipo: `text` (text/textInput), `label` (textInput/button), `checked` (checkbox), `options: [{key,label}]` + `selectedOption` (combobox), `nodes: [{id, label, children}]` (tree, recursivo). `parentId = -1` = raiz. `CEF_UITextElement::setText` usa a variante *throttled* (30 ms) por ser candidata a updates por-frame.

A tree manda **a árvore inteira de uma vez**: cada `createNode`/`updateNode`/`removeNode` bem-sucedido chama `redrawTree()`, que envia um único `updateElement(id, {nodes})`. O `data` de cada nó (o `DataType`) **nunca** é serializado: fica só no C++. Antes do `draw()` o elemento ainda não tem id no CEF, então o `redrawTree()` não envia nada e os nós ficam guardados. O `draw()` manda o `addElement` com `nodes: []` e em seguida chama o `redrawTree()`, que envia a árvore já montada.

### JS → C++ (eventos)
Frontend chama `window.cefQuery({request: JSON.stringify(payload)})` (função injetada pelo MessageRouter). O `OnQuery` no browser process parseia:

| Payload | Efeito |
|---|---|
| `{event: "ui_ready"}` | seta `m_uiAppReady` — destrava o `start()` (enviado pelo React ao montar) |
| `{id, type, value?}` | `m_uiElements[id].invokeEvents<UIRenderer<filament::Renderer>>(type, value)` → dispara os callbacks registrados via `UIElement::registerEvent(type, cb)` |

Nomes de evento em uso: `"click"` (botões; na tree, `{id: <id da tree>, type: 'click', value: '<id do nó>'}` — o id do nó chega como string no `value`, e o clique na seta de expandir/recolher não gera evento), `"changeValue"` (inputs/combos/checkbox — os widgets concretos já registram um handler interno de `changeValue` no construtor para sincronizar o estado C++ e disparar `notifyChange`). O fluxo completo de um clique: DOM `onClick` → `sendToNative({id, type:'click'})` → `cefQuery` → IPC → `OnQuery` → `UIElementHandler::invokeEvents` → `UIElement::invokeEvent("click")` → lambda registrada na main.

## 6. Input (core → CEF)

`sendInputEvent(InputEvent)` traduz o evento agnóstico do core ([core.md §6](../core.md)):
- `analogs[MOUSE]` → `SendMouseMoveEvent`; `analogs[MOUSE_WHEEL]` → `SendMouseWheelEvent`;
- `MOUSE_LEFT/RIGHT/MIDDLE` com estado DOWN/UP → `SendMouseClickEvent`;
- teclas (< 400) → `SendKeyEvent` KEYDOWN/KEYUP via `inputKeyToVirtualKey`; no DOWN também envia `KEYEVENT_CHAR` via `inputKeyToChar` (minúscula sem Shift), **exceto** com Ctrl/Alt ativos (para não digitar "v" num Ctrl+V);
- **modificadores persistentes**: `processModifiers` mantém `m_currentModifiers` (SHIFT/CTRL/ALT) entre frames — tecla ausente no mapa = estado mantido (convenção `INPUT_KEY_STATES::NONE`).

## 7. Frontend — app React (`CEF/ui/resources/cef-ui/`)

React 19 + Vite + TypeScript + react-bootstrap (+ react-router). Build: `npm install && npm run build` → `dist/index.html` (o que o C++ carrega). **Sem rebuild do frontend, mudanças em `.tsx` não aparecem.**

- **`src/engine/uiStore.ts`** — a ponte: mantém `UIElementDescriptor[]` (espelho dos descriptors JSON), expõe `window.liteUI.{addElement,updateElement,removeElement,clearElements}` para o C++ e `sendToNative(data)` (promise sobre `cefQuery`) para os componentes. `addElement` faz upsert por id; mutações chamam `renderCallback` para re-render.
- **`src/engine/UIRoot.tsx`** — renderiza a árvore: filtra filhos por `parentId`; painéis viram CSS **grid** (`gridColumn/gridRow` a partir de `line/column/lineSpan/columnSpan` — mesma semântica do `UIPanelElement` do core); raiz é transparente (a cena 3D aparece atrás), painéis não-raiz viram `Card` bootstrap. Componentes por tipo: Panel/Text/TextInput (debounce 400 ms antes de `sendToNative` com `changeValue`)/Checkbox/Combobox/Button (`type:'click'`)/Tree (`type:'click'` com o id do nó em `value`).
- **Tree** (`TreeComponent`): aparece só como lista, sem card próprio (para ter o visual de card, põe-se a tree dentro de um painel). Os nós começam **recolhidos**; expansão e seleção são estado **local do React** (o C++ não sabe quais nós estão abertos nem qual está destacado). Recuo de `10 + 16 × profundidade` px; estilos `.tree*` em `src/engine/UIRoot.css`, seguindo o painel "Scene Objects" da referência de design do editor. O tipo `UITreeNodeDescriptor` fica em `uiStore.ts`.
- Evento `ui_ready` é enviado na montagem do app — é o handshake que o `start()` C++ espera.

## 8. Ordem de inicialização da UI (quem chama o quê)

```
main: CefExecuteProcess(CEF_UIApp)                      // subprocessos saem aqui
render thread (setup): new CEF_Filament_UIRendererThreaded(engine, w, h)
render thread (setup): uiRenderer->createFilamentResources()  // GPU, na thread do Engine,
                                                              // ANTES do waitReady() destravar
main: uiInstance = new CEF_Filament_UIInstance(uiRenderer)   // só constrói
main: sceneRenderer.start()
main: configurer->configure(scene)                      // desde 2026-09-19 a UI é montada AQUI
        └─ configureUI():
            └─ getUIRenderer() → scene->getCurrentUI()  // o renderer já existe (setup)
            └─ root = uiInstance->start()
                  └─ uiRenderer->start()                // SÓ CEF: thread CEF + espera ui_ready
                  └─ createRoot() → new CEF_UIPanelElement
                  └─ root->draw()                       // painel raiz vai ao DOM
            └─ createPanel/createText/createTextInput/createButton  (factory methods)
            └─ addChildComponent(...) e root->draw()    // redesenha a árvore completa
// update/render da UI: integrados pela própria Scene — nada a registrar na main
```

**Mudança de 2026-09-19**: a montagem da UI saiu da `main` para o `SceneConfigurer` ([core.md §10](../core.md)). Os widgets concretos não são mais instanciados com `new CEF_...` no código de aplicação: o `EditorSceneConfigurer` chama **factory methods** puros (`createPanel`, `createText`, `createTextInput`, `createButton`) que só o `FilamentEditorSceneConfigurer` implementa — é o ponto onde `CEF_UIPanelElement` e companhia aparecem. ⚠️ Esse `configureUI()` **não compila hoje** (identificadores herdados do recorte da `main`) — ver [ARCHITECTURE.md §5](../ARCHITECTURE.md), item 12.

## 9. Dívidas específicas do módulo

- `getText()` de `CEF_UITextElement` retorna `""` (estado só existe no DOM; não há query de volta).
- `isFoccused()` retorna `false` em todos os widgets (não implementado).
- Mapa `cef_events` interno marcado com TODO ("turn in more generic") — só trata `ui_ready`.
- `OnQuery` responde sempre `Success("ok")` e tem TODO de parsing/roteamento mais rico.
- URL do HTML e path do `ui_overlay.filamat` hardcoded.
- UI antiga pré-React (`CEF/ui/resources/index.html|script.js|styles.css`) ainda no repositório — obsoleta.
- `CEF/ui/CEF_Filament_UIInstance.cpp` existe mas está fora do build (classe é header-only).
- `include/CEF/ui/CEF_UIEditor.h` — stub quebrado (headers inexistentes), não incluir.

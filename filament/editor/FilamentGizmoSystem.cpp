#include <filament/editor/FilamentGizmoSystem.h>

#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>
#include <filament/utils/FilamentTransformUtils.h>

#include <filament/TransformManager.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>

#include <SDL.h>
#include <SDL_syswm.h>

#include <iostream>

namespace lite {

FilamentGizmoSystem::FilamentGizmoSystem(filament::Engine* engine, GizmoParts parts, int viewHeight, int viewWidth)
    : GizmoSystem(std::move(parts), viewHeight, viewWidth)
    , m_engine(engine)
{
    // Só guarda dependências — nada de GPU aqui (construído na main thread).
}

FilamentGizmoSystem::~FilamentGizmoSystem() {
    // THREADING: destrói recursos do Engine — precisa rodar na render thread
    // (ver nota de uso no header).
    m_gizmoScene.reset();   // destrói as instâncias e a factory do overlay

    if (!m_engine) return;

    if (!m_rootEntity.isNull()) {
        m_engine->destroy(m_rootEntity);
        utils::EntityManager::get().destroy(m_rootEntity);
    }

    if (m_view) {
        m_engine->destroy(m_view);
        m_view = nullptr;
    }

    if (m_filamentScene) {
        m_engine->destroy(m_filamentScene);
        m_filamentScene = nullptr;
    }
}

bool FilamentGizmoSystem::initializeOverlay() {

    if (!m_engine || !m_liteScene || !m_camera) return false;

    filament::Renderer* renderer = m_liteScene->getFilamentRenderer();
    filament::View* sceneView = m_liteScene->getFilamentView();
    if (!renderer || !sceneView) return false;

    // --- Cena + view próprias: é o que faz o gizmo compor POR CIMA da cena 3D
    // sem ser ocluído por ela, e ficar fora do picking da cena principal.
    m_filamentScene = m_engine->createScene();
    m_view = m_engine->createView();

    m_view->setScene(m_filamentScene);
    // Mesma câmera da cena 3D: nada a sincronizar por frame. m_camera é herdado
    // da base em tipo agnóstico; getFilamentCamera() não é do contrato, então
    // aqui (única classe que sabe o tipo nativo) fazemos o cast.
    m_view->setCamera(
        static_cast<FilamentCameraAsset3dInstance*>(m_camera)->getFilamentCamera());
    m_view->setViewport(sceneView->getViewport());
    // Sem pós-processamento: tone mapping/FXAA lavariam as cores dos eixos.
    m_view->setPostProcessingEnabled(false);
    // TRANSLUCENT: não limpa a cor, mistura com o que já está no swapchain.
    m_view->setBlendMode(filament::View::BlendMode::TRANSLUCENT);
    m_view->setShadowingEnabled(false);
    m_view->setScreenSpaceRefractionEnabled(false);

    // --- Cena de overlay: factory própria apontada para a filament::Scene do
    // gizmo, então tudo que ela instancia já nasce no overlay.
    m_gizmoScene = std::make_unique<FilamentOverlayScene>(
        std::make_unique<FilamentInstanceFactory>(m_engine, m_filamentScene),
        renderer,
        m_filamentScene,
        m_view
    );

    // Picking dos eixos: o selector da base varre a cena de overlay (só as 9
    // peças) usando a mesma câmera da cena 3D.
    attachSelector(m_gizmoScene.get(), m_camera);

    std::cout << "FilamentGizmoSystem: overlay scene/view created" << std::endl;
    return true;
}

std::unique_ptr<MeshAsset3dInstance<FilamentAsset3dTransform>> FilamentGizmoSystem::createRoot() {
    // Só entity + componente de transform: o root não tem geometria, serve de
    // pai comum das 9 peças. A entity fica guardada aqui porque o parentesco
    // (setParent) e a destruição são trabalho do renderer; a posse do nó vai
    // para a base.
    auto& transformManager = m_engine->getTransformManager();
    m_rootEntity = utils::EntityManager::get().create();
    transformManager.create(m_rootEntity);

    auto root = std::make_unique<FilamentMeshAsset3dInstance>(
        m_engine,
        m_gizmoScene.get()->getFilamentScene()
    );
    root->name = "gizmo_root";
    root->initializeTransform(m_rootEntity);

    // Força um setTransform explícito: create() reserva o componente, mas o
    // world transform do Filament só é preenchido num setTransform/commit. Como
    // nada mais move o root, sem isto getWorldMatrix() leria memória não
    // inicializada (0xCCCCCCCC). Escrever identidade preenche local E world.
    root->getTransform()->setLocalMatrix(glm::mat4(1.0f));

    return root;
}

int FilamentGizmoSystem::createPart(const Asset3dData& data,
                                    const std::vector<std::unique_ptr<MaterialData>>& materials) {
    if (!m_gizmoScene) return -1;

    return m_gizmoScene->create(
        data,
        materials,
        TransformUtils<FilamentAsset3dTransform>::build(),
        true    // deepIds: sem isso os meshes das peças ficariam com id -1
    );
}

void FilamentGizmoSystem::updateOverlay(float deltaTime) {
    if (!m_gizmoScene) return;

    // FORA do frame: instancia o que estiver na fila, roda os systems da cena de
    // overlay e faz o flush das deleções. Nada é desenhado aqui — as fases de
    // frame e o renderScene() estão neutralizados em FilamentOverlayScene.
    // Instanciar dentro do frame quebraria o commit das MaterialInstance feito
    // por FEngine::prepare() no beginFrame (ver nota no header da cena).

    m_gizmoScene->update(deltaTime);
}

void FilamentGizmoSystem::renderOverlay() {
    if (!m_view || !m_liteScene) return;

    filament::Renderer* renderer = m_liteScene->getFilamentRenderer();
    if (!renderer) return;

    // Render pass extra dentro do frame já aberto: a view é TRANSLUCENT, então
    // compõe por cima da cena 3D em vez de limpar a cor.
    renderer->render(m_view);
}

void FilamentGizmoSystem::attachPartToRoot(int partId) {
    if (!m_gizmoScene || partId < 0 || m_rootEntity.isNull()) return;

    // Seguro: chamado DEPOIS do update(), então a peça já foi instanciada e o
    // get() retorna sem esperar (esperar aqui seria deadlock — instantiate()
    // roda nesta mesma thread).
    FilamentAsset3dInstance* part = m_gizmoScene->get(partId);
    if (!part) return;

    // O parentesco é feito na hierarquia de transform da Filament, não com
    // Asset3dInstance::addChild — addChild adota a posse do ponteiro, e a peça
    // já pertence à cena de overlay (seriam dois donos).
    auto& transformManager = m_engine->getTransformManager();
    auto partTransform = transformManager.getInstance(part->getEntity());
    auto rootTransform = transformManager.getInstance(m_rootEntity);

    if (partTransform && rootTransform) {
        transformManager.setParent(partTransform, rootTransform);
        //FIXME: PERTÊNCIMENTO "DUPLO" DO PONTEIRO DO PART
        m_root->addChild(part);
    }
}

} // namespace lite

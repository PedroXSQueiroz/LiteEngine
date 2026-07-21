#include <filament/editor/FilamentGizmoSystem.h>

#include <filament/assets/instanceFactory/FilamentInstanceFactory.h>
#include <filament/data/assets/FilamentAsset3dTransform.h>
#include <filament/utils/FilamentTransformUtils.h>

#include <filament/TransformManager.h>
#include <filament/Viewport.h>
#include <utils/EntityManager.h>

#include <iostream>

namespace lite {

FilamentGizmoSystem::FilamentGizmoSystem(filament::Engine* engine, GizmoParts parts)
    : GizmoSystem(std::move(parts))
    , m_engine(engine)
{
    // Só guarda dependências — nada de GPU aqui (construído na main thread).
}

FilamentGizmoSystem::~FilamentGizmoSystem() {
    // THREADING: destrói recursos do Engine — precisa rodar na render thread
    // (ver nota de uso no header).
    m_gizmoScene.reset();   // destrói as instâncias e a factory do overlay
    m_root.reset();

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
    // Mesma câmera da cena 3D: nada a sincronizar por frame.
    m_view->setCamera(m_camera->getFilamentCamera());
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

    // --- Root: só entity + transform. Não passa pela Scene de propósito — a
    // Scene seria dona dele, e o root precisa ser do sistema.
    auto& transformManager = m_engine->getTransformManager();
    m_rootEntity = utils::EntityManager::get().create();
    transformManager.create(m_rootEntity);

    m_root = std::make_unique<FilamentAsset3dInstance>(
        m_rootEntity,
        FilamentAsset3dTransform(transformManager, m_rootEntity)
    );
    m_root->name = "gizmo_root";

    std::cout << "FilamentGizmoSystem: overlay scene/view created" << std::endl;
    return true;
}

int FilamentGizmoSystem::createPart(const Asset3dData& data,
                                    const std::vector<MaterialData>& materials) {
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
    }
}

} // namespace lite

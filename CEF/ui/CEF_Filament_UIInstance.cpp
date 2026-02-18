#include <CEF/ui/CEF_Filament_UIInstance.h>
#include <CEF/ui/elements/CEF_UIElements.h>

namespace lite {

UIElement<UIInstance<CEF_Filament_UIRendererThreaded>>*
CEF_Filament_UIInstance::getElementByIdFromRenderer(int id) {
    return nullptr;
}

UIPanelElement<UIInstance<CEF_Filament_UIRendererThreaded>>*
CEF_Filament_UIInstance::createRoot() {
    return new CEF_UIPanelElement(this);
}

} // namespace lite

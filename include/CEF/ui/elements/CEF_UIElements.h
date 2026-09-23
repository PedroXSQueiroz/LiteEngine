#pragma once

#include <core/ui/elements/UIElements.h>
#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>

// Filament macros collide with nlohmann/json internals; restored right after the include
#pragma push_macro("assert_invariant")
#pragma push_macro("UTILS_VERY_LIKELY")
#undef assert_invariant
#undef UTILS_VERY_LIKELY
#include <nlohmann/json.hpp>
#pragma pop_macro("UTILS_VERY_LIKELY")
#pragma pop_macro("assert_invariant")

namespace lite {

// --- Panel ---
class CEF_UIPanelElement: public UIPanelElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UIPanelElement(CEF_Filament_UIRendererThreaded* renderer): UIPanelElement(renderer) {};
    virtual int drawContainer(int parentId, int line, int column, int lineSpan = 1, int columnSpan = 1) override;
    virtual bool isFoccused() override;
};

// --- Text ---
class CEF_UITextElement : public UITextElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UITextElement(CEF_Filament_UIRendererThreaded* renderer): UITextElement(renderer) {};
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan = 1, int columnSpan = 1) override;
    virtual bool setText(std::string text) override;
    virtual std::string getText() override;
};

// --- CheckBox ---
class CEF_UICheckBoxElement : public UICheckBoxElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UICheckBoxElement(CEF_Filament_UIRendererThreaded* renderer): UICheckBoxElement(renderer) {};
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan = 1, int columnSpan = 1) override;
    virtual bool isChecked() override;
    virtual bool setChecked(bool check) override;
private:
    bool m_checked = false;
};

// --- ComboBox ---
class CEF_UIComboBoxInputElement : public UIComboBoxInputElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UIComboBoxInputElement(CEF_Filament_UIRendererThreaded* renderer): UIComboBoxInputElement(renderer) {
        this->registerEvent("changeValue", [this](CEF_Filament_UIRendererThreaded*, int, std::string value) {
            this->setSelectedOption(value);
        });
    };
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan = 1, int columnSpan = 1) override;
    virtual bool addOption(std::string key, std::string label) override;
    virtual std::string getSelectedOption() override;
protected:
    virtual bool updateInput(std::string key) override;
private:
    std::string m_selectedKey;
    std::vector<std::pair<std::string, std::string>> m_options;
};

// --- TextInput ---
class CEF_UITextInputElement : public UITextInputElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UITextInputElement(CEF_Filament_UIRendererThreaded* renderer, std::string label): UITextInputElement(renderer, label) {
        this->registerEvent("changeValue", [this](CEF_Filament_UIRendererThreaded*, int, std::string value) {
            this->m_text = value;
            this->notifyChange(value);
        });
    };
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan = 1, int columnSpan = 1) override;
    virtual std::string getText() override;
protected:
    virtual bool updateInput(std::string text) override;
private:
    std::string m_text;
};

// --- Button ---
class CEF_UIButtonElement : public UIButtonElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UIButtonElement(CEF_Filament_UIRendererThreaded* renderer, const std::string& label = "Button")
        : UIButtonElement(renderer), m_label(label) {};
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan = 1, int columnSpan = 1) override;
private:
    std::string m_label;
};

// --- Tree ---
template<typename DataType>
class CEF_UITreeElement : public UITreeElement<CEF_Filament_UIRendererThreaded, DataType> {
public:
    using UITreeComponentNode = typename UITreeElement<CEF_Filament_UIRendererThreaded, DataType>::UITreeComponentNode;

    CEF_UITreeElement(CEF_Filament_UIRendererThreaded* renderer)
        : UITreeElement<CEF_Filament_UIRendererThreaded, DataType>(renderer) {};

    virtual bool isFoccused() override { return false; }

    virtual int draw(int parentId, int line, int column, int lineSpan = 1, int columnSpan = 1) override {

        UIElement<CEF_Filament_UIRendererThreaded>::draw(parentId, line, column, lineSpan, columnSpan);

        nlohmann::json j = {
            {"id", this->m_currentId},
            {"type", "tree"},
            {"parentId", parentId},
            {"line", line},
            {"column", column},
            {"lineSpan", lineSpan},
            {"columnSpan", columnSpan},
            {"nodes", nlohmann::json::array()}
        };

        this->m_uiRenderer->executeJavaScript("window.liteUI.addElement(" + j.dump() + ")");

        // Nos criados antes do draw() vao agora, de uma vez
        this->redrawTree();

        return this->m_currentId;
    }

protected:

    virtual void redrawTree() override {

        // Ainda sem id no CEF: os nos ficam guardados e o draw() os envia
        if (this->m_currentId == UIElement<CEF_Filament_UIRendererThreaded>::EMPTY_ELEMENT_ID) return;

        // So id, label e children vao para o React; data fica no C++
        auto toJson = [](auto& self, const std::vector<UITreeComponentNode>& nodes) -> nlohmann::json {
            nlohmann::json nodesArr = nlohmann::json::array();
            for (const UITreeComponentNode& node : nodes) {
                nlohmann::json nodeJson = {
                    {"id", node.id},
                    {"label", node.label},
                    {"children", self(self, node.children)}
                };
                nodesArr.push_back(nodeJson);
            }
            return nodesArr;
        };

        nlohmann::json j = {{"nodes", toJson(toJson, this->m_nodes)}};
        this->m_uiRenderer->executeJavaScript(
            "window.liteUI.updateElement(" + std::to_string(this->m_currentId) + "," + j.dump() + ")");
    }
};

} // namespace lite

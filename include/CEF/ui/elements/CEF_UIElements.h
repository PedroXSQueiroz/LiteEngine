#pragma once

#include <core/ui/elements/UIElements.h>
#include <CEF/ui/CEF_Filament_UIRendererThreaded.h>

namespace lite {

// --- Panel ---
class CEF_UIPanelElement: public UIPanelElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UIPanelElement(CEF_Filament_UIRendererThreaded* renderer): UIPanelElement(renderer) {};
    virtual int drawContainer(int parentId, int line, int column, int lineSpan, int columnSpan) override;
    virtual bool isFoccused() override;
};

// --- Text ---
class CEF_UITextElement : public UITextElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UITextElement(CEF_Filament_UIRendererThreaded* renderer): UITextElement(renderer) {};
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan, int columnSpan) override;
    virtual bool setText(std::string text) override;
    virtual std::string getText() override;
};

// --- CheckBox ---
class CEF_UICheckBoxElement : public UICheckBoxElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UICheckBoxElement(CEF_Filament_UIRendererThreaded* renderer): UICheckBoxElement(renderer) {};
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan, int columnSpan) override;
    virtual bool isChecked() override;
    virtual bool setChecked(bool check) override;
private:
    bool m_checked = false;
};

// --- ComboBox ---
class CEF_UIComboBoxInputElement : public UIComboBoxInputElement<CEF_Filament_UIRendererThreaded> {
public:
    CEF_UIComboBoxInputElement(CEF_Filament_UIRendererThreaded* renderer): UIComboBoxInputElement(renderer) {};
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan, int columnSpan) override;
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
    CEF_UITextInputElement(CEF_Filament_UIRendererThreaded* renderer): UITextInputElement(renderer) {};
    virtual bool isFoccused() override;
    virtual int draw(int parentId, int line, int column, int lineSpan, int columnSpan) override;
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
    virtual int draw(int parentId, int line, int column, int lineSpan, int columnSpan) override;
private:
    std::string m_label;
};

} // namespace lite

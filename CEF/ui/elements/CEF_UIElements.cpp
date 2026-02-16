#include <CEF/ui/elements/CEF_UIElements.h>

#undef assert_invariant
#undef UTILS_VERY_LIKELY
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace lite {

// ==================== Panel ====================
int CEF_UIPanelElement::drawContainer(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    json j = {
        {"id", this->m_currentId},
        {"type", "panel"},
        {"parentId", parentId},
        {"line", line},
        {"column", column},
        {"lineSpan", lineSpan},
        {"columnSpan", columnSpan}
    };

    this->m_uiRenderer->executeJavaScript("window.liteUI.addElement(" + j.dump() + ")");
    return this->m_currentId;
}

bool CEF_UIPanelElement::isFoccused() { return false; }

// ==================== Text ====================
int CEF_UITextElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    json j = {
        {"id", this->m_currentId},
        {"type", "text"},
        {"parentId", parentId},
        {"line", line},
        {"column", column},
        {"lineSpan", lineSpan},
        {"columnSpan", columnSpan},
        {"text", ""}
    };

    this->m_uiRenderer->executeJavaScript("window.liteUI.addElement(" + j.dump() + ")");
    return this->m_currentId;
}

bool CEF_UITextElement::setText(std::string text) {
    json j = {{"text", text}};
    this->m_uiRenderer->executeJavaScriptThrottled(
        "window.liteUI.updateElement(" + std::to_string(this->m_currentId) + "," + j.dump() + ")", 30);
    return true;
}

std::string CEF_UITextElement::getText() { return ""; }
bool CEF_UITextElement::isFoccused() { return false; }

// ==================== CheckBox ====================
int CEF_UICheckBoxElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    json j = {
        {"id", this->m_currentId},
        {"type", "checkbox"},
        {"parentId", parentId},
        {"line", line},
        {"column", column},
        {"lineSpan", lineSpan},
        {"columnSpan", columnSpan},
        {"checked", false}
    };

    this->m_uiRenderer->executeJavaScript("window.liteUI.addElement(" + j.dump() + ")");
    return this->m_currentId;
}

bool CEF_UICheckBoxElement::isChecked() { return m_checked; }

bool CEF_UICheckBoxElement::setChecked(bool check) {
    m_checked = check;
    json j = {{"checked", check}};
    this->m_uiRenderer->executeJavaScript(
        "window.liteUI.updateElement(" + std::to_string(this->m_currentId) + "," + j.dump() + ")");
    return true;
}

bool CEF_UICheckBoxElement::isFoccused() { return false; }

// ==================== ComboBox ====================
int CEF_UIComboBoxInputElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    json j = {
        {"id", this->m_currentId},
        {"type", "combobox"},
        {"parentId", parentId},
        {"line", line},
        {"column", column},
        {"lineSpan", lineSpan},
        {"columnSpan", columnSpan},
        {"options", json::array()},
        {"selectedOption", ""}
    };

    this->m_uiRenderer->executeJavaScript("window.liteUI.addElement(" + j.dump() + ")");
    return this->m_currentId;
}

bool CEF_UIComboBoxInputElement::addOption(std::string key, std::string label) {
    m_options.push_back({key, label});

    json optionsArr = json::array();
    for (const auto& opt : m_options) {
        optionsArr.push_back({{"key", opt.first}, {"label", opt.second}});
    }

    json j = {{"options", optionsArr}};
    this->m_uiRenderer->executeJavaScript(
        "window.liteUI.updateElement(" + std::to_string(this->m_currentId) + "," + j.dump() + ")");
    return true;
}

std::string CEF_UIComboBoxInputElement::getSelectedOption() {
    return m_selectedKey;
}

bool CEF_UIComboBoxInputElement::updateInput(std::string key) {
    m_selectedKey = key;
    json j = {{"selectedOption", key}};
    this->m_uiRenderer->executeJavaScript(
        "window.liteUI.updateElement(" + std::to_string(this->m_currentId) + "," + j.dump() + ")");
    return true;
}

bool CEF_UIComboBoxInputElement::isFoccused() { return false; }

// ==================== TextInput ====================
int CEF_UITextInputElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    json j = {
        {"id", this->m_currentId},
        {"type", "textInput"},
        {"parentId", parentId},
        {"line", line},
        {"column", column},
        {"lineSpan", lineSpan},
        {"columnSpan", columnSpan},
        {"text", ""}
    };

    this->m_uiRenderer->executeJavaScript("window.liteUI.addElement(" + j.dump() + ")");
    return this->m_currentId;
}

std::string CEF_UITextInputElement::getText() { return m_text; }

bool CEF_UITextInputElement::updateInput(std::string text) {
    m_text = text;
    json j = {{"text", text}};
    this->m_uiRenderer->executeJavaScript(
        "window.liteUI.updateElement(" + std::to_string(this->m_currentId) + "," + j.dump() + ")");
    return true;
}

bool CEF_UITextInputElement::isFoccused() { return false; }

// ==================== Button ====================
int CEF_UIButtonElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    json j = {
        {"id", this->m_currentId},
        {"type", "button"},
        {"parentId", parentId},
        {"line", line},
        {"column", column},
        {"lineSpan", lineSpan},
        {"columnSpan", columnSpan},
        {"label", m_label}
    };

    this->m_uiRenderer->executeJavaScript("window.liteUI.addElement(" + j.dump() + ")");
    return this->m_currentId;
}

bool CEF_UIButtonElement::isFoccused() { return false; }

} // namespace lite

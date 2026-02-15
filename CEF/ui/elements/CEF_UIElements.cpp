#include <CEF/ui/elements/CEF_UIElements.h>
#include <sstream>

namespace lite {

// Helper para escapar strings para JS
static std::string jsEscape(const std::string& s) {
    std::string result;
    for (char c : s) {
        if (c == '\'' || c == '\\') result += '\\';
        result += c;
    }
    return result;
}

// ==================== Panel ====================
int CEF_UIPanelElement::drawContainer(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    std::ostringstream js;
    js << "window.liteUI.addElement({"
       << "id:" << this->m_currentId
       << ",type:'panel'"
       << ",parentId:" << parentId
       << ",line:" << line
       << ",column:" << column
       << ",lineSpan:" << lineSpan
       << ",columnSpan:" << columnSpan
       << "})";

    this->m_uiRenderer->executeJavaScript(js.str());
    return this->m_currentId;
}

bool CEF_UIPanelElement::isFoccused() { return false; }

// ==================== Text ====================
int CEF_UITextElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    std::ostringstream js;
    js << "window.liteUI.addElement({"
       << "id:" << this->m_currentId
       << ",type:'text'"
       << ",parentId:" << parentId
       << ",line:" << line
       << ",column:" << column
       << ",lineSpan:" << lineSpan
       << ",columnSpan:" << columnSpan
       << ",text:''"
       << "})";

    this->m_uiRenderer->executeJavaScript(js.str());
    return this->m_currentId;
}

bool CEF_UITextElement::setText(std::string text) {
    std::ostringstream js;
    js << "window.liteUI.updateElement(" << this->m_currentId
       << ",{text:'" << jsEscape(text) << "'})";
    this->m_uiRenderer->executeJavaScriptThrottled(js.str(), 30);
    return true;
}

std::string CEF_UITextElement::getText() { return ""; }
bool CEF_UITextElement::isFoccused() { return false; }

// ==================== CheckBox ====================
int CEF_UICheckBoxElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    std::ostringstream js;
    js << "window.liteUI.addElement({"
       << "id:" << this->m_currentId
       << ",type:'checkbox'"
       << ",parentId:" << parentId
       << ",line:" << line
       << ",column:" << column
       << ",lineSpan:" << lineSpan
       << ",columnSpan:" << columnSpan
       << ",checked:false"
       << "})";

    this->m_uiRenderer->executeJavaScript(js.str());
    return this->m_currentId;
}

bool CEF_UICheckBoxElement::isChecked() { return m_checked; }

bool CEF_UICheckBoxElement::setChecked(bool check) {
    m_checked = check;
    std::ostringstream js;
    js << "window.liteUI.updateElement(" << this->m_currentId
       << ",{checked:" << (check ? "true" : "false") << "})";
    this->m_uiRenderer->executeJavaScript(js.str());
    return true;
}

bool CEF_UICheckBoxElement::isFoccused() { return false; }

// ==================== ComboBox ====================
int CEF_UIComboBoxInputElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    std::ostringstream js;
    js << "window.liteUI.addElement({"
       << "id:" << this->m_currentId
       << ",type:'combobox'"
       << ",parentId:" << parentId
       << ",line:" << line
       << ",column:" << column
       << ",lineSpan:" << lineSpan
       << ",columnSpan:" << columnSpan
       << ",options:[]"
       << ",selectedOption:''"
       << "})";

    this->m_uiRenderer->executeJavaScript(js.str());
    return this->m_currentId;
}

bool CEF_UIComboBoxInputElement::addOption(std::string key, std::string label) {
    m_options.push_back({key, label});

    std::ostringstream optionsJs;
    optionsJs << "[";
    for (size_t i = 0; i < m_options.size(); i++) {
        if (i > 0) optionsJs << ",";
        optionsJs << "{key:'" << jsEscape(m_options[i].first)
                  << "',label:'" << jsEscape(m_options[i].second) << "'}";
    }
    optionsJs << "]";

    std::ostringstream js;
    js << "window.liteUI.updateElement(" << this->m_currentId
       << ",{options:" << optionsJs.str() << "})";
    this->m_uiRenderer->executeJavaScript(js.str());
    return true;
}

std::string CEF_UIComboBoxInputElement::getSelectedOption() {
    return m_selectedKey;
}

bool CEF_UIComboBoxInputElement::updateInput(std::string key) {
    m_selectedKey = key;
    std::ostringstream js;
    js << "window.liteUI.updateElement(" << this->m_currentId
       << ",{selectedOption:'" << jsEscape(key) << "'})";
    this->m_uiRenderer->executeJavaScript(js.str());
    return true;
}

bool CEF_UIComboBoxInputElement::isFoccused() { return false; }

// ==================== TextInput ====================
int CEF_UITextInputElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    std::ostringstream js;
    js << "window.liteUI.addElement({"
       << "id:" << this->m_currentId
       << ",type:'textInput'"
       << ",parentId:" << parentId
       << ",line:" << line
       << ",column:" << column
       << ",lineSpan:" << lineSpan
       << ",columnSpan:" << columnSpan
       << ",text:''"
       << "})";

    this->m_uiRenderer->executeJavaScript(js.str());
    return this->m_currentId;
}

std::string CEF_UITextInputElement::getText() { return m_text; }

bool CEF_UITextInputElement::updateInput(std::string text) {
    m_text = text;
    std::ostringstream js;
    js << "window.liteUI.updateElement(" << this->m_currentId
       << ",{text:'" << jsEscape(text) << "'})";
    this->m_uiRenderer->executeJavaScript(js.str());
    return true;
}

bool CEF_UITextInputElement::isFoccused() { return false; }

// ==================== Button ====================
int CEF_UIButtonElement::draw(int parentId, int line, int column, int lineSpan, int columnSpan) {
    this->m_parentId = parentId;

    std::ostringstream js;
    js << "window.liteUI.addElement({"
       << "id:" << this->m_currentId
       << ",type:'button'"
       << ",parentId:" << parentId
       << ",line:" << line
       << ",column:" << column
       << ",lineSpan:" << lineSpan
       << ",columnSpan:" << columnSpan
       << ",label:'" << jsEscape(m_label) << "'"
       << "})";

    this->m_uiRenderer->executeJavaScript(js.str());
    return this->m_currentId;
}

bool CEF_UIButtonElement::isFoccused() { return false; }

} // namespace lite

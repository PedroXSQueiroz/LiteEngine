#include <core/input/InputEnums.h>

namespace lite {

char inputKeyToChar(INPUT_KEYS key) {
    uint16_t val = static_cast<uint16_t>(key);
    if (val >= 32 && val < 127) return static_cast<char>(val);
    return 0;
}

int inputKeyToVirtualKey(INPUT_KEYS key) {
    uint16_t val = static_cast<uint16_t>(key);
    if (val >= 'A' && val <= 'Z') return val;
    if (val >= '0' && val <= '9') return val;
    if (key == INPUT_KEYS::KEY_SPACE)     return 0x20;
    if (key == INPUT_KEYS::KEY_ENTER)     return 0x0D;
    if (key == INPUT_KEYS::KEY_TAB)       return 0x09;
    if (key == INPUT_KEYS::KEY_BACKSPACE) return 0x08;
    if (key == INPUT_KEYS::KEY_ESCAPE)    return 0x1B;
    if (key == INPUT_KEYS::KEY_LSHIFT || key == INPUT_KEYS::KEY_RSHIFT) return 0x10;
    if (key == INPUT_KEYS::KEY_LCTRL  || key == INPUT_KEYS::KEY_RCTRL)  return 0x11;
    if (key == INPUT_KEYS::KEY_LALT   || key == INPUT_KEYS::KEY_RALT)   return 0x12;
    if (key == INPUT_KEYS::KEY_CAPSLOCK)  return 0x14;
    if (key == INPUT_KEYS::KEY_UP)        return 0x26;
    if (key == INPUT_KEYS::KEY_DOWN)      return 0x28;
    if (key == INPUT_KEYS::KEY_LEFT)      return 0x25;
    if (key == INPUT_KEYS::KEY_RIGHT)     return 0x27;
    if (key == INPUT_KEYS::KEY_INSERT)    return 0x2D;
    if (key == INPUT_KEYS::KEY_DELETE)    return 0x2E;
    if (key == INPUT_KEYS::KEY_HOME)      return 0x24;
    if (key == INPUT_KEYS::KEY_END)       return 0x23;
    if (key == INPUT_KEYS::KEY_PAGEUP)    return 0x21;
    if (key == INPUT_KEYS::KEY_PAGEDOWN)  return 0x22;
    if (key == INPUT_KEYS::KEY_NUMLOCK)   return 0x90;
    if (key == INPUT_KEYS::KEY_COMMA)     return 0xBC;
    if (key == INPUT_KEYS::KEY_PERIOD)    return 0xBE;
    if (key == INPUT_KEYS::KEY_SLASH)     return 0xBF;
    if (key == INPUT_KEYS::KEY_SEMICOLON) return 0xBA;
    if (key == INPUT_KEYS::KEY_APOSTROPHE)return 0xDE;
    if (key == INPUT_KEYS::KEY_LBRACKET)  return 0xDB;
    if (key == INPUT_KEYS::KEY_RBRACKET)  return 0xDD;
    if (key == INPUT_KEYS::KEY_BACKSLASH) return 0xDC;
    if (key == INPUT_KEYS::KEY_MINUS)     return 0xBD;
    if (key == INPUT_KEYS::KEY_EQUALS)    return 0xBB;
    if (key == INPUT_KEYS::KEY_BACKTICK)  return 0xC0;
    // F1-F12
    if (val >= static_cast<uint16_t>(INPUT_KEYS::KEY_F1) &&
        val <= static_cast<uint16_t>(INPUT_KEYS::KEY_F12))
        return 0x70 + (val - static_cast<uint16_t>(INPUT_KEYS::KEY_F1));
    // Numpad 0-9
    if (val >= static_cast<uint16_t>(INPUT_KEYS::KEY_KP_0) &&
        val <= static_cast<uint16_t>(INPUT_KEYS::KEY_KP_9))
        return 0x60 + (val - static_cast<uint16_t>(INPUT_KEYS::KEY_KP_0));
    if (key == INPUT_KEYS::KEY_KP_ENTER)    return 0x0D;
    if (key == INPUT_KEYS::KEY_KP_PLUS)     return 0x6B;
    if (key == INPUT_KEYS::KEY_KP_MINUS)    return 0x6D;
    if (key == INPUT_KEYS::KEY_KP_MULTIPLY) return 0x6A;
    if (key == INPUT_KEYS::KEY_KP_DIVIDE)   return 0x6F;
    if (key == INPUT_KEYS::KEY_KP_PERIOD)   return 0x6E;
    return 0;
}

} // namespace lite

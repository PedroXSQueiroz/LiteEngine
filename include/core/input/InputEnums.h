#pragma once
#include <cstdint>

namespace lite {

enum class INPUT_KEYS : uint16_t {
    // Letras (valores ASCII para parse fácil)
    KEY_A = 'A', KEY_B = 'B', KEY_C = 'C', KEY_D = 'D',
    KEY_E = 'E', KEY_F = 'F', KEY_G = 'G', KEY_H = 'H',
    KEY_I = 'I', KEY_J = 'J', KEY_K = 'K', KEY_L = 'L',
    KEY_M = 'M', KEY_N = 'N', KEY_O = 'O', KEY_P = 'P',
    KEY_Q = 'Q', KEY_R = 'R', KEY_S = 'S', KEY_T = 'T',
    KEY_U = 'U', KEY_V = 'V', KEY_W = 'W', KEY_X = 'X',
    KEY_Y = 'Y', KEY_Z = 'Z',

    // Números
    KEY_0 = '0', KEY_1 = '1', KEY_2 = '2', KEY_3 = '3',
    KEY_4 = '4', KEY_5 = '5', KEY_6 = '6', KEY_7 = '7',
    KEY_8 = '8', KEY_9 = '9',

    // Símbolos e whitespace
    KEY_SPACE = ' ',
    KEY_ENTER = '\r',
    KEY_TAB = '\t',
    KEY_BACKSPACE = '\b',
    KEY_COMMA = ',',
    KEY_PERIOD = '.',
    KEY_SLASH = '/',
    KEY_SEMICOLON = ';',
    KEY_APOSTROPHE = '\'',
    KEY_LBRACKET = '[',
    KEY_RBRACKET = ']',
    KEY_BACKSLASH = '\\',
    KEY_MINUS = '-',
    KEY_EQUALS = '=',
    KEY_BACKTICK = '`',

    // Teclas especiais (fora do range ASCII)
    KEY_ESCAPE = 256,
    KEY_LSHIFT, KEY_RSHIFT,
    KEY_LCTRL, KEY_RCTRL,
    KEY_LALT, KEY_RALT,
    KEY_CAPSLOCK,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_INSERT, KEY_DELETE, KEY_HOME, KEY_END,
    KEY_PAGEUP, KEY_PAGEDOWN,
    KEY_PRINTSCREEN, KEY_SCROLLLOCK, KEY_PAUSE,
    KEY_NUMLOCK,
    KEY_KP_0, KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4,
    KEY_KP_5, KEY_KP_6, KEY_KP_7, KEY_KP_8, KEY_KP_9,
    KEY_KP_ENTER, KEY_KP_PLUS, KEY_KP_MINUS,
    KEY_KP_MULTIPLY, KEY_KP_DIVIDE, KEY_KP_PERIOD,

    // Mouse buttons
    MOUSE_LEFT = 400, MOUSE_RIGHT, MOUSE_MIDDLE,

    // Controller buttons
    GAMEPAD_A = 500, GAMEPAD_B, GAMEPAD_X, GAMEPAD_Y,
    GAMEPAD_LB, GAMEPAD_RB, GAMEPAD_LT, GAMEPAD_RT,
    GAMEPAD_START, GAMEPAD_SELECT,
    GAMEPAD_LSTICK_PRESS, GAMEPAD_RSTICK_PRESS,
    GAMEPAD_DPAD_UP, GAMEPAD_DPAD_DOWN,
    GAMEPAD_DPAD_LEFT, GAMEPAD_DPAD_RIGHT,

    KEY_UNKNOWN = 0
};

inline char inputKeyToChar(INPUT_KEYS key) {
    uint16_t val = static_cast<uint16_t>(key);
    if (val >= 32 && val < 127) return static_cast<char>(val);
    return 0;
}

inline int inputKeyToVirtualKey(INPUT_KEYS key) {
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

enum class INPUT_KEY_STATES : uint8_t {
    NONE,
    DOWN,
    PRESSED,
    UP
};

enum class INPUT_ANALOGS : uint8_t {
    MOUSE,
    MOUSE_WHEEL,
    GAMEPAD_LSTICK,
    GAMEPAD_RSTICK
};

} // namespace lite

// Hash specializations para usar em unordered_map
namespace std {
template<> struct hash<lite::INPUT_KEYS> {
    size_t operator()(lite::INPUT_KEYS k) const noexcept {
        return hash<uint16_t>()(static_cast<uint16_t>(k));
    }
};
template<> struct hash<lite::INPUT_ANALOGS> {
    size_t operator()(lite::INPUT_ANALOGS a) const noexcept {
        return hash<uint8_t>()(static_cast<uint8_t>(a));
    }
};
} // namespace std

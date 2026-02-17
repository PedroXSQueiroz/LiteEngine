#pragma once
#include <cstdint>
#include <functional>

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

char inputKeyToChar(INPUT_KEYS key);
int inputKeyToVirtualKey(INPUT_KEYS key);

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

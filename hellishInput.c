#include <SDL3/SDL.h>
#include "quakedef.h"

void HellishInput(void) {
    const bool* keyboard = SDL_GetKeyboardState(NULL);

    Key_Event(K_ESCAPE, keyboard[SDL_SCANCODE_ESCAPE]);
    Key_Event(K_ENTER, keyboard[SDL_SCANCODE_RETURN]);
    Key_Event(K_BACKSPACE, keyboard[SDL_SCANCODE_BACKSPACE]);
    Key_Event(K_DEL, keyboard[SDL_SCANCODE_DELETE]);
    Key_Event(K_TAB, keyboard[SDL_SCANCODE_TAB]);
    Key_Event(K_ALT, keyboard[SDL_SCANCODE_LALT] || keyboard[SDL_SCANCODE_RALT]);
    Key_Event(K_CTRL, keyboard[SDL_SCANCODE_LCTRL] || keyboard[SDL_SCANCODE_RCTRL]);
    Key_Event(K_SHIFT, keyboard[SDL_SCANCODE_LSHIFT] || keyboard[SDL_SCANCODE_RSHIFT]);
    Key_Event(K_SPACE, keyboard[SDL_SCANCODE_SPACE]);
    Key_Event(K_UPARROW, keyboard[SDL_SCANCODE_UP]);
    Key_Event(K_DOWNARROW, keyboard[SDL_SCANCODE_DOWN]);
    Key_Event(K_LEFTARROW, keyboard[SDL_SCANCODE_LEFT]);
    Key_Event(K_RIGHTARROW, keyboard[SDL_SCANCODE_RIGHT]);
    Key_Event(K_F1, keyboard[SDL_SCANCODE_F1]);
    Key_Event(K_F2, keyboard[SDL_SCANCODE_F2]);
    Key_Event(K_F3, keyboard[SDL_SCANCODE_F3]);
    Key_Event(K_F4, keyboard[SDL_SCANCODE_F4]);
    Key_Event(K_F5, keyboard[SDL_SCANCODE_F5]);
    Key_Event(K_F6, keyboard[SDL_SCANCODE_F6]);
    Key_Event(K_F7, keyboard[SDL_SCANCODE_F7]);
    Key_Event(K_F8, keyboard[SDL_SCANCODE_F8]);
    Key_Event(K_F9, keyboard[SDL_SCANCODE_F9]);
    Key_Event(K_F10, keyboard[SDL_SCANCODE_F10]);
    Key_Event(K_F11, keyboard[SDL_SCANCODE_F11]);
    Key_Event(K_F12, keyboard[SDL_SCANCODE_F12]);
    Key_Event('a', keyboard[SDL_SCANCODE_A]);
    Key_Event('b', keyboard[SDL_SCANCODE_B]);
    Key_Event('c', keyboard[SDL_SCANCODE_C]);
    Key_Event('d', keyboard[SDL_SCANCODE_D]);
    Key_Event('e', keyboard[SDL_SCANCODE_E]);
    Key_Event('f', keyboard[SDL_SCANCODE_F]);
    Key_Event('g', keyboard[SDL_SCANCODE_G]);
    Key_Event('h', keyboard[SDL_SCANCODE_H]);
    Key_Event('i', keyboard[SDL_SCANCODE_I]);
    Key_Event('j', keyboard[SDL_SCANCODE_J]);
    Key_Event('k', keyboard[SDL_SCANCODE_K]);
    Key_Event('l', keyboard[SDL_SCANCODE_L]);
    Key_Event('m', keyboard[SDL_SCANCODE_M]);
    Key_Event('n', keyboard[SDL_SCANCODE_N]);
    Key_Event('o', keyboard[SDL_SCANCODE_O]);
    Key_Event('p', keyboard[SDL_SCANCODE_P]);
    Key_Event('q', keyboard[SDL_SCANCODE_Q]);
    Key_Event('r', keyboard[SDL_SCANCODE_R]);
    Key_Event('s', keyboard[SDL_SCANCODE_S]);
    Key_Event('t', keyboard[SDL_SCANCODE_T]);
    Key_Event('u', keyboard[SDL_SCANCODE_U]);
    Key_Event('v', keyboard[SDL_SCANCODE_V]);
    Key_Event('w', keyboard[SDL_SCANCODE_W]);
    Key_Event('x', keyboard[SDL_SCANCODE_X]);
    Key_Event('y', keyboard[SDL_SCANCODE_Y]);
    Key_Event('z', keyboard[SDL_SCANCODE_Z]);
    Key_Event('0', keyboard[SDL_SCANCODE_0]);
    Key_Event('1', keyboard[SDL_SCANCODE_1]);
    Key_Event('2', keyboard[SDL_SCANCODE_2]);
    Key_Event('3', keyboard[SDL_SCANCODE_3]);
    Key_Event('4', keyboard[SDL_SCANCODE_4]);
    Key_Event('5', keyboard[SDL_SCANCODE_5]);
    Key_Event('6', keyboard[SDL_SCANCODE_6]);
    Key_Event('7', keyboard[SDL_SCANCODE_7]);
    Key_Event('8', keyboard[SDL_SCANCODE_8]);
    Key_Event('9', keyboard[SDL_SCANCODE_9]);
}

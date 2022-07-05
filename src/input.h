#pragma once

#include "defines.h"
#include "my_math.h"

enum KeyState : u8
{
    KEY_STATE_UP,
    KEY_STATE_DOWN
};

struct Key
{
    u8 halfTransitionCount;
    u8 keyState;
};

struct InputState
{
    Vec2 clickMousePos;
    Vec2 oldMousePos;
    Vec2 mousePos;
    Vec2 relMouse;
    Vec2 screenSize;

    s32 wheelDelta;
    Key keys[255];
};

// TODO: Think about how to handle keys and actions
bool key_pressed_this_frame(InputState *input, s32 keyType);
bool key_released_this_frame(InputState *input, s32 keyType);
bool key_is_down(InputState *input, s32 keyType);
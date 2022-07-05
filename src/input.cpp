bool key_pressed_this_frame(InputState *input, s32 keyType)
{
    Key *key = &input->keys[keyType];
    bool result = ((key->keyState == KEY_STATE_DOWN) && (key->halfTransitionCount > 0));
    return result;
}

bool key_released_this_frame(InputState *input, s32 keyType)
{
    Key *key = &input->keys[keyType];
    bool result = ((key->keyState == KEY_STATE_UP) && (key->halfTransitionCount > 0));
    return result;
}

bool key_is_down(InputState *input, s32 keyType)
{
    Key *key = &input->keys[keyType];
    return key->keyState & KEY_STATE_DOWN;
}
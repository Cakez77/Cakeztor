#include "defines.h"
#include "input.h"

u32 constexpr MAX_BUFFER_LENGTH = MB(1);

struct AppState
{
    uint32_t charCount;
    //TODO: Replace with MAX_BUFFER_LENGTH
    unsigned char buffer[1000];
};

internal void update_app(AppState* app, InputState* input)
{
    for(u8 keyIdx = 0; keyIdx < 255; keyIdx++)
    {
        if(key_pressed_this_frame(input, keyIdx))
        {
            if(keyIdx == 8)
            {
                if(app->charCount > 0)
                {
                    app->buffer[--app->charCount] = 0;
                }
            }
            else
            {
               app->buffer[app->charCount++] = (char)keyIdx;
            }
        }
    }
}
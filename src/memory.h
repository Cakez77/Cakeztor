#pragma once

#include "defines.h"
#include "logger.h"

struct GameMemory
{
    u8 *memory;
    u32 allocatedBytes;
    u32 memorySizeInBytes;
};

//TODO: Atm we can only allocate and will run out of memory
u8 *allocate_memory(GameMemory *gameMemory, u32 sizeInBytes)
{
    if (gameMemory->allocatedBytes + sizeInBytes < gameMemory->memorySizeInBytes)
    {
        u8 *memory = gameMemory->memory + gameMemory->allocatedBytes;
        gameMemory->allocatedBytes += sizeInBytes;
        return memory;
    }
    else
    {
        CAKEZ_ASSERT(false, "Game is out of memory");
        return 0;
    }
}

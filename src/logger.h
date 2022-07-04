#pragma once

#include "platform.h"

// Standard Library for sprs32f
#include <stdio.h>

template <typename... Args>
void _log(u8 *prefix, TextColor color, u8 *msg, Args... args)
{
    char fmtBuffer[32000] = {};
    char msgBuffer[32000] = {};
    sprintf(fmtBuffer, "%s: %s\n", prefix, msg);
    sprintf(msgBuffer, fmtBuffer, args...);
    platform_log(msgBuffer, color);
}

#define CAKEZ_TRACE(msg, ...) _log((u8*)"TRACE", TEXT_COLOR_GREEN, (u8*)msg, __VA_ARGS__)
#define CAKEZ_WARN(msg, ...) _log((u8*)"WARN", TEXT_COLOR_YELLOW, (u8*)msg, __VA_ARGS__)
#define CAKEZ_ERROR(msg, ...) _log((u8*)"ERROR", TEXT_COLOR_RED, (u8*)msg, __VA_ARGS__)
#define CAKEZ_FATAL(msg, ...) _log((u8*)"FATAL", TEXT_COLOR_LIGHT_RED, (u8*)msg, __VA_ARGS__)

#ifdef DEBUG
#define CAKEZ_ASSERT(x, message, ...)          \
{                                          \
    if (!(x))                              \
    {                                      \
        CAKEZ_ERROR(message, __VA_ARGS__); \
        __debugbreak();                    \
    }                                      \
}
#else
#define CAKEZ_ASSERT(x, ...)
#endif

//TODO: Maybe use my own defines?
#pragma once
#include <cstdint>
#define BIT(x) (1u << x)

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float r32;
typedef double r64;

typedef uintptr_t u_ptr;
typedef intptr_t s_ptr;

typedef u32 b32;
typedef size_t memory_index;

#define internal static
#define local_persist static
#define global_variable static

#define false 0
#define true 1

#define ArraySize(arr) sizeof((arr)) / sizeof((arr)[0])

#ifdef WINDOWS_BUILD
#define line_id(index) (size_t)((__LINE__ << 16) | (index))
#endif

// Used to generate Enums and Strings of those Enums
#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

#define KB(x) ((uint64_t)1024 * x)
#define MB(x) ((uint64_t)1024 * KB(x))
#define GB(x) ((uint64_t)1024 * MB(x))

#define U32_ERROR UINT32_MAX
#define INVALID_IDX UINT32_MAX

#define FOURCC(str) (u32)(((u32)str[0]) | ((u32)str[1] << 8) | ((u32)str[2] << 16) | ((u32)str[3] << 24))

inline void ChangeEndianness(u32 *val)
{
    *val = (*val & 0xff) |
           ((*val >> 8) & 0xff00) |
           ((*val & 0xff00) << 8) |
           *val >> 24;
}
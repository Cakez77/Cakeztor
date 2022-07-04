#pragma once

#include "defines.h"

enum TextColor
{
    TEXT_COLOR_WHITE,
    TEXT_COLOR_GREEN,
    TEXT_COLOR_YELLOW,
    TEXT_COLOR_RED,
    TEXT_COLOR_LIGHT_RED,
};

void platform_log(char *msg, TextColor color);

/**
 * This function tests for the existence of a file in
 * a given path. This function has to be implemented 
 * for each platform.
 * @param path The path to the file
 * @return true if the file exists, false otherwise
 */
bool platform_file_exists(char *path);

/**
 * This function reads an entire file s32o a buffer 
 * allocated on the heap and returns that buffer. If
 * no file could be found, 0 is returned.
 * @param fileName The path to the file
 * @param fileSize The size of file in bytes; will be written to by the function
 * @return u8* buffer or 0 if the file doesn't exist.
 */
char *platform_read_file(char *fileName, u32 *fileSize);

/**
 * This function reads size bytes of a file at a byteOffset s32o a buffer 
 * allocated on the heap and returns that buffer. If
 * no file could be found, 0 is returned.
 * @param fileName The path to the file
 * @param byteOffset The offset in bytes s32o the file
 * @param size The size of bytes to read
 * @return u8* buffer or 0 if the file doesn't exist.
 */
char *platform_read_file(char *path, u32 byteOffset, u32 size);

unsigned long platform_write_file(
    char *path,
    char *buffer,
    u32 size,
    bool overwrite);

void platform_delete_file(char *path);

long long platform_last_edit_timestamp(char *path);

u64 platform_get_file_size(char *path);

void platform_get_window_size(u32 *windowWidth, u32 *windowHeight);

void platform_show_message_box(char *msg);

u32 platform_get_file_count(char *path);

bool platform_replace_file(char *fileToReplace, char *replaceFile, bool keepBackup = true);

void platform_set_volume(float volume);

void platform_exit_game();

bool platform_get_first_filename(char *fileName, char *folderPath);

bool platform_get_next_filename(char *fileName, char *folderPath);

u64 platform_get_performance_tick_count();
u64 platform_get_performance_tick_frequency();


struct Instrumentor
{
    u64 start, stop;
    float *outDT;

    Instrumentor(float *outDT)
        : outDT(outDT)
    {
        start = platform_get_performance_tick_count();
    }

    ~Instrumentor()
    {
        stop = platform_get_performance_tick_count();
        u64 frequency = platform_get_performance_tick_frequency();
        u64 elapsedTicks = stop - start;

        // Convert to Microseconds to not loose precision,
        // by deviding a small numbner by a large one
        u64 elapsedTimeInMicroseconds =
            (elapsedTicks * 1000000) / frequency;

        // Time in milliseconds
        *outDT = (float)elapsedTimeInMicroseconds / 1000.0f;
    }
};

// void start_thread(params...);


Instrumentor _measure_scope_time(char *text, char *unit, bool log = false);

#ifdef MEASURE_PERF 
#define MEASURE_SCOPE(text, unit) Instrumentor i = _measure_scope_time(text, unit);
#define MEASURE_SCOPE_LOG(text, unit) Instrumentor i = _measure_scope_time(text, unit, true);

#else
#define MEASURE_SCOPE(text, unit)
#define MEASURE_SCOPE_LOG(text, unit)
#endif
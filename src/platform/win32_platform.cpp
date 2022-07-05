// Defines
#include "defines.h"

// Logger
#include "logger.h"

// Math
#include "my_math.cpp"

// App
#include "app/app.cpp"

// Memory
#include "memory.h"

// Input
#include "input.cpp"

// Platform layer
#include <windows.h>
#include <windowsx.h>

// Renderer
#include "renderer/vulkan/vk_renderer.cpp"

global_variable bool running;
global_variable InputState* input;
global_variable HWND window;
global_variable WINDOWPLACEMENT prevWindowPlacment = {};

LRESULT CALLBACK window_callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ERASEBKGND:
    {
        // Notify the OS that erasing will be handled by the application to prevent flicker
        return 1;
    }

    case WM_CLOSE:
    {
        running = false;
        return 0;
    }

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }

    case WM_SIZE:
    {
        // Screen Size
        u32 width, height;
        platform_get_window_size(&width, &height);
        input->screenSize = {(float)width, (float)height};
        break;
    }

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        if(wParam == VK_F11 && msg == WM_KEYDOWN)
        {
            DWORD windowStyle = GetWindowLong(hwnd, GWL_STYLE);

            // Only change if not already fullscreen
            if (windowStyle & WS_OVERLAPPEDWINDOW) {
                MONITORINFO mi = {sizeof(mi)};
                    
                if (GetWindowPlacement(hwnd, &prevWindowPlacment) &&
                    GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi))
                {
                    // Remove Overlapped
                    SetWindowLong(hwnd, GWL_STYLE,
                            windowStyle & ~WS_OVERLAPPEDWINDOW);

                    SetWindowPos(hwnd, HWND_TOP,
                            mi.rcMonitor.left, mi.rcMonitor.top,
                            mi.rcMonitor.right - mi.rcMonitor.left,
                            mi.rcMonitor.bottom - mi.rcMonitor.top, 
                            SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

                }
            }
            else
            {
                SetWindowLong(hwnd, GWL_STYLE,
                        windowStyle | WS_OVERLAPPEDWINDOW);
                SetWindowPlacement(hwnd, &prevWindowPlacment);
                SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

            }
        }

        KeyState newKeyState = (msg == WM_SYSKEYUP || msg == WM_KEYUP) ? KEY_STATE_UP : KEY_STATE_DOWN;

        if (newKeyState == KEY_STATE_UP && input->keys[wParam].keyState == KEY_STATE_DOWN ||
            newKeyState == KEY_STATE_DOWN && input->keys[wParam].keyState == KEY_STATE_UP)
        {
            input->keys[wParam].halfTransitionCount++;
        }

        input->keys[wParam].keyState = newKeyState;
        break;
    }

    case WM_MOUSEMOVE:
    {
        input->oldMousePos = input->mousePos;
        input->mousePos.x = (float)GET_X_LPARAM(lParam);
        input->mousePos.y = (float)GET_Y_LPARAM(lParam);
        input->relMouse += input->mousePos - input->oldMousePos;
        return 1;
    }

    case WM_MOUSEWHEEL:
    {
        s32 delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (delta != 0)
        {
            delta = (delta < 0) ? -1 : 1;
        }
        input->wheelDelta = delta;
        break;
    }

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    {
        // KeyType mouseKey =
        //     (msg == WM_RBUTTONDOWN || (msg == WM_RBUTTONUP)
        //          ? KEY_RIGHT_MOUSE
        //      : (msg == WM_LBUTTONDOWN) || (msg == WM_LBUTTONUP)
        //          ? KEY_LEFT_MOUSE
        //          : KEY_MIDDLE_MOUSE);

        // KeyState newKeyState = (msg == WM_RBUTTONDOWN || msg == WM_LBUTTONDOWN || msg == WM_MBUTTONDOWN)
        //                            ? KEY_STATE_DOWN
        //                            : KEY_STATE_UP;

        // input->clickMousePos.x = (float)GET_X_LPARAM(lParam);
        // input->clickMousePos.y = (float)GET_Y_LPARAM(lParam);

        // if (newKeyState == KEY_STATE_UP && input->keys[mouseKey].keyState == KEY_STATE_DOWN ||
        //     newKeyState == KEY_STATE_DOWN && input->keys[mouseKey].keyState == KEY_STATE_UP)
        // {
        //     input->keys[mouseKey].halfTransitionCount++;
        // }

        // input->keys[mouseKey].keyState = newKeyState;

        return 1;
    }
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

internal bool platform_create_window(s32 width, s32 height, char *title)
{
    HINSTANCE instance = GetModuleHandleA(0);

    // Setup and register window class
    HICON icon = LoadIcon(instance, IDI_APPLICATION);
    WNDCLASS wc = {};
    wc.lpfnWndProc = window_callback;
    wc.hInstance = instance;
    wc.hIcon = icon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW); // NULL; => Manage the cursor manually
    wc.lpszClassName = "cakez_window_class";

    if (!RegisterClassA(&wc))
    {
        MessageBoxA(0, "Window registration failed", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // Create window
    u32 client_x = 100;
    u32 client_y = 100;
    u32 client_width = width;
    u32 client_height = height;

    u32 window_x = client_x;
    u32 window_y = client_y;
    u32 window_width = client_width;
    u32 window_height = client_height;

    u32 window_style =
        WS_OVERLAPPED |
        WS_SYSMENU |
        WS_CAPTION |
        WS_THICKFRAME |
        WS_MINIMIZEBOX |
        WS_MAXIMIZEBOX;

    // topmost | WS_EX_TOPMOST;
    u32 window_ex_style = WS_EX_APPWINDOW;

    // Obtain the size of the border
    RECT border_rect = {};
    AdjustWindowRectEx(
        &border_rect,
        (DWORD)window_style,
        0,
        (DWORD)window_ex_style);

    window_x += border_rect.left;
    window_y += border_rect.top;

    window_width += border_rect.right - border_rect.left;
    window_height += border_rect.bottom - border_rect.top;

    window = CreateWindowExA(
        (DWORD)window_ex_style, "cakez_window_class", title,
        (DWORD)window_style, window_x, window_y, window_width, window_height,
        0, 0, instance, 0);

    if (window == 0)
    {
        MessageBoxA(NULL, "Window creation failed!", "Error", MB_ICONEXCLAMATION | MB_OK);
        return false;
    }

    // DragAcceptFiles(window, true);

    // Show the window
    ShowWindow(window, SW_SHOW);

    return true;
}

internal void platform_update_window()
{
    // Clear the transitionCount for every key
    {
        for (u32 keyIdx = 0; keyIdx < 255; keyIdx++)
        {
            input->keys[keyIdx].halfTransitionCount = 0;
        }
    }

    // Reset relative Mouse Movement
    {
        input->relMouse.x = 0.0f;
        input->relMouse.y = 0.0f;
        input->wheelDelta = 0;
    }

    MSG msg;

    while (PeekMessageA(&msg, window, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}


global_variable LARGE_INTEGER ticksPerSecond;
u32 constexpr FILE_IO_BUFFER_SIZE = MB(1);
global_variable char *fileIOBuffer;
global_variable char *fontAtlasBuffer;

s32 main (){
    running = true;

    LARGE_INTEGER lastTickCount, currentTickCount;
    QueryPerformanceFrequency(&ticksPerSecond);
    QueryPerformanceCounter(&lastTickCount);
    float dt = 0;

    GameMemory gameMemory = {};
    gameMemory.memory = (u8 *)malloc(MB(10));
    gameMemory.memorySizeInBytes = MB(10);

    input = (InputState*)allocate_memory(&gameMemory, sizeof(InputState));
    if(!input)
    {
        CAKEZ_FATAL("Failed to allocate Memory for Input");
        return -1;
    }

    // Window
    IVec2 windowSize = {1720, 900};
    platform_create_window(windowSize.x, windowSize.y, "Cakeztor");

    // Allocate File I/O Memory
    fileIOBuffer = (char *)allocate_memory(&gameMemory, FILE_IO_BUFFER_SIZE);
    if (!fileIOBuffer)
    {
        CAKEZ_FATAL("Failed to allocate memory to handle File I/O");
        return -1;
    }

    // Renderer
    VkContext* vkcontext = (VkContext*)allocate_memory(&gameMemory, sizeof(VkContext));
    if (!vkcontext || !vk_init(vkcontext, window, true))
    {
        CAKEZ_FATAL("Failed to allocate memory for the Vulkan Context");
        return -1;
    }

    fontAtlasBuffer = (char *)allocate_memory(&gameMemory, MB(1));
    if (!fontAtlasBuffer)
    {
        CAKEZ_FATAL("Failed to allocate memory to Upload the font Atlas");
        return -1;
    }
    vk_init_font(vkcontext, fontAtlasBuffer, 512, 42);

    AppState* app = (AppState*)allocate_memory(&gameMemory, sizeof(AppState));
    if (!app)
    {
        CAKEZ_FATAL("Failed to allocate memory for the AppState");
        return -1;
    }
    memset(app->buffer, 0, 1000);

    while(running)
    {
        platform_update_window();
        update_app(app, input);
        vk_render(vkcontext, input, app);
    }

    return 0;
}

void platform_log(char *msg, TextColor color)
{
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    u32 colorBits = 0;

    switch (color)
    {
    case TEXT_COLOR_WHITE:
        colorBits = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        break;

    case TEXT_COLOR_GREEN:
        colorBits = FOREGROUND_GREEN;
        break;

    case TEXT_COLOR_YELLOW:
        colorBits = FOREGROUND_GREEN | FOREGROUND_RED;
        break;

    case TEXT_COLOR_RED:
        colorBits = FOREGROUND_RED;
        break;

    case TEXT_COLOR_LIGHT_RED:
        colorBits = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    }

    SetConsoleTextAttribute(consoleHandle, (WORD)colorBits);

#ifdef DEBUG
    OutputDebugStringA(msg);
#endif

    WriteConsoleA(consoleHandle, msg, strlen(msg), 0, 0);
}

void platform_get_window_size(u32 *windowWidth, u32 *windowHeight)
{
    RECT r;
    GetClientRect(window, &r);

    *windowWidth = r.right - r.left;
    *windowHeight = r.bottom - r.top;
}

char *platform_read_file(char *path, u32 *fileSize)
{
    char *buffer = 0;

    if (fileSize)
    {
        if (fileIOBuffer)
        {
            HANDLE file = CreateFile(
                path,
                GENERIC_READ,
                FILE_SHARE_READ,
                0,
                OPEN_EXISTING,
                0, 0);

            if (file != INVALID_HANDLE_VALUE)
            {
                LARGE_INTEGER fSize;
                if (GetFileSizeEx(file, &fSize))
                {
                    *fileSize = (u32)fSize.QuadPart;

                    if (*fileSize < FILE_IO_BUFFER_SIZE)
                    {
                        // Use File IO Buffer
                        buffer = fileIOBuffer;

                        DWORD bytesRead;
                        if (ReadFile(file, buffer, *fileSize, &bytesRead, 0) &&
                            *fileSize == bytesRead)
                        {
                        }
                        else
                        {
                            CAKEZ_WARN("Failed reading file %s", path);
                            buffer = 0;
                        }
                    }
                    else
                    {
                        CAKEZ_ASSERT(0, "File size: %d, too large for File IO Buffer", *fileSize);
                        CAKEZ_WARN("File size: %d, too large for File IO Buffer", *fileSize);
                    }
                }
                else
                {
                    CAKEZ_WARN("Failed getting size of file %s", path);
                }

                CloseHandle(file);
            }
            else
            {
                CAKEZ_WARN("Failed opening file %s", path);
            }
        }
        else
        {
            CAKEZ_ASSERT(0, "No File IO Buffer");
            CAKEZ_WARN("No File IO Buffer");
        }
    }
    else
    {
        CAKEZ_ASSERT(0, "No Length supplied!");
        CAKEZ_WARN("No Length supplied!");
    }

    return buffer;
}
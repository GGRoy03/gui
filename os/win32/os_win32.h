#pragma once

// [Includes & Linking]

#include <windows.h>
#pragma comment(lib, "user32")

// [Core Types]

typedef struct os_win32_window os_win32_window;
struct os_win32_window
{
    os_win32_window *Next;
    vec2_i32         Resolution;
    HWND             Handle;
};

typedef struct os_win32_state
{
    // Misc
    memory_arena *Arena;

    // Info
    os_system_info SystemInfo;

    // Window
    HWND WindowHandle;
} os_win32_state;

// [Globals]

global os_win32_state OSWin32State;

// [Win32 Specific API]

internal HWND OSWin32GetWindowHandle(void);
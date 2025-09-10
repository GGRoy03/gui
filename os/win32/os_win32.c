// [Internal Implementation]

internal LRESULT CALLBACK
OSWin32WindowProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    switch(Message)
    {

    case WM_CLOSE:
    {
        DestroyWindow(Handle);
        return 0;
    } break;

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    } break;

    }

    return DefWindowProc(Handle, Message, WParam, LParam);
}

i32 WINAPI
wWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPWSTR CmdLine, i32 ShowCmd)
{
    UNUSED(PrevInstance);
    UNUSED(CmdLine);
    UNUSED(Instance);

    {
        SYSTEM_INFO SystemInfo = {0};
        GetSystemInfo(&SystemInfo);

        OSWin32State.SystemInfo.PageSize = (u64)SystemInfo.dwPageSize;
    }

    {
        WNDCLASSEX WindowClass = { 0 };
        WindowClass.cbSize        = sizeof(WNDCLASSEX);
        WindowClass.style         = CS_HREDRAW | CS_VREDRAW;
        WindowClass.lpfnWndProc   = OSWin32WindowProc;
        WindowClass.hInstance     = GetModuleHandle(0);
        WindowClass.hIcon         = LoadIcon(0, IDI_APPLICATION);
        WindowClass.hCursor       = LoadCursor(0, IDC_ARROW);
        WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
        WindowClass.lpszMenuName  = 0;
        WindowClass.lpszClassName = L"Game Window";
        WindowClass.hIconSm       = LoadIcon(0, IDI_APPLICATION);

        if(!RegisterClassEx(&WindowClass))
        {
            MessageBox(0, L"Error registering class", L"Error", MB_OK | MB_ICONERROR);
            return 0;
        }

        OSWin32State.WindowHandle = CreateWindowEx(0, WindowClass.lpszClassName,
                                                   L"Engine", WS_OVERLAPPEDWINDOW,
                                                   0, 0, 1920, 1080, // WARN: Lazy
                                                   0, 0, WindowClass.hInstance, 0);
        ShowWindow(OSWin32State.WindowHandle, ShowCmd);
    }

    GameEntryPoint();

    return 0;
}

// [Per-OS API Memory Implementation]

internal void * 
OSReserveMemory(u64 Size)
{
    void *Result = VirtualAlloc(0, Size, MEM_RESERVE, PAGE_READWRITE);
    return Result;
}

internal b32 
OSCommitMemory(void *Pointer, u64 Size)
{
    b32 Result = 0;
    if(Pointer)
    {
        Result = (VirtualAlloc(Pointer, Size, MEM_COMMIT, PAGE_READWRITE) != 0);
    }
    return Result;
}

internal void
OSRelease(void *Memory)
{
    VirtualFree(Memory, 0, MEM_RELEASE);
}

// [Per-OS API File Implementation]

internal os_handle
OSFindFile(byte_string Path)
{
    os_handle Handle = {0};

    if (Path.String)
    {
        Handle.u64[0] = (u64) CreateFileA((LPCSTR)Path.String, GENERIC_READ, FILE_SHARE_READ, NULL,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    }

    return Handle;
}

internal os_file
OSReadFile(os_handle Handle, memory_arena *Arena)
{
    os_file Result     = {0};
    HANDLE  FileHandle = (HANDLE)Handle.u64[0];

    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSizeWin32;
        if (GetFileSizeEx(FileHandle, &FileSizeWin32))
        {
            DWORD Unused       = 0;
            BOOL  Success      = 1;
            u64   FileSize     = FileSizeWin32.QuadPart;
            u64   ToRead       = FileSize;
            u32   ReadSize     = Kilobyte(8);
            u8   *WritePointer = 0;

            Result.Content.Size   = FileSize;
            Result.Content.String = PushArena(Arena, FileSize, AlignOf(u8));

            while (Success && ToRead > ReadSize)
            {
                WritePointer = Result.Content.String + (FileSize - ToRead);

                Success = ReadFile(FileHandle, WritePointer, ReadSize, &Unused, NULL);
                ToRead -= ReadSize;
            }

            if (Success)
            {
                WritePointer     = Result.Content.String + (FileSize - ToRead);
                Result.FullyRead = (b32)ReadFile(FileHandle, WritePointer, (u32)ToRead, &Unused, NULL);
            }
        }

    }

    return Result;
}

// [Per-OS API Frame Context Implementation]

internal b32
OSUpdateWindow()
{
    b32 Result = 1;

    MSG Message;
    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        if(Message.message == WM_QUIT)
        {
            Result = 0;
            return Result;
        }

        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    return Result;
}

internal void
OSSleep(u32 Milliseconds)
{
    Sleep(Milliseconds);
}

// [Per-OS API Getters Implementation]

internal os_system_info *
OSGetSystemInfo(void)
{
    os_system_info *Result = &OSWin32State.SystemInfo;
    return Result;
}

internal vec2_i32
OSGetClientSize(void)
{
    vec2_i32 Result = { 0 };
    HWND     Handle = OSWin32State.WindowHandle;

    if (Handle)
    {
        RECT ClientRect;
        GetClientRect(Handle, &ClientRect);

        Result.X = (ClientRect.right  - ClientRect.left);
        Result.Y = (ClientRect.bottom - ClientRect.top);
    }

    return Result;
}

// [Per-OS API Crash/Debug Implementation]

internal void
OSAbort(i32 ExitCode)
{
    ExitProcess(ExitCode);
}

// [WIN32 SPECIFIC API]

internal HWND 
OSWin32GetWindowHandle(void)
{
    HWND Result = OSWin32State.WindowHandle;
    return Result;
}

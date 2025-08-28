// [Platform Agnostic Helpers]

static void
ProcessInputMessage(cim_button_state *NewState, bool IsDown)
{
    if (NewState->EndedDown != IsDown)
    {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}

static bool
IsMouseClicked(CimMouse_Button MouseButton, cim_inputs *Inputs)
{
    cim_button_state *State = &Inputs->MouseButtons[MouseButton];
    bool IsClicked = (State->EndedDown) && (State->HalfTransitionCount > 0);

    return IsClicked;
}

// [Win32]

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h>

#include "d2d1.h"
#include "dwrite.h"

#pragma comment (lib, "dwrite")
#pragma comment (lib, "d2d1")

// [Internals]

// This is the only thing that remains a global.
static IDWriteFactory *DWriteFactory;

static DWORD   WINAPI   Win32WatcherThread(LPVOID Param);
static LRESULT CALLBACK Win32CimProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam);
static int              Win32UTF8ToWideString(char *UTF8String, WCHAR *WideString, cim_i32 WideStringBufferSize, cim_i32 CharactersToProcess);

typedef class win32_glyph_catcher : public IDWriteTextRenderer 
{

public:
    win32_glyph_catcher(text_layout_info *Out) : RefCount_(1), Out_(Out) {}
    virtual ~win32_glyph_catcher() = default;

    IFACEMETHODIMP QueryInterface(REFIID riid, void **Object) 
        override 
    {
        if (!Object) return E_POINTER;

        if (riid == __uuidof(IUnknown)            ||
            riid == __uuidof(IDWriteTextRenderer) ||
            riid == __uuidof(IDWritePixelSnapping) ) 
        {
            *Object = static_cast<IDWriteTextRenderer *>(this);
            this->AddRef();

            return S_OK;
        }

        *Object = nullptr;
        return E_NOINTERFACE;
    }

    IFACEMETHODIMP_(ULONG) AddRef()  
        override 
    { 
        return InterlockedIncrement(&RefCount_); 
    }

    IFACEMETHODIMP_(ULONG) Release() 
        override 
    {
        ULONG u = InterlockedDecrement(&RefCount_);
        if (u == 0) delete this;
        return u;
    }

    IFACEMETHODIMP IsPixelSnappingDisabled(void *ClientDrawingContext, BOOL *IsDisabled) 
        noexcept override
    {
        Cim_Unused(ClientDrawingContext);

        *IsDisabled = FALSE;
        return S_OK;
    }

    IFACEMETHODIMP GetCurrentTransform(void *ClientDrawingContext, DWRITE_MATRIX *Transform) 
        noexcept override
    {
        Cim_Unused(ClientDrawingContext);

        if (!Transform) return E_POINTER;

        Transform->m11 = 1.0f; Transform->m12 = 0.0f;
        Transform->m21 = 0.0f; Transform->m22 = 1.0f;
        Transform->dx  = 0.0f; Transform->dy = 0.0f;

        return S_OK;
    }

    IFACEMETHODIMP GetPixelsPerDip(void *ClientDrawingContext, FLOAT *PixelsPerDip) 
        noexcept override 
    {
        Cim_Unused(ClientDrawingContext);

        *PixelsPerDip = 1.0f;
        return S_OK;
    }

    IFACEMETHODIMP DrawGlyphRun(void *ClientDrawingContext, FLOAT /* BaselineOriginX */, FLOAT /* BaselineOriginY */,
                                DWRITE_MEASURING_MODE /* MeasuringMode */, const DWRITE_GLYPH_RUN *GlyphRun,
                                const DWRITE_GLYPH_RUN_DESCRIPTION *GlyphRunDescription, IUnknown *ClientDrawingEffect) 
        noexcept override
    {
        Cim_Unused(ClientDrawingContext);
        Cim_Unused(ClientDrawingEffect);
        Cim_Unused(GlyphRunDescription);

        // NOTE: bidi level is useful to know if we are drawing left to right or right to left.

        if (!GlyphRun || !GlyphRun->fontFace || !Out_) return E_FAIL;

        UINT GlyphCount = GlyphRun->glyphCount;

        for (UINT32 Idx = 0; Idx < GlyphCount; ++Idx)
        {
            glyph_layout_info *GlyphInfo = Out_->GlyphLayoutInfo + Idx;

            GlyphInfo->AdvanceX = GlyphRun->glyphAdvances[Idx];
            GlyphInfo->GlyphId  = GlyphRun->glyphIndices[Idx];

            if (GlyphRun->glyphOffsets)
            {
                GlyphInfo->OffsetX = GlyphRun->glyphOffsets[Idx].advanceOffset;
                GlyphInfo->OffsetY = GlyphRun->glyphOffsets[Idx].ascenderOffset;
            }
            else
            {
                GlyphInfo->OffsetX = 0;
                GlyphInfo->OffsetY = 0;
            }
        }

        return S_OK;
    }

    IFACEMETHODIMP DrawUnderline      (void *, FLOAT, FLOAT, const DWRITE_UNDERLINE *, IUnknown *)          noexcept override { return S_OK; }
    IFACEMETHODIMP DrawStrikethrough  (void *, FLOAT, FLOAT, const DWRITE_STRIKETHROUGH *, IUnknown *)      noexcept override { return S_OK; }
    IFACEMETHODIMP DrawInlineObject   (void *, FLOAT, FLOAT, IDWriteInlineObject *, BOOL, BOOL, IUnknown *) noexcept override { return S_OK; }

private:
    long              RefCount_;
    text_layout_info *Out_;

} win32_glyph_catcher;

// [Public API Implementation]

typedef struct os_font_objects
{
    IDWriteTextFormat    *Format;
    ID2D1RenderTarget    *RenderTarget;
    ID2D1SolidColorBrush *FillBrush;
} os_font_objects;

static bool
CreateFontObjects(const char *FontName, cim_f32 FontSize, void *TransferSurface, ui_font *Font)
{
    Cim_Assert(DWriteFactory);

    HRESULT Error = E_FAIL;

    WCHAR WideFontName[256];
    int FontNeeded = Win32UTF8ToWideString((char *)FontName, WideFontName, 256, -1);

    if (FontNeeded > 0)
    {
        // NOTE: What is the locale
        Error = DWriteFactory->CreateTextFormat(WideFontName, NULL, DWRITE_FONT_WEIGHT_REGULAR,
                                                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                                FontSize, L"en-us", &Font->OSFontObjects->Format);
        if (SUCCEEDED(Error))
        {
            Font->OSFontObjects->Format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
            Font->OSFontObjects->Format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);

            DWRITE_LINE_SPACING_METHOD UnusedMethod;
            FLOAT                      UnusedBaseline;
            Error = Font->OSFontObjects->Format->GetLineSpacing(&UnusedMethod, &Font->LineHeight, &UnusedBaseline);
        }
    }

    if (SUCCEEDED(Error))
    {
        ID2D1Factory        *Factory = NULL;
        D2D1_FACTORY_OPTIONS Options = { D2D1_DEBUG_LEVEL_ERROR };
        Error = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), &Options, (void **)&Factory);

        if (SUCCEEDED(Error))
        {
            D2D1_RENDER_TARGET_PROPERTIES Props =
                D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                0, 0);

            Error = Factory->CreateDxgiSurfaceRenderTarget((IDXGISurface *)TransferSurface, &Props, &Font->OSFontObjects->RenderTarget);
            if (SUCCEEDED(Error))
            {
                Error = Font->OSFontObjects->RenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0F, 1.0F, 1.0F, 1.0F), &Font->OSFontObjects->FillBrush);
            }
        }

        if (Factory)
        {
            Factory->Release();
        }
    }

#if 0
    Font->OSFontObjects->RenderTarget->Release();
    Font->OSFontObjects->FillBrush->Release();
    Font->OSFontObjects->Format->Release();
#endif


    return SUCCEEDED(Error);
}

static void
ReleaseFontObjects(os_font_objects *Objects)
{
    if (Objects->FillBrush)
    {
        Objects->FillBrush->Release();
        Objects->FillBrush = NULL;
    }

    if (Objects->Format)
    {
        Objects->Format->Release();
        Objects->Format = NULL;
    }

    if (Objects->RenderTarget)
    {
        Objects->RenderTarget->Release();
        Objects->RenderTarget = NULL;
    }
}

static size_t
GetFontObjectsFootprint()
{
    size_t Result = sizeof(os_font_objects);
    return Result;
}

static bool
PlatformInit(const char *StyleDir)
{
    dir_watcher_context *Watcher = (dir_watcher_context *)calloc(1, sizeof(dir_watcher_context));
    if (!Watcher)
    {
        CimLog_Error("Win32 Init: Failed to allocate memory for the IO context");
        return false;
    }
    strncpy_s(Watcher->Directory, sizeof(Watcher->Directory),
              StyleDir, strlen(StyleDir));
    Watcher->Directory[(sizeof(Watcher->Directory) - 1)] = '\0';
    Watcher->RefCount = 2; // NOTE: Need it on the main thread as well as the watcher thread.

    WIN32_FIND_DATAA FindData;
    HANDLE           FindHandle = INVALID_HANDLE_VALUE;

    char   SearchPattern[MAX_PATH];
    size_t DirLength = strlen(StyleDir);

    // Path/To/Dir[/*.cim]" 6 Char
    if (DirLength + 6 >= MAX_PATH)
    {
        CimLog_Error("Win32 Init: Provided directory is too long: %s", StyleDir);
        return false;
    }
    snprintf(SearchPattern, MAX_PATH, "%s/*.cim", StyleDir);

    FindHandle = FindFirstFileA(SearchPattern, &FindData);
    if (FindHandle == INVALID_HANDLE_VALUE)
    {
        DWORD Error = GetLastError();
        if (Error == ERROR_FILE_NOT_FOUND)
        {
            CimLog_Warn("Win32 Init: Provided directory: %s does not contain any .cim files", StyleDir);
            return true;
        }
        else
        {
            CimLog_Error("Win32 Init: FindFirstFileA Failed with error %u", Error);
            return false;
        }
    }

    const cim_u32 Capacity = 4;

    Watcher->Files     = (cim_watched_file *)calloc(Capacity, sizeof(cim_watched_file));
    Watcher->FileCount = 0;

    if (!Watcher->Files)
    {
        CimLog_Error("Win32 Init: Failed to allocate memory for the wathched files.");
        return false;
    }

    do
    {
        if (!(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (Watcher->FileCount == Capacity)
            {
                CimLog_Warn("Win32 Init: Maximum number of style files reached.");
                break;
            }

            cim_watched_file *Watched = Watcher->Files + Watcher->FileCount;

            size_t NameLength = strlen(FindData.cFileName);
            if (NameLength >= MAX_PATH)
            {
                CimLog_Error("Win32 Init: File name is too long: %s",
                              FindData.cFileName);
                return false;
            }

            memcpy(Watched->FileName, FindData.cFileName, NameLength);
            Watched->FileName[NameLength] = '\0';

            size_t FullLength = DirLength + 1 + NameLength + 1;
            if (FullLength >= MAX_PATH)
            {
                CimLog_Error("Win32 Init: Full path for %s is too long.",
                              FindData.cFileName);
                return false;
            }

            snprintf(Watched->FullPath, FullLength, "%s/%s", StyleDir,
                                        FindData.cFileName);
            Watched->FullPath[FullLength] = '\0';

            ++Watcher->FileCount;
        }
    } while (FindNextFileA(FindHandle, &FindData));
    FindClose(FindHandle);

    if (Watcher->FileCount > 0)
    {
        HANDLE IOThreadHandle = CreateThread(NULL, 0, Win32WatcherThread, Watcher, 0, NULL);
        if (!IOThreadHandle)
        {
            CimLog_Error("Win32 Init: Failed to launch IO Thread with error : %u", GetLastError());
            return false;
        }

        CloseHandle(IOThreadHandle);

        // BUG: This is incorrect. Perhpas the LoadThemeFiles function should accept
        // a watched file as argument? Forced to pass 1 right now to circumvent it.
        char *P = Watcher->Files[0].FullPath;
        LoadThemeFiles(&P, 1);
    }

    if (InterlockedDecrement(&Watcher->RefCount) == 0)
    {
        if( Watcher->Files) free(Watcher->Files);
        if (Watcher)        free(Watcher);
    }

    // Simply Init DWrite here.
    HRESULT Error = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown **)&DWriteFactory);
    if (FAILED(Error) || !DWriteFactory)
    {
        CimLog_Error("Failed to create the factory for DWrite.");
        return false;
    }

    return true;
}

static buffer
PlatformReadFile(char *FileName)
{
    // TODO: Make this a buffered read.

    buffer Result = {};

    if (!FileName)
    {
        CimLog_Error("ReadEntireFile: FileName is NULL");
        return Result;
    }

    HANDLE hFile = CreateFileA(FileName,
                               GENERIC_READ,
                               FILE_SHARE_READ,
                               NULL,
                               OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL,
                               NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        CimLog_Error("Unable to open file: %s", FileName);
        return Result;
    }

    LARGE_INTEGER FileSizeLI = {};
    if (!GetFileSizeEx(hFile, &FileSizeLI))
    {
        CimLog_Error("GetFileSizeEx failed for: %s", FileName);
        CloseHandle(hFile);
        return Result;
    }

    if (FileSizeLI.QuadPart == 0)
    {
        CloseHandle(hFile);
        return Result;
    }

    size_t FileSize = (size_t)FileSizeLI.QuadPart;

    Result = AllocateBuffer(FileSize + 1);
    if (!IsValidBuffer(&Result) || !Result.Data)
    {
        CimLog_Error("Unable to allocate buffer for file: %s", FileName);
        CloseHandle(hFile);
        return Result;
    }

    DWORD BytesToRead = (DWORD)FileSize;
    DWORD BytesRead   = 0;
    BOOL ok = ReadFile(hFile, Result.Data, BytesToRead, &BytesRead, NULL);
    if (!ok || BytesRead == 0)
    {
        CimLog_Error("ReadFile failed or read zero bytes for: %s", FileName);
        FreeBuffer(&Result);
        Result.Data = NULL;
        Result.Size = 0;
        Result.At = 0;
        CloseHandle(hFile);
        return Result;
    }

    ((char *)Result.Data)[BytesRead] = '\0';

    Result.Size = BytesRead;
    Result.At = 0;

    CloseHandle(hFile);
    return Result;
}

static void
PlatformLogMessage(CimLog_Severity Level, const char *File, cim_i32 Line, const char *Format, ...)
{
    Cim_Unused(Level);

    va_list Args;
    __crt_va_start(Args, Format);
    __va_start(&Args, Format);

    char Buffer[1024] = { 0 };
    vsnprintf(Buffer, sizeof(Buffer), Format, Args);

    char FinalMessage[2048] = { 0 };
    snprintf(FinalMessage, sizeof(FinalMessage), "[%s:%d] %s\n", File, Line, Buffer);

    OutputDebugStringA(FinalMessage);

    __crt_va_end(Args);
}

static text_layout_info
CreateTextLayout(char *String, cim_u32 Width, cim_u32 Height, ui_font *Font)
{
    text_layout_info LayoutInfo  = {};
    HRESULT          Error       = S_OK;
    os_font_objects *FontObjects = Font->OSFontObjects;

    Cim_Assert(FontObjects && FontObjects->Format);

    if (DWriteFactory)
    {
        // WARN: Probably don't want to set an hardcoded limit like that.
        WCHAR WideString[1024];
        int   WideSize = Win32UTF8ToWideString(String, WideString, 1024, -1);
        if (WideSize == 0)
        {
            return LayoutInfo;
        }

        IDWriteTextLayout *TextLayout = NULL;
        Error = DWriteFactory->CreateTextLayout(WideString, WideSize, FontObjects->Format,
                                                Width, Height, &TextLayout);
        if (FAILED(Error) || !TextLayout)
        {
            return LayoutInfo;
        }

        DWRITE_TEXT_METRICS Metrics = {};
        Error = TextLayout->GetMetrics(&Metrics);
        if (FAILED(Error))
        {
            return LayoutInfo;
        }

        size_t UTF8StringLength = strlen(String);
        LayoutInfo.GlyphCount      = (cim_u32)UTF8StringLength; Cim_Assert(UTF8StringLength == LayoutInfo.GlyphCount);
        LayoutInfo.GlyphLayoutInfo = (glyph_layout_info*)calloc(LayoutInfo.GlyphCount, sizeof(LayoutInfo.GlyphLayoutInfo[0]));
        win32_glyph_catcher Catcher(&LayoutInfo);
        
        // Need to call whatever.
        Error = TextLayout->Draw(NULL, &Catcher, 0.0f, 0.0f);
        if (FAILED(Error))
        {
            return LayoutInfo;
        }

        LayoutInfo.Width         = (cim_u32)(Metrics.width + 0.5f);
        LayoutInfo.Height        = (cim_u32)(Metrics.height + 0.5f);
        LayoutInfo.BackendLayout = TextLayout;

    }  

    return LayoutInfo;
}

// WARN: This doesn't make much sense, because it acts as if it takes in a string,
// but in reality it only takes in a simple character.

static glyph_size
GetGlyphExtent(char *String, cim_u32 StringLength, ui_font *Font)
{
    // WARN: Obviously setting a hard limit on the string is trash, what can we do?

    glyph_size       Result      = {};
    os_font_objects *FontObjects = Font->OSFontObjects;

    if (DWriteFactory || StringLength >= 64)
    {
        WCHAR   WideString[64];
        cim_i32 WideSize = Win32UTF8ToWideString(String, WideString, 64, 1);
        if (WideSize == 0)
        {
            return Result;
        }

        // NOTE: Does the size we pass to the layout matter here? I am kind of unusure.
        IDWriteTextLayout *TextLayout = NULL;
        HRESULT Error = DWriteFactory->CreateTextLayout(WideString, WideSize, FontObjects->Format,
                                                        1024, 1024, &TextLayout);
        if (FAILED(Error) || !TextLayout)
        {
            return Result;
        }

        DWRITE_TEXT_METRICS Metrics = {};
        TextLayout->GetMetrics(&Metrics);
        Cim_Assert(Metrics.left == 0);
        Cim_Assert(Metrics.top  == 0);

        Result.Width  = (cim_u16)(Metrics.width  + 0.5f);
        Result.Height = (cim_u16)(Metrics.height + 0.5f);
    }

    return Result;
}

static void
RasterizeGlyph(char Character, stbrp_rect Rect, ui_font *Font)
{
    os_font_objects *FontObjects = (os_font_objects * )Font->OSFontObjects;

    if (!FontObjects->RenderTarget|| !FontObjects->FillBrush)
    {
        return;
    }

    // BUG: Really bug-prone, unsure how script characters get converted.
    // can they get converted to more than 2 bytes? Well, it's UTF-16, so no?
    // This code is shit anyways.
    WCHAR   WideCharacter[2];
    cim_i32 WideSize = Win32UTF8ToWideString(&Character, WideCharacter, 3, 1);
    WideCharacter[WideSize] = 0;
    if (WideSize == 0)
    {
        return;
    }

    D2D1_RECT_F DrawRect;
    DrawRect.left   = 0;
    DrawRect.top    = 0;
    DrawRect.right  = Rect.w;
    DrawRect.bottom = Rect.h;

    FontObjects->RenderTarget->SetTransform(D2D1::Matrix3x2F::Scale(D2D1::Size(1, 1),
                                            D2D1::Point2F(0.0f, 0.0f)));
    FontObjects->RenderTarget->BeginDraw();
    FontObjects->RenderTarget->Clear(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0));
    FontObjects->RenderTarget->DrawTextW(WideCharacter, WideSize, FontObjects->Format, &DrawRect, FontObjects->FillBrush,
                                         D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
                                         DWRITE_MEASURING_MODE_NATURAL);

    HRESULT Error = FontObjects->RenderTarget->EndDraw();
    if (FAILED(Error))
    {
        Cim_Assert(!"EndDraw failed.");
    }

    // NOTE: Right now we don't batch glyph, so if one frame we have 100 new glyphs we will do 100 draw calls.
    // I have no idea how slow this is and this is relatively easy to fix?
    TransferGlyph(Rect, Font);
}

// [Internals Implementation]

static int
Win32UTF8ToWideString(char *UTF8String, WCHAR *WideString, cim_i32 WideStringBufferSize, cim_i32 CharactersToProcess)
{
    if (!UTF8String || !WideString)
    {
        return 0;
    }

    int Needed = MultiByteToWideChar(CP_UTF8, 0, UTF8String, CharactersToProcess, NULL, 0);
    if (Needed <= 0 || Needed >= WideStringBufferSize)
    {
        return 0;
    }

    MultiByteToWideChar(CP_UTF8, 0, UTF8String, CharactersToProcess, WideString, Needed);

    return Needed;
}

static LRESULT CALLBACK
Win32CimProc(HWND Handle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    Cim_Unused(Handle);

    if (!CimCurrent)
    {
        return FALSE;
    }
    cim_inputs *Inputs = UIP_INPUT;

    switch (Message)
    {

    case WM_MOUSEMOVE:
    {
        cim_i32 MouseX = GET_X_LPARAM(LParam);
        cim_i32 MouseY = GET_Y_LPARAM(LParam);

        Inputs->MouseDeltaX += (MouseX - Inputs->MouseX);
        Inputs->MouseDeltaY += (MouseY - Inputs->MouseY);

        Inputs->MouseX = MouseX;
        Inputs->MouseY = MouseY;
    } break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    {
        cim_u32 VKCode = (cim_u32)WParam;
        bool    WasDown = ((LParam & ((size_t)1 << 30)) != 0);
        bool    IsDown = ((LParam & ((size_t)1 << 31)) == 0);

        if (WasDown != IsDown && VKCode < CimIO_KeyboardKeyCount)
        {
            ProcessInputMessage(&Inputs->Buttons[VKCode], IsDown);
        }
    } break;

    case WM_LBUTTONDOWN:
    {
        ProcessInputMessage(&Inputs->MouseButtons[CimMouse_Left], true);
    } break;

    case WM_LBUTTONUP:
    {
        ProcessInputMessage(&Inputs->MouseButtons[CimMouse_Left], false);
    } break;

    case WM_RBUTTONDOWN:
    {
        ProcessInputMessage(&Inputs->MouseButtons[CimMouse_Right], true);
    } break;

    case WM_RBUTTONUP:
    {
        ProcessInputMessage(&Inputs->MouseButtons[CimMouse_Right], false);
    } break;

    case WM_MBUTTONDOWN:
    {
        ProcessInputMessage(&Inputs->MouseButtons[CimMouse_Middle], true);
    } break;

    case WM_MBUTTONUP:
    {
        ProcessInputMessage(&Inputs->MouseButtons[CimMouse_Middle], false);
    } break;

    case WM_MOUSEWHEEL:
    {
    } break;

    }

    return FALSE; // We don't want to block any messages right now.
}

static DWORD WINAPI
Win32WatcherThread(LPVOID Param)
{
    dir_watcher_context *Watcher = (dir_watcher_context *)Param;

    HANDLE DirHandle = CreateFileA(Watcher->Directory, FILE_LIST_DIRECTORY,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (DirHandle == INVALID_HANDLE_VALUE)
    {
        CimLog_Fatal("Failed to open directory : %s", Watcher->Directory);
        return 1;
    }

    DWORD BytesReturned = 0;
    BYTE  Buffer[4096]  = {};

    while (true)
    {
        DWORD Filter = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_CREATION;

        BOOL FoundUpdate = ReadDirectoryChangesW(DirHandle, Buffer, sizeof(Buffer),
                                                 FALSE, Filter, &BytesReturned,
                                                 NULL, NULL);
        if (!FoundUpdate)
        {
            CimLog_Error("ReadDirectoryChangesW failed: %u\n", GetLastError());
            break;
        }

        BYTE *Ptr = Buffer;
        do
        {
            FILE_NOTIFY_INFORMATION *Info = (FILE_NOTIFY_INFORMATION *)Ptr;

            char    Name[CimIO_MaxPath];
            cim_i32 Length = WideCharToMultiByte(CP_UTF8, 0,
                                                 Info->FileName,
                                                 Info->FileNameLength / sizeof(WCHAR),
                                                 Name, sizeof(Name) - 1, NULL, NULL);
            Name[Length] = '\0';

            for (cim_u32 FileIdx = 0; FileIdx < Watcher->FileCount; FileIdx++)
            {
                if (_stricmp(Name, Watcher->Files[FileIdx].FileName) == 0)
                {
                    // WARN:
                    // 1) We only handle modifications right now. Should also handle deletion
                    //    and new files.
                    // 2) It's better to batch them. Should accumulate then send the batch.

                    if (Info->Action == FILE_ACTION_MODIFIED)
                    {
                        char *FileToChange = Watcher->Files[FileIdx].FullPath;
                        LoadThemeFiles(&FileToChange, 1);
                    }

                    break;
                }
            }

            if (Info->NextEntryOffset == 0)
            {
                break;
            }

            Ptr += Info->NextEntryOffset;
        } while (true);

    }

    CloseHandle(DirHandle);

    if (InterlockedDecrement(&Watcher->RefCount) == 0)
    {
        if (Watcher->Files) free(Watcher->Files);
        if (Watcher)        free(Watcher);
    }

    return 0;
}

#endif

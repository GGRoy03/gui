#pragma once

// [Logging Interface]

typedef enum CimLog_Severity
{
    CimLog_Info = 0,
    CimLog_Warning = 1,
    CimLog_Error = 2,
    CimLog_Fatal = 3,
} CimLog_Severity;

#define CimLog_Info(...)  PlatformLogMessage(CimLog_Info   , __FILE__, __LINE__, __VA_ARGS__);
#define CimLog_Warn(...)  PlatformLogMessage(CimLog_Warning, __FILE__, __LINE__, __VA_ARGS__)
#define CimLog_Error(...) PlatformLogMessage(CimLog_Error  , __FILE__, __LINE__, __VA_ARGS__)
#define CimLog_Fatal(...) PlatformLogMessage(CimLog_Fatal  , __FILE__, __LINE__, __VA_ARGS__)

// [IO Interface]

#define CimIO_MaxPath 256
#define CimIO_KeyboardKeyCount 256
#define CimIO_KeybordEventByufferSize 128

typedef enum CimMouse_Button
{
    CimMouse_Left        = 0,
    CimMouse_Right       = 1,
    CimMouse_Middle      = 2,
    CimMouse_ButtonCount = 3,
} CimMouse_Button;

typedef struct cim_button_state
{
    bool    EndedDown;
    cim_u32 HalfTransitionCount;
} cim_button_state;

typedef struct cim_keyboard_event
{
    cim_u8 VKCode;
    bool   IsDown;
} cim_keyboard_event;

typedef struct cim_inputs
{
    cim_button_state Buttons[CimIO_KeyboardKeyCount];
    cim_f32          ScrollDelta;
    cim_i32          MouseX, MouseY;
    cim_i32          MouseDeltaX, MouseDeltaY;
    cim_button_state MouseButtons[5];
} cim_inputs;

typedef struct cim_watched_file
{
    char FileName[CimIO_MaxPath];
    char FullPath[CimIO_MaxPath];
} cim_watched_file;

typedef struct dir_watcher_context
{
    char              Directory[CimIO_MaxPath];
    cim_watched_file *Files;
    cim_u32           FileCount;
    cim_u32 volatile  RefCount;  // NOTE: Does this need volatile?
} dir_watcher_context;

// [Public API]

// Opaque types.
typedef struct os_font_objects os_font_objects;

// Window/IO/Misc
static bool   PlatformInit                (const char *StyleDir);
static buffer PlatformReadFile            (char *FileName);
static void   PlatformLogMessage          (CimLog_Severity Level, const char *File, cim_i32 Line, const char *Format, ...);

// Font
static bool    CreateFontObjects        (const char *FontName, cim_f32 FontSize, void *TransferSurface, ui_font *Font);
static void    ReleaseFontObjects       (os_font_objects *Objects);
static size_t  GetFontObjectsFootprint  ();

// Glyphs
static void             RasterizeGlyph    (char Character, stbrp_rect Rect, ui_font *Font);
static glyph_size       GetGlyphExtent    (char *String, cim_u32 StringLength, ui_font *Font);
static text_layout_info CreateTextLayout  (char *String, cim_u32 Width, cim_u32 Height, ui_font *Font);
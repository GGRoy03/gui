#pragma once

// [Globals]

#define ThemeNameLength          64
#define ThemeCount              256
#define ThemeErrorMessageLength 256

// [Enums]

typedef enum UIThemeAttribute_Flag
{
    UIThemeAttribute_None        = 0,
    UIThemeAttribute_Size        = 1 << 0,
    UIThemeAttribute_Color       = 1 << 1,
    UIThemeAttribute_Padding     = 1 << 2,
    UIThemeAttribute_Spacing     = 1 << 3,
    UIThemeAttribute_LayoutOrder = 1 << 4,
    UIThemeAttribute_BorderColor = 1 << 5,
    UIThemeAttribute_BorderWidth = 1 << 6,
} UIThemeAttribute_Flag;

typedef enum UIStyleToken_Type
{
    // Extended ASCII... (0..255)

    // Flags used when parsing values.
    UIStyleToken_String      = 1 << 8,
    UIStyleToken_Identifier  = 1 << 9,
    UIStyleToken_Number      = 1 << 10,
    UIStyleToken_Assignment  = 1 << 11,
    UIStyleToken_Vector      = 1 << 12,
    UIStyleToken_Style       = 1 << 13,
    UIStyleToken_For         = 1 << 14,

    // Effects (Does it need to be flagged?)
    UIStyleToken_HoverEffect = 1 << 15,
    UIStyleToken_ClickEffect = 1 << 16,
} UIStyleToken_Type;

typedef enum UITheme_Type
{
    UITheme_None        = 0,
    UITheme_Window      = 1,
    UITheme_Button      = 2,
    UITheme_EffectHover = 3,
    UITheme_EffectClick = 4,
} UITheme_Type;

typedef enum UIThemeParsingError_Type
{
    ThemeParsingError_None     = 0,
    ThemeParsingError_Syntax   = 1,
    ThemeParsingError_Argument = 2,
    ThemeParsingError_Internal = 3,
} UIThemeParsingError_Type;

// [Types]

typedef struct style_id
{
    u32 Value;
} style_id;

// NOTE: Change this. Too verbose.
typedef struct style_parsing_error
{
    UIThemeParsingError_Type Type;
    union
    {
        u32 LineInFile;
        u32 ArgumentIndex;
    };

    char Message[ThemeErrorMessageLength];
} style_parsing_error;

typedef struct style_token
{
    u32 LineInFile;

    UIStyleToken_Type Type;
    union
    {
        f32         Float32;
        u32         UInt32;
        byte_string Identifier;
        vec4_f32    Vector;
    };
} style_token;

typedef struct ui_window_style
{
    style_id ThemeId;

    // Attributes
    vec4_f32 Color;
    vec4_f32 BorderColor;
    u32      BorderWidth;
    vec2_f32 Size;
    vec2_f32 Spacing;
    vec4_f32 Padding;
} ui_window_style;

typedef struct ui_button_style
{
    style_id ThemeId;

    // Attributes
    vec4_f32 Color;
    vec4_f32 BorderColor;
    u32     BorderWidth;
    vec2_f32 Size;
} ui_button_style;

typedef struct ui_style
{
    UITheme_Type Type;
    union
    {
        ui_window_style Window;
        ui_button_style Button;
    };

    style_id Id;
    style_id HoverTheme;
    style_id ClickTheme;
} ui_style;

typedef struct style_parser
{
    u32           TokenCount;
    style_token  *TokenBuffer;
    memory_arena *Arena;       // NOTE: When is this arena used exactly? Only for storing the tokens? If not, maybe use it like a scratch?

    u32 AtLine;
    u32 AtToken;

    // WARN: TERRIBLE.
    style_token *ActiveThemeNameToken;
    ui_style    *ActiveTheme;
    ui_style    *ActiveEffectTheme;
} style_parser;

typedef struct style_info
{
    u8  Name[ThemeNameLength];
    u32 NameLength;

    style_id NextWithSameLength;
    ui_style Theme;
} style_info;

typedef struct style_table
{
    style_info Themes[ThemeCount];
    u32        NextWriteIndex;
} style_table;

// [API]

internal ui_style       *GetUITheme      (read_only char *ThemeName, style_id *ComponentId);
internal ui_window_style GetWindowTheme  (read_only char *ThemeName, style_id ThemeId);
internal ui_button_style GetButtonTheme  (read_only char *ThemeName, style_id ThemeId);
internal void            LoadThemeFiles  (byte_string *Files, u32 FileCount);

// [Parsing]

static style_token        *CreateStyleToken        (UIStyleToken_Type Type, style_parser *Parser);
static style_parsing_error GetNextTokenBuffer      (os_file *File, style_parser *Parser);
static style_parsing_error StoreAttributeInTheme   (UIThemeAttribute_Flag Attribute, style_token *Value, style_parser *Parser);
static style_parsing_error ValidateAndStoreThemes  (style_parser *Parser);

// [Error Handling]

static read_only char *UIThemeAttributeToString  (UIThemeAttribute_Flag Flag);
static void            WriteErrorMessage         (style_parsing_error *Error, read_only char *Format, ...);
static void            HandleThemeError          (style_parsing_error *Error, u8 *FileName, read_only char *PublicAPIFunctionName);

// [Queries]
internal ui_window_style GetWindowTheme  (read_only char *ThemeName, style_id ThemeId);
internal ui_button_style GetButtonTheme  (read_only char *ThemeName, style_id ThemeId);

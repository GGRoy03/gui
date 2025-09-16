#pragma once

// TO ADD NEW STYLES, FOLLOW THIS GUIDE.
// 1) Add the attribute flag in UIStyleAttribute_Flag.
// 2) Add the valid style types which may have this new attribute in this table: StyleTypeValidAttributesTable.
// 3) Add the check for the new attribute in GetStyleAttributeFlagFromIdentifier and return the correct value.
// 4) Add the appropriate fields in ui_style.
// 5) Add the format check in IsAttributeFormattedCorrectly
// 6) Add the WriteStyleAttribute implementation for the new type (must convert to the correct format if needed)

// NOTE: Is there no way to make this way simpler? With a single 'key' per type, the type itself?

// [Globals]

#define ThemeNameLength 64
#define ThemeMaxCount   256

// [Enums]

typedef enum UIStyleAttribute_Flag
{
    UIStyleAttribute_None           = 0,
    UIStyleAttribute_Size           = 1 << 0,
    UIStyleAttribute_Color          = 1 << 1,
    UIStyleAttribute_Padding        = 1 << 2,
    UIStyleAttribute_Spacing        = 1 << 3,
    UIStyleAttribute_BorderColor    = 1 << 4,
    UIStyleAttribute_BorderWidth    = 1 << 5,
    UIStyleAttribute_BorderRadius   = 1 << 6,
    UIStyleAttribute_BorderSoftness = 1 << 7,

    UIStyleAttribute_Flag_Count = 8,
} UIStyleAttribute_Flag;

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

typedef enum UIStyle_Type
{
    UIStyle_None   = 0,
    UIStyle_Window = 1,
    UIStyle_Button = 2,

    // Unused.
    UIStyle_EffectHover = 3,
    UIStyle_EffectClick = 4,
} UIStyle_Type;

// [Types]

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


// TODO: Maybe separate this into a tokenizer and a parser.
typedef struct style_parser
{
    u32           TokenCount;
    u32           TokenCapacity;
    style_token  *TokenBuffer;

    memory_arena *Arena;

    u32 AtLine;
    u32 AtToken;

    byte_string  CurrentUserStyleName;
    ui_style     CurrentStyle;
    UIStyle_Type CurrentType;
} style_parser;


// [GLOBALS]

read_only global bit_field StyleTokenValueMask = UIStyleToken_String | UIStyleToken_Number | UIStyleToken_Vector;

read_only global bit_field StyleTypeValidAttributesTable[] =
{
    {0}, // None

    // Window
    {UIStyleAttribute_Size           | UIStyleAttribute_Padding     | UIStyleAttribute_Spacing |
     UIStyleAttribute_BorderColor    | UIStyleAttribute_BorderWidth | UIStyleAttribute_Color   |
     UIStyleAttribute_BorderSoftness | UIStyleAttribute_BorderRadius                           },

    // Button
    {UIStyleAttribute_Size  | UIStyleAttribute_BorderColor    | UIStyleAttribute_BorderWidth   |
     UIStyleAttribute_Color | UIStyleAttribute_BorderSoftness | UIStyleAttribute_BorderRadius  },
};

// [API]

internal void LoadThemeFiles  (byte_string *Files, u32 FileCount, ui_style_registery *Registery);

// [Parsing]

internal style_token          *CreateStyleToken                     (UIStyleToken_Type Type, style_parser *Parser);
internal b32                   TokenizeStyleFile                    (os_file *File, style_parser *Parser);
internal void                  WriteStyleAttribute                  (UIStyleAttribute_Flag Attribute, style_token ValueToken, style_parser *Parser);
internal b32                   ParseStyleTokens                     (style_parser *Parser, ui_style_registery *Registery);
internal UIStyleAttribute_Flag GetStyleAttributeFlagFromIdentifier  (byte_string Identifier);
internal b32                   IsAttributeFormattedCorrectly        (UIStyleToken_Type TokenType, UIStyleAttribute_Flag AttributeFlag);
internal void                  CacheStyle                           (ui_style Style, byte_string Name, ui_style_registery *Registery);

// [Error Handling]

internal read_only char *UIStyleAttributeToString  (UIStyleAttribute_Flag Flag);
internal void            WriteStyleErrorMessage    (u32 LineInFile, OSMessage_Severity Severity, byte_string Format, ...);

// [Public API Implementation]

internal void
LoadThemeFiles(byte_string *Files, u32 FileCount, ui_style_registery *Registery, render_handle Renderer)
{
    if (!Files)
    {
        WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("'Files' must be a valid pointer."));

        return;
    }

    style_parser    Parser    = {0};
    style_tokenizer Tokenizer = {0};
    {
        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize        = Kilobyte(64);
        Params.ReserveSize       = Megabyte(10);

        Tokenizer.Arena    = AllocateArena(Params);
        Tokenizer.Capacity = 10'000;
        Tokenizer.Buffer   = PushArray(Tokenizer.Arena, style_token, Tokenizer.Capacity);
        Tokenizer.AtLine   = 1;

        Parser.Registery = Registery;
        Parser.Renderer  = Renderer;
    }

    // NOTE: Basically an initialization routine... Probably move this?
    if (!Registery->Arena)
    {
        u32 SentinelCount    = ThemeNameLength;
        u32 CachedStyleCount = ThemeMaxCount - ThemeNameLength;

        // NOTE: While this is better than what we did previously, I still think we "waste" memory.
        // because this is worst case allocation. We might let the arena chain and then consolidate into
        // a single one.

        size_t Footprint = sizeof(ui_style_registery);
        Footprint += SentinelCount    * sizeof(ui_cached_style);
        Footprint += CachedStyleCount * sizeof(ui_cached_style);
        Footprint += CachedStyleCount * sizeof(ui_style_name);

        memory_arena_params Params = { 0 };
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize        = OSGetSystemInfo()->PageSize;
        Params.ReserveSize       = Footprint;

        Registery->Arena        = AllocateArena(Params);
        Registery->Sentinels    = PushArena(Registery->Arena, SentinelCount    * sizeof(ui_cached_style), AlignOf(ui_cached_style));
        Registery->CachedStyles = PushArena(Registery->Arena, CachedStyleCount * sizeof(ui_cached_style), AlignOf(ui_cached_style));
        Registery->CachedName   = PushArena(Registery->Arena, CachedStyleCount * sizeof(ui_style_name)  , AlignOf(ui_style_name));
        Registery->Capacity     = CachedStyleCount;
        Registery->Count        = 0;
    }

    for (u32 FileIdx = 0; FileIdx < FileCount; FileIdx++)
    {
        byte_string FileName     = Files[FileIdx];
        os_handle   OSFileHandle = OSFindFile(FileName);
        os_file     OSFile       = OSReadFile(OSFileHandle, Tokenizer.Arena); // BUG: Can easily overflow.
  
        if (!OSFile.FullyRead)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("File with path: %s does not exist on disk."), FileName);
            continue;
        }

        b32 TokenSuccess = TokenizeStyleFile(&OSFile, &Tokenizer);
        if (!TokenSuccess)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("Failed to tokenize file. See error(s) above."));
            continue;
        }

        b32 ParseSuccess = ParseStyleFile(&Parser, Tokenizer.Buffer, Tokenizer.Count);
        if (!ParseSuccess)
        {
            WriteStyleErrorMessage(0, OSMessage_Warn, byte_string_literal("Failed to parse file. See error(s) above."));
            continue;
        }

        Tokenizer.AtLine = 1;
        Tokenizer.Count  = 0;
        PopArenaTo(Tokenizer.Arena, 0); // BUG: Wrong pop?
    }

    ReleaseArena(Tokenizer.Arena);
}

// [Internal Implementation]

internal style_token *
CreateStyleToken(UIStyleToken_Type Type, style_tokenizer *Tokenizer)
{
    style_token *Result = 0;

    if (Tokenizer->Count == Tokenizer->Capacity)
    {
        WriteStyleErrorMessage(0, OSMessage_Fatal, byte_string_literal("TOKEN LIMIT EXCEEDED."));
    }
    else
    {
        Result             = Tokenizer->Buffer + Tokenizer->Count++;
        Result->LineInFile = Tokenizer->AtLine;
        Result->Type       = Type;
    }

    return Result;
}

// [Tokenizer]

internal u32
ReadNumber(os_file *File)
{   Assert(IsDigit(PeekFile(File, 0)));

    u32 Number    = 0;
    u8  Character = PeekFile(File, 0);

    while (IsDigit(Character))
    {
        Number = Number * 10 + (Character - '0');
        AdvanceFile(File, 1);
        Character = PeekFile(File, 0);
    }

    return Number;
}


internal b32
ReadString(os_file *File, byte_string *OutString)
{
    OutString->String = PeekFilePointer(File);
    OutString->Size   = 0;

    u8 Character = *OutString->String;

    while (IsAlpha(Character))
    {
        OutString->Size += 1;
        AdvanceFile(File, 1);
        Character = PeekFile(File, 0);
    }

    b32 Result = (OutString->String != 0) && (OutString->Size != 0);
    return Result;
}

internal b32
ReadVector(os_file *File, u32 MinimumSize, u32 MaximumSize, style_token *Result)
{   Assert(PeekFile(File, 0) == '[');

    AdvanceFile(File, 1); // Skips '['

    u32 Count = 0;
    while (Count < MaximumSize)
    {
        SkipWhiteSpaces(File);

        u8 Character = PeekFile(File, 0);
        if (!IsDigit(Character))
        {
            WriteStyleErrorMessage(Result->LineInFile, OSMessage_Error, byte_string_literal("Vectors must only contain digits."));
            break;
        }

        Result->Vector.Values[Count++] = (f32)ReadNumber(File);

        SkipWhiteSpaces(File);

        Character = PeekFile(File, 0);
        if (Character == ',')
        {
            AdvanceFile(File, 1);
            continue;
        }
        else if (Character == ']')
        {
            AdvanceFile(File, 1);
            break;
        }
        else
        {
            WriteStyleErrorMessage(Result->LineInFile, OSMessage_Error, byte_string_literal("Invalid character found in vector %c"), Character);
            break;
        }
    }

    b32 Valid = (Count >= MinimumSize) && (Count <= MaximumSize);
    return Valid;
}

internal b32
TokenizeStyleFile(os_file *File, style_tokenizer *Tokenizer)
{
    b32 Success = 0;

    while (IsValidFile(File))
    {
        SkipWhiteSpaces(File);

        u8 Char = PeekFile(File, 0);

        if (IsAlpha(Char))
        {
            byte_string Identifier;
            Success = ReadString(File, &Identifier);
            if (!Success)
            {
                WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Could not parse string. EOF?"));
                return Success;
            }

            if (ByteStringMatches(Identifier, byte_string_literal("style")))
            {
                CreateStyleToken(UIStyleToken_Style, Tokenizer);
            }
            else if (ByteStringMatches(Identifier, byte_string_literal("for")))
            {
                CreateStyleToken(UIStyleToken_For, Tokenizer);
            }
            else
            {
                CreateStyleToken(UIStyleToken_Identifier, Tokenizer)->Identifier = Identifier;
            }

            continue;
        }

        if (IsDigit(Char))
        {
            CreateStyleToken(UIStyleToken_Number, Tokenizer)->UInt32 = ReadNumber(File);
            continue;
        }

        if (Char == '\r' || Char == '\n')
        {
            u8 Next = PeekFile(File, 1);
            if (Char == '\r' && Next == '\n')
            {
                Tokenizer->AtLine += 1;
                AdvanceFile(File, 1);
            }

            Tokenizer->AtLine += 1;
            AdvanceFile(File, 1);

            continue;
        }

        if (Char == ':')
        {
            if (PeekFile(File, 1) == '=')
            {
                CreateStyleToken(UIStyleToken_Assignment, Tokenizer);
                AdvanceFile(File, 2);
            }
            else
            {
                WriteStyleErrorMessage(Tokenizer->AtLine, OSMessage_Error, byte_string_literal("Stray ':'. Did you mean := ?"));
                return Success;
            }

            continue;
        }

        if (Char == '[')
        {
            read_only u32 MinimumVectorSize = 2;
            read_only u32 MaximumVectorSize = 4;
            Success = ReadVector(File, MinimumVectorSize, MaximumVectorSize, CreateStyleToken(UIStyleToken_Vector, Tokenizer));
            if (!Success)
            {
                WriteStyleErrorMessage(Tokenizer->AtLine, OSMessage_Error, byte_string_literal("Could not read vector. See error(s) above."));
                return Success;
            }

            continue;
        }

        if (Char == '"')
        {
            AdvanceFile(File, 1); // Skip the first '"'

            byte_string String;
            Success = ReadString(File, &String);
            if (!Success)
            {
                WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Could not parse string. EOF?"));
                return Success;
            }

            CreateStyleToken(UIStyleToken_String, Tokenizer)->Identifier = String;

            AdvanceFile(File, 1); // Skip the second '"'

            continue;
        }

        CreateStyleToken((UIStyleToken_Type)Char, Tokenizer);
        AdvanceFile(File, 1);
    }

    CreateStyleToken(UIStyleToken_EndOfFile, Tokenizer);

    return Success;
}

// [Parser]

internal UIStyleAttribute_Flag
GetStyleAttributeFlag(byte_string Identifier)
{
    UIStyleAttribute_Flag Result = UIStyleAttribute_None;

    // Valid Types (Clearer as a non-table)
    byte_string Size         = byte_string_literal("size");
    byte_string Color        = byte_string_literal("color");
    byte_string Padding      = byte_string_literal("padding");
    byte_string Spacing      = byte_string_literal("spacing");
    byte_string FontSize     = byte_string_literal("fontsize");
    byte_string FontName     = byte_string_literal("fontname");
    byte_string Softness     = byte_string_literal("softness");
    byte_string BorderWidth  = byte_string_literal("borderwidth");
    byte_string BorderColor  = byte_string_literal("bordercolor");
    byte_string BorderRadius = byte_string_literal("borderradius");

    if (ByteStringMatches(Identifier, Size))
    {
        Result = UIStyleAttribute_Size;
    }
    else if (ByteStringMatches(Identifier, Color))
    {
        Result = UIStyleAttribute_Color;
    }
    else if (ByteStringMatches(Identifier, Padding))
    {
        Result = UIStyleAttribute_Padding;
    }
    else if (ByteStringMatches(Identifier, Spacing))
    {
        Result = UIStyleAttribute_Spacing;
    }
    else if (ByteStringMatches(Identifier, FontSize))
    {
        Result = UIStyleAttribute_FontSize;
    }
    else if (ByteStringMatches(Identifier, FontName))
    {
        Result = UIStyleAttribute_FontName;
    }
    else if (ByteStringMatches(Identifier, Softness))
    {
        Result = UIStyleAttribute_Softness;
    }
    else if (ByteStringMatches(Identifier, BorderWidth))
    {
        Result = UIStyleAttribute_BorderWidth;
    }
    else if (ByteStringMatches(Identifier, BorderColor))
    {
        Result = UIStyleAttribute_BorderColor;
    }
    else if (ByteStringMatches(Identifier, BorderRadius))
    {
        Result = UIStyleAttribute_BorderRadius;
    }

    return Result;
}

internal b32
IsAttributeFormattedCorrectly(UIStyleToken_Type TokenType, UIStyleAttribute_Flag AttributeFlag)
{
    b32 Result = 0;

    switch (TokenType)
    {

    case UIStyleToken_Number:
    {
        Result = (AttributeFlag & UIStyleAttribute_BorderWidth) ||
                 (AttributeFlag & UIStyleAttribute_Softness   ) ||
                 (AttributeFlag & UIStyleAttribute_FontSize   );
    } break;

    case UIStyleToken_Vector:
    {
        Result = (AttributeFlag & UIStyleAttribute_BorderColor ) ||
                 (AttributeFlag & UIStyleAttribute_BorderRadius) ||
                 (AttributeFlag & UIStyleAttribute_Padding     ) ||
                 (AttributeFlag & UIStyleAttribute_Spacing     ) ||
                 (AttributeFlag & UIStyleAttribute_Color       ) ||
                 (AttributeFlag & UIStyleAttribute_Size        );
    } break;

    case UIStyleToken_String:
    {
        Result = (AttributeFlag & UIStyleAttribute_FontName);
    } break;

    default:
    {

    } break;

    }

    return Result;
}

internal b32
SaveStyleAttribute(UIStyleAttribute_Flag Attribute, style_token *Value, style_parser *Parser)
{
    switch (Attribute)
    {

    default:
    {
        WriteStyleErrorMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Invalid style supplied."));
        return 0;
    } break;

    case UIStyleAttribute_Size:         Parser->Style.Size         = Vec2F32(Value->Vector.X, Value->Vector.Y); break;
    case UIStyleAttribute_Color:        Parser->Style.Color        = NormalizedColor(Value->Vector);            break;
    case UIStyleAttribute_Padding:      Parser->Style.Padding      = Value->Vector;                             break;
    case UIStyleAttribute_Spacing:      Parser->Style.Spacing      = Vec2F32(Value->Vector.X, Value->Vector.Y); break;
    case UIStyleAttribute_FontSize:     Parser->Style.FontSize     = (f32)Value->UInt32;                        break;
    case UIStyleAttribute_FontName:     Parser->Style.Font.Name    = Value->Identifier;                         break;
    case UIStyleAttribute_Softness:     Parser->Style.Softness     = (f32)Value->UInt32;                        break;
    case UIStyleAttribute_BorderColor:  Parser->Style.BorderColor  = NormalizedColor(Value->Vector);            break;
    case UIStyleAttribute_BorderWidth:  Parser->Style.BorderWidth  = Value->UInt32;                             break;
    case UIStyleAttribute_BorderRadius: Parser->Style.CornerRadius = Value->Vector;                             break;

    }

    return 1;
}

internal void
CacheStyle(ui_style Style, byte_string Name, ui_style_registery *Registery, render_handle RendererHandle)
{
    if (Name.Size <= ThemeNameLength)
    {
        // NOTE: I believe this check is now useless since it is caught way earlier?
        ui_style_name CachedName = UIGetCachedNameFromStyleName(Name, Registery);

        if (!IsValidByteString(CachedName.Value))
        {
            // BUG: Doesn't check if it's an already referenced font.

            // Load deferred data
            if(IsValidByteString(Style.Font.Name))
            {
                Style.Font.Ref = UILoadFont(Style.Font.Name, Style.FontSize, RendererHandle, UIFontCoverage_ASCIIOnly);
            }

            ui_cached_style *Sentinel = UIGetStyleSentinel(Name, Registery);

            ui_cached_style *CachedStyle = Registery->CachedStyles + Registery->Count;
            CachedStyle->Style = Style;
            CachedStyle->Index = Registery->Count;
            CachedStyle->Next  = Sentinel->Next;

            ui_style_name *NewName = Registery->CachedName + CachedStyle->Index;
            NewName->Value.String = PushArena(Registery->Arena, Name.Size + 1, AlignOf(u8));
            NewName->Value.Size   = Name.Size;
            memcpy(NewName->Value.String, Name.String, Name.Size);

            if (Sentinel->Next)
            {
                Sentinel->Next->Next = CachedStyle;
            }
            Sentinel->Next = CachedStyle;

            Registery->Count += 1;
        }
        else
        {
            WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Two different styles cannot have the same name."));
        }
    }
    else
    {
        WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Style name exceeds maximum length of %u"), ThemeNameLength);
    }
}

internal style_token *
PeekStyleToken(style_token *Tokens, u32 TokenBufferSize, u32 Index, u32 Offset)
{
    style_token *Result = 0;

    u32 PostIndex = Index + Offset;
    if(PostIndex < TokenBufferSize)
    {
        Result = Tokens + PostIndex;
    }

    return Result;
}

internal void
ConsumeStyleTokens(style_parser *Parser, u32 Count)
{
    Parser->TokenIndex += Count;
}

internal b32
ParseStyleAttribute(style_parser *Parser, style_token *Tokens, u32 TokenBufferSize)
{
    UIStyleAttribute_Flag Flag = UIStyleAttribute_None;
    {
        style_token *AttributeName = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (AttributeName->Type != UIStyleToken_Identifier)
        {
            WriteStyleErrorMessage(AttributeName->LineInFile, OSMessage_Error, byte_string_literal("Expected: Attribute Name"));
            return 0;
        }

        Flag = GetStyleAttributeFlag(AttributeName->Identifier);
        if (Flag == UIStyleAttribute_None)
        {
            WriteStyleErrorMessage(AttributeName->LineInFile, OSMessage_Error, byte_string_literal("Invalid: Attribute Name"));
            return 0;
        }

        ConsumeStyleTokens(Parser, 1);
    }

    {
        style_token *Assignment = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (Assignment->Type != UIStyleToken_Assignment)
        {
            WriteStyleErrorMessage(Assignment->LineInFile, OSMessage_Error, byte_string_literal("Expected: Assignment"));
            return 0;
        }

        ConsumeStyleTokens(Parser, 1);      
    }

    {
        read_only bit_field ValueMask = UIStyleToken_String | UIStyleToken_Number | UIStyleToken_Vector;

        style_token *Value = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (!(Value->Type & ValueMask))
        {
            WriteStyleErrorMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Expected: Value"));
            return 0;
        }

        if (!(Flag & StyleTypeValidAttributesTable[Parser->StyleType]))
        {
            WriteStyleErrorMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Invalid attribute supplied to a theme. Invalid: %s"), UIStyleAttributeToString(Flag));
            return 0;
        }

        if (!IsAttributeFormattedCorrectly(Value->Type, Flag))
        {
            WriteStyleErrorMessage(Value->LineInFile, OSMessage_Error, byte_string_literal("Invalid formatting for %s"), UIStyleAttributeToString(Flag));
            return 0;
        }

        b32 Saved = SaveStyleAttribute(Flag, Value, Parser);
        if (!Saved)
        {
            return 0;
        }

        ConsumeStyleTokens(Parser, 1);
    }

    {
        style_token *EndOfAttribute = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (EndOfAttribute->Type != ';')
        {
            WriteStyleErrorMessage(EndOfAttribute->LineInFile, OSMessage_Error, byte_string_literal("Expected: ';' after setting an attribute."));
            return 0;
        }

        ConsumeStyleTokens(Parser, 1);
    }

    return 1;
}

internal b32
ParseStyleHeader(style_parser *Parser, style_token *Tokens, u32 TokenBufferSize)
{
    {
        style_token *Style = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (Style->Type != UIStyleToken_Style)
        {
            WriteStyleErrorMessage(Style->LineInFile, OSMessage_Error, byte_string_literal("Expected: 'Style'"));
            return 0;
        }

        ConsumeStyleTokens(Parser, 1);
    }

    {
        style_token *Name = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (Name->Type != UIStyleToken_String)
        {
            WriteStyleErrorMessage(Name->LineInFile, OSMessage_Error, byte_string_literal("Expected: '\"style_name\""));
            return 0;
        }

        ui_style_name CachedName = UIGetCachedNameFromStyleName(Name->Identifier, Parser->Registery);
        if (IsValidByteString(CachedName.Value))
        {
            WriteStyleErrorMessage(Name->LineInFile, OSMessage_Error, byte_string_literal("A style with the same name already exists."));
            return 0;
        }

        Parser->StyleName = Name->Identifier;

        ConsumeStyleTokens(Parser, 1);
    }

    {
        style_token *For = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (For->Type != UIStyleToken_For)
        {
            WriteStyleErrorMessage(For->LineInFile, OSMessage_Error, byte_string_literal("Expected: 'For'"));
            return 0;
        }

        ConsumeStyleTokens(Parser, 1);
    }

    {
        style_token *Type = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (Type->Type != UIStyleToken_Identifier)
        {
            WriteStyleErrorMessage(0, OSMessage_Error, byte_string_literal("Expected: 'Type'"));
            return 0;
        }

        byte_string WindowString = byte_string_literal("window");
        byte_string ButtonString = byte_string_literal("button");
        byte_string LabelString  = byte_string_literal("label");

        if (ByteStringMatches(Type->Identifier, WindowString))
        {
            Parser->StyleType = UIStyle_Window;
        }
        else if (ByteStringMatches(Type->Identifier, ButtonString))
        {
            Parser->StyleType = UIStyle_Button;
        }
        else if (ByteStringMatches(Type->Identifier, LabelString))
        {
            Parser->StyleType = UIStyle_Label;
        }
        else
        {
            WriteStyleErrorMessage(Type->LineInFile, OSMessage_Error, byte_string_literal("Expected: 'Type'"));
            return 0;
        }

        ConsumeStyleTokens(Parser, 1);
    }

    {
        style_token *NextToken = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        if (NextToken->Type != '{')
        {
            WriteStyleErrorMessage(NextToken->LineInFile, OSMessage_Error, byte_string_literal("Expect: '{' after style header."));
            return 0;
        }

        ConsumeStyleTokens(Parser, 1);

        while (NextToken->Type != '}')
        {
            if (NextToken == '\0')
            {
                WriteStyleErrorMessage(NextToken->LineInFile, OSMessage_Error, byte_string_literal("Unexpected end of file."));
                return 0;
            }

            if (!ParseStyleAttribute(Parser, Tokens, TokenBufferSize))
            {
                WriteStyleErrorMessage(NextToken->LineInFile, OSMessage_Error, byte_string_literal("Unexpected end of file."));
                return 0;
            }

            NextToken = PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0);
        }

        ConsumeStyleTokens(Parser, 1);
    }

    return 1;
}

internal b32
ParseStyleFile(style_parser *Parser, style_token *Tokens, u32 TokenBufferSize)
{
    while (PeekStyleToken(Tokens, TokenBufferSize, Parser->TokenIndex, 0)->Type != UIStyleToken_EndOfFile)
    {
        if (!ParseStyleHeader(Parser, Tokens, TokenBufferSize))
        {
            return 0;
        }

        CacheStyle(Parser->Style, Parser->StyleName, Parser->Registery, Parser->Renderer);

        Parser->StyleName = ByteString(0, 0);
        Parser->Style     = (ui_style){ 0 };
        Parser->StyleType = UIStyle_None;
    }

    return 1;
}

// [Success Handling]

internal read_only char *
UIStyleAttributeToString(UIStyleAttribute_Flag Flag)
{
    switch (Flag)
    {

    case UIStyleAttribute_None:         return "None";
    case UIStyleAttribute_Size:         return "Size";
    case UIStyleAttribute_Color:        return "Color";
    case UIStyleAttribute_Padding:      return "Padding";
    case UIStyleAttribute_Spacing:      return "Spacing";
    case UIStyleAttribute_FontSize:     return "Font Size";
    case UIStyleAttribute_FontName:     return "Font Name";
    case UIStyleAttribute_Softness:     return "BorderSoftness";
    case UIStyleAttribute_BorderColor:  return "BorderColor";
    case UIStyleAttribute_BorderWidth:  return "BorderWidth";
    case UIStyleAttribute_BorderRadius: return "BorderRadius";
    default:                            return "Unknown";

    }
}

internal void
WriteStyleErrorMessage(u32 LineInFile, OSMessage_Severity Severity, byte_string Format, ...)
{
    va_list Args;
    __crt_va_start(Args, Format);
    __va_start(&Args, Format);

    u8  Buffer[Kilobyte(2)];

    byte_string ErrorString = ByteString(Buffer, 0);
    ErrorString.Size  = snprintf ((char *) Buffer, sizeof(Buffer), "[Parsing Error: Line=%u] -> ", LineInFile);
    ErrorString.Size += vsnprintf((char *)(Buffer + ErrorString.Size), sizeof(Buffer), (char *)Format.String, Args);

    OSLogMessage(ErrorString, Severity);

    __crt_va_end(Args);
}
internal void 
ProccessInputMessage(os_button_state *NewState, b32 IsDown)
{
    if(NewState->EndedDown != IsDown)
    {
        NewState->EndedDown            = IsDown;
        NewState->HalfTransitionCount += 1;
    }
}

// [Agnostic File API]

internal b32 
OSFileIsValid(os_file *File)
{
    b32 Result = File->At < File->Content.Size;
    return Result;
}

internal void 
OSFileIgnoreWhiteSpaces(os_file *File)
{
    while(OSFileIsValid(File) && IsWhiteSpace(File->Content.String[File->At]))
    {
        ++File->At;
    }
}

internal u8
OSFileGetChar(os_file *File)
{
    u8 Result = File->Content.String[File->At];
    return Result;
}

internal u8
OSFileGetNextChar(os_file *File)
{
    File->At  += 1;
    u8 Result  = OSFileGetChar(File);

    return Result;
}
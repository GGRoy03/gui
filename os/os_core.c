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
OSIsValidFile(os_file *File)
{
    b32 Result = File->At < File->Content.Size;
    return Result;
}

internal void 
OSIgnoreWhiteSpaces(os_file *File)
{
    while(OSIsValidFile(File) && IsWhiteSpace(File->Content.String[File->At]))
    {
        ++File->At;
    }
}

internal u8
OSGetFileChar(os_file *File)
{
    u8 Result = File->Content.String[File->At];
    return Result;
}

internal u8
OSGetNextFileChar(os_file *File)
{
    File->At  += 1;
    u8 Result = OSGetFileChar(File);
    return Result;
}
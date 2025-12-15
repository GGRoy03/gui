static byte_string 
ByteString(char *String, uint64_t Size)
{
    byte_string Result = { String, Size };
    return Result;
}


static bool
IsValidByteString(byte_string Input)
{
    bool Result = (Input.String) && (Input.Size);
    return Result;
}


static uint64_t
HashByteString(byte_string Input)
{
    uint64_t Result = XXH3_64bits(Input.String, Input.Size);
    return Result;
}

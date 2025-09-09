// [API IMPLEMENTATION]

internal byte_string 
ByteString(u8 *String, u64 Size)
{
	byte_string Result = { String, Size };
	return Result;
}
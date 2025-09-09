#pragma once

// [CORE TYPES]

typedef struct byte_string
{
	u8 *String;
	u64 Size;
} byte_string;

typedef struct wide_string
{
	u16 *String;
	u64  Size;
} wide_string;

// [API]

#define byte_string_literal(String) ByteString(String, sizeof(String))

internal byte_string ByteString(u8 *String, u64 Size);
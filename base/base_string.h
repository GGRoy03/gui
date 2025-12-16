#pragma once

struct byte_string
{
    char    *String;
    uint64_t Size;
};

// Helper Macros

#define str8_lit(String)  ByteString(String, sizeof(String) - 1)
#define str8_comp(String) {(char *)(String), sizeof(String) - 1}

// [Constructors]

static byte_string ByteString         (char *String, uint64_t Size);
static bool        IsValidByteString  (byte_string Input);
static uint64_t    HashByteString     (byte_string Input);

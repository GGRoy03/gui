#pragma once

// ====================================================================================
// @Internal: Standard Includes & Context Cracking & Utility Macros

#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <stdint.h>

#if defined(_MSV_VER)
    #define GUI_MSVC 1
#elif defined(__clang__)
    #define GUI_CLANG 1
#elif defined(__GNUC__)
    #define GUI_GCC 1
#else
    #error "GUI: Unknown Compiler"
#endif

#if GUI_MSVC || GUI_CLANG
    #define AlignOf(T) __alignof(T)
#elif GUI_GCC
    #define AlignOf(T) __alignof__(T)
#endif

#if GUI_MSVC
    #define GUI_FindFirstBit(Mask) _tzcnt_u32(Mask)
#elif GUI_CLANG || GUI_GCC
    #define GUI_FindFirstBit(Mask) __builtin_ctz(Mask)
#endif

#define GUI_NAMECONCAT2(a, b)   a##b
#define GUI_NAMECONCAT(a, b)    GUI_NAMECONCAT2(a, b)

#define GUI_ASSERT(Cond)        do { if (!(Cond)) __debugbreak(); } while (0)
#define GUI_UNUSED(X)           (void)(X)

#define GUI_ARRAYCOUNT(A)       (sizeof(A) / sizeof(A[0]))
#define GUI_ISPOWEROFTWO(Value) (((Value) & ((Value) - 1)) == 0)

#define GUI_KILOBYTE(n)         (((uint64_t)(n)) << 10)
#define GUI_MEGABYTE(n)         (((uint64_t)(n)) << 20)
#define GUI_GIGABYTE(n)         (((uint64_t)(n)) << 30)

#define GUI_BIT(n)              ((uint64_t)( 1ULL << ((n) - 1)))
#define GUI_BITMASK(n)          ((uint64_t)((1ULL << (n)) - 1ULL))

// [Alignment]

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))
#define AlignMultiple(Value, Multiple) (Value += Multiple - 1); (Value -= Value % Multiple);

// [Clampers/Min/Max]

#define Min(A,B) (((A)<(B))?(A):(B))
#define Max(A,B) (((A)>(B))?(A):(B))
#define ClampTop(A,X) Min(A,X)
#define ClampBot(X,B) Max(X,B)

// [Memory Ops]

#define MemoryCopy(dst, src, size) memmove((dst), (src), (size))
#define MemorySet(dst, byte, size) memset((dst), (byte), (size))
#define MemoryCompare(a, b, size)  memcmp((a), (b), (size))
#define MemoryZero(s,z)            memset((s),0,(z))

// [Loop Macros]

#define ForEachEnum(Type, Count, It)  for(uint32_t It = (uint32_t)0; (uint32_t)It < Count; It = ((uint32_t)It + 1))
#define DeferLoop(Begin, End) for(int GUI_NAMECONCAT(_defer_, __LINE__) = ((Begin), 0); !GUI_NAMECONCAT(_defer_, __LINE__); GUI_NAMECONCAT(_defer_, __LINE__)++, (End))

// [Linked List]

#define AppendToLinkedList(List, Node, Counter)       if(!List->First) List->First = Node; if(List->Last) List->Last->Next = Node; List->Last = Node; ++Counter
#define AppendToDoublyLinkedList(List, Node, Counter) if(!List->First) List->First = Node; if(List->Last) List->Last->Next = Node; Node->Prev = List->Last; List->Last = Node; ++Counter;
#define IterateLinkedList(List, Type, N)             for(Type N = List->First; N != 0; N = N->Next)
#define IterateLinkedListBackward(List, Type, N)     for(Type N = List->Last ; N != 0; N = N->Prev)

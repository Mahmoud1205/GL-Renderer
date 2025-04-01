#pragma once

#include <cstdint>

/// @brief ANSI bold red color.
#define ZR_B_RED "\033[1;31m"
/// @brief ANSI text formatting reset.
#define ZR_ANSI_RESET "\033[0m"
// TODO: disable asserts in release
#define ZR_ASSERT(inExpression, inMessage, ...) \
	do { \
		if (!(inExpression)) { \
			fprintf(stderr, "%sZR_ASSERT: (%s:%d): " inMessage "%s\n", ZR_B_RED, __FILE__, __LINE__, __VA_ARGS__, ZR_ANSI_RESET); \
			__debugbreak(); \
			abort(); } \
	} while (0)

#define STATIC_ASSERT static_assert

#define OFFSETOF __builtin_offsetof

#define REQUEST_DEDICATED_GPU() extern "C" __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001

#define sinline static inline
#define sconst static inline const

typedef size_t			usize;
typedef uintptr_t		uptr;
typedef intptr_t		iptr;
typedef unsigned char	uchar;
typedef unsigned short	ushort;
typedef unsigned int	uint;
typedef unsigned long	ulong;

typedef signed char i8;
STATIC_ASSERT(sizeof(i8) == 1, "Expected i8 to be 1 byte.");

typedef unsigned char u8;
STATIC_ASSERT(sizeof(u8) == 1, "Expected u8 to be 1 byte.");

typedef int_fast8_t i8f;
STATIC_ASSERT(sizeof(i8f) >= 1, "Expected i8f to be at least 1 byte.");

typedef uint_fast8_t u8f;
STATIC_ASSERT(sizeof(u8f) >= 1, "Expected u8f to be at least 1 byte.");

typedef unsigned short int u16;
STATIC_ASSERT(sizeof(u16) == 2, "Expected u16 to be 2 bytes.");

typedef short int i16;
STATIC_ASSERT(sizeof(i16) == 2, "Expected i16 to be 2 bytes.");

typedef int_fast16_t i16f;
STATIC_ASSERT(sizeof(i16f) >= 2, "Expected i16f to be at least 2 bytes.");

typedef uint_fast16_t u16f;
STATIC_ASSERT(sizeof(u16f) >= 2, "Expected u16f to be at least 2 bytes.");

typedef int i32;
STATIC_ASSERT(sizeof(i32) == 4, "Expected i32 to be 4 bytes.");

typedef unsigned int u32;
STATIC_ASSERT(sizeof(u32) == 4, "Expected u32 to be 4 bytes.");

typedef long long int i64;
STATIC_ASSERT(sizeof(i64) == 8, "Expected i64 to be 8 bytes.");

typedef unsigned long long int u64;
STATIC_ASSERT(sizeof(u64) == 8, "Expected u64 to be 8 bytes.");

typedef float f32;
STATIC_ASSERT(sizeof(f32) == 4, "Expected f32 to be 4 bytes.");

typedef double f64;
STATIC_ASSERT(sizeof(f64) == 8, "Expected f64 to be 8 bytes.");

// dont static assert for long floats cuz theyre size can be 80 or 128
// depending on platform
typedef long double fll;

STATIC_ASSERT(sizeof(bool) == 1, "Expected bool to be 1 byte.");

#define SIZE_KB(inSize) (u64(inSize * 1024))
#define SIZE_MB(inSize) (u64(inSize * 1024 * 1024))
#define SIZE_GB(inSize) (u64(inSize * 1024 * 1024 * 1024))

constexpr u64 operator"" _kb(u64 inSize) { return SIZE_KB(inSize); }
constexpr u64 operator"" _mb(u64 inSize) { return SIZE_MB(inSize); }
constexpr u64 operator"" _gb(u64 inSize) { return SIZE_GB(inSize); }

constexpr f64 operator"" _kb(fll inSize) { return inSize * 1024.0;						}
constexpr f64 operator"" _mb(fll inSize) { return inSize * 1024.0 * 1024.0;				}
constexpr f64 operator"" _gb(fll inSize) { return inSize * 1024.0 * 1024.0 * 1024.0;	}

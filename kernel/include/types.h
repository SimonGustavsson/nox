#ifndef NOX_TYPES_H
#define NOX_TYPES_H

#include <stdbool.h> // C Does not have bool :(
#include <stdint.h>

#if PLATFORM_BITS==32
typedef uint32_t            native_uint_t;
typedef int32_t             native_int_t;
#elif PLATFORM_BITS==64
typedef uint64_t            native_uint_t;
typedef int64_t             native_int_t;
#else
#error  PLATFORM_BITS has an invalid value
#endif

typedef native_uint_t       size_t;

#define NULL                (0)
#define UINT32_MAXVALUE  4294967295
#define UINT16_MAXVALUE  65535

#endif

#ifndef _TYPES_H
#define _TYPES_H

typedef unsigned char       uint8_t;
typedef unsigned short      uint16_t;
typedef unsigned int        uint32_t;
typedef unsigned long long  uint64_t;

typedef signed char         int8_t;
typedef signed short        int16_t;
typedef signed int          int32_t;
typedef signed long long    int64_t;

typedef uint64_t            size_t;
typedef uint64_t            phys_addr_t;
typedef uint64_t            virt_addr_t;

#define NULL                ((void*)0)
#define true                1
#define false               0
typedef uint8_t             bool;

#define PACKED              __attribute__((packed))
#define NORETURN            __attribute__((noreturn))
#define ALIGNED(x)          __attribute__((aligned(x)))

NORETURN void kernel_panic(const char *file, int line, const char *msg);

#define KERNEL_PANIC(msg)   kernel_panic(__FILE__, __LINE__, msg)
#define ASSERT(cond)        do { if (!(cond)) KERNEL_PANIC("Assertion failed: " #cond); } while(0)

#endif
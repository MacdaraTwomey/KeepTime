
#ifndef BASE_H
#define BASE_H

#include <stdint.h>

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef  i8  s8;
typedef  i16 s16;
typedef  i32 s32;
typedef  i64 s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int32_t  b32;
typedef int16_t  b16;

typedef float  f32;
typedef double f64;

static_assert(sizeof(u8)  == sizeof(i8), "");
static_assert(sizeof(u16) == sizeof(i16), "");
static_assert(sizeof(u32) == sizeof(i32), "");
static_assert(sizeof(u64) == sizeof(i64), "");

static_assert(sizeof(u8)  == 1, "");
static_assert(sizeof(u16) == 2, "");
static_assert(sizeof(u32) == 4, "");
static_assert(sizeof(u64) == 8, "");

#ifndef CIAN_DEBUG_TRAP
#if defined(_MSC_VER)
// debugbreak and Assert is not triggered when stepping through code in debugger only, while running (or pressing continue in debugger)

// This is what SDL does instead of including intrin.h
extern void __cdecl __debugbreak(void);
#define CIAN_DEBUG_TRAP() __debugbreak() 
#elif defined(__GNUC__)
#define CIAN_DEBUG_TRAP() __builtin_trap()
#else
#define CIAN_DEBUG_TRAP() (*(int *)0 = 0;)
#endif
#endif

#define CIAN_STRINGIFY1(expr) #expr
#define CIAN_STRINGIFY(expr) CIAN_STRINGIFY1(expr)

#define CIAN_ASSERT1(cond, file, func, line)  do { \
if (!static_cast<bool>(cond)) { \
PrintAssertMessage(CIAN_STRINGIFY(cond), (file), (func), (line)); \
CIAN_DEBUG_TRAP(); \
} \
} while(0) 

#define Assert(cond) CIAN_ASSERT1(cond, __FILE__, __func__, __LINE__)

void
PrintAssertMessage(const char *assertion, const char *file, const char *func, int line)
{
    const char *char_after_last_slash = file;
    for (const char *c = file; c[0]; ++c)
    {
        if ((*c == '\\' || *c == '/') && c[1])
        {
            char_after_last_slash = c + 1;
        }
    }
    
    fprintf(stderr, "Assertion: (%s), %s, %s():line %i\n", assertion, char_after_last_slash, func, line);
    fflush(stderr);
}


#define KB(n) ((n) * 1024LLU)
#define MB(n) KB((n) * 1024LLU)
#define GB(n) MB((n) * 1024LLU)
#define TB(n) GB((n) * 1024LLU)

#define Minimum(a, b) ((a) < (b) ? (a) : (b))
#define Maximum(a, b) ((a) > (b) ? (a) : (b))
#define ClampTop(a, b)    Minimum((a), (b))
#define ClampBottom(a, b) Maximum((a), (b))

template<class T>
constexpr const T& Clamp(const T& v, const T& lo, const T& hi)
{
    Assert( !(hi < lo) );
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

template <typename T, size_t n>
constexpr size_t ArrayCount(T(&)[n])
{
    return n;
}

#define CONCATENATE_1(x, y) x##y // Can concat literal characters xy if x and y are macro definitions
#define CONCATENATE_2(x, y) CONCATENATE_1(x, y) // So macro expand x and y first

#ifndef __COUNTER__
#define ANON_VARIABLE(x)    CONCATENATE_2(x, __COUNTER__)
#else
#define ANON_VARIABLE(x)    CONCATENATE_2(x, __LINE__)
#endif

// NOTE:
// defer calls are executed in reverse order (later in scope called first)
// If deferred function takes arguments, the arguments values can change before being called at
// scope exit.

#define Defer(code) auto ANON_VARIABLE(_defer_) = gb_defer_func([&]()->void{code;})
// expanded to:
// auto _defer_347 = gb__defer_func([&]() -> void { tprint("SCOPE EXIT"); });


/*
 * Defer C++ implementation by Ginger Bill
 * gb single-file public domain libraries for C & C++
 * https://github.com/gingerBill/gb
 */
template <typename T> struct gbRemoveReference       { typedef T Type; };
template <typename T> struct gbRemoveReference<T &>  { typedef T Type; };
template <typename T> struct gbRemoveReference<T &&> { typedef T Type; };

template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &t)  { return static_cast<T &&>(t); }
template <typename T> inline T &&gb_forward(typename gbRemoveReference<T>::Type &&t) { return static_cast<T &&>(t); }
template <typename T> inline T &&gb_move   (T &&t)                                   { return static_cast<typename gbRemoveReference<T>::Type &&>(t); }
template <typename F>
struct gbprivDefer
{
    F f;
    gbprivDefer(F &&f) : f(gb_forward<F>(f)) {}
    ~gbprivDefer() { f(); }
};
template <typename F> gbprivDefer<F> gb_defer_func(F &&f) { return gbprivDefer<F>(gb_forward<F>(f)); }


struct arena
{
    u8 *Base;
    u64 Pos;
    u64 Commit;
    u64 Capacity;
    
    u32 TempCount;
};

struct temp_memory
{
    arena *Arena;
    u64 StartPos;
};

enum arena_push_flags
{
    ArenaPushFlags_None = 0,
    ArenaPushFlags_ClearToZero = 1,
};

arena_push_flags DefaultArenaPushFlags()
{
    arena_push_flags Flags = ArenaPushFlags_ClearToZero;
    return Flags;
}

arena_push_flags NoClear()
{
    arena_push_flags Flags = ArenaPushFlags_None;
    return Flags;
}

void *PushSize(arena *Arena, u64 Size, u32 Alignment, u8 ArenaPushFlags = DefaultArenaPushFlags());
void MemoryCopy(u64 Size, void *Source, void *Dest);
void MemoryZero(u64 Size, void *Memory);
void *CheckTypeAlignment(void *Ptr, u32 Alignment); 


#if MONITOR_DEBUG
// May need to prefix the varargs with '##' (e.g. " ##__VA_ARGS__") this removes comma if there are no arguments)
#define Allocate(Arena, Size, Type, ...) \
(Type *)CheckTypeAlignment(PushSize((Arena), (Size)*sizeof(Type), alignof(Type), __VA_ARGS__), \
alignof(Type)) 
#else
#define Allocate(Arena, Size, Type, ...) (Type *)PushSize((Arena), (Size)*sizeof(Type), alignof(Type),  __VA_ARGS__)
#endif



#endif //BASE_H

#pragma once

// ver 0.011

// TODO: Look at musl (simple implementation of c and unix libs)

#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

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

typedef float  r32;
typedef double r64;

static_assert(sizeof(u8)  == sizeof(i8), "");
static_assert(sizeof(u16) == sizeof(i16), "");
static_assert(sizeof(u32) == sizeof(i32), "");
static_assert(sizeof(u64) == sizeof(i64), "");

static_assert(sizeof(u8)  == 1, "");
static_assert(sizeof(u16) == 2, "");
static_assert(sizeof(u32) == 4, "");
static_assert(sizeof(u64) == 8, "");

#if defined(_WIN64) || defined(__x86_64__) || defined(__64BIT__) || defined(_M_X64)
#ifndef CIAN_ARCH_64
#define CIAN_ARCH_64 1
#endif
#else
#error cian.h only supports 64-bit systems currently
#endif

#if defined(_WIN32)
//#include <windows.h>
#elif defined (__linux__)
#else
#error ravel.h only supports Windows and Linux
#endif

#include <assert.h>

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

void
cian_print_assert_msg_and_panic(const char *assertion, const char *file, const char *func, int line)
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

#define CIAN_STRINGIFY1(expr) #expr
#define CIAN_STRINGIFY(expr) CIAN_STRINGIFY1(expr)

#define CIAN_ASSERT1(cond, file, func, line)  do { \
if (!(cond)) { \
cian_print_assert_msg_and_panic(CIAN_STRINGIFY((cond)), (file), (func), (line)); \
CIAN_DEBUG_TRAP(); \
} \
} while(0) 

#define Assert(cond) CIAN_ASSERT1(cond, __FILE__, __func__, __LINE__)

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes((Value)) * 1024LL)
#define Gigabytes(Value) (Megabytes((Value)) * 1024LL)


template <typename T, size_t n>
constexpr size_t array_count(T(&)[n])
{
    return n;
}

template<class T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
    Assert( !(hi < lo) );
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

// ------------------------------------------------------------------------------------------------
// This is suspect in lieu of bugs in term on linux
// NOTE: If values passed to defered function change, this is reflected when the function is called.

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

#define defer(code) auto ANON_VARIABLE(_defer_) = gb_defer_func([&]()->void{code;})
// expanded to:
// auto _defer_347 = gb__defer_func([&]() -> void { tprint("SCOPE EXIT"); });
// -------------------------------------------------------------------------------------------------

/* -----------------------------------------------------------------------------------------
  Debug printing code
  (warning: disgusting c++ template code)

 BUGS:
 TODO: TPRINT void * buffers are being pronted as a pointer rather than giving a compile error saying expected a char * array and recieved a void *.
 also wchar_t need %ls i think.

Usage:
- NOTE: Debug code only (because of size restrictions, and possible bugs)
- Integers, floats, strings, bools (in single argument tprint), pointers allowed

tprint(variable);
tprint("% variable slot", x);
tprint_col(Red, "Hello %", name);

- Doesn't handle other format specifiers such as width, precision etc

Future nicehaves TODO:
- Allow other format specifiers (maybe above todo would allow this also)
- tprint() just prints newline
- Print (null) explicitly if null pointer passed (GCC and MSVC do this I think but it's
  implementation defined).

// Example code to test

int i = 9;
char c = 'H';
double d = 3.0;
float f = 4.5f;
int *ip = &i;
double *dp = &d;
void *vp = (void*)0x232323344;
char *str = "test";

tprint_col(Red, "TEST");
tprint_col(Blue, "var: % %", i, str);

tprint(i);
tprint(c);
tprint(d);
tprint(f);
tprint(ip);
tprint(str);
tprint("Hello: %", i);
tprint("Hello: %", c);
tprint("Hello: %", ip);
tprint("Hello: %, here: %", str, f);
tprint("Hello: %", &dp);

--------------------------------------------------
// Things that same_type<T1, T2> evaluates as true
// On 64 bit Windows

// const char * != char *

// 8 bit integers
// signed char == int8_t
// unsigned char == uint8_t
// char != signed char != unsigned char

// 16 bit integers
// short == signed short == int16_t
// unsigned short == uint16_t

// 32 bit integers
// int == signed int == int32_t
// unsigned int == uint32_t
// long != int != int32_t
// unsigned long != uint32_t
// long == signed long

// 64 bit integers
// uint64_t == size_t
// unsigned long long == size_t
// long long == signed long long == int64_t
// unsigned long long == uint64_t
// double != long double (even if size is or isn't the same)
// Single variable, newline terminating tprint functions
*/

#define DEFINE_GET_SPECIFIER(type, specifier) const char *_get_specifier(type x) {return specifier;}

DEFINE_GET_SPECIFIER(char               , "%c")
DEFINE_GET_SPECIFIER(signed char        , "%c")
DEFINE_GET_SPECIFIER(unsigned char      , "%hhu")
DEFINE_GET_SPECIFIER(long               , "%li")
DEFINE_GET_SPECIFIER(unsigned long      , "%lu")
DEFINE_GET_SPECIFIER(float              , "%f")
DEFINE_GET_SPECIFIER(double             , "%lf")
DEFINE_GET_SPECIFIER(long double        , "%Lf")
DEFINE_GET_SPECIFIER(short              , "%hi")
DEFINE_GET_SPECIFIER(unsigned short     , "%hu")
DEFINE_GET_SPECIFIER(int                , "%i")
DEFINE_GET_SPECIFIER(unsigned int       , "%u")
DEFINE_GET_SPECIFIER(long long          , "%lli")
DEFINE_GET_SPECIFIER(unsigned long long , "%llu")

DEFINE_GET_SPECIFIER(const char *       , "%s")
DEFINE_GET_SPECIFIER(char *             , "%s")

DEFINE_GET_SPECIFIER(bool               , "%i")

// Handles pointers other than [const] char *
template<typename T>
const char *_get_specifier(T *x) { return "%p"; }


// Cant re-define function
// DEFINE_GET_SPECIFIER(int8_t             , "%hhi")
// DEFINE_GET_SPECIFIER(int16_t            , "%hi")
// DEFINE_GET_SPECIFIER(int32_t            , "%i")
// DEFINE_GET_SPECIFIER(int64_t            , "%lli")
// DEFINE_GET_SPECIFIER(uint8_t            , "%hhu")
// DEFINE_GET_SPECIFIER(uint16_t           , "%hu")
// DEFINE_GET_SPECIFIER(uint32_t           , "%u")
// DEFINE_GET_SPECIFIER(uint64_t           , "%llu")

struct cian_false_type { static constexpr bool value = false; };
struct cian_true_type  { static constexpr bool value = true; };

template<typename T, typename U> struct same_type : cian_false_type {};
template<typename T> struct same_type<T,T> : cian_true_type {}; //specialization

template< class T > struct remove_const          { typedef T type; };
template< class T > struct remove_const<const T> { typedef T type; };
template< class T > struct remove_volatile             { typedef T type; };
template< class T > struct remove_volatile<volatile T> { typedef T type; };

template< class T >
struct remove_cv {
    typedef typename remove_volatile<typename remove_const<T>::type>::type type;
};

template< class T > struct is_pointer_helper     : cian_false_type {};
template< class T > struct is_pointer_helper<T*> : cian_true_type {};
template< class T > struct is_pointer : is_pointer_helper<typename remove_cv<T>::type> {};

#define CIAN_VALID_TYPE(T) (same_type<T, const char *>::value   || same_type<T, char *>::value || \
same_type<T, char>::value           || same_type<T, signed char>::value || \
same_type<T, unsigned char >::value || same_type<T, long>::value || \
same_type<T, unsigned long>::value  || same_type<T, float>::value || \
same_type<T, double>::value         || same_type<T, long double>::value || \
same_type<T, short>::value          || same_type<T, unsigned short>::value || \
same_type<T, int>::value            || same_type<T, unsigned int>::value || \
same_type<T, long long>::value      || same_type<T, unsigned long long>::value || \
is_pointer<T>::value)

template<typename T>
void tprint(T var)
{
    static_assert(CIAN_VALID_TYPE(T),
                  "static assert: invalid type passed to tprint()."
                  "Integer, floating point or a constant string is required");
    
    printf(_get_specifier(var), var);
    printf("\n");
}
void tprint(bool var)
{
    printf("%s\n", (var) ? "true" : "false");
}

template<typename T>
char *get_spec(T x)
{
    static_assert(CIAN_VALID_TYPE(T) || same_type<T, bool>::value,
                  "static assert: invalid type passed to tprint()."
                  "Integer, floating point or a constant string is required");
    return (char *)_get_specifier(x);
};

// Multiple variable, newline terminating tprint
template<typename... Targs>
void tprint(const char *str, Targs... args)
{
#if 1
    // This versions is hopefully faster
    
    // NOTE: Doesn't handle booleans
    // Max string size must be <= 4000
    
    // Have to add 1 for some reason I dont understand
    char *specifiers[1+sizeof...(args)] = { get_spec(args)... };
    
    // May not gracefully handle arument number and '%' number mismatch
    if (strlen(str) > 3900)
    {
        // Need extra room for specifiers
        Assert(0);
        return;
    }
    
    char buf[4096];
    char *at = buf;
    i32 i = 0;
    for (; str[0]; ++str)
    {
        if (str[0] == '%')
        {
            if (i < sizeof...(args))
            {
                while (*specifiers[i])
                {
                    *at++ = *specifiers[i]++;
                }
                
                ++i;
            }
            else
            {
                // Include extra so it doesn't get read as a specifiers for non-existant variable
                *at++ = '%';
                *at++ = '%';
            }
        }
        else
        {
            *at++ = *str;
        }
    }
    
    *at++ = '\n';
    *at = '\0';
    printf(buf, args...);
    
    
#elif
    
    // need this function signature to run this
    // template<typename T, typename... Targs>
    // void tprint(const char *str, T var, Targs... args)
    
    for (; str[0]; ++str)
    {
        if (str[0] == '%')
        {
            // TODO: Replace with if constexpr
            // Have to call this way because tprint(bool) null terminates
            if (same_type<T, bool>::value)
                printf(_get_specifier(var), (var) ? "true" : "false");
            else
                printf(_get_specifier(var), var);
            
            tprint(str + 1, args...);
            return;
        }
        else
        {
            printf("%c", *str);
        }
    }
    
    // This catches calls to tprint with too many args for '%' slots, that would normally
    // like to call single arg tprint and be newline terminated
    printf("\n");
#endif
}

template<typename... Targs>
void cian_snprintf(char *buf, size_t n, const char *str, Targs... args)
{
    char *specifiers[1+sizeof...(args)] = { get_spec(args)... };
    
    // Doesn't handle booleans
    // May not gracefully handle arument number and '%' number mismatch
    
    if (n >= 3900)
    {
        Assert(0);
        return;
    }
    
    char format_str[4094];
    char *at = format_str;
    i32 i = 0;
    for (; str[0]; ++str)
    {
        if (str[0] == '%')
        {
            if (i < sizeof...(args))
            {
                while (*specifiers[i])
                {
                    *at++ = *specifiers[i]++;
                }
                
                ++i;
            }
            else
            {
                // Include extra so it doesn't get read as a specifiers for non-existant variable
                *at++ = '%';
                *at++ = '%';
            }
        }
        else
        {
            *at++ = *str;
        }
    }
    
    *at = '\0';
    snprintf(buf, n, format_str, args...);
}


// ------------------------------------------------------------------------------------------


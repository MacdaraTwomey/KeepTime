#pragma once

// ver 0.12
// added newline to print_assert_msg

// Windows will always load kernel32.dll

// Undef stuff??

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

typedef int32_t  bool32;

static_assert(sizeof(u8)  == sizeof(i8), "");
static_assert(sizeof(u16) == sizeof(i16), "");
static_assert(sizeof(u32) == sizeof(i32), "");
static_assert(sizeof(u64) == sizeof(i64), "");

static_assert(sizeof(u8)  == 1, "");
static_assert(sizeof(u16) == 2, "");
static_assert(sizeof(u32) == 4, "");
static_assert(sizeof(u64) == 8, "");

#if defined(_WIN64) || defined(__x86_64__) || defined(__64BIT__) || defined(_M_X64)
#ifndef RVL_ARCH_64
#define RVL_ARCH_64 1
#endif
#else
#error ravel.h only supports 64-bit systems currently
// #ifndef RVL_ARCH_32
// #ndifdefine RVL_ARCH_32 1
// #endif
#endif

#if defined(_WIN32)
// Or use lean and mean, because windows defines lots of macros unconditionally
// #define NOMINMAX
#include <windows.h>
#elif defined (__linux__)
#include <stdio.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#else
#error ravel.h only supports Windows and Linux
#endif

// IMPORTANT:
// C's <assert.h> has function assert() which could get confused with my macro

// NOTE: Must wrap macro parameters in parens e.g. BAD: if(!true | false), GOOD: if(!(true | false))
// Wrapping in do {}while(0) allows you to use if (cond) assert(line < max);
// No semicolon after while(0) is intentional

// Useful article: http://cnicholson.net/2009/02/stupid-c-tricks-adventures-in-assert/

#if defined(_MSC_VER)
#include <intrin.h>
#define RVL_DEBUG_TRAP() __debugbreak()
#elif defined(__GNUC__)
#define RVL_DEBUG_TRAP() __builtin_trap()
#else
#define RVL_DEBUG_TRAP() (*(int *)0 = 0;)
#endif

const char *
strip_path(const char *filepath)
{
    const char *char_after_last_slash = filepath;
    for (const char *c = filepath; c[0]; ++c)
    {
        if ((*c == '\\' || *c == '/') && c[1])
        {
            char_after_last_slash = c + 1;
        }
    }
    
    return char_after_last_slash;
}

void
print_assert_msg(const char *assertion, const char *file, const char *func, int line)
{
    file = strip_path(file);
    fprintf(stderr, "Assertion: (%s), %s, %s():line %i\n", assertion, file, func, line);
}

#define RVL_STRINGIFY1(expr) #expr
#define RVL_STRINGIFY(expr) RVL_STRINGIFY1(expr)

#define RVL_ASSERT1(cond, file, func, line) do { if(!(cond)) { print_assert_msg(RVL_STRINGIFY(cond), file, func, line); RVL_DEBUG_TRAP(); } } while(0)
#define rvl_assert(cond) RVL_ASSERT1(cond, __FILE__, __func__, __LINE__)


// NOTE: Semicolon at end makes two semicolons after removing asserts (assert(cond); <- this stays).
// This can orphan else in an unscoped if else statement.
// This may be preferable as I can track these cases down. Alternatively could remove else on after while(0).
// #define assert(cond) do {}while(0);

#define KILOBYTES(Value) ((Value) * 1024LL)
#define MEGABYTES(Value) (Kilobytes(Value) * 1024LL)

template <typename T, size_t n>
constexpr size_t array_count(T(&)[n])
{
    return n;
}

// ------------------------------------------------------------------------------------------------
// This is suspect in lieu of bugs in term on linux

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

struct File_Data
{
    // Should this contain size?
    // Should size include null terminator?
    u8 *data;
    u64 size;
};

File_Data
read_entire_file_and_null_terminate(const char *filename)
{
    // Returns null terminated char array of data, with size of file (not including null terminator)
    // If file is empty, result contains an array with a null character and size 0
    
    // NOTE:
    // In text mode fread translates Windows \r\n to unix/C stlye \n,
    // so doesn't fill allocated memory.
    // On POSIX systems there is no difference between binary and text mode
    
    // TODO:
    // Filename encoding for windows? Path length?
    
    // TODO:
    // Use ReadFile (windows) and read (posix) to read in 32bit
    // and make multiple read calls in 64 bit mode, to read >2Gb files and such
    
    File_Data result = {};
    
#if defined(_WIN32)
    HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file_handle, &size))
        {
            // Can't read more than a 4GB file
            if (size.QuadPart <= UINT32_MAX)
            {
                u32 size32 = (u32)size.QuadPart;
                result.data = (u8 *)malloc(size32 + 1);
                if (result.data)
                {
                    DWORD bytes_read;
                    if (ReadFile(file_handle, result.data, size32, &bytes_read, 0) &&
                        size32 == bytes_read)
                    {
                        CloseHandle(file_handle);
                        result.data[size32] = 0;
                        result.size = size32;
                        return result;
                    }
                }
            }
        }
        
        CloseHandle(file_handle);
    }
    
#else
    
    // TODO: REWORK
    // TODO: Most likely can't read large files (>2GB) on 32-bit systems,
    // and maybe not on 64-bit systems (if the 64bit versions of functions don't replace 32-bit
    // automatically.
    int fd = open(filename, O_RDONLY);
    if (fd != -1)
    {
        FILE *file = fdopen(fd, "r");
        if (file != NULL)
        {
            // Ensure that the file is a regular file
            struct stat st;
            if ((fstat(fd, &st) == 0) && (S_ISREG(st.st_mode)))
            {
                // signed integer I think
                off_t size = st.st_size;
                if (size >= 0)
                {
                    result.data = (char *)malloc(size + 1);
                    if (result.data)
                    {
                        size_t bytes_read = fread(result.data, 1, size, file);
                        if (bytes_read == size)
                        {
                            fclose(file);
                            result.data[size] = 0;
                            result.size = size;
                            return result;
                        }
                    }
                }
            }
        }
        
        fclose(file); // Will also close fd
    }
    
#endif
    
    free(result.data);
    result.data = nullptr;
    result.size = 0;
    
    return result;
}

File_Data
read_entire_file(const char *filename)
{
    // TODO: Can't destinguish 0 size file from read error
    // Returns char array of data, with size of file
    // If file is empty, result contains null pointer and size 0
    
#if !defined(_WIN32)
#error read_entire_file not implementedon linux
#endif
    
    File_Data result = {};
    
    HANDLE file_handle = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ,
                                     0, OPEN_EXISTING, 0, 0);
    if (file_handle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size;
        if (GetFileSizeEx(file_handle, &size))
        {
            // Can't read more than a 4GB file
            if (size.QuadPart <= UINT32_MAX && size.QuadPart > 0)
            {
                u32 size32 = (u32)size.QuadPart;
                result.data = (u8 *)malloc(size32);
                if (result.data)
                {
                    DWORD bytes_read;
                    if (ReadFile(file_handle, result.data, size32, &bytes_read, 0) &&
                        size32 == bytes_read)
                    {
                        CloseHandle(file_handle);
                        result.size = size32;
                        return result;
                    }
                }
            }
        }
        
        CloseHandle(file_handle);
    }
    
    free(result.data);
    result.data = nullptr;
    result.size = 0;
    
    return result;
}

/* -----------------------------------------------------------------------------------------
  Debug printing code
  (warning: disgusting c++ template code)

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

struct rvl_false_type { static constexpr bool value = false; };
struct rvl_true_type  { static constexpr bool value = true; };

template<typename T, typename U> struct same_type : rvl_false_type {};
template<typename T> struct same_type<T,T> : rvl_true_type {}; //specialization

template< class T > struct remove_const          { typedef T type; };
template< class T > struct remove_const<const T> { typedef T type; };
template< class T > struct remove_volatile             { typedef T type; };
template< class T > struct remove_volatile<volatile T> { typedef T type; };

template< class T >
struct remove_cv {
    typedef typename remove_volatile<typename remove_const<T>::type>::type type;
};

template< class T > struct is_pointer_helper     : rvl_false_type {};
template< class T > struct is_pointer_helper<T*> : rvl_true_type {};
template< class T > struct is_pointer : is_pointer_helper<typename remove_cv<T>::type> {};

#define RVL_VALID_TYPE(T) (same_type<T, const char *>::value   || same_type<T, char *>::value || \
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
    static_assert(RVL_VALID_TYPE(T),
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
    static_assert(RVL_VALID_TYPE(T) || same_type<T, bool>::value,
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
    if (strlen(str) > 3900) return;    // Need extra room for specifiers
    
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

enum RVL_Colour
{
    Red,
    Blue,
    Green,
    Yellow,
    Purple,
};

template<typename... Targs>
void tprint_col(RVL_Colour colour, const char *str, Targs... args)
{
    switch (colour)
    {
        case RVL_Colour::Red: printf("\033[31m"); break;
        case RVL_Colour::Blue: printf("\033[34m"); break;
        case RVL_Colour::Green: printf("\033[32m"); break;
        case RVL_Colour::Yellow: printf("\033[33m"); break;
        case RVL_Colour::Purple: printf("\033[35m"); break;
    }
    tprint(str, args...);
    printf("\033[37m");
}

void tprint_col(RVL_Colour colour, const char *str)
{
    switch (colour)
    {
        case RVL_Colour::Red: printf("\033[31m"); break;
        case RVL_Colour::Blue: printf("\033[34m"); break;
        case RVL_Colour::Green: printf("\033[32m"); break;
        case RVL_Colour::Yellow: printf("\033[33m"); break;
        case RVL_Colour::Purple: printf("\033[35m"); break;
    }
    tprint(str);
    printf("\033[37m");
}


template<typename... Targs>
void rvl_snprintf(char *buf, size_t n, const char *str, Targs... args)
{
    char *specifiers[1+sizeof...(args)] = { get_spec(args)... };
    
    // Doesn't handle booleans
    // May not gracefully handle arument number and '%' number mismatch
    
    if (n >= 3900) return;
    
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

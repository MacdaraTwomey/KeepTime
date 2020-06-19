

#include <stdlib.h>
#include <string.h>

#include "graphics.h"

void *xalloc(size_t size)
{
    Assert(size > 0);
    Assert(size < Gigabytes(1)); // Just a guess of an upper bound on size
    void *p = calloc(1, size);
    Assert(p);
    return p;
}


void
concat_strings(char *dest, size_t dest_len,
               const char *str1, size_t len1,
               const char *str2, size_t len2)
{
    Assert(dest && str1 && str2);
    if ((len1 + len2 + 1) > dest_len) return;
    
    memcpy(dest, str1, len1);
    memcpy(dest + len1, str2, len2);
    dest[len1 + len2] = '\0';
}




static char *
get_filename_from_path(const char *filepath)
{
    // Returns pointer to filename part of filepath
    char *char_after_last_slash = (char *)filepath;
    for (char *at = char_after_last_slash; at[0]; ++at)
    {
        if (*at == '\\')
        {
            char_after_last_slash = at + 1;
        }
    }
    
    return char_after_last_slash;
}


// TODO: Use in old win32 code
char *
make_filepath_from_fullpath(char *exe_path, const char *filename)
{
    char *buf = (char *)xalloc(PLATFORM_MAX_PATH_LEN);
    if (!buf)
    {
        return nullptr;
    }
    
    char *exe_name = get_filename_from_path(exe_path);
    ptrdiff_t dir_len = exe_name - exe_path;
    if (dir_len + strlen(filename) + 1 > PLATFORM_MAX_PATH_LEN)
    {
        free(buf);
        return nullptr;
    }
    
    concat_strings(buf, PLATFORM_MAX_PATH_LEN,
                   exe_path, dir_len,
                   filename, strlen(filename));
    
    realloc(buf, dir_len + strlen(filename) + 1);
    if (!buf)
    {
        return nullptr;
    }
    
    return buf;
}


char *
make_filepath_with_dir(char *dir, const char *filename)
{
    // UTF8 when using SDL
    size_t len = strlen(dir) + strlen(filename) + 1;
    char *buf = (char *)xalloc(len);
    if (!buf)
    {
        return nullptr;
    }
    
    memcpy(buf, dir, strlen(dir));
    memcpy(buf + strlen(dir), filename, strlen(filename) + 1);
    
    return buf;
}

inline V2i
operator-(V2i a)
{
    V2i result;
    result.x = -a.x;
    result.y = -a.y;
    return result;
}

inline V2i
operator+(V2i a, V2i b)
{
    V2i result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline V2i &
operator+=(V2i &a, V2i b)
{
    a = a + b;
    return a;
}

inline V2i
operator-(V2i a, V2i b)
{
    V2i result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return(result);
}

inline V2i
operator*(int a, V2i b)
{
    V2i result;
    result.x = a * b.x;
    result.y = a * b.y;
    return result;
}

inline V2i
operator*(V2i a, int b)
{
    V2i result = b * a;
    return result;
}

inline V2i &
operator*=(V2i &b, int a)
{
    b = a * b;
    return b;
}


inline Rect
shrink(Rect rect, int amount)
{
    Rect result;
    result.pos.x = rect.pos.x + amount;
    result.pos.y = rect.pos.y + amount;
    result.dim.x = rect.dim.x - amount*2;
    result.dim.y = rect.dim.y - amount*2;
    return result;
}

inline Rect
grow(Rect rect, int amount)
{
    return shrink(rect, -amount);
}

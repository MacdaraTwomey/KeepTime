

#include <stdlib.h>
#include <string.h>


void *xalloc(size_t size)
{
    rvl_assert(size > 0);
    rvl_assert(size < Gigabytes(1)); // Just a guess of an upper bound on size
    void *p = calloc(1, size);
    rvl_assert(p);
    return p;
}


// len is string length including null terminator
char *clone_string(char *str, size_t len)
{
    rvl_assert(len > 0);
    if (len == 0) return nullptr;
    
    char *clone = (char *)xalloc(len);
    if (clone)
    {
        strcpy(clone, str);
    }
    return clone;
}

char *
clone_string(char *string)
{
    rvl_assert(string);
    size_t len = strlen(string);
    rvl_assert(len > 0);
    
    if (len == 0) return nullptr;
    
    return clone_string(string, len+1);
}

void
concat_strings(char *dest, size_t dest_len,
               const char *str1, size_t len1,
               const char *str2, size_t len2)
{
    rvl_assert(dest && str1 && str2);
    if ((len1 + len2 + 1) > dest_len) return;
    
    memcpy(dest, str1, len1);
    memcpy(dest + len1, str2, len2);
    dest[len1 + len2] = '\0';
}


#define rvl_lerp(x, y, t) (((1.0f-(t))*(x)) + ((t)*(y)))

struct V2i
{
    int x, y;
};
struct Rect2i
{
    V2i min, max;
};

inline V2i
operator-(V2i a)
{
    V2i result;
    
    result.x = -a.x;
    result.y = -a.y;
    
    return(result);
}

inline V2i
operator+(V2i a, V2i b)
{
    V2i result;
    
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    
    return(result);
}

inline V2i &
operator+=(V2i &a, V2i b)
{
    a = a + b;
    
    return(a);
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
operator*(r32 A, V2i B)
{
    V2i Result;
    
    Result.x = A*B.x;
    Result.y = A*B.y;
    
    return(Result);
}

inline V2i
operator*(V2i B, r32 A)
{
    V2i Result = A*B;
    
    return(Result);
}

inline V2i &
operator*=(V2i &B, r32 A)
{
    B = A * B;
    
    return(B);
}

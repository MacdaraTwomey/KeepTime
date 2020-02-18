

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

char *
copy_string(char *string)
{
    rvl_assert(string);
    char *s = (char *)xalloc(strlen(string)+1);
    if (s)
    {
        memcpy(s, string, strlen(string)+1);
    }
    
    return s;
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

// len is string length including null terminator
char *clone_string(char *str, size_t len)
{
    char *clone = (char *)xalloc(len);
    if (clone)
    {
        strcpy(clone, str);
    }
    return clone;
}

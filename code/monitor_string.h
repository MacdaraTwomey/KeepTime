#pragma once

// NOTE: Most of this inspired/created by Allen Webster in the 4coder_string.h string library.
// https://4coder.handmade.network/

// TODO: Pre/post condition tests for all calls in this library


#include <string.h> // just for strlen, could just implement

// probably don't even need a capacity

struct String
{
    char *str;
    i32 length;
    i32 capacity;
};

typedef String CString;

template<size_t N>
constexpr String make_const_string(const char (&a)[N])
{
    return String{const_cast<char *>(a), N-1, N-1};
}

// should I say capacity == sizeof(lit)? we can't write to it but can make use of knowledge of null terminator
// Encode null terminated with type system?
// Must be a actual string literal not a char *array_of_str[] reference
#define make_string_from_literal(lit) make_string((lit), sizeof(lit)-1)
#define make_fixed_size_string(buf) make_string_size_cap((buf), 0, sizeof((buf)))

String
make_string_size_cap(void *str, i32 size, i32 capacity)
{
    String s;
    s.str      = (char *)str;
    s.length   = size;
    s.capacity = capacity;
    return s;
}

// Same as make_fixed_sized_string
template<size_t N>
String
make_empty_string(const char (&a)[N])
{
    String s;
    s.str      = (char *)a;
    s.length   = 0;
    s.capacity = N;
    return s;
}

String
make_string(void *str, i32 size)
{
    String s;
    s.str      = (char *)str;
    s.length   = size;
    s.capacity = size;
    return s;
}

String
substr(String parent, i32 offset)
{
    Assert(offset < parent.length);
    String s;
    s.str      = parent.str + offset;
    s.length   = parent.length - offset;
    s.capacity = 0;
    return s;
}


String
substr_len(String parent, i32 offset, i32 len)
{
    Assert(offset < parent.length);
    String s;
    s.str      = parent.str + offset;
    s.length   = (offset + len <= parent.length) ? len : parent.length - offset;
    s.capacity = 0;
    return s;
}



String
substr_range(String parent, i32 start, i32 end)
{
    // [start, end] inclusive, inclusive
    
    // Clipped to parent string's end
    Assert(start < parent.length && start <= end);
    i32 len = end - start + 1;
    return substr_len(parent, start, len);
}

i32
search_for_substr(String str, i32 offset, String substr)
{
    if (substr.length == 0) return -1;
    i32 match = -1;
    
    i32 last_possible_char = str.length - substr.length;
    for (i32 i = offset; i <= last_possible_char; ++i)
    {
        if (str.str[i] == substr.str[0])
        {
            match = i;
            for (i32 j = 1; j < substr.length; ++j)
            {
                if (str.str[i + j] != substr.str[j])
                {
                    match = -1;
                    break;
                }
            }
            
            if (match != -1) break;
        }
    }
    
    return match;
}

// Could template on size of substr, to allow length of string literals to be deduced.
// Or maybe compilers just optimise this anyway (I think so).
i32
search_for_substr(String str, i32 offset, char *substr)
{
    size_t substr_length = strlen(substr);
    if (substr_length == 0) return -1;
    i32 match = -1;
    
    i32 last_possible_char = str.length - substr_length;
    for (i32 i = offset; i <= last_possible_char; ++i)
    {
        if (str.str[i] == substr[0])
        {
            match = i;
            for (i32 j = 1; j < substr_length; ++j)
            {
                if (str.str[i + j] != substr[j])
                {
                    match = -1;
                    break;
                }
            }
            
            if (match != -1) break;
        }
    }
    
    return match;
}


// Maybe shoudl return new string because might need to keep track of previous position,
// e.g. to free memory, to make other strings with spaces.
void
skip_whitespace(String *str)
{
    if (str->length > 0)
    {
        i32 i = 0;
        while (i < str->length && str->str[i] == ' ')
        {
            i += 1;
        }
        
        str->str += i;
        str->length -= i;
        str->capacity -= i;
    }
}

bool
string_equals(String a, char *b)
{
    size_t len = strlen(b);
    if (a.length != len) return false;
    
    for (i32 i = 0; i < len; ++i)
    {
        if (a.str[i] != b[i]) return false;
    }
    
    return true;
}


bool
string_equals(String a, String b)
{
    if (a.length != b.length) return false;
    
    for (i32 i = 0; i < a.length; ++i)
    {
        if (a.str[i] != b.str[i]) return false;
    }
    
    return true;
}



i32
reverse_search_for_char(String str, i32 offset, char c)
{
    if (offset < str.length)
    {
        for (i32 i = offset; i >= 0; --i)
        {
            if (str.str[i] == c)
            {
                return i;
            }
        }
    }
    
    return -1;
}


i32
search_for_char(String str, i32 offset, char c)
{
    for (i32 i = offset; i < str.length; ++i)
    {
        if (str.str[i] == c)
        {
            return i;
        }
    }
    
    return -1;
}

bool
is_upper(char c)
{
    return (c >= 'A' && c <= 'Z');
}

bool
is_lower(char c)
{
    return (c >= 'a' && c <= 'z');
}

char
to_upper(char c)
{
    return (is_lower(c)) ? c - 32 : c;
}

char
to_lower(char c)
{
    return (is_upper(c)) ? c + 32 : c;
}

bool
prefix_match_case_insensitive(String str, String prefix)
{
    if (str.length < prefix.length) return false;
    for (i32 i = 0; i < prefix.length; ++i)
    {
        if (to_upper(str.str[i]) != to_upper(prefix.str[i])) return false;
    }
    
    return true;
}


bool
prefix_match_case_insensitive(String str, char *prefix)
{
    size_t prefix_length = strlen(prefix);
    if (str.length < prefix_length) return false;
    for (i32 i = 0; i < prefix_length; ++i)
    {
        if (to_upper(str.str[i]) != to_upper(prefix[i])) return false;
    }
    
    return true;
}

void
string_to_lower(String *s)
{
    for (i32 i = 0; i < s->length; ++i)
    {
        s->str[i] = to_lower(s->str[i]);
    }
}

// String to String copy only copies if there is enough room
// char * to String copy does a partial copy if there is not enough room and returns false.
// The same with appends.

bool
copy_string(String *dest, String source)
{
    // If dest capacity is too small the string is not truncated.
    if (dest->capacity < source.length) return false;
    
    for (i32 i = 0; i < source.length; ++i)
    {
        dest->str[i] = source.str[i];
    }
    
    dest->length = source.length;
    return true;
}


bool
copy_string(String *dest, char *source)
{
    // If dest capacity is too small the string is truncated to dest capacity, and false returned.
    i32 i = 0;
    for (; source[i]; ++i)
    {
        if (i >= dest->capacity) return false;
        dest->str[i] = source[i];
    }
    
    dest->length = i;
    return true;
}

String
tailstr(String str)
{
    String result;
    result.str = str.str + str.length;
    result.capacity = str.capacity - str.length;
    result.length = 0;
    return result;
}

bool
append_string(String *dest, String source)
{
    String end = tailstr(*dest);
    bool result = copy_string(&end, source);
    // NOTE: This depends on end.size still being 0 if the check failed and no copy occurred.
    dest->length += end.length;
    return result;
}

bool
append_string(String *dest, char *source)
{
    String end = tailstr(*dest);
    bool result = copy_string(&end, source);
    dest->length += end.length;
    return result;
}

bool
null_terminate(String *str)
{
    bool result = false;
    if (str->length < str->capacity)
    {
        str->str[str->length] = '\0';
        result = true;
    }
    return result;
}

i32
last_slash_pos(String path)
{
    for (i32 i = path.length-1; i >= 0; --i)
    {
        if (path.str[i] == '\\' || path.str[i] == '/')
        {
            return i;
        }
    }
    
    return -1;
}


String
get_filename_from_path(String path)
{
    // Not sure that i love what we do when the path ends with a slash
    String result;
    i32 slash_pos = last_slash_pos(path);
    if (slash_pos == -1)            result = substr(path, 0);
    if (slash_pos == path.length-1) result = {nullptr, 0, 0};
    else                            result = substr(path, slash_pos+1);
    
    return result;
}

void
remove_extension(String *file)
{
    // Do reverse find because files can have multiple dots, and only last is the real extension I think.
    i32 dot_pos = file->length;
    for (i32 i = file->length-1; i >= 0; --i)
    {
        if (file->str[i] == '.')
        {
            dot_pos = i;
            break;
        }
    }
    // If file is only ".exe" with nothing before extension size == 0
    // If no dots length stays the same
    // If there is a dot, the string is clipped to before the dot
    file->length = dot_pos;
}

bool
string_is_null_terminated(String s)
{
    bool is_null_terminated = false;
    if (s.capacity > s.length)
    {
        is_null_terminated = (s.str[s.length] == '\0');
    }
    
    return is_null_terminated;
}
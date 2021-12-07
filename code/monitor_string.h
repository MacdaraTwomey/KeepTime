#pragma once

#include "base.h"

//
// NOTE:
// In a string the null terminator doesn't count towards the length unless you use string_builder
// and call NullTerminate(), or otherwise make one with it included.
//
// TODO: 
// Correct handling of UTF8 strings
// e.g. ToUpper


struct string
{
    char *Str;
    u64 Length;
    
    const char &operator[](size_t Index) const
    {
        Assert(Index >= 0 && Index < Length);
        return Str[Index];
    }
    char &operator[](size_t Index)
    {
        Assert(Index >= 0 && Index < Length);
        return Str[Index];
    }
};

// It would be nice to be able to pass a c_string to a function expecting a string but NOT
// be able to pass a string to a function expecting a c_string.

//typedef string c_string; /// c_string means must be null terminated. Good idea?
struct c_string : public string
{
    static constexpr bool IS_C_STRING = true;
    // use using to make alias to string.Str etc
};

#define StringLiteral(lit) string{(lit), sizeof(lit) - 1} // Null terminator not included in Length

char StringGetChar(string String, u64 Index)
{
    char Result = 0;
    if (Index < String.Length)
    {
        Result = String.Str[Index];
    }
    
    return Result;
}

// This doesn't count the null terminator as part of the length
u64 StringLength(char *CString)
{
    u64 Length = 0; 
    while (*CString) 
    {
        ++CString;
        Length += 1;
    }
    
    return Length;
}

string MakeString(char *CString)
{
    string Result;
    Result.Str = CString;
    Result.Length = StringLength(CString);
    return Result;
}

bool StringEquals(string a, string b)
{
    bool Result = (a.Length == b.Length);
    if (Result)
    {
        for (u32 i = 0; i < a.Length; ++i)
        {
            if (a.Str[i] != b.Str[i]) 
            {
                Result = false;
                break;
            }
        }
    }
    
    return Result;
}

bool IsUpper(char c)
{
    return (c >= 'A' && c <= 'Z');
}

bool IsLower(char c)
{
    return (c >= 'a' && c <= 'z');
}

bool IsAlpha()
{
    return IsLower(c) || IsUpper(c);
}

bool IsNumeric(char c)
{
    return (c >= '0' && c <= '9');
}

bool IsAlphaNumeric(char c)
{
    return IsAlpha(c) || IsNumeric(c);
}

char ToUpper(char c)
{
    return (IsUpper(c)) ? c - 32 : c;
}

char ToLower(char c)
{
    return (ToLower(c)) ? c + 32 : c;
}

string StringPrefix(string String, u64 n)
{
    String.Length = ClampTop(n, String->Length);
    return String;
}

void StringSkip(string *String, u64 n)
{
    n = ClampTop(n, String->Length);
    String->Str = String->Str + n;
    String->Length -= n;
}

void StringChop(string *String, u64 n)
{
    n = ClampTop(n, String->Length);
    String->Length -= n;
}

u64 StringFindChar(string String, char Char)
{
    u64 Position = String.Length;
    for (u64 i = 0; i < String.Length; ++i)
    {
        if (String.Str[i] == Char)
        {
            Position = i;
            break;
        }
    }
    
    return Position;
}

u64 StringFindCharReverse(string String, char Char)
{
    u64 Position = String.Length;
    for (u64 i = String.Length - 1; i != 0; --i)
    {
        if (String.Str[i] == Char)
        {
            Position = i;
            break;
        }
    }
    
    return Position;
}

s32 StringFindLastSlash(string Path)
{
    u64 Result = Path.Length;
    for (u64 i = Path.Length - 1; i >= 0; --i)
    {
        if (Path.Str[i] == '\\' || Path.Str[i] == '/')
        {
            Result = i;
            break;
        }
    }
    
    return Result;
}

void StringRemoveExtension(string *File)
{
    // Files can have multiple dots, and only last is the real extension.
    
    // If no dot is found Length stays the same
    u64 DotPosition = StringReverseSearch(*File, '.');
    File->Length = DotPosition;
}

// Returns length 0 string if Path has no filename
string StringFilenameFromPath(string Path)
{
    u64 LastSlash = StringFindLastSlash(Path);
    StringSuffix(&Path, LastSlash);
    return Path;
}


// This requires that arena allocations are contiguous and no one else can use the arena
// Not sure if this is a good idea, but we almost never actually need to build strings
struct string_builder
{
    char *Memory;
    u64 Length;
    u64 Capacity;
    
    void Append(string NewString)
    {
        u64 Remaining = Capacity - Length;
        if (NewString.Length <= Remaining)
        {
            char *p = Memory + Length;
            for (u64 i = 0; i < NewString.Length; ++i)
            {
                *p++ = NewString[i];
            }
            
            Length += NewString.Length;
        }
        else
        {
            Assert(0);
        }
    }
    
    // Null terminator counts as length in built string
    void NullTerminate()
    {
        Assert(Capacity > Length);
        
        if (Memory[Length] != '\0')
        {
            Memory[Length] = '\0';
            Length += 1;
        }
    }
    
    string GetString()
    {
        return string{Memory, Length};
    }
};

string_builder StringBuilder(arena *Arena, u64 Capacity)
{
    string_builder Result;
    Result.Memory = Allocate(Arena, Capacity, char);
    Result.Length = 0;
    Result.Capacity = Capacity;
    return Result;
}

#if 0
// should I say capacity == sizeof(lit)? we can't write to it but can make use of knowledge of null terminator
// Encode null terminated with type system?
// Must be a actual string literal not a char *array_of_str[] reference

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
#endif

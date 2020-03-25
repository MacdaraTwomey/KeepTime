
#include <string.h> // just for strlen, could just implement

// Const string class
struct String
{
    char *str;
    i32 length;
    i32 capacity;
};

struct Const_String
{
    char *str;
    i32 length;
    i32 capacity;
    
    // Isn't POD
    template<std::size_t N>
        constexpr Const_String(const char (&a)[N]) :
    str(const_cast<char *>(a)), length(N-1), capacity(N-1) {  }
};

constexpr Const_String arr[] = {
    "hi",
    "BE"
};

constexpr Const_String cs = "icon";
static_assert(arr[0].length == 2, "not right length");

template<std::size_t N>
constexpr String make_const_string(const char (&a)[N])
{
    return String{const_cast<char *>(a), N-1, N-1};
}

// should I say capacity == sizeof(lit)? we can't write to it but can make use of knowledge of null terminator
// Encode null terminated with type system?
// Must be a actual string literal not a char *array_of_str[] reference
#define make_string_from_literal(lit) make_string((lit), sizeof(lit)-1)

String
make_string_from_buf(void *str, i32 size, i32 capacity)
{
    String s;
    s.str      = (char *)str;
    s.length   = size;
    s.capacity = capacity;
    return s;
}

template<size_t N>
String
make_empty_string(const char (&a)[N])
{
    String s;
    s.str      = (char *)str;
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
    // Clipped to parent string's end
    Assert(start < parent.length && start <= end);
    i32 len = end - start + 1;
    return substr_len(parent, start, len);
}

bool
copy_string(String *a, String b)
{
    // Capacity of dest string remains the same.
    if (a->capacity < b.length) return false;
    for (i32 i = 0; i < b.length; ++i)
    {
        a->str[i] = b.str[i];
    }
    
    a->length = b.length;
    return true;
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
    if (offset < str.length)
    {
        for (i32 i = offset; i >= 0; ++i)
        {
            if (str.str[i] == c)
            {
                return i;
            }
        }
    }
    
    return -1;
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
null_terminate(String *str)
{
    if (str->length < str->capacity)
    {
        str->str[str->length] = '\0';
        return true;
    }
    
    return false;
}

// append

// bool null_terminate_string(String *s)

// Arena
// temp_memory or temp_arena
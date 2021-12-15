#pragma once

#include "base.h"

//
// NOTE:
// In a string the null terminator doesn't count towards the length unless you use string_builder
// and call NullTerminate(), or otherwise make one with it included.
//
// TODO: 
// Correct handling of UTF8 strings (e.g. ToUpper)

struct string
{
    char *Str;
    u64 Length;
    
    string() = default;
    string(char *StrData, u64 StrLength) : Str(StrData), Length(StrLength) {}
    
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

struct c_string : public string
{
    c_string() = default;
    c_string(char *CStringData, u64 CStringLength) : string(CStringData, CStringLength) {}
};

static_assert(sizeof(string) == sizeof(c_string), "");
static_assert(sizeof(string) == sizeof(char *) + sizeof(u64), "");

#include <type_traits>
static_assert(std::is_trivial<string>::value, "string should be trivial");
static_assert(std::is_pod<string>::value, "string should be POD");

static_assert(std::is_trivial<c_string>::value, "c_string should be trivial");
static_assert(std::is_pod<c_string>::value, "c_string should be POD");


#define StringLit(lit) string{(lit), sizeof(lit) - 1} // Null terminator not included in Length

c_string CStringFromString(string String)
{
    if (String.Length > 0)
    {
        // NOTE: NULL byte counted as part of length
        Assert(String.Str[String.Length-1] == 0); // Last character is NULL byte
    } 
    
    c_string Result {String.Str, String.Length};
    return Result;
}

struct fake_settings
{
    string Keywords[2];
};

struct fake_settings2
{
    c_string Keywords[2];
};

void StrToStr(fake_settings *Settings, string Keyword)
{
    Settings->Keywords[0] = Keyword;
}

void CStrToCStr(fake_settings2 *Settings, c_string Keyword)
{
    Settings->Keywords[0] = Keyword;
    char *Str = Keyword.Str;
}

void CStrToStr(fake_settings *Settings, c_string Keyword)
{
    Settings->Keywords[0] = Keyword;
    char *Str = Keyword.Str;
}

void StrToCStr(fake_settings2 *Settings, string Keyword)
{
    //Settings->Keywords[0] = Keyword; // Doesn't like
    char *Str = Keyword.Str;
}

void TestStrings()
{
    fake_settings Settings = {};
    fake_settings2 Settings2 = {};
    
    string String = {};
    c_string CString = {};
    StrToStr(&Settings, String);
    CStrToCStr(&Settings2, CString);
    CStrToStr(&Settings, CString);
    StrToCStr(&Settings2, String); // Doesn't like
    
    StrToStr(&Settings, CString); // Fine
    //CStrToCStr(&Settings2, String); // Doesn't like
    
    c_string BadCString = CStringFromString(string{"sdd", 4});
    c_string BadCString2 = CStringFromString(c_string{"sdd", 4});
    
    string StringTest{};
    string StringTest2 = string();
    string StringTest3("sdfsdf", 3);
    string StringTest4 = {};
    string StringTest5 = string{};
    string StringTest6 = string{"ddfg", 5};
    
    c_string CStringTest{};
    c_string CStringTest2 = c_string();
    c_string CStringTest3("sdfsdf", 3);
    c_string CStringTest4 = {};
    c_string CStringTest5 = c_string{};
    c_string CStringTest6 = c_string{"ddfg", 5};
    
    string ToStringTest{CStringTest};
    string ToStringTest2 = CStringTest;
    string ToStringTest3(CStringTest);
    string ToStringTest4 = {CStringTest};
    string ToStringTest5 = c_string{CStringTest};
    string ToStringTest6 = c_string{"ddfg", 5};
    
    // Not allowed
    //c_string ToCStringTest{StringTest};
    //c_string ToCStringTest2 = c_string(StringTest);
    //c_string ToCStringTest3(StringTest);
    //c_string ToCStringTest4 = {StringTest};
    //c_string ToCStringTest5 = c_string{StringTest};
    //c_string ToCStringTest6 = c_string{StringTest};
}


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

string MakeString(char *StringData, u64 Length)
{
    string Result;
    Result.Str = StringData;
    Result.Length = Length;
    return Result;
}

bool IsUpper(char c)
{
    return (('A' <= c) && (c <= 'Z'));
}

bool IsLower(char c)
{
    return (('a' <= c) && (c <= 'z'));
}

bool IsAlpha(char c)
{
    return IsLower(c) || IsUpper(c);
}

bool IsNumeric(char c)
{
    return (('0' <= c) && (c <= '9'));
}

bool IsAlphaNumeric(char c)
{
    return IsAlpha(c) || IsNumeric(c);
}

char ToUpper(char c)
{
    return IsLower(c) ? c - 32 : c;
}

char ToLower(char c)
{
    return IsUpper(c) ? c + 32 : c;
}

void StringToLower(string *String)
{
    for (u64 i = 0; i < String->Length; ++i)
    {
        String->Str[i] = ToLower(String->Str[i]);
    }
}

void StringToUpper(string *String)
{
    for (u64 i = 0; i < String->Length; ++i)
    {
        String->Str[i] = ToUpper(String->Str[i]);
    }
}

string StringPrefix(string String, u64 n)
{
    String.Length = ClampTop(n, String.Length);
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
    for (u64 i = String.Length - 1; i != static_cast<u64>(-1); --i)
    {
        if (String.Str[i] == Char)
        {
            Position = i;
            break;
        }
    }
    
    return Position;
}

bool StringContainsChar(string String, char Char)
{
    return StringFindChar(String, Char) < String.Length;
}

bool StringsAreEqual(string A, string B)
{
    bool Result = (A.Length == B.Length);
    if (Result)
    {
        for (u64 i = 0; i < A.Length; ++i)
        {
            if (A.Str[i] != B.Str[i]) 
            {
                Result = false;
                break;
            }
        }
    }
    
    return Result;
}

bool StringsAreEqual(string A, char *B, u64 BLength)
{
    bool Result = (A.Length == BLength);
    if (Result)
    {
        for (u64 i = 0; i < A.Length; ++i)
        {
            if (A.Str[i] != B[i]) 
            {
                Result = false;
                break;
            }
        }
    }
    
    return Result;
}

bool StringsAreEqualCaseInsensitive(string a, string b)
{
    bool Result = (a.Length == b.Length);
    if (Result)
    {
        for (u64 i = 0; i < a.Length; ++i)
        {
            if (ToLower(a.Str[i]) != ToLower(b.Str[i])) 
            {
                Result = false;
                break;
            }
        }
    }
    
    return Result;
}

bool StringContainsSubstr(string String, string Substr)
{
    bool ContainsSubstr = false;
    if (Substr.Length > 0) 
    {
        u64 LastPossibleCharIndex = String.Length - Substr.Length;
        for (u64 i = 0; i <= LastPossibleCharIndex; ++i)
        {
            if (String.Str[i] == Substr.Str[0])
            {
                ContainsSubstr = true;
                for (u64 j = 1; j < Substr.Length; ++j)
                {
                    if (String.Str[i + j] != Substr.Str[j])
                    {
                        ContainsSubstr = false;
                        break;
                    }
                }
                
                if (ContainsSubstr) break;
            }
        }
    }
    
    return ContainsSubstr;
}


bool CharIsSlash(char c)
{
    return (c == '\\' || c == '/');
}

u64 StringFindLastSlash(string Path)
{
    u64 Position = Path.Length;
    for (u64 i = Path.Length - 1; i != static_cast<u64>(-1); --i)
    {
        if (CharIsSlash(Path.Str[i]))
        {
            Position = i;
            break;
        }
    }
    
    return Position;
}

void StringRemoveExtension(string *File)
{
    // Files can have multiple dots, and only last is the real extension.
    
    // If no dot is found Length stays the same
    u64 DotPosition = StringFindCharReverse(*File, '.');
    File->Length = DotPosition;
}

// Returns length 0 string if Path has no filename
string StringFilenameFromPath(string Path)
{
    u64 LastSlash = StringFindLastSlash(Path);
    StringSkip(&Path, LastSlash + 1);
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
            MemoryCopy(NewString.Length, NewString.Str, Memory + Length);
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
        Memory[Length] = '\0';
        Length += 1;
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


#include "monitor.h"
#include "stdlib.h"

#include <stdarg.h>
#include <stdio.h>


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

// TODO: BETTER HASH FUNCTION
// TODO: This only returns 32-bit value in windows
unsigned long djb2(unsigned char *str)
{
    // djb2
    // this algorithm (k=33) was first reported by dan bernstein many years ago in comp.lang.c. another version of this algorithm
    // (now favored by bernstein) uses xor: hash(i) = hash(i - 1) * 33 ^ str[i]; the magic of number 33
    // (why it works better than many other constants, prime or not) has never been adequately explained.
    unsigned long hash = 5381;
    int c;
    
    while (c = *str++)
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    
    return hash;
}

bool init_hash_table(Hash_Table *table, s64 table_size)
{
    rvl_assert(table_size > 0);
    table->buckets = (Hash_Node *)xalloc(table_size * sizeof(Hash_Node));
    table->occupancy = (u8 *)xalloc(table_size * sizeof(u8));
    table->size = table_size;
    table->count = 0;
    
    return (table->buckets && table->occupancy);
}


void free_table(Hash_Table *table)
{
    rvl_assert(table);
    for (s64 i = 0; i < table->size; ++i)
    {
        if (table->occupancy[i] == Hash_Table::OCCUPIED)
        {
            free(table->buckets[i].key);
        }
    }
    free(table->buckets);
    free(table->occupancy);
    *table = {};
}

// Returns -1 if already added or index of added node
s64 Hash_Table::add_item(char *key, u32 value)
{
    rvl_assert(key);
    rvl_assert(key[0]);
    if (count >= size*0.75)
    {
        grow_table();
    }
    
    s64 i = (s64) djb2((unsigned char *)key) % size;
    
    for (; i < size && occupancy[i % size] == OCCUPIED; ++i)
    {
        s64 index = i % size;
        if (strcmp(key, buckets[index].key) == 0)
        {
            return -1;
        }
    }
    
    s64 index = i % size;
    buckets[index].key = copy_string(key);
    buckets[index].value = value;
    occupancy[index] = OCCUPIED;
    count += 1;
    
    return i;
}

bool Hash_Table::search(char *key, u32 *found_value)
{
    rvl_assert(key);
    rvl_assert(key[0]);
    rvl_assert(found_value);
    
    if (count == 0) return false;
    s64 i = (s64) djb2((unsigned char *)key) % size;
    
    for (; i < size && occupancy[i % size] != EMPTY; ++i)
    {
        s64 index = i % size;
        if (occupancy[i] == OCCUPIED)
        {
            if (strcmp(key, buckets[i].key) == 0)
            {
                *found_value = buckets[i].value;
                return true;
            }
        }
    }
    
    return false; 
}

void Hash_Table::remove(char *key)
{
    rvl_assert(key);
    rvl_assert(key[0]);
    
    if (count == 0) return;
    s64 i = (s64) djb2((unsigned char *)key) % size;
    
    for (; i < size && occupancy[i % size] != EMPTY; ++i)
    {
        s64 index = i % size;
        if (occupancy[index] == OCCUPIED)
        {
            if (strcmp(key, buckets[index].key) == 0)
            {
                free(buckets[index].key);
                occupancy[index] = DELETED;
                count -= 1;
                return;
            }
        }
    }
}

void Hash_Table::grow_table()
{
    s64 new_size = size*2;
    Hash_Node *new_buckets = (Hash_Node *)xalloc(new_size * sizeof(Hash_Node));
    u8 *new_occupancy = (u8 *)xalloc(new_size * sizeof(u8));
    rvl_assert(new_buckets);
    rvl_assert(new_occupancy);
    
    for (s64 i = 0; i < size; ++i)
    {
        if (occupancy[i] == OCCUPIED)
        {
            // Rehash key
            s64 index = (s64) djb2((unsigned char *)buckets[i].key) % new_size;
            
            while (new_occupancy[index] == OCCUPIED)
            {
                index = (index+1) % new_size;
            }
            
            new_buckets[index] = buckets[i];
            new_occupancy[index] = OCCUPIED;
        }
    }
    
    free(buckets);
    free(occupancy);
    buckets = new_buckets;
    occupancy = new_occupancy;
    size = new_size;
    // count stays same
}

char *Hash_Table::search_by_value(u32 value)
{
    for (s64 i = 0; i < size; ++i)
    {
        if (occupancy[i] == OCCUPIED && buckets[i].value == value)
        {
            return buckets[i].key;
        }
    }
    
    return nullptr;
}


String_Builder create_string_builder()
{
    String_Builder sb = {};
    sb.capacity = 30;
    sb.str = (char *)xalloc(sb.capacity);
    rvl_assert(sb.str);
    sb.len = 0;
    return sb;
}


void free_string_builder(String_Builder *sb)
{
    if (sb)
    {
        free(sb->str);
        *sb = {};
    }
}


void String_Builder::grow(size_t min_amount)
{
    size_t new_capacity  = std::max(capacity + min_amount, capacity*2);
    capacity = new_capacity;
    str = (char *)realloc(str, capacity);
    rvl_assert(str);
}

void String_Builder::append(char *new_str)
{
    rvl_assert(new_str);
    add_bytes(new_str, strlen(new_str)); // We want to only add the characters and let add string add the null terminator
}

void String_Builder::appendf(char *new_str, ...)
{
    rvl_assert(new_str);
    char buffer[512];
    va_list args;
    va_start (args, new_str);
    vsnprintf (buffer, 512, new_str, args);
    va_end (args);
    
    add_bytes(buffer, strlen(buffer)); // We want to only add the characters and let add string add the null terminator
}

// Can add any character (including null terminators) to string
// Always terminates provided characters with a '\0'
void String_Builder::add_bytes(char *bytes, size_t added_bytes)
{
    if (added_bytes == 0) return;
    if (len + added_bytes + 1 > capacity) 
    {
        grow(added_bytes);
    }
    
    memcpy(str + len, bytes, added_bytes);
    len += added_bytes;
    str[len] = '\0';
}

void String_Builder::clear()
{
    rvl_assert(capacity > 0);
    len = 0;
    str[0] = '\0';
}

// Windows stuff that we don't want clogging up main file
void
print_tray_icon_message(LPARAM lParam)
{
    static int counter = 0;
    switch (LOWORD(lParam))
    {
        case WM_RBUTTONDOWN: 
        {
            tprint("%. WM_RBUTTONDOWN", counter++); 
        }break;
        case WM_RBUTTONUP:                  
        {
            tprint("%. WM_RBUTTONUP", counter++); 
        } break;
        case WM_LBUTTONDOWN:
        {
            tprint("%. WM_LBUTTONDOWN", counter++);
        }break;
        case WM_LBUTTONUP:
        {
            tprint("%. WM_LBUTTONUP", counter++);
        }break;
        case WM_LBUTTONDBLCLK:
        {
            tprint("%. WM_LBUTTONDBLCLK", counter++);
        }break;
        case NIN_BALLOONSHOW:
        {
            tprint("%. NIN_BALLOONSHOW", counter++);
        }break;
        case NIN_POPUPOPEN:
        {
            tprint("%. NIN_POPUPOPEN", counter++);
        }break;
        case NIN_KEYSELECT:
        {
            tprint("%. NIN_KEYSELECT", counter++);
        }break;
        case NIN_BALLOONTIMEOUT:
        {
            tprint("%. NIN_BALLOONTIMEOUT", counter++);
        }break;
        case NIN_BALLOONUSERCLICK:
        {
            tprint("%. NIN_BALLOONUSERCLICK", counter++);
        }break;
        case NIN_SELECT:
        {
            tprint("%. NIN_SELECT\n", counter++);
        } break;
        case WM_CONTEXTMENU:
        {
            tprint("%. WM_CONTEXTMENU\n", counter++);
        } break;
        default:
        {
            tprint("%. OTHER", counter++);
        }break;
    }
}
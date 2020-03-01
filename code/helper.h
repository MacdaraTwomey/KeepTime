#pragma once


#include <type_traits>

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes((Value)) * 1024LL)
#define Gigabytes(Value) (Megabytes((Value)) * 1024LL)


struct Hash_Node
{
    char *key;
    u32 value;
};

struct Hash_Table
{
    static constexpr u64 DEFAULT_TABLE_SIZE = 128;
    static constexpr u8 EMPTY = 0;
    static constexpr u8 DELETED = 1;
    static constexpr u8 OCCUPIED = 2;
    
    s64 count;
    s64 size;
    Hash_Node *buckets;
    u8 *occupancy;
    
    s64 add_item(char *key, u32 value);
    bool search(char *key, u32 *value);
    void remove(char *key);
    char *get_key_by_value(u32 value);
    private:
    void grow_table();
};

struct String_Builder
{
    char *str;
    size_t capacity;
    size_t len; // Length not including null terminator
    
    void grow(size_t min_amount);
    void append(char *new_str);
    void appendf(char *new_str, ...);
    void add_bytes(char *new_str, size_t len); // Will add all bytes of string of size len (including extra null terminators you insert)
    void clear();
};


// TODO: Do you really need rear index, or is just count enough
template<typename T>
struct Queue {
    s64 front_i;
    s64 rear;
    s64 count;
    s64 capacity;
    T *data;
    
    void enqueue(T x);
    T dequeue();
    T front();
    bool empty();
    private:
    void grow();
    
    static_assert(std::is_pod<T>::value, "");
};


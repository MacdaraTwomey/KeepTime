#pragma once


#include <type_traits>

struct Block
{
    u64 size;
    u64 used;
    u8 *buffer;
    Block *prev;
};

static_assert(sizeof(Block) == 32, ""); // For alignment of buffer after block

struct Arena
{
    Block *block;
};

template<typename T, size_t N>
struct Array
{
    T data[N];
    size_t count;
    
    Array() { count = 0; }
    
    void add_item(T item) {
        //Assert(count < N);
        data[count] = item;
        count += 1;
    }
    
    void clear() { count = 0; }
    
    T &operator[](size_t index)
    {
        Assert(index >= 0 && index < count);
        return data[index];
    }
};


// Need to declare these, so hash and equals specialisations can see them
unsigned long djb2(unsigned char *str);
unsigned long djb2(unsigned char *str, size_t len);

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


struct Size_Mem
{
    u8 *memory;
    size_t size;
};


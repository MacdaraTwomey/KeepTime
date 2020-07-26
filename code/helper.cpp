
//#include "monitor.h"

#include "helper.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <algorithm>

// ---------------------------------------------- 
// Arena

// Not necessary to call this is arena is 0 initialised then can just start allocating from it
void
init_arena(Arena *arena, size_t size)
{
    u64 real_size = size + Kilobytes(10);
    
    Assert(real_size > size);
    
    // TODO: Just abort if malloc fails, not to much we can do, and can restore from savefile
    Block *block = (Block *)calloc(1, real_size + sizeof(Block));
    Assert(block);
    
    u8 *buffer = (u8 *)(block + 1); // buffer starts after block
    block->buffer = buffer;
    block->size = real_size;
    block->used = 0;
    block->prev = nullptr;
    
    arena->block = block;
}

void *
push_size(Arena *arena, size_t size)
{
    // NOTE: No heed paid to alignment, should be file as we only allocate strings and fixed size records in arenas and the initial alignment is on a 16-byte boundary.
    
    if (arena->block == nullptr ||
        arena->block->used + size > arena->block->size)
    {
        // Make arena point to a new block in list of blocks
        u64 real_size = size + Kilobytes(10);
        
        Assert(real_size > size);
        
        // TODO: Just abort if malloc fails, not to much we can do, and can restore from savefile
        Block *block = (Block *)calloc(1, real_size + sizeof(Block));
        Assert(block);
        
        u8 *buffer = (u8 *)(block + 1); // buffer starts after block
        block->buffer = buffer;
        block->size = real_size;
        block->used = 0;
        block->prev = arena->block; // null if this is the first block
        
        arena->block = block;
    }
    
    Assert(arena->block);
    Assert(arena->block->used + size <= arena->block->size);
    
    void *ptr = arena->block->buffer + arena->block->used;
    arena->block->used += size;
    
    return ptr;
}

String
push_string(Arena *arena, String string)
{
    // Passed string doesn't need to be null terminated
    // Returns null terminated string
    
    char *mem = (char *)push_size(arena, string.length+1);
    memcpy(mem, string.str, string.length);
    mem[string.length] = '\0';
    
    String s;
    s.str = mem;
    s.length = string.length;
    s.capacity = s.length;
    return s;
}

// We rely on returning null terminated string in settings keyword code
String
push_string(Arena *arena, char *string)
{
    // Passed string must be null terminated
    // Returns null terminated string
    size_t len = strlen(string);
    char *mem = (char *)push_size(arena, len+1);
    memcpy(mem, string, len+1);
    
    String s;
    s.str = mem;
    s.length = len;
    s.capacity = len;
    return s;
}

void
reset_arena(Arena *arena)
{
    // frees all blocks but the first and sets used to 0
    
    // Use this for freeing last days records after we get a new day, and also reset keyword arena on keyword update
    
    Block *current_block = arena->block;
    while (current_block)
    {
        Block *prev = current_block->prev;
        if (prev)
        {
            free(current_block); // frees whole block of allocated memory, including buffer
            current_block = prev;
        }
        else
        {
            break;
        }
    }
    
    // size stays the same
    current_block->used = 0;
    current_block->prev = nullptr;
    
    arena->block = current_block;
}

void
free_arena(Arena *arena)
{
    Block *current_block = arena->block;
    while (current_block)
    {
        Block *prev = current_block->prev;
        free(current_block); // frees whole block of allocated memory, including buffer
        if (prev)
        {
            current_block = prev;
        }
        else
        {
            break;
        }
    }
    
    arena->block = nullptr;
}

// for record allocator each day do a big alloc and say thats the max number of records for that day
// it doesn't matter if this is not filled because long term we'll be writing past records to a file
// and they wont be in memory anyway.
// Can tune amount of 'extra' if any that the arena gives, because we might not want any extra for records
// maybe assign variable to arena that says the minmumum allocation size or the amount extra it gives.

// -----------------------------------------------------------


#include <stdlib.h> // rand (debug)

// TODO: debug
int 
rand_between(int low, int high) {
    static bool first = true;
    if (first)
    {
        time_t time_now;
        srand((unsigned) time(&time_now));
        first = false;
    }
    
    float t = (float)rand() / (float)RAND_MAX;
    float result = round((1.0f - t) * low + t*high);
    return (int)result;
}

float
rand_between(float low, float high) {
    static bool first = true;
    if (first)
    {
        time_t time_now;
        srand((unsigned) time(&time_now));
        first = false;
    }
    
    float t = (float)rand() / (float)RAND_MAX;
    return (1.0f - t)*low + t*high;
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
unsigned long djb2(unsigned char *str, size_t len)
{
    unsigned long hash = 5381;
    int c;
    for (int i = 0; i < len; ++i) {
        c = str[i];
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

// TODO: Don't use this in finished version
// Also null terminates
String
copy_alloc_string(String str)
{
    String result;
    int capacity = str.length + 1;
    
    result.str = (char *)xalloc(capacity);
    memcpy(result.str, str.str, str.length);
    result.length = str.length;
    result.capacity = capacity;
    null_terminate(&result); // requires len < cap
    
    return result;
}


// TODO: Don't use this in finished version
// Also null terminates
String
copy_alloc_string(char *str)
{
    size_t length = strlen(str);
    
    String result;
    int capacity = length + 1;
    
    result.str = (char *)xalloc(capacity);
    memcpy(result.str, str, length);
    result.length = length;
    result.capacity = capacity;
    null_terminate(&result); // requires len < cap
    
    return result;
}


#if 0
bool init_hash_table(Hash_Table *table, s64 table_size)
{
    Assert(table_size > 0);
    table->buckets = (Hash_Node *)xalloc(table_size * sizeof(Hash_Node));
    table->occupancy = (u8 *)xalloc(table_size * sizeof(u8));
    table->size = table_size;
    table->count = 0;
    
    return (table->buckets && table->occupancy);
}


void free_table(Hash_Table *table)
{
    Assert(table);
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

// Returns -1 if already added, or if not, the index of the added node.
s64 Hash_Table::add_item(char *key, u32 value)
{
    Assert(key);
    Assert(key[0]);
    
    // TODO: Remove or make into more specific class
    Assert(!(value & (1 << 31))); // is_exe
    
    if (count >= size*0.75)
    {
        grow_table();
    }
    
    s64 start = (s64) djb2((unsigned char *)key) % size;
    
    s64 i = 0;
    for (; i < size && occupancy[(i+start) % size] == OCCUPIED; ++i)
    {
        if (strcmp(key, buckets[(i+start) % size].key) == 0)
        {
            return -1;
        }
    }
    
    s64 index = (i+start) % size;
    buckets[index].key = clone_string(key);
    buckets[index].value = value;
    occupancy[index] = OCCUPIED;
    count += 1;
    
    return i;
}

bool Hash_Table::search(char *key, u32 *value)
{
    Assert(key);
    Assert(key[0]);
    Assert(value);
    
    if (count == 0) return false;
    
    s64 start = (s64) djb2((unsigned char *)key) % size;
    s64 i = 0;
    for (; i < size && occupancy[(i+start) % size] != EMPTY; ++i)
    {
        s64 index = (i+start) % size;
        if (occupancy[index] == OCCUPIED)
        {
            if (strcmp(key, buckets[index].key) == 0)
            {
                *value = buckets[index].value;
                return true;
            }
        }
    }
    
    return false;
}

void Hash_Table::remove(char *key)
{
    Assert(key);
    Assert(key[0]);
    
    if (count == 0) return;
    s64 start = (s64) djb2((unsigned char *)key) % size;
    
    s64 i = 0;
    for (; i < size && occupancy[(i+start) % size] != EMPTY; ++i)
    {
        s64 index = (i+start) % size;
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
    Assert(new_buckets);
    Assert(new_occupancy);
    
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

char *Hash_Table::get_key_by_value(u32 value)
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

#endif

// -------------------------------------------------------------------------

String_Builder create_string_builder()
{
    String_Builder sb = {};
    sb.capacity = 30;
    sb.str = (char *)xalloc(sb.capacity);
    Assert(sb.str);
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
    size_t new_capacity  = (std::max)(capacity + min_amount, capacity*2);
    capacity = new_capacity;
    str = (char *)realloc(str, capacity);
    Assert(str);
}

void String_Builder::append(char *new_str)
{
    Assert(new_str);
    add_bytes(new_str, strlen(new_str)); // We want to only add the characters and let add string add the null terminator
}

void String_Builder::appendf(char *new_str, ...)
{
    Assert(new_str);
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
    Assert(capacity > 0);
    len = 0;
    str[0] = '\0';
}

// ------------------------------------------------------------------------------

template<typename T>
void init_queue(Queue<T> *queue, s64 initial_capacity)
{
    queue->front_i = 0;
    queue->capacity = std::max(initial_capacity, (s64)30);
    queue->rear = queue->capacity - 1;
    queue->count = 0;
    queue->data = (T *)malloc(sizeof(T) * queue->capacity);
    Assert(queue->data);
}

template<typename T>
void
Queue<T>::enqueue(T x) {
    if (count + 1 > capacity)
    {
        grow();
    }
    
    rear = (rear + 1) % capacity;
    data[rear] = x;
    ++count;
}

template<typename T>
T
Queue<T>::dequeue() {
    Assert(count > 0);
    T x = data[front_i];
    front_i = (front_i + 1) % capacity;
    --count;
    return x;
}

template<typename T>
T
Queue<T>::front() {
    Assert(count > 0);
    T x = data[front_i];
    return x;
}

template<typename T>
bool
Queue<T>::empty() {
    return count == 0;
}

template<typename T>
void
Queue<T>::grow()
{
    T *new_data = (T *)malloc(sizeof(T) * (capacity * 2));
    Assert(new_data);
    
    s64 n = count;
    for (s64 i = 0; i < n; ++i)
    {
        new_data[i] = dequeue();
    }
    
    front_i = 0;
    rear = n - 1;
    count = n;
    capacity *= 2;
    free(data);
    data = new_data;
}

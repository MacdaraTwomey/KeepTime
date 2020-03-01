
//#include "monitor.h"

#include "helper.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <algorithm>

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

// Returns -1 if already added, or if not, the index of the added node.
s64 Hash_Table::add_item(char *key, u32 value)
{
    rvl_assert(key);
    rvl_assert(key[0]);
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
    rvl_assert(key);
    rvl_assert(key[0]);
    rvl_assert(value);
    
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
    rvl_assert(key);
    rvl_assert(key[0]);
    
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

// ------------------------------------------------------------------------------

template<typename T>
void init_queue(Queue<T> *queue, s64 initial_capacity)
{
    queue->front_i = 0;
    queue->capacity = std::max(initial_capacity, (s64)30);
    queue->rear = queue->capacity - 1;
    queue->count = 0;
    queue->data = (T *)malloc(sizeof(T) * queue->capacity);
    rvl_assert(queue->data);
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
    rvl_assert(count > 0);
    T x = data[front_i];
    front_i = (front_i + 1) % capacity;
    --count;
    return x;
}

template<typename T>
T 
Queue<T>::front() {
    rvl_assert(count > 0);
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
    rvl_assert(new_data);
    
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

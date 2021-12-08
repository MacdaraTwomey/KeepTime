
//#include "monitor.h"

#include "helper.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include <algorithm>


//c 16 * 2 * 10000 = 320K for 10000 element hash table
//c 8 * 2 * 10000 = 160K for 10000 element hash table

// Options could make like 2000 entry table with open addressing and data inline
// Or could make the hash table smaller and use a use linked lists for collisions solving run out of room problem

hash_table MakeHashTable(arena *Arena, u64 MaximumItems)
{
    hash_table Table = {};
    Table.Buckets = Allocate(Arena, MaximumItems, hash_node);
    Table.Count = 0;
    Table.Size = MaximumItems;
    return Table;
}

// TODO Better hash function 
// Also this returns u32 on windows
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

hash_node *hash_table::GetItemSlot(string Key)
{
    u64 StartIndex = (u64)djb2((unsigned char *)Key.Str, Key.Length) % Size;
    
    hash_node *Result = nullptr;
    for (u64 i = 0; i < Size; ++i)
    {
        u64 Index = (StartIndex + i) % Size;
        if (Buckets[Index].Occupancy == OCCUPIED)
        {
            if (StringEquals(Key, Buckets[Index].Key))
            {
                // Found matching item
                Result = &Buckets[Index];
                break;
            }
        }
        else if (Buckets[Index].Occupancy == EMPTY)
        {
            // Found Empty slot
            Result = &Buckets[Index];
            break;
        }
        // If current slot deleted keep looking
    }
    
    return Result;
}

bool hash_table::ItemExists(hash_node *Node)
{
    return Node->Occupancy == OCCUPIED;
}

void hash_table::AddItem(hash_node *Node, string Key, u32 Value)
{
    Node->Key = Key;
    Node->Value = Value;
    Node->Occupancy = OCCUPIED;
    
    Count += 1;
}

#if 0
hash_node *hash_table::GetItem(string Key)
{
    u64 StartIndex = (u64)djb2((unsigned char *)Key.Str, Key.Length) % Size;
    
    hash_node *Result = nullptr;
    for (u64 i = StartIndex; i < Size; ++i)
    {
        u64 Index = i % Size;
        if (Buckets[Index].Occupancy == OCCUPIED)
        {
            if (StringEquals(Key, Buckets[Index].Key))
            {
                // Found matching item
                Result = &Buckets[Index];
                break;
            }
        }
        else if (Buckets[Index].Occupancy == EMPTY)
        {
            // Couldn't find item
            break;
        }
        // If deleted keep looping
    }
    
    return Result;
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

#endif
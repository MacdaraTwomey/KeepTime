#pragma once

template<typename T, size_t N>
struct Array
{
    T data[N];
    size_t count;
    
    Array() { count = 0; memset(data, 0, sizeof(T) * N); }
    
    void add_item(T item) {
        //Assert(count < N);
        data[count] = item;
        count += 1;
    }
    
    void clear() { count = 0; memset(data, 0, sizeof(T) * N); }
    
    T &operator[](size_t index)
    {
        Assert(index >= 0 && index < count);
        return data[index];
    }
};

// Need to declare these, so hash and equals specialisations can see them
unsigned long djb2(unsigned char *str);
unsigned long djb2(unsigned char *str, size_t len);

struct hash_node
{
    // Programs can use only long name rather than both lond and short, and if needed can have
    // auxillary cache storing program short names if we don't want to re-extract them.
    string Key;
    u32 Value;
    u32 Occupancy;
};

// TODO: Do we need deletion
struct hash_table
{
    static constexpr u8 EMPTY = 0;
    static constexpr u8 DELETED = 1; // Needed for open addressing for an Insert to know to keep probing 
    static constexpr u8 OCCUPIED = 2;
    
    hash_node *Buckets;
    u64 Count;
    u64 Size;
    //u8 *Occupancy;
    
    hash_node *GetItemSlot(string Key);
    bool ItemExists(hash_node *Node);
    void AddItem(hash_node *Node, string Key, u32 Value);
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


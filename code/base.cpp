
#include "platform.h"
#include "base.h"

// TODO: 
// Guard pages? does os already do it, probably not
// via win32 VirtualProtect
//
// GetScratchArena
// returns one of two or more thread local arena allocators and begins temporary memory with it.
// Call release scratch after done.
// Can pass currently using scratch arenas to avoid getting the same one again
// Often in a function you will pass a function parameter arena as a conflict, get a different one
// then use it in tandem.
// Reference metadesk.
//
// Default alignment of 8 is questionable, maybe default to 4, and allow passing in alignment

#define PAGE_SIZE 4096
#define DEFAULT_RESERVE_SIZE GB(1)
#define DEFAULT_COMMIT_SIZE MB(8)

// TODO: Make Better
void MemoryCopy(u64 Size, void *Source, void *Dest)
{
    u8 *SourceByte = (u8 *)Source;
    u8 *DestByte = (u8 *)Dest;
    for (u64 i = 0; i < Size; ++i)
    {
        DestByte[i] = SourceByte[i];
    }
}

void MemoryZero(u64 Size, void *Memory)
{
    u8 *Byte = (u8 *)Memory;
    for (u64 i = 0; i < Size; ++i)
    {
        Byte[i] = 0;
    }
}

bool IsPowerOfTwo(u64 x) {
	return (x & (x-1)) == 0;
}

u64 AlignUp(u64 Value, u64 Align)
{
    Assert(IsPowerOfTwo(Align));
    
    // Add maximum alignment amount (align - 1)
    // Then round off what we don't need by applying mask
    return (Value + Align - 1) & ~(Align - 1);
}

u8 *AlignUp(u8 *Ptr, u64 Align)
{
    return (u8 *)AlignUp((u64)Ptr, Align);
}

arena MakeArena(u64 Size, u32 Alignment)
{
    size_t InitalCommitSize = AlignUp(Size, PAGE_SIZE);
    
    // To reserve 1 GB it takes about 2 MB of memory for the page tables.
    
    void *Memory = PlatformMemoryReserve(DEFAULT_RESERVE_SIZE);
    
    // Can be used with temp_arena or arena
    
    Assert(Memory);
    
    void *MemoryCommited = PlatformMemoryCommit(Memory, InitalCommitSize);
    
    Assert(MemoryCommited);
    Assert(Memory == MemoryCommited);
    
    arena Arena;
    Arena.Base = (u8 *)Memory;
    Arena.Pos = 0;
    Arena.CommitPos = InitalCommitSize;
    
    return Arena;
}

arena MakeArena(u64 Size)
{
    return MakeArena(Size, 8);
}

arena MakeArena()
{
    return MakeArena(DEFAULT_COMMIT_SIZE);
}

void *PushSize(arena *Arena, u64 Size)
{
    Assert(Size != 0);
    
    u64 AlignedPos = AlignUp(Arena->Pos, Arena->Alignment);
    
    u64 Remaining = Arena->CommitPos - AlignedPos;
    
    if (Size > Remaining)
    {
        // Align up and overflow past our current page aligned capcity commit mark then adde extra free space
        size_t CommitSize = AlignUp(Size - Remaining, PAGE_SIZE) + DEFAULT_COMMIT_SIZE;
        u8 *StartOfNextCommit= Arena->Base + Arena->CommitPos;
        
        Assert(Arena->CommitPos + CommitSize <= DEFAULT_RESERVE_SIZE);
        void *CommmitResult = PlatformMemoryCommit(StartOfNextCommit, CommitSize);
        Assert(CommmitResult);
        Arena->CommitPos += CommitSize;
    }
    
    void *Result = Arena->Base + AlignedPos;
    Arena->Pos = AlignedPos + Size;
    
    return Result;
}

void *PushCopy(arena *Arena, void *Source, u64 Size)
{
    void *Memory = PushSize(Arena, Size);
    MemoryCopy(Size, Source, Memory);
    return Memory;
}

char *PushCString(arena *Arena, char *String)
{
    u64 Length = StringLength(String) + 1;
    char *Memory = (char *)PushCopy(Arena, String, Length);
    return Memory;
}

// TODO:
void PopSize(arena *Arena, u64 Pos)
{
    if (Pos < Arena->Pos)
    {
        // If more than xxx still committed is unused then decommit
    }
}

temp_memory BeginTempArena(arena *Arena)
{
    temp_memory Temp = {};
    Temp.Arena = Arena;
    Temp.StartPos = Arena->Pos; 
    
    Arena->TempCount += 1;
    
    return Temp;
}

void EndTempMemory(temp_memory TempMemory)
{
    arena *Arena = TempMemory.Arena;
    Assert(Arena->TempCount > 0);
    
    Arena->Pos = TempMemory.StartPos;
    Arena->TempCount -= 1;
}

void CheckArena(arena *Arena)
{
    // Called frequently at end of update loop
    Assert(Arena->TempCount == 0);
}

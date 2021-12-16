
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
#define COMMIT_GROW_SIZE MB(2)

// TODO: Make Better
void MemoryCopy(u64 Size, void *Source, void *Dest)
{
    Assert((Size > 0 && Source != nullptr && Dest != nullptr) || (Size == 0 && Dest == nullptr && Dest == nullptr));
    
    u8 *SourceByte = (u8 *)Source;
    u8 *DestByte = (u8 *)Dest;
    for (u64 i = 0; i < Size; ++i)
    {
        DestByte[i] = SourceByte[i];
    }
}

void MemoryZero(u64 Size, void *Memory)
{
    Assert((Size > 0 && Memory != nullptr) || 
           (Size == 0 && Memory == nullptr));
    
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
    u64 AlignMask = Align - 1;
    return (Value + AlignMask) & ~AlignMask;
}

arena CreateArena(u64 CommitSize, u64 ReserveSize)
{
    Assert(CommitSize <= ReserveSize);
    
    u64 AlignedCommitSize = AlignUp(CommitSize, PAGE_SIZE);
    u64 AlignedReserveSize = AlignUp(ReserveSize, PAGE_SIZE);
    
    // To reserve 1 GB it takes about 2 MB of memory for the page tables.
    void *Memory = PlatformMemoryReserve(AlignedReserveSize);
    Assert(Memory);
    
    arena Arena = {};
    if (PlatformMemoryCommit(Memory, AlignedCommitSize))
    {
        Arena.Base = (u8 *)Memory;
        Arena.Pos = 0;
        Arena.Commit = AlignedCommitSize;
        Arena.Capacity = AlignedReserveSize;
        Arena.TempCount = 0;
    }
    
    return Arena;
}

void FreeArena(arena *Arena)
{
    PlatformMemoryFree(Arena->Base);
    *Arena = {};
}

void *CheckTypeAlignment(void *Ptr, u32 Alignment)
{
    Assert(IsPowerOfTwo(Alignment));
    Assert(((u64)Ptr & (Alignment - 1)) == 0);
    return Ptr;
}

#if 0
void *PushSize(arena *Arena, u64 Size)
{
    Assert(Size != 0);
    
    u64 AlignedPos = AlignUp(Arena->Pos, Arena->Alignment);
    
    u64 Remaining = Arena->CommitPos - AlignedPos;
    
    if (Size > Remaining)
    {
        // Align up and overflow past our current page aligned capcity commit mark then adde extra free space
        u64 CommitSize = AlignUp(Size - Remaining, PAGE_SIZE) + COMMIT_GROW_SIZE; // needs clamping
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
#endif

// It is not really necessary to properly align allocations on modern x64 processors (unless you are doing SIMD)
// It may be undefined behaviour to access an unaligned type, but we are already ignoring strict aliasing.
// So it would be nice to disable this assuming alignment behaviour.
void *PushSize(arena *Arena, u64 Size, u32 Alignment, u8 ArenaPushFlags)
{
    Assert(Size != 0);
    
    u8 *Result = nullptr;
    u64 AlignedPos = AlignUp(Arena->Pos, Alignment);
    u64 NewPos = AlignedPos + Size;
    
    if (NewPos < Arena->Capacity)
    {
        if (NewPos > Arena->Commit)
        {
            u64 RequiredSize = Arena->Commit - NewPos;
            u64 CommitSize = AlignUp(RequiredSize + COMMIT_GROW_SIZE, PAGE_SIZE);
            CommitSize = ClampTop(CommitSize, Arena->Capacity);
            u8 *StartOfNextCommit = Arena->Base + Arena->Commit;
            if (PlatformMemoryCommit(StartOfNextCommit, CommitSize))
            {
                // Disable clear to zero as OS will clear for us
                ArenaPushFlags &= ~ArenaPushFlags_ClearToZero; 
                Arena->Commit += CommitSize;
            }
        }
        
        if (NewPos <= Arena->Commit)
        {
            Result = Arena->Base + AlignedPos;
            Arena->Pos = NewPos;
        }
    }
    
    // TODO: Could return pointer to stub of preallocated memory on failure then set some arena error flag
    Assert(Result);
    
    if (ArenaPushFlags & ArenaPushFlags_ClearToZero)
    {
        MemoryZero(Size, Result);
    }
    
    return Result;
}

// Defaults to alignment of 4 bytes 
void *PushCopy(arena *Arena, void *Source, u64 Size)
{
    void *Memory = PushSize(Arena, Size, 4, ArenaPushFlags_None);
    MemoryCopy(Size, Source, Memory);
    return Memory;
}

char *PushCString(arena *Arena, char *String)
{
    u64 Length = StringLength(String) + 1;
    char *Memory = (char *)PushCopy(Arena, String, Length);
    return Memory;
}

string PushString(arena *Arena, string String)
{
    char *StringData = (char *)PushCopy(Arena, String.Str, String.Length);
    return MakeString(StringData, String.Length);
}

// Make a new null terminated string
string PushStringNullTerminated(arena *Arena, string String)
{
    u64 Length = String.Length;
    char *StringData = (char *)PushSize(Arena, Length + 1, 4, ArenaPushFlags_None);
    MemoryCopy(Length, String.Str, StringData);
    StringData[Length] = 0;
    return MakeString(StringData, String.Length);
}

// TODO:
void PopSize(arena *Arena, u64 Pos)
{
    if (Pos < Arena->Pos)
    {
        // If more than xxx still committed is unused then decommit
    }
}

temp_memory BeginTempMemory(arena *Arena)
{
    Assert(Arena->TempCount == 0); // Assume this until I find a good way to have multiple
    
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
    
    Assert(Arena->TempCount == 0); // Assume this until I find a good way to have multiple
}

void CheckArena(arena *Arena)
{
    // Called frequently at end of update loop
    Assert(Arena->TempCount == 0);
}

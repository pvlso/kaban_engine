#if !defined(EDITOR_MEMORY_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct memory_block_footer
{
    u8 *Base;
    umm Size;
    umm Used;
};

struct temporary_memory
{
    memory_arena *Arena;
    u8 *Base;
    umm Used;
};

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
#define ZeroArray(Count, Pointer) ZeroSize(Count*sizeof((Pointer)[0]), Pointer)
inline void
ZeroSize(umm Size, void *Ptr)
{
    u8 *Byte = (u8 *)Ptr;

    __m256i Zero_32x = _mm256_set1_epi8(0);
    __m128i Zero_16x = _mm_set1_epi8(0);
    if(Size >= 32)
    {
        umm AVXSize = Size / 32;
        Size %= 32;

        for(umm I = 0; I < AVXSize; ++I)
        {
            _mm256_store_si256((__m256i *)Byte, Zero_32x);
            Byte += 32;
        }
    }

    if(Size >= 16)
    {
        umm SSESize = Size / 16;
        Size %= 16;

        for(umm I = 0; I < SSESize; ++I)
        {
            _mm_store_si128((__m128i *)Byte, Zero_16x);
            Byte += 16;
        }
    }

    while(Size--)
    {
        *Byte++ = 0;
    }
}

inline void
SetMinimumBlockSize(memory_arena *Arena, umm MinimumBlockSize)
{
    Arena->MinimumBlockSize = MinimumBlockSize;
}

inline umm
GetAlignmentOffset(memory_arena *Arena, umm Alignment)
{
    umm AlignmentOffset = 0;
    
    umm ResultPointer = (umm)Arena->Base + Arena->Used;
    umm AlignmentMask = Alignment - 1;
    if(ResultPointer & AlignmentMask)
    {
        AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
    }

    return(AlignmentOffset);
}

enum arena_push_flag
{
    ArenaFlag_ClearToZero = 0x1,
};
struct arena_push_params
{
    u32 Flags;
    u32 Alignment;
};

inline arena_push_params
DefaultArenaParams(void)
{
    arena_push_params Params;
    Params.Flags = ArenaFlag_ClearToZero;
    Params.Alignment = 4;
    return(Params);
}

inline arena_push_params
AlignNoClear(u32 Alignment)
{
    arena_push_params Params = DefaultArenaParams();
    Params.Flags &= ~ArenaFlag_ClearToZero;
    Params.Alignment = Alignment;
    return(Params);
}

inline arena_push_params
Align(u32 Alignment, b32 Clear)
{
    arena_push_params Params = DefaultArenaParams();
    if(Clear)
    {
        Params.Flags |= ArenaFlag_ClearToZero;
    }
    else
    {
        Params.Flags &= ~ArenaFlag_ClearToZero;
    }
    Params.Alignment = Alignment;
    return(Params);
}

inline arena_push_params
NoClear(void)
{
    arena_push_params Params = DefaultArenaParams();
    Params.Flags &= ~ArenaFlag_ClearToZero;
    return(Params);
}

inline umm
GetArenaSizeRemaining(memory_arena *Arena, arena_push_params Params = DefaultArenaParams())
{
    umm Result = Arena->Size - (Arena->Used + GetAlignmentOffset(Arena, Params.Alignment));

    return(Result);
}

// TODO(casey): Optional "clear" parameter!!!!
#define PushStruct(Arena, type, ...) (type *)PushSize_(Arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(Arena, Count, type, ...) (type *)PushSize_(Arena, (Count)*sizeof(type), ## __VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(Arena, Size, ## __VA_ARGS__)
#define PushCopy(Arena, Size, Source, ...) Copy(Size, Source, PushSize_(Arena, Size, ## __VA_ARGS__))
inline umm
GetEffectiveSizeFor(memory_arena *Arena, umm SizeInit, arena_push_params Params = DefaultArenaParams())
{
    umm Size = SizeInit;
        
    umm AlignmentOffset = GetAlignmentOffset(Arena, Params.Alignment);
    Size += AlignmentOffset;

    return(Size);
}

inline b32
ArenaHasRoomFor(memory_arena *Arena, umm SizeInit, arena_push_params Params = DefaultArenaParams())
{
    umm Size = GetEffectiveSizeFor(Arena, SizeInit, Params);
    b32 Result = ((Arena->Used + Size) <= Arena->Size);
    return(Result);
}

inline memory_block_footer *
GetFooter(memory_arena *Arena)
{
    memory_block_footer *Result = (memory_block_footer *)(Arena->Base + Arena->Size);
    return(Result);
}

inline void *
PushSize_(memory_arena *Arena, umm SizeInit, arena_push_params Params = DefaultArenaParams())
{
    umm Size = GetEffectiveSizeFor(Arena, SizeInit, Params);
    
    if((Arena->Used + Size) > Arena->Size)
    {
        if(!Arena->MinimumBlockSize)
        {
            Arena->MinimumBlockSize = Megabytes(1);
        }

        memory_block_footer Save;
        Save.Base = Arena->Base;
        Save.Size = Arena->Size;
        Save.Used = Arena->Used;
        
        Size = SizeInit; // NOTE(casey): The base will automatically be align now!
        umm BlockSize = Maximum(Size + sizeof(memory_block_footer), Arena->MinimumBlockSize);
        Arena->Size = BlockSize - sizeof(memory_block_footer);
        Arena->Base = (u8 *)Platform.AllocateMemory(BlockSize);
        Arena->Used = 0;
        ++Arena->BlockCount;
        
        memory_block_footer *Footer = GetFooter(Arena);
        *Footer = Save;
    }

    Assert((Arena->Used + Size) <= Arena->Size);
    
    umm AlignmentOffset = GetAlignmentOffset(Arena, Params.Alignment);
    void *Result = Arena->Base + Arena->Used + AlignmentOffset;
    Arena->Used += Size;

    Assert(Size >= SizeInit);

    if(Params.Flags & ArenaFlag_ClearToZero)
    {
        ZeroSize(SizeInit, Result);
    }
    
    return(Result);
}

// NOTE(casey): This is generally not for production use, this is probably
// only really something we need during testing, but who knows
inline char *
PushString(memory_arena *Arena, char *Source)
{
    u32 Size = 1;
    for(char *At = Source;
        *At;
        ++At)
    {
        ++Size;
    }
    
    char *Dest = (char *)PushSize_(Arena, Size, NoClear());
    for(u32 CharIndex = 0;
        CharIndex < Size;
        ++CharIndex)
    {
        Dest[CharIndex] = Source[CharIndex];
    }

    return(Dest);
}

inline char *
PushAndNullTerminate(memory_arena *Arena, u32 Length, char *Source)
{
    char *Dest = (char *)PushSize_(Arena, Length + 1, NoClear());
    for(u32 CharIndex = 0;
        CharIndex < Length;
        ++CharIndex)
    {
        Dest[CharIndex] = Source[CharIndex];
    }
    Dest[Length] = 0;

    return(Dest);
}

inline temporary_memory
BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;

    Result.Arena = Arena;
    Result.Base = Arena->Base;
    Result.Used = Arena->Used;

    ++Arena->TempCount;

    return(Result);
}

inline void
FreeLastBlock(memory_arena *Arena)
{
    void *Free = Arena->Base;
    memory_block_footer *Footer = GetFooter(Arena);

    Arena->Base = Footer->Base;
    Arena->Size = Footer->Size;
    Arena->Used = Footer->Used;

    Platform.DeallocateMemory(Free);

    --Arena->BlockCount;
}

inline void
EndTemporaryMemory(temporary_memory TempMem)
{
    memory_arena *Arena = TempMem.Arena;

    while(Arena->Base != TempMem.Base)
    {
        FreeLastBlock(Arena);
    }
    
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    Assert(Arena->TempCount > 0);
    --Arena->TempCount;
}

inline void
Clear(memory_arena *Arena)
{
    while(Arena->BlockCount > 0)
    {
        FreeLastBlock(Arena);
    }
}

inline void
CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

inline void
SubArena(memory_arena *Result, memory_arena *Arena, umm Size, arena_push_params Params = DefaultArenaParams())
{
    Result->Size = Size;
    Result->Base = (uint8 *)PushSize_(Arena, Size, Params);
    Result->Used = 0;
    Result->TempCount = 0;
}

inline void *
Copy(umm Size, void *SourceInit, void *DestInit)
{
    u8 *Source = (u8 *)SourceInit;
    u8 *Dest = (u8 *)DestInit;

    if(Size >= 32)
    {
        umm AVXSize = Size / 32;
        Size %= 32;

        for(umm I = 0; I < AVXSize; ++I)
        {
            __m256i Data = _mm256_loadu_si256((__m256i *)Source);
            _mm256_storeu_si256((__m256i *)Dest, Data);
            Source += 32;
            Dest += 32;
        }
    }
    
    while(Size--) {*Dest++ = *Source++;}

    return(DestInit);
}

inline void
StringCopy(char *Source, char *Dest)
{
    while(*Source)
    {
        *Dest++ = *Source++;
    }

    *Dest = 0;
}

#define BootstrapPushStruct(type, Member, ...) (type *)BootstrapPushSize_(sizeof(type), OffsetOf(type, Member), ## __VA_ARGS__)
inline void *
BootstrapPushSize_(umm StructSize, umm OffsetToArena, umm MinimumBlockSize = 0,
                   arena_push_params Params = DefaultArenaParams())
{
    memory_arena Bootstrap = {};
    Bootstrap.MinimumBlockSize = MinimumBlockSize;

    void *Struct = PushSize(&Bootstrap, StructSize, Params);
    *(memory_arena *)((u8 *)Struct + OffsetToArena) = Bootstrap;

    return(Struct);
}

#define EDITOR_MEMORY_H
#endif

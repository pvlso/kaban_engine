#if !defined(ENGINE_PLATFORM_H)
/* ========================================================================
   $File: $
   $Date: 2025 $
   $Revision: $
   $Creator: Pavlo Solodrai  $
   $Notice: $
   ======================================================================== */
#ifdef __cplusplus
extern "C" {
#endif

//
// NOTE(paul): Compilers
//

#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif
    
#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error SEE/NEON optimizations are not available for this compiler yet!!!!
#endif
    
//
// NOTE(paul): Types
//

#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>
    
typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef s32     b32;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
    
typedef float  f32;
typedef double f64;


typedef uintptr_t umm;
typedef intptr_t  smm;

union v2
{
    struct
    {
        f32 x, y;
    };

    struct
    {
        f32 u, v;
    };

    f32 E[2];
};

union v2i
{
    struct
    {
        s32 x, y;
    };

    s32 E[2];
};
    
union v2d
{
    __m128d V;

    struct
    {
        f64 x, y;
    };
};

union v3
{
    struct
    {
        f32 x, y, z;
    };
    struct
    {
        f32 u, v, w;
    };
    struct
    {
        f32 r, g, b;
    };
    struct
    {
        v2 xy;
        f32 Ignored0_;
    };
    struct
    {
        f32 Ignored1_;
        v2 yz;
    };
    struct
    {
        v2 uv;
        f32 Ignored2_;
    };
    struct
    {
        f32 Ignored3_;
        v2 vw;
    };
    f32 E[3];
};

union v4
{
    struct
    {
        union
        {
            v3 xyz;
            struct
            {
                f32 x, y, z;
            };
        };
        
        f32 w;        
    };
    struct
    {
        union
        {
            v3 rgb;
            struct
            {
                f32 r, g, b;
            };
        };
        
        f32 a;        
    };
    struct
    {
        v2 xy;
        f32 Ignored0_;
        f32 Ignored1_;
    };
    struct
    {
        f32 Ignored2_;
        v2 yz;
        f32 Ignored3_;
    };
    struct
    {
        f32 Ignored4_;
        f32 Ignored5_;
        v2 zw;
    };
    f32 E[4];
};

struct rectangle2
{
    v2 Min;
    v2 Max;
};

struct rectangle2i
{
    v2i Min;
    v2i Max;
};

struct rectangle3
{
    v3 Min;
    v3 Max;
};

//
// NOTE(paul): Defines
//

#define U32FromPointer(Pointer) ((u32)(umm)(Pointer))
#define PointerFromU32(type, Value) (type *)((umm)Value)
#define OffsetOf(type, Member) (umm)&(((type *)0)->Member)
    
#define U16Maximum 0xFFFF
#define U32Maximum 0xFFFFFFFF
#define U64Maximum 0xFFFFFFFFFFFFFFFF

#define F32Maximum FLT_MAX
#define F32Minimum -FLT_MAX

#define Real64Maximum DBL_MAX
#define Real64Minimum -DBL_MAX
    
#if !defined(internal)
#define internal static
#endif
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f
#define Tau32 6.28318530717958647692f
    
#if ENGINE_SLOW
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {InvalidCodePath;} break

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)

//
// NOTE(paul): Atomics
//
#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()

inline u32 AtomicCompareExchangeUInt32(u32 volatile *Value, u32 New, u32 Expected)
{
    u32 Result = _InterlockedCompareExchange((long volatile *)Value, New, Expected);

    return(Result);
}
inline u64 AtomicExchangeU64(u64 volatile *Value, u64 New)
{
    u64 Result = _InterlockedExchange64((__int64 volatile *)Value, New);

    return(Result);
}
inline u64 AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    // NOTE(casey): Returns the original value _prior_ to adding
    u64 Result = _InterlockedExchangeAdd64((__int64 volatile *)Value, Addend);

    return(Result);
}    
inline u32 GetThreadID(void)
{
    u8 *ThreadLocalStorage = (u8 *)__readgsqword(0x30);
    u32 ThreadID = *(u32 *)(ThreadLocalStorage + 0x48);

    return(ThreadID);
}

#elif COMPILER_LLVM
#define CompletePreviousReadsBeforeFutureReads asm volatile("" ::: "memory")
#define CompletePreviousWritesBeforeFutureWrites asm volatile("" ::: "memory")
inline uint32 AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = __sync_val_compare_and_swap(Value, Expected, New);

    return(Result);
}
inline u64 AtomicExchangeU64(u64 volatile *Value, u64 New)
{
    u64 Result = __sync_lock_test_and_set(Value, New);

    return(Result);
}
inline u64 AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    // NOTE(casey): Returns the original value _prior_ to adding
    u64 Result = __sync_fetch_and_add(Value, Addend);

    return(Result);
}    
inline u32 GetThreadID(void)
{
    u32 ThreadID;
#if defined(__APPLE__) && defined(__x86_64__)
    asm("mov %%gs:0x00,%0" : "=r"(ThreadID));
#elif defined(__i386__)
    asm("mov %%gs:0x08,%0" : "=r"(ThreadID));
#elif defined(__x86_64__)
    asm("mov %%fs:0x10,%0" : "=r"(ThreadID));
#else
#error Unsupported architecture
#endif

    return(ThreadID);
}
#else
// TODO(casey): Other compilers/platforms??
#endif

struct ticket_mutex
{
    u64 volatile Ticket;
    u64 volatile Serving;
};

inline void
BeginTicketMutex(ticket_mutex *Mutex)
{
    u64 Ticket = AtomicAddU64(&Mutex->Ticket, 1);
    while(Ticket != Mutex->Serving) {_mm_pause();};
}

inline void
EndTicketMutex(ticket_mutex *Mutex)
{
    AtomicAddU64(&Mutex->Serving, 1);
}

/*
  NOTE(casey): Services that the platform layer provides to the editor
*/
#if EDITOR_INTERNAL
// TODO(paul): Debug Staff
#endif

/*
  NOTE(casey): Services that the editor provides to the platform layer.
  (this may expand in the future - sound on separate thread, etc.)
*/

// FOUR THINGS - timing, controller/keyboard input, bitmap buffer to use, sound buffer to use

#define BITMAP_BYTES_PER_PIXEL 4
typedef struct offscreen_buffer
{
    // NOTE(casey): Pixels are always 32-bits wide, Memory Order BB GG RR XX
    void *Memory;
    s32 Width;
    s32 Height;
    s32 Pitch;
} offscreen_buffer;

typedef struct render_commands
{
    u32 Width;
    u32 Height;
    
    u32 MaxPushBufferSize;
    u32 PushBufferSize;
    u8 *PushBufferBase;
    
    u32 PushBufferElementCount;
    u32 SortEntryAt;

    v4 ClearColor;
    
    u32 ClipRectCount;

    u32 MaxRenderTargetIndex;

    struct render_entry_cliprect *FirstRect;
    struct render_entry_cliprect *LastRect;
} render_commands;

#define RenderCommandStruct(MaxPushBufferSize, PushBuffer, Width, Height) \
    {Width, Height, MaxPushBufferSize, 0, (u8 *)PushBuffer, 0, MaxPushBufferSize};

inline struct sort_sprite_bound *
GetSortEntries(render_commands *Commands)
{
    sort_sprite_bound *Result = (sort_sprite_bound *)Commands->PushBufferBase;

    return(Result);
}

typedef struct editor_render_prep
{
    struct render_entry_cliprect *ClipRects;
} editor_rende_prep;

typedef struct button_state
{
    s32 HalfTransitionCount;
    b32 EndedDown;
} button_state;

typedef struct controller_input
{
    b32 IsConnected;
    b32 IsAnalog;    
    f32 StickAverageX;
    f32 StickAverageY;
    
    union
    {
        button_state Buttons[20];
        struct
        {
            button_state MoveUp;
            button_state MoveDown;
            button_state MoveLeft;
            button_state MoveRight;
            
            button_state ActionUp;
            button_state ActionDown;
            button_state ActionLeft;
            button_state ActionRight;

            button_state FirstMode;
            button_state SecondMode;
            button_state ThirdMode;
            button_state Fill;
            
            button_state LeftShoulder;
            button_state RightShoulder;

            button_state Back;
            button_state Start;

            button_state PlayMusic;
            button_state TerminateSound;

            button_state Undo;
            button_state UIEnable;

            // NOTE(casey): All buttons must be added above this line
            
            button_state Terminator;
        };
    };

} controller_input;

enum input_mouse_button
{
    PlatformMouseButton_Left,
    PlatformMouseButton_Middle,
    PlatformMouseButton_Right,
    PlatformMouseButton_Extended0,
    PlatformMouseButton_Extended1,

    PlatformMouseButton_Count,
};

typedef struct input
{
    f32 dtForFrame;

    controller_input Controllers[2];

    // NOTE(casey): Signals back to the platform layer
    b32 QuitRequested;

    button_state MouseButtons[PlatformMouseButton_Count];
    f32 MouseX, MouseY;
    s16 MouseZ;
    
    b32 ShiftDown, AltDown, ControlDown;
} input;

inline controller_input *
GetController(input *Input, u32 ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    
    controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}

inline b32
WasPressed(button_state State)
{
    b32 Result = ((State.HalfTransitionCount > 1) ||
                  ((State.HalfTransitionCount == 1) && (State.EndedDown)));

    return(Result);
}

typedef struct memory
{
    struct editor_state *EditorState;
    struct transient_state *TransientState;
    
    b32 ExecutableReloaded;
} memory;

#define ENGINE_UPDATE_AND_RENDER(name) void name(memory *Memory, input *Input, render_commands *RenderCommands)
typedef ENGINE_UPDATE_AND_RENDER(engine_update_and_render);

#ifdef __cplusplus
}
#endif
    
#define ENGINE_PLATFORM_H
#endif

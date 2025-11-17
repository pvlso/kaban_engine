#if !defined(EDITOR_PLATFORM_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(casey):

  EDITOR_INTERNAL:
    0 - Build for public release
    1 - Build for developer only

  EDITOR_SLOW:
    0 - Not slow code allowed!
    1 - Slow code welcome.
*/

#ifdef __cplusplus
extern "C" {
#endif

//
// NOTE(casey): Compilers
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
    
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Types
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>
    
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;
    
typedef float real32;
typedef double real64;
    
typedef int8 s8;
typedef int8 s08;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef bool32 b32;

typedef uint8 u8;
typedef uint8 u08;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef real32 r32;
typedef real64 r64;
typedef real32 f32;
typedef real64 f64;

typedef uintptr_t umm;
typedef intptr_t  smm;

typedef int32 fp22_10;
    
struct memory_arena
{
    umm Size;
    u8 *Base;
    umm Used;

    umm MinimumBlockSize;
    
    u32 BlockCount;
    s32 TempCount;
};

// NOTE(pvlso): V2 ============================================================================================================================================
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
    
union v2d
{
    __m128d V;

    struct
    {
        f64 x, y;
    };

    struct
    {
        f32 u, v;
    };

    f64 E[2];
};

union v2i
{
    struct
    {
        s32 x, y;
    };

    s32 E[2];
};

union v2di
{
    __m128i V;

    struct
    {
        s64 x, y;
    };

    s64 E[2];
};

// ===========================================================================================================================================================

// NOTE(pvlso): V3 ============================================================================================================================================
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

union v3d
{
    struct
    {
        f64 x, y, z;
    };
    struct
    {
        f64 u, v, w;
    };
    struct
    {
        f64 r, g, b;
    };
    struct
    {
        v2d xy;
        f64 Ignored0_;
    };
    struct
    {
        f64 Ignored1_;
        v2d yz;
    };
    struct
    {
        v2d uv;
        f64 Ignored2_;
    };
    struct
    {
        f64 Ignored3_;
        v2d vw;
    };

    f64 E[3];
};

union v3i
{
    struct
    {
        s32 x, y, z;
    };
    struct
    {
        s32 u, v, w;
    };
    struct
    {
        s32 r, g, b;
    };
    struct
    {
        v2i xy;
        s32 Ignored0_;
    };
    struct
    {
        s32 Ignored1_;
        v2i yz;
    };
    struct
    {
        v2i uv;
        s32 Ignored2_;
    };
    struct
    {
        s32 Ignored3_;
        v2i vw;
    };

    s32 E[3];
};

union v3di
{
    struct
    {
        s64 x, y, z;
    };
    struct
    {
        s64 u, v, w;
    };
    struct
    {
        s64 r, g, b;
    };
    struct
    {
        v2di xy;
        s64 Ignored0_;
    };
    struct
    {
        s64 Ignored1_;
        v2di yz;
    };
    struct
    {
        v2di uv;
        s64 Ignored2_;
    };
    struct
    {
        s64 Ignored3_;
        v2di vw;
    };

    s64 E[3];
};

// ===========================================================================================================================================================

// NOTE(pvlso): V4 ============================================================================================================================================
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

    __m128 V;
    f32 E[4];
};

union v4d
{
    __m256d V;
    struct
    {
        union
        {
            v3d xyz;
            struct
            {
                f64 x, y, z;
            };
        };
        
        f64 w;        
    };

    struct
    {
        union
        {
            v3d rgb;
            struct
            {
                f64 r, g, b;
            };
        };
        
        f64 a;        
    };

    struct
    {
        v2d xy;
        f64 Ignored0_;
        f64 Ignored1_;
    };

    struct
    {
        f64 Ignored2_;
        v2d yz;
        f64 Ignored3_;
    };

    struct
    {
        f64 Ignored4_;
        f64 Ignored5_;
        v2d zw;
    };

    f64 E[4];
};

union v4i
{
    struct
    {
        union
        {
            v3i xyz;
            struct
            {
                s32 x, y, z;
            };
        };
        
        s32 w;        
    };

    struct
    {
        union
        {
            v3i rgb;
            struct
            {
                s32 r, g, b;
            };
        };
        
        s32 a;        
    };
    struct
    {
        v2i xy;
        s32 Ignored0_;
        s32 Ignored1_;
    };
    struct
    {
        s32 Ignored2_;
        v2i yz;
        s32 Ignored3_;
    };
    struct
    {
        s32 Ignored4_;
        s32 Ignored5_;
        v2i zw;
    };

    __m128i V;
    s32 E[4];
};

union v4di
{
    __m256i V;
    struct
    {
        union
        {
            v3di xyz;
            struct
            {
                s64 x, y, z;
            };
        };
        
        s64 w;        
    };

    struct
    {
        union
        {
            v3di rgb;
            struct
            {
                s64 r, g, b;
            };
        };
        
        s64 a;        
    };

    struct
    {
        v2di xy;
        s64 Ignored0_;
        s64 Ignored1_;
    };

    struct
    {
        s64 Ignored2_;
        v2di yz;
        s64 Ignored3_;
    };

    struct
    {
        s64 Ignored4_;
        s64 Ignored5_;
        v2di zw;
    };

    s64 E[4];
};
// ===========================================================================================================================================================

// NOTE(pvlso): RECT2 =========================================================================================================================================

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

struct rectangle2d
{
    v2d Min;
    v2d Max;
};

struct rectangle2di
{
    v2di Min;
    v2di Max;
};

// ===========================================================================================================================================================

// NOTE(pvlso): RECT3 =========================================================================================================================================

struct rectangle3
{
    v3 Min;
    v3 Max;
};

struct rectangle3i
{
    v3i Min;
    v3i Max;
};

struct rectangle3d
{
    v3d Min;
    v3d Max;
};

struct rectangle3di
{
    v3di Min;
    v3di Max;
};

// ===========================================================================================================================================================

// NOTE(pvlso): TRIANGLE ======================================================================================================================================
// ===========================================================================================================================================================
struct triangle
{
    v2 Vertices[3];
    rectangle2 Bounds;
};

struct triangled
{
    v2d Vertices[3];
    rectangle2d Bounds;
};

// NOTE(pvlso): POLYGON =======================================================================================================================================

#define COMMON_EPSILON_F32 1.19e-07
#define COMMON_EPSILON_F64 0.0000000000001f

#define MAX_HOLE_COUNT 128

struct polygon2
{
    s32 VertexCount;
    v2 *Vertices;

    b32 HasHoles;
    s32 HoleCount;
    s32 *HoleVertexCounts;
    v2 *HolesVertices;
};

struct polygon2d
{
    s32 VertexCount;
    v2d *Vertices;

    b32 HasHoles;
    s32 HoleCount;
    s32 *HoleVertexCounts;
    v2d *HolesVertices;
};

#define MAX_POLYGON_COUNT 32
struct polygon2_set
{
    s32 PolygonCount;
    polygon2 *Polygons;
};

struct polygon2d_set
{
    s32 PolygonCount;
    polygon2d *Polygons;
};
    
// ===========================================================================================================================================================

// NOTE(pvlso): LINE ==========================================================================================================================================
struct lined
{
    v2d a;
    v2d b;
};

struct line
{
    v2 a;
    v2 b;
};
// ===========================================================================================================================================================

#include "engine_file_formats.h"
#include "engine_file_formats_.h"
//#include "engine_asset_new.h"

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): COMMON DEFINES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define U32FromPointer(Pointer) ((u32)(umm)(Pointer))
#define PointerFromU32(type, Value) (type *)((umm)Value)

#define OffsetOf(type, Member) (umm)&(((type *)0)->Member)
    
#define U16Maximum 0xFFFF
#define U32Maximum 0xFFFFFFFF
#define U64Maximum 0xFFFFFFFFFFFFFFFF

#define Real32Maximum FLT_MAX
#define Real32Minimum -FLT_MAX

#define Real64Maximum DBL_MAX
#define Real64Minimum -DBL_MAX

#if !defined(internal)
#define internal static
#endif
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f
#define Tau32 6.28318530717958647692f
    
#if EDITOR_SLOW
// TODO(casey): Complete assertion macro - don't worry everyone!
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
// TODO(casey): swap, min, max ... macros???

#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
    

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): MULTITHREADING
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline uint32
SafeTruncateUInt64(uint64 Value)
{
    Assert(Value <= U32Maximum);
    uint32 Result = (uint32)Value;
    return(Result);
}

inline u16
SafeTruncateToU16(uint32 Value)
{
    Assert(Value <= U16Maximum);
    u16 Result = (u16)Value;
    return(Result);
}

#if COMPILER_MSVC
#define CompletePreviousReadsBeforeFutureReads _ReadBarrier()
#define CompletePreviousWritesBeforeFutureWrites _WriteBarrier()
inline uint32
AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = _InterlockedCompareExchange((long volatile *)Value, New, Expected);

    return(Result);
}

inline u64
AtomicExchangeU64(u64 volatile *Value, u64 New)
{
    u64 Result = _InterlockedExchange64((__int64 volatile *)Value, New);

    return(Result);
}

inline u64
AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    // NOTE(casey): Returns the original value _prior_ to adding
    u64 Result = _InterlockedExchangeAdd64((__int64 volatile *)Value, Addend);

    return(Result);
}    

inline u32
GetThreadID(void)
{
    u8 *ThreadLocalStorage = (u8 *)__readgsqword(0x30);
    u32 ThreadID = *(u32 *)(ThreadLocalStorage + 0x48);

    return(ThreadID);
}

#elif COMPILER_LLVM
#define CompletePreviousReadsBeforeFutureReads asm volatile("" ::: "memory")
#define CompletePreviousWritesBeforeFutureWrites asm volatile("" ::: "memory")

inline uint32
AtomicCompareExchangeUInt32(uint32 volatile *Value, uint32 New, uint32 Expected)
{
    uint32 Result = __sync_val_compare_and_swap(Value, Expected, New);

    return(Result);
}

inline u64
AtomicExchangeU64(u64 volatile *Value, u64 New)
{
    u64 Result = __sync_lock_test_and_set(Value, New);

    return(Result);
}

inline u64
AtomicAddU64(u64 volatile *Value, u64 Addend)
{
    // NOTE(casey): Returns the original value _prior_ to adding
    u64 Result = __sync_fetch_and_add(Value, Addend);

    return(Result);
}    

inline u32
GetThreadID(void)
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
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

/*
  NOTE(casey): Services that the editor provides to the platform layer.
  (this may expand in the future - sound on separate thread, etc.)
*/

// FOUR THINGS - timing, controller/keyboard input, bitmap buffer to use, sound buffer to use

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): RENDERING
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct editor_render_commands
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
} editor_render_commands;

#define RenderCommandStruct(MaxPushBufferSize, PushBuffer, Width, Height) \
    {Width, Height, MaxPushBufferSize, 0, (u8 *)PushBuffer, 0, MaxPushBufferSize};

inline struct sort_sprite_bound *
GetSortEntries(editor_render_commands *Commands)
{
    sort_sprite_bound *Result = (sort_sprite_bound *)Commands->PushBufferBase;

    return(Result);
}

typedef struct editor_render_prep
{
    struct render_entry_cliprect *ClipRects;
} editor_rende_prep;
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): INPUT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct engine_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
} engine_button_state;

typedef struct engine_controller_input
{
    bool32 IsConnected;
    bool32 IsAnalog;    
    real32 StickAverageX;
    real32 StickAverageY;
    
    union
    {
        engine_button_state Buttons[22];
        struct
        {
            engine_button_state MoveUp;
            engine_button_state MoveDown;
            engine_button_state MoveLeft;
            engine_button_state MoveRight;
            
            engine_button_state ActionUp;
            engine_button_state ActionDown;
            engine_button_state ActionLeft;
            engine_button_state ActionRight;

            engine_button_state FirstMode;
            engine_button_state SecondMode;
            engine_button_state ThirdMode;
            engine_button_state ForthMode;
            engine_button_state Fill;
            
            engine_button_state LeftShoulder;
            engine_button_state RightShoulder;

            engine_button_state Back;
            engine_button_state Start;

            engine_button_state PlayMusic;
            engine_button_state TerminateSound;

            engine_button_state Undo;
            engine_button_state ShowProfiler;
            engine_button_state ShowUI;

            // NOTE(casey): All buttons must be added above this line
            
            engine_button_state Terminator;
        };
    };

} engine_controller_input;

enum engine_input_mouse_button
{
    PlatformMouseButton_Left,
    PlatformMouseButton_Middle,
    PlatformMouseButton_Right,
    PlatformMouseButton_Extended0,
    PlatformMouseButton_Extended1,

    PlatformMouseButton_Count,
};

typedef struct engine_input
{
    r32 dtForFrame;

    engine_controller_input Controllers[2];

    // NOTE(casey): Signals back to the platform layer
    b32 QuitRequested;

    engine_button_state MouseButtons[PlatformMouseButton_Count];
    r32 MouseX, MouseY;
    s16 MouseZ;
    
    b32 ShiftDown, AltDown, ControlDown;
} engine_input;

inline engine_controller_input *
GetController(engine_input *Input, int unsigned ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    
    engine_controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}

inline b32
WasPressed(engine_button_state State)
{
    b32 Result = ((State.HalfTransitionCount > 1) ||
                  ((State.HalfTransitionCount == 1) && (State.EndedDown)));

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): DEBUG
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#if EDITOR_INTERNAL
typedef struct debug_executing_process
{
    u64 OSHandle;
} debug_executing_process;
    
typedef struct debug_process_state
{
    b32 StartedSuccessfully;
    b32 IsRunning;
    s32 ReturnCode;
} debug_process_state;
    

#define DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(name) debug_executing_process name(char *Path, char *Command, char *CommandLine)
typedef DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(debug_platform_execute_system_command);

#define DEBUG_PLATFORM_GET_PROCESS_STATE(name) debug_process_state name(debug_executing_process Process)
typedef DEBUG_PLATFORM_GET_PROCESS_STATE(debug_platform_get_process_state);

extern struct engine_memory *DebugGlobalMemory;
    
#endif
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): FILE API
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct platform_file_handle
{
    b32 NoErrors;
    void *Platform;
} platform_file_handle;
    
typedef struct platform_file_group
{
    u32 FileCount;
    void *Platform;
} platform_file_group;

// NOTE(pvlso): File type classification used by the platform layer.
// - None: use the filename exactly as provided (no prefix/pattern).
// - Other types: map to a directory or wildcard used during file lookup.
// - TXT/JSON: treated as text files; loader appends a null terminator.
typedef enum platform_file_type
{
    PlatformFileType_None,

    PlatformFileType_KEA,
    PlatformFileType_KESA,
    PlatformFileType_KEWM,

    PlatformFileType_BMP,
    PlatformFileType_SSBMP, // Sprite Sheet
    PlatformFileType_TSBMP, // Tile Set
    PlatformFileType_STBMP, // Solid Tile
    PlatformFileType_WAV,
    PlatformFileType_TXT,
    PlatformFileType_JSON,
    PlatformFileType_TTF,
    PlatformFileType_BIN,
    
    PlatformFileType_Count,
} platform_file_type;

typedef enum platform_file_op
{
    PlatformFileOp_Read,
    PlatformFileOp_Write,
    PlatformFileOp_WriteExisting,

} platform_file_op;

#define PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(name) platform_file_group name(platform_file_type Type)
typedef PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(platform_get_all_files_of_type_begin);

#define PLATFORM_GET_ALL_FILE_OF_TYPE_END(name) void name(platform_file_group *FileGroup)
typedef PLATFORM_GET_ALL_FILE_OF_TYPE_END(platform_get_all_files_of_type_end);

#define PLATFORM_OPEN_NEXT_FILE(name) platform_file_handle name(platform_file_group *FileGroup)
typedef PLATFORM_OPEN_NEXT_FILE(platform_open_next_file);

#define PLATFORM_OPEN_FILE(name) platform_file_handle name(char *FileName, platform_file_type Type, platform_file_op Op)
typedef PLATFORM_OPEN_FILE(platform_open_file);

#define PLATFORM_CLOSE_FILE(name) void name(platform_file_handle *Handle)
typedef PLATFORM_CLOSE_FILE(platform_close_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_WRITE_DATA_TO_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Data)
typedef PLATFORM_WRITE_DATA_TO_FILE(platform_write_data_to_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char *Message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

// NOTE(pvlso): Dest and Arena can be specified to 0 if you want to know only count
#define PLATFORM_LIST_FILES_IN_DIRECTORY(name) u32 name(platform_file_type Type, char **Dest, memory_arena *Arena)
typedef PLATFORM_LIST_FILES_IN_DIRECTORY(platform_list_files_in_directory);

typedef struct read_file_result
{
    u32 Size;
    void *Contents;
} read_file_result;

#define PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_FREE_FILE_MEMORY(platform_free_file_memory);

#define PLATFORM_READ_ENTIRE_FILE(name) read_file_result name(char *FileName, platform_file_type Type, memory_arena *Arena, b32 IsTXT)
typedef PLATFORM_READ_ENTIRE_FILE(platform_read_entire_file);

#define PLATFORM_WRITE_ENTIRE_FILE(name) u32 name(char *FileName, platform_file_type Type, u8 *Data, u32 Size)
typedef PLATFORM_WRITE_ENTIRE_FILE(platform_write_entire_file);

#define PlatformNoFileErrors(Handle) ((Handle)->NoErrors)
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): WORK QUEUES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
struct platform_work_queue;
#define PLATFORM_WORK_QUEUE_CALLBACK(name) void name(platform_work_queue *Queue, void *Data)
typedef PLATFORM_WORK_QUEUE_CALLBACK(platform_work_queue_callback);

#define PLATFORM_ALLOCATE_MEMORY(name) void *name(umm Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_DEALLOCATE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_DEALLOCATE_MEMORY(platform_deallocate_memory);
    
typedef void platform_add_entry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data);
typedef void platform_complete_all_work(platform_work_queue *Queue);

struct platform_texture_op_queue
{
    ticket_mutex Mutex;

    struct texture_op *First;
    texture_op *Last;
    texture_op *FirstFree;
};
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): NUKLEAR API
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#include "engine_platform_nuklear.h"

typedef struct builder_loaded_font
{
    u32 OnePastHighestCodePoint;
    u32 GlyphCount;
    r32 AscenderHeight;
    r32 DescenderHeight;
    r32 ExternalLeading;

    u32 *UnicodeCodePoints;
    struct loaded_bitmap *Glyphs;
    r32 *HorizontalAdvance;
    u16 *UnicodeMap;
} builder_loaded_font;

#define PLATFORM_LOAD_FONT_ASSET(name) builder_loaded_font name(char *FileName, u32 FontSize, memory_arena *Arena)
typedef PLATFORM_LOAD_FONT_ASSET(platform_load_font_asset);

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): PLATFORM API
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct platform_api
{
    platform_add_entry *AddEntry;
    platform_complete_all_work *CompleteAllWork;

    platform_get_all_files_of_type_begin *GetAllFilesOfTypeBegin;
    platform_get_all_files_of_type_end *GetAllFilesOfTypeEnd;
    platform_open_next_file *OpenNextFile;
    platform_open_file *OpenFile;
    platform_close_file *CloseFile;
    platform_read_data_from_file *ReadDataFromFile;
    platform_write_data_to_file *WriteDataToFile;
    platform_file_error *FileError;

    platform_list_files_in_directory *ListFilesInDirectory;
    platform_free_file_memory *FreeFileMemory;
    platform_read_entire_file *ReadEntireFile;
    platform_write_entire_file *WriteEntireFile;

    
    platform_allocate_memory *AllocateMemory;
    platform_deallocate_memory *DeallocateMemory;

    nk_ui UI;

    platform_load_font_asset *LoadFontAsset;    
    
#if EDITOR_INTERNAL
    debug_platform_execute_system_command *DEBUGExecuteSystemCommand;
    debug_platform_get_process_state *DEBUGGetProcessState;
#endif

} platform_api;

extern platform_api Platform;
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): ENGINE MEMORY
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

typedef struct engine_memory
{
    struct editor_state *EditorState;
    struct transient_state *TransientState;
    
#if EDITOR_INTERNAL
    struct debug_table *DebugTable;
    struct debug_state *DebugState;
#endif

    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;
    platform_texture_op_queue TextureOpQueue;
    
    b32 ExecutableReloaded;
    platform_api PlatformAPI;
} engine_memory;

#define ENGINE_UPDATE_AND_RENDER(name) void name(struct nk_context *nk, engine_memory *Memory, engine_input *Input, editor_render_commands *RenderCommands)
typedef ENGINE_UPDATE_AND_RENDER(engine_update_and_render);
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): AUDIO
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct engine_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;

    // IMPORTANT(casey): Samples must be padded to a multiple of 4 samples!
    int16 *Samples;
} engine_sound_output_buffer;

// NOTE(casey): At the moment, this has to be a very fast function, it cannot be
// more than a millisecond or so.
// TODO(casey): Reduce the pressure on this function's performance by measuring it
// or asking about it, etc.
#define ENGINE_GET_SOUND_SAMPLES(name) void name(engine_memory *Memory, engine_sound_output_buffer *SoundBuffer)
typedef ENGINE_GET_SOUND_SAMPLES(engine_get_sound_samples);
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): DEBUG INTERFACE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

struct debug_table;
#define DEBUG_EDITOR_FRAME_END(name) void name(struct nk_context *nk, engine_memory *Memory, engine_input *Input, editor_render_commands *RenderCommands)
typedef DEBUG_EDITOR_FRAME_END(debug_editor_frame_end);

struct debug_id
{
    void *Value[2];
};

#if EDITOR_INTERNAL
enum debug_type
{
    DebugType_Unknown,

    DebugType_FrameMarker,
    DebugType_BeginBlock,
    DebugType_EndBlock,

    DebugType_OpenDataBlock,
    DebugType_CloseDataBlock,

//    DebugType_MarkDebugValue,

    DebugType_b32,
    DebugType_r32,
    DebugType_u32,
    DebugType_s32,
    DebugType_v2,
    DebugType_v3,
    DebugType_v4,
    DebugType_rectangle2,
    DebugType_rectangle3,
    DebugType_bitmap_id,    
    DebugType_sound_id,    
    DebugType_font_id,    
    DebugType_memory_arena_p,

    DebugType_ThreadIntervalGraph,
    DebugType_FrameBarGraph,
    DebugType_LastFrameInfo,
    DebugType_DebugMemoryInfo,
    DebugType_FrameSlider,
    DebugType_TopClocksList,
    
    DebugType_ArenaOccupancy,
};
typedef struct memory_arena *memory_arena_p;
struct debug_event
{
    u64 Clock;
    char *GUID;
    u16 ThreadID;
    u16 CoreIndex;
    u8 Type;
    union
    {
        debug_id DebugID;
        debug_event *Value_debug_event;

        b32 Value_b32;
        s32 Value_s32;
        u32 Value_u32;
        r32 Value_r32;
        v2 Value_v2;
        v3 Value_v3;
        v4 Value_v4;
        rectangle2 Value_rectangle2;
        rectangle3 Value_rectangle3;
        bitmap_id Value_bitmap_id;
        sound_id Value_sound_id;
        font_id Value_font_id;
        memory_arena_p Value_memory_arena_p;
    };
};

struct debug_table
{
    debug_event EditEvent;
    u32 RecordIncrement;
    
    // TODO(casey): No attempt is currently made to ensure that the final
    // debug records being written to the event array actually complete
    // their output prior to the swap of the event array index.    
    u32 CurrentEventArrayIndex;
    // TODO(casey): This could actually be a u32 atomic now, since we
    // only need 1 bit to store which array we're using...
    u64 volatile EventArrayIndex_EventIndex;
    debug_event Events[2][16*65536];
};

extern debug_table *GlobalDebugTable;

#define UniqueFileCounterString__(A, B, C, D) A "|" #B "|" #C "|" D
#define UniqueFileCounterString_(A, B, C, D) UniqueFileCounterString__(A, B, C, D)
#define DEBUG_NAME(Name) UniqueFileCounterString_(__FILE__, __LINE__, __COUNTER__, Name)

#define DEBUGSetEventRecording(Enabled) (GlobalDebugTable->RecordIncrement = (Enabled) ? 1 : 0)

#define RecordDebugEvent(EventType, GUIDInit)                           \
    u64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, GlobalDebugTable->RecordIncrement); \
    u32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;                \
    Assert(EventIndex < ArrayCount(GlobalDebugTable->Events[0]));       \
    debug_event *Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32] + EventIndex; \
    Event->Clock = __rdtsc();                                           \
    Event->Type = (u8)EventType;                                        \
    Event->CoreIndex = 0;                                               \
    Event->ThreadID = (u16)GetThreadID();                               \
    Event->GUID = GUIDInit;

#define FRAME_MARKER(SecondsElapsedInit)                                \
    {RecordDebugEvent(DebugType_FrameMarker, DEBUG_NAME("Frame Marker")); \
        Event->Value_r32 = SecondsElapsedInit;}  

#define TIMED_BLOCK__(GUID, Number, ...) timed_block TimedBlock_##Number(GUID, ## __VA_ARGS__)
#define TIMED_BLOCK_(GUID, Number, ...) TIMED_BLOCK__(GUID, Number, ## __VA_ARGS__)
#define TIMED_BLOCK(Name, ...) TIMED_BLOCK_(DEBUG_NAME(Name), __COUNTER__, ## __VA_ARGS__)
#define TIMED_FUNCTION(...) TIMED_BLOCK_(DEBUG_NAME(__FUNCTION__), ## __VA_ARGS__)

#define BEGIN_BLOCK_(GUID) {RecordDebugEvent(DebugType_BeginBlock, GUID);}
#define END_BLOCK_(GUID) {RecordDebugEvent(DebugType_EndBlock, GUID);}

#define BEGIN_BLOCK(Name) BEGIN_BLOCK_(DEBUG_NAME(Name))
#define END_BLOCK() END_BLOCK_(DEBUG_NAME("END_BLOCK_"))

struct timed_block
{
    timed_block(char *GUID, u32 HitCountInit = 1)
    {
        BEGIN_BLOCK_(GUID);
    }
    
    ~timed_block()
    {
        END_BLOCK();
    }
};

#else

#define TIMED_BLOCK(...) 
#define TIMED_FUNCTION(...) 
#define BEGIN_BLOCK(...)
#define END_BLOCK(...)
#define FRAME_MARKER(...)

#endif

//
// NOTE(casey): Shared utils
//
inline u32
StringLength(char *String)
{
    u32 Count = 0;
    while(*String++)
    {
        ++Count;
    }
    return(Count);
}

#ifdef __cplusplus
}
#endif


#if defined(__cplusplus) && EDITOR_INTERNAL

extern debug_event *DEBUGGlobalEditEvent;

#define DEBUGValueSetEventData_(type)                                   \
    inline void                                                         \
    DEBUGValueSetEventData(debug_event *Event, type Ignored, void *Value) \
    {                                                                   \
        Event->Type = DebugType_##type;                                 \
        if(GlobalDebugTable->EditEvent.GUID == Event->GUID)             \
        {                                                               \
            *(type *)Value = GlobalDebugTable->EditEvent.Value_##type;  \
        }                                                               \
                                                                        \
        Event->Value_##type = *(type *)Value;                           \
    }

DEBUGValueSetEventData_(r32);
DEBUGValueSetEventData_(u32);
DEBUGValueSetEventData_(s32);
DEBUGValueSetEventData_(v2);
DEBUGValueSetEventData_(v3);
DEBUGValueSetEventData_(v4);
DEBUGValueSetEventData_(rectangle2);
DEBUGValueSetEventData_(rectangle3);
DEBUGValueSetEventData_(bitmap_id);
DEBUGValueSetEventData_(sound_id);
DEBUGValueSetEventData_(font_id);
DEBUGValueSetEventData_(memory_arena_p);

struct debug_data_block 
{
    debug_data_block(char *Name)
    {
        RecordDebugEvent(DebugType_OpenDataBlock, Name);
        //Event->DebugID = ID;                                      
    }
    
    ~debug_data_block(void)
    {
        RecordDebugEvent(DebugType_CloseDataBlock, DEBUG_NAME("End Data Block"));
    }
};

#define DEBUG_DATA_BLOCK(Name) debug_data_block DataBlock__(DEBUG_NAME(Name))
#define DEBUG_BEGIN_DATA_BLOCK(Name) RecordDebugEvent(DebugType_OpenDataBlock, DEBUG_NAME(Name))
#define DEBUG_END_DATA_BLOCK(Name) RecordDebugEvent(DebugType_CloseDataBlock, DEBUG_NAME("End Data Block"))

internal void DEBUGEditEventData(char *GUID, debug_event *Event);

#define DEBUG_VALUE(Value)                                          \
    {                                                               \
        RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value));    \
        DEBUGValueSetEventData(Event, Value, (void *)&(Value));     \
    } 
#define DEBUG_NAMED_VALUE(Value)                                    \
    {                                                               \
        RecordDebugEvent(DebugType_Unknown, __FUNCTION__ #Value);   \
        DEBUGValueSetEventData(Event, Value, (void *)&(Value));     \
    } 

#define DEBUG_B32(Value)                                            \
    {                                                               \
        RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value));    \
        DEBUGValueSetEventData(Event, (s32)0, (void *)&Value);      \
        Event->Type = DebugType_b32;                                \
    } 

#define DEBUG_UI_ELEMENT(Type, Name)            \
    {                                           \
        RecordDebugEvent(Type, #Name);          \
    } 

#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)

inline debug_id DEBUG_POINTER_ID(void *Pointer)
{
    debug_id ID = {Pointer};

    return(ID);
}

#define DEBUG_UI_ENABLED 1

internal void DEBUG_HIT(debug_id ID, r32 ZValue);
internal b32 DEBUG_HIGHLIGHTED(debug_id ID, v4 *Color);
internal b32 DEBUG_REQUESTED(debug_id ID);

#else

#define DEBUGSetEventRecording(Enabled)

inline debug_id DEBUG_POINTER_ID(void *Pointer) {debug_id NullID = {}; return(NullID);}

#define DEBUG_DATA_BLOCK(...)
#define DEBUG_VALUE(...)
#define DEBUG_BEGIN_ARRAY(...)
#define DEBUG_END_ARRAY(...)
#define DEBUG_UI_ENABLED 0
#define DEBUG_HIT(...)
#define DEBUG_HIGHLIGHTED(...) 0
#define DEBUG_REQUESTED(...) 0
#define DEBUG_B32(Value)
#endif

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#define EDITOR_PLATFORM_H
#endif

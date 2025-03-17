#if !defined(EDITOR_PLATFORM_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
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
// NOTE(paul): Types
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
    
struct memory_arena
{
    umm Size;
    u8 *Base;
    umm Used;

    umm MinimumBlockSize;
    
    u32 BlockCount;
    s32 TempCount;
};

union v2
{
    struct
    {
        real32 x, y;
    };
    struct
    {
        real32 u, v;
    };
    real32 E[2];
};

union v3
{
    struct
    {
        real32 x, y, z;
    };
    struct
    {
        real32 u, v, w;
    };
    struct
    {
        real32 r, g, b;
    };
    struct
    {
        v2 xy;
        real32 Ignored0_;
    };
    struct
    {
        real32 Ignored1_;
        v2 yz;
    };
    struct
    {
        v2 uv;
        real32 Ignored2_;
    };
    struct
    {
        real32 Ignored3_;
        v2 vw;
    };
    real32 E[3];
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
                real32 x, y, z;
            };
        };
        
        real32 w;        
    };
    struct
    {
        union
        {
            v3 rgb;
            struct
            {
                real32 r, g, b;
            };
        };
        
        real32 a;        
    };
    struct
    {
        v2 xy;
        real32 Ignored0_;
        real32 Ignored1_;
    };
    struct
    {
        real32 Ignored2_;
        v2 yz;
        real32 Ignored3_;
    };
    struct
    {
        real32 Ignored4_;
        real32 Ignored5_;
        v2 zw;
    };
    real32 E[4];
};

struct rectangle2
{
    v2 Min;
    v2 Max;
};

struct rectangle3
{
    v3 Min;
    v3 Max;
};


#include "engine_file_formats.h"
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): COMMON DEFINES
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
// NOTE(paul): MULTITHREADING
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
// NOTE(paul): RENDERING
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
// NOTE(paul): INPUT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct editor_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
} editor_button_state;

typedef struct editor_controller_input
{
    bool32 IsConnected;
    bool32 IsAnalog;    
    real32 StickAverageX;
    real32 StickAverageY;
    
    union
    {
        editor_button_state Buttons[20];
        struct
        {
            editor_button_state MoveUp;
            editor_button_state MoveDown;
            editor_button_state MoveLeft;
            editor_button_state MoveRight;
            
            editor_button_state ActionUp;
            editor_button_state ActionDown;
            editor_button_state ActionLeft;
            editor_button_state ActionRight;

            editor_button_state FirstMode;
            editor_button_state SecondMode;
            editor_button_state ThirdMode;
            editor_button_state Fill;
            
            editor_button_state LeftShoulder;
            editor_button_state RightShoulder;

            editor_button_state Back;
            editor_button_state Start;

            editor_button_state PlayMusic;
            editor_button_state TerminateSound;

            editor_button_state Undo;
            editor_button_state UIEnable;

            // NOTE(casey): All buttons must be added above this line
            
            editor_button_state Terminator;
        };
    };

} editor_controller_input;

enum editor_input_mouse_button
{
    PlatformMouseButton_Left,
    PlatformMouseButton_Middle,
    PlatformMouseButton_Right,
    PlatformMouseButton_Extended0,
    PlatformMouseButton_Extended1,

    PlatformMouseButton_Count,
};

typedef struct editor_input
{
    r32 dtForFrame;

    editor_controller_input Controllers[2];

    // NOTE(casey): Signals back to the platform layer
    b32 QuitRequested;

    // NOTE(casey): For debugging only
    editor_button_state MouseButtons[PlatformMouseButton_Count];
    r32 MouseX, MouseY;
    s16 MouseZ;
    
    b32 ShiftDown, AltDown, ControlDown;
} editor_input;

inline editor_controller_input *
GetController(editor_input *Input, int unsigned ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));
    
    editor_controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}

inline b32
WasPressed(editor_button_state State)
{
    b32 Result = ((State.HalfTransitionCount > 1) ||
                  ((State.HalfTransitionCount == 1) && (State.EndedDown)));

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): DEBUG
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
// NOTE(paul): FILE API
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

typedef enum platform_file_type
{
    PlatformFileType_AssetFile,
    PlatformFileType_SavedEditorFile,
    PlatformFileType_PNG,
    PlatformFileType_BMP,
    PlatformFileType_SSBMP, // Sprite Sheet
    PlatformFileType_TSBMP, // Tile Set
    PlatformFileType_STBMP, // Solid Tile
    PlatformFileType_WAV,
    PlatformFileType_TXT,
    PlatformFileType_JSON,
    PlatformFileType_TTF,
    PlatformFileType_BIN,
    PlatformFileType_SSWM,
    
    PlatformFileType_Count,
} platform_file_type;
    
#define PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(name) platform_file_group name(platform_file_type Type)
typedef PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(platform_get_all_files_of_type_begin);

#define PLATFORM_GET_ALL_FILE_OF_TYPE_END(name) void name(platform_file_group *FileGroup)
typedef PLATFORM_GET_ALL_FILE_OF_TYPE_END(platform_get_all_files_of_type_end);

#define PLATFORM_OPEN_FILE(name) platform_file_handle name(platform_file_group *FileGroup)
typedef PLATFORM_OPEN_FILE(platform_open_next_file);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char *Message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

// NOTE(paul): Dest and Arena can be specified to 0 if you want to know only count
#define PLATFORM_LIST_FILES_IN_DIRECTORY(name) u32 name(platform_file_type Type, char **Dest, memory_arena *Arena)
typedef PLATFORM_LIST_FILES_IN_DIRECTORY(platform_list_files_in_directory);

typedef struct read_file_result
{
    u32 Size;
    void *Contents;
} read_file_result;

#define PLATFORM_FREE_FILE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_FREE_FILE_MEMORY(platform_free_file_memory);

#define PLATFORM_READ_ENTIRE_FILE(name) read_file_result name(char *FileName, platform_file_type Type, memory_arena *Arena)
typedef PLATFORM_READ_ENTIRE_FILE(platform_read_entire_file);

#define PlatformNoFileErrors(Handle) ((Handle)->NoErrors)
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): WORK QUEUES
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
// NOTE(paul): PLATFORM API
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
typedef struct platform_api
{
    platform_add_entry *AddEntry;
    platform_complete_all_work *CompleteAllWork;

    platform_get_all_files_of_type_begin *GetAllFilesOfTypeBegin;
    platform_get_all_files_of_type_end *GetAllFilesOfTypeEnd;
    platform_open_next_file *OpenNextFile;
    platform_read_data_from_file *ReadDataFromFile;
    platform_file_error *FileError;

    platform_list_files_in_directory *ListFilesInDirectory;
    platform_free_file_memory *FreeFileMemory;
    platform_read_entire_file *ReadEntireFile;

    platform_allocate_memory *AllocateMemory;
    platform_deallocate_memory *DeallocateMemory;

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
// NOTE(paul): ENGINE MEMORY
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

#define ENGINE_UPDATE_AND_RENDER(name) void name(engine_memory *Memory, editor_input *Input, editor_render_commands *RenderCommands)
typedef ENGINE_UPDATE_AND_RENDER(engine_update_and_render);
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): DEBUG INTERFACE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

struct debug_table;
#define DEBUG_EDITOR_FRAME_END(name) void name(engine_memory *Memory, editor_input *Input, editor_render_commands *RenderCommands)
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

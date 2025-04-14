#if !defined(SPELLWEAVER_DEBUG_INTERFACE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

struct debug_table;
#define DEBUG_GAME_FRAME_END(name) void name(game_memory *Memory, game_input *Input, game_render_commands *RenderCommands)
typedef DEBUG_GAME_FRAME_END(debug_game_frame_end);

struct debug_id
{
    void *Value[2];
};

#if SPELLWEAVER_INTERNAL
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

#define RecordDebugEvent(EventType, GUIDInit)           \
u64 ArrayIndex_EventIndex = AtomicAddU64(&GlobalDebugTable->EventArrayIndex_EventIndex, GlobalDebugTable->RecordIncrement); \
        u32 EventIndex = ArrayIndex_EventIndex & 0xFFFFFFFF;            \
        Assert(EventIndex < ArrayCount(GlobalDebugTable->Events[0]));   \
        debug_event *Event = GlobalDebugTable->Events[ArrayIndex_EventIndex >> 32] + EventIndex; \
        Event->Clock = __rdtsc();                       \
        Event->Type = (u8)EventType;                                    \
        Event->CoreIndex = 0;                                           \
        Event->ThreadID = (u16)GetThreadID();                         \
        Event->GUID = GUIDInit;

#define FRAME_MARKER(SecondsElapsedInit) \
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


#if defined(__cplusplus) && SPELLWEAVER_INTERNAL

extern debug_event *DEBUGGlobalEditEvent;

#define DEBUGValueSetEventData_(type) \
inline void \
DEBUGValueSetEventData(debug_event *Event, type Ignored, void *Value) \
{ \
    Event->Type = DebugType_##type; \
    if(GlobalDebugTable->EditEvent.GUID == Event->GUID) \
    { \
        *(type *)Value = GlobalDebugTable->EditEvent.Value_##type; \
    } \
    \
    Event->Value_##type = *(type *)Value; \
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

#define DEBUG_VALUE(Value)  \
     { \
         RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value));                              \
         DEBUGValueSetEventData(Event, Value, (void *)&(Value)); \
     } 
#define DEBUG_NAMED_VALUE(Value)  \
{ \
    RecordDebugEvent(DebugType_Unknown, __FUNCTION__ #Value);                              \
    DEBUGValueSetEventData(Event, Value, (void *)&(Value)); \
} 

#define DEBUG_B32(Value)  \
{ \
    RecordDebugEvent(DebugType_Unknown, DEBUG_NAME(#Value));                              \
    DEBUGValueSetEventData(Event, (s32)0, (void *)&Value); \
    Event->Type = DebugType_b32; \
} 

#define DEBUG_UI_ELEMENT(Type, Name) \
{ \
    RecordDebugEvent(Type, #Name);                   \
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

#define SPELLWEAVER_DEBUG_INTERFACE_H
#endif

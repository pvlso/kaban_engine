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

#include "engine_types.h"
#include "engine_defines.h"
#include "engine_intrinsics.h"

#ifdef __cplusplus
extern "C" {
#endif

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
        engine_button_state Buttons[19];
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
            
            engine_button_state RightShoulder;

            engine_button_state Back;
            engine_button_state Start;

            engine_button_state Undo;
            engine_button_state ShowUI;
            engine_button_state ToggleMute;

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
// - None: use the filename exactly as provided (no prefix/pattern), relative to the
//   executable directory. Always use '/' as the separator, the platform converts it.
// - Other types: map to a directory or wildcard used during file lookup.
// - TXT/JSON: treated as text files; loader appends a null terminator.
typedef enum platform_file_type
{
    PlatformFileType_None,

    PlatformFileType_KEA,
    PlatformFileType_KESA,
    PlatformFileType_KEWM,
    PlatformFileType_KET,

    PlatformFileType_BMP,
    PlatformFileType_SSBMP, // Sprite Sheet
    PlatformFileType_TSBMP, // Tile Set
    PlatformFileType_STBMP, // Solid Tile
    PlatformFileType_WAV,
    PlatformFileType_TXT,
    PlatformFileType_JSON,
    PlatformFileType_TTF,
    PlatformFileType_BIN,

    // NOTE(pvlso): Relative to the projects directory (next to data/), e.g. "game_name/game_name.cpp"
    PlatformFileType_Project,
    
    PlatformFileType_Count,
} platform_file_type;

typedef enum platform_file_op
{
    PlatformFileOp_Read,
    PlatformFileOp_Write,
    PlatformFileOp_WriteExisting,

} platform_file_op;

typedef enum platform_file_seek_op
{
    PlatformFileSeek_Set,
    PlatformFileSeek_Current,
    PlatformFileSeek_End,

} platform_file_seek_op;

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

#define PLATFORM_SEEK_FILE(name) void name(platform_file_handle *Handle, u64 Offset, platform_file_seek_op Op)
typedef PLATFORM_SEEK_FILE(platform_seek_file);

#define PLATFORM_FILE_TELL(name) u64 name(platform_file_handle *Handle)
typedef PLATFORM_FILE_TELL(platform_file_tell);

#define PLATFORM_READ_DATA_FROM_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Dest)
typedef PLATFORM_READ_DATA_FROM_FILE(platform_read_data_from_file);

#define PLATFORM_WRITE_DATA_TO_FILE(name) void name(platform_file_handle *Source, u64 Offset, u64 Size, void *Data)
typedef PLATFORM_WRITE_DATA_TO_FILE(platform_write_data_to_file);

#define PLATFORM_FILE_ERROR(name) void name(platform_file_handle *Handle, char *Message)
typedef PLATFORM_FILE_ERROR(platform_file_error);

// NOTE(pvlso): Dest and Arena can be specified to 0 if you want to know only count
#define PLATFORM_LIST_FILES_IN_DIRECTORY(name) u32 name(platform_file_type Type, char **Dest, memory_arena *Arena)
typedef PLATFORM_LIST_FILES_IN_DIRECTORY(platform_list_files_in_directory);

// NOTE(pvlso): Names of the directories in projects/ that hold a project, which is
// game_name/game_name.cpp. Dest and Arena can be 0 to only count, otherwise at most DestCount are stored.
#define PLATFORM_LIST_PROJECTS(name) u32 name(char **Dest, u32 DestCount, memory_arena *Arena)
typedef PLATFORM_LIST_PROJECTS(platform_list_projects);

// NOTE(pvlso): Path is relative to the Type's directory. False if it already exists or can't be created.
#define PLATFORM_MAKE_DIRECTORY(name) b32 name(char *Path, platform_file_type Type)
typedef PLATFORM_MAKE_DIRECTORY(platform_make_directory);

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

typedef struct platform_loaded_code
{
    b32 NoErrors;
    void *Platform;
} platform_loaded_code;

// NOTE(pvlso): ModuleName is the bare name of the library ("arkham"), the platform layer
// adds the OS specific prefix/extension and looks for it next to the executable.
#define PLATFORM_LOAD_CODE(name) platform_loaded_code name(char *ModuleName)
typedef PLATFORM_LOAD_CODE(platform_load_code);

#define PLATFORM_UNLOAD_CODE(name) void name(platform_loaded_code *Code)
typedef PLATFORM_UNLOAD_CODE(platform_unload_code);

#define PLATFORM_GET_PROC_ADDRESS(name) void *name(platform_loaded_code *Code, char *FunctionName)
typedef PLATFORM_GET_PROC_ADDRESS(platform_get_proc_address);

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

#define PLATFORM_REALLOCATE_MEMORY(name) void *name(void *Source, u32 InitSize, u32 Size)
typedef PLATFORM_REALLOCATE_MEMORY(platform_reallocate_memory);
    
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
// NOTE(pvlso): UI
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define UI_BASE_RESOLUTION_X 1920
#define UI_BASE_RESOLUTION_Y 1080

#define PLATFORM_SLEEP(name) void name(s32 Time)
typedef PLATFORM_SLEEP(platform_sleep);

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
    platform_list_projects *ListProjects;
    platform_make_directory *MakeDirectory;
    platform_free_file_memory *FreeFileMemory;
    platform_read_entire_file *ReadEntireFile;
    platform_write_entire_file *WriteEntireFile;

    platform_seek_file *FileSeek;
    platform_file_tell *FileTell;

    platform_allocate_memory *AllocateMemory;
    platform_deallocate_memory *DeallocateMemory;
    platform_reallocate_memory *ReallocateMemory;
    
    platform_load_code *LoadCode;
    platform_unload_code *UnloadCode;
    platform_get_proc_address *GetProcAddress;
    platform_sleep *Sleep;

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

#define ENGINE_UPDATE_AND_RENDER(name) void name(struct nk_context *nk, v2 UIScale, engine_memory *Memory, engine_input *Input, editor_render_commands *RenderCommands)
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

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif

#include "engine_debug_interface.h"

#define EDITOR_PLATFORM_H
#endif

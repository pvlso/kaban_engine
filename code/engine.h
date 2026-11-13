#if !defined(EDITOR_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): engine_api.h is shared with the games, this hides its game side wrappers
#define ENGINE_BUILD 1

#include "engine_platform.h"
#include "engine_file_formats.h"
#include "engine_config.h"
#include "engine_shared.h"
#include "engine_api.h"

#define DLIST_INSERT(Sentinel, Element)         \
    (Element)->Next = (Sentinel)->Next;         \
    (Element)->Prev = (Sentinel);               \
    (Element)->Next->Prev = (Element);          \
    (Element)->Prev->Next = (Element); 

#define DLIST_REMOVE(Element)                   \
    (Element)->Prev->Next = (Element)->Next;    \
    (Element)->Next->Prev = (Element)->Prev;

#define DLIST_INSERT_AS_LAST(Sentinel, Element) \
    (Element)->Next = (Sentinel);               \
    (Element)->Prev = (Sentinel)->Prev;         \
    (Element)->Next->Prev = (Element);          \
    (Element)->Prev->Next = (Element); 

#define DLIST_INIT(Sentinel) \
    (Sentinel)->Next = (Sentinel); \
    (Sentinel)->Prev = (Sentinel);

#define POLY_FREELIST_ALLOCATE(Result, FreeListPointer, AllocationCode) \
    (Result) = (FreeListPointer);                                       \
    if(Result) {                                                        \
        FreeListPointer = (Result)->Next;                               \
        (Result)->Next = 0;                                             \
    } else {                                                            \
        Result = AllocationCode;                                        \
    }

#define POLY_FREELIST_DEALLOCATE(Pointer, FreeListPointer)  \
    if(Pointer) {                                           \
        ZeroStruct(*(Pointer));                             \
        (Pointer)->Next = (FreeListPointer);                \
        (FreeListPointer) = (Pointer);                      \
    }

#define FREELIST_ALLOCATE(Result, FreeListPointer, AllocationCode)      \
    (Result) = (FreeListPointer);                                       \
    if(Result) {FreeListPointer = (Result)->NextFree;} else {Result = AllocationCode;}
#define FREELIST_DEALLOCATE(Pointer, FreeListPointer)                   \
    if(Pointer) {(Pointer)->NextFree = (FreeListPointer); (FreeListPointer) = (Pointer);}

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

#define BITMAP_BYTES_PER_PIXEL 4

struct task_with_memory
{
    b32 BeingUsed;
    memory_arena Arena;

    temporary_memory MemoryFlush;
};

#include "engine_render.h"
#include "engine_render_group.h"
#include "engine_map_file.h"
#include "engine_map_nav.h"
#include "engine_asset.h"
#include "engine_font.h"
#include "editor_audio.h"
#include "engine_json_parser.h"
#include "engine_ui.h"
#include "engine_hash.h"
#include "engine_clipper.h"

#include "editor_title_mode.h"
#include "editor_assets_mode.h"
#include "editor_entity_mode.h"
#include "editor_map_mode.h"
#include "editor_game_mode.h"

//#include "engine_navigation_mesh.h"
//#include "engine_map_editor_mode.h"

enum editor_mode
{
    EditorMode_None,
    EditorMode_TitleScreen,
    EditorMode_AssetsMode,
    EditorMode_MapEditor,
    EditorMode_Game,
    EditorMode_EntityEditor,
};

struct editor_meta
{
    u8 KESAVersion[4];
};

// NOTE(pvlso): A KEA build running on a worker thread (StartKEABuild). The worker owns everything below until Running
// goes false; Result and Built say how it went.
struct kea_build_job
{
    volatile b32 Running;
    b32 Result;
    char Built[64];      // NOTE(pvlso): the new KEA's name
    char KeepOpen[256];  // NOTE(pvlso): the KEA a running game reads, not deleted
    struct editor_state *EditorState;
    platform_work_queue *Queue;
    memory_arena Arena;
};

// NOTE(pvlso): The game library the last successful build made this session: Run and Play load it as it is when the
// project's sources haven't changed since (no build.bat)
struct game_code_memo
{
    char ProjectName[PROJECT_NAME_COUNT];
    u64 SourceWriteTime;
};

struct game_state;
struct editor_state
{
    memory_arena TotalArena;
    memory_arena ModeArena;

    memory_arena AudioArena; // TODO(casey): Move this into the audio system proper!
    audio_state AudioState;

    pcg32_random_t Rng;

    b32 Play;
    playing_sound *Sound;

    editor_meta EditorMeta;
    
    b32 UIEnable;
    ui_state UIState;

    char window_title[64];
    editor_mode EditorMode;

    union
    {
        editor_mode_title_screen *TitleScreen;
        editor_mode_assets *AssetsMode;
        editor_mode_map *MapMode;
        editor_mode_entity *EntityMode;
        editor_mode_game *Game;
//        engine_map_editor *MapEditor;
//        editor_game_simulate_mode *SimulateGame;
    };

    // NOTE(pvlso): Play from the map editor (map editor plan, phase 7): the map editor stays in the mode arena while
    // the game runs, the game mode lives in PlayArena; Stop (or F5) clears PlayArena and the map editor goes on where
    // it was, its undo history and all. 0 when the game was started by Run.
    editor_mode_map *PausedMap;
    memory_arena PlayArena;

    kea_build_job KEABuild;
    game_code_memo LastGameBuild;
};

struct transient_state
{
    memory_arena TranArena;    

    memory_arena AssetsArena;

    task_with_memory Tasks[4];

    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    platform_texture_op_queue *TextureOpQueue;
};

internal task_with_memory *BeginTaskWithMemory(u32 TaskCount, task_with_memory *Tasks);
internal void EndTaskWithMemory(task_with_memory *Task);
internal void SetEditorMode(editor_state *EditorState, transient_state *TranState, editor_mode EditorMode);

#define EDITOR_H
#endif

#if !defined(EDITOR_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#include "engine_platform.h"
#include "engine_config.h"
#include "engine_shared.h"

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
#include "engine_asset.h"
#include "editor_audio.h"
#include "engine_json_parser.h"
#include "engine_ui.h"
#include "engine_hash.h"

#include "editor_title_mode.h"
#include "editor_assets_mode.h"
#include "arkham/arkham.h"

//#include "engine_api.h"
//#include "engine_navigation_mesh.h"
//#include "engine_map_editor_mode.h"

enum editor_mode
{
    EditorMode_None,
    EditorMode_TitleScreen,
    EditorMode_AssetsMode,
    EditorMode_MapEditor,
    EditorMode_Arkham,
};

struct editor_meta
{
    u8 KESAVersion[4];
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

//    engine_api EngineAPI;
//    platform_loaded_code ArkhamCode;
//    arkham_update_and_render *Arkham;
    union
    {
        editor_mode_title_screen *TitleScreen;
        editor_mode_assets *AssetsMode;
        game_state *ArkhamGameState;
//        engine_map_editor *MapEditor;
//        editor_game_simulate_mode *SimulateGame;
    };
};

struct transient_state
{
    memory_arena TranArena;    

    memory_arena AssetsArena;
    engine_assets *ArkhamAssets;
    
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

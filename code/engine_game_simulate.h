#if !defined(ENGINE_GAME_SIMULATE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#include "spellweaver\spellweaver_cutscene.h"

#include "spellweaver\spellweaver_render.h"

struct text_config
{
    loaded_font *Font;
    ssa_font *FontInfo;

    object_transform TextTransform;
    object_transform TextShadowTransform;
    
    v2 P;
    r32 FontScale;

    v4 Color;
    r32 AtZ;
};

enum text_op
{
    TextOp_DrawText,
    TextOp_SizeText,
};

#include "spellweaver\spellweaver_world_mode.h"
#include "spellweaver\spellweaver_title_mode.h"

struct controlled_hero
{
    entity_id EntityIndex;
    // NOTE(casey): These are the controller requests for simulation
    v2 ddP;

    b32 Attack;
    b32 Action;

    b32 Move;
    b32 SetObstacle;

    sphere_type SphereNewType;
    b32 InvokeAndCastSpell;
};

struct hero_bitmap_ids
{
    bitmap_id Head;
    bitmap_id Cape;
    bitmap_id Torso;
};

enum game_mode
{
    GameMode_None,
    
    GameMode_TitleScreen,
    GameMode_CutScene,
    GameMode_World,
};

enum music_state
{
    MusicState_Ambient,
    MusicState_DarkAmbient,
    MusicState_Action,

    MusicState_Count,
};

enum fade_state
{
    FadeState_None,
    FadeState_FadeIn,
    FadeState_FadeOut,
};

struct game_state
{
    bool32 IsInitialized;

    memory_arena ModeArena;
    memory_arena AudioArena; // TODO(casey): Move this into the audio system proper!

    controlled_hero ControlledHeroes[ArrayCount(((engine_input *)0)->Controllers)];

    audio_state *AudioState;
    
    b32 GameHaveStarted;
    bitmap_id CursorBitmapHover;
    bitmap_id CursorBitmapClick;

    r32 CurrentAlpha;
    fade_state FadeState;
    
    random_series MusicEntropy;
    b32 ChangeMusic;
    s32 MusicState;
    sound_id GameStartFX;
    sound_id GameEndDeathFX;
    sound_id AmbientMusic[15];
    sound_id DarkAmbientMusic[5];
    sound_id ActionMusic[5];

    b32 MusicIsPlaying;
    playing_sound *Music;
    
    game_mode GameMode;
    union
    {
        game_mode_title_screen *TitleScreen;
        game_mode_cutscene *CutScene;
        world_state *WorldState;
    };
};

struct game_task_with_memory
{
    b32 BeingUsed;
    b32 DependsOnGameMode;
    memory_arena Arena;

    temporary_memory MemoryFlush;
};

struct game_transient_state
{
    bool32 IsInitialized;
    memory_arena TranArena;    

    game_task_with_memory Tasks[4];

    editor_assets *Assets;
    u32 MainGenerationID;

    loaded_bitmap MiniMap;
    
    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;
};

internal game_task_with_memory *BeginTaskWithMemory(game_transient_state *TranState, b32 DependsOnGameMode);
internal void EndTaskWithMemory(game_task_with_memory *Task);
internal void SetGameMode(game_state *GameState, game_transient_state *TranState, game_mode GameMode);

struct editor_game_simulate_mode
{
    memory_arena GameArena;
    memory_arena GameTranArena;
};

internal void PlaySimulation(editor_state *EditorState, transient_state *TranState);

#define ENGINE_GAME_SIMULATE_H
#endif

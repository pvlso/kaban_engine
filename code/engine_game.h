#if !defined(ENGINE_GAME_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

//#include "spellweaver_platform.h"
//#include "spellweaver_config.h"
#include "spellweaver\spellweaver_shared.h"
#include "spellweaver\spellweaver_cutscene.h"

#define DLIST_INSERT(Sentinel, Element)         \
    (Element)->Next = (Sentinel)->Next;         \
    (Element)->Prev = (Sentinel);               \
    (Element)->Next->Prev = (Element);          \
    (Element)->Prev->Next = (Element); 
#define DLIST_INSERT_AS_LAST(Sentinel, Element)         \
    (Element)->Next = (Sentinel);               \
    (Element)->Prev = (Sentinel)->Prev;         \
    (Element)->Next->Prev = (Element);          \
    (Element)->Prev->Next = (Element); 

#define DLIST_INIT(Sentinel) \
    (Sentinel)->Next = (Sentinel); \
    (Sentinel)->Prev = (Sentinel);

#define FREELIST_ALLOCATE(Result, FreeListPointer, AllocationCode)             \
    (Result) = (FreeListPointer); \
    if(Result) {FreeListPointer = (Result)->NextFree;} else {Result = AllocationCode;}
#define FREELIST_DEALLOCATE(Pointer, FreeListPointer) \
    if(Pointer) {(Pointer)->NextFree = (FreeListPointer); (FreeListPointer) = (Pointer);}

#include "spellweaver\spellweaver_world.h"
#include "spellweaver\spellweaver_entity.h"
#include "spellweaver\spellweaver_sim_region.h"

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
//#include "a_star_test.h"

struct controlled_hero
{
    entity_id EntityIndex;
    // NOTE(casey): These are the controller requests for simulation
    v2 ddP;

    b32 Attack;
    b32 Action;

    b32 Move;

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
    GameMode_Test,
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
    sound_id AmbientMusic[2];
    sound_id DarkAmbientMusic[5];
    sound_id ActionMusic[5];

    b32 MusicIsPlaying;
    playing_sound *Music;
    
    game_mode GameMode;
    union
    {
        game_mode_title_screen *TitleScreen;
        game_mode_cutscene *CutScene;
        game_mode_world *WorldMode;
//        game_mode_a_star_test *AStarTest;
    };
};


struct game_transient_state
{
    bool32 IsInitialized;
    memory_arena TranArena;    

    editor_assets *Assets;
    u32 MainGenerationID;

    b32 WorldTilesInitialized;
    tileset_id GlobalTilesetID;

    loaded_bitmap MiniMap;
};

internal void SetGameMode(game_state *GameState, transient_state *TranState, game_mode GameMode);

#define ENGINE_GAME_H
#endif

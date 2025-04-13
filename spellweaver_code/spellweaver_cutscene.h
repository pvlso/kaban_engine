#if !defined(SPELLWEAVER_CUTSCENE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

/*
  NOTE(casey):

  - Snow for shot 2 (and 1?)
  - Closed door in shot 1
  - Handedness of glove?
  
 */

enum scene_layer_flags
{
    SceneLayerFlag_AtInfinty = 0x1,
    SceneLayerFlag_CounterCameraX = 0x2,
    SceneLayerFlag_CounterCameraY = 0x4,
    SceneLayerFlag_Transient = 0x8,
    SceneLayerFlag_Floaty = 0x10,
};

struct scene_layer
{
    v3 P;
    r32 Height;
    u32 Flags;
    v2 Param;
};

struct layered_scene
{
    asset_type_id AssetType;
    u32 ShotIndex;
    u32 LayerCount;
    scene_layer *Layers;

    r32 Duration;
    v3 CameraStart;
    v3 CameraEnd;

    r32 tFadeIn;
};

// NOTE(casey): Thank you to Ron Gilbert and friends for
// introducing the word "cutscene" into the game development
// vocabulary.
enum cutscene_id
{
    CutsceneID_Intro,
};
struct game_mode_cutscene
{
    cutscene_id ID;
    r32 t;
};

struct game_state;
struct transient_state;
internal void PlayIntroCutscene(game_state *GameState, transient_state *TranState);
internal void PlayTitleScreen(game_state *GameState, transient_state *TranState);

#define SPELLWEAVER_CUTSCENE_H
#endif

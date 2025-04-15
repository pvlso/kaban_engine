/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

#define CUTSCENE_WARMUP_SECONDS 2.0f

internal void
PlayIntroCutscene(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_CutScene);
        
    game_mode_cutscene *Result = PushStruct(&GameState->ModeArena, game_mode_cutscene);

    Result->ID = CutsceneID_Intro;
    Result->t = 0;

    GameState->CutScene = Result;
}

#if 0
internal void
RenderLayeredScene(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
                   layered_scene *Scene, r32 tNormal)
{
    // TODO(casey): Unify this stuff?
    real32 WidthOfMonitor = 0.635f; // NOTE(casey): Horizontal measurement of monitor in meters
    real32 MetersToPixels = (real32)DrawBuffer->Width*WidthOfMonitor;
    real32 FocalLength = 0.6f;

    r32 SceneFadeValue = 1.0f;
    if(tNormal < Scene->tFadeIn)
    {
        SceneFadeValue = Clamp01MapToRange(0.0f, tNormal, Scene->tFadeIn);
    }

    // TODO(casey): Why does this fade seem to be wrong?  It appears nonlinear, but
    // if it is in linear brightness, shouldn't it _appear_ linear?
    v4 Color = {SceneFadeValue, SceneFadeValue, SceneFadeValue, 1.0f};

    v3 CameraStart = Scene->CameraStart;
    v3 CameraEnd = Scene->CameraEnd;
    v3 CameraOffset = Lerp(CameraStart, tNormal, CameraEnd);
    if(RenderGroup)
    {
        Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, 0.0f);
    }

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    WeightVector.E[Tag_ShotIndex] = 10.0f;
    WeightVector.E[Tag_LayerIndex] = 1.0f;
    
    MatchVector.E[Tag_ShotIndex] = (r32)Scene->ShotIndex;

    if(Scene->LayerCount == 0)
    {
        Clear(RenderGroup, V4(0.0f, 0.0f, 0.0f, 0.0f));
    }
    
    for(u32 LayerIndex = 1;
        LayerIndex <= Scene->LayerCount;
        ++LayerIndex)
    {
        scene_layer Layer = Scene->Layers[LayerIndex - 1];
        b32 Active = true;
        if(Layer.Flags & SceneLayerFlag_Transient)
        {
            Active = ((tNormal >= Layer.Param.x) &&
                      (tNormal < Layer.Param.y));
        }

        if(Active)
        {
            MatchVector.E[Tag_LayerIndex] = (r32)LayerIndex;        
            bitmap_id LayerImage = GetBestMatchBitmapFrom(Assets, Scene->AssetType, &MatchVector, &WeightVector);

            if(RenderGroup)
            {
                object_transform Transform = DefaultFlatTransform();
                
                v3 P = Layer.P;
                if(Layer.Flags & SceneLayerFlag_AtInfinty)
                {
                    P.z += CameraOffset.z;
                }

                if(Layer.Flags & SceneLayerFlag_Floaty)
                {
                    P.y += Layer.Param.x*Sin(Layer.Param.y*tNormal);
                }
            
                if(Layer.Flags & SceneLayerFlag_CounterCameraX)
                {
                    Transform.OffsetP.x = P.x + CameraOffset.x;
                }
                else
                {
                    Transform.OffsetP.x = P.x - CameraOffset.x;
                }

                if(Layer.Flags & SceneLayerFlag_CounterCameraY)
                {
                    Transform.OffsetP.y = P.y + CameraOffset.y;
                }
                else
                {
                    Transform.OffsetP.y = P.y - CameraOffset.y;
                }

                Transform.OffsetP.z = P.z - CameraOffset.z;
        
                PushBitmap(RenderGroup, Transform, LayerImage, Layer.Height, V3(0, 0, 0), Color);
            }
            else
            {
                PrefetchBitmap(Assets, LayerImage);
            }
        }
    }
}

global_variable scene_layer IntroLayers1[] =
{
    {{0.0f, 0.0f, -200.0f}, 300.0f, SceneLayerFlag_AtInfinty}, // NOTE(casey): Sky background
    {{0.0f, 0.0f, -170.0f}, 300.0f}, // NOTE(casey): Weird sky light
    {{0.0f, 0.0f, -100.0f}, 40.0f}, // NOTE(casey): Backmost row of trees
    {{0.0f, 10.0f, -70.0f}, 80.0f}, // NOTE(casey): Middle hills and trees
    {{0.0f, 0.0f, -50.0f}, 70.0f}, // NOTE(casey): Front hills and trees 
    {{30.0f, 0.0f, -30.0f}, 50.0f}, // NOTE(casey): Right side tree and fence
    {{0.0f, -2.0f, -20.0f}, 40.0f}, // NOTE(casey): 7
    {{2.0f, -1.0f, -5.0f}, 25.0f}, // NOTE(casey): 8
};

global_variable scene_layer IntroLayers2[] =
{
    {{3.0f, -4.0f, -62.0f}, 102.0f}, // NOTE(casey): Hero and tree
    {{0.0f, 0.0f, -14.0f}, 22.0f}, // NOTE(casey): Wall and window
    {{0.0f, 2.0f, -8.0f}, 10.0f}, // NOTE(casey): Icicles
};

global_variable scene_layer IntroLayers3[] =
{
    {{0.0f, 0.0f, -30.0f}, 100.0f, SceneLayerFlag_AtInfinty}, // NOTE(casey): Sky
    {{0.0f, 0.0f, -20.0f}, 45.0f, SceneLayerFlag_CounterCameraY}, // NOTE(casey): Wall and window
    {{0.0f, -2.0f, -4.0f}, 15.0f, SceneLayerFlag_CounterCameraY}, // NOTE(casey): Icicles
    {{0.0f, 0.35f, -0.5f}, 1.0f}, // NOTE(casey): Icicles
};

global_variable scene_layer IntroLayers4[] =
{
    {{0.0f, 0.0f, -4.1f}, 6.0f},
    {{-1.2f, -0.2f, -4.0f}, 4.0f, SceneLayerFlag_Transient, {0.0f, 0.5f}},
    {{-1.2f, -0.2f, -4.0f}, 4.0f, SceneLayerFlag_Transient, {0.5f, 1.0f}},
    {{2.25f, -1.5f, -3.0f}, 2.0f},
    {{0.0f, 0.35f, -1.0f}, 1.0f},
};

global_variable scene_layer IntroLayers5[] =
{
    {{0.0f, 0.0f, -20.0f}, 30.0f},
    {{0.0f, 0.0f, -5.0f}, 8.0f, SceneLayerFlag_Transient, {0.0f, 0.5f}},
    {{0.0f, 0.0f, -5.0f}, 8.0f, SceneLayerFlag_Transient, {0.5f, 1.0f}},
    {{0.0f, 0.0f, -3.0f}, 4.0f, SceneLayerFlag_Transient, {0.5f, 1.0f}},
    {{0.0f, 0.0f, -2.0f}, 3.0f, SceneLayerFlag_Transient, {0.5f, 1.0f}},
};

global_variable scene_layer IntroLayers6[] =
{
    {{0.0f, 0.0f, -8.0f}, 12.0f},
    {{0.0f, 0.0f, -5.0f}, 8.0f},
    {{1.0f, -1.0f, -3.0f}, 3.0f},
    {{0.85f, -0.95f, -3.0f}, 0.5f},
    {{-2.0f, -1.0f, -2.5f}, 2.0f},
    {{0.2f, 0.5f, -1.0f}, 1.0f},
};

global_variable scene_layer IntroLayers7[] =
{
    {{-0.5f, 0.0f, -8.0f}, 12.0f, SceneLayerFlag_CounterCameraX},
    {{-1.0f, 0.0f, -4.0f}, 6.0f},
};

global_variable scene_layer IntroLayers8[] =
{
    {{0.0f, 0.0f, -8.0f}, 12.0f},
    {{0.0f, -1.0f, -5.0f}, 4.0f, SceneLayerFlag_Floaty, {0.05f, 15.0f}},
    {{3.0f, -1.5f, -3.0f}, 2.0f},
    {{0.0f, 0.0f, -1.5f}, 2.5f},
};

global_variable scene_layer IntroLayers9[] =
{
    {{0.0f, 0.0f, -8.0f}, 12.0f},
    {{0.0f, 0.25f, -3.0f}, 4.0f},
    {{1.0f, 0.0f, -2.0f}, 3.0f},
    {{1.0f, 0.1f, -1.0f}, 2.0f},
};

global_variable scene_layer IntroLayers10[] =
{
    {{-15.0f, 25.0f, -100.0f}, 130.0f, SceneLayerFlag_AtInfinty},
    {{0.0f, 0.0f, -10.0f}, 22.0f},
    {{-0.8f, -0.2f, -3.0f}, 4.5f},
    {{0.0f, 0.0f, -2.0f}, 4.5f},
    {{0.0f, -0.25f, -1.0f}, 1.5f},
    {{0.2f, 0.2f, -0.5f}, 1.0f},
};

global_variable scene_layer IntroLayers11[] =
{
    {{0.0f, 0.0f, -100.0f}, 150.0f, SceneLayerFlag_AtInfinty},
    {{0.0f, 10.0f, -40.0f}, 40.0f},
    {{0.0f, 3.2f, -20.0f}, 23.0f},
    {{0.25f, 0.9f, -10.0f}, 13.5f},
    {{-0.5f, 0.625f, -5.0f}, 7.0f},
    {{0.0f, 0.1f, -2.5f}, 3.9f},
    {{-0.3f, -0.15f, -1.0f}, 1.2f},
};

#define INTRO_SHOT(Index) Asset_OpeningCutscene, Index, ArrayCount(IntroLayers##Index), IntroLayers##Index
global_variable layered_scene IntroCutscene[] =
{
    {Asset_None, 0, 0, 0, CUTSCENE_WARMUP_SECONDS},
    {INTRO_SHOT(1), 20.0f, {0.0f, 0.0f, 10.0f}, {-4.0f, -2.0f, 5.0f}, 0.5f},
    {INTRO_SHOT(2), 20.0f, {0.0f, 0.0f, 0.0f}, {2.0f, -2.0f, -4.0f}},
    {INTRO_SHOT(3), 20.0f, {0.0f, 0.5f, 0.0f}, {0.0f, 6.5f, -1.5f}},
    {INTRO_SHOT(4), 20.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -0.5f}},
    {INTRO_SHOT(5), 20.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.5f, -1.0f}},
    {INTRO_SHOT(6), 20.0f, {0.0f, 0.0f, 0.0f}, {-0.5f, 0.5f, -1.0f}},
    {INTRO_SHOT(7), 20.0f, {0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}},
    {INTRO_SHOT(8), 20.0f, {0.0f, 0.0f, 0.0f}, {0.0f, -0.5f, -1.0f}},
    {INTRO_SHOT(9), 20.0f, {0.0f, 0.0f, 0.0f}, {-0.75f, -0.5f, -1.0f}},
    {INTRO_SHOT(10), 20.0f, {0.0f, 0.0f, 0.0f}, {-0.1f, 0.05f, -0.5f}},
    {INTRO_SHOT(11), 20.0f, {0.0f, 0.0f, 0.0f}, {0.6f, 0.5f, -2.0f}},
};

struct cutscene
{
    u32 SceneCount;
    layered_scene *Scenes;
};
global_variable cutscene Cutscenes[] =
{
    {ArrayCount(IntroCutscene), IntroCutscene},
};

internal b32
RenderCutsceneAtTime(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
                     game_mode_cutscene *Cutscene, r32 tCutScene)
{
    b32 CutsceneStillRunning = false;
    
    cutscene Info = Cutscenes[Cutscene->ID];
    r32 tBase = 0.0f;
    for(u32 ShotIndex = 0;
        ShotIndex < Info.SceneCount;
        ++ShotIndex)
    {
        layered_scene *Scene = Info.Scenes + ShotIndex;
        r32 tStart = tBase;
        r32 tEnd = tStart + Scene->Duration;

        if((tCutScene >= tStart) &&
           (tCutScene < tEnd))
        {
            r32 tNormal = Clamp01MapToRange(tStart, tCutScene, tEnd);
            RenderLayeredScene(Assets, RenderGroup, DrawBuffer, &IntroCutscene[ShotIndex], tNormal);
            CutsceneStillRunning = true;
        }

        tBase = tEnd;
    }

    return(CutsceneStillRunning);
}
#endif

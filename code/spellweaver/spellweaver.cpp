/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
#include "spellweaver.h"
#include "spellweaver_sort.cpp"
#include "spellweaver_render_group.cpp"
#include "spellweaver_asset.cpp"
#include "spellweaver_text.cpp"
#include "spellweaver_audio.cpp"
#include "spellweaver_world.cpp"
#include "spellweaver_sim_region.cpp"
#include "spellweaver_entity.cpp"

internal loaded_bitmap
MakeEmptyBitmap(memory_arena *Arena, int32 Width, int32 Height, bool32 ClearToZero = true)
{
    loaded_bitmap Result = {};

    Result.AlignPercentage = V2(0.5f, 0.5f);
    Result.WidthOverHeight = SafeRatio1((r32)Width, (r32)Height);

    Result.Width = Width;
    Result.Height = Height;
    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
    int32 TotalBitmapSize = Width*Height*BITMAP_BYTES_PER_PIXEL;
    Result.Memory = PushSize(Arena, TotalBitmapSize, Align(16, ClearToZero));

    return(Result);
}

inline void
ChangeBackgroundMusic(game_state *GameState, s32 NewMusicState)
{
    GameState->MusicState = NewMusicState;
    GameState->ChangeMusic = true;
}

internal void
PlayBackGroundMusic(game_state *GameState)
{
    if(!GameState->MusicIsPlaying)
    {
        u32 RandomNumber = 0;
        sound_id Music = {};
        switch(GameState->MusicState)
        {
            case MusicState_Ambient:
            {
                RandomNumber = RandomBetween(&GameState->MusicEntropy, 0, ArrayCount(GameState->AmbientMusic) - 1);
                Music = GameState->AmbientMusic[RandomNumber];
            } break;

            case MusicState_DarkAmbient:
            {
                RandomNumber = RandomBetween(&GameState->MusicEntropy, 0, ArrayCount(GameState->DarkAmbientMusic) - 1);
                Music = GameState->DarkAmbientMusic[RandomNumber];
            } break;

            case MusicState_Action:
            {
                RandomNumber = RandomBetween(&GameState->MusicEntropy, 0, ArrayCount(GameState->ActionMusic) - 1);
                Music = GameState->ActionMusic[RandomNumber];
            } break;
        }

        GameState->Music = PlaySound(&GameState->AudioState, Music);
        GameState->MusicIsPlaying = true;
    }
    else
    {
        if(!GameState->Music->SoundIsPlaying)
        {
            GameState->MusicIsPlaying = false;
        }

        if(GameState->ChangeMusic)
        {
            MuteAndTerminateSound(&GameState->AudioState, GameState->Music, 1.0f);
            if(GameState->Music->Terminated)
            {
                GameState->ChangeMusic = false;                
            }
        }
    }
}

internal b32
CheckForMetaInput(game_state *GameState, transient_state *TranState, game_input *Input)
{
    b32 Result = false;
#if SPELLWEAVER_INTERNAL
    for(u32 ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        if(WasPressed(Controller->Back))
        {
            Input->QuitRequested = true;
            break;
        }
        else if(WasPressed(Controller->Start))
        {
            GameState->GameHaveStarted = true;
            PlayWorld(GameState, TranState);
            Result = true;
            break;
        }
    }
#endif

    return(Result);
}

#include "spellweaver_world_mode.cpp"
#include "spellweaver_title_mode.cpp"
#include "spellweaver_cutscene.cpp"

internal task_with_memory *
BeginTaskWithMemory(transient_state *TranState, b32 DependsOnGameMode)
{
    task_with_memory *FoundTask = 0;

    for(uint32 TaskIndex = 0;
        TaskIndex < ArrayCount(TranState->Tasks);
        ++TaskIndex)
    {
        task_with_memory *Task = TranState->Tasks + TaskIndex;
        if(!Task->BeingUsed)
        {
            FoundTask = Task;
            Task->BeingUsed = true;
            Task->DependsOnGameMode = DependsOnGameMode;
            Task->MemoryFlush = BeginTemporaryMemory(&Task->Arena);
            break;
        }
    }

    return(FoundTask);
}

internal void
EndTaskWithMemory(task_with_memory *Task)
{
    EndTemporaryMemory(Task->MemoryFlush);

    CompletePreviousWritesBeforeFutureWrites;
    Task->BeingUsed = false;
}

#if SPELLWEAVER_INTERNAL
internal u32
DEBUGGetMainGenerationID(game_memory *Memory)
{
    u32 Result = 0;
    
    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(TranState->IsInitialized)
    {
        Result = TranState->MainGenerationID;
    }

    return(Result);
}

internal game_assets *
DEBUGGetGameAssets(game_memory *Memory)
{
    game_assets *Assets = 0;
    
    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(TranState->IsInitialized)
    {
        Assets = TranState->Assets;
    }

    return(Assets);
}
#endif

internal void
SetGameMode(game_state *GameState, transient_state *TranState, game_mode GameMode)
{
    b32 NeedToWait = false;
    for(u32 TaskIndex = 0;
        TaskIndex < ArrayCount(TranState->Tasks);
        ++TaskIndex)
    {
        NeedToWait = NeedToWait || TranState->Tasks[TaskIndex].DependsOnGameMode;
    }
    if(NeedToWait)
    {
        Platform.CompleteAllWork(TranState->LowPriorityQueue);
    }

    Clear(&GameState->ModeArena);
    GameState->GameMode = GameMode;
}

internal void
Fade(game_state *GameState, r32 dt)
{
    switch(GameState->FadeState)
    {
        case FadeState_None:
        {
        } break;

        case FadeState_FadeIn:
        {
            GameState->CurrentAlpha += 0.75f*dt;
            if(GameState->CurrentAlpha > 1.0f)
            {
                GameState->CurrentAlpha = 1.0f;
                GameState->FadeState = FadeState_None;
            }
        } break;

        case FadeState_FadeOut:
        {
            GameState->CurrentAlpha -= 0.75f*dt;
            if(GameState->CurrentAlpha < 0.0f)
            {
                GameState->CurrentAlpha = 0.0f;
                GameState->FadeState = FadeState_None;
            }
        } break;
    }
}

#if SPELLWEAVER_INTERNAL
debug_table *GlobalDebugTable;
game_memory *DebugGlobalMemory;
#endif

platform_api Platform;
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Platform = Memory->PlatformAPI;    

    Platform.WriteLogFile(L"Game Update Started", __FILE__, __LINE__);
    
#if SPELLWEAVER_INTERNAL
    GlobalDebugTable = Memory->DebugTable;
    DebugGlobalMemory = Memory;
    
    {DEBUG_DATA_BLOCK("Renderer");
        DEBUG_B32(Global_Renderer_TestWeirdDrawBufferSize);
        {DEBUG_DATA_BLOCK("Camera");
            DEBUG_B32(Global_Renderer_Camera_UseDebug);
            DEBUG_VALUE(Global_Renderer_Camera_DebugDistance);
            DEBUG_B32(Global_Renderer_Camera_RoomBased);
        }
    }
    {DEBUG_DATA_BLOCK("GroundChunks");
        DEBUG_B32(Global_GroundChunks_Checkerboards);
        DEBUG_B32(Global_GroundChunks_RecomputeOnEXEChange);
        DEBUG_B32(Global_GroundChunks_Outlines);
        DEBUG_B32(Global_GroundChunksOn);
    }
    {DEBUG_DATA_BLOCK("AI/Familiar");
        DEBUG_B32(Global_AI_Familiar_FollowsHero); 
    }
    {DEBUG_DATA_BLOCK("Particles");
        DEBUG_B32(Global_Particles_Test); 
        DEBUG_B32(Global_Particles_ShowGrid);
    }
    {DEBUG_DATA_BLOCK("Simulation");
        DEBUG_B32(Global_Simulation_UseSpaceOutlines);
    }
    {DEBUG_DATA_BLOCK("Profile");
        DEBUG_UI_ELEMENT(DebugType_FrameSlider, FrameSlider);
        DEBUG_UI_ELEMENT(DebugType_LastFrameInfo, LastFrame);
        DEBUG_UI_ELEMENT(DebugType_DebugMemoryInfo, DebugMemory);
        DEBUG_UI_ELEMENT(DebugType_TopClocksList, GameUpdateAndRender);
    }

#endif

    TIMED_FUNCTION();

    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           (ArrayCount(Input->Controllers[0].Buttons)));

    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);    
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if(!GameState->IsInitialized)
    {
        memory_arena TotalArena;
        InitializeArena(&TotalArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *)Memory->PermanentStorage + sizeof(game_state));

        SubArena(&GameState->AudioArena, &TotalArena, Megabytes(1));
        SubArena(&GameState->ModeArena, &TotalArena,
                 GetArenaSizeRemaining(&TotalArena));

        InitializeAudioState(&GameState->AudioState, &GameState->AudioArena);

        GameState->MusicEntropy = RandomSeed(8902354); 
        GameState->CurrentAlpha = 1.0f;
        
        Platform.WriteLogFile(L"GameState Initialized", __FILE__, __LINE__);
        GameState->IsInitialized = true;
    }

    // NOTE(casey): Transient initialization
    Assert(sizeof(transient_state) <= Memory->TransientStorageSize);    
    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(!TranState->IsInitialized)
    {
        InitializeArena(&TranState->TranArena, Memory->TransientStorageSize - sizeof(transient_state),
                        (uint8 *)Memory->TransientStorage + sizeof(transient_state));
            
        TranState->HighPriorityQueue = Memory->HighPriorityQueue;
        TranState->LowPriorityQueue = Memory->LowPriorityQueue;
        for(uint32 TaskIndex = 0;
            TaskIndex < ArrayCount(TranState->Tasks);
            ++TaskIndex)
        {
            task_with_memory *Task = TranState->Tasks + TaskIndex;

            Task->BeingUsed = false;
            SubArena(&Task->Arena, &TranState->TranArena, Megabytes(1));
        }

        TranState->Assets = AllocateGameAssets(&TranState->TranArena, Megabytes(512), TranState,
                                               &Memory->TextureOpQueue);

        uint32 GroundBufferWidth = 128; 
        uint32 GroundBufferHeight = 128;
        
        TranState->MiniMap = MakeEmptyBitmap(&TranState->TranArena, 4096, 4096, false);

        GameState->CursorBitmapHover = GetFirstBitmapFrom(TranState->Assets, Asset_CursorHover);
        GameState->CursorBitmapClick = GetFirstBitmapFrom(TranState->Assets, Asset_CursorClick);

        asset_vector MusicWeightVector = {};
        MusicWeightVector.E[Tag_MusicType] = 1;
        MusicWeightVector.E[Tag_Variety] = 1;
        asset_vector MusicMatchVector = {};
        MusicMatchVector.E[Tag_MusicType] = MusicType_Ambient;

        MusicMatchVector.E[Tag_Variety] = VarietyType_0;
        GameState->AmbientMusic[0] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_1;
        GameState->AmbientMusic[1] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_2;
        GameState->AmbientMusic[2] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_3;
        GameState->AmbientMusic[3] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_4;
        GameState->AmbientMusic[4] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_5;
        GameState->AmbientMusic[5] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_6;
        GameState->AmbientMusic[6] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_7;
        GameState->AmbientMusic[7] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_8;
        GameState->AmbientMusic[8] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_9;
        GameState->AmbientMusic[9] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_10;
        GameState->AmbientMusic[10] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_11;
        GameState->AmbientMusic[11] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_12;
        GameState->AmbientMusic[12] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_13;
        GameState->AmbientMusic[13] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_14;
        GameState->AmbientMusic[14] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

        MusicMatchVector.E[Tag_MusicType] = MusicType_DarkAmbient;
        MusicMatchVector.E[Tag_Variety] = VarietyType_0;
        GameState->DarkAmbientMusic[0] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_1;
        GameState->DarkAmbientMusic[1] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_2;
        GameState->DarkAmbientMusic[2] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_3;
        GameState->DarkAmbientMusic[3] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_4;
        GameState->DarkAmbientMusic[4] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

        MusicMatchVector.E[Tag_MusicType] = MusicType_Action;
        MusicMatchVector.E[Tag_Variety] = VarietyType_0;
        GameState->ActionMusic[0] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_1;
        GameState->ActionMusic[1] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_2;
        GameState->ActionMusic[2] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_3;
        GameState->ActionMusic[3] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = VarietyType_4;
        GameState->ActionMusic[4] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

        MusicMatchVector.E[Tag_Variety] = VarietyType_None;
        MusicMatchVector.E[Tag_MusicType] = MusicType_GameStartFX;
        GameState->GameStartFX = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

        MusicMatchVector.E[Tag_MusicType] = MusicType_GameEndDeathFX;
        GameState->GameEndDeathFX = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        
        Platform.WriteLogFile(L"Transient State Initialized", __FILE__, __LINE__);
        TranState->IsInitialized = true;
    }

    {DEBUG_DATA_BLOCK("Memory");
        memory_arena *ModeArena = &GameState->ModeArena;
        DEBUG_VALUE(ModeArena);
        
        memory_arena *AudioArena = &GameState->AudioArena;
        DEBUG_VALUE(AudioArena);
        
        memory_arena *TranArena = &TranState->TranArena;
        DEBUG_VALUE(TranArena);
    }

    if(TranState->MainGenerationID)
    {
        EndGeneration(TranState->Assets, TranState->MainGenerationID);
    }

    TranState->MainGenerationID = BeginGeneration(TranState->Assets);

    if(GameState->GameMode == GameMode_None)
    {
//        PlayTest(GameState, TranState);
//        PlayTitleScreen(GameState, TranState);
        GameState->GameHaveStarted = true;
        PlayWorld(GameState, TranState);
    }
    
    //
    // NOTE(casey): Render
    //
    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

    // TODO(casey): Decide what our pushbuffer size is!
    render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands, TranState->MainGenerationID,
                                                 false, RenderCommands->Width, RenderCommands->Height);
    render_group *RenderGroup = &RenderGroup_;

    // TODO(casey): Eliminate these entirely
    loaded_bitmap DrawBuffer = {};
    DrawBuffer.Width = RenderCommands->Width;
    DrawBuffer.Height = RenderCommands->Height;

    Orthographic(RenderGroup, 1.0f);
    object_transform MouseTransform = DefaultFlatTransform();
    MouseTransform.ChunkZ = 1200000;
    v3 MouseP = Unproject(RenderGroup, &MouseTransform, V2(Input->MouseX, Input->MouseY));

    bitmap_id MouseBitmap = {};
    if(WasPressed(Input->MouseButtons[0]))
    {
        MouseBitmap = GameState->CursorBitmapClick;
    }
    else
    {
        MouseBitmap = GameState->CursorBitmapHover;
    }

    Fade(GameState, Input->dtForFrame);

    if(GameState->CurrentAlpha != 0.0f)
    {
        object_transform Transform = DefaultFlatTransform(); 
        PushRect(RenderGroup, &Transform, V3(0, 0, 0), V2i(RenderCommands->Width, RenderCommands->Height),
                 V4(0, 0, 0, GameState->CurrentAlpha));
    }

    PushBitmap(RenderGroup, &MouseTransform, MouseBitmap, 25.0f, MouseP);
    
    PlayBackGroundMusic(GameState);

    b32 Rerun = false;
    do
    {
        switch(GameState->GameMode)
        {
            case GameMode_TitleScreen:
            {
                Rerun = UpdateAndRenderTitleScreen(GameState, TranState, RenderGroup, &DrawBuffer,
                                                   Input, GameState->TitleScreen);
            } break;

            case GameMode_World:
            {
                Rerun = UpdateAndRenderWorld(GameState, GameState->WorldMode, TranState, Input, RenderGroup,
                                             &DrawBuffer);
            } break;

            InvalidDefaultCase;
        }

    } while(Rerun);

    EndRenderGroup(RenderGroup);

    EndTemporaryMemory(RenderMemory);
    
    CheckArena(&GameState->ModeArena);
    CheckArena(&TranState->TranArena);

    Platform.WriteLogFile(L"Game Update End", __FILE__, __LINE__);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    transient_state *TranState = (transient_state *)Memory->TransientStorage;

    OutputPlayingSounds(&GameState->AudioState, SoundBuffer, TranState->Assets, &TranState->TranArena);
}

#if SPELLWEAVER_INTERNAL
#include "spellweaver_debug.cpp"
#else
extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd)
{
}
#endif


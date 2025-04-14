/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#include "engine_game.h"

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

internal void
SetGameMode(game_state *GameState, transient_state *TranState, game_mode GameMode)
{
    b32 NeedToWait = false;
    Platform.CompleteAllWork(TranState->LowPriorityQueue);

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

inline void
ChangeBackgroundMusic(game_state *GameState, s32 NewMusicState)
{
    GameState->MusicState = NewMusicState;
    GameState->ChangeMusic = true;
}

internal void
PlayTitleScreen(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_TitleScreen);

    GameState->FadeState = FadeState_FadeOut;
    if(GameState->MusicState != MusicState_Ambient)
    {
        ChangeBackgroundMusic(GameState, MusicState_Ambient);
    }
    
    game_mode_title_screen *Result = PushStruct(&GameState->ModeArena, game_mode_title_screen);
    Result->t = 0;

    GameState->TitleScreen = Result;
}

#include "spellweaver\spellweaver_title_mode.cpp"

internal b32
GameUpdateAndRender(editor_state *EditorState, transient_state *TranState,
                    engine_input *Input, memory_arena *GamePermArena,
                    memory_arena *GameTranArena,
                    platform_texture_op_queue *TextureOpQueue,
                    editor_render_commands *RenderCommands)
{
    b32 Result = false;//CheckForMetaInput(EditorState, TranState, Input);
    if(!Result)
    {

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

        game_state *GameState = (game_state *)GamePermArena->Base;
        if(!GameState->IsInitialized)
        {
            Assert(sizeof(game_state) <= GetArenaSizeRemaining(GamePermArena));
            GameState = PushStruct(GamePermArena, game_state);

            SubArena(&GameState->AudioArena, GamePermArena, Megabytes(1));
            SubArena(&GameState->ModeArena, GamePermArena,
                     GetArenaSizeRemaining(GamePermArena));

//            InitializeAudioState(&GameState->AudioState, &GameState->AudioArena);
            GameState->AudioState = EditorState->AudioState;
            GameState->MusicEntropy = RandomSeed(8902354); 
            GameState->CurrentAlpha = 1.0f;
        
//        Platform.WriteLogFile(L"GameState Initialized", __FILE__, __LINE__);
            GameState->IsInitialized = true;
        }

        // NOTE(casey): Transient initialization
        game_transient_state *GameTranState = (game_transient_state *)GameTranArena->Base;
        if(!GameTranState->IsInitialized)
        {
            Assert(sizeof(transient_state) <= GetArenaSizeRemaining(GameTranArena));    
            game_transient_state *GameTranState = PushStruct(GameTranArena, game_transient_state);

            SubArena(&GameTranState->TranArena, GameTranArena,
                     GetArenaSizeRemaining(GameTranArena));

            GameTranState->Assets = AllocateEditorAssets(&GameTranState->TranArena, Megabytes(512),
                                                         TranState, TextureOpQueue);

            uint32 GroundBufferWidth = 128; 
            uint32 GroundBufferHeight = 128;
        
            GameTranState->MiniMap = MakeEmptyBitmap(&GameTranState->TranArena, 4096, 4096, false);

            asset_vector MatchVector = {};
            asset_vector WeightVector = {};
            WeightVector.E[Tag_BiomeType] = 1;
            WeightVector.E[Tag_TileMainSurface] = 1;
            WeightVector.E[Tag_TileMergeSurface] = 1;

            MatchVector.E[Tag_BiomeType] = 64;
            MatchVector.E[Tag_TileMainSurface] = 64;
            MatchVector.E[Tag_TileMergeSurface] = 64;
            tileset_id ID = GetBestMatchTilesetFrom(GameTranState->Assets, Asset_Tileset,
                                                    &MatchVector, &WeightVector);
            GameTranState->GlobalTilesetID = ID;

            GameState->CursorBitmapHover = GetFirstBitmapFrom(GameTranState->Assets, Asset_CursorHover);
            GameState->CursorBitmapClick = GetFirstBitmapFrom(GameTranState->Assets, Asset_CursorClick);

            asset_vector MusicWeightVector = {};
            MusicWeightVector.E[Tag_MusicType] = 1;
            MusicWeightVector.E[Tag_Variety] = 1;
            asset_vector MusicMatchVector = {};
            MusicMatchVector.E[Tag_MusicType] = MusicType_Ambient;

            MusicMatchVector.E[Tag_Variety] = VarietyType_0;
            GameState->AmbientMusic[0] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_1;
            GameState->AmbientMusic[1] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_2;
            GameState->AmbientMusic[2] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_3;
            GameState->AmbientMusic[3] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_4;
            GameState->AmbientMusic[4] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_5;
            GameState->AmbientMusic[5] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_6;
            GameState->AmbientMusic[6] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_7;
            GameState->AmbientMusic[7] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_8;
            GameState->AmbientMusic[8] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_9;
            GameState->AmbientMusic[9] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_10;
            GameState->AmbientMusic[10] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_11;
            GameState->AmbientMusic[11] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_12;
            GameState->AmbientMusic[12] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_13;
            GameState->AmbientMusic[13] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_14;
            GameState->AmbientMusic[14] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

            MusicMatchVector.E[Tag_MusicType] = MusicType_DarkAmbient;
            MusicMatchVector.E[Tag_Variety] = VarietyType_0;
            GameState->DarkAmbientMusic[0] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_1;
            GameState->DarkAmbientMusic[1] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_2;
            GameState->DarkAmbientMusic[2] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_3;
            GameState->DarkAmbientMusic[3] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_4;
            GameState->DarkAmbientMusic[4] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

            MusicMatchVector.E[Tag_MusicType] = MusicType_Action;
            MusicMatchVector.E[Tag_Variety] = VarietyType_0;
            GameState->ActionMusic[0] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_1;
            GameState->ActionMusic[1] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_2;
            GameState->ActionMusic[2] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_3;
            GameState->ActionMusic[3] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
            MusicMatchVector.E[Tag_Variety] = VarietyType_4;
            GameState->ActionMusic[4] = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

            MusicMatchVector.E[Tag_Variety] = VarietyType_None;
            MusicMatchVector.E[Tag_MusicType] = MusicType_GameStartFX;
            GameState->GameStartFX = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

            MusicMatchVector.E[Tag_MusicType] = MusicType_GameEndDeathFX;
            GameState->GameEndDeathFX = GetBestMatchSoundFrom(GameTranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        
//        Platform.WriteLogFile(L"Transient State Initialized", __FILE__, __LINE__);
            GameTranState->IsInitialized = true;
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
            PlayTitleScreen(GameState, TranState);
        }

#if 0
        //
        // NOTE(casey): 
        //
        {
            v2 MusicVolume;
            MusicVolume.y = SafeRatio0((r32)Input->MouseX, (r32)Buffer->Width);
            MusicVolume.x = 1.0f - MusicVolume.y;
            ChangeVolume(&GameState->AudioState, GameState->Music, 0.01f, MusicVolume);
        }
#endif
    
        //
        // NOTE(casey): Render
        //
        temporary_memory RenderMemory = BeginTemporaryMemory(&GameTranState->TranArena);

        // TODO(casey): Decide what our pushbuffer size is!
        render_group RenderGroup_ = BeginRenderGroup(GameTranState->Assets, RenderCommands,
                                                     GameTranState->MainGenerationID, false, 
                                                     RenderCommands->Width, RenderCommands->Height);
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
            Transform.ChunkZ = 1100000;
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
                    Rerun = UpdateAndRenderTitleScreen(GameState, GameTranState, RenderGroup, &DrawBuffer,
                                                       Input, GameState->TitleScreen);
                } break;

                case GameMode_World:
                {
//                Rerun = UpdateAndRenderWorld(GameState, GameState->WorldMode, TranState, Input, RenderGroup,
//                                             &DrawBuffer);
                } break;

                case GameMode_Test:
                {
//                Rerun = UpdateAndRenderAStarTest(GameState, TranState, RenderGroup, Input, GameState->AStarTest,
//                                                 &DrawBuffer);
                } break;

                InvalidDefaultCase;
            }

        } while(Rerun);

        EndRenderGroup(RenderGroup);

        EndTemporaryMemory(RenderMemory);
    
        CheckArena(&GameState->ModeArena);
        CheckArena(&GameTranState->TranArena);

//    Platform.WriteLogFile(L"Game Update End", __FILE__, __LINE__);
    }

    return(Result);
}

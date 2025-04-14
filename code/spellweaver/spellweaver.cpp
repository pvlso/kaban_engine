
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

internal void
PlayIntroCutscene(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_CutScene);
    
    
    game_mode_cutscene *Result = PushStruct(&GameState->ModeArena, game_mode_cutscene);

    Result->ID = CutsceneID_Intro;
    Result->t = 0;

    GameState->CutScene = Result;
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

inline void
InitASNode(as_node *Node, s32 X, s32 Y)
{
    Node->Obstacle = false;
    Node->Visited = false;
    Node->X = X;
    Node->Y = Y;
    Node->Parent = 0;
}

internal void
PlayTest(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_Test);

    GameState->FadeState = FadeState_FadeOut;
    if(GameState->MusicState != MusicState_Ambient)
    {
        ChangeBackgroundMusic(GameState, MusicState_Ambient);
    }
    
    game_mode_a_star_test *Result = PushStruct(&GameState->ModeArena, game_mode_a_star_test);

    Result->NodeCount = 32*32;
    Result->Nodes = PushArray(&GameState->ModeArena, Result->NodeCount, as_node);
    Result->Heap.MaxSize = Result->NodeCount;
    Result->Heap.Nodes = PushArray(&GameState->ModeArena, Result->Heap.MaxSize, sort_entry);
    for(s32 Y = 0;
        Y < 32;
        ++Y)
    {
        for(s32 X = 0;
            X < 32;
            ++X)
        {
            as_node *Node = Result->Nodes + Y*32 + X;
            InitASNode(Node, X, Y);
        }
    }
    
    GameState->AStarTest = Result;
}

inline b32
DecorationIsValid(decoration *Decoration)
{
    b32 Result = false;

    if((IsValid(Decoration->P)) && (IsValid(Decoration->BitmapID)))
    {
        Result = true;
    }
    
    return(Result);
}

internal void
CreateMiniMap(game_mode_world *WorldMode, transient_state *TranState)
{
    temporary_memory GroundMemory = BeginTemporaryMemory(&TranState->TranArena);

    loaded_bitmap *Buffer = &TranState->MiniMap;
    Buffer->AlignPercentage = V2(0.5f, 0.5f);
    Buffer->WidthOverHeight = 1.0f; 

    u32 PushBufferSize = Megabytes(4);
    void *PushBuffer = PushSize(GroundMemory.Arena, PushBufferSize);
    void *SortMemory = PushSize(GroundMemory.Arena, PushBufferSize/2);
    void *ClipRectMemory = PushSize(GroundMemory.Arena, PushBufferSize/2);

    game_render_commands Commands = RenderCommandStruct(PushBufferSize, PushBuffer, 
                                                        (u32)Buffer->Width,
                                                        (u32)Buffer->Height);

    render_group RenderGroup = BeginRenderGroup(TranState->Assets, &Commands, TranState->MainGenerationID, true);
    Orthographic(&RenderGroup, Buffer->Width, Buffer->Height, 1.0f);
    Clear(&RenderGroup, V4(1.0f, 0.0f, 1.0f, 1.0f));
    object_transform Transform = DefaultFlatTransform();
    RenderGroup.RendersInBackground = true;

    v2 MapHalfDim = 0.5f*V2i(Buffer->Width, Buffer->Height);
    
    int32 TileCount = WORLD_TILE_COUNT_PER_DIM;

    int32 MinTileX = 0;
    int32 MaxTileX = TileCount;
    int32 MinTileY = 0;
    int32 MaxTileY = TileCount;

    r32 TileDim = (r32)Buffer->Width / (r32)TileCount;
    for(int32 TileY = MinTileY;
        TileY < MaxTileY;
        ++TileY)
    {
        for(int32 TileX = MinTileX;
            TileX < MaxTileX;
            ++TileX)
        {
            if((TileX < WORLD_TILE_COUNT_PER_DIM) && (TileY < WORLD_TILE_COUNT_PER_DIM))
            {
                world_tile *WorldTile = WorldMode->World->Tiles + TileY*WORLD_TILE_COUNT_PER_DIM + TileX;

                real32 X = (real32)(TileX - MinTileX);
                real32 Y = (real32)(TileY - MinTileY);
                v2 P = -MapHalfDim + V2(TileDim*X, TileDim*Y);
                    
                bitmap_id ID = WorldTile->TileBitmapID;
                Transform.OffsetP = V3(P, 1.0f);

                Assert(ID.Value);
                PushBitmap(&RenderGroup, Transform, ID, TileDim, V3(0, 0, 0));
            }
        }
    }

    for(int32 TileY = MinTileY;
        // NOTE(paul): This is a hack to not render out of the bitmap memory
        TileY < MaxTileY - 6;
        ++TileY)
    {
        for(int32 TileX = MinTileX;
            TileX < MaxTileX;
            ++TileX)
        {
            if((TileX < WORLD_TILE_COUNT_PER_DIM) && (TileY < WORLD_TILE_COUNT_PER_DIM))
            {
                decoration *Decoration = WorldMode->World->Decorations + TileY*WORLD_TILE_COUNT_PER_DIM + TileX;
                if(DecorationIsValid(Decoration))
                {
                    r32 RenderHeight = Decoration->Height * TileDim;
                    real32 X = (real32)(TileX - MinTileX);
                    real32 Y = (real32)(TileY - MinTileY);
                    v2 P = -MapHalfDim + V2(TileDim*X, TileDim*Y);
                    Transform.OffsetP = V3(P, 1.0f);
                
                    bitmap_id ID = Decoration->BitmapID;
                    if(Decoration->IsSpriteSheet)
                    {
                        loaded_spritesheet *SpriteSheet = PushSpriteSheet(&RenderGroup, Decoration->SpriteSheetID, true);
                        ID = SpriteSheet->SpriteIDs[0];
                        Assert(ID.Value);
                        PushBitmap(&RenderGroup, Transform, ID, RenderHeight, V3(0, 0, 0));
                    }
                    else
                    {
                        Assert(ID.Value);
                        PushBitmap(&RenderGroup, Transform, ID, RenderHeight, V3(0, 0, 0));
                    }
                }
            }
        }
    }
    
    Platform.SortRenderEntries(&Commands, SortMemory);
    Platform.LinearizeRenderClipRects(&Commands, ClipRectMemory);
    Platform.SoftwareRenderCommands(TranState->HighPriorityQueue, &Commands, Buffer);        
    EndRenderGroup(&RenderGroup);

    if(Buffer->TextureHandle)
    {
        Platform.DeallocateTexture(Buffer->TextureHandle);
        Buffer->TextureHandle = 
            Platform.AllocateTexture(Buffer->Width, Buffer->Height, Buffer->Memory);
    }
    else
    {
        Buffer->TextureHandle = 
            Platform.AllocateTexture(Buffer->Width, Buffer->Height, Buffer->Memory);
    }
    
    EndTemporaryMemory(GroundMemory);
}

internal void
PlayWorld(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_World);
    GameState->FadeState = FadeState_FadeOut;

    PlaySound(&GameState->AudioState, GameState->GameStartFX);
    ChangeBackgroundMusic(GameState, MusicState_Ambient);
        
    game_mode_world *WorldMode = PushStruct(&GameState->ModeArena, game_mode_world);

    real32 PixelsToMeters = 1.0f / 32.0f;
    uint32 TileSideInPixels = 32;
    real32 TileSideInMeters = TileSideInPixels * PixelsToMeters;

    WorldMode->EffectsEntropy = RandomSeed(1257457);
    WorldMode->MathEntropy = RandomSeed(562739124);
    WorldMode->TypicalFloorHeight = 3.0f;
    
    v2 WorldChunkDimInMeters =
        {
            TileSideInMeters*TILES_PER_CHUNK_DIM,
            TileSideInMeters*TILES_PER_CHUNK_DIM,
        };

    WorldMode->World = CreateWorld(TranState, WorldChunkDimInMeters, TileSideInMeters, &GameState->ModeArena);
    
    WorldMode->MiniMapBitmap = MakeEmptyBitmap(&WorldMode->World->Arena, 720, 720, false);

    real32 TileDepthInMeters = WorldMode->TypicalFloorHeight;

    WorldMode->NullCollision = MakeNullCollision(WorldMode);
    WorldMode->SphereCollision = MakeSimpleGroundedCollision(WorldMode, 0.5f, 0.5f, 0.5f);
    WorldMode->ItemCollision = MakeSimpleGroundedCollision(WorldMode, 0.5f, 0.5f, 0.5f);
    WorldMode->PlayerCollision = MakeSimpleGroundedCollision(WorldMode, 0.8f, 0.5f, 1.2f);
    WorldMode->MonsterCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 2.0f);
    WorldMode->FamiliarCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.5f);
    WorldMode->WallCollision = MakeSimpleGroundedCollision(WorldMode,
                                                           TileSideInMeters,
                                                           TileSideInMeters,
                                                           TileSideInMeters);

    WorldMode->NPCCollision = MakeSimpleGroundedCollision(WorldMode, 0.8f, 0.5f, 1.2f);

    WorldMode->MagicSwordCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.1f);
    WorldMode->SpellCollision = MakeSimpleGroundedCollision(WorldMode, 0.5f, 0.5f, 0.5f);

    v2 square[] = {{4.0f, 160.0f}, {11.0f, 160.0f}, {11.0f, 167.0f}, {4.0f, 167.0f}};

    WorldMode->BirdSoundPolygon.Vertices = PushArray(&WorldMode->World->Arena, 38, v2);
    WorldMode->BirdSoundPolygon.VertexCount = 38;
    WorldMode->BirdSoundPolygon.Vertices[0] = V2(0.0f, 0.0f);
    WorldMode->BirdSoundPolygon.Vertices[1] = V2(150.0f, 0.0f);
    WorldMode->BirdSoundPolygon.Vertices[2] = V2(150.0f, 33.0f);
    WorldMode->BirdSoundPolygon.Vertices[3] = V2(155.0f, 33.0f);
    WorldMode->BirdSoundPolygon.Vertices[4] = V2(155.0f, 64.0f);
    WorldMode->BirdSoundPolygon.Vertices[5] = V2(163.0f, 64.0f);
    WorldMode->BirdSoundPolygon.Vertices[6] = V2(163.0f, 86.0f);
    WorldMode->BirdSoundPolygon.Vertices[7] = V2(168.0f, 86.0f);
    WorldMode->BirdSoundPolygon.Vertices[8] = V2(168.0f, 105.0f);
    WorldMode->BirdSoundPolygon.Vertices[9] = V2(154.0f, 105.0f);
    WorldMode->BirdSoundPolygon.Vertices[10] = V2(154.0f, 99.0f);
    WorldMode->BirdSoundPolygon.Vertices[11] = V2(144.0f, 99.0f);
    WorldMode->BirdSoundPolygon.Vertices[12] = V2(144.0f, 92.0f);
    WorldMode->BirdSoundPolygon.Vertices[13] = V2(127.0f, 92.0f);
    WorldMode->BirdSoundPolygon.Vertices[14] = V2(127.0f, 95.0f);
    WorldMode->BirdSoundPolygon.Vertices[15] = V2(123.0f, 95.0f);
    WorldMode->BirdSoundPolygon.Vertices[16] = V2(123.0f, 99.0f);
    WorldMode->BirdSoundPolygon.Vertices[17] = V2(118.0f, 99.0f);
    WorldMode->BirdSoundPolygon.Vertices[18] = V2(118.0f, 103.0f);
    WorldMode->BirdSoundPolygon.Vertices[19] = V2(114.0f, 103.0f);
    WorldMode->BirdSoundPolygon.Vertices[20] = V2(114.0f, 107.0f);
    WorldMode->BirdSoundPolygon.Vertices[21] = V2(108.0f, 107.0f);
    WorldMode->BirdSoundPolygon.Vertices[22] = V2(108.0f, 111.0f);
    WorldMode->BirdSoundPolygon.Vertices[23] = V2(101.0f, 111.0f);
    WorldMode->BirdSoundPolygon.Vertices[24] = V2(101.0f, 115.0f);
    WorldMode->BirdSoundPolygon.Vertices[25] = V2(94.0f, 115.0f);
    WorldMode->BirdSoundPolygon.Vertices[26] = V2(94.0f, 119.0f);
    WorldMode->BirdSoundPolygon.Vertices[27] = V2(88.0f, 119.0f);
    WorldMode->BirdSoundPolygon.Vertices[28] = V2(88.0f, 123.0f);
    WorldMode->BirdSoundPolygon.Vertices[29] = V2(83.0f, 123.0f);
    WorldMode->BirdSoundPolygon.Vertices[30] = V2(83.0f, 127.0f);
    WorldMode->BirdSoundPolygon.Vertices[31] = V2(77.0f, 127.0f);
    WorldMode->BirdSoundPolygon.Vertices[32] = V2(77.0f, 130.0f);
    WorldMode->BirdSoundPolygon.Vertices[33] = V2(51.0f, 130.0f);
    WorldMode->BirdSoundPolygon.Vertices[34] = V2(51.0f, 135.0f);
    WorldMode->BirdSoundPolygon.Vertices[35] = V2(34.0f, 135.0f);
    WorldMode->BirdSoundPolygon.Vertices[36] = V2(34.0f, 139.0f);
    WorldMode->BirdSoundPolygon.Vertices[37] = V2(0.0f, 139.0f);

    WorldMode->RiverPolygon0.Vertices = PushArray(&WorldMode->World->Arena, 14, v2);
    WorldMode->RiverPolygon0.VertexCount = 14;
    WorldMode->RiverPolygon0.Vertices[0] = V2(0.0f, 0.0f);
    WorldMode->RiverPolygon0.Vertices[1] = V2(107.0f, 0.0f);
    WorldMode->RiverPolygon0.Vertices[2] = V2(107.0f, 3.0f);
    WorldMode->RiverPolygon0.Vertices[3] = V2(93.0f, 26.0f);
    WorldMode->RiverPolygon0.Vertices[4] = V2(90.0f, 32.0f);
    WorldMode->RiverPolygon0.Vertices[5] = V2(90.0f, 44.0f);
    WorldMode->RiverPolygon0.Vertices[6] = V2(77.0f, 44.0f);
    WorldMode->RiverPolygon0.Vertices[7] = V2(72.0f, 35.0f);
    WorldMode->RiverPolygon0.Vertices[8] = V2(55.0f, 29.0f);
    WorldMode->RiverPolygon0.Vertices[9] = V2(49.0f, 25.0f);
    WorldMode->RiverPolygon0.Vertices[10] = V2(42.0f, 18.0f);
    WorldMode->RiverPolygon0.Vertices[11] = V2(40.0f, 12.0f);
    WorldMode->RiverPolygon0.Vertices[12] = V2(40.0f, 4.0f);
    WorldMode->RiverPolygon0.Vertices[13] = V2(0.0f, 4.0f);

    WorldMode->RiverPolygon1.Vertices = PushArray(&WorldMode->World->Arena, 58, v2);
    WorldMode->RiverPolygon1.VertexCount = 58;
    WorldMode->RiverPolygon1.Vertices[0] = V2(0.0f, 136.0f);
    WorldMode->RiverPolygon1.Vertices[1] = V2(27.0f, 136.0f);
    WorldMode->RiverPolygon1.Vertices[2] = V2(36.0f, 131.0f);
    WorldMode->RiverPolygon1.Vertices[3] = V2(48.0f, 128.0f);
    WorldMode->RiverPolygon1.Vertices[4] = V2(56.0f, 127.0f);
    WorldMode->RiverPolygon1.Vertices[5] = V2(74.0f, 126.0f);
    WorldMode->RiverPolygon1.Vertices[6] = V2(82.0f, 121.0f);
    WorldMode->RiverPolygon1.Vertices[7] = V2(94.0f, 112.0f);
    WorldMode->RiverPolygon1.Vertices[8] = V2(105.0f, 105.0f);
    WorldMode->RiverPolygon1.Vertices[9] = V2(119.0f, 95.0f);
    WorldMode->RiverPolygon1.Vertices[10] = V2(127.0f, 89.0f);
    WorldMode->RiverPolygon1.Vertices[11] = V2(145.0f, 89.0f);
    WorldMode->RiverPolygon1.Vertices[12] = V2(156.0f, 98.0f);
    WorldMode->RiverPolygon1.Vertices[13] = V2(164.0f, 100.0f);
    WorldMode->RiverPolygon1.Vertices[14] = V2(161.0f, 86.0f);
    WorldMode->RiverPolygon1.Vertices[15] = V2(155.0f, 72.0f);
    WorldMode->RiverPolygon1.Vertices[16] = V2(152.0f, 66.0f);
    WorldMode->RiverPolygon1.Vertices[17] = V2(150.0f, 58.0f);
    WorldMode->RiverPolygon1.Vertices[18] = V2(148.0f, 43.0f);
    WorldMode->RiverPolygon1.Vertices[19] = V2(147.0f, 27.0f);
    WorldMode->RiverPolygon1.Vertices[20] = V2(146.0f, 22.0f);
    WorldMode->RiverPolygon1.Vertices[21] = V2(145.0f, 0.0f);
    WorldMode->RiverPolygon1.Vertices[22] = V2(171.0f, 0.0f);
    WorldMode->RiverPolygon1.Vertices[23] = V2(171.0f, 108.0f);
    WorldMode->RiverPolygon1.Vertices[24] = V2(155.0f, 107.0f);
    WorldMode->RiverPolygon1.Vertices[25] = V2(134.0f, 97.0f);
    WorldMode->RiverPolygon1.Vertices[26] = V2(123.0f, 101.0f);
    WorldMode->RiverPolygon1.Vertices[27] = V2(122.0f, 112.0f);
    WorldMode->RiverPolygon1.Vertices[28] = V2(122.0f, 182.0f);
    WorldMode->RiverPolygon1.Vertices[29] = V2(128.0f, 207.0f);
    WorldMode->RiverPolygon1.Vertices[30] = V2(139.0f, 230.0f);
    WorldMode->RiverPolygon1.Vertices[31] = V2(143.0f, 239.0f);
    WorldMode->RiverPolygon1.Vertices[32] = V2(143.0f, 255.0f);
    WorldMode->RiverPolygon1.Vertices[33] = V2(134.0f, 255.0f);
    WorldMode->RiverPolygon1.Vertices[34] = V2(134.0f, 236.0f);
    WorldMode->RiverPolygon1.Vertices[35] = V2(129.0f, 224.0f);
    WorldMode->RiverPolygon1.Vertices[36] = V2(123.0f, 211.0f);
    WorldMode->RiverPolygon1.Vertices[37] = V2(117.0f, 199.0f);
    WorldMode->RiverPolygon1.Vertices[38] = V2(115.0f, 190.0f);
    WorldMode->RiverPolygon1.Vertices[39] = V2(113.0f, 178.0f);
    WorldMode->RiverPolygon1.Vertices[40] = V2(113.0f, 174.0f);
    WorldMode->RiverPolygon1.Vertices[41] = V2(109.0f, 162.0f);
    WorldMode->RiverPolygon1.Vertices[42] = V2(107.0f, 152.0f);
    WorldMode->RiverPolygon1.Vertices[43] = V2(108.0f, 137.0f);
    WorldMode->RiverPolygon1.Vertices[44] = V2(110.0f, 135.0f);
    WorldMode->RiverPolygon1.Vertices[45] = V2(110.0f, 124.0f);
    WorldMode->RiverPolygon1.Vertices[46] = V2(103.0f, 124.0f);
    WorldMode->RiverPolygon1.Vertices[47] = V2(91.0f, 132.0f);
    WorldMode->RiverPolygon1.Vertices[48] = V2(77.0f, 140.0f);
    WorldMode->RiverPolygon1.Vertices[49] = V2(63.0f, 147.0f);
    WorldMode->RiverPolygon1.Vertices[50] = V2(42.0f, 147.0f);
    WorldMode->RiverPolygon1.Vertices[51] = V2(40.0f, 148.0f);
    WorldMode->RiverPolygon1.Vertices[52] = V2(25.0f, 148.0f);
    WorldMode->RiverPolygon1.Vertices[53] = V2(22.0f, 150.0f);
    WorldMode->RiverPolygon1.Vertices[54] = V2(19.0f, 150.0f);
    WorldMode->RiverPolygon1.Vertices[55] = V2(13.0f, 155.0f);
    WorldMode->RiverPolygon1.Vertices[56] = V2(3.0f, 159.0f);
    WorldMode->RiverPolygon1.Vertices[57] = V2(0.0f, 159.0f);

    uint32 ScreenBaseX = 0;
    uint32 ScreenBaseY = 0;

#if 1    
    // NOTE(paul): Loading Decoration Entities
    for(u32 DecorationIndex = 0;
        DecorationIndex < WorldMode->World->TileCount;
        ++DecorationIndex)
    {
        decoration *Decoration = WorldMode->World->Decorations + DecorationIndex;
        if(DecorationIsValid(Decoration))
        {
            if(Decoration->IsSpriteSheet)
            {
                AddAnimatedDecorationEntity(WorldMode, TranState->Assets, Decoration);
            }
            else
            {
                AddDecorationEntity(WorldMode, TranState->Assets, Decoration);
            }
        }
    }

    // NOTE(paul): Loading Collision Entities
    for(u32 CollisionIndex = 0;
        CollisionIndex < WorldMode->World->TileCount;
        ++CollisionIndex)
    {
        collision *Collision = WorldMode->World->Collisions + CollisionIndex;
        if(IsValid(Collision->P) && HasArea(Collision->Rect))
        {
            AddCollisionEntity(WorldMode, TranState->Assets, Collision);
            world_position P = TilePositionFromChunkPosition(&Collision->P);

            P = MapIntoTileSpace(WorldMode->World, P, V2(-1.5f, -1.5f));
            as_tile_node *ClosestNode = GetTileNode(WorldMode->World, P);
            s32 MinX = ClosestNode->X - 3;
            s32 MinY = ClosestNode->Y - 3;

            s32 MaxX = MinX + 5;
            s32 MaxY = MinY + 5;

            for(s32 Y = MinY;
                Y <= MaxY;
                ++Y)
            {
                for(s32 X = MinX;
                    X <= MaxX;
                    ++X)
                {
                    if((X >= 0) && (X < WORLD_TILE_NODE_COUNT_PER_DIM) &&
                       (Y >= 0) && (Y < WORLD_TILE_NODE_COUNT_PER_DIM))
                    {
                        as_tile_node *Node = GetTileNode(WorldMode->World, X, Y);
                        Node->Obstacle = true;
                    }
                }
            }
        }
    }
#endif

    //
    // NOTE(paul): Camera Setup
    //
    world_position NewCameraP = {};
    uint32 CameraTileX = 71;
    uint32 CameraTileY = 233;
    NewCameraP = ChunkPositionFromTilePosition(WorldMode->World,
                                               CameraTileX,
                                               CameraTileY);
    WorldMode->CameraBoundsMin.TileX = 0;
    WorldMode->CameraBoundsMin.TileY = 0;
    WorldMode->CameraBoundsMin.Offset = V2(-0.5f, -0.5f);
    
    WorldMode->CameraBoundsMax.TileX = WORLD_TILE_COUNT_PER_DIM;
    WorldMode->CameraBoundsMax.TileY = WORLD_TILE_COUNT_PER_DIM;
    WorldMode->CameraBoundsMax.Offset = V2(0.5f, 0.5f);

    WorldMode->CameraP = NewCameraP;

    // NOTE(paul): General Text Config
    WorldMode->GeneralTextConfig.Font = 0;
    WorldMode->GeneralTextConfig.FontInfo = 0;
    WorldMode->GeneralTextConfig.TextTransform.OffsetP = V3(0.0f, 0.0f, 0.0f);
    WorldMode->GeneralTextConfig.TextTransform.SortBias = 1000000.0f;
    WorldMode->GeneralTextConfig.TextShadowTransform.OffsetP = V3(0.0f, 0.0f, 0.0f);
    WorldMode->GeneralTextConfig.TextShadowTransform.SortBias = 100000.0f;
    WorldMode->GeneralTextConfig.FontScale = 0.018f;
    WorldMode->GeneralTextConfig.Color = V4(1, 1, 1, 1);

    entity_id ElderTavor = AddElderTavor(WorldMode, TranState->Assets, 75, 236);
    entity_id HerbalistElara = AddHerbalistElara(WorldMode, TranState->Assets, 8, 193);
    entity_id Jacob = AddJacob(WorldMode, TranState->Assets, 99, 133);
    
    WorldMode->Quests[0] = {};
    WorldMode->Quests[0].Location = NullPosition();
    WorldMode->Quests[0].GiverLocation = NullPosition();
        
    // NOTE(paul): QuestName_Begining
    quest *Quest = &WorldMode->Quests[QuestName_FindTavor];
    Quest->Type = QuestType_Main;
    Quest->QuestName = QuestName_FindTavor;
    Quest->UnCompletedText = PushString(&WorldMode->World->Arena, "Find and talk to Elder Tavor");
    Quest->CompletedText = PushString(&WorldMode->World->Arena, "Completed");
    Quest->QuestTextID.Value = 0;

    Quest->Location = CenteredTilePoint(75, 236);

    Quest->Completed = false;
    ZeroArray(8, Quest->IsCompleted);

    Quest->QuestGiverNPC.Value = 0;
    Quest->RewardCondition = RewardCondition_WhenCompleted;

    Quest->ComplitionType[0] = ComplitionType_Talk;
    Quest->CompRequirements[0].TalkToNPC.NPCToTalk = ElderTavor;
    ++Quest->RequirementsCount;

    Quest->RewardType[0] = RewardType_TalkingGiver;
    Quest->Reward[0].TalkingGiverID = ElderTavor;
    ++Quest->RewardCount;
    ++WorldMode->QuestCount;
    
    // NOTE(paul): QuestName_TheLostTome
    asset_vector QuestMatchVector = {};
    asset_vector QuestWeightVector = {};
    QuestMatchVector.E[Tag_NPCName] = (r32)NPCName_ElderTavor;
    QuestMatchVector.E[Tag_QuestType] = (r32)QuestType_Main;
    QuestMatchVector.E[Tag_QuestName] = (r32)QuestName_TheLostTome;

    QuestWeightVector.E[Tag_NPCName] = 1.0f;
    QuestWeightVector.E[Tag_QuestType] = 1.0f;
    QuestWeightVector.E[Tag_QuestName] = 1.0f;

    quest *Quest0 = &WorldMode->Quests[QuestName_TheLostTome];
    Quest0->Type = QuestType_Main;
    Quest0->QuestName = QuestName_TheLostTome;
    Quest0->UnCompletedText = PushString(&WorldMode->World->Arena, "Find and kill all monsters");
    Quest0->CompletedText = PushString(&WorldMode->World->Arena, "Return to Elder Tavor");
    Quest0->QuestTextID = GetBestMatchQuestFrom(TranState->Assets, Asset_Quest,
                                               &QuestMatchVector, &QuestWeightVector);
    Quest0->Completed = false;
    ZeroArray(8, Quest0->IsCompleted);

    Quest0->QuestGiverNPC = ElderTavor;

    Quest0->RewardCondition = RewardCondition_TalkToGiver;

    Quest0->ComplitionType[0] = ComplitionType_Kill;
    Quest0->CompRequirements[0].KillMonsters.MonsterCount = 4;
    Quest0->CompRequirements[0].KillMonsters.MonstersToKill[0] = AddNecromancer(WorldMode, TranState->Assets, 77, 18);
    Quest0->CompRequirements[0].KillMonsters.MonstersToKill[1] = AddCultist(WorldMode, TranState->Assets, 71, 17);
    Quest0->CompRequirements[0].KillMonsters.MonstersToKill[2] = AddCultist(WorldMode, TranState->Assets, 75, 23);
    Quest0->CompRequirements[0].KillMonsters.MonstersToKill[3] = AddCultist(WorldMode, TranState->Assets, 75, 15);
    ++Quest0->RequirementsCount;

    Quest0->Location = CenteredTilePoint(75, 18);
    Quest0->GiverLocation = CenteredTilePoint(75, 236);
   
    Quest0->RewardType[0] = RewardType_Quest;
    Quest0->Reward[0].QuestID = QuestName_FindHerbalist;
    ++Quest0->RewardCount;
    ++WorldMode->QuestCount;

    // NOTE(paul): QuestName_FindHerbalist
    quest *Quest1 = &WorldMode->Quests[QuestName_FindHerbalist];
    Quest1->Type = QuestType_Main;
    Quest1->QuestName = QuestName_FindHerbalist;
    Quest1->UnCompletedText = PushString(&WorldMode->World->Arena, "Find and talk to herbalist");
    Quest1->CompletedText = PushString(&WorldMode->World->Arena, "Completed");
    Quest1->QuestTextID.Value = 0;

    Quest1->Location = CenteredTilePoint(8, 193);

    Quest1->Completed = false;
    ZeroArray(8, Quest1->IsCompleted);

    Quest1->QuestGiverNPC = ElderTavor;
    Quest1->RewardCondition = RewardCondition_WhenCompleted;

    Quest1->ComplitionType[0] = ComplitionType_Talk;
    Quest1->CompRequirements[0].TalkToNPC.NPCToTalk = HerbalistElara;
    ++Quest1->RequirementsCount;

    Quest1->ComplitionType[1] = ComplitionType_Quest;
    Quest1->CompRequirements[1].FinishedQuest.QuestID = QuestName_TheLostTome;
    ++Quest1->RequirementsCount;

    Quest1->RewardType[0] = RewardType_TalkingGiver;
    Quest1->Reward[0].TalkingGiverID = HerbalistElara;
    ++Quest1->RewardCount;
    ++WorldMode->QuestCount;

    // NOTE(paul): QuestName_HerbalistsPlea
    asset_vector QuestMatchVector0 = {};
    asset_vector QuestWeightVector0 = {};
    QuestMatchVector0.E[Tag_NPCName] = (r32)NPCName_Elara;
    QuestMatchVector0.E[Tag_QuestType] = (r32)QuestType_Main;
    QuestMatchVector0.E[Tag_QuestName] = (r32)QuestName_HerbalistsPlea;

    QuestWeightVector0.E[Tag_NPCName] = 1.0f;
    QuestWeightVector0.E[Tag_QuestType] = 1.0f;
    QuestWeightVector0.E[Tag_QuestName] = 1.0f;

    quest *Quest2 = &WorldMode->Quests[QuestName_HerbalistsPlea];
    Quest2->Type = QuestType_Main;
    Quest2->QuestName = QuestName_HerbalistsPlea;
    Quest2->UnCompletedText = PushString(&WorldMode->World->Arena, "Find and kill all monsters");
    Quest2->CompletedText = PushString(&WorldMode->World->Arena, "Return to Elara");
    Quest2->QuestTextID = GetBestMatchQuestFrom(TranState->Assets, Asset_Quest,
                                               &QuestMatchVector0, &QuestWeightVector0);
    Quest0->Completed = false;
    ZeroArray(8, Quest2->IsCompleted);

    Quest2->QuestGiverNPC = HerbalistElara;

    Quest2->RewardCondition = RewardCondition_TalkToGiver;

    Quest2->ComplitionType[0] = ComplitionType_Kill;
    Quest2->CompRequirements[0].KillMonsters.MonsterCount = 4;
    Quest2->CompRequirements[0].KillMonsters.MonstersToKill[0] = AddGoblinBeast(WorldMode, TranState->Assets, 8, 133);
    Quest2->CompRequirements[0].KillMonsters.MonstersToKill[1] = AddGoblinBerserker(WorldMode, TranState->Assets, 14, 134);
    Quest2->CompRequirements[0].KillMonsters.MonstersToKill[2] = AddGoblinRider(WorldMode, TranState->Assets, 8, 129);
    Quest2->CompRequirements[0].KillMonsters.MonstersToKill[3] = AddGoblinBerserker(WorldMode, TranState->Assets, 13, 133);
    ++Quest2->RequirementsCount;

    Quest2->Location = CenteredTilePoint(11, 132);
    Quest2->GiverLocation = CenteredTilePoint(8, 193);
   
    Quest2->RewardType[0] = RewardType_Quest;
    Quest2->Reward[0].QuestID = QuestName_FindJacob;
    ++Quest2->RewardCount;
    ++WorldMode->QuestCount;

    // NOTE(paul): QuestName_FindJacob
    quest *Quest3 = &WorldMode->Quests[QuestName_FindJacob];
    Quest3->Type = QuestType_Main;
    Quest3->QuestName = QuestName_FindJacob;
    Quest3->UnCompletedText = PushString(&WorldMode->World->Arena, "Find and talk to Jacob");
    Quest3->CompletedText = PushString(&WorldMode->World->Arena, "Completed");
    Quest3->QuestTextID.Value = 0;

    Quest3->Location = CenteredTilePoint(99, 133);

    Quest3->Completed = false;
    ZeroArray(8, Quest3->IsCompleted);

    Quest3->QuestGiverNPC = HerbalistElara;
    Quest3->RewardCondition = RewardCondition_WhenCompleted;

    Quest3->ComplitionType[0] = ComplitionType_Talk;
    Quest3->CompRequirements[0].TalkToNPC.NPCToTalk = Jacob;
    ++Quest3->RequirementsCount;

    Quest3->ComplitionType[1] = ComplitionType_Quest;
    Quest3->CompRequirements[1].FinishedQuest.QuestID = QuestName_HerbalistsPlea;
    ++Quest3->RequirementsCount;

    Quest3->RewardType[0] = RewardType_TalkingGiver;
    Quest3->Reward[0].TalkingGiverID = Jacob;
    ++Quest3->RewardCount;
    ++WorldMode->QuestCount;

    // NOTE(paul): QuestName_JacobTalk
    asset_vector QuestMatchVector1 = {};
    asset_vector QuestWeightVector1 = {};
    QuestMatchVector1.E[Tag_NPCName] = (r32)NPCName_Jacob;
    QuestMatchVector1.E[Tag_QuestType] = (r32)QuestType_Main;
    QuestMatchVector1.E[Tag_QuestName] = (r32)QuestName_JacobTalk;

    QuestWeightVector1.E[Tag_NPCName] = 1.0f;
    QuestWeightVector1.E[Tag_QuestType] = 1.0f;
    QuestWeightVector1.E[Tag_QuestName] = 1.0f;

    quest *Quest4 = &WorldMode->Quests[QuestName_JacobTalk];
    Quest4->Type = QuestType_Main;
    Quest4->QuestName = QuestName_JacobTalk;
    Quest4->UnCompletedText = PushString(&WorldMode->World->Arena, "Talk to Jacob");
    Quest4->CompletedText = PushString(&WorldMode->World->Arena, "Talk to Jacob");
    Quest4->QuestTextID = GetBestMatchQuestFrom(TranState->Assets, Asset_Quest,
                                               &QuestMatchVector1, &QuestWeightVector1);
    Quest4->Location = CenteredTilePoint(99, 133);
    Quest4->Completed = false;
    ZeroArray(8, Quest4->IsCompleted);

    Quest4->QuestGiverNPC = Jacob;

    Quest4->RewardCondition = RewardCondition_TalkToGiver;

    Quest4->ComplitionType[0] = ComplitionType_Talk;
    Quest4->CompRequirements[0].TalkToNPC.NPCToTalk = Jacob;
    ++Quest4->RequirementsCount;
    Quest4->ComplitionType[1] = ComplitionType_Quest;
    Quest4->CompRequirements[1].FinishedQuest.QuestID = QuestName_FindJacob;
    ++Quest4->RequirementsCount;
    
    Quest4->RewardType[0] = RewardType_Quest;
    Quest4->Reward[0].QuestID = QuestName_SkeletonKing;
    ++Quest4->RewardCount;
    Quest4->RewardType[1] = RewardType_DestroyObstacle;

    asset_vector ObstacleMatchVector0 = {};
    ObstacleMatchVector0.E[Tag_BiomeType] = (r32)BiomeType_AncientForest;
    ObstacleMatchVector0.E[Tag_TreeType] = (r32)TreeType_Fallen;
    ObstacleMatchVector0.E[Tag_Variety] = (r32)VarietyType_0;

    asset_vector ObstacleWeightVector0 = {};
    ObstacleWeightVector0.E[Tag_BiomeType] = 1.0f;
    ObstacleWeightVector0.E[Tag_TreeType] = 1.0f;
    ObstacleWeightVector0.E[Tag_Variety] = 1.0f;

    Quest4->Reward[1].Obstacles[0] = AddObstacle(WorldMode, TranState->Assets, 13, 18,
                                                 Asset_Tree, &ObstacleMatchVector0, &ObstacleWeightVector0,
                                                 4.0f, V2(4.0f, 1.0f), V3(3.5f, 0, 0));
    ++Quest4->RewardCount;
    ++WorldMode->QuestCount;

    // NOTE(paul): QuestName_SkeletonKing
    quest *Quest5 = &WorldMode->Quests[QuestName_SkeletonKing];
    Quest5->Type = QuestType_Main;
    Quest5->QuestName = QuestName_SkeletonKing;
    Quest5->UnCompletedText = PushString(&WorldMode->World->Arena, "Kill Skeleton King");
    Quest5->CompletedText = PushString(&WorldMode->World->Arena, "Completed");
    Quest5->QuestTextID.Value = 0;

    Quest5->Completed = false;
    ZeroArray(8, Quest5->IsCompleted);

    Quest5->QuestGiverNPC = Jacob;
    Quest5->RewardCondition = RewardCondition_WhenCompleted;

    Quest5->ComplitionType[0] = ComplitionType_Kill;
    Quest5->CompRequirements[0].KillMonsters.MonsterCount = 5;
    Quest5->CompRequirements[0].KillMonsters.MonstersToKill[0] = AddSkeletonKing(WorldMode, TranState->Assets, 17, 7);
    Quest5->CompRequirements[0].KillMonsters.MonstersToKill[1] = AddSkeletonHunter(WorldMode, TranState->Assets, 7, 7);
    Quest5->CompRequirements[0].KillMonsters.MonstersToKill[2] = AddSkeletonHunter(WorldMode, TranState->Assets, 24, 6);
    Quest5->CompRequirements[0].KillMonsters.MonstersToKill[3] = AddSkeletonGrunt(WorldMode, TranState->Assets, 14, 7);
    Quest5->CompRequirements[0].KillMonsters.MonstersToKill[4] = AddSkeletonGrunt(WorldMode, TranState->Assets, 20, 8);
    ++Quest5->RequirementsCount;

    Quest5->Location = CenteredTilePoint(16, 7);

    Quest5->ComplitionType[1] = ComplitionType_Find;
    Quest5->CompRequirements[1].FindItem.Name = ItemName_MapToTheTrees;
    ++Quest5->RequirementsCount;

    Quest5->RewardType[0] = RewardType_Quest;
    Quest5->Reward[0].QuestID = QuestName_HidenMap;
    ++Quest5->RewardCount;
    Quest5->RewardType[1] = RewardType_DestroyObstacle;

    asset_vector ObstacleMatchVector = {};
    ObstacleMatchVector.E[Tag_BiomeType] = (r32)BiomeType_AncientForest;
    ObstacleMatchVector.E[Tag_SizeLevel] = (r32)SizeLevel_0;
    asset_vector ObstacleWeightVector = {};
    ObstacleWeightVector.E[Tag_BiomeType] = 1.0f;
    ObstacleWeightVector.E[Tag_SizeLevel] = 1.0f;

    Quest5->Reward[1].Obstacles[0] = AddObstacle(WorldMode, TranState->Assets, 153, 87,
                                                 Asset_Stone, &ObstacleMatchVector, &ObstacleWeightVector,
                                                 2.0f, V2(1.0f, 1.0f));
    Quest5->Reward[1].Obstacles[1] = AddObstacle(WorldMode, TranState->Assets, 154, 87,
                                                 Asset_Stone, &ObstacleMatchVector, &ObstacleWeightVector,
                                                 2.0f, V2(1.0f, 1.0f));
    Quest5->Reward[1].Obstacles[2] = AddObstacle(WorldMode, TranState->Assets, 155, 87,
                                                 Asset_Stone, &ObstacleMatchVector, &ObstacleWeightVector,
                                                 2.0f, V2(1.0f, 1.0f));
    Quest5->Reward[1].Obstacles[3] = AddObstacle(WorldMode, TranState->Assets, 156, 87,
                                                 Asset_Stone, &ObstacleMatchVector, &ObstacleWeightVector,
                                                 2.0f, V2(1.0f, 1.0f));
    Quest5->Reward[1].Obstacles[4] = AddObstacle(WorldMode, TranState->Assets, 157, 87,
                                                 Asset_Stone, &ObstacleMatchVector, &ObstacleWeightVector,
                                                 2.0f, V2(1.0f, 1.0f));
    ++Quest5->RewardCount;
    ++WorldMode->QuestCount;

    //NOTE(paul): QuestName_HidenMap
    quest *Quest6 = &WorldMode->Quests[QuestName_HidenMap];
    Quest6->Type = QuestType_Main;
    Quest6->QuestName = QuestName_HidenMap;
    Quest6->UnCompletedText = PushString(&WorldMode->World->Arena, "Find Three Ancient Trees");
    Quest6->CompletedText = PushString(&WorldMode->World->Arena, "Completed");
    Quest6->QuestTextID.Value = 0;
    Quest5->Location = CenteredTilePoint(16, 7);

    Quest6->Completed = false;
    ZeroArray(8, Quest6->IsCompleted);

    Quest6->QuestGiverNPC.Value = 0;
    Quest6->RewardCondition = RewardCondition_WhenCompleted;

    Quest6->ComplitionType[0] = ComplitionType_Find;
    Quest6->CompRequirements[0].FindItem.Name = ItemName_Relic;

    world_position P = CenteredTilePoint(158, 95);
    AddItem(WorldMode, TranState->Assets, P, ItemName_Relic);
    Quest6->Location = CenteredTilePoint(158, 95);

    ++Quest6->RequirementsCount;

    Quest6->RewardType[0] = RewardType_GameEnd;
    ++Quest6->RewardCount;
    ++WorldMode->QuestCount;

    WorldMode->MovePointMaxHeap.MaxSize = 8;
    WorldMode->MovePointMaxHeap.Size = 0;
    WorldMode->MovePointMaxHeap.Nodes = PushArray(&WorldMode->World->Arena, WorldMode->MovePointMaxHeap.MaxSize, sort_entry);
    
    // NOTE(paul): Create Mini Map
    if(!TranState->MiniMap.TextureHandle)
    {
        CreateMiniMap(WorldMode, TranState);
    }
}

#include "spellweaver_world_mode.cpp"
#include "spellweaver_title_mode.cpp"
#include "spellweaver_cutscene.cpp"
#include "a_star_test.cpp"

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

        TranState->Assets = AllocateGameAssets(&TranState->TranArena, Megabytes(512), TranState);

        uint32 GroundBufferWidth = 128; 
        uint32 GroundBufferHeight = 128;
        
        TranState->MiniMap = MakeEmptyBitmap(&TranState->TranArena, 4096, 4096, false);

        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        WeightVector.E[Tag_BiomeType] = 1.0f;
        WeightVector.E[Tag_TileMainSurface] = 1.0f;
        WeightVector.E[Tag_TileMergeSurface] = 1.0f;

        MatchVector.E[Tag_BiomeType] = (r32)BiomeType_Global;
        MatchVector.E[Tag_TileMainSurface] = (r32)TileSurface_Global;
        MatchVector.E[Tag_TileMergeSurface] = (r32)TileSurface_Global;
        tileset_id ID = GetBestMatchTilesetFrom(TranState->Assets, Asset_Tileset,
                                                &MatchVector, &WeightVector);
        TranState->GlobalTilesetID = ID;

        GameState->CursorBitmapHover = GetFirstBitmapFrom(TranState->Assets, Asset_CursorHover);
        GameState->CursorBitmapClick = GetFirstBitmapFrom(TranState->Assets, Asset_CursorClick);

        asset_vector MusicWeightVector = {};
        MusicWeightVector.E[Tag_MusicType] = 1.0f;
        MusicWeightVector.E[Tag_Variety] = 1.0f;
        asset_vector MusicMatchVector = {};
        MusicMatchVector.E[Tag_MusicType] = (r32)MusicType_Ambient;

        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_0;
        GameState->AmbientMusic[0] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_1;
        GameState->AmbientMusic[1] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_2;
        GameState->AmbientMusic[2] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_3;
        GameState->AmbientMusic[3] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_4;
        GameState->AmbientMusic[4] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_5;
        GameState->AmbientMusic[5] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_6;
        GameState->AmbientMusic[6] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_7;
        GameState->AmbientMusic[7] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_8;
        GameState->AmbientMusic[8] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_9;
        GameState->AmbientMusic[9] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_10;
        GameState->AmbientMusic[10] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_11;
        GameState->AmbientMusic[11] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_12;
        GameState->AmbientMusic[12] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_13;
        GameState->AmbientMusic[13] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_14;
        GameState->AmbientMusic[14] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

        MusicMatchVector.E[Tag_MusicType] = (r32)MusicType_DarkAmbient;
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_0;
        GameState->DarkAmbientMusic[0] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_1;
        GameState->DarkAmbientMusic[1] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_2;
        GameState->DarkAmbientMusic[2] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_3;
        GameState->DarkAmbientMusic[3] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_4;
        GameState->DarkAmbientMusic[4] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

        MusicMatchVector.E[Tag_MusicType] = (r32)MusicType_Action;
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_0;
        GameState->ActionMusic[0] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_1;
        GameState->ActionMusic[1] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_2;
        GameState->ActionMusic[2] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_3;
        GameState->ActionMusic[3] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);
        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_4;
        GameState->ActionMusic[4] = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

        MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_None;
        MusicMatchVector.E[Tag_MusicType] = (r32)MusicType_GameStartFX;
        GameState->GameStartFX = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

        MusicMatchVector.E[Tag_MusicType] = (r32)MusicType_GameEndDeathFX;
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
    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);

    // TODO(casey): Decide what our pushbuffer size is!
    render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands,
                                                 TranState->MainGenerationID, false);
    render_group *RenderGroup = &RenderGroup_;

    // TODO(casey): Eliminate these entirely
    loaded_bitmap DrawBuffer = {};
    DrawBuffer.Width = RenderCommands->Width;
    DrawBuffer.Height = RenderCommands->Height;

    Orthographic(RenderGroup, RenderCommands->Width, RenderCommands->Height, 1.0f);
    object_transform MouseTransform = DefaultFlatTransform();
    MouseTransform.SortBias = 1200000.0f;
    v3 MouseP = Unproject(RenderGroup, MouseTransform, V2(Input->MouseX, Input->MouseY));

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
        Transform.SortBias = 1100000.0f;
        PushRect(RenderGroup, Transform, V3(0, 0, 0), V2i(RenderCommands->Width, RenderCommands->Height),
                 V4(0, 0, 0, GameState->CurrentAlpha));
    }

    PushBitmap(RenderGroup, MouseTransform, MouseBitmap, 25.0f, MouseP);
    
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

            case GameMode_Test:
            {
                Rerun = UpdateAndRenderAStarTest(GameState, TranState, RenderGroup, Input, GameState->AStarTest,
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


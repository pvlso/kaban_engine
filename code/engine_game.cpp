/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#include "engine_game.h"

#include "spellweaver\spellweaver_text.cpp"
#include "spellweaver\spellweaver_world.cpp"
#include "spellweaver\spellweaver_sim_region.cpp"
#include "spellweaver\spellweaver_entity.cpp"

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

        GameState->Music = PlaySound(GameState->AudioState, Music);
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
            MuteAndTerminateSound(GameState->AudioState, GameState->Music, 1.0f);
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
PlayWorld(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_World);
    GameState->FadeState = FadeState_FadeOut;

    PlaySound(GameState->AudioState, GameState->GameStartFX);
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
    WorldMode->GeneralTextConfig.TextTransform.ChunkZ = 1000000;
    WorldMode->GeneralTextConfig.TextShadowTransform.OffsetP = V3(0.0f, 0.0f, 0.0f);
    WorldMode->GeneralTextConfig.TextShadowTransform.ChunkZ = 100000;
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
    QuestMatchVector.E[Tag_NPCName] = NPCName_ElderTavor;
    QuestMatchVector.E[Tag_QuestType] = QuestType_Main;
    QuestMatchVector.E[Tag_QuestName] = QuestName_TheLostTome;

    QuestWeightVector.E[Tag_NPCName] = 1;
    QuestWeightVector.E[Tag_QuestType] = 1;
    QuestWeightVector.E[Tag_QuestName] = 1;

    quest *Quest0 = &WorldMode->Quests[QuestName_TheLostTome];
    Quest0->Type = QuestType_Main;
    Quest0->QuestName = QuestName_TheLostTome;
    Quest0->UnCompletedText = PushString(&WorldMode->World->Arena, "Find and kill all monsters");
    Quest0->CompletedText = PushString(&WorldMode->World->Arena, "Return to Elder Tavor");
//    Quest0->QuestTextID = GetBestMatchQuestFrom(TranState->Assets, Asset_Quest,
//                                               &QuestMatchVector, &QuestWeightVector);
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
    QuestMatchVector0.E[Tag_NPCName] = NPCName_Elara;
    QuestMatchVector0.E[Tag_QuestType] = QuestType_Main;
    QuestMatchVector0.E[Tag_QuestName] = QuestName_HerbalistsPlea;

    QuestWeightVector0.E[Tag_NPCName] = 1;
    QuestWeightVector0.E[Tag_QuestType] = 1;
    QuestWeightVector0.E[Tag_QuestName] = 1;

    quest *Quest2 = &WorldMode->Quests[QuestName_HerbalistsPlea];
    Quest2->Type = QuestType_Main;
    Quest2->QuestName = QuestName_HerbalistsPlea;
    Quest2->UnCompletedText = PushString(&WorldMode->World->Arena, "Find and kill all monsters");
    Quest2->CompletedText = PushString(&WorldMode->World->Arena, "Return to Elara");
//    Quest2->QuestTextID = GetBestMatchQuestFrom(TranState->Assets, Asset_Quest,
//                                               &QuestMatchVector0, &QuestWeightVector0);
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
    QuestMatchVector1.E[Tag_NPCName] = NPCName_Jacob;
    QuestMatchVector1.E[Tag_QuestType] = QuestType_Main;
    QuestMatchVector1.E[Tag_QuestName] = QuestName_JacobTalk;

    QuestWeightVector1.E[Tag_NPCName] = 1;
    QuestWeightVector1.E[Tag_QuestType] = 1;
    QuestWeightVector1.E[Tag_QuestName] = 1;

    quest *Quest4 = &WorldMode->Quests[QuestName_JacobTalk];
    Quest4->Type = QuestType_Main;
    Quest4->QuestName = QuestName_JacobTalk;
    Quest4->UnCompletedText = PushString(&WorldMode->World->Arena, "Talk to Jacob");
    Quest4->CompletedText = PushString(&WorldMode->World->Arena, "Talk to Jacob");
//    Quest4->QuestTextID = GetBestMatchQuestFrom(TranState->Assets, Asset_Quest,
//                                               &QuestMatchVector1, &QuestWeightVector1);
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
    ObstacleMatchVector0.E[Tag_BiomeType] = BiomeType_AncientForest;
    ObstacleMatchVector0.E[Tag_TreeType] = TreeType_Fallen;
    ObstacleMatchVector0.E[Tag_Variety] = VarietyType_0;

    asset_vector ObstacleWeightVector0 = {};
    ObstacleWeightVector0.E[Tag_BiomeType] = 1;
    ObstacleWeightVector0.E[Tag_TreeType] = 1;
    ObstacleWeightVector0.E[Tag_Variety] = 1;

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
    ObstacleMatchVector.E[Tag_BiomeType] = BiomeType_AncientForest;
    ObstacleMatchVector.E[Tag_SizeLevel] = SizeLevel_0;
    asset_vector ObstacleWeightVector = {};
    ObstacleWeightVector.E[Tag_BiomeType] = 1;
    ObstacleWeightVector.E[Tag_SizeLevel] = 1;

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
//    if(!TranState->MiniMap.TextureHandle)
    {
//        CreateMiniMap(WorldMode, TranState);
    }
}

#include "spellweaver\spellweaver_world_mode.cpp"
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
            GameState->AudioState = &EditorState->AudioState;
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

            GameTranState->Assets = AllocateEditorAssets(&GameTranState->TranArena, Megabytes(64),
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
#if 0
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
#endif
        
//        Platform.WriteLogFile(L"Transient State Initialized", __FILE__, __LINE__);
            GameTranState->IsInitialized = true;
        }

        {DEBUG_DATA_BLOCK("Memory");
            memory_arena *GameModeArena = &GameState->ModeArena;
            DEBUG_VALUE(GameModeArena);
        
            memory_arena *GameTranArena_ = &GameTranState->TranArena;
            DEBUG_VALUE(GameTranArena_);
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
            MusicVolume.y = SafeRatio0((r32)Input->MouseX, (r32)RenderCommands->Width);
            MusicVolume.x = 1.0f - MusicVolume.y;
            ChangeVolume(GameState->AudioState, GameState->Music, 0.01f, MusicVolume);
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
                    Rerun = UpdateAndRenderTitleScreen(GameState, GameTranState, TranState, RenderGroup, &DrawBuffer,
                                                       Input, GameState->TitleScreen);
                } break;

                case GameMode_World:
                {
                    Rerun = UpdateAndRenderWorld(GameState, GameState->WorldMode, TranState, GameTranState, Input, RenderGroup,
                                                 &DrawBuffer);
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

        if(Input->QuitRequested)
        {
            EditorState->SimulationQuit = true;;
            MuteAndTerminateSound(GameState->AudioState, GameState->Music, 3.0f);
            Input->QuitRequested = false;
        }
    }
    
    return(Result);
}

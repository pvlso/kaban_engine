/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

internal void
PlayWorld(game_state *GameState, game_transient_state *TranState)
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

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    MatchVector.E[Tag_VersionMajorHigh] = 0;
    MatchVector.E[Tag_VersionMajorLow] = 0;
    MatchVector.E[Tag_VersionMinorHigh] = 0;
    MatchVector.E[Tag_VersionMinorLow] = 0;
    WeightVector.E[Tag_VersionMajorHigh] = 2;
    WeightVector.E[Tag_VersionMajorLow] = 2;
    WeightVector.E[Tag_VersionMinorHigh] = 2;
    WeightVector.E[Tag_VersionMinorLow] = 2;

    sswm_id SSWMID = GetBestMatchSSWMFrom(TranState->Assets, Asset_SSWM, &MatchVector, &WeightVector);

    loaded_world_map *Map = PushSSWM(TranState->Assets, TranState->MainGenerationID, SSWMID, true);

    editor_assets *Assets = TranState->Assets;

    u32 ZLayerCount = Map->Header->GroundLayer_ZLayerCount & 0xFFFF;
    for(u32 TileIndex = 0;
        TileIndex < Map->Header->MapWidth*Map->Header->MapHeight;
        ++TileIndex)
    {
        sswm_ground_tile *Tile = Map->GroundTiles + TileIndex;
        for(u32 BitmapIndex = 0;
            BitmapIndex < ZLayerCount;
            ++BitmapIndex)
        {
            u32 CheckSum = Tile->CheckSum[BitmapIndex];
            bitmap_id ID = GetTileBitmapByChecksumTag(TranState->Assets, CheckSum);
            Tile->BitmapID[BitmapIndex] = ID.Value;
        }
    }

    WorldMode->World = CreateWorld(TileSideInMeters, Map);
    world *World = WorldMode->World;
    
    WorldMode->MiniMapBitmap = MakeEmptyBitmap(&WorldMode->World->Arena, 720, 720, false);

    WorldMode->NullCollision = MakeNullCollision(WorldMode);
    WorldMode->SphereCollision = MakeSimpleGroundedCollision(WorldMode, 0.5f, 0.5f, 0.5f);
    WorldMode->PlayerCollision = MakeSimpleGroundedCollision(WorldMode, 0.8f, 0.5f, 1.2f);
    WorldMode->TileCollision = MakeSimpleGroundedCollision(WorldMode, World->TileDimInMeters.x,
                                                           World->TileDimInMeters.y,
                                                           World->TileDimInMeters.z);

    WorldMode->SpellCollision = MakeSimpleGroundedCollision(WorldMode, 0.5f, 0.5f, 0.5f);
    WorldMode->GolemCollision = MakeSimpleGroundedCollision(WorldMode, 1.4f, 0.7f, 1.5f);

    WorldMode->TileMap = PushArray(&World->Arena, World->TileCount, entity_id);
    for(u32 TileIndex = 0;
        TileIndex < World->TileCount;
        ++TileIndex)
    {
        s32 HighestZ = 0;
        sswm_ground_tile *SourceTile = Map->GroundTiles + TileIndex;
        for(u32 BitmapIDIndex = 0;
            BitmapIDIndex < ArrayCount(SourceTile->BitmapID);
            ++BitmapIDIndex)
        {
            if(SourceTile->BitmapID[BitmapIDIndex] != 0)
            {
                if(HighestZ < (s32)BitmapIDIndex)
                {
                    HighestZ = BitmapIDIndex;
                }
            }
        }

        WorldMode->TileMap[TileIndex] = AddTile(WorldMode, WorldMode->TileCollision, false, SourceTile, HighestZ);
    }

//    AddGolem(WorldMode, TranState->Assets, CenteredTilePoint(World, 12, 12));

    //
    // NOTE(paul): Camera Setup
    //

    world_position NewCameraP = {};
    uint32 CameraTileX = World->TileWidth / 2;
    uint32 CameraTileY = World->TileHeight / 2;
    NewCameraP = ChunkPositionFromTilePosition(World, CameraTileX, CameraTileY);

    WorldMode->CameraBoundsMin.TileX = 0;
    WorldMode->CameraBoundsMin.TileY = 0;
    WorldMode->CameraBoundsMin.Offset = V2(-0.5f, -0.5f);
    
    WorldMode->CameraBoundsMax.TileX = World->TileWidth;
    WorldMode->CameraBoundsMax.TileY = World->TileHeight;
    WorldMode->CameraBoundsMax.Offset = V2(0.5f, 0.5f);
    WorldMode->CameraP = NewCameraP;
    
    GameState->WorldMode = WorldMode;
}

internal void
UpdateAndRenderGroundTiles(render_group *RenderGroup, world *World, world_position CameraP, rectangle2 CameraBoundsInMeters)
{
    TIMED_FUNCTION();
#if 0
    world_position MinTileP = MapIntoTileSpace(World, CameraP, GetMinCorner(CameraBoundsInMeters));
    world_position MaxTileP = MapIntoTileSpace(World, CameraP, GetMaxCorner(CameraBoundsInMeters));
    v2 CameraDim = GetDim(CameraBoundsInMeters);
    v2 CameraHalfDim = 0.5f*CameraDim;

    object_transform Transform = DefaultFlatTransform();
    for(s32 TileY = MinTileP.TileY;
        TileY <= MaxTileP.TileY;
        ++TileY)
    {
        for(s32 TileX = MinTileP.TileX;
            TileX <= MaxTileP.TileX;
            ++TileX)
        {
            world_position TileP = CenteredTilePoint(World, TileX, TileY);
            if(((u32)TileX < World->TileWidth) && ((u32)TileY < WORLD_TILE_COUNT_PER_DIM))
            {
                v2 Delta = Subtract(World, &TileP, &CameraP);

                sswm_ground_tile *Tile = World->Map->GroundTiles + TileY*World->Map->Header->MapWidth + TileX;
                u32 ZLayerCount = World->Map->Header->GroundLayer_ZLayerCount & 0xFFFF;
                for(u32 BitmapIndex = 0;
                    BitmapIndex < ZLayerCount;
                    ++BitmapIndex)
                {
                    bitmap_id ID = {Tile->BitmapID[BitmapIndex]};
                    PushBitmap(RenderGroup, &Transform, ID, World->TileDimInMeters.y, V3(Delta, 0.0f));
                    Transform.ChunkZ += 1;
                }

                Transform.ChunkZ -= ZLayerCount;
                
#if SPELLWEAVER_INTERNAL
                PushRectOutline(RenderGroup, &Transform, V3(Delta, 2.0f), World->TileDimInMeters.xy,
                                V4(1.0f, 1.0f, 0.0f, 1.0f), 0.02f);
#endif
            }
        }
    }
#endif
}

internal void
DestroyEntities(game_mode_world *WorldMode, sim_region *SimRegion)
{
    for(u32 EntityIndex = 0;
        EntityIndex < ArrayCount(WorldMode->EntitiesToDestroy);
        ++EntityIndex)
    {
        entity_id ID = WorldMode->EntitiesToDestroy[EntityIndex];
        if(ID.Value)
        {
            entity *Entity = GetEntityByID(SimRegion, ID);
            if(Entity)
            {
                AddFlags(Entity, EntityFlag_Deleted);
                WorldMode->EntitiesToDestroy[EntityIndex].Value = 0;                
            }
        }
    }
}

internal void
DrawTileNodes(game_mode_world *WorldMode, transient_state *TranState, rectangle2 CameraBoundsInMeters,
              render_group *RenderGroup, as_tile_node *StartNode, as_tile_node *EndNode)
{
    TIMED_FUNCTION();

    world_position MinTileP = MapIntoTileSpace(WorldMode->World, WorldMode->CameraP,
                                               GetMinCorner(CameraBoundsInMeters));
    world_position MaxTileP = MapIntoTileSpace(WorldMode->World, WorldMode->CameraP,
                                               GetMaxCorner(CameraBoundsInMeters));

    v2 CameraDim = GetDim(CameraBoundsInMeters);
    v2 CameraHalfDim = 0.5f*CameraDim;

#if 0
    object_transform Transform = DefaultFlatTransform();
    for(int32 TileY = MinTileP.TileY;
        TileY <= MaxTileP.TileY;
        ++TileY)
    {
        for(int32 TileX = MinTileP.TileX;
            TileX <= MaxTileP.TileX;
            ++TileX)
        {
            world_position TileP = CenteredTilePoint(TileX, TileY);
            if((TileX < WORLD_TILE_COUNT_PER_DIM) && (TileY < WORLD_TILE_COUNT_PER_DIM))
            {
                as_tile_node *Node = GetTileNode(WorldMode->World, TileP);
                v2 Delta = Subtract(WorldMode->World, &TileP, &WorldMode->CameraP) - V2(0.5f, 0.5f);

                PushRectOutline(RenderGroup, Transform, V3(Delta + V2(0.5f, 0.5f), 2.0f),
                                V2(WorldMode->World->TileSideInMeters,
                                   WorldMode->World->TileSideInMeters),
                                V4(1.0f, 0.0f, 0.0f, 1.0f), 0.02f);

                for(u32 NIndex = 0;
                    NIndex < ArrayCount(Node->Neighbours);
                    ++NIndex)
                {
                    as_tile_node *NeighborNode = Node->Neighbours[NIndex];
                    if(NeighborNode)
                    {
                        v2 NDelta = Subtract(WorldMode->World, &NeighborNode->TileP, &WorldMode->CameraP) - V2(0.5f, 0.5f);
                        PushLine(RenderGroup, DefaultFlatTransform(),
                                 V3(Delta + V2(0.5f, 0.5f), 3.0f),
                                 V3(NDelta + V2(0.5f, 0.5f), 3.0f), V4(0, 0, 1, 1));
                    }
                }
            }
        }
    }
#endif

    if(EndNode)
    {
        as_tile_node *Node = EndNode;
        while(Node->Parent)
        {
            v2 Delta = Subtract(WorldMode->World, &Node->TileP, &WorldMode->CameraP) - V2(0.125f, 0.125f);
            v2 NDelta = Subtract(WorldMode->World, &Node->Parent->TileP, &WorldMode->CameraP) - V2(0.125f, 0.125f);

            object_transform Flat = DefaultFlatTransform();
            PushLine(RenderGroup, &Flat,
                     V3(NDelta + V2(0.125f, 0.125f), 4.0f), V3(Delta + V2(0.125f, 0.125f), 4.0f), V4(1, 1, 0, 1));

            Node = Node->Parent;
        }
    }
}

internal b32
UpdateAndRenderWorld(game_state *GameState, game_mode_world *WorldMode, game_transient_state *TranState,
                     engine_input *Input, render_group *RenderGroup, loaded_bitmap *DrawBuffer)
{
    TIMED_FUNCTION();
    
    b32 Result = false;

    {DEBUG_DATA_BLOCK("WorldMemory");
        memory_arena *WorldArena = &WorldMode->World->Arena;
        DEBUG_VALUE(WorldArena);
    }

    world *World = WorldMode->World;

    v2 MouseP = {Input->MouseX, Input->MouseY};

    real32 WidthOfMonitor = 0.635f; // NOTE(casey): Horizontal measurement of monitor in meters
    real32 MetersToPixels = (real32)DrawBuffer->Width/WidthOfMonitor;

    real32 FocalLength = 0.2f;
    real32 DistanceAboveGround = 7.2f;
    Perspective(RenderGroup, MetersToPixels, FocalLength, DistanceAboveGround);

    Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 1.0f));
    
    v2 ScreenCenter = {0.5f*(real32)DrawBuffer->Width,
                       0.5f*(real32)DrawBuffer->Height};

    rectangle2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);
    rectangle2 CameraBoundsInMeters = RectMinMax(ScreenBounds.Min, ScreenBounds.Max);

    // NOTE(casey): Ground tiles rendering

    UpdateAndRenderGroundTiles(RenderGroup, World, WorldMode->CameraP, CameraBoundsInMeters);

    //
    // NOTE(paul): Take Input
    //

    controlled_hero *ConHero = 0;
    for(int ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        engine_controller_input *Controller = GetController(Input, ControllerIndex);
        if(Controller->IsConnected)
        {
            ConHero = GameState->ControlledHeroes + ControllerIndex;
            if(ConHero->EntityIndex.Value == 0)
            {
                if(GameState->GameHaveStarted)
                {
                    *ConHero = {};
                    ConHero->EntityIndex = AddPlayer(WorldMode, TranState->Assets);
                    WorldMode->HeroExist = true;
                    GameState->GameHaveStarted = false;
                }
            }

            if(ConHero->EntityIndex.Value && WorldMode->HeroExist && !WorldMode->GameFinished)
            {
                ConHero->ddP = {};
                ConHero->SphereNewType = SphereType_Null;
                ConHero->InvokeAndCastSpell = false;
                ConHero->Attack = false;
                ConHero->Action = false;
                ConHero->Move = false;
            
                if(Controller->IsAnalog)
                {
                    // NOTE(casey): Use analog movement tuning
                    ConHero->ddP = V2(Controller->StickAverageX, Controller->StickAverageY);
                }
                else
                {
                }

                if(WasPressed(Input->MouseButtons[0]))
                    ConHero->Attack = true;

                if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
                    ConHero->Move = true;

                if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
                    ConHero->SetObstacle = true;

                if(WasPressed(Controller->RightShoulder))
                {
                    ConHero->Action = true;
                    PlaySound(GameState->AudioState, GetSoundEffectForType(TranState->Assets, SoundEffect_Click));
                }

                if(WasPressed(Controller->FirstMode))
                    ConHero->SphereNewType = SphereType_Water;
                else if(WasPressed(Controller->SecondMode))
                    ConHero->SphereNewType = SphereType_Wind;
                else if(WasPressed(Controller->ThirdMode))
                    ConHero->SphereNewType = SphereType_Fire;

                if(WasPressed(Controller->LeftShoulder))
                    ConHero->InvokeAndCastSpell = true;

                if(WasPressed(Controller->Back))
                {
                    WorldMode->QuitRequested = !WorldMode->QuitRequested;
                    PlaySound(GameState->AudioState, GetSoundEffectForType(TranState->Assets, SoundEffect_Click));
                }
            }
        }
    }
    
    // TODO(casey): How big do we actually want to expand here?
    // TODO(casey): Do we want to simulate upper floors, etc.?
    v2 SimBoundsExpansion = {5.0f, 5.0f};
    rectangle2 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundsExpansion);
    temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
    world_position SimCenterP = WorldMode->CameraP;
    sim_region *SimRegion = BeginSim(&TranState->TranArena, WorldMode->World,
                                     SimCenterP, SimBounds, Input->dtForFrame);
    
    v2 CameraP = Subtract(World, &WorldMode->CameraP, &SimCenterP);

//    DrawTileNodes(WorldMode, TranState, SimBounds, RenderGroup, WorldMode->StartNode, WorldMode->EndNode);
    
    object_transform Flat = DefaultFlatTransform();
#if SPELLWEAVER_INTERNAL    
    PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1), 0.05f);
    PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(SimBounds), V4(0.0f, 1.0f, 1.0f, 1));
    PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(SimRegion->Bounds), V4(1.0f, 0.0f, 1.0f, 1));
#endif

    object_transform BarsTransform = DefaultFlatTransform();
    u32 OldClipRect = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, &BarsTransform, ScreenBounds, 0.0f, 1);

    if(WorldMode->HeroExist)
    {
#if 0        
        bitmap_id HealthBar = GetFirstBitmapFrom(TranState->Assets, Asset_HeroHealthBar);
        bitmap_id SphereBar = GetFirstBitmapFrom(TranState->Assets, Asset_SphereBar);
        bitmap_id SpellBar = GetFirstBitmapFrom(TranState->Assets, Asset_SpellBar);
        bitmap_id QuestBorder = GetFirstBitmapFrom(TranState->Assets, Asset_QuestBorder);

        PushBitmap(RenderGroup, &BarsTransform, HealthBar, 1.8f, V3(-12.5f, 7.8f, 0));
        PushBitmap(RenderGroup, &BarsTransform, SphereBar, 1.0f, V3(0.0f, -8.0f, 0), V4(1, 1, 1, 0.95f));
        PushBitmap(RenderGroup, &BarsTransform, SpellBar, 6.75f, V3(-12.7f, -5.4f, 0), V4(1, 1, 1, 0.95f));
//        PushBitmap(RenderGroup, &BarsTransform, SpellBar, 6.75f, V3(0, 0, 0), V4(1, 1, 1, 0.95f));
#endif        
    }

    RenderGroup->CurrentClipRectIndex = OldClipRect; 
    PushBlendRenderTarget(RenderGroup, 0.0f, 1);
    
    {
        TIMED_BLOCK("EntityRender");
        UpdateAndRenderEntities(WorldMode, SimRegion, GameState->AudioState, ConHero, RenderGroup, Input->dtForFrame, MouseP);
                
        DestroyEntities(WorldMode, SimRegion);
        WorldMode->Time += Input->dtForFrame;
    }
    
    RenderGroup->GlobalAlpha = 1.0f;

    EndSim(WorldMode, SimRegion, CameraBoundsInMeters);
    EndTemporaryMemory(SimMemory);

    if(!WorldMode->HeroExist || WorldMode->QuitRequested || WorldMode->GameFinished)
    {
        b32 Hover = false;
        object_transform Transform = DefaultFlatTransform();
        Transform.ChunkZ = 100000;
        v4 RectColor = V4(0, 0, 0, 0.5f);
        if(WorldMode->GameFinished)
        {
            RectColor = V4(0, 0, 0, 0.1f);
        }

        PushRect(RenderGroup, &Transform, V3(0, 0, 0), V2(DrawBuffer->Width, DrawBuffer->Height), RectColor);
        Transform.OffsetP = V3(0, -4.0f, 0.0f);
            
        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        MatchVector.E[Tag_Variety] = VarietyType_4;
        WeightVector.E[Tag_Variety] = 2;
        bitmap_id TitleImage = GetBestMatchBitmapFrom(RenderGroup->Assets, Asset_TitleImage, &MatchVector, &WeightVector);

        v3 LocalMouseP = Unproject(RenderGroup, &Transform, MouseP);
        loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, TitleImage, RenderGroup->GenerationID);
        v3 AdditionalOffset = V3(0, 0, 0);
        if(Bitmap)
        {
            used_bitmap_dim Dim = GetBitmapDim(RenderGroup, &Transform, Bitmap, 2.0f, V3(0, 0, 0), 1.0f);
            rectangle2 HoverRect = RectCenterDim(Dim.P.xy + 0.5f*Dim.Size, Dim.Size);
            v4 Color = V4(0, 0, 1, 1);
            if(IsInRectangle(HoverRect, LocalMouseP.xy))
            {
                Color = V4(1, 0, 0, 1);
                AdditionalOffset = V3(0, 0.0f, 1.0f);
            }
        }

        if(AdditionalOffset.x || AdditionalOffset.y || AdditionalOffset.z)
        {
            Hover = true;
        }
            
        Transform.OffsetP += AdditionalOffset;
        PushBitmap(RenderGroup, &Transform, TitleImage, 2.0f, V3(0, 0, 0));

        if(WorldMode->QuitRequested)
        {
        }
        else if(WorldMode->GameFinished)
        {
            if(!WorldMode->GameEndMusic)
            {
                asset_vector MusicWeightVector = {};
                MusicWeightVector.E[Tag_MusicType] = 1;
                MusicWeightVector.E[Tag_Variety] = 1;
                asset_vector MusicMatchVector = {};
                MusicMatchVector.E[Tag_MusicType] = MusicType_GameVictoryMusic;
                MusicMatchVector.E[Tag_Variety] = VarietyType_None;
                sound_id MusicID = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

                WorldMode->GameEndMusic = PlaySound(GameState->AudioState, MusicID);
                ChangeVolume(GameState->AudioState, WorldMode->GameEndMusic, 2.0f, V2(0.7f, 0.7f));
            }
            else
            {
                ChangeVolume(GameState->AudioState, GameState->Music, 2.0f, V2(0.0f, 0.0f));
                if(!WorldMode->GameEndMusic->SoundIsPlaying)
                {
                    ChangeVolume(GameState->AudioState, GameState->Music, 2.0f, V2(0.5f, 0.5f));
                    GameState->FadeState = FadeState_FadeIn;
                    ChangeVolume(GameState->AudioState, GameState->Music, 2.0f, V2(0.5f, 0.5f));
                    MuteAndTerminateSound(GameState->AudioState, WorldMode->GameEndMusic, 2.0f);
                }
            }

            asset_vector MatchVector = {};
            asset_vector WeightVector = {};
            MatchVector.E[Tag_Variety] = VarietyType_5;
            WeightVector.E[Tag_Variety] = 1;

            Transform.OffsetP -= AdditionalOffset;
            bitmap_id TitleImage = GetBestMatchBitmapFrom(RenderGroup->Assets, Asset_TitleImage, &MatchVector, &WeightVector);
            PushBitmap(RenderGroup, &Transform, TitleImage, 5.0f, V3(0, 5.0f, 0));
            
        }
        else
        {
            if(GameState->MusicState != MusicState_DarkAmbient)
            {
                ChangeBackgroundMusic(GameState, MusicState_DarkAmbient);
                ChangeVolume(GameState->AudioState, GameState->Music, 2.0f, V2(1.0f, 1.0f));
            }
        }

        if(Hover && WasPressed(Input->MouseButtons[0]))
        {
            PlaySound(GameState->AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_Click));
            GameState->FadeState = FadeState_FadeIn;
            ChangeVolume(GameState->AudioState, GameState->Music, 2.0f, V2(0.5f, 0.5f));
            if(WorldMode->GameFinished)
            {
                MuteAndTerminateSound(GameState->AudioState, WorldMode->GameEndMusic, 2.0f);
            }
        }

        if(GameState->CurrentAlpha == 1.0f)
        {
            WorldMode->QuitRequested = false;
            GameState->GameHaveStarted = false;
            ConHero->EntityIndex.Value = 0;
            WorldMode->CameraFollowingEntityIndex.Value = 0;

            for(int ControllerIndex = 0;
                ControllerIndex < ArrayCount(Input->Controllers);
                ++ControllerIndex)
            {
                engine_controller_input *Controller = GetController(Input, ControllerIndex);
                controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;
                ZeroStruct(*ConHero);
            }

            PlayGameTitleScreen(GameState, TranState);
        }
    }    

    return(Result);
}


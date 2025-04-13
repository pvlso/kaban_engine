/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

internal void
UpdateAndRenderGroundTiles(game_mode_world *WorldMode, transient_state *TranState, rectangle2 CameraBoundsInMeters,
                            render_group *RenderGroup)
{
    TIMED_FUNCTION();

    world_position MinTileP = MapIntoTileSpace(WorldMode->World, WorldMode->CameraP,
                                                 GetMinCorner(CameraBoundsInMeters));
    world_position MaxTileP = MapIntoTileSpace(WorldMode->World, WorldMode->CameraP,
                                                 GetMaxCorner(CameraBoundsInMeters));
    v2 CameraDim = GetDim(CameraBoundsInMeters);
    v2 CameraHalfDim = 0.5f*CameraDim;

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
                v2 Delta = Subtract(WorldMode->World, &TileP, &WorldMode->CameraP) - V2(0.5f, 0.5f);
                
                world_tile *WorldTile = WorldMode->World->Tiles + TileY*WORLD_TILE_COUNT_PER_DIM + TileX;
                    
                bitmap_id ID = WorldTile->TileBitmapID;

                Transform.SortBias = -1000.0f;
                PushBitmap(RenderGroup, Transform, ID, WorldMode->World->TileSideInMeters, V3(Delta, 0.0f));// + V3(0.5f, 0.5f, 0.0f));
#if SPELLWEAVER_INTERNAL
                PushRectOutline(RenderGroup, Transform, V3(Delta + V2(0.5f, 0.5f), 2.0f), V2(WorldMode->World->TileSideInMeters,
                                                                            WorldMode->World->TileSideInMeters),
                                V4(1.0f, 1.0f, 0.0f, 1.0f), 0.02f);
#endif
            }
        }
    }
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
DrawEntityMarker(world_position EntityTileP, r32 TileDim, s32 WorldMapMinX, s32 WorldMapMinY,
                 loaded_bitmap *MiniMap, u32 Color)
{
    s32 MarkerDim = 16;
    s32 HalfMarkerDim = MarkerDim / 2;
    
    s32 TileX = EntityTileP.TileX;
    s32 TileY = EntityTileP.TileY;

    s32 OffsetX = (s32)(TileDim*EntityTileP.Offset.x);
    s32 OffsetY = (s32)(TileDim*EntityTileP.Offset.y);
    
    s32 MinX = (s32)TileDim*TileX + OffsetX - WorldMapMinX - HalfMarkerDim;
    s32 MinY = (s32)TileDim*TileY + OffsetY - WorldMapMinY - HalfMarkerDim;

    s32 MaxX = MinX + MarkerDim;
    s32 MaxY = MinY + MarkerDim;

    if(MinX < 0)
    {
        MaxX -= MinX;
        MinX = 0;
    }

    if(MaxX > MiniMap->Width)
    {
        MinX -= (MaxX - MiniMap->Width);
        MaxX = MiniMap->Width;
    }

    if(MinY < 0)
    {
        MaxY -= MinY;
        MinY = 0;
    }

    if(MaxY > MiniMap->Height)
    {
        MinY -= (MaxY - MiniMap->Height);
        MaxY = MiniMap->Height;
    }

    u8 *DestRow = (u8 *)MiniMap->Memory + MinY*MiniMap->Pitch + BITMAP_BYTES_PER_PIXEL*MinX;
    for(s32 Y = MinY;
        Y < MaxY;
        ++Y)
    {
        u32 *Dest = (u32 *)DestRow;
        for(s32 X = MinX;
            X < MaxX;
            ++X)
        {
            *Dest = Color;
            ++Dest;
        }

        DestRow += MiniMap->Pitch;
    }
}

internal void
UpdateMiniMap(game_mode_world *WorldMode, sim_region *SimRegion, loaded_bitmap *CompleteMap, loaded_bitmap *MapToShow, rectangle2 MapBounds)
{
    TIMED_FUNCTION();

    world_position CenterP = MapIntoTileSpace(WorldMode->World, WorldMode->CameraP,
                                               GetCenter(MapBounds));
    v2 MapCenter = GetDim(MapBounds);

    int32 TileCount = WORLD_TILE_COUNT_PER_DIM;
    r32 TileDim = (r32)CompleteMap->Width / (r32)TileCount;

    s32 OffsetX = (s32)(TileDim*CenterP.Offset.x);
    s32 OffsetY = (s32)(TileDim*CenterP.Offset.y);
    
    s32 MinX = (s32)TileDim*CenterP.TileX + OffsetX - 360;
    s32 MinY = (s32)TileDim*CenterP.TileY + OffsetY - 360;

    s32 MaxX = MinX + 720;
    s32 MaxY = MinY + 720;

    if(MinX < 0)
    {
        MaxX -= MinX;
        MinX = 0;
    }

    if(MaxX > CompleteMap->Width)
    {
        MinX -= (MaxX - CompleteMap->Width);
        MaxX = CompleteMap->Width;
    }

    if(MinY < 0)
    {
        MaxY -= MinY;
        MinY = 0;
    }

    if(MaxY > CompleteMap->Height)
    {
        MinY -= (MaxY - CompleteMap->Height);
        MaxY = CompleteMap->Height;
    }

    u8 *SourceRow = (u8 *)CompleteMap->Memory + MinY*CompleteMap->Pitch + BITMAP_BYTES_PER_PIXEL*MinX;
    u8 *DestRow = (u8 *)MapToShow->Memory;
    for(s32 Y = MinY;
        Y < MaxY;
        ++Y)
    {
        u32 *Dest = (u32 *)DestRow;
        u32 *Source = (u32 *)SourceRow;
        for(s32 X = MinX;
            X < MaxX;
            X += 8)
        {
            __m128i Source_4x = _mm_load_si128((__m128i *)Source);
            _mm_store_si128((__m128i *)Dest, Source_4x);
            Dest += 4;
            Source += 4;

            Source_4x = _mm_load_si128((__m128i *)Source);
            _mm_store_si128((__m128i *)Dest, Source_4x);
            Dest += 4;
            Source += 4;
        }

        DestRow += MapToShow->Pitch;
        SourceRow += CompleteMap->Pitch;
    }

    for(u32 EntityIndex = 0;
        EntityIndex < SimRegion->EntityCount;
        ++EntityIndex)
    {
        entity *Entity = SimRegion->Entities + EntityIndex;
        if(Entity->GeneralType == GeneralType_Enemy)
        {
            DrawEntityMarker(Entity->TileP, TileDim, MinX, MinY, MapToShow, 0xFFFF0000);
        }
        else if(Entity->GeneralType == GeneralType_Allay)
        {
            DrawEntityMarker(Entity->TileP, TileDim, MinX, MinY, MapToShow, 0xFF00FF00);
        }
        else if(Entity->GeneralType == GeneralType_Item)
        {
            DrawEntityMarker(Entity->TileP, TileDim, MinX, MinY, MapToShow, 0xFF0000FF);
        }
        else if(Entity->GeneralType == GeneralType_Hero)
        {
            DrawEntityMarker(Entity->TileP, TileDim, MinX, MinY, MapToShow, 0xFFFFFFFF);

            hero_entity *HeroData = (hero_entity *)Entity->Data;
            for(u32 QuestIndex = 0;
                QuestIndex < ArrayCount(HeroData->CurrentQuests);
                ++QuestIndex)
            {
                quest *Quest = WorldMode->Quests + HeroData->CurrentQuests[QuestIndex];
                if(IsValid(Quest->Location))
                {
                    r32 Distance = LengthSq(Subtract(WorldMode->World, &Entity->TileP, &Quest->Location));
                    if(Distance > Square(20.0f))
                    {
                        DrawEntityMarker(Quest->Location, TileDim, MinX, MinY, MapToShow, 0xFFFFFF00);
                    }
                }
            }
        }
    }
    
    if(MapToShow->TextureHandle)
    {
        Platform.DeallocateTexture(MapToShow->TextureHandle);
        MapToShow->TextureHandle = 
            Platform.AllocateTexture(MapToShow->Width, MapToShow->Height, MapToShow->Memory);
    }
    else
    {
        MapToShow->TextureHandle = 
            Platform.AllocateTexture(MapToShow->Width, MapToShow->Height, MapToShow->Memory);
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

            PushLine(RenderGroup, DefaultFlatTransform(),
                     V3(NDelta + V2(0.125f, 0.125f), 4.0f), V3(Delta + V2(0.125f, 0.125f), 4.0f), V4(1, 1, 0, 1));

            Node = Node->Parent;
        }
    }
}

internal b32
UpdateAndRenderWorld(game_state *GameState, game_mode_world *WorldMode, transient_state *TranState,
                     game_input *Input, render_group *RenderGroup, loaded_bitmap *DrawBuffer)
{
    TIMED_FUNCTION();
    
    b32 Result = false;

    {DEBUG_DATA_BLOCK("WorldMemory");
        memory_arena *WorldArena = &WorldMode->World->Arena;
        DEBUG_VALUE(WorldArena);
    }

    world *World = WorldMode->World;
    if(!TranState->WorldTilesInitialized)
    {
        loaded_tileset *Tileset = PushTileset(RenderGroup, TranState->GlobalTilesetID, true);
        ssa_tileset *TilesetInfo = GetTilesetInfo(TranState->Assets, TranState->GlobalTilesetID);
    
        u32 TileCount = WORLD_TILE_COUNT_PER_DIM*WORLD_TILE_COUNT_PER_DIM;
        for(u32 TileIndex = 0;
            TileIndex < TileCount;
            ++TileIndex)
        {
            world_tile *Tile = World->Tiles + TileIndex;
            Tile->TileBitmapID = GetBitmapForTile(TranState->Assets, TilesetInfo, Tileset, Tile->TileID);
        }

        Platform.WriteLogFile(L"World tiles initialized", __FILE__, __LINE__);
        TranState->WorldTilesInitialized = true;
    }

    v2 MouseP = {Input->MouseX, Input->MouseY};

    real32 WidthOfMonitor = 0.635f; // NOTE(casey): Horizontal measurement of monitor in meters
    real32 MetersToPixels = (real32)DrawBuffer->Width*WidthOfMonitor;

    real32 FocalLength = 0.5f;
    real32 DistanceAboveGround = 10.0f;
    Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, DistanceAboveGround);

    Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 0.0f));
    
    v2 ScreenCenter = {0.5f*(real32)DrawBuffer->Width,
                       0.5f*(real32)DrawBuffer->Height};

    rectangle2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);
    rectangle2 CameraBoundsInMeters = RectMinMax(ScreenBounds.Min, ScreenBounds.Max);

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    MatchVector.E[Tag_FontType] = (r32)FontType_Nice;
    WeightVector.E[Tag_FontType] = 1.0f;

    text_config TextConfig = {};
    font_id FontID = GetBestMatchFontFrom(TranState->Assets, Asset_Font, &MatchVector, &WeightVector);
    WorldMode->GeneralTextConfig.Font = PushFont(RenderGroup, FontID);
    WorldMode->GeneralTextConfig.FontInfo = GetFontInfo(TranState->Assets, FontID);

    // NOTE(casey): Ground tiles rendering

    UpdateAndRenderGroundTiles(WorldMode, TranState, CameraBoundsInMeters, RenderGroup);

    //
    // NOTE(paul): Take Input
    //

    controlled_hero *ConHero = 0;
    for(int ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
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
            
            if(Controller->IsAnalog)
            {
                // NOTE(casey): Use analog movement tuning
                ConHero->ddP = V2(Controller->StickAverageX, Controller->StickAverageY);
            }
            else
            {
#if 0
                // NOTE(casey): Use digital movement tuning
                if(Controller->MoveUp.EndedDown)
                {
                    ConHero->ddP.y = 1.0f;
                }
                if(Controller->MoveDown.EndedDown)
                {
                    ConHero->ddP.y = -1.0f;
                }
                if(Controller->MoveLeft.EndedDown)
                {
                    ConHero->ddP.x = -1.0f;
                }
                if(Controller->MoveRight.EndedDown)
                {
                    ConHero->ddP.x = 1.0f;
                }
#endif
            }

            if(WasPressed(Input->MouseButtons[0]))
            {
                ConHero->Attack = true;
            }

            if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
            {
                ConHero->Move = true;
            }

            if(WasPressed(Controller->RightShoulder))
            {
                ConHero->Action = true;
                PlaySound(&GameState->AudioState, GetSoundEffectForType(TranState->Assets, SoundEffect_Click));
            }

            if(WasPressed(Controller->ChangeSphereToWater))
            {
                ConHero->SphereNewType = SphereType_Water;
            }
            else if(WasPressed(Controller->ChangeSphereToWind))
            {
                ConHero->SphereNewType = SphereType_Wind;
            }
            else if(WasPressed(Controller->ChangeSphereToFire))
            {
                ConHero->SphereNewType = SphereType_Fire;
            }

            if(WasPressed(Controller->LeftShoulder))
            {
                ConHero->InvokeAndCastSpell = true;
            }

            if(WasPressed(Controller->Back) && (WorldMode->UpdateMode == UpdateMode_Conversation))
            {
                WorldMode->UpdateMode = UpdateMode_Entities;
                PlaySound(&GameState->AudioState, GetSoundEffectForType(TranState->Assets, SoundEffect_Click));
            }
            else if(WasPressed(Controller->Back))
            {
                WorldMode->QuitRequested = !WorldMode->QuitRequested;
                PlaySound(&GameState->AudioState, GetSoundEffectForType(TranState->Assets, SoundEffect_Click));
            }
        }
    }
    
    // TODO(casey): How big do we actually want to expand here?
    // TODO(casey): Do we want to simulate upper floors, etc.?
    v2 SimBoundsExpansion = {10.0f, 10.0f};
    rectangle2 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundsExpansion);
    temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
    world_position SimCenterP = WorldMode->CameraP;
    sim_region *SimRegion = BeginSim(&TranState->TranArena, WorldMode, WorldMode->World,
                                     SimCenterP, SimBounds, Input->dtForFrame);
    
    v2 CameraP = Subtract(World, &WorldMode->CameraP, &SimCenterP);

    DrawTileNodes(WorldMode, TranState, SimBounds, RenderGroup,
                  WorldMode->StartNode, WorldMode->EndNode);
    
#if SPELLWEAVER_INTERNAL    
    PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(0.0f, 0.0f, 0.0f), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1));
//    PushRectOutline(RenderGroup, V3(0.0f, 0.0f, 0.0f), GetDim(CameraBoundsInMeters).xy, V4(1.0f, 1.0f, 1.0f, 1));
    PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(0.0f, 0.0f, 0.0f), GetDim(SimBounds), V4(0.0f, 1.0f, 1.0f, 1));
    PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(0.0f, 0.0f, 0.0f), GetDim(SimRegion->Bounds), V4(1.0f, 0.0f, 1.0f, 1));
#endif

    if(WorldMode->HeroExist)
    {
        object_transform BarsTransform = DefaultFlatTransform();
        
        bitmap_id HealthBar = GetFirstBitmapFrom(TranState->Assets, Asset_HeroHealthBar);
        bitmap_id SphereBar = GetFirstBitmapFrom(TranState->Assets, Asset_SphereBar);
        bitmap_id SpellBar = GetFirstBitmapFrom(TranState->Assets, Asset_SpellBar);
        bitmap_id QuestBorder = GetFirstBitmapFrom(TranState->Assets, Asset_QuestBorder);
        BarsTransform.SortBias = 10000.0f;
        PushBitmap(RenderGroup, BarsTransform, HealthBar, 1.8f, V3(-12.5f, 7.8f, 0));
        PushBitmap(RenderGroup, BarsTransform, SphereBar, 1.0f, V3(0.0f, -8.0f, 0), V4(1, 1, 1, 0.95f));
        PushBitmap(RenderGroup, BarsTransform, SpellBar, 6.75f, V3(-12.7f, -5.4f, 0), V4(1, 1, 1, 0.95f));

        UpdateMiniMap(WorldMode, SimRegion, &TranState->MiniMap, &WorldMode->MiniMapBitmap, CameraBoundsInMeters);

        PushRect(RenderGroup, BarsTransform, V3(13.0f, 6.1f, 1.0f), V2(5.3f, 5.3f), V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1));
        PushRect(RenderGroup, BarsTransform, V3(13.0f, 6.1f, 2.0f), V2(5.2f, 5.2f), V4(0.725490196078f, 0.478431372549f, 0.341176470588f, 1));
        PushRect(RenderGroup, BarsTransform, V3(13.0f, 6.1f, 3.0f), V2(5.1f, 5.1f), V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1));
        PushBitmap(RenderGroup, BarsTransform, &WorldMode->MiniMapBitmap, 5.0f, V3(13.0f, 6.1f, 4.0f), V4(1, 1, 1, 1));
        
        for(uint32 ControlIndex = 0;
            ControlIndex < ArrayCount(GameState->ControlledHeroes);
            ++ControlIndex)
        {
            controlled_hero *ConHero = GameState->ControlledHeroes + ControlIndex;

            if(ConHero->EntityIndex.Value)
            {
                entity *HeroEntity = GetEntityByID(SimRegion, ConHero->EntityIndex);
                if(HeroEntity)
                {
                    WorldMode->StartNode = GetTileNode(WorldMode->World, HeroEntity->TileP);

                    hero_entity *HeroData = (hero_entity *)HeroEntity->Data;
                    if((HeroData->QuestCount > 0) && (WorldMode->UpdateMode == UpdateMode_Entities))
                    {
                        PushBitmap(RenderGroup, BarsTransform, QuestBorder, 2.3f, V3(13.0f, 2.2f, 0), V4(1, 1, 1, 0.95f));
                    }
                }
            }
        }

    }
    
    {
        TIMED_BLOCK("EntityRender");
        switch(WorldMode->UpdateMode)
        {
            case UpdateMode_Entities:
            {
                UpdateAndRenderEntities(WorldMode, GameState, SimRegion, RenderGroup, Input->dtForFrame, MouseP);

                text_config TextConfig = WorldMode->GeneralTextConfig;

                TextConfig.TextTransform.OffsetP = V3(-9.0f, -4.0f, 0.0f);
                TextConfig.TextShadowTransform.OffsetP = V3(-10.95f, -2.03f, 0.0f);
                TextConfig.Color = V4(1, 1, 1, 1);

                for(uint32 ControlIndex = 0;
                    ControlIndex < ArrayCount(GameState->ControlledHeroes);
                    ++ControlIndex)
                {
                    controlled_hero *ConHero = GameState->ControlledHeroes + ControlIndex;

                    if(ConHero->EntityIndex.Value)
                    {
                        entity *HeroEntity = GetEntityByID(SimRegion, ConHero->EntityIndex);
                        if(HeroEntity)
                        {
                            //NOTE(paul): Quest Update
                            UpdateQuests(WorldMode, SimRegion, HeroEntity);

                            found_entity ClosestEnemy = FindClosestEntityOfGeneralType(SimRegion, HeroEntity, GeneralType_Enemy, 10.0f);
                            if(ClosestEnemy.Entity &&
                               (ClosestEnemy.DistanceSq < Square(11.0f)) &&
                               GameState->MusicState != MusicState_Action)
                            {
                                ChangeBackgroundMusic(GameState, MusicState_Action);
                            }
                            else if((GameState->MusicState == MusicState_Action) &&
                                    (ClosestEnemy.DistanceSq > Square(8.0f)))
                            {
                                ChangeBackgroundMusic(GameState, MusicState_Ambient);
                            }

                            // NOTE(paul): Display hero's quests
                            DrawHeroQuests(WorldMode, SimRegion, HeroEntity, RenderGroup, TextConfig);
                        }
                    }
                }

                DestroyEntities(WorldMode, SimRegion);
            } break;

            case UpdateMode_Conversation:
            {
                object_transform EntityTransform = DefaultUprightTransform();
                v3 LocalMouseP = Unproject(RenderGroup, EntityTransform, MouseP);

                for(uint32 EntityIndex = 0;
                    EntityIndex < SimRegion->EntityCount;
                    ++EntityIndex)
                {
                    entity *Entity = SimRegion->Entities + EntityIndex;
                    EntityTransform.OffsetP = GetEntityGroundPoint(Entity);

                    if(Entity->Updatable)
                    {
                        Entity->AnimationType = AnimationType_Idle;
                        b32 AnimationFinished = false;
                        ssa_spritesheet *SpriteSheetInfo = 0;
                        u32 AnimationSpeed = Entity->SpriteSheetSpeed[Entity->AnimationType][Entity->FacingDirection];
                        spritesheet_id ID = Entity->SpriteSheets[Entity->AnimationType][Entity->FacingDirection];

                        render_entity RenderEntity = {};
                        if(IsValid(ID))
                        {
                            RenderEntity.SpriteSheet = PushSpriteSheet(RenderGroup, ID, true);

                            if(IsValid(RenderEntity.SpriteSheet->SpriteIDs[0]))
                            {
                                SpriteSheetInfo = GetSpriteSheetInfo(RenderGroup->Assets, ID);
                                RenderEntity.EntitySpriteIndex = UpdateSpriteIndex(Entity, WorldMode->Time,
                                                                                   SpriteSheetInfo->SpriteCount, AnimationSpeed);
                                AnimationFinished = AnimationHasComleted(WorldMode->Time, Entity->SpriteSheetOffset,
                                                                         RenderEntity.EntitySpriteIndex,
                                                                         SpriteSheetInfo->SpriteCount,
                                                                         Input->dtForFrame, AnimationSpeed);
                            }
                            else
                            {
                                RenderEntity.SpriteSheet = 0;
                            }
                        }
        
                        RenderEntities(WorldMode, SimRegion, RenderGroup, EntityTransform, Entity,
                                       Input->dtForFrame, &RenderEntity);
                    }
                }

                object_transform DialogueBorderTransform = DefaultFlatTransform();
                DialogueBorderTransform.OffsetP = V3(0.0f, -3.3f, 4.0f);
                bitmap_id BorderID = GetFirstBitmapFrom(TranState->Assets, Asset_DialogueBorder);

                PushBitmap(RenderGroup, DialogueBorderTransform, BorderID, 4.15f, V3(0, 0, 0), V4(1, 1, 1, 0.9f));

                text_config TextConfig = WorldMode->GeneralTextConfig;

                TextConfig.TextTransform.OffsetP = V3(-8.45f, -3.2f, 0.0f);
                TextConfig.TextShadowTransform.OffsetP = V3(-10.41f, -1.24f, 0.0f);
                TextConfig.Color = V4(1, 1, 1, 1);
                TextConfig.FontScale = 0.013;

                entity *NPCEntity = GetEntityByID(SimRegion, WorldMode->TalkingEntityID);
                talkingnpc_entity *NPCData = (talkingnpc_entity *)NPCEntity->Data;

                text_id TextID = {};
                ssa_quest *QuestTextInfo = 0;
                if(NPCData->TalkingState != TalkingState_General)
                {
                    quest *Quest = WorldMode->Quests + NPCData->QuestID;
                    loaded_quest *QuestText = PushQuest(RenderGroup, Quest->QuestTextID, true);
                    QuestTextInfo = GetQuestInfo(TranState->Assets, Quest->QuestTextID);
                    TextID = GetQuestTextIDForNPC(QuestText, QuestTextInfo, NPCData);
                }
                else
                {
                    TextID = NPCData->GeneralText;
                }
                
                ssa_text *TextInfo = GetTextInfo(TranState->Assets, TextID);
                loaded_text *Text = PushText(RenderGroup, TextID, true);
                    
                TextOutAt(RenderGroup, TextConfig, Text->String, TextInfo->Length);

                for(uint32 ControlIndex = 0;
                    ControlIndex < ArrayCount(GameState->ControlledHeroes);
                    ++ControlIndex)
                {
                    controlled_hero *ConHero = GameState->ControlledHeroes + ControlIndex;

                    if(ConHero->EntityIndex.Value)
                    {
                        if(ConHero->Action)
                        {
                            entity *HeroEntity = GetEntityByID(SimRegion, ConHero->EntityIndex);
                            AdvanceParagraphIndexForNPC(WorldMode, SimRegion, QuestTextInfo, NPCData, HeroEntity);
                        }
                    }
                }

            } break;
        }
#if 1
        for(uint32 ControlIndex = 0;
            ControlIndex < ArrayCount(GameState->ControlledHeroes);
            ++ControlIndex)
        {
            controlled_hero *ConHero = GameState->ControlledHeroes + ControlIndex;

            if(ConHero->EntityIndex.Value)
            {
                entity *HeroEntity = GetEntityByID(SimRegion, ConHero->EntityIndex);
                if(HeroEntity)
                {
                    v2 Point = V2i(HeroEntity->TileP.TileX, HeroEntity->TileP.TileY);
                    if(InsidePolygon(Point, &WorldMode->BirdSoundPolygon, &WorldMode->MathEntropy))
                    {
                        asset_vector MusicWeightVector = {};
                        MusicWeightVector.E[Tag_MusicType] = 1.0f;
                        asset_vector MusicMatchVector = {};
                        MusicMatchVector.E[Tag_MusicType] = (r32)MusicType_Forest;
                        sound_id BirdsSound = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

                        if(!WorldMode->BirdSound)
                        {
                            WorldMode->BirdSound = PlaySound(&GameState->AudioState, BirdsSound);
                        }
                        else
                        {
                            if(!WorldMode->BirdSound->SoundIsPlaying)
                            {
                                WorldMode->BirdSound = PlaySound(&GameState->AudioState, BirdsSound);
                            }
                        }
                    }
                    else
                    {
                        MuteAndTerminateSound(&GameState->AudioState, WorldMode->BirdSound, 2.0f);
                    }

                    if(InsidePolygon(Point, &WorldMode->RiverPolygon0, &WorldMode->MathEntropy) ||
                       InsidePolygon(Point, &WorldMode->RiverPolygon1, &WorldMode->MathEntropy))
                    {
                        asset_vector MusicWeightVector = {};
                        MusicWeightVector.E[Tag_MusicType] = 1.0f;
                        asset_vector MusicMatchVector = {};
                        MusicMatchVector.E[Tag_MusicType] = (r32)MusicType_WaterStream;
                        sound_id RiverSound = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

                        if(!WorldMode->RiverSound)
                        {
                            WorldMode->RiverSound = PlaySound(&GameState->AudioState, RiverSound);
                        }
                        else
                        {
                            if(!WorldMode->RiverSound->SoundIsPlaying)
                            {
                                WorldMode->RiverSound = PlaySound(&GameState->AudioState, RiverSound);
                            }
                        }

                    }
                    else
                    {
                        MuteAndTerminateSound(&GameState->AudioState, WorldMode->RiverSound, 2.0f);
                    }
                }
            }
        }
#endif        
        WorldMode->Time += Input->dtForFrame;

#if 0
        if(DEBUG_UI_ENABLED)
        {
            debug_id EntityDebugID_ = DEBUG_POINTER_ID(WorldMode->LowEntities + Entity->StorageIndex);

            for(uint32 VolumeIndex = 0;
                VolumeIndex < Entity->Collision->VolumeCount;
                ++VolumeIndex)
            {
                sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;                        

                v3 LocalMouseP = Unproject(RenderGroup, EntityTransform, MouseP);

                if((LocalMouseP.x > -0.5f*Volume->Dim.x) && (LocalMouseP.x < 0.5f*Volume->Dim.x) &&
                   (LocalMouseP.y > -0.5f*Volume->Dim.y) && (LocalMouseP.y < 0.5f*Volume->Dim.y))
                {
                    DEBUG_HIT(EntityDebugID_, LocalMouseP.z);
                }

                v4 OutlineColor;
                if(DEBUG_HIGHLIGHTED(EntityDebugID_, &OutlineColor))
                {
                    PushRectOutline(RenderGroup, EntityTransform, Volume->OffsetP - V3(0, 0, 0.5f*Volume->Dim.z), Volume->Dim.xy, OutlineColor, 0.05f);
                }
            }
                
            if(DEBUG_REQUESTED(EntityDebugID))
            {
                DEBUG_VALUE(Entity->StorageIndex);
                DEBUG_VALUE(Entity->Updatable);
                DEBUG_VALUE(Entity->Type);
                DEBUG_VALUE(Entity->P);
                DEBUG_VALUE(Entity->dP);
                DEBUG_VALUE(Entity->DistanceLimit);
                DEBUG_VALUE(Entity->FacingDirection);
                DEBUG_VALUE(Entity->tBob);
                DEBUG_VALUE(Entity->dAbsTileZ);
                DEBUG_VALUE(Entity->HitPointMax);
#if 0
                DEBUG_BEGIN_ARRAY(Entity->HitPoint);                    
                for(u32 HitPointIndex = 0;
                    HitPointIndex < Entity->HitPointMax;
                    ++HitPointIndex)
                {
                    DEBUG_VALUE(Entity->HitPoint[HitPointIndex]);
                }
                DEBUG_END_ARRAY();
                DEBUG_VALUE(Entity->Sword);
#endif
                DEBUG_VALUE(Entity->WalkableDim);
                DEBUG_VALUE(Entity->WalkableHeight);

                DEBUG_END_DATA_BLOCK("Simulation/Entity");
            }
        }
#endif
    }
    
    RenderGroup->GlobalAlpha = 1.0f;

    EndSim(WorldMode, SimRegion, CameraBoundsInMeters);
    EndTemporaryMemory(SimMemory);

    if(!WorldMode->HeroExist || WorldMode->QuitRequested || WorldMode->GameFinished)
    {
        b32 Hover = false;
        object_transform Transform = DefaultFlatTransform();
        Transform.SortBias = 10000000.0f;
        v4 RectColor = V4(0, 0, 0, 0.5f);
        if(WorldMode->GameFinished)
        {
            RectColor = V4(0, 0, 0, 0.1f);
        }

        PushRect(RenderGroup, Transform, V3(0, 0, 0), V2i(DrawBuffer->Width, DrawBuffer->Height), RectColor);
        Transform.SortBias = 10001000.0f;
        Transform.OffsetP = V3(0, -4.0f, 0.0f);
            
        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        MatchVector.E[Tag_Variety] = (r32)VarietyType_4;
        WeightVector.E[Tag_Variety] = 1.0f;
        bitmap_id TitleImage = GetBestMatchBitmapFrom(RenderGroup->Assets, Asset_TitleImage, &MatchVector, &WeightVector);

        v3 LocalMouseP = Unproject(RenderGroup, Transform, MouseP);
        loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, TitleImage, RenderGroup->GenerationID);
        v3 AdditionalOffset = V3(0, 0, 0);
        if(Bitmap)
        {
            used_bitmap_dim Dim = GetBitmapDim(RenderGroup, Transform, Bitmap, 2.0f, V3(0, 0, 0), 1.0f);
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
        PushBitmap(RenderGroup, Transform, TitleImage, 2.0f, V3(0, 0, 0));

        text_config TextConfig = WorldMode->GeneralTextConfig;
        if(WorldMode->QuitRequested)
        {
            TextConfig.TextTransform.SortBias = 10001000.0f;
            TextConfig.TextTransform.OffsetP = V3(-7.4f, 0.0f, 0);
            TextConfig.TextShadowTransform.OffsetP = V3(-9.25f, 1.85f, 0);
            TextConfig.FontScale = 0.04f;
            TextConfig.Color = V4(1, 1, 1, 1);
            TextOutAt(RenderGroup, TextConfig, "Do you want to exit?", 0);
        }
        else if(WorldMode->GameFinished)
        {
            if(!WorldMode->GameEndMusic)
            {
                asset_vector MusicWeightVector = {};
                MusicWeightVector.E[Tag_MusicType] = 1.0f;
                MusicWeightVector.E[Tag_Variety] = 1.0f;
                asset_vector MusicMatchVector = {};
                MusicMatchVector.E[Tag_MusicType] = (r32)MusicType_GameVictoryMusic;
                MusicMatchVector.E[Tag_Variety] = (r32)VarietyType_None;
                sound_id MusicID = GetBestMatchSoundFrom(TranState->Assets, Asset_Music, &MusicMatchVector, &MusicWeightVector);

                WorldMode->GameEndMusic = PlaySound(&GameState->AudioState, MusicID);
                ChangeVolume(&GameState->AudioState, WorldMode->GameEndMusic, 2.0f, V2(0.7f, 0.7f));
            }
            else
            {
                ChangeVolume(&GameState->AudioState, GameState->Music, 2.0f, V2(0.0f, 0.0f));
                if(!WorldMode->GameEndMusic->SoundIsPlaying)
                {
                    ChangeVolume(&GameState->AudioState, GameState->Music, 2.0f, V2(0.5f, 0.5f));
                    GameState->FadeState = FadeState_FadeIn;
                    ChangeVolume(&GameState->AudioState, GameState->Music, 2.0f, V2(0.5f, 0.5f));
                    MuteAndTerminateSound(&GameState->AudioState, WorldMode->BirdSound, 2.0f);
                    MuteAndTerminateSound(&GameState->AudioState, WorldMode->RiverSound, 2.0f);
                    MuteAndTerminateSound(&GameState->AudioState, WorldMode->GameEndMusic, 2.0f);
                }
            }

            asset_vector MatchVector = {};
            asset_vector WeightVector = {};
            MatchVector.E[Tag_Variety] = (r32)VarietyType_5;
            WeightVector.E[Tag_Variety] = 1.0f;

            Transform.OffsetP -= AdditionalOffset;
            bitmap_id TitleImage = GetBestMatchBitmapFrom(RenderGroup->Assets, Asset_TitleImage, &MatchVector, &WeightVector);
            PushBitmap(RenderGroup, Transform, TitleImage, 5.0f, V3(0, 5.0f, 0));
            
        }
        else
        {
            TextConfig.TextTransform.SortBias = 10001000.0f;
            TextConfig.TextTransform.OffsetP = V3(-7.4f, 1.0f, 0);
            TextConfig.TextShadowTransform.OffsetP = V3(-9.25f, 2.85f, 0);
            TextConfig.FontScale = 0.08f;
            TextConfig.Color = V4(1, 0, 0.1f, 1);
            TextOutAt(RenderGroup, TextConfig, "GAME OVER", 0);

            if(GameState->MusicState != MusicState_DarkAmbient)
            {
                ChangeBackgroundMusic(GameState, MusicState_DarkAmbient);
                ChangeVolume(&GameState->AudioState, GameState->Music, 2.0f, V2(1.0f, 1.0f));
            }
        }

        if(Hover && WasPressed(Input->MouseButtons[0]))
        {
            PlaySound(&GameState->AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_Click));
            GameState->FadeState = FadeState_FadeIn;
            ChangeVolume(&GameState->AudioState, GameState->Music, 2.0f, V2(0.5f, 0.5f));
            MuteAndTerminateSound(&GameState->AudioState, WorldMode->BirdSound, 2.0f);
            MuteAndTerminateSound(&GameState->AudioState, WorldMode->RiverSound, 2.0f);
            if(WorldMode->GameFinished)
            {
                MuteAndTerminateSound(&GameState->AudioState, WorldMode->GameEndMusic, 2.0f);
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
                game_controller_input *Controller = GetController(Input, ControllerIndex);
                controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;
                ZeroStruct(*ConHero);
            }

            PlayTitleScreen(GameState, TranState);
        }
    }    

    return(Result);
}


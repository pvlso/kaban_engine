/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_game_mode_undo.cpp"

#include "engine_game_mode_tile.cpp"
#include "engine_game_mode_navmesh.cpp"

#include "engine_game_mode_ui.cpp"

inline b32
AbleToStart(world_map_startup MapStartup)
{
    b32 Result = (((MapStartup.MapWidth >= 48) &&
                   (MapStartup.MapHeight >= 48) &&
                   MapStartup.NewMap) || MapStartup.ID.Value);
    return(Result);
}

internal void
ReadPolygons(editor_mode_game *GameMode)
{
    FILE *Out;
    fopen_s(&Out, "polygons.nmp", "rb");
    if(Out)
    {
        fread(&GameMode->PolygonCount, sizeof(u32), 1, Out);
        
        for(u32 PolygonIndex = 0;
            PolygonIndex < GameMode->PolygonCount;
            ++PolygonIndex)
        {
            world_polygon *Current = GameMode->Polies + PolygonIndex;
            fread(&Current->VertexCount, sizeof(u32), 1, Out); 
            u32 VerticesSize = Current->VertexCount*sizeof(world_position);
            fread(Current->Vertices, VerticesSize, 1, Out);
        }

        GameMode->CurrentPolygon = GameMode->Polies + 0;
        GameMode->CurrentPolygonIndex = GameMode->PolygonCount - 1;
    }

    fclose(Out);
}

internal void
WriteSSWM(editor_state *EditorState, world *World, u32 GroundLayer_ZLayerCount)
{
    loaded_world_map *Map = World->Map;

    u32 Length = 0;
    char LogBuffer[512];
    FILE *LogFile;
    FormatString(ArrayCount(LogBuffer), LogBuffer, "logs\\sswm_writing_log_%d.%d.%d.%d.txt",
                 EditorState->Version.MajorHigh, EditorState->Version.MajorLow,
                 EditorState->Version.MinorHigh, EditorState->Version.MinorLow);
    fopen_s(&LogFile, LogBuffer, "wb");
    
    char FileName[256];
    FormatString(ArrayCount(FileName), FileName, "sswms//%s%d_%dx%d_%d.%d.%d.%d.sswm",
                 Map->Header->Name, EditorState->MapStartup.MapID, Map->Header->MapWidth, Map->Header->MapHeight,
                 EditorState->Version.MajorHigh, EditorState->Version.MajorLow,
                 EditorState->Version.MinorHigh, EditorState->Version.MinorLow);

    Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer, "Writing to: %s\n", FileName);
    fwrite(LogBuffer, Length, 1, LogFile);

    FILE *Out;
    fopen_s(&Out, FileName, "wb");
    if(Out)
    {
        sswm_header Header = {};
        Header.MagicValue = SSWM_MAGIC_VALUE;
        Header.Version = (u32)((EditorState->Version.MajorHigh << 24) | (EditorState->Version.MajorLow << 16) |
                               (EditorState->Version.MinorHigh << 8) | EditorState->Version.MinorLow);
        FormatString(ArrayCount(Header.Name), Header.Name, "%s", Map->Header->Name);
        Header.MapWidth = Map->Header->MapWidth;
        Header.MapHeight = Map->Header->MapHeight;

        Header.GroundLayer_ZLayerCount = GroundLayer_ZLayerCount;
        Header.EntityCount = Map->Header->EntityCount;

        u32 TilesSize = sizeof(sswm_ground_tile)*Header.MapWidth*Header.MapHeight;
        u32 EntitiesSize = sizeof(sswm_entity)*Header.EntityCount;
        Header.Tiles = sizeof(sswm_header);
        Header.Entities = Header.Tiles + TilesSize;  
        
        fwrite(&Header, sizeof(Header), 1, Out);
        fwrite(Map->GroundTiles, TilesSize, 1, Out);
        fwrite(Map->Entities, EntitiesSize, 1, Out);
        fclose(Out);
        Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer, "Writing SSWM comleted\n");
        fwrite(LogBuffer, Length, 1, LogFile);
    }
    else
    {
        Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer, "ERROR: Fail to open %s\n", FileName);
        fwrite(LogBuffer, Length, 1, LogFile);
    }

    fclose(LogFile);
}

internal void
PlayGameMode(editor_state *EditorState, transient_state *TranState)
{
    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
#if 0
    MatchVector.E[Tag_VersionMajorHigh] =
        EditorState->MapStartup.MapVersion.MajorHigh;
    MatchVector.E[Tag_VersionMajorLow] =
        EditorState->MapStartup.MapVersion.MajorLow;
    MatchVector.E[Tag_VersionMinorHigh] =
        EditorState->MapStartup.MapVersion.MinorHigh;
    MatchVector.E[Tag_VersionMinorLow] =
        EditorState->MapStartup.MapVersion.MinorLow;
#endif
    WeightVector.E[Tag_VersionMajorHigh] = 2;
    WeightVector.E[Tag_VersionMajorLow] = 2;
    WeightVector.E[Tag_VersionMinorHigh] = 2;
    WeightVector.E[Tag_VersionMinorLow] = 2;
    EditorState->MapStartup.ID = GetBestMatchSSWMFrom(TranState->Assets, Asset_SSWM, &MatchVector, &WeightVector);

    if(AbleToStart(EditorState->MapStartup))
    {
        SetEditorMode(EditorState, TranState, EditorMode_GameMode);
    
        editor_mode_game *Result = PushStruct(&EditorState->ModeArena, editor_mode_game);
        Result->WorldState = PushStruct(&EditorState->ModeArena, game_mode_world);
        
        r32 PixelsToMeters = 1.0f / 32.0f;
        u32 TileSideInPixels = 32;
        r32 TileSideInMeters = TileSideInPixels * PixelsToMeters;

        if(EditorState->MapStartup.NewMap)
        {
            Result->WorldState->World = EDITORCreateWorld(TranState, TileSideInMeters, EditorState->MapStartup.MapWidth,
                                              EditorState->MapStartup.MapHeight, 1, 0);
        }
        else
        {
            loaded_world_map *Map = PushSSWM(TranState->Assets, TranState->MainGenerationID,
                                             EditorState->MapStartup.ID, true);
            Result->WorldState->World = EDITORCreateWorld(TranState, TileSideInMeters, Map->Header->MapWidth, Map->Header->MapHeight, 1, Map);

            Result->LayerCount = Map->Header->GroundLayer_ZLayerCount & 0xFFFF;
            Result->MapGroundLayer = Map->Header->GroundLayer_ZLayerCount >> 16;
        }

        world_position NewCameraP = {};
        uint32 CameraTileX = Result->WorldState->World->TileWidth / 2;
        uint32 CameraTileY = Result->WorldState->World->TileHeight / 2;
        NewCameraP = ChunkPositionFromTilePosition(Result->WorldState->World, CameraTileX, CameraTileY);

        Result->WorldState->CameraBoundsMin.TileX = 0;
        Result->WorldState->CameraBoundsMin.TileY = 0;
        Result->WorldState->CameraBoundsMin.Offset = V2(-0.5f, -0.5f);
    
        Result->WorldState->CameraBoundsMax.TileX = Result->WorldState->World->TileWidth;
        Result->WorldState->CameraBoundsMax.TileY = Result->WorldState->World->TileHeight;
        Result->WorldState->CameraBoundsMax.Offset = V2(0.5f, 0.5f);
        Result->WorldState->CameraP = NewCameraP;
        Result->CameraMoveStep = 2;

        InitializeCursor(&Result->TileCursor, 8);

        InitActionStack(&Result->UndoStack, &EditorState->ModeArena);
        InitActionStack(&Result->RedoStack, &EditorState->ModeArena);

        Result->Polies = PushArray(&EditorState->ModeArena, 256, world_polygon);
        for(s32 Index = 0;
            Index < 256;
            ++Index)
        {
            world_polygon *Poly = Result->Polies + Index;
            Poly->Vertices = PushArray(&EditorState->ModeArena, MAX_VERTEX_COUNT, world_position);
        }

        ReadPolygons(Result);

//        Result->MeshTriangles = PushArray(&EditorState->ModeArena, 4096, world_triangle);
//        Result->FreeTriangleIndices = PushArray(&EditorState->ModeArena, 1024, s32);
        
        Result->AutoWriteSeconds = 300.0f;

        DLIST_INIT(&Result->MeshPolygonsSentinal);

        Result->PolyNodeCount = 0;
        Result->PolyNodes = PushArray(&EditorState->ModeArena, 512, nav_poly_node);
        Result->MinPolyNodeHeap.MaxSize = 256;
        Result->MinPolyNodeHeap.Size = 0;
        Result->MinPolyNodeHeap.Nodes = PushArray(&EditorState->ModeArena, Result->MinPolyNodeHeap.MaxSize, sort_entry);

        Result->StartNode = NullPosition();
        Result->EndNode = NullPosition();
        
        EditorState->GameMode = Result;
    }
    else
    {
        PlayTitleScreen(EditorState, TranState);
    }
}

internal void
RenderMapGrid(render_group *RenderGroup, ui_state *UIState, world *World, world_position CameraP, rectangle2 SimBounds)
{
    TIMED_FUNCTION();

    world_position MinTileP = MapIntoTileSpace(World, CameraP, GetMinCorner(SimBounds));
    world_position MaxTileP = MapIntoTileSpace(World, CameraP, GetMaxCorner(SimBounds));
    v2 CameraDim = GetDim(SimBounds);
    v2 CameraHalfDim = 0.5f*CameraDim;

    char Text[32];
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
            if(((u32)TileX < World->TileWidth) && ((u32)TileY < World->TileHeight))
            {
                v2 Delta = Subtract(World, &TileP, &CameraP);
#if 1

//                if(Global_EditorGameMode_ShowCoords)
                {
                    FormatString(ArrayCount(Text), Text, "%d,%d", TileX, TileY);
                    entity_basis_p_result BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, &Transform, V3(Delta, 0.0f) - V3(0.32f, 0.48f, 0.0f));
                    v3 P = Unproject(&UIState->RenderGroup, &Transform, BasisP.P);
                    UITextOutAt(UIState, P.xy, Text, 0.42f);
                }

                PushRectOutline(RenderGroup, &Transform, V3(Delta, 0.0f),
                                World->TileDimInMeters.xy, V4(0.0f, 0.0f, 1.0f, 1.0f), 0.04f);
#endif

                
            }
        }
    }
}

internal void
RenderMapGroundTiles(render_group *RenderGroup, world *World, world_position CameraP, rectangle2 SimBounds,
                     u32 ZLayer, b32 ShowOnly = false)
{
    TIMED_FUNCTION();

    world_position MinTileP = MapIntoTileSpace(World, CameraP, GetMinCorner(SimBounds));
    world_position MaxTileP = MapIntoTileSpace(World, CameraP, GetMaxCorner(SimBounds));
    v2 CameraDim = GetDim(SimBounds);
    v2 CameraHalfDim = 0.5f*CameraDim;

    object_transform Transform = DefaultFlatTransform();
    Transform.ChunkZ = -100;
    for(s32 TileY = MinTileP.TileY;
        TileY <= MaxTileP.TileY;
        ++TileY)
    {
        for(s32 TileX = MinTileP.TileX;
            TileX <= MaxTileP.TileX;
            ++TileX)
        {
            world_position TileP = CenteredTilePoint(World, TileX, TileY);
            if(((u32)TileX < World->TileWidth) && ((u32)TileY < World->TileHeight))
            {
                v2 Delta = Subtract(World, &TileP, &CameraP);

                sswm_ground_tile *Tile = EDITORGetWorldMapGroundTile(World, TileX, TileY);
                
                u32 ZLayerCount = ZLayer;
                if(!ShowOnly)
                {
                    for(u32 BitmapIndex = 0;
                        BitmapIndex < ZLayerCount;
                        ++BitmapIndex)
                    {
                        bitmap_id ID = {Tile->BitmapID[BitmapIndex]};
                        PushBitmap(RenderGroup, &Transform, ID, World->TileDimInMeters.y, V3(Delta, 0.0f));
                        Transform.ChunkZ += 1;
                    }

                    Transform.ChunkZ -= ZLayerCount;
                }
                else
                {
                    bitmap_id ID = {Tile->BitmapID[ZLayer]};
                    PushBitmap(RenderGroup, &Transform, ID, World->TileDimInMeters.y, V3(Delta, 0.0f));
                }
            }
        }
    }
}

internal void
DrawMeshTriangles(ui_state *UIState, render_group *RenderGroup, world *World, world_triangle *Triangles, s32 Count, sim_region *SimRegion, rectangle2 Bounds)
{
    object_transform Flat = DefaultFlatTransform();
    entity_basis_p_result BasisP = {};
    v3 P = {};
    f32 OneOverThree = 1.0f / 3.0f;
    char Buffer[16];
    for(s32 Index = 0;
        Index < Count;
        ++Index)
    {
        world_triangle *T = Triangles + Index; 
        r32 Z = 10.0f;

        triangle ConvertedT = {};
        ConvertedT.Vertices[0] = Subtract(World, &T->V1, &SimRegion->Origin);
        ConvertedT.Vertices[1] = Subtract(World, &T->V2, &SimRegion->Origin);
        ConvertedT.Vertices[2] = Subtract(World, &T->V3, &SimRegion->Origin);
//        CalculateTriangleBoundingBox(&ConvertedT);
#if 1
        v2 Center = OneOverThree*(ConvertedT.Vertices[0] + ConvertedT.Vertices[1] + ConvertedT.Vertices[2]);
        BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, &Flat, V3(Center, 0.0f));
        P = Unproject(&UIState->RenderGroup, &Flat, BasisP.P);
        FormatString(ArrayCount(Buffer), Buffer, "%d", Index);
        UITextOutAt(UIState, P.xy, Buffer, 0.8f);
#endif        
//        PushTriangle(RenderGroup, &Flat, ConvertedT, Z, V4(RectanglesIntersect(ConvertedT.Bounds, Bounds) ? V3(1, 0, 0) : DebugColorTable[Index % ArrayCount(DebugColorTable)], 0.5f));
        PushTriangle(RenderGroup, &Flat, ConvertedT, Z, V4(DebugColorTable[Index % ArrayCount(DebugColorTable)], 0.5f));
    }
}

//#include "subtruct_poly.cpp"
inline controlled_camera *
CheckForInput(editor_mode_game *GameMode, engine_input *Input)
{
    controlled_camera *Result = 0;
    for(int ControllerIndex = 0;
        ControllerIndex < ArrayCount(Input->Controllers);
        ++ControllerIndex)
    {
        engine_controller_input *Controller = GetController(Input, ControllerIndex);
        Result = GameMode->ControlledCameras + ControllerIndex;

        if(Controller->IsConnected)
        {
            if(Controller->IsAnalog)
            {
            }
            else
            {

                if(WasPressed(Controller->FirstMode))
                {
                    GameMode->GameEditMode = EditGameMode_None;
                }

                if(WasPressed(Controller->SecondMode))
                {
                    GameMode->GameEditMode = EditGameMode_Terrain;
                }

                if(WasPressed(Controller->ThirdMode))
                {
                    GameMode->GameEditMode = EditGameMode_NavMeshes;
                }

                if(WasPressed(Controller->RightShoulder))
                {
                    if(IsSetGameModeFlag(GameMode, GMFlag_EditEnable))
                    {
                        ClearGameModeFlag(GameMode, GMFlag_EditEnable);
                    }
                    else
                    {
                        SetGameModeFlag(GameMode, GMFlag_EditEnable);
                    }
                }

                if(Input->ControlDown)
                    GameMode->Zoom += (f32)-0.8f*Input->MouseZ;

                if(WasPressed(Input->MouseButtons[PlatformMouseButton_Middle]))
                    GameMode->Zoom = 0.0f;
                
                if(WasPressed(Controller->MoveUp))
                    GameMode->WorldState->CameraP.TileY += GameMode->CameraMoveStep;

                if(WasPressed(Controller->MoveDown))
                    GameMode->WorldState->CameraP.TileY -= GameMode->CameraMoveStep;

                if(WasPressed(Controller->MoveLeft))
                    GameMode->WorldState->CameraP.TileX -= GameMode->CameraMoveStep;

                if(WasPressed(Controller->MoveRight))
                    GameMode->WorldState->CameraP.TileX += GameMode->CameraMoveStep;

                if(WasPressed(Controller->ActionLeft))
                {
                    if(GameMode->CameraMoveStep > 1)
                    {
                        GameMode->CameraMoveStep -= 1;
                    }
                }

                if(WasPressed(Controller->ActionRight))
                    GameMode->CameraMoveStep += 1;

                if(Input->ShiftDown && Input->AltDown && WasPressed(Controller->Undo))
                {
                    RedoTileChanges(GameMode->WorldState->World, &GameMode->UndoStack, &GameMode->RedoStack);
                }
                else if(Input->AltDown && WasPressed(Controller->Undo))
                {
                    UndoTileChanges(GameMode->WorldState->World, &GameMode->UndoStack, &GameMode->RedoStack);
                }
                
                switch(GameMode->GameEditMode)
                {
                    case EditGameMode_None:
                    {
                    } break;

                    case EditGameMode_Terrain:
                    {
                        if(WasPressed(Controller->LeftShoulder))
                            ToggleGMFlag(GameMode, GMFlag_ShowCurrentLayer);

                        if(WasPressed(Controller->Fill))
                            GameMode->FillActive = !GameMode->FillActive;

                        if(WasPressed(Controller->ActionUp))
                        {
                            GameMode->CurrentZLayer += 1;
                            if(GameMode->CurrentZLayer >= 16)
                            {
                                GameMode->CurrentZLayer = 15;
                                GameMode->WorldState->World->Map->Header->GroundLayer_ZLayerCount &= 0xFFFF0010;
                            }
                        }

                        if(WasPressed(Controller->ActionDown))
                        {
                            GameMode->CurrentZLayer -= 1;
                            if(GameMode->CurrentZLayer < 0)
                            {
                                GameMode->CurrentZLayer = 0;
                            }
                        }
                    } break;

                    case EditGameMode_NavMeshes:
                    {
                        if(WasPressed(Controller->Start) && Input->ControlDown)
                            GameMode->CurrentAction = GMAction_NavMeshPlaceEnd;
                        else if(WasPressed(Controller->Start))
                            GameMode->CurrentAction = GMAction_NavMeshPlaceStart;
                    } break;
                }
            }
        }
    }

    return(Result);
}

internal b32
UpdateAndRenderGameMode(editor_state *EditorState, transient_state *TranState, render_group *RenderGroup,
                        engine_input *Input, u32 RenderWidth, u32 RenderHeight)
{
    editor_assets *Assets = TranState->Assets;
    editor_mode_game *GameMode = EditorState->GameMode;
    ui_state *UIState = &EditorState->UIState;

    b32 Result = false;//CheckForMetaInput(EditorState, TranState, Input);
    if(!Result)
    {
        nk_ui *UI = UIState->UI;
        nk_context *Nk = UIState->Nk;
        
        real32 WidthOfMonitor = 0.635f; // NOTE(casey): Horizontal measurement of monitor in meters
        real32 MetersToPixels = (real32)RenderWidth/WidthOfMonitor;

        real32 FocalLength = 0.2f;
        real32 DistanceAboveGround = 7.2f;
        Perspective(RenderGroup, MetersToPixels, FocalLength, DistanceAboveGround);

        Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 1.0f));
    
        v2 ScreenCenter = {0.5f*(real32)RenderWidth, 0.5f*(real32)RenderHeight};

        rectangle2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);
        rectangle2 CameraBoundsInMeters = RectMinMax(ScreenBounds.Min, ScreenBounds.Max);

        object_transform Flat = DefaultFlatTransform();
        v2 MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;

        DrawGameModeUI(GameMode, TranState->Assets, TranState->MainGenerationID, UIState);

        b32 Exit = false;
        switch(GameMode->CurrentAction)
        {
            case GMAction_None:
            {
                // NOTE(babykaban): Do nothing
            } break;

            case GMAction_Exit:
            {
                Exit = true;
            } break;

            case GMAction_WriteSSWM:
            {
                if((GameMode->AutoWriteSeconds - Input->dtForFrame) > 0.0f)
                {
                    u32 GroundLayer_ZLayerCount = (u32)((GameMode->MapGroundLayer << 16) | (GameMode->LayerCount & 0xFFFF));
                    WriteSSWM(EditorState, GameMode->WorldState->World, GroundLayer_ZLayerCount);
                    GameMode->AutoWriteSeconds = 300.0f;
                }
            } break;

            case GMAction_EditEnable:
            {
                ToggleGMFlag(GameMode, GMFlag_EditEnable);
            } break;

            case GMAction_ShowCurrentLayer:
            {
                ToggleGMFlag(GameMode, GMFlag_ShowCurrentLayer);
            } break;
        }
        
        if(!Exit)
        {
            controlled_camera *ConCamera = CheckForInput(GameMode, Input);

            RenderGroup->CameraTransform.DistanceAboveTarget += GameMode->Zoom;
            MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;
            
            PushRect(RenderGroup, &Flat, V3(0, 0, 0), V2(0.25f, 0.25f), V4(0, 1, 0, 1.0f));

            world *World = GameMode->WorldState->World;
            memory_arena *WorldArena = &World->Arena;
            {DEBUG_DATA_BLOCK("EditorGameMode");
                {DEBUG_DATA_BLOCK("Memory");
                    DEBUG_VALUE(WorldArena);
                }
            }

            v2 SimBoundsExpansion = {3.0f + GameMode->Zoom, 3.0f + GameMode->Zoom};
            rectangle2 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundsExpansion);
            temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
            world_position SimCenterP = GameMode->WorldState->CameraP;

            sim_region *SimRegion = BeginSim(&TranState->TranArena, GameMode->WorldState->World,
                                             SimCenterP, SimBounds, Input->dtForFrame);

#if 0    
            PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1), 0.05f);
            PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(SimRegion->Bounds), V4(0.0f, 1.0f, 1.0f, 1));
            PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(SimRegion->UpdatableBounds), V4(1.0f, 0.0f, 1.0f, 1));
#endif

            UI->NkLayoutRowStatic(Nk, 20, 120, 1);
            UI->NkCheckboxLabel(Nk, "Show Grid", &GameMode->ShowGrid);

            if(GameMode->ShowGrid)
            {
                RenderMapGrid(RenderGroup, UIState, World, GameMode->WorldState->CameraP, SimRegion->Bounds);
            }

            world_position MouseWorldP = MapIntoTileSpace(World, SimRegion->Origin, MouseP);
            v2 CameraP = Subtract(World, &GameMode->WorldState->CameraP, &SimCenterP);

            
            if(IsSetGameModeFlag(GameMode, GMFlag_ShowCurrentLayer))
            {
                RenderMapGroundTiles(RenderGroup, World, GameMode->WorldState->CameraP, SimBounds, GameMode->CurrentZLayer, true);
            }
            else
            {
                RenderMapGroundTiles(RenderGroup, World, GameMode->WorldState->CameraP, SimBounds, GameMode->LayerCount);
            }
        
//            UpdateAndRenderEntities(GameMode, SimRegion, RenderGroup, Input->dtForFrame, MouseP);

            switch(GameMode->GameEditMode)
            {
                case EditGameMode_None:
                {
                } break;

                case EditGameMode_Terrain:
                {
                    if(IsSetGameModeFlag(GameMode, GMFlag_EditEnable))
                    {
                        if(GameMode->FillActive)
                        {
                            if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
                            {
                                TileFloodFill(&GameMode->UndoStack, World, MouseWorldP, GameMode->Tile, GameMode->CurrentZLayer);
                            }
                            else if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
                            {
                                ssa_tile Tile = {};
                                TileFloodFill(&GameMode->UndoStack, World, MouseWorldP, Tile, GameMode->CurrentZLayer);
                            }
                        }
                        else
                        {
                            if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
                            {
                                AddGroundTile(GameMode, World, MouseWorldP, false);
                            }
                            else if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
                            {
                                RemoveGroundTile(&GameMode->UndoStack, GameMode, World, MouseWorldP);
                            }
                        }
                    }

                    GameMode->Tileset = PushTileset(RenderGroup, GameMode->CurrentTileset);
                    GameMode->TilesetInfo = GetTilesetInfo(Assets, GameMode->CurrentTileset);
                } break;

                case EditGameMode_NavMeshes:
                {
                    UpdateAndRenderNavMeshMode(GameMode, UIState, SimRegion, RenderGroup,
                                               &Flat, Input, MouseP);
                } break;

                InvalidDefaultCase;
            }

            GameMode->AutoWriteSeconds -= Input->dtForFrame;
            if(GameMode->AutoWriteSeconds <= 0.0f)
            {
                u32 GroundLayer_ZLayerCount = (u32)((GameMode->MapGroundLayer << 16) | (GameMode->LayerCount & 0xFFFF));
                WriteSSWM(EditorState, World, GroundLayer_ZLayerCount);
                GameMode->AutoWriteSeconds = 300.0f;
            }

            // NOTE(babykaban): Clear action
            GameMode->CurrentAction = 0;
            
            EndSim(GameMode->WorldState, SimRegion, CameraBoundsInMeters);
            EndTemporaryMemory(SimMemory);
        }
        else
        {
            PlayTitleScreen(EditorState, TranState);
        }
    }
    
    return(Result);
}

/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_game_mode_world.cpp"
#include "engine_game_mode_entity.cpp"
#include "engine_game_mode_sim_region.cpp"

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
WritePolygons(world_polygon *Polygons, s32 PolygonCount)
{
    FILE *Out;
    fopen_s(&Out, "polygons.nmp", "wb");
    if(Out)
    {
        fwrite(&PolygonCount, sizeof(u32), 1, Out);
        for(s32 PolygonIndex = 0;
            PolygonIndex < PolygonCount;
            ++PolygonIndex)
        {
            world_polygon *Poly = Polygons + PolygonIndex;
            u32 VerticesSize = Poly->VertexCount*sizeof(world_position);
            fwrite(&Poly->VertexCount, sizeof(u32), 1, Out);
            fwrite(Poly->Vertices, VerticesSize, 1, Out);
        }
    }

    fclose(Out);
}

internal void
PlayGameMode(editor_state *EditorState, transient_state *TranState)
{
    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    MatchVector.E[Tag_VersionMajorHigh] =
        EditorState->MapStartup.MapVersion.MajorHigh;
    MatchVector.E[Tag_VersionMajorLow] =
        EditorState->MapStartup.MapVersion.MajorLow;
    MatchVector.E[Tag_VersionMinorHigh] =
        EditorState->MapStartup.MapVersion.MinorHigh;
    MatchVector.E[Tag_VersionMinorLow] =
        EditorState->MapStartup.MapVersion.MinorLow;
    WeightVector.E[Tag_VersionMajorHigh] = 2;
    WeightVector.E[Tag_VersionMajorLow] = 2;
    WeightVector.E[Tag_VersionMinorHigh] = 2;
    WeightVector.E[Tag_VersionMinorLow] = 2;
    EditorState->MapStartup.ID = GetBestMatchSSWMFrom(TranState->Assets, Asset_SSWM, &MatchVector, &WeightVector);

    if(AbleToStart(EditorState->MapStartup))
    {
        SetEditorMode(EditorState, TranState, EditorMode_GameMode);
    
        editor_mode_game *Result = PushStruct(&EditorState->ModeArena, editor_mode_game);

        r32 PixelsToMeters = 1.0f / 32.0f;
        u32 TileSideInPixels = 32;
        r32 TileSideInMeters = TileSideInPixels * PixelsToMeters;

        if(EditorState->MapStartup.NewMap)
        {
            Result->World = CreateWorld(TranState, TileSideInMeters, EditorState->MapStartup.MapWidth,
                                        EditorState->MapStartup.MapHeight, 1, 0);
        }
        else
        {
            loaded_world_map *Map = PushSSWM(TranState, EditorState->MapStartup.ID, true);
            Result->World = CreateWorld(TranState, TileSideInMeters, Map->Header->MapWidth, Map->Header->MapHeight, 1, Map);

            Result->LayerCount = Map->Header->GroundLayer_ZLayerCount & 0xFFFF;
            Result->MapGroundLayer = Map->Header->GroundLayer_ZLayerCount >> 16;
        }

        world_position NewCameraP = {};
        uint32 CameraTileX = Result->World->TileWidth / 2;
        uint32 CameraTileY = Result->World->TileHeight / 2;
        NewCameraP = ChunkPositionFromTilePosition(Result->World, CameraTileX, CameraTileY);
        Result->CameraP = NewCameraP;
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

        Result->MeshTriangles = PushArray(&EditorState->ModeArena, 4096, world_triangle);
        Result->FreeTriangleIndices = PushArray(&EditorState->ModeArena, 1024, s32);
        
        Result->AutoWriteSeconds = 300.0f;

        Result->JsonStringsHead = ParseJson("enum_strings.json", &EditorState->ModeArena);

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
#if 0
                for(f32 I = -0.5f;
                    I < 0.5f;
                    I += 0.125f)
                {
                    for(f32 J = -0.5f;
                        J < 0.5f;
                        J += 0.125f)
                    {
                        rectangle2 R = RectCenterDim(Delta + V2(J + 0.05f, I + 0.05f), V2(0.05f, 0.05f));
                        PushRect(RenderGroup, &Transform, R, 20.0f, V4(0, 0, 1, 1));
                    }
                }
#endif
#if 1

                if(Global_EditorGameMode_ShowCoords)
                {
                    FormatString(ArrayCount(Text), Text, "%d,%d", TileX, TileY);
                    entity_basis_p_result BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform, &Transform, V3(Delta, 0.0f) - V3(0.32f, 0.48f, 0.0f));
                    v3 P = Unproject(&UIState->RenderGroup, &Transform, BasisP.P);
                    UITextOutAt(UIState, P.xy, Text, 0.6f);
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

                sswm_ground_tile *Tile = GetWorldMapGroundTile(World, TileX, TileY);
                
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
DrawPolygons(render_group *RenderGroup, world *World, world_polygon *Polygons, s32 Count, world_position BaseP, s32 CurrentPolygonIndex, memory_arena *Arena)
{
    object_transform Flat = DefaultFlatTransform();
    for(s32 Index = 0;
        Index < Count;
        ++Index)
    {
        world_polygon *Polygon = Polygons + Index;
        
        polygon2 TempPoly = {};
        TempPoly.VertexCount = Polygon->VertexCount;
        TempPoly.Vertices = PushArray(Arena, Polygon->VertexCount, v2);
        
        ConvertWorldPolygonToPolygon2(World, &BaseP, Polygon, &TempPoly);

        b32 Clockwise = (PolygonSignedArea2(&TempPoly) < 0.0f);
    
        s32 PrevOffset = Clockwise ? -1 : 1;
        s32 NextOffset = Clockwise ? 1 : -1;

        v4 Color = V4(0, 1, 0, 1);
        v4 VertexColor = V4(1, 0, 0, 1);
        r32 Z = 10.0f;
        if(Index == CurrentPolygonIndex)
        {
            Color = V4(1, 0, 1, 1);
            VertexColor = V4(0, 0.5f, 1, 1);
            Z = 12.0f;
        }

        for(s32 VertexIndex = 0;
            VertexIndex < Polygon->VertexCount;
            ++VertexIndex)
        {
            world_position *Vertex = Polygon->Vertices + VertexIndex;
            v2 Delta = Subtract(World, Vertex, &BaseP);
        
            if(VertexIndex == 0)
            {
                PushRect(RenderGroup, &Flat, V3(Delta, 52.0f), V2(0.1f, 0.1f), V4(1, 0, 1, 1));
            }
            else
            {
                PushRect(RenderGroup, &Flat, V3(Delta, Z), V2(0.1f, 0.1f),
                         (VertexIndex == Polygon->VertexCount - 1) ? V4(DebugColorTable[4], 1) : VertexColor);
            }
        }
#if 0
        if(Index == CurrentPolygonIndex)
        {
            for(s32 VertexIndex = 0;
                VertexIndex < Polygon->VertexCount;
                ++VertexIndex)
            {
                world_position *VertexPrev = Polygon->Vertices + GetIndex(Polygon->VertexCount, VertexIndex + PrevOffset);
                world_position *VertexCur = Polygon->Vertices + VertexIndex;
                world_position *VertexNext = Polygon->Vertices + GetIndex(Polygon->VertexCount, VertexIndex + NextOffset);
                v2 Delta0 = Subtract(World, VertexPrev, &BaseP);
                v2 Delta1 = Subtract(World, VertexCur, &BaseP);
                v2 Delta2 = Subtract(World, VertexNext, &BaseP);
                
                v2 a = Delta1 - Delta0;
                v2 b = Delta2 - Delta1;
                
                if(Cross(a, b) > 0.0f)
                {

                    v2 Perpendicular = Perp(Delta0 - Delta1);
                    v2 NewP = Delta1 + Normalize(Perpendicular)*10.0f;
                    PushLine(RenderGroup, &Flat, V3(Delta1, Z), V3(NewP, Z));
                    PushRect(RenderGroup, &Flat, V3(Delta1, 50.0f), V2(0.1f, 0.1f), V4(0, 0, 0, 1));
                }
            }
        }
#endif
        if(Polygon->VertexCount > 1)
        {
            for(s32 VertexIndex = 0;
                VertexIndex < (Polygon->VertexCount - 1);
                ++VertexIndex)
            {
                world_position *Vertex0 = Polygon->Vertices + VertexIndex;
                world_position *Vertex1 = Polygon->Vertices + VertexIndex + 1;
                v2 Delta0 = Subtract(World, Vertex0, &BaseP);
                v2 Delta1 = Subtract(World, Vertex1, &BaseP);

                PushLine(RenderGroup, &Flat, V3(Delta0, Z), V3(Delta1, Z), V4(0, 0, 0, 1));
            }

            if(Polygon->VertexCount > 2)
            {
                world_position Vertex0 = Polygon->Vertices[Polygon->VertexCount - 1];
                world_position Vertex1 = Polygon->Vertices[0];
                v2 Delta0 = Subtract(World, &Vertex0, &BaseP);
                v2 Delta1 = Subtract(World, &Vertex1, &BaseP);
                PushLine(RenderGroup, &Flat, V3(Delta0, Z), V3(Delta1, Z), V4(0, 0, 0, 1));
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

                if(WasPressed(Controller->MoveUp))
                    GameMode->CameraP.TileY += GameMode->CameraMoveStep;

                if(WasPressed(Controller->MoveDown))
                    GameMode->CameraP.TileY -= GameMode->CameraMoveStep;

                if(WasPressed(Controller->MoveLeft))
                    GameMode->CameraP.TileX -= GameMode->CameraMoveStep;

                if(WasPressed(Controller->MoveRight))
                    GameMode->CameraP.TileX += GameMode->CameraMoveStep;

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
                    RedoTileChanges(GameMode->World, &GameMode->UndoStack, &GameMode->RedoStack);
                }
                else if(Input->AltDown && WasPressed(Controller->Undo))
                {
                    UndoTileChanges(GameMode->World, &GameMode->UndoStack, &GameMode->RedoStack);
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
                                GameMode->World->Map->Header->GroundLayer_ZLayerCount &= 0xFFFF0010;
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
                        if(WasPressed(Controller->Start))
                            GameMode->CurrentAction = GMAction_SubtractRegion;
                    } break;
                }
            }
        }
    }

    return(Result);
}

internal b32
UpdateAndRenderGameMode(editor_state *EditorState, transient_state *TranState, render_group *RenderGroup,
                        nk_context *Nk, engine_input *Input, u32 RenderWidth, u32 RenderHeight,
                        editor_mode_game *GameMode)
{
    editor_assets *Assets = TranState->Assets;
    ui_state *UIState = &EditorState->UIState;
    b32 Result = false;//CheckForMetaInput(EditorState, TranState, Input);
    if(!Result)
    {
        nk_ui *UI = &Platform.UI;

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

        DrawGameModeUI(GameMode, TranState->Assets, TranState->MainGenerationID, UI, Nk, Input->MouseZ);

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
                    WriteSSWM(EditorState, GameMode->World, GroundLayer_ZLayerCount);
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

            PushRect(RenderGroup, &Flat, V3(0, 0, 0), V2(0.25f, 0.25f), V4(0, 1, 0, 1.0f));

            world *World = GameMode->World;
            memory_arena *WorldArena = &World->Arena;
            {DEBUG_DATA_BLOCK("EditorGameMode");
                {DEBUG_DATA_BLOCK("Memory");
                    DEBUG_VALUE(WorldArena);
                }
            }

            v2 SimBoundsExpansion = {3.0f, 3.0f};
            rectangle2 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundsExpansion);
            temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
            world_position SimCenterP = GameMode->CameraP;
            sim_region *SimRegion = BeginSim(&TranState->TranArena, GameMode->World,
                                             SimCenterP, SimBounds, Input->dtForFrame);

#if 0    
            PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1), 0.05f);
            PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(SimRegion->Bounds), V4(0.0f, 1.0f, 1.0f, 1));
            PushRectOutline(RenderGroup, &Flat, V3(0.0f, 0.0f, 0.0f), GetDim(SimRegion->UpdatableBounds), V4(1.0f, 0.0f, 1.0f, 1));
#endif

            UI->NkLayoutRowStatic(Nk, 30, 120, 1);
            UI->NkCheckboxLabel(Nk, "Show Grid", &GameMode->ShowGrid);
            if(GameMode->ShowGrid)
            {
                RenderMapGrid(RenderGroup, UIState, World, GameMode->CameraP, SimRegion->Bounds);
            }

            world_position MouseWorldP = MapIntoTileSpace(World, SimRegion->Origin, MouseP);
            v2 CameraP = Subtract(World, &GameMode->CameraP, &SimCenterP);

            
            if(IsSetGameModeFlag(GameMode, GMFlag_ShowCurrentLayer))
            {
                RenderMapGroundTiles(RenderGroup, World, GameMode->CameraP, SimBounds, GameMode->CurrentZLayer, true);
            }
            else
            {
                RenderMapGroundTiles(RenderGroup, World, GameMode->CameraP, SimBounds, GameMode->LayerCount);
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
                    GameMode->CurrentPolygon = GameMode->Polies + GameMode->CurrentPolygonIndex;

                    v2 PointDim = V2(0.1f, 0.1f);
                    v2 P = (MouseP - 0.5f*PointDim) * (1.0f / PointDim.x);
                    s32 X = RoundReal32ToInt32(P.x);
                    s32 Y = RoundReal32ToInt32(P.y);

                    v2 PointP = PointDim.x*V2i(X, Y) + 0.5f*PointDim;
                    world_position TestP = MapIntoTileSpace(World, SimRegion->Origin, PointP);
                    PushRect(RenderGroup, &Flat, V3(PointP, 8.0f), PointDim);

                    rectangle2 MouseRect = RectCenterDim(MouseP, V2(2.0f, 2.0f));

                    polygon2 Poly = {};
                    temporary_memory TempMem = BeginTemporaryMemory(&World->Arena);
#if 0
                    Poly.VertexCount = 8;
                    Poly.Vertices = PushArray(TempMem.Arena, 8, v2);
                    Poly.Vertices[0] = V2(-1.0f, -2.0f);
                    Poly.Vertices[1] = V2(1.0f, -2.0f);

                    Poly.Vertices[2] = V2(2.0f, -1.0f);
                    Poly.Vertices[3] = V2(2.0f, 1.0f);;

                    Poly.Vertices[4] = V2(1.0f, 2.0f);;
                    Poly.Vertices[5] = V2(-1.0f, 2.0f);;

                    Poly.Vertices[6] = V2(-2.0f, 1.0f);;
                    Poly.Vertices[7] = V2(-2.0f, -1.0f);;
#endif
                    Poly.VertexCount = 4;
                    Poly.Vertices = PushArray(TempMem.Arena, 4, v2);
                    Poly.Vertices[0] = V2(-0.5f, -0.5f);
                    Poly.Vertices[1] = V2(0.5f, -0.5f);

                    Poly.Vertices[2] = V2(0.5f, 0.5f);
                    Poly.Vertices[3] = V2(-0.5f, 0.5f);;
                    
                    triangulate_result TriangulatedPoly = ConstrainedDelaunayTriangulate(&Poly, TempMem.Arena);
                    for(s32 I = 0;
                        I < 1;//TriangulatedPoly.TriangleCount;
                        ++I)
                    {
                        PushTriangle(RenderGroup, &Flat, TriangulatedPoly.Triangles[I], 60.0f, V4(0, 0, 0, 0.5f));
                    }
                    Platform.DeallocateMemory(TriangulatedPoly.Triangles);
                    Platform.DeallocateMemory(TriangulatedPoly.Adjacencies);

                    switch(GameMode->CurrentAction)
                    {
                        case GMAction_StartNewPolygon:
                        {
                            StartNewPolygon(GameMode);
                        } break;

                        case GMAction_ResetCurrentPolygon:
                        {
                            ResetPolygon(GameMode->CurrentPolygon);
                        } break;

                        case GMAction_DeleteCurrentPolygon:
                        {
                            DeletePolygon(GameMode);
                        } break;

                        case GMAction_WritePolygons:
                        {
                            WritePolygons(GameMode->Polies, GameMode->PolygonCount);
                        } break;

                        case GMAction_TriangulateAll:
                        {
                            TriangulatePolygons(RenderGroup, &Flat, GameMode, &SimRegion->Origin, &World->Arena);
//                            BuildAdjacenciesArray(GameMode, SimRegion);
                            GameMode->Triangulated = true;
                        } break;

                        case GMAction_SubtractRegion:
                        {
                            SubtractPolyFromMesh(GameMode, SimRegion, &Poly, &World->Arena);
                        } break;
                    }
                        
                    if(IsSetGameModeFlag(GameMode, GMFlag_EditEnable))
                    {
                        if(GameMode->ChosenVertex)
                        {
                            *GameMode->ChosenVertex = TestP;
                        }
                    
                        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
                        {
                            AddVertex(GameMode, GameMode->CurrentPolygon, World, TestP);
                        }
                        else if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
                        {
                            RemoveVertex(GameMode, GameMode->CurrentPolygon, World, TestP);
                        }
                    }

                    DrawPolygons(RenderGroup, World, GameMode->Polies, GameMode->PolygonCount, SimRegion->Origin, GameMode->CurrentPolygonIndex,
                                 TempMem.Arena);

                    EndTemporaryMemory(TempMem);
                    
                    if(GameMode->Triangulated)
                    {
//                            BuildAdjacenciesArray(GameMode, SimRegion);
//                            MergeTriangels(RenderGroup, &Flat, GameMode, &SimRegion->Origin, &World->Arena);
                        DrawMeshTriangles(UIState, RenderGroup, World, GameMode->MeshTriangles, GameMode->MeshTriangleCount, SimRegion, MouseRect);
                    }
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
            
            EndSim(SimRegion, CameraBoundsInMeters);
            EndTemporaryMemory(SimMemory);
        }
        else
        {
            PlayTitleScreen(EditorState, TranState);
        }
    }
    
    return(Result);
}

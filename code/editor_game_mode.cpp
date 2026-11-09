/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice:  $
   ======================================================================== */

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): ENGINE API, the engine side of the table the games call through (engine_api.h)
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal render_group
GameBeginRenderGroup(engine_assets *Assets, editor_render_commands *Commands)
{
    u32 GenerationID = Assets ? BeginGeneration(Assets) : 0;
    render_group Result = BeginRenderGroup(Assets, Commands, GenerationID, false,
                                           Commands->Width, Commands->Height);
    return(Result);
}

internal void
GameEndRenderGroup(render_group *Group)
{
    EndRenderGroup(Group);
    if(Group->Assets)
    {
        EndGeneration(Group->Assets, Group->GenerationID);
    }
}

// NOTE(pvlso): PushTriangle is overloaded, the table needs the one taking three corners
internal void
GamePushTriangle(render_group *Group, object_transform *ObjectTransform, v3 A, v3 B, v3 C, v4 Color)
{
    PushTriangle(Group, ObjectTransform, A, B, C, Color);
}

internal void
GamePushBitmap(render_group *Group, object_transform *ObjectTransform, bitmap_id ID, r32 Height, v3 Offset,
               v4 Color, r32 CAlign, r32 Angle, bitmap_id NormalMap)
{
    if(Group->Assets)
    {
        PushBitmap(Group, ObjectTransform, ID, Height, Offset, Color, CAlign, Angle, NormalMap);
    }
}

internal bitmap_id
GameGetNormalMap(engine_assets *Assets, bitmap_id Bitmap)
{
    bitmap_id Result = {};
    if(Assets && Bitmap.Value)
    {
        // NOTE(pvlso): An ID is an asset index or a GUID
        u64 GUID = (Bitmap.Value < Assets->AssetCount) ? Assets->Assets[Bitmap.Value].KEA.GUID : Bitmap.Value;
        u64 NormalGUID = NormalMapGUID(GUID);
        if(GetAssetByGUID(Assets, NormalGUID))
        {
            Result.Value = NormalGUID;
        }
    }

    return(Result);
}

internal asset_best_match_result
GameGetBestMatchAssets(engine_assets *Assets, asset_tag_vector *Vector, memory_arena *Arena)
{
    asset_best_match_result Result = {};
    if(Assets)
    {
        Result = GetBestMatchAssets(Assets, Vector, Arena);
    }

    return(Result);
}

// NOTE(pvlso): The sheet only holds its sprites' GUIDs, so it's loaded right away the first time
internal bitmap_id
GameGetSprite(engine_assets *Assets, u32 SheetIndex, u32 SpriteIndex)
{
    bitmap_id Result = {};
    if(Assets && SheetIndex && (SheetIndex < Assets->AssetCount))
    {
        asset *Asset = Assets->Assets + SheetIndex;
        if((Asset->KEA.Type == KEAType_SpriteSheet) && (SpriteIndex < Asset->KEA.SpriteSheet.SpriteCount))
        {
            spritesheet_id SheetID = {SheetIndex};
            loaded_spritesheet *Sheet = GetSpriteSheet(Assets, SheetID, 0);
            if(!Sheet)
            {
                LoadSpriteSheet(Assets, SheetID, true);
                Sheet = GetSpriteSheet(Assets, SheetID, 0);
            }

            if(Sheet)
            {
                Result = Sheet->SpriteGUIDs[SpriteIndex];
            }
        }
    }

    return(Result);
}

// NOTE(pvlso): The renderer compiles the source, the text only has to stay loaded through this frame
// NOTE(pvlso): The tile set at TilesetIndex, loaded now if it isn't yet; 0 when it isn't a tile set
internal loaded_tileset *
GameLoadedTileset(engine_assets *Assets, u32 TilesetIndex, kea_tileset **Info)
{
    loaded_tileset *Result = 0;
    if(Assets && TilesetIndex && (TilesetIndex < Assets->AssetCount))
    {
        asset *Asset = Assets->Assets + TilesetIndex;
        if(Asset->KEA.Type == KEAType_Tileset)
        {
            tileset_id TilesetID = {TilesetIndex};
            Result = GetTileset(Assets, TilesetID, 0);
            if(!Result)
            {
                LoadTileset(Assets, TilesetID, true);
                Result = GetTileset(Assets, TilesetID, 0);
            }
            *Info = &Asset->KEA.Tileset;
        }
    }

    return(Result);
}

internal bitmap_id
GameGetTilesetTile(engine_assets *Assets, u32 TilesetIndex, u32 TileIndex)
{
    bitmap_id Result = {};
    kea_tileset *Info = 0;
    loaded_tileset *Tileset = GameLoadedTileset(Assets, TilesetIndex, &Info);
    if(Tileset && (TileIndex < Info->TileCount))
    {
        Result.Value = Tileset->Tiles[TileIndex].BitmapGUID;
    }

    return(Result);
}

internal tileset_info
GameGetTilesetInfo(engine_assets *Assets, u32 TilesetIndex)
{
    tileset_info Result = {};
    kea_tileset *Info = 0;
    if(GameLoadedTileset(Assets, TilesetIndex, &Info))
    {
        Result.TileCount = Info->TileCount;
        Result.GridWidth = Info->GridWidth;
        Result.GridHeight = Info->GridHeight;
        Result.TileSize = Info->TileSize;
    }

    return(Result);
}

internal tile_info
GameGetTileInfo(engine_assets *Assets, u32 TilesetIndex, u32 TileIndex, r32 Time)
{
    tile_info Result = {};
    kea_tileset *Info = 0;
    loaded_tileset *Tileset = GameLoadedTileset(Assets, TilesetIndex, &Info);
    if(Tileset && (TileIndex < Info->TileCount))
    {
        kea_tile *Tile = Tileset->Tiles + TileIndex;
        Result.Bitmap.Value = Tile->BitmapGUID;
        Result.Flags = Tile->Flags;
        Result.Kind = Tile->Kind;
        Result.GridX = Tile->GridX;
        Result.GridY = Tile->GridY;
        Result.Animated = (Tile->FrameCount > 1);
        FormatString(sizeof(Result.Name), Result.Name, "%.*s", KEA_TILE_NAME_LENGTH, Tile->Name);
        if(Result.Animated && (Tile->FirstFrame + Tile->FrameCount <= Info->FrameCount))
        {
            u32 Frame = (u32)FloorReal32ToInt32(Time*Tile->FramesPerSecond) % Tile->FrameCount;
            Result.Bitmap = Tileset->Frames[Tile->FirstFrame + Frame];
        }
    }

    return(Result);
}

// NOTE(pvlso): The map at MapIndex, loaded now if it isn't yet; 0 when it isn't a map
internal loaded_map *
GameLoadedMap(engine_assets *Assets, u32 MapIndex, kea_map **Info)
{
    loaded_map *Result = 0;
    if(Assets && MapIndex && (MapIndex < Assets->AssetCount))
    {
        asset *Asset = Assets->Assets + MapIndex;
        if(Asset->KEA.Type == KEAType_Map)
        {
            asset_memory_header *Header = GetAsset(Assets, MapIndex, 0);
            if(!Header)
            {
                LoadAsset(Assets, AssetType_Map, MapIndex, true);
                Header = GetAsset(Assets, MapIndex, 0);
            }
            Result = Header ? &Header->Map : 0;
            *Info = &Asset->KEA.Map;
        }
    }

    return(Result);
}

internal map_info
GameGetMapInfo(engine_assets *Assets, u32 MapIndex)
{
    map_info Result = {};
    kea_map *Info = 0;
    if(GameLoadedMap(Assets, MapIndex, &Info))
    {
        Result.Width = Info->Width;
        Result.Height = Info->Height;
        Result.LayerCount = Info->LayerCount;
        Result.SetCount = Info->SetCount;
        Result.EntityCount = Info->EntityCount;
        Result.NavPolyCount = Info->NavPolyCount;
    }

    return(Result);
}

internal map_layer_info
GameGetMapLayer(engine_assets *Assets, u32 MapIndex, u32 LayerIndex)
{
    map_layer_info Result = {};
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map && (LayerIndex < Info->LayerCount))
    {
        kea_map_layer *Layer = Map->Layers + LayerIndex;
        FormatString(sizeof(Result.Name), Result.Name, "%.*s", (int)sizeof(Layer->Name), Layer->Name);
        Result.Z = Layer->Z;
        Result.Level = Layer->Level;
        Result.Hidden = (Layer->Flags & KEAMapLayer_Hidden) != 0;
        Result.Cells = (map_tile_cell *)(Map->Cells + (umm)LayerIndex*Info->Width*Info->Height);
    }

    return(Result);
}

internal u32
GameGetMapTileset(engine_assets *Assets, u32 MapIndex, u32 SetIndex)
{
    u32 Result = 0;
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map && (SetIndex < Info->SetCount))
    {
        asset *Set = GetAssetByGUID(Assets, Map->SetGUIDs[SetIndex]);
        if(Set && (Set->KEA.Type == KEAType_Tileset))
        {
            Result = (u32)(Set - Assets->Assets);
        }
    }

    return(Result);
}

// NOTE(pvlso): The entity at EntityIndex, loaded now if it isn't yet; 0 when it isn't an entity
internal loaded_entity *
GameLoadedEntity(engine_assets *Assets, u32 EntityIndex, kea_entity **Info)
{
    loaded_entity *Result = 0;
    if(Assets && EntityIndex && (EntityIndex < Assets->AssetCount))
    {
        asset *Asset = Assets->Assets + EntityIndex;
        if(Asset->KEA.Type == KEAType_Entity)
        {
            asset_memory_header *Header = GetAsset(Assets, EntityIndex, 0);
            if(!Header)
            {
                LoadAsset(Assets, AssetType_Entity, EntityIndex, true);
                Header = GetAsset(Assets, EntityIndex, 0);
            }
            Result = Header ? &Header->Entity : 0;
            *Info = &Asset->KEA.Entity;
        }
    }

    return(Result);
}

// NOTE(pvlso): Every entity's name is in the asset list, no entity is loaded to find one
internal u32
GameFindEntity(engine_assets *Assets, char *Name)
{
    u32 Result = 0;
    for(u32 AssetIndex = 1; Assets && Name && !Result && (AssetIndex < Assets->AssetCount); ++AssetIndex)
    {
        kea_asset *KEA = &Assets->Assets[AssetIndex].KEA;
        if((KEA->Type == KEAType_Entity) && StringsAreEqual(Name, KEA->Entity.Name))
        {
            Result = AssetIndex;
        }
    }

    return(Result);
}

internal u32
GameFindMap(engine_assets *Assets, char *FileName)
{
    u32 Result = 0;
    asset *Asset = (Assets && FileName && FileName[0]) ? GetAssetByGUID(Assets, GUIDFromString(FileName)) : 0;
    if(Asset && (Asset->KEA.Type == KEAType_Map))
    {
        Result = (u32)(Asset - Assets->Assets);
    }

    return(Result);
}

internal u32
GameFindEntityByGUID(engine_assets *Assets, u64 GUID)
{
    u32 Result = 0;
    asset *Asset = (Assets && GUID) ? GetAssetByGUID(Assets, GUID) : 0;
    if(Asset && (Asset->KEA.Type == KEAType_Entity))
    {
        Result = (u32)(Asset - Assets->Assets);
    }

    return(Result);
}

inline entity_shape_info
EntityShapeInfo(kea_entity_shape *Shape)
{
    entity_shape_info Result;
    Result.Shape = (entity_collision_shape)Shape->Shape;
    Result.Center = V2(Shape->Center[0], Shape->Center[1]);
    Result.Size = V2(Shape->Size[0], Shape->Size[1]);

    return(Result);
}

internal entity_info
GameGetEntityInfo(engine_assets *Assets, u32 EntityIndex)
{
    entity_info Result = {};
    kea_entity *Info = 0;
    loaded_entity *Entity = GameLoadedEntity(Assets, EntityIndex, &Info);
    if(Entity)
    {
        Result.GUID = Assets->Assets[EntityIndex].KEA.GUID;
        FormatString(sizeof(Result.Name), Result.Name, "%.*s", KEA_ENTITY_NAME_LENGTH, Info->Name);
        FormatString(sizeof(Result.DisplayName), Result.DisplayName, "%.*s", KEA_ENTITY_DISPLAY_LENGTH,
                     Entity->DisplayName);
        FormatString(sizeof(Result.Type), Result.Type, "%.*s", KEA_ENTITY_NAME_LENGTH, Entity->Type);
        Result.ClipCount = Info->ClipCount;
        Result.Footprint = EntityShapeInfo(&Info->Footprint);
        Result.HitArea = EntityShapeInfo(&Info->HitArea);
    }

    return(Result);
}

internal entity_clip_info
GameGetEntityClip(engine_assets *Assets, u32 EntityIndex, u32 ClipIndex, r32 Time)
{
    entity_clip_info Result = {};
    kea_entity *Info = 0;
    loaded_entity *Entity = GameLoadedEntity(Assets, EntityIndex, &Info);
    if(Entity && (ClipIndex < Info->ClipCount))
    {
        kea_entity_clip *Clip = Entity->Clips + ClipIndex;
        FormatString(sizeof(Result.Name), Result.Name, "%.*s", KEA_ENTITY_NAME_LENGTH, Clip->Name);
        Result.FramesPerSecond = Clip->FramesPerSecond;
        Result.Loop = Clip->Loop;
        Result.Align = V2(Clip->Align[0], Clip->Align[1]);
        asset *Look = GetAssetByGUID(Assets, Clip->LookGUID);
        if(Look)
        {
            u32 LookIndex = (u32)(Look - Assets->Assets);
            if(Look->KEA.Type == KEAType_Bitmap)
            {
                Result.Bitmap.Value = LookIndex;
                Result.FrameCount = 1;
            }
            else if(Look->KEA.Type == KEAType_SpriteSheet)
            {
                Result.SpriteSheet = LookIndex;
                Result.FrameCount = Look->KEA.SpriteSheet.SpriteCount;
                u32 Frame = (u32)Maximum(0, FloorReal32ToInt32(Time*Clip->FramesPerSecond));
                if(Result.FrameCount)
                {
                    if(Result.Loop) {Frame %= Result.FrameCount;}
                    else if(Frame >= Result.FrameCount) {Frame = Result.FrameCount - 1; Result.Finished = true;}
                    Result.Bitmap = GameGetSprite(Assets, LookIndex, Frame);
                }
            }
        }
        u64 Frame = Result.Bitmap.Value;
        if(Frame && (Frame < Assets->AssetCount) &&
           ((Assets->Assets[Frame].KEA.Type == KEAType_Bitmap) || (Assets->Assets[Frame].KEA.Type == KEAType_Sprite)))
        {
            Result.Dim = V2((r32)Assets->Assets[Frame].KEA.Bitmap.Dim[0], (r32)Assets->Assets[Frame].KEA.Bitmap.Dim[1]);
        }
    }

    return(Result);
}

internal map_entity_info
GameGetMapEntity(engine_assets *Assets, u32 MapIndex, u32 EntityIndex)
{
    map_entity_info Result = {};
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map && (EntityIndex < Info->EntityCount))
    {
        kea_map_entity *Entity = Map->Entities + EntityIndex;
        Result.GUID = Entity->GUID;
        Result.Definition = Entity->Definition;
        Result.Entity = GameFindEntityByGUID(Assets, Entity->Definition);
        Result.P = V2(Entity->P[0], Entity->P[1]);
        Result.Flip = (Entity->Flags & KEAMapEntity_Flip) != 0;
        Result.Level = Entity->Level;
    }

    return(Result);
}

internal s32
GameFindNavPoly(engine_assets *Assets, u32 MapIndex, v2 P, s32 Level)
{
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    s32 Result = Map ? FindMapNavPoly(&Map->Nav, P, Level) : -1;

    return(Result);
}

internal s32
GameGetNavPolyLevel(engine_assets *Assets, u32 MapIndex, s32 Poly)
{
    s32 Result = 0;
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map && (Poly >= 0) && ((u32)Poly < Map->Nav.PolyCount))
    {
        Result = Map->Nav.Polys[Poly].Level;
    }

    return(Result);
}

internal u32
GameGetNavPolyPoints(engine_assets *Assets, u32 MapIndex, s32 Poly, v2 *Points, u32 MaxPoints)
{
    u32 Result = 0;
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map && (Poly >= 0) && ((u32)Poly < Map->Nav.PolyCount))
    {
        kea_nav_poly *Found = Map->Nav.Polys + Poly;
        Result = Minimum(Found->PointCount, MaxPoints);
        for(u32 Index = 0; Index < Result; ++Index)
        {
            Points[Index] = Map->Nav.Points[Found->FirstPoint + Index];
        }
    }

    return(Result);
}

internal u32
GameFindNavPath(engine_assets *Assets, u32 MapIndex, v2 From, s32 FromLevel, v2 To, s32 ToLevel, nav_path_point *Points,
                u32 MaxPoints)
{
    u32 Result = 0;
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map)
    {
        Result = FindMapNavPath(&Map->Nav, &Map->NavScratch, From, FromLevel, To, ToLevel, Points, MaxPoints);
    }

    return(Result);
}

internal v2
GameMoveOnNav(engine_assets *Assets, u32 MapIndex, v2 From, s32 *Level, v2 To)
{
    v2 Result = To;
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map && Map->Nav.PolyCount)
    {
        Result = MoveMapNav(&Map->Nav, From, Level, To);
    }

    return(Result);
}

internal v2
GameClosestNavPointOnLevel(engine_assets *Assets, u32 MapIndex, v2 P, s32 Level)
{
    v2 Result = P;
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map)
    {
        ClosestMapNavPointOnLevel(&Map->Nav, &Result, Level);
    }

    return(Result);
}

internal v2
GameClosestNavPoint(engine_assets *Assets, u32 MapIndex, v2 P, s32 *Level)
{
    v2 Result = P;
    kea_map *Info = 0;
    loaded_map *Map = GameLoadedMap(Assets, MapIndex, &Info);
    if(Map)
    {
        ClosestMapNavPoint(&Map->Nav, &Result, Level);
    }

    return(Result);
}

internal void
GameSetPixelWorldShader(render_group *Group, u32 ShaderIndex, render_shader_params Params)
{
    render_shader *Shader = &Group->Commands->PixelWorld.Shader;
    *Shader = {};
    Shader->Params = Params;

    engine_assets *Assets = Group->Assets;
    if(Assets && ShaderIndex && (ShaderIndex < Assets->AssetCount))
    {
        asset *Asset = Assets->Assets + ShaderIndex;
        if(Asset->KEA.Type == KEAType_TXT)
        {
            text_id TextID = {ShaderIndex};
            loaded_text *Text = GetText(Assets, TextID, Group->GenerationID);
            if(!Text)
            {
                LoadText(Assets, TextID, true);
                Text = GetText(Assets, TextID, Group->GenerationID);
            }

            if(Text)
            {
                Shader->Source = Text->String;
                Shader->SourceLength = Asset->KEA.Text.Length;
            }
        }
    }
}

// NOTE(pvlso): Built every frame, the function addresses change when the engine is reloaded
internal engine_api
GetEngineAPI(void)
{
    engine_api Result = {};

    Result.BeginRenderGroup = GameBeginRenderGroup;
    Result.EndRenderGroup = GameEndRenderGroup;
    Result.Perspective = Perspective;
    Result.Orthographic = Orthographic;
    Result.Clear = Clear;
    Result.BeginPixelWorld = BeginPixelWorld;
    Result.EndPixelWorld = EndPixelWorld;
    Result.SetPixelWorldScroll = SetPixelWorldScroll;
    Result.SetPixelWorldShader = GameSetPixelWorldShader;
    Result.SetPixelWorldLighting = SetPixelWorldLighting;
    Result.PushLight = PushLight;

    Result.PushBitmap = GamePushBitmap;
    Result.PushRect = PushRect;
    Result.PushLine = PushLine;
    Result.PushCircleOutline = PushCircleOutline;
    Result.PushTriangle = GamePushTriangle;

    Result.GetFont = GameGetFont;
    Result.PushText = GamePushText;
    Result.GetTextDim = GameGetTextDim;

    Result.GetBestMatchAssets = GameGetBestMatchAssets;
    Result.GetSprite = GameGetSprite;
    Result.GetTilesetTile = GameGetTilesetTile;
    Result.GetTilesetInfo = GameGetTilesetInfo;
    Result.GetTileInfo = GameGetTileInfo;
    Result.GetMapInfo = GameGetMapInfo;
    Result.FindMap = GameFindMap;
    Result.GetMapLayer = GameGetMapLayer;
    Result.GetMapTileset = GameGetMapTileset;
    Result.GetMapEntity = GameGetMapEntity;
    Result.FindNavPoly = GameFindNavPoly;
    Result.GetNavPolyLevel = GameGetNavPolyLevel;
    Result.GetNavPolyPoints = GameGetNavPolyPoints;
    Result.FindNavPath = GameFindNavPath;
    Result.MoveOnNav = GameMoveOnNav;
    Result.ClosestNavPoint = GameClosestNavPoint;
    Result.ClosestNavPointOnLevel = GameClosestNavPointOnLevel;
    Result.FindEntity = GameFindEntity;
    Result.FindEntityByGUID = GameFindEntityByGUID;
    Result.GetEntityInfo = GameGetEntityInfo;
    Result.GetEntityClip = GameGetEntityClip;
Result.GetNormalMap = GameGetNormalMap;

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): GAME MODE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// NOTE(pvlso): "game_data_<version>.kea" -> version, false for any other name
internal b32
ParseGameDataKEAVersion(char *Name, u32 *Version)
{
    b32 Result = false;

    char *Prefix = "game_data_";
    umm PrefixLength = StringLength(Prefix);
    if(StringsAreEqual(PrefixLength, Name, Prefix) && IsDigit(Name[PrefixLength]))
    {
        u64 Value = 0;
        char *At = Name + PrefixLength;
        while(IsDigit(*At) && (Value <= U32Maximum))
        {
            Value = 10*Value + (*At++ - '0');
        }

        if((Value <= U32Maximum) && StringsAreEqual(At, ".kea"))
        {
            *Version = (u32)Value;
            Result = true;
        }
    }

    return(Result);
}

// NOTE(pvlso): The newest game_data_<version>.kea in the project's data/keas (the data root is the
// project's while it runs), the assets mode writes those. Its name goes to FileName, empty when there's none. The
// assets go into Arena, the game's own (editor_mode_game AssetArena), so a swap can free them.
internal engine_assets *
LoadGameAssets(memory_arena *Arena, transient_state *TranState, char *FileName, umm FileNameCount)
{
    TIMED_FUNCTION();

    engine_assets *Result = 0;
    FileName[0] = 0;

    temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
    u32 FileCount = Platform.ListFilesInDirectory(PlatformFileType_KEA, 0, 0);
    if(FileCount)
    {
        char **FileNames = PushArray(TempMem.Arena, FileCount, char *);
        FileCount = Platform.ListFilesInDirectory(PlatformFileType_KEA, FileNames, TempMem.Arena);

        char *BestFileName = 0;
        u32 BestVersion = 0;
        for(u32 FileIndex = 0; FileIndex < FileCount; ++FileIndex)
        {
            u32 Version = 0;
            if(ParseGameDataKEAVersion(FileNames[FileIndex], &Version) &&
               (!BestFileName || (Version > BestVersion)))
            {
                BestFileName = FileNames[FileIndex];
                BestVersion = Version;
            }
        }

        if(BestFileName)
        {
            Result = AllocateAssets(BestFileName, Arena, TempMem.Arena,
                                    TranState->LowPriorityQueue, TranState->TextureOpQueue, Megabytes(128));
            FormatString(FileNameCount, FileName, "%s", BestFileName);
        }
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}

#define GAME_SOURCE_CHECK_INTERVAL 0.5f
#define GAME_DATA_CHECK_INTERVAL 0.5f

// NOTE(pvlso): The data folders are checked like the sources: when what a KEA is built from changes (a map saved in
// the map editor, a picture dropped in bmps/), a new KEA is built and the running game is swapped to it (see
// SwapRequested).
internal void
UpdateGameData(editor_state *EditorState, editor_mode_game *Game, transient_state *TranState, f32 dt)
{
    Game->DataCheckTimer -= dt;
    if(Game->DataCheckTimer <= 0.0f)
    {
        Game->DataCheckTimer = GAME_DATA_CHECK_INTERVAL;
        u64 Signature = AssetDataSignature(&TranState->TranArena);
        if(Signature != Game->BuiltDataSignature)
        {
            // NOTE(pvlso): Set before the build, a failed one isn't retried until the data changes again
            Game->BuiltDataSignature = Signature;
            char Built[64];
            if(BuildProjectKEA(EditorState, &TranState->TranArena, Game->AssetFileName, Built, sizeof(Built)))
            {
                Game->SwapRequested = true;
                FormatString(sizeof(Game->AssetStatus), Game->AssetStatus, "The data changed: %s built", Built);
            }
            else
            {
                FormatString(sizeof(Game->AssetStatus), Game->AssetStatus,
                             "The data changed, but building %s failed (see the log)", Built);
            }
        }
    }
}

internal void
StartGameBuild(editor_mode_game *Game)
{
    Game->BuiltSourceWriteTime = Platform.GetProjectWriteTime(Game->ProjectName);
    Game->BuildProcess = Platform.BuildProject(Game->ProjectName);
    Game->Building = true;
    ++Game->BuildCount;

    FormatString(sizeof(Game->Status), Game->Status, "Building %s...", Game->ProjectName);
    LOG("Building %s", Game->ProjectName);
}

// NOTE(pvlso): The new library is loaded before the old one goes, so a library that doesn't
// load leaves the old code running
internal b32
LoadGameCode(editor_mode_game *Game)
{
    b32 Result = false;

    char ModuleName[256];
    FormatString(sizeof(ModuleName), ModuleName, "%s/build/%s", Game->ProjectName, Game->ProjectName);

    platform_loaded_code Code = Platform.LoadCode(ModuleName, PlatformFileType_Project);
    game_update_and_render *UpdateAndRender = 0;
    if(Code.NoErrors)
    {
        UpdateAndRender = (game_update_and_render *)Platform.GetProcAddress(&Code, "GameUpdateAndRender");
    }

    if(UpdateAndRender)
    {
        if(Game->UpdateAndRender)
        {
            Platform.UnloadCode(&Game->Code);
        }

        Game->Code = Code;
        Game->UpdateAndRender = UpdateAndRender;
        ++Game->LoadCount;
        Result = true;
    }
    else
    {
        Platform.UnloadCode(&Code);
    }

    return(Result);
}

internal void
ReadGameBuildLog(editor_mode_game *Game, transient_state *TranState)
{
    Game->BuildLog[0] = 0;

    char FileName[256];
    FormatString(sizeof(FileName), FileName, "%s/build/build.log", Game->ProjectName);

    temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
    read_file_result File = Platform.ReadEntireFile(FileName, PlatformFileType_Project, TempMem.Arena, true);
    if(File.Contents)
    {
        FormatString(sizeof(Game->BuildLog), Game->BuildLog, "%s", (char *)File.Contents);
    }
    EndTemporaryMemory(TempMem);
}

internal void
UpdateGameBuild(editor_mode_game *Game, transient_state *TranState, f32 dt)
{
    if(Game->Building)
    {
        platform_process_state State = Platform.GetProcessState(&Game->BuildProcess);
        if(!State.IsRunning)
        {
            Game->Building = false;
            ReadGameBuildLog(Game, TranState);

            Game->BuildFailed = true;
            if(!State.StartedSuccessfully)
            {
                FormatString(sizeof(Game->Status), Game->Status, "Could not start the build of %s", Game->ProjectName);
                LOG_ERROR("Could not start the build of %s", Game->ProjectName);
            }
            else if(State.ReturnCode != 0)
            {
                FormatString(sizeof(Game->Status), Game->Status, "%s: build failed%s", Game->ProjectName,
                             Game->UpdateAndRender ? ", the last good build keeps running" : "");
                LOG_ERROR("%s: build failed (exit code %d)", Game->ProjectName, State.ReturnCode);
            }
            else if(!LoadGameCode(Game))
            {
                FormatString(sizeof(Game->Status), Game->Status,
                             "%s built, but it doesn't load or has no GameUpdateAndRender export", Game->ProjectName);
                LOG_ERROR("%s built, but it doesn't load or has no GameUpdateAndRender export", Game->ProjectName);
            }
            else
            {
                Game->BuildFailed = false;
                FormatString(sizeof(Game->Status), Game->Status, "Running %s", Game->ProjectName);
                LOG("%s %s", Game->ProjectName, (Game->LoadCount > 1) ? "reloaded" : "loaded");
            }
        }
    }

    // NOTE(pvlso): A save during a build is caught here once that build is done
    Game->SourceCheckTimer -= dt;
    if(!Game->Building && (Game->SourceCheckTimer <= 0.0f))
    {
        Game->SourceCheckTimer = GAME_SOURCE_CHECK_INTERVAL;
        if(Platform.GetProjectWriteTime(Game->ProjectName) != Game->BuiltSourceWriteTime)
        {
            StartGameBuild(Game);
        }
    }
}

// NOTE(pvlso): The game mode, pushed on Arena: the mode arena for Run, PlayArena when the map editor's Play started it
internal editor_mode_game *
StartGame(editor_state *EditorState, transient_state *TranState, char *Name, memory_arena *Arena)
{
    // NOTE(pvlso): The game's assets come from its own data/, built again first when it changed since the last KEA
    UseDataOf(EditorState, Name);
    BuildProjectKEAIfStale(EditorState, &TranState->TranArena);

    editor_mode_game *Game = PushStruct(Arena, editor_mode_game);
    FormatString(sizeof(Game->ProjectName), Game->ProjectName, "%s", Name);
    Game->BuiltDataSignature = AssetDataSignature(&TranState->TranArena);
    Game->DataCheckTimer = GAME_DATA_CHECK_INTERVAL;
    Game->Assets = LoadGameAssets(&Game->AssetArena, TranState, Game->AssetFileName, sizeof(Game->AssetFileName));
    EditorState->Game = Game;

    LOG("Run %s", Game->ProjectName);
    if(Game->Assets)
    {
        LOG("%s assets: %s", Game->ProjectName, Game->AssetFileName);

        // NOTE(pvlso): One a game held open while a newer one was built goes now
        u32 Version;
        if(ParseGameDataKEAVersion(Game->AssetFileName, &Version))
        {
            DeleteOlderKEAs(Version, Game->AssetFileName, &TranState->TranArena);
        }
    }
    else
    {
        LOG_WARNING("%s has no game_data_<version>.kea in its data/keas", Game->ProjectName);
    }

    // NOTE(pvlso): Always builds first, so the library matches the sources and the editor's mode
    StartGameBuild(Game);

    return(Game);
}

internal void
PlayGame(editor_state *EditorState, transient_state *TranState, char *ProjectName)
{
    // NOTE(pvlso): ProjectName lives in the mode arena, which SetEditorMode frees
    char Name[PROJECT_NAME_COUNT];
    FormatString(sizeof(Name), Name, "%s", ProjectName);

    SetEditorMode(EditorState, TranState, EditorMode_Game);
    EditorState->PausedMap = 0;
    StartGame(EditorState, TranState, Name, &EditorState->ModeArena);
}

// NOTE(pvlso): The map editor's Play (map editor plan, phase 7): the map editor, its map saved, waits in the mode arena
// (EditorState->PausedMap) and the game starts on that map, at P on Level when HasP (cells from the map's top left,
// y down), else where the game itself starts. Stop or F5 goes back (ReturnToMapEditor).
internal void
PlayGameFromMap(editor_state *EditorState, transient_state *TranState, editor_mode_map *MapMode, b32 HasP, v2 P,
                s32 Level)
{
    Platform.CompleteAllWork(TranState->LowPriorityQueue);

    EditorState->PausedMap = MapMode;
    EditorState->EditorMode = EditorMode_Game;
    LOG("Editor mode: Game, from the map editor on %s%s", MapMode->FileName, HasP ? " at the mouse" : "");

    editor_mode_game *Game = StartGame(EditorState, TranState, MapMode->ProjectName, &EditorState->PlayArena);
    Game->FromMap = true;
    game_start *Start = &Game->Memory.Start;
    FormatString(sizeof(Start->MapFileName), Start->MapFileName, "%s", MapMode->FileName);
    Start->HasP = HasP;
    Start->P = P;
    Start->Level = Level;
    if(HasP)
    {
        LOG("Start on %s at %.1f,%.1f, level %d", Start->MapFileName, P.x, P.y, Level);
    }
}

// NOTE(pvlso): Back from a game the map editor's Play started: the game mode goes with PlayArena, the map editor goes on
// as it was. Drags it had going ended while the game ran.
internal void
ReturnToMapEditor(editor_state *EditorState, transient_state *TranState)
{
    Platform.CompleteAllWork(TranState->LowPriorityQueue);

    editor_mode_map *MapMode = EditorState->PausedMap;
    EditorState->PausedMap = 0;
    Clear(&EditorState->PlayArena);

    MapMode->Panning = 0;
    MapMode->Painting = false;
    MapMode->Erasing = false;
    MapMode->RectActive = false;
    MapMode->PickingBrush = false;
    MapMode->DraggingEntity = false;
    MapMode->Scattering = false;
    MapMode->DragVertex = -1;
    EditorState->EditorMode = EditorMode_MapEditor;
    EditorState->MapMode = MapMode;
    LOG("Editor mode: Map Editor, back on %s", MapMode->FileName);
}

internal void
ClearGameMemory(editor_mode_game *Game)
{
    Clear(&Game->Memory.PermanentArena);
    Clear(&Game->Memory.TransientArena);
    Game->Memory.GameState = 0;
}

// NOTE(pvlso): Queues a delete for the texture of every loaded bitmap and closes the KEA. The asset memory itself
// is in Game->AssetArena, which the caller clears.
internal void
FreeGameAssets(engine_assets *Assets)
{
    TIMED_FUNCTION();

    u32 TextureCount = 0;
    if(Assets->Assets)
    {
        for(u32 AssetIndex = 0; AssetIndex < Assets->AssetCount; ++AssetIndex)
        {
            asset *Asset = Assets->Assets + AssetIndex;
            if((Asset->State == AssetState_Loaded) && Asset->Header &&
               (Asset->Header->AssetType == AssetType_Bitmap) && Asset->Header->Bitmap.TextureHandle)
            {
                texture_op Op = {};
                Op.IsAllocate = false;
                Op.Deallocate.Handle = Asset->Header->Bitmap.TextureHandle;
                AddOp(Assets->TextureOpQueue, &Op);
                ++TextureCount;
            }
        }
    }

    Platform.CloseFile(&Assets->File.Handle);
    LOG("Freed %u texture(s) of the game's assets", TextureCount);
}

// NOTE(pvlso): Stop takes two frames. The first one lets the in-flight asset loads finish
// and doesn't run the game. Their texture uploads are queued, and the platform runs that
// queue at the end of the frame, writing the handles into the asset memory. The second
// frame (FinishStopGame) can then delete every texture and free the mode arena.
internal void
StopGame(editor_state *EditorState, transient_state *TranState)
{
    editor_mode_game *Game = EditorState->Game;

    // NOTE(pvlso): A build left running would race the next Run's build for the same files.
    // Stopping mid build is rare, so just wait for it.
    b32 Waited = false;
    while(Game->Building && Platform.GetProcessState(&Game->BuildProcess).IsRunning)
    {
        if(!Waited)
        {
            LOG_WARNING("Stop: waiting for the running build of %s", Game->ProjectName);
            Waited = true;
        }
        Platform.Sleep(10);
    }

    // NOTE(pvlso): Asset loads still in flight write into the mode arena
    Platform.CompleteAllWork(TranState->LowPriorityQueue);

    ClearGameMemory(Game);
    if(Game->UpdateAndRender)
    {
        Platform.UnloadCode(&Game->Code);
        Game->UpdateAndRender = 0;
    }

    Game->Stopping = true;
    LOG("Stop %s", Game->ProjectName);
    FormatString(sizeof(Game->Status), Game->Status, "Stopping %s...", Game->ProjectName);
}

internal void
FinishStopGame(editor_state *EditorState, transient_state *TranState)
{
    editor_mode_game *Game = EditorState->Game;
    if(Game->Assets)
    {
        FreeGameAssets(Game->Assets);
    }
    Clear(&Game->AssetArena);
    FreeGameFonts(&Game->Fonts, TranState->TextureOpQueue);

    if(Game->FromMap && EditorState->PausedMap)
    {
        ReturnToMapEditor(EditorState, TranState);
    }
    else
    {
        PlayTitleScreen(EditorState, TranState);
    }
}

// NOTE(pvlso): The second frame of a swap (see SwapRequested): the loads the first frame finished have their
// textures by now, so the old assets can go. The game is told by Memory.AssetSwapCount and reloads what it took
// from them on this frame's update.
internal void
FinishAssetSwap(editor_mode_game *Game, transient_state *TranState)
{
    TIMED_FUNCTION();

    Game->Swapping = false;

    char OldFileName[256];
    FormatString(sizeof(OldFileName), OldFileName, "%s", Game->AssetFileName);
    if(Game->Assets)
    {
        FreeGameAssets(Game->Assets);
        Game->Assets = 0;
    }
    Clear(&Game->AssetArena);

    Game->Assets = LoadGameAssets(&Game->AssetArena, TranState, Game->AssetFileName, sizeof(Game->AssetFileName));
    ++Game->Memory.AssetSwapCount;

    if(Game->Assets)
    {
        LOG("%s assets swapped: %s -> %s", Game->ProjectName, OldFileName[0] ? OldFileName : "none",
            Game->AssetFileName);
        FormatString(sizeof(Game->AssetStatus), Game->AssetStatus, "The data changed: the game swapped to %s",
                     Game->AssetFileName);

        // NOTE(pvlso): Closed by FreeGameAssets, so the one the game held goes now too
        u32 Version;
        if(ParseGameDataKEAVersion(Game->AssetFileName, &Version))
        {
            DeleteOlderKEAs(Version, Game->AssetFileName, &TranState->TranArena);
        }
    }
    else
    {
        LOG_WARNING("%s: the swap found no game_data_<version>.kea in its data/keas", Game->ProjectName);
        FormatString(sizeof(Game->AssetStatus), Game->AssetStatus, "The data changed, but no KEA was found to swap to");
    }
}

internal void
DrawGameBuildLog(editor_mode_game *Game, nk_context *Nk, r32 Height)
{
    nk_layout_row_dynamic(Nk, Height, 1);
    if(nk_group_begin(Nk, "Build Log", NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(Nk, 18, 1);

        char *LineStart = Game->BuildLog;
        for(char *At = Game->BuildLog;; ++At)
        {
            if((*At == '\n') || (*At == 0))
            {
                int Length = (int)(At - LineStart);
                if(Length && (LineStart[Length - 1] == '\r'))
                {
                    --Length;
                }
                nk_text(Nk, LineStart, Length, NK_TEXT_ALIGN_LEFT);

                if(*At == 0)
                {
                    break;
                }
                LineStart = At + 1;
            }
        }

        nk_group_end(Nk);
    }
}

// NOTE(pvlso): The dev controls of a running project: Stop, Restart, the build status and counts,
// the log of a failed build. Internal builds draw them in the debug window (engine_debug.cpp),
// others in the main window. The buttons only set requests: the debug window is drawn after the
// frame's render commands are built, and those still point into the mode arena that Stop frees.
// Fits a narrow panel: buttons, then status and counts on their own rows, then the log.
internal void
DrawGameControls(editor_mode_game *Game, nk_context *Nk, r32 LogHeight)
{
    nk_layout_row_static(Nk, 30, 110, 2);
    if(nk_button_label(Nk, "Stop"))
    {
        Game->StopRequested = true;
    }
    if(nk_button_label(Nk, "Restart"))
    {
        Game->RestartRequested = true;
    }

    nk_layout_row_dynamic(Nk, 30, 1);
    nk_label(Nk, Game->Status, NK_TEXT_ALIGN_LEFT);
    if(Game->FromMap)
    {
        nk_labelf(Nk, NK_TEXT_ALIGN_LEFT, "Started from the map editor on %s: Stop or F5 goes back",
                  Game->Memory.Start.MapFileName);
    }

    char Assets[320];
    if(Game->AssetFileName[0])
    {
        FormatString(sizeof(Assets), Assets, "Assets: %s/data/keas/%s", Game->ProjectName, Game->AssetFileName);
    }
    else
    {
        FormatString(sizeof(Assets), Assets, "Assets: none (no game_data_<version>.kea in %s/data/keas)",
                     Game->ProjectName);
    }
    nk_label(Nk, Assets, NK_TEXT_ALIGN_LEFT);
    if(Game->AssetStatus[0])
    {
        nk_label(Nk, Game->AssetStatus, NK_TEXT_ALIGN_LEFT);
    }

    char Counts[128];
    FormatString(sizeof(Counts), Counts, "Builds: %u   Reloads: %u", Game->BuildCount,
                 Game->LoadCount ? (Game->LoadCount - 1) : 0);
    nk_label(Nk, Counts, NK_TEXT_ALIGN_LEFT);

    if(Game->BuildFailed && Game->BuildLog[0])
    {
        DrawGameBuildLog(Game, Nk, LogHeight);
    }
}

internal b32
UpdateAndRenderGame(editor_state *EditorState, transient_state *TranState, engine_input *Input,
                    editor_render_commands *RenderCommands)
{
    TIMED_FUNCTION();

    b32 Result = false;

    editor_mode_game *Game = EditorState->Game;
    nk_context *Nk = EditorState->UIState.Nk;

    if(Game->FromMap && !Game->Stopping && WasPressed(GetController(Input, 0)->Play))
    {
        Game->StopRequested = true;
    }

    if(Game->Stopping)
    {
        FinishStopGame(EditorState, TranState);
        Result = true;
    }
    else if(Game->StopRequested)
    {
        StopGame(EditorState, TranState);
    }
    else
    {
        UpdateGameBuild(Game, TranState, Input->dtForFrame);
        if(Game->Swapping)
        {
            FinishAssetSwap(Game, TranState);
        }
        UpdateGameData(EditorState, Game, TranState, Input->dtForFrame);

#if !EDITOR_INTERNAL
        DrawGameControls(Game, Nk, 400.0f);
#endif

        // NOTE(pvlso): The game starts over with empty memory, the code stays
        if(Game->RestartRequested)
        {
            ClearGameMemory(Game);
            Game->RestartRequested = false;
            LOG("Restart %s", Game->ProjectName);
        }

        if(Game->SwapRequested)
        {
            // NOTE(pvlso): The first frame of a swap: the game doesn't run, so nothing new starts loading
            Platform.CompleteAllWork(TranState->LowPriorityQueue);
            Game->SwapRequested = false;
            Game->Swapping = true;
        }
        else if(Game->UpdateAndRender)
        {
            Game->Memory.PlatformAPI = Platform;
            Game->Memory.EngineAPI = GetEngineAPI();
#if EDITOR_INTERNAL
            Game->Memory.DebugTable = GlobalDebugTable;
#endif

            // NOTE(pvlso): The engine_api font calls work on the running game's fonts
            Game->Fonts.TextureOpQueue = TranState->TextureOpQueue;
            GlobalGameFonts = &Game->Fonts;

            // NOTE(pvlso): The game's own TIMED_* blocks nest under this one
            BEGIN_BLOCK("GameUpdateAndRender");
            Game->UpdateAndRender(&Game->Memory, Input, RenderCommands, Game->Assets, Nk);
            END_BLOCK();

            GlobalGameFonts = 0;
        }
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

internal void
ListMapFiles(editor_mode_map *MapMode, memory_arena *Arena)
{
    MapMode->MapFileCount = Platform.ListFilesInDirectory(PlatformFileType_Map, 0, 0);
    MapMode->MapFiles = PushArray(Arena, Maximum(1, MapMode->MapFileCount), char *);
    Platform.ListFilesInDirectory(PlatformFileType_Map, MapMode->MapFiles, Arena);
    MapMode->MapFileIndex = 0;
}

// NOTE(pvlso): The map editor: see editor_map_mode.h. The canvas holds the map (middle drag pans, the wheel zooms, a
// left drag paints with the brush, a right drag erases, on the current layer) and the palette (the current tile set's
// grid: drag over tiles to pick the brush). The panel on the left makes maps, edits layers and adds tile sets.

internal void
PlayMapEditor(editor_state *EditorState, transient_state *TranState, char *ProjectName)
{
    // NOTE(pvlso): ProjectName lives in the mode arena, which SetEditorMode frees
    char Name[PROJECT_NAME_COUNT];
    FormatString(sizeof(Name), Name, "%s", ProjectName);

    SetEditorMode(EditorState, TranState, EditorMode_MapEditor);
    LOG("Map Editor for %s", Name);
    UseDataOf(EditorState, Name);

    editor_mode_map *MapMode = PushStruct(&EditorState->ModeArena, editor_mode_map);
    FormatString(sizeof(MapMode->ProjectName), MapMode->ProjectName, "%s", Name);
    MapMode->CurrentSet = -1;
    MapMode->HoverX = MapMode->HoverY = -1;
    MapMode->Zoom = 32.0f;
    MapMode->PaletteZoom = 32.0f;
    MapMode->ShowGrid = true;
    MapMode->NewWidth = 64;
    MapMode->NewHeight = 48;
    MapMode->CurrentDef = -1;
    MapMode->SelectedEntity = -1;
    MapMode->LastSelectedIndex = -1;
    MapMode->ShowEntities = true;

    ListMapFiles(MapMode, &EditorState->ModeArena);
    MapMode->TilesetFileCount = Platform.ListFilesInDirectory(PlatformFileType_TSBMP, 0, 0);
    MapMode->TilesetFiles = PushArray(&EditorState->ModeArena, Maximum(1, MapMode->TilesetFileCount), char *);
    Platform.ListFilesInDirectory(PlatformFileType_TSBMP, MapMode->TilesetFiles, &EditorState->ModeArena);

    EditorState->MapMode = MapMode;
}

internal void
ReleaseMapTileset(platform_texture_op_queue *TextureOpQueue, map_tileset *Set)
{
    for(u32 TileIndex = 0; Set->Tiles && (TileIndex < Set->File.TileCount); ++TileIndex)
    {
        ReleaseTileTexture(TextureOpQueue, Set->Tiles + TileIndex);
    }
    Clear(&Set->Arena);
    *Set = {};
}

internal void
LoadMapTileset(platform_texture_op_queue *TextureOpQueue, map_tileset *Set, char *FileName)
{
    memory_arena *Arena = &Set->Arena;
    FormatString(sizeof(Set->FileName), Set->FileName, "%s", FileName);
    Set->FlagNames = ReadTileFlagNames(Arena);
    Set->File = ReadTilesetFile(FileName, &Set->FlagNames, Arena);
    tileset_file *File = &Set->File;
    for(u32 SheetIndex = 0; SheetIndex < File->SheetCount; ++SheetIndex)
    {
        Set->Sheets[SheetIndex] = LoadImageFile(File->Sheets[SheetIndex], PlatformFileType_DataRoot, Arena);
    }

    Set->Tiles = PushArray(Arena, Maximum(1, File->TileCount), loaded_bitmap);
    u32 GridCells = Maximum(1, File->GridWidth*File->GridHeight);
    Set->TileAt = PushArray(Arena, GridCells, s32);
    for(u32 Index = 0; Index < GridCells; ++Index) {Set->TileAt[Index] = -1;}
    for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
    {
        tileset_file_tile *Tile = File->Tiles + TileIndex;
        tileset_file_cell Cell = Tile->Frames[0];
        loaded_bitmap *Bitmap = Set->Tiles + TileIndex;
        *Bitmap = CutTilesetCell(Set->Sheets + Cell.Sheet, File->TileSize, Cell.X, Cell.Y, Arena);
        if(Bitmap->Memory)
        {
            AllocateBitmap(TextureOpQueue, Bitmap);
        }
        Set->TileAt[Tile->GridY*File->GridWidth + Tile->GridX] = (s32)TileIndex;
    }
    LOG("Map editor: tile set %s, %u tiles", FileName, File->TileCount);
}

inline s32
MapTileAt(map_tileset *Set, s32 X, s32 Y)
{
    s32 Result = -1;
    if((X >= 0) && (Y >= 0) && ((u32)X < Set->File.GridWidth) && ((u32)Y < Set->File.GridHeight))
    {
        Result = Set->TileAt[Y*Set->File.GridWidth + X];
    }

    return(Result);
}

internal map_cell *
AllocateLayerCells(u32 Width, u32 Height)
{
    umm Size = (umm)Width*Height*sizeof(map_cell);
    map_cell *Result = (map_cell *)Platform.AllocateMemory(Size);
    ZeroSize(Size, Result);

    return(Result);
}

internal void
FreeMapLayers(editor_mode_map *MapMode)
{
    for(u32 LayerIndex = 0; LayerIndex < MapMode->LayerCount; ++LayerIndex)
    {
        Platform.DeallocateMemory(MapMode->Layers[LayerIndex].Cells);
    }
    MapMode->LayerCount = 0;
}

// NOTE(pvlso): A whole map state: its size, layers, every layer's cells one after another, then its entities (copied as
// bytes: after the cells they may not be aligned)
struct map_snapshot
{
    umm Size;
    u32 Width;
    u32 Height;
    u32 LayerCount;
    s32 CurrentLayer;
    u32 EntityCount;
    map_layer Layers[MAP_MAX_LAYERS];
    map_cell Cells[1];
};

// NOTE(pvlso): Room for Count entities, the ones there kept
internal void
ReserveMapEntities(editor_mode_map *MapMode, u32 Count)
{
    if(Count > MapMode->EntityCapacity)
    {
        u32 Capacity = Maximum(Count, Maximum(64u, 2*MapMode->EntityCapacity));
        map_placement *Entities = (map_placement *)Platform.AllocateMemory(Capacity*sizeof(map_placement));
        if(MapMode->Entities)
        {
            Copy(MapMode->EntityCount*sizeof(map_placement), MapMode->Entities, Entities);
            Platform.DeallocateMemory(MapMode->Entities);
        }
        MapMode->Entities = Entities;
        MapMode->EntityCapacity = Capacity;
    }
}

internal void
DropOldestMapState(editor_mode_map *MapMode)
{
    MapMode->HistoryBytes -= MapMode->History[0]->Size;
    Platform.DeallocateMemory(MapMode->History[0]);
    for(u32 Index = 1; Index < MapMode->HistoryCount; ++Index)
    {
        MapMode->History[Index - 1] = MapMode->History[Index];
    }
    --MapMode->HistoryCount;
    if(MapMode->HistoryAt) {--MapMode->HistoryAt;}
}

internal void
FreeMapHistory(editor_mode_map *MapMode)
{
    while(MapMode->HistoryCount) {DropOldestMapState(MapMode);}
    MapMode->HistoryAt = 0;
}

internal void
RecordMapState(editor_mode_map *MapMode)
{
    // NOTE(pvlso): A change after undoing drops what could have been redone
    while(MapMode->HistoryCount > MapMode->HistoryAt + 1)
    {
        map_snapshot *Dropped = MapMode->History[--MapMode->HistoryCount];
        MapMode->HistoryBytes -= Dropped->Size;
        Platform.DeallocateMemory(Dropped);
    }

    umm LayerCells = (umm)MapMode->Width*MapMode->Height;
    umm CellsSize = Maximum(1, MapMode->LayerCount)*LayerCells*sizeof(map_cell);
    umm Size = sizeof(map_snapshot) + CellsSize + MapMode->EntityCount*sizeof(map_placement);
    while(MapMode->HistoryCount &&
          ((MapMode->HistoryCount == ArrayCount(MapMode->History)) || (MapMode->HistoryBytes + Size > MAP_HISTORY_BYTES)))
    {
        DropOldestMapState(MapMode);
    }

    map_snapshot *Snapshot = (map_snapshot *)Platform.AllocateMemory(Size);
    Snapshot->Size = Size;
    Snapshot->Width = MapMode->Width;
    Snapshot->Height = MapMode->Height;
    Snapshot->LayerCount = MapMode->LayerCount;
    Snapshot->CurrentLayer = MapMode->CurrentLayer;
    Copy(sizeof(Snapshot->Layers), MapMode->Layers, Snapshot->Layers);
    for(u32 LayerIndex = 0; LayerIndex < MapMode->LayerCount; ++LayerIndex)
    {
        Copy(LayerCells*sizeof(map_cell), MapMode->Layers[LayerIndex].Cells, Snapshot->Cells + LayerIndex*LayerCells);
        Snapshot->Layers[LayerIndex].Cells = 0;
    }
    Snapshot->EntityCount = MapMode->EntityCount;
    Copy(MapMode->EntityCount*sizeof(map_placement), MapMode->Entities, (u8 *)Snapshot->Cells + CellsSize);
    MapMode->History[MapMode->HistoryCount] = Snapshot;
    MapMode->HistoryAt = MapMode->HistoryCount++;
    MapMode->HistoryBytes += Size;
}

internal void
RestoreMapState(editor_mode_map *MapMode, u32 HistoryIndex)
{
    map_snapshot *Snapshot = MapMode->History[HistoryIndex];
    FreeMapLayers(MapMode);
    MapMode->Width = Snapshot->Width;
    MapMode->Height = Snapshot->Height;
    MapMode->LayerCount = Snapshot->LayerCount;
    MapMode->CurrentLayer = Snapshot->CurrentLayer;
    Copy(sizeof(Snapshot->Layers), Snapshot->Layers, MapMode->Layers);
    umm LayerCells = (umm)MapMode->Width*MapMode->Height;
    for(u32 LayerIndex = 0; LayerIndex < MapMode->LayerCount; ++LayerIndex)
    {
        map_layer *Layer = MapMode->Layers + LayerIndex;
        Layer->Cells = AllocateLayerCells(MapMode->Width, MapMode->Height);
        Copy(LayerCells*sizeof(map_cell), Snapshot->Cells + LayerIndex*LayerCells, Layer->Cells);
    }
    umm CellsSize = Maximum(1, MapMode->LayerCount)*LayerCells*sizeof(map_cell);
    ReserveMapEntities(MapMode, Snapshot->EntityCount);
    MapMode->EntityCount = Snapshot->EntityCount;
    Copy(MapMode->EntityCount*sizeof(map_placement), (u8 *)Snapshot->Cells + CellsSize, MapMode->Entities);
    if(MapMode->SelectedEntity >= (s32)MapMode->EntityCount) {MapMode->SelectedEntity = -1;}
    MapMode->HistoryAt = HistoryIndex;
}

// NOTE(pvlso): The map grown or cropped on each side (negative crops), the cells staying where they were on the
// ground
internal void
ResizeMap(editor_mode_map *MapMode, s32 Left, s32 Right, s32 Top, s32 Bottom)
{
    s32 Width = ClampCell(1, (s32)MapMode->Width + Left + Right, MAP_MAX_SIDE);
    s32 Height = ClampCell(1, (s32)MapMode->Height + Top + Bottom, MAP_MAX_SIDE);
    for(u32 LayerIndex = 0; LayerIndex < MapMode->LayerCount; ++LayerIndex)
    {
        map_layer *Layer = MapMode->Layers + LayerIndex;
        map_cell *Cells = AllocateLayerCells(Width, Height);
        for(s32 Y = 0; Y < (s32)MapMode->Height; ++Y)
        {
            for(s32 X = 0; X < (s32)MapMode->Width; ++X)
            {
                s32 NewX = X + Left;
                s32 NewY = Y + Top;
                if((NewX >= 0) && (NewY >= 0) && (NewX < Width) && (NewY < Height))
                {
                    Cells[NewY*Width + NewX] = Layer->Cells[Y*MapMode->Width + X];
                }
            }
        }
        Platform.DeallocateMemory(Layer->Cells);
        Layer->Cells = Cells;
    }
    for(u32 EntityIndex = 0; EntityIndex < MapMode->EntityCount; ++EntityIndex)
    {
        MapMode->Entities[EntityIndex].P += V2((r32)Left, (r32)Top);
    }
    MapMode->Width = Width;
    MapMode->Height = Height;
    LOG("Map editor: resized to %d x %d", Width, Height);
}

// NOTE(pvlso): A map's file name from what was typed: letters, digits, '_' and '-', then .txt. False when nothing's left.
internal b32
MapFileNameFrom(char *Typed, char *Dest, u32 DestCount)
{
    char Clean[MAP_FILE_SET_NAME_LENGTH];
    u32 Length = 0;
    for(char *C = Typed; *C && (Length < MAP_FILE_SET_NAME_LENGTH - 5); ++C)
    {
        if(((*C >= 'a') && (*C <= 'z')) || ((*C >= 'A') && (*C <= 'Z')) || IsDigit(*C) || (*C == '_') || (*C == '-'))
        {
            Clean[Length++] = *C;
        }
    }
    Clean[Length] = 0;
    FormatString(DestCount, Dest, "%s.txt", Clean);
    if(!Length)
    {
        LOG_ERROR("A map needs a name");
    }

    return(Length != 0);
}

// NOTE(pvlso): The map's set for a tile set file, loading it when the map hasn't got it yet; -1 when it can't
internal s32
MapSetFor(platform_texture_op_queue *TextureOpQueue, editor_mode_map *MapMode, char *FileName)
{
    s32 Result = -1;
    for(u32 SetIndex = 0; SetIndex < MapMode->SetCount; ++SetIndex)
    {
        if(StringsAreEqual(MapMode->Sets[SetIndex].FileName, FileName)) {Result = (s32)SetIndex;}
    }
    if((Result < 0) && (MapMode->SetCount < MAP_MAX_TILESETS))
    {
        Result = (s32)MapMode->SetCount++;
        LoadMapTileset(TextureOpQueue, MapMode->Sets + Result, FileName);
    }
    else if(Result < 0)
    {
        LOG_ERROR("A map uses at most %d tile sets", MAP_MAX_TILESETS);
    }

    return(Result);
}

// NOTE(pvlso): The project's entity blocks and definitions, each with its look, in the order of the project's categories
// (those without one first)
internal void
LoadMapEntityDefs(editor_state *EditorState, platform_texture_op_queue *TextureOpQueue, editor_mode_map *MapMode)
{
    memory_arena *Arena = &EditorState->ModeArena;
    entity_project *Project = ReadEntityProject(Arena);
    MapMode->EntityProject = Project;
    MapMode->EntityFiles = ListEntityFiles(Project, Arena);
    MapMode->EntityDefCount = MapMode->EntityFiles.Count;
    MapMode->EntityDefs = PushArray(Arena, Maximum(1, MapMode->EntityDefCount), map_entity_def);

    kesa_header Header = {};
    kesa_asset *Stored = ReadStoredAssetList(&EditorState->EditorMeta, &Header);
    for(u32 DefIndex = 0; DefIndex < MapMode->EntityDefCount; ++DefIndex)
    {
        map_entity_def *Def = MapMode->EntityDefs + DefIndex;
        *Def = {};
        Def->FileName = MapMode->EntityFiles.Files[DefIndex];
        Def->File = ReadEntityFile(Def->FileName, Project, Arena);
        if(Def->File.Category > Project->CategoryCount) {Def->File.Category = 0;}
        Def->Align = V2(0.5f, 0.0f);
        Def->FrameCount = 1;
        entity_clip *Clip = 0;
        for(u32 ClipIndex = 0; !Clip && (ClipIndex < Project->ClipCount); ++ClipIndex)
        {
            if(Def->File.Clips[ClipIndex].Kind != EntityLook_None) {Clip = Def->File.Clips + ClipIndex;}
        }
        if(Clip)
        {
            Def->LookKind = Clip->Kind;
            platform_file_type Type = (Clip->Kind == EntityLook_Bitmap) ? PlatformFileType_BMP : PlatformFileType_SSBMP;
            Def->Look = LoadImageFile(Clip->FileName, Type, 0);
            if(Def->Look.Memory)
            {
                AllocateBitmap(TextureOpQueue, &Def->Look);
            }
            kesa_type Wanted = (Clip->Kind == EntityLook_Bitmap) ? KESA_Bitmap : KESA_SpriteSheet;
            for(u32 AssetIndex = 1; Stored && (AssetIndex < Header.AssetCount); ++AssetIndex)
            {
                kesa_asset *Asset = Stored + AssetIndex;
                if((Asset->Type == Wanted) && StringsAreEqual(Asset->SourceFileName, Clip->FileName))
                {
                    Def->Align = (Wanted == KESA_Bitmap) ? Asset->Bitmap.AlignPercentage :
                        Asset->SpriteSheet.SpriteAlignPercentage;
                    Def->FrameCount = (Wanted == KESA_Bitmap) ? 1 : Maximum(1, Asset->SpriteSheet.SpriteCount);
                }
            }
        }
    }
    if(Stored) {Platform.DeallocateMemory(Stored);}

    // NOTE(pvlso): By category, keeping the files' order within one
    for(u32 Index = 1; Index < MapMode->EntityDefCount; ++Index)
    {
        map_entity_def Node = MapMode->EntityDefs[Index];
        s32 At = (s32)Index - 1;
        while((At >= 0) && (MapMode->EntityDefs[At].File.Category > Node.File.Category))
        {
            MapMode->EntityDefs[At + 1] = MapMode->EntityDefs[At];
            --At;
        }
        MapMode->EntityDefs[At + 1] = Node;
    }
    MapMode->EntityDefsLoaded = true;
    LOG("Map editor: %u entity definitions", MapMode->EntityDefCount);
}

internal s32
FindMapEntityDef(editor_mode_map *MapMode, u64 GUID)
{
    s32 Result = -1;
    for(u32 DefIndex = 0; (Result < 0) && (DefIndex < MapMode->EntityDefCount); ++DefIndex)
    {
        if(MapMode->EntityDefs[DefIndex].File.GUID == GUID) {Result = (s32)DefIndex;}
    }

    return(Result);
}

// NOTE(pvlso): A new placement of a definition, its index
internal s32
AddMapEntity(editor_mode_map *MapMode, u64 Definition, v2 P)
{
    ReserveMapEntities(MapMode, MapMode->EntityCount + 1);
    map_placement *Entity = MapMode->Entities + MapMode->EntityCount;
    *Entity = {};
    Entity->GUID = NewEntityGUID();
    Entity->Definition = Definition;
    Entity->P = P;

    return((s32)MapMode->EntityCount++);
}

internal void
RemoveMapEntity(editor_mode_map *MapMode, s32 Index)
{
    if((Index >= 0) && (Index < (s32)MapMode->EntityCount))
    {
        for(u32 At = (u32)Index + 1; At < MapMode->EntityCount; ++At)
        {
            MapMode->Entities[At - 1] = MapMode->Entities[At];
        }
        --MapMode->EntityCount;
        if(MapMode->SelectedEntity == Index) {MapMode->SelectedEntity = -1;}
        else if(MapMode->SelectedEntity > Index) {--MapMode->SelectedEntity;}
    }
}

// NOTE(pvlso): The placement's override of a field, 0 when it hasn't one
internal map_placement_override *
FindMapEntityOverride(map_placement *Entity, u32 Block, u32 Field)
{
    map_placement_override *Result = 0;
    for(u32 Index = 0; !Result && (Index < Entity->OverrideCount); ++Index)
    {
        map_placement_override *Override = Entity->Overrides + Index;
        if((Override->Block == Block) && (Override->Field == Field)) {Result = Override;}
    }

    return(Result);
}

// NOTE(pvlso): A map file's entities into placements, their overrides found in the project's blocks; one the project
// hasn't got (or with a value that isn't one of the field's) is dropped, and goes when the map is saved
internal void
SetMapEntitiesFromFile(editor_mode_map *MapMode, map_file *File)
{
    entity_project *Project = MapMode->EntityProject;
    MapMode->EntityCount = 0;
    MapMode->SelectedEntity = -1;
    ReserveMapEntities(MapMode, File->EntityCount);
    for(u32 EntityIndex = 0; EntityIndex < File->EntityCount; ++EntityIndex)
    {
        map_file_entity *Source = File->Entities + EntityIndex;
        map_placement *Entity = MapMode->Entities + MapMode->EntityCount++;
        *Entity = {};
        Entity->GUID = Source->GUID;
        Entity->Definition = Source->Definition;
        Entity->P = Source->P;
        Entity->Flip = Source->Flip;
        if(FindMapEntityDef(MapMode, Source->Definition) < 0)
        {
            char Hex[17];
            EntityGUIDToHex(Source->Definition, Hex);
            LOG_WARNING("Map editor: an entity at %.1f,%.1f is of a definition not in data/entities (%s), kept as it is",
                        Source->P.x, Source->P.y, Hex);
        }
        for(u32 OverrideIndex = 0; OverrideIndex < Source->OverrideCount; ++OverrideIndex)
        {
            map_file_override *Override = Source->Overrides + OverrideIndex;
            s32 Block = FindEntityBlock(Project, Override->Block);
            s32 Field = (Block >= 0) ? FindEntityBlockField(Project->Blocks + Block, Override->Field) : -1;
            map_placement_override *Dest = Entity->Overrides + Entity->OverrideCount;
            if(Field >= 0)
            {
                Dest->Value = Project->Blocks[Block].Fields[Field].Default;
            }
            if((Field >= 0) && ReadEntityFieldValue(Project->Blocks[Block].Fields + Field, Override->Value, &Dest->Value))
            {
                Dest->Block = (u16)Block;
                Dest->Field = (u16)Field;
                ++Entity->OverrideCount;
            }
            else
            {
                LOG_WARNING("Map editor: an entity overrides %s.%s with %s, which isn't a field of data/txts/%s or a "
                            "value of it: dropped", Override->Block, Override->Field, Override->Value,
                            ENTITY_PROJECT_FILE_NAME);
            }
        }
    }
}

// NOTE(pvlso): The placements as a map file has them, on Arena
internal void
MapEntitiesToFile(editor_mode_map *MapMode, map_file *File, memory_arena *Arena)
{
    entity_project *Project = MapMode->EntityProject;
    File->EntityCount = MapMode->EntityCount;
    File->Entities = PushArray(Arena, Maximum(1, MapMode->EntityCount), map_file_entity);
    for(u32 EntityIndex = 0; EntityIndex < MapMode->EntityCount; ++EntityIndex)
    {
        map_placement *Entity = MapMode->Entities + EntityIndex;
        map_file_entity *Dest = File->Entities + EntityIndex;
        *Dest = {};
        Dest->GUID = Entity->GUID;
        Dest->Definition = Entity->Definition;
        Dest->P = Entity->P;
        Dest->Flip = Entity->Flip;
        for(u32 OverrideIndex = 0; OverrideIndex < Entity->OverrideCount; ++OverrideIndex)
        {
            map_placement_override *Override = Entity->Overrides + OverrideIndex;
            if((Override->Block < Project->BlockCount) && (Override->Field < Project->Blocks[Override->Block].FieldCount))
            {
                entity_block_def *Block = Project->Blocks + Override->Block;
                map_file_override *Out = Dest->Overrides + Dest->OverrideCount++;
                FormatString(sizeof(Out->Block), Out->Block, "%s", Block->Name);
                FormatString(sizeof(Out->Field), Out->Field, "%s", Block->Fields[Override->Field].Name);
                FormatEntityFieldValue(Out->Value, sizeof(Out->Value), Block->Fields + Override->Field, &Override->Value);
            }
        }
    }
}

internal void FreeMapLayers(editor_mode_map *MapMode);
internal void FreeMapHistory(editor_mode_map *MapMode);
internal void RecordMapState(editor_mode_map *MapMode);
internal map_cell *AllocateLayerCells(u32 Width, u32 Height);

// NOTE(pvlso): Opens data/maps/<FileName>: its sets loaded (or found among those already loaded), its layers and cells
internal void
OpenMap(platform_texture_op_queue *TextureOpQueue, editor_mode_map *MapMode, char *FileName, memory_arena *TempArena)
{
    temporary_memory TempMem = BeginTemporaryMemory(TempArena);
    map_file File = ReadMapFile(FileName, TempArena);
    if(File.Loaded)
    {
        u16 SetOf[MAP_FILE_MAX_SETS + 1] = {};
        for(u32 SetIndex = 0; SetIndex < File.SetCount; ++SetIndex)
        {
            s32 Set = MapSetFor(TextureOpQueue, MapMode, File.Sets[SetIndex]);
            SetOf[SetIndex + 1] = (u16)(Set + 1);
        }

        FreeMapLayers(MapMode);
        FreeMapHistory(MapMode);
        MapMode->Width = File.Width;
        MapMode->Height = File.Height;
        MapMode->LayerCount = Minimum(File.LayerCount, (u32)MAP_MAX_LAYERS);
        umm CellCount = (umm)File.Width*File.Height;
        for(u32 LayerIndex = 0; LayerIndex < MapMode->LayerCount; ++LayerIndex)
        {
            map_file_layer *Source = File.Layers + LayerIndex;
            map_layer *Layer = MapMode->Layers + LayerIndex;
            *Layer = {};
            FormatString(sizeof(Layer->Name), Layer->Name, "%s", Source->Name);
            Layer->Z = Source->Z;
            Layer->Hidden = Source->Hidden;
            Layer->Cells = AllocateLayerCells(File.Width, File.Height);
            for(umm Index = 0; Index < CellCount; ++Index)
            {
                map_cell Cell = Source->Cells[Index];
                if(Cell.Set && SetOf[Cell.Set])
                {
                    Cell.Set = SetOf[Cell.Set];
                    Layer->Cells[Index] = Cell;
                }
            }
        }
        SetMapEntitiesFromFile(MapMode, &File);
        MapMode->CurrentLayer = 0;
        MapMode->HasMap = true;
        MapMode->Dirty = false;
        MapMode->Pan = V2(0, 0);
        FormatString(sizeof(MapMode->FileName), MapMode->FileName, "%s", FileName);
        RecordMapState(MapMode);
        LOG("Map editor: opened %s, %u x %u, %u layers, %u entities%s", FileName, File.Width, File.Height,
            File.LayerCount, MapMode->EntityCount, File.ErrorCount ? " (with mistakes, see above)" : "");
    }
    EndTemporaryMemory(TempMem);
}

// NOTE(pvlso): Writes the map to data/maps/<FileName>, with every set the map has loaded, in order
internal b32
SaveMap(editor_mode_map *MapMode, char *FileName, memory_arena *TempArena)
{
    map_file File = {};
    File.Width = MapMode->Width;
    File.Height = MapMode->Height;
    File.SetCount = MapMode->SetCount;
    for(u32 SetIndex = 0; SetIndex < MapMode->SetCount; ++SetIndex)
    {
        FormatString(MAP_FILE_SET_NAME_LENGTH, File.Sets[SetIndex], "%s", MapMode->Sets[SetIndex].FileName);
    }
    File.LayerCount = MapMode->LayerCount;
    for(u32 LayerIndex = 0; LayerIndex < MapMode->LayerCount; ++LayerIndex)
    {
        map_layer *Layer = MapMode->Layers + LayerIndex;
        map_file_layer *Dest = File.Layers + LayerIndex;
        FormatString(sizeof(Dest->Name), Dest->Name, "%s", Layer->Name);
        Dest->Z = Layer->Z;
        Dest->Hidden = Layer->Hidden;
        Dest->Cells = Layer->Cells;
    }
    temporary_memory TempMem = BeginTemporaryMemory(TempArena);
    MapEntitiesToFile(MapMode, &File, TempArena);
    b32 Result = WriteMapFile(FileName, &File, TempArena);
    EndTemporaryMemory(TempMem);
    if(Result)
    {
        FormatString(sizeof(MapMode->FileName), MapMode->FileName, "%s", FileName);
        MapMode->Dirty = false;
        MapMode->ExitArmed = false;
        LOG("Map editor: saved %s", FileName);
    }

    return(Result);
}

internal void
AddMapLayer(editor_mode_map *MapMode, char *Name, s32 Z)
{
    if(MapMode->LayerCount < MAP_MAX_LAYERS)
    {
        map_layer *Layer = MapMode->Layers + MapMode->LayerCount++;
        *Layer = {};
        FormatString(sizeof(Layer->Name), Layer->Name, "%s", Name);
        Layer->Z = Z;
        Layer->Cells = AllocateLayerCells(MapMode->Width, MapMode->Height);
        MapMode->CurrentLayer = (s32)MapMode->LayerCount - 1;
    }
    else
    {
        LOG_ERROR("A map has at most %d layers", MAP_MAX_LAYERS);
    }
}

internal void
NewMap(editor_mode_map *MapMode, u32 Width, u32 Height)
{
    FreeMapLayers(MapMode);
    FreeMapHistory(MapMode);
    MapMode->Width = (u32)ClampCell(1, (s32)Width, MAP_MAX_SIDE);
    MapMode->Height = (u32)ClampCell(1, (s32)Height, MAP_MAX_SIDE);
    MapMode->LayerCount = 0;
    MapMode->EntityCount = 0;
    MapMode->SelectedEntity = -1;
    AddMapLayer(MapMode, "Ground", 0);
    MapMode->HasMap = true;
    MapMode->Pan = V2(0, 0);
    RecordMapState(MapMode);
    LOG("Map editor: new map %u x %u", MapMode->Width, MapMode->Height);
}

// NOTE(pvlso): The brush laid with its top left on (X, Y) of the current layer; cells of the block the set has no tile
// for are left as they are
internal void
StampBrush(editor_mode_map *MapMode, s32 X, s32 Y)
{
    map_tileset *Set = MapMode->Sets + MapMode->CurrentSet;
    map_layer *Layer = MapMode->Layers + MapMode->CurrentLayer;
    s32 X0 = Minimum(MapMode->BrushX0, MapMode->BrushX1);
    s32 X1 = Maximum(MapMode->BrushX0, MapMode->BrushX1);
    s32 Y0 = Minimum(MapMode->BrushY0, MapMode->BrushY1);
    s32 Y1 = Maximum(MapMode->BrushY0, MapMode->BrushY1);
    for(s32 BY = Y0; BY <= Y1; ++BY)
    {
        for(s32 BX = X0; BX <= X1; ++BX)
        {
            s32 CellX = X + BX - X0;
            s32 CellY = Y + BY - Y0;
            if((CellX >= 0) && (CellY >= 0) && ((u32)CellX < MapMode->Width) && ((u32)CellY < MapMode->Height) &&
               (MapTileAt(Set, BX, BY) >= 0))
            {
                map_cell *Cell = Layer->Cells + CellY*MapMode->Width + CellX;
                Cell->Set = (u16)(MapMode->CurrentSet + 1);
                Cell->X = (u16)BX;
                Cell->Y = (u16)BY;
            }
        }
    }
}

// NOTE(pvlso): A rectangle of the current layer filled with the brush, a block brush repeated from the rectangle's top
// left, or cleared
internal void
FillMapRect(editor_mode_map *MapMode, s32 X0, s32 Y0, s32 X1, s32 Y1, b32 Erase)
{
    map_layer *Layer = MapMode->Layers + MapMode->CurrentLayer;
    s32 MinX = Maximum(0, Minimum(X0, X1));
    s32 MaxX = Minimum((s32)MapMode->Width - 1, Maximum(X0, X1));
    s32 MinY = Maximum(0, Minimum(Y0, Y1));
    s32 MaxY = Minimum((s32)MapMode->Height - 1, Maximum(Y0, Y1));
    s32 BrushX = Minimum(MapMode->BrushX0, MapMode->BrushX1);
    s32 BrushY = Minimum(MapMode->BrushY0, MapMode->BrushY1);
    s32 W = AbsoluteValue(MapMode->BrushX1 - MapMode->BrushX0) + 1;
    s32 H = AbsoluteValue(MapMode->BrushY1 - MapMode->BrushY0) + 1;
    for(s32 Y = MinY; Y <= MaxY; ++Y)
    {
        for(s32 X = MinX; X <= MaxX; ++X)
        {
            map_cell *Cell = Layer->Cells + Y*MapMode->Width + X;
            if(Erase)
            {
                *Cell = {};
            }
            else
            {
                s32 TileX = BrushX + (X - Minimum(X0, X1)) % W;
                s32 TileY = BrushY + (Y - Minimum(Y0, Y1)) % H;
                if(MapTileAt(MapMode->Sets + MapMode->CurrentSet, TileX, TileY) >= 0)
                {
                    Cell->Set = (u16)(MapMode->CurrentSet + 1);
                    Cell->X = (u16)TileX;
                    Cell->Y = (u16)TileY;
                }
            }
        }
    }
}

// NOTE(pvlso): The eyedropper: the tile under the mouse on the current layer becomes the brush, its set the palette's
internal void
PickBrushFromMap(editor_mode_map *MapMode, s32 X, s32 Y)
{
    map_cell Cell = MapMode->Layers[MapMode->CurrentLayer].Cells[Y*MapMode->Width + X];
    if(Cell.Set && (Cell.Set <= MapMode->SetCount))
    {
        MapMode->CurrentSet = Cell.Set - 1;
        MapMode->HasBrush = true;
        MapMode->BrushX0 = MapMode->BrushX1 = Cell.X;
        MapMode->BrushY0 = MapMode->BrushY1 = Cell.Y;
    }
}

// NOTE(pvlso): The map's layers in drawing order: by Z, those sharing a Z in the list's order
internal u32
MapLayerOrder(editor_mode_map *MapMode, u32 *Order)
{
    u32 Count = MapMode->LayerCount;
    for(u32 Index = 0; Index < Count; ++Index) {Order[Index] = Index;}
    for(u32 Index = 1; Index < Count; ++Index)
    {
        u32 Node = Order[Index];
        s32 At = (s32)Index - 1;
        while((At >= 0) && (MapMode->Layers[Order[At]].Z > MapMode->Layers[Node].Z))
        {
            Order[At + 1] = Order[At];
            --At;
        }
        Order[At + 1] = Node;
    }

    return(Count);
}

// NOTE(pvlso): A cell's size in the pixels of the map's tiles (and so of its entities' looks)
inline r32
MapPixelsPerCell(editor_mode_map *MapMode)
{
    r32 Result = 32.0f;
    if(MapMode->SetCount && MapMode->Sets[0].File.TileSize) {Result = (r32)MapMode->Sets[0].File.TileSize;}

    return(Result);
}

inline v2
MapCellOf(tileset_grid_view *View, v2 P)
{
    v2 Result = V2((P.x - View->TopLeft.x) / View->Cell, (View->TopLeft.y - P.y) / View->Cell);
    return(Result);
}

inline v2
SnapToCellCenter(v2 P)
{
    v2 Result = V2((r32)FloorReal32ToInt32(P.x) + 0.5f, (r32)FloorReal32ToInt32(P.y) + 0.5f);
    return(Result);
}

inline map_entity_def *
MapEntityDefOf(editor_mode_map *MapMode, map_placement *Entity)
{
    s32 DefIndex = FindMapEntityDef(MapMode, Entity->Definition);
    map_entity_def *Result = (DefIndex >= 0) ? (MapMode->EntityDefs + DefIndex) : 0;

    return(Result);
}

// NOTE(pvlso): Where a look is drawn on the view: its first frame standing on the foot (at P), mirrored when flipped.
// Without a look, a square over the foot.
internal rectangle2
MapEntityRect(editor_mode_map *MapMode, tileset_grid_view *View, map_entity_def *Def, v2 P, b32 Flip)
{
    v2 Foot = View->TopLeft + View->Cell*V2(P.x, -P.y);
    rectangle2 Result = RectMinDim(Foot - V2(0.25f*View->Cell, 0.0f), V2(0.5f*View->Cell, 0.5f*View->Cell));
    if(Def && Def->Look.Memory)
    {
        r32 Scale = View->Cell / MapPixelsPerCell(MapMode);
        v2 Size = Scale*V2((r32)Def->Look.Width / (r32)Def->FrameCount, (r32)Def->Look.Height);
        v2 Align = V2(Flip ? (1.0f - Def->Align.x) : Def->Align.x, Def->Align.y);
        Result = RectMinDim(Foot - Hadamard(Align, Size), Size);
    }

    return(Result);
}

internal void
PushMapEntity(render_group *RenderGroup, object_transform *Flat, editor_mode_map *MapMode, tileset_grid_view *View,
              map_entity_def *Def, v2 P, b32 Flip, r32 Z, r32 Alpha)
{
    rectangle2 Rect = MapEntityRect(MapMode, View, Def, P, Flip);
    if(Def && Def->Look.Memory)
    {
        r32 FrameU = 1.0f / (r32)Def->FrameCount;
        v2 MinUV = V2(Flip ? FrameU : 0.0f, 0.0f);
        v2 MaxUV = V2(Flip ? 0.0f : FrameU, 1.0f);
        PushBitmapPart(RenderGroup, Flat, &Def->Look, Rect, Z, MinUV, MaxUV, V4(1, 1, 1, Alpha));
    }
    else
    {
        PushRect(RenderGroup, Flat, Rect, Z, V4(1.0f, 0.2f, 0.8f, 0.8f*Alpha));
    }
}

// NOTE(pvlso): The frontmost placement whose look is under P, -1 for none
internal s32
MapEntityUnder(editor_mode_map *MapMode, tileset_grid_view *View, v2 P)
{
    s32 Result = -1;
    for(u32 EntityIndex = 0; EntityIndex < MapMode->EntityCount; ++EntityIndex)
    {
        map_placement *Entity = MapMode->Entities + EntityIndex;
        rectangle2 Rect = MapEntityRect(MapMode, View, MapEntityDefOf(MapMode, Entity), Entity->P, Entity->Flip);
        if(IsInRectangle(Rect, P) && ((Result < 0) || (Entity->P.y >= MapMode->Entities[Result].P.y)))
        {
            Result = (s32)EntityIndex;
        }
    }

    return(Result);
}

// NOTE(pvlso): Entities stand over every layer, those lower on the map in front
inline r32
MapEntityZ(editor_mode_map *MapMode, v2 P)
{
    r32 Result = 6.4f + 0.5f*Clamp01(P.y / Maximum(1.0f, (r32)MapMode->Height));
    return(Result);
}

// NOTE(pvlso): The Entities tool on the map (the panel's help says how). True when a change was finished, for the
// history.
internal b32
UpdateMapEntityTool(editor_mode_map *MapMode, engine_input *Input, tileset_grid_view *View, v2 MouseP, b32 Inside)
{
    b32 Finished = false;
    engine_button_state Left = Input->MouseButtons[PlatformMouseButton_Left];
    engine_button_state Right = Input->MouseButtons[PlatformMouseButton_Right];
    v2 MouseCell = MapCellOf(View, MouseP);
    v2 MapSize = V2((r32)MapMode->Width, (r32)MapMode->Height);
    b32 OnMap = Inside && (MouseCell.x >= 0.0f) && (MouseCell.y >= 0.0f) && (MouseCell.x < MapSize.x) &&
        (MouseCell.y < MapSize.y);
    if(Inside && !MapMode->DraggingEntity)
    {
        s32 Under = MapEntityUnder(MapMode, View, MouseP);
        if(WasPressed(Left))
        {
            if(Input->AltDown)
            {
                s32 Def = (Under >= 0) ? FindMapEntityDef(MapMode, MapMode->Entities[Under].Definition) : -1;
                if(Def >= 0) {MapMode->CurrentDef = Def;}
            }
            else if(Under >= 0)
            {
                MapMode->SelectedEntity = Under;
                MapMode->DraggingEntity = true;
                MapMode->EntityMoved = false;
                MapMode->DragOffset = MapMode->Entities[Under].P - MouseCell;
            }
            else if(OnMap && (MapMode->CurrentDef >= 0))
            {
                v2 P = Input->ControlDown ? SnapToCellCenter(MouseCell) : MouseCell;
                MapMode->SelectedEntity = AddMapEntity(MapMode, MapMode->EntityDefs[MapMode->CurrentDef].File.GUID, P);
                MapMode->DraggingEntity = true;
                MapMode->EntityMoved = false;
                MapMode->DragOffset = P - MouseCell;
                Finished = true;
            }
            else
            {
                MapMode->SelectedEntity = -1;
            }
        }
        else if(WasPressed(Right) && (Under >= 0))
        {
            RemoveMapEntity(MapMode, Under);
            Finished = true;
        }
    }
    if(MapMode->DraggingEntity)
    {
        if(!Left.EndedDown || (MapMode->SelectedEntity < 0))
        {
            MapMode->DraggingEntity = false;
            Finished |= MapMode->EntityMoved;
        }
        else
        {
            v2 P = MouseCell + MapMode->DragOffset;
            if(Input->ControlDown) {P = SnapToCellCenter(P);}
            P = V2(Clamp(0.0f, P.x, MapSize.x), Clamp(0.0f, P.y, MapSize.y));
            map_placement *Entity = MapMode->Entities + MapMode->SelectedEntity;
            if((P.x != Entity->P.x) || (P.y != Entity->P.y))
            {
                Entity->P = P;
                MapMode->EntityMoved = true;
            }
        }
    }

    return(Finished);
}

// NOTE(pvlso): The placements over the layers; in the Entities tool the selected one's outline, footprint (green) and
// hit area (blue), and the picked definition under the mouse where a click would place it
internal void
RenderMapEntities(editor_mode_map *MapMode, engine_input *Input, render_group *RenderGroup, object_transform *Flat,
                  tileset_grid_view *View, v2 MouseP, b32 Inside)
{
    b32 Tool = (MapMode->Tool == MapTool_Entities);
    for(u32 EntityIndex = 0; EntityIndex < MapMode->EntityCount; ++EntityIndex)
    {
        map_placement *Entity = MapMode->Entities + EntityIndex;
        PushMapEntity(RenderGroup, Flat, MapMode, View, MapEntityDefOf(MapMode, Entity), Entity->P, Entity->Flip,
                      MapEntityZ(MapMode, Entity->P), 1.0f);
    }

    if(Tool && (MapMode->SelectedEntity >= 0))
    {
        map_placement *Entity = MapMode->Entities + MapMode->SelectedEntity;
        map_entity_def *Def = MapEntityDefOf(MapMode, Entity);
        PushRectOutline(RenderGroup, Flat, MapEntityRect(MapMode, View, Def, Entity->P, Entity->Flip), 8.0f,
                        V4(1, 0.85f, 0.2f, 1), 1.0f);
        if(Def)
        {
            v2 Foot = View->TopLeft + View->Cell*V2(Entity->P.x, -Entity->P.y);
            r32 Scale = View->Cell / MapPixelsPerCell(MapMode);
            entity_shape HitArea = Def->File.HitArea;
            entity_shape Footprint = Def->File.Footprint;
            if(Entity->Flip)
            {
                HitArea.Center.x = -HitArea.Center.x;
                Footprint.Center.x = -Footprint.Center.x;
            }
            RenderEntityShape(RenderGroup, Flat, &HitArea, Foot, Scale, V4(0.3f, 0.6f, 1.0f, 1.0f));
            RenderEntityShape(RenderGroup, Flat, &Footprint, Foot, Scale, V4(0.2f, 1.0f, 0.4f, 1.0f));
        }
    }

    v2 MouseCell = MapCellOf(View, MouseP);
    b32 OnMap = Inside && (MouseCell.x >= 0.0f) && (MouseCell.y >= 0.0f) && (MouseCell.x < (r32)MapMode->Width) &&
        (MouseCell.y < (r32)MapMode->Height);
    if(Tool && OnMap && !MapMode->DraggingEntity && !Input->AltDown && (MapMode->CurrentDef >= 0) &&
       (MapEntityUnder(MapMode, View, MouseP) < 0))
    {
        v2 P = Input->ControlDown ? SnapToCellCenter(MouseCell) : MouseCell;
        PushMapEntity(RenderGroup, Flat, MapMode, View, MapMode->EntityDefs + MapMode->CurrentDef, P, false, 6.98f, 0.5f);
    }
}

// NOTE(pvlso): The definitions as a grid of their looks: a click picks the one to place, the wheel scrolls
internal void
UpdateAndRenderEntityPalette(editor_mode_map *MapMode, engine_input *Input, render_group *RenderGroup,
                             object_transform *Flat, v2 MouseP, rectangle2 Rect, r32 Wheel)
{
    r32 Cell = 96.0f;
    r32 Pad = 8.0f;
    u32 Columns = Maximum(1u, (u32)((GetDim(Rect).x - Pad) / Cell));
    u32 Rows = (MapMode->EntityDefCount + Columns - 1) / Columns;
    r32 MaxScroll = Maximum(0.0f, (r32)Rows*Cell + Pad - GetDim(Rect).y);
    b32 Inside = IsInRectangle(Rect, MouseP);
    if(Inside && (Wheel != 0.0f))
    {
        MapMode->EntityPaletteScroll -= ((Wheel > 0.0f) ? 0.5f : -0.5f)*Cell;
    }
    MapMode->EntityPaletteScroll = Clamp(0.0f, MapMode->EntityPaletteScroll, MaxScroll);
    if(!Input->MouseButtons[PlatformMouseButton_Middle].EndedDown) {MapMode->Panning = 0;}

    u32 SavedClip = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
    PushRect(RenderGroup, Flat, Rect, 4.0f, V4(0.25f, 0.25f, 0.25f, 1.0f));
    for(u32 DefIndex = 0; DefIndex < MapMode->EntityDefCount; ++DefIndex)
    {
        map_entity_def *Def = MapMode->EntityDefs + DefIndex;
        u32 Column = DefIndex % Columns;
        u32 Row = DefIndex / Columns;
        v2 Min = V2(Rect.Min.x + Pad + (r32)Column*Cell, Rect.Max.y - (r32)(Row + 1)*Cell + MapMode->EntityPaletteScroll);
        rectangle2 Box = RectMinDim(Min, V2(Cell - Pad, Cell - Pad));
        b32 Hover = Inside && IsInRectangle(Box, MouseP);
        PushRect(RenderGroup, Flat, Box, 5.0f, Hover ? V4(0.4f, 0.4f, 0.42f, 1.0f) : V4(0.32f, 0.32f, 0.34f, 1.0f));
        if(Def->Look.Memory)
        {
            v2 Frame = V2((r32)Def->Look.Width / (r32)Def->FrameCount, (r32)Def->Look.Height);
            r32 Fit = Minimum((GetDim(Box).x - Pad) / Frame.x, (GetDim(Box).y - Pad) / Frame.y);
            if(Fit >= 1.0f) {Fit = (r32)FloorReal32ToInt32(Fit);}
            v2 Size = Fit*Frame;
            PushBitmapPart(RenderGroup, Flat, &Def->Look, RectMinDim(GetCenter(Box) - 0.5f*Size, Size), 6.0f,
                           V2(0, 0), V2(1.0f / (r32)Def->FrameCount, 1.0f));
        }
        else
        {
            PushRect(RenderGroup, Flat, RectCenterDim(GetCenter(Box), V2(16, 16)), 6.0f, V4(1.0f, 0.2f, 0.8f, 0.8f));
        }
        if((s32)DefIndex == MapMode->CurrentDef)
        {
            PushRectOutline(RenderGroup, Flat, Box, 8.0f, V4(1, 0.85f, 0.2f, 1), 2.0f);
        }
        if(Hover && WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
        {
            MapMode->CurrentDef = (s32)DefIndex;
        }
    }
    RenderGroup->CurrentClipRectIndex = SavedClip;
}

// NOTE(pvlso): True when a change was finished this frame (a stroke, a rectangle), for the history
internal b32
UpdateAndRenderMapView(editor_mode_map *MapMode, engine_input *Input, render_group *RenderGroup, object_transform *Flat,
                       v2 MouseP, rectangle2 Rect, r32 Wheel)
{
    b32 Finished = false;
    b32 Inside = IsInRectangle(Rect, MouseP);
    engine_button_state Left = Input->MouseButtons[PlatformMouseButton_Left];
    engine_button_state Right = Input->MouseButtons[PlatformMouseButton_Right];
    engine_button_state Middle = Input->MouseButtons[PlatformMouseButton_Middle];
    if(Inside)
    {
        ZoomGridView(&MapMode->Zoom, &MapMode->Pan, Rect, MouseP, Wheel, 4.0f, 96.0f);
        if(WasPressed(Middle)) {MapMode->Panning = 1; MapMode->PanFrom = MouseP;}
    }
    if((MapMode->Panning == 1) && Middle.EndedDown)
    {
        MapMode->Pan += V2(MouseP.x - MapMode->PanFrom.x, MapMode->PanFrom.y - MouseP.y);
        MapMode->PanFrom = MouseP;
    }

    tileset_grid_view View = GridView(Rect, MapMode->Pan, MapMode->Zoom);
    s32 CellX, CellY;
    CellUnder(&View, MouseP, &CellX, &CellY);
    b32 OnMap = Inside && (CellX >= 0) && (CellY >= 0) && ((u32)CellX < MapMode->Width) && ((u32)CellY < MapMode->Height);
    MapMode->HoverX = OnMap ? CellX : -1;
    MapMode->HoverY = OnMap ? CellY : -1;

    b32 CanPaint = MapMode->HasBrush && (MapMode->CurrentSet >= 0) && MapMode->LayerCount;
    b32 Busy = MapMode->Painting || MapMode->Erasing || MapMode->RectActive;
    b32 TileTool = (MapMode->Tool == MapTool_Tiles);
    if(!TileTool)
    {
        Finished |= UpdateMapEntityTool(MapMode, Input, &View, MouseP, Inside);
    }
    if(TileTool && OnMap && !Busy && MapMode->LayerCount)
    {
        if(Input->AltDown && WasPressed(Left))
        {
            PickBrushFromMap(MapMode, CellX, CellY);
        }
        else if(Input->ShiftDown && ((CanPaint && WasPressed(Left)) || WasPressed(Right)))
        {
            MapMode->RectActive = true;
            MapMode->RectErase = !WasPressed(Left);
            MapMode->RectX0 = CellX;
            MapMode->RectY0 = CellY;
        }
        else if(CanPaint && WasPressed(Left))
        {
            MapMode->Painting = true;
            MapMode->PaintFromX = MapMode->LastStampX = CellX;
            MapMode->PaintFromY = MapMode->LastStampY = CellY;
            StampBrush(MapMode, CellX, CellY);
        }
        else if(WasPressed(Right))
        {
            MapMode->Erasing = true;
        }
    }
    if(MapMode->Painting)
    {
        if(!Left.EndedDown)
        {
            MapMode->Painting = false;
            Finished = true;
        }
        else if(OnMap)
        {
            // NOTE(pvlso): A block brush is laid on a grid of its own size from where the drag started, so blocks
            // line up instead of overlapping
            s32 W = AbsoluteValue(MapMode->BrushX1 - MapMode->BrushX0) + 1;
            s32 H = AbsoluteValue(MapMode->BrushY1 - MapMode->BrushY0) + 1;
            s32 X = MapMode->PaintFromX + W*FloorReal32ToInt32((r32)(CellX - MapMode->PaintFromX) / (r32)W);
            s32 Y = MapMode->PaintFromY + H*FloorReal32ToInt32((r32)(CellY - MapMode->PaintFromY) / (r32)H);
            if((X != MapMode->LastStampX) || (Y != MapMode->LastStampY))
            {
                StampBrush(MapMode, X, Y);
                MapMode->LastStampX = X;
                MapMode->LastStampY = Y;
            }
        }
    }
    if(MapMode->Erasing)
    {
        if(!Right.EndedDown)
        {
            MapMode->Erasing = false;
            Finished = true;
        }
        else if(OnMap)
        {
            MapMode->Layers[MapMode->CurrentLayer].Cells[CellY*MapMode->Width + CellX] = {};
        }
    }
    s32 RectX1 = ClampCell(0, CellX, (s32)MapMode->Width - 1);
    s32 RectY1 = ClampCell(0, CellY, (s32)MapMode->Height - 1);
    if(MapMode->RectActive && !(MapMode->RectErase ? Right.EndedDown : Left.EndedDown))
    {
        FillMapRect(MapMode, MapMode->RectX0, MapMode->RectY0, RectX1, RectY1, MapMode->RectErase);
        MapMode->RectActive = false;
        Finished = true;
    }

    u32 SavedClip = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
    PushRect(RenderGroup, Flat, Rect, 4.0f, V4(0.12f, 0.12f, 0.14f, 1.0f));
    v2 MapMin = View.TopLeft + V2(0.0f, -(r32)MapMode->Height*View.Cell);
    v2 MapMax = View.TopLeft + V2((r32)MapMode->Width*View.Cell, 0.0f);
    PushRect(RenderGroup, Flat, RectMinMax(MapMin, MapMax), 5.0f, V4(0.22f, 0.22f, 0.24f, 1.0f));

    // NOTE(pvlso): Only the cells in view
    s32 FirstX = ClampCell(0, FloorReal32ToInt32((Rect.Min.x - View.TopLeft.x) / View.Cell), (s32)MapMode->Width);
    s32 LastX = ClampCell(0, CeilReal32ToInt32((Rect.Max.x - View.TopLeft.x) / View.Cell), (s32)MapMode->Width);
    s32 FirstY = ClampCell(0, FloorReal32ToInt32((View.TopLeft.y - Rect.Max.y) / View.Cell), (s32)MapMode->Height);
    s32 LastY = ClampCell(0, CeilReal32ToInt32((View.TopLeft.y - Rect.Min.y) / View.Cell), (s32)MapMode->Height);
    u32 Order[MAP_MAX_LAYERS];
    u32 OrderCount = MapLayerOrder(MapMode, Order);
    for(u32 OrderIndex = 0; OrderIndex < OrderCount; ++OrderIndex)
    {
        u32 LayerIndex = Order[OrderIndex];
        map_layer *Layer = MapMode->Layers + LayerIndex;
        if(Layer->Hidden) {continue;}
        b32 Dim = MapMode->DimOtherLayers && ((s32)LayerIndex != MapMode->CurrentLayer);
        v4 Color = Dim ? V4(1, 1, 1, 0.3f) : V4(1, 1, 1, 1);
        r32 Z = 6.0f + 0.01f*(r32)OrderIndex;
        for(s32 Y = FirstY; Y < LastY; ++Y)
        {
            for(s32 X = FirstX; X < LastX; ++X)
            {
                map_cell *Cell = Layer->Cells + Y*MapMode->Width + X;
                if(Cell->Set && (Cell->Set <= MapMode->SetCount))
                {
                    map_tileset *Set = MapMode->Sets + Cell->Set - 1;
                    s32 TileIndex = MapTileAt(Set, Cell->X, Cell->Y);
                    if((TileIndex >= 0) && Set->Tiles[TileIndex].Memory)
                    {
                        PushBitmap(RenderGroup, Flat, Set->Tiles + TileIndex, View.Cell, V3(CellCenter(&View, X, Y), Z),
                                   Color);
                    }
                }
            }
        }
    }

    if(MapMode->ShowEntities || !TileTool)
    {
        RenderMapEntities(MapMode, Input, RenderGroup, Flat, &View, MouseP, Inside);
    }

    if(MapMode->ShowGrid && (View.Cell >= 8.0f))
    {
        for(s32 X = FirstX; X <= LastX; ++X)
        {
            r32 LineX = View.TopLeft.x + (r32)X*View.Cell;
            PushLine(RenderGroup, Flat, V3(LineX, Maximum(MapMin.y, Rect.Min.y), 7.0f),
                     V3(LineX, Minimum(MapMax.y, Rect.Max.y), 7.0f), V4(1, 1, 1, 0.08f));
        }
        for(s32 Y = FirstY; Y <= LastY; ++Y)
        {
            r32 LineY = View.TopLeft.y - (r32)Y*View.Cell;
            PushLine(RenderGroup, Flat, V3(Maximum(MapMin.x, Rect.Min.x), LineY, 7.0f),
                     V3(Minimum(MapMax.x, Rect.Max.x), LineY, 7.0f), V4(1, 1, 1, 0.08f));
        }
    }
    PushRectOutline(RenderGroup, Flat, RectMinMax(MapMin, MapMax), 7.5f, V4(0.8f, 0.8f, 0.8f, 1), 1.0f);
    if(MapMode->RectActive)
    {
        v2 Min = View.TopLeft + V2((r32)Minimum(MapMode->RectX0, RectX1)*View.Cell,
                                   -(r32)(Maximum(MapMode->RectY0, RectY1) + 1)*View.Cell);
        v2 Max = View.TopLeft + V2((r32)(Maximum(MapMode->RectX0, RectX1) + 1)*View.Cell,
                                   -(r32)Minimum(MapMode->RectY0, RectY1)*View.Cell);
        v4 Color = MapMode->RectErase ? V4(1, 0.3f, 0.3f, 1) : V4(1, 0.85f, 0.2f, 1);
        PushRectOutline(RenderGroup, Flat, RectMinMax(Min, Max), 8.0f, Color, 2.0f);
    }
    else if(OnMap && TileTool)
    {
        s32 W = MapMode->HasBrush ? (AbsoluteValue(MapMode->BrushX1 - MapMode->BrushX0) + 1) : 1;
        s32 H = MapMode->HasBrush ? (AbsoluteValue(MapMode->BrushY1 - MapMode->BrushY0) + 1) : 1;
        v2 Min = View.TopLeft + V2((r32)CellX*View.Cell, -(r32)(CellY + H)*View.Cell);
        v2 Max = View.TopLeft + V2((r32)(CellX + W)*View.Cell, -(r32)CellY*View.Cell);
        PushRectOutline(RenderGroup, Flat, RectMinMax(Min, Max), 8.0f, V4(1, 0.85f, 0.2f, 0.9f), 1.0f);
    }
    RenderGroup->CurrentClipRectIndex = SavedClip;

    return(Finished);
}

// NOTE(pvlso): The current tile set's grid: a left drag picks the brush (a tile or a block)
internal void
UpdateAndRenderPalette(editor_mode_map *MapMode, engine_input *Input, render_group *RenderGroup, object_transform *Flat,
                       v2 MouseP, rectangle2 Rect, r32 Wheel)
{
    b32 Inside = IsInRectangle(Rect, MouseP);
    engine_button_state Left = Input->MouseButtons[PlatformMouseButton_Left];
    engine_button_state Middle = Input->MouseButtons[PlatformMouseButton_Middle];
    if(Inside)
    {
        ZoomGridView(&MapMode->PaletteZoom, &MapMode->PalettePan, Rect, MouseP, Wheel, 8.0f, 96.0f);
        if(WasPressed(Middle)) {MapMode->Panning = 2; MapMode->PanFrom = MouseP;}
    }
    if((MapMode->Panning == 2) && Middle.EndedDown)
    {
        MapMode->PalettePan += V2(MouseP.x - MapMode->PanFrom.x, MapMode->PanFrom.y - MouseP.y);
        MapMode->PanFrom = MouseP;
    }
    if(!Middle.EndedDown) {MapMode->Panning = 0;}

    u32 SavedClip = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
    PushRect(RenderGroup, Flat, Rect, 4.0f, V4(0.25f, 0.25f, 0.25f, 1.0f));
    if(MapMode->CurrentSet >= 0)
    {
        map_tileset *Set = MapMode->Sets + MapMode->CurrentSet;
        tileset_file *File = &Set->File;
        tileset_grid_view View = GridView(Rect, MapMode->PalettePan, MapMode->PaletteZoom);
        s32 CellX, CellY;
        CellUnder(&View, MouseP, &CellX, &CellY);
        CellX = ClampCell(0, CellX, Maximum(0, (s32)File->GridWidth - 1));
        CellY = ClampCell(0, CellY, Maximum(0, (s32)File->GridHeight - 1));
        if(Inside && File->TileCount && WasPressed(Left))
        {
            MapMode->PickingBrush = true;
            MapMode->HasBrush = true;
            MapMode->BrushX0 = MapMode->BrushX1 = CellX;
            MapMode->BrushY0 = MapMode->BrushY1 = CellY;
        }
        if(MapMode->PickingBrush)
        {
            MapMode->BrushX1 = CellX;
            MapMode->BrushY1 = CellY;
            if(!Left.EndedDown) {MapMode->PickingBrush = false;}
        }

        DrawGridLines(RenderGroup, Flat, &View, 5.0f, V4(1, 1, 1, 0.1f));
        for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
        {
            tileset_file_tile *Tile = File->Tiles + TileIndex;
            if(Set->Tiles[TileIndex].Memory)
            {
                PushBitmap(RenderGroup, Flat, Set->Tiles + TileIndex, View.Cell,
                           V3(CellCenter(&View, Tile->GridX, Tile->GridY), 6.0f));
            }
        }
        if(MapMode->HasBrush)
        {
            s32 X0 = Minimum(MapMode->BrushX0, MapMode->BrushX1);
            s32 X1 = Maximum(MapMode->BrushX0, MapMode->BrushX1);
            s32 Y0 = Minimum(MapMode->BrushY0, MapMode->BrushY1);
            s32 Y1 = Maximum(MapMode->BrushY0, MapMode->BrushY1);
            v2 Min = View.TopLeft + V2((r32)X0*View.Cell, -(r32)(Y1 + 1)*View.Cell);
            v2 Max = View.TopLeft + V2((r32)(X1 + 1)*View.Cell, -(r32)Y0*View.Cell);
            PushRectOutline(RenderGroup, Flat, RectMinMax(Min, Max), 8.0f, V4(1, 0.85f, 0.2f, 1), 2.0f);
        }
    }
    RenderGroup->CurrentClipRectIndex = SavedClip;
}

// NOTE(pvlso): One field of the selected placement: ticked, it's overridden and edited here; unticked, it shows the
// definition's value. True while a text is typed in.
internal b32
DrawMapEntityField(editor_mode_map *MapMode, nk_context *Nk, map_placement *Entity, map_entity_def *Def, u32 Block,
                   u32 Field)
{
    b32 Typing = false;
    entity_field_def *FieldDef = MapMode->EntityProject->Blocks[Block].Fields + Field;
    map_placement_override *Override = FindMapEntityOverride(Entity, Block, Field);
    nk_layout_row_dynamic(Nk, 22, 1);
    if(Override)
    {
        char Label[64];
        FormatString(sizeof(Label), Label, "%s (overridden)", FieldDef->Name);
        nk_bool On = true;
        if(nk_checkbox_label(Nk, Label, &On) && !On)
        {
            u32 Index = (u32)(Override - Entity->Overrides);
            for(u32 At = Index + 1; At < Entity->OverrideCount; ++At)
            {
                Entity->Overrides[At - 1] = Entity->Overrides[At];
            }
            --Entity->OverrideCount;
            Override = 0;
        }
        if(Override)
        {
            Typing = DrawEntityFieldEditor(&MapMode->EntityFiles, Nk, FieldDef, &Override->Value);
        }
    }
    else
    {
        entity_field_value *Value = &Def->File.Fields[Block][Field];
        char Text[ENTITY_TEXT_LENGTH + 8];
        char *Name = (FieldDef->Kind == EntityField_Entity) ? EntityFileOfGUID(&MapMode->EntityFiles, Value->Entity) : 0;
        if(Name) {FormatString(sizeof(Text), Text, "%s", Name);}
        else {FormatEntityFieldValue(Text, sizeof(Text), FieldDef, Value);}
        char Label[ENTITY_TEXT_LENGTH + 64];
        FormatString(sizeof(Label), Label, "%s: %s", FieldDef->Name, Text);
        nk_bool On = false;
        if(nk_checkbox_label(Nk, Label, &On) && On)
        {
            if(Entity->OverrideCount < MAP_FILE_MAX_ENTITY_OVERRIDES)
            {
                map_placement_override *New = Entity->Overrides + Entity->OverrideCount++;
                New->Block = (u16)Block;
                New->Field = (u16)Field;
                New->Value = *Value;
            }
            else
            {
                LOG_WARNING("An entity on a map overrides at most %d fields", MAP_FILE_MAX_ENTITY_OVERRIDES);
            }
        }
    }

    return(Typing);
}

// NOTE(pvlso): The Entities tool's part of the panel: the definitions to place, the selected placement. True while a
// text is typed in.
internal b32
DrawMapEntityPanel(editor_mode_map *MapMode, nk_context *Nk)
{
    b32 Typing = false;
    entity_project *Project = MapMode->EntityProject;
    nk_layout_row_dynamic(Nk, 24, 1);
    nk_label(Nk, "", NK_TEXT_LEFT);
    nk_labelf(Nk, NK_TEXT_LEFT, "Entities (data/entities), %u placed:", MapMode->EntityCount);
    nk_layout_row_dynamic(Nk, 20, 1);
    u32 Category = (u32)-1;
    for(u32 DefIndex = 0; DefIndex < MapMode->EntityDefCount; ++DefIndex)
    {
        map_entity_def *Def = MapMode->EntityDefs + DefIndex;
        if(Def->File.Category != Category)
        {
            Category = Def->File.Category;
            nk_label(Nk, Category ? Project->Categories[Category - 1] : "(no category)", NK_TEXT_LEFT);
        }
        char Label[128];
        FormatString(sizeof(Label), Label, "    %s  %s", Def->FileName, Def->File.DisplayName);
        nk_bool Picked = ((s32)DefIndex == MapMode->CurrentDef);
        if(nk_selectable_label(Nk, Label, NK_TEXT_LEFT, &Picked)) {MapMode->CurrentDef = Picked ? (s32)DefIndex : -1;}
    }
    if(!MapMode->EntityDefCount)
    {
        nk_label(Nk, "None yet: make them in the Entity Editor.", NK_TEXT_LEFT);
    }

    if(MapMode->SelectedEntity >= 0)
    {
        map_placement *Entity = MapMode->Entities + MapMode->SelectedEntity;
        map_entity_def *Def = MapEntityDefOf(MapMode, Entity);
        char GUID[17];
        EntityGUIDToHex(Entity->GUID, GUID);
        nk_layout_row_dynamic(Nk, 24, 1);
        nk_label(Nk, "", NK_TEXT_LEFT);
        nk_labelf(Nk, NK_TEXT_LEFT, "Selected: %s", Def ? Def->FileName : "(a definition not in data/entities)");
        nk_labelf(Nk, NK_TEXT_LEFT, "Placement GUID %s", GUID);
        nk_layout_row_dynamic(Nk, 28, 2);
        nk_property_float(Nk, "#x", 0.0f, &Entity->P.x, (r32)MapMode->Width, 0.5f, 0.02f);
        nk_property_float(Nk, "#y", 0.0f, &Entity->P.y, (r32)MapMode->Height, 0.5f, 0.02f);
        nk_bool Flip = (Entity->Flip != 0);
        if(nk_checkbox_label(Nk, "Flip", &Flip)) {Entity->Flip = Flip;}
        if(nk_button_label(Nk, "Remove")) {MapMode->RemoveEntityRequested = true;}
        if(Def)
        {
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_labelf(Nk, NK_TEXT_LEFT, "Overrides (%u of at most %d), tick a field:", Entity->OverrideCount,
                      MAP_FILE_MAX_ENTITY_OVERRIDES);
            for(u32 Block = 0; Block < Project->BlockCount; ++Block)
            {
                if(Def->File.Blocks & (1u << Block))
                {
                    nk_layout_row_dynamic(Nk, 22, 1);
                    nk_label(Nk, Project->Blocks[Block].Name, NK_TEXT_LEFT);
                    for(u32 Field = 0; Field < Project->Blocks[Block].FieldCount; ++Field)
                    {
                        Typing |= DrawMapEntityField(MapMode, Nk, Entity, Def, Block, Field);
                    }
                }
            }
        }
    }

    nk_layout_row_dynamic(Nk, 18, 1);
    nk_label(Nk, "", NK_TEXT_LEFT);
    nk_label(Nk, "Palette or list: pick a definition to place.", NK_TEXT_LEFT);
    nk_label(Nk, "Map: left click places it, left drag moves one.", NK_TEXT_LEFT);
    nk_label(Nk, "Hold Ctrl to stand the foot on a cell's centre.", NK_TEXT_LEFT);
    nk_label(Nk, "Right click removes one, Del the selected one.", NK_TEXT_LEFT);
    nk_label(Nk, "Alt+click picks the definition of the one under it.", NK_TEXT_LEFT);
    nk_label(Nk, "Middle drag pans, the wheel zooms. Ctrl+Z / Ctrl+Y.", NK_TEXT_LEFT);

    return(Typing);
}

internal void
DrawMapEditorUI(editor_mode_map *MapMode, ui_state *UIState)
{
    nk_context *Nk = UIState->Nk;

    nk_layout_space_begin(Nk, NK_STATIC, 1000, INT_MAX);
    nk_layout_space_push(Nk, nk_rect(10, 10, 410, 1000));
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "Map Editor Panel", 0))
    {
        nk_layout_row_dynamic(Nk, 30, 2);
        if(nk_button_label(Nk, "Exit")) {MapMode->Exit = true;}
        nk_labelf(Nk, NK_TEXT_LEFT, "Project: %s", MapMode->ProjectName);

        if(MapMode->ExitArmed)
        {
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "Unsaved changes: Exit again to drop them.", NK_TEXT_LEFT);
        }

        // NOTE(pvlso): The map's file: open one of data/maps, or a name for New and Save as
        nk_layout_row_dynamic(Nk, 24, 1);
        nk_label(Nk, "Maps (data/maps):", NK_TEXT_LEFT);
        nk_layout_row_template_begin(Nk, 28);
        nk_layout_row_template_push_dynamic(Nk);
        nk_layout_row_template_push_static(Nk, 100);
        nk_layout_row_template_end(Nk);
        const char *MapPreview = MapMode->MapFileCount ? MapMode->MapFiles[MapMode->MapFileIndex] : "<none>";
        if(nk_combo_begin_label(Nk, MapPreview, nk_vec2(nk_widget_width(Nk), 240)))
        {
            nk_layout_row_dynamic(Nk, 20, 1);
            for(u32 Index = 0; Index < MapMode->MapFileCount; ++Index)
            {
                nk_bool Selected = (Index == MapMode->MapFileIndex);
                if(nk_selectable_label(Nk, MapMode->MapFiles[Index], NK_TEXT_LEFT, &Selected))
                {
                    MapMode->MapFileIndex = Index;
                    nk_combo_close(Nk);
                }
            }
            nk_combo_end(Nk);
        }
        if(nk_button_label(Nk, "Open")) {MapMode->OpenRequested = true;}

        nk_layout_row_dynamic(Nk, 24, 1);
        nk_label(Nk, "Name (for New and Save as):", NK_TEXT_LEFT);
        nk_layout_row_dynamic(Nk, 28, 1);
        nk_flags NameState = nk_edit_string(Nk, NK_EDIT_FIELD, MapMode->NameInput, &MapMode->NameInputLength,
                                            MAP_FILE_SET_NAME_LENGTH - 5, nk_filter_default);
        MapMode->NameInput[MapMode->NameInputLength] = 0;
        b32 TypingMapName = ((NameState & NK_EDIT_ACTIVE) != 0);
        MapMode->TypingLayerName = TypingMapName;
        nk_layout_row_dynamic(Nk, 28, 3);
        nk_property_int(Nk, "#W", 1, &MapMode->NewWidth, MAP_MAX_SIDE, 1, 1.0f);
        nk_property_int(Nk, "#H", 1, &MapMode->NewHeight, MAP_MAX_SIDE, 1, 1.0f);
        if(nk_button_label(Nk, "New map")) {MapMode->NewMapRequested = true;}
        if(MapMode->HasMap)
        {
            nk_layout_row_dynamic(Nk, 28, 2);
            if(nk_button_label(Nk, MapMode->Dirty ? "Save *" : "Save")) {MapMode->SaveRequested = true;}
            if(nk_button_label(Nk, "Save as")) {MapMode->SaveAsRequested = true;}
        }

        if(MapMode->HasMap)
        {
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_labelf(Nk, NK_TEXT_LEFT, "%s%s", MapMode->FileName, MapMode->Dirty ? "  (not saved)" : "");
            nk_labelf(Nk, NK_TEXT_LEFT, "Map %u x %u, cell %d,%d", MapMode->Width, MapMode->Height, MapMode->HoverX,
                      MapMode->HoverY);
            nk_layout_row_dynamic(Nk, 24, 2);
            nk_bool ShowGrid = MapMode->ShowGrid;
            if(nk_checkbox_label(Nk, "Grid", &ShowGrid)) {MapMode->ShowGrid = ShowGrid;}
            nk_bool Dim = MapMode->DimOtherLayers;
            if(nk_checkbox_label(Nk, "Dim other layers", &Dim)) {MapMode->DimOtherLayers = Dim;}

            // NOTE(pvlso): The tool: painting tiles or placing entities
            nk_layout_row_dynamic(Nk, 28, 3);
            nk_bool Tiles = (MapMode->Tool == MapTool_Tiles);
            if(nk_selectable_label(Nk, "Tiles", NK_TEXT_CENTERED, &Tiles) && Tiles) {MapMode->Tool = MapTool_Tiles;}
            nk_bool Entities = (MapMode->Tool == MapTool_Entities);
            if(nk_selectable_label(Nk, "Entities", NK_TEXT_CENTERED, &Entities) && Entities)
            {
                MapMode->Tool = MapTool_Entities;
            }
            nk_bool ShowEntities = MapMode->ShowEntities;
            if(nk_checkbox_label(Nk, "Show entities", &ShowEntities)) {MapMode->ShowEntities = ShowEntities;}

            if(MapMode->Tool == MapTool_Tiles)
            {
                // NOTE(pvlso): Layers: pick one to paint on; Z, name, hidden for the picked one
                nk_layout_row_dynamic(Nk, 24, 1);
                nk_label(Nk, "Layers (painting on the picked one):", NK_TEXT_LEFT);
                for(u32 LayerIndex = 0; LayerIndex < MapMode->LayerCount; ++LayerIndex)
                {
                    map_layer *Layer = MapMode->Layers + LayerIndex;
                    char Label[64];
                    FormatString(sizeof(Label), Label, "Z %d  %s%s", Layer->Z, Layer->Name, Layer->Hidden ? "  (hidden)" : "");
                    nk_bool Picked = ((s32)LayerIndex == MapMode->CurrentLayer);
                    if(nk_selectable_label(Nk, Label, NK_TEXT_LEFT, &Picked)) {MapMode->CurrentLayer = (s32)LayerIndex;}
                }
                if(MapMode->LayerCount)
                {
                    map_layer *Layer = MapMode->Layers + MapMode->CurrentLayer;
                    nk_layout_row_template_begin(Nk, 28);
                    nk_layout_row_template_push_static(Nk, 50);
                    nk_layout_row_template_push_dynamic(Nk);
                    nk_layout_row_template_end(Nk);
                    nk_label(Nk, "Name", NK_TEXT_LEFT);
                    int NameLength = (int)StringLength(Layer->Name);
                    int OldNameLength = NameLength;
                    nk_flags LayerNameState = nk_edit_string(Nk, NK_EDIT_FIELD, Layer->Name, &NameLength, MAP_NAME_LENGTH - 1,
                                                             nk_filter_default);
                    Layer->Name[NameLength] = 0;
                    MapMode->TypingLayerName = ((LayerNameState & NK_EDIT_ACTIVE) != 0) || TypingMapName;
                    if(NameLength != OldNameLength) {MapMode->LayerEdited = true;}

                    nk_layout_row_dynamic(Nk, 28, 2);
                    s32 OldZ = Layer->Z;
                    nk_property_int(Nk, "#Z", -16, &Layer->Z, 16, 1, 0.2f);
                    if(Layer->Z != OldZ) {MapMode->LayerEdited = true;}
                    nk_bool Hidden = Layer->Hidden;
                    if(nk_checkbox_label(Nk, "Hidden", &Hidden)) {Layer->Hidden = Hidden; MapMode->LayerEdited = true;}
                    nk_layout_row_dynamic(Nk, 28, 3);
                    if(nk_button_label(Nk, "Up")) {MapMode->MoveLayerRequested = -1;}
                    if(nk_button_label(Nk, "Down")) {MapMode->MoveLayerRequested = 1;}
                    if(nk_button_label(Nk, "Remove")) {MapMode->RemoveLayerRequested = true;}
                }
                nk_layout_row_template_begin(Nk, 28);
                nk_layout_row_template_push_dynamic(Nk);
                nk_layout_row_template_push_static(Nk, 100);
                nk_layout_row_template_end(Nk);
                nk_edit_string(Nk, NK_EDIT_FIELD, MapMode->NewLayerName, &MapMode->NewLayerNameLength, MAP_NAME_LENGTH - 1,
                               nk_filter_default);
                MapMode->NewLayerName[MapMode->NewLayerNameLength] = 0;
                if(nk_button_label(Nk, "Add layer")) {MapMode->AddLayerRequested = true;}

                // NOTE(pvlso): Resizing: cells added on each side, negative crops
                nk_layout_row_dynamic(Nk, 24, 1);
                nk_label(Nk, "Resize (cells added on each side, negative crops):", NK_TEXT_LEFT);
                nk_layout_row_dynamic(Nk, 28, 4);
                nk_property_int(Nk, "#L", -MAP_MAX_SIDE, &MapMode->ResizeLeft, MAP_MAX_SIDE, 1, 0.5f);
                nk_property_int(Nk, "#R", -MAP_MAX_SIDE, &MapMode->ResizeRight, MAP_MAX_SIDE, 1, 0.5f);
                nk_property_int(Nk, "#T", -MAP_MAX_SIDE, &MapMode->ResizeTop, MAP_MAX_SIDE, 1, 0.5f);
                nk_property_int(Nk, "#B", -MAP_MAX_SIDE, &MapMode->ResizeBottom, MAP_MAX_SIDE, 1, 0.5f);
                nk_layout_row_dynamic(Nk, 28, 1);
                if(nk_button_label(Nk, "Resize")) {MapMode->ResizeRequested = true;}
            }
        }

        if(MapMode->Tool == MapTool_Entities)
        {
            MapMode->TypingLayerName |= DrawMapEntityPanel(MapMode, Nk);
        }
        else
        {
            // NOTE(pvlso): Tile sets: add one from data/tilesets, pick the one in the palette
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_label(Nk, "", NK_TEXT_LEFT);
            nk_label(Nk, "Tile sets (data/tilesets):", NK_TEXT_LEFT);
            nk_layout_row_template_begin(Nk, 28);
            nk_layout_row_template_push_dynamic(Nk);
            nk_layout_row_template_push_static(Nk, 100);
            nk_layout_row_template_end(Nk);
            const char *Preview = MapMode->TilesetFileCount ? MapMode->TilesetFiles[MapMode->TilesetFileIndex] : "<none>";
            if(nk_combo_begin_label(Nk, Preview, nk_vec2(nk_widget_width(Nk), 240)))
            {
                nk_layout_row_dynamic(Nk, 20, 1);
                for(u32 Index = 0; Index < MapMode->TilesetFileCount; ++Index)
                {
                    nk_bool Selected = (Index == MapMode->TilesetFileIndex);
                    if(nk_selectable_label(Nk, MapMode->TilesetFiles[Index], NK_TEXT_LEFT, &Selected))
                    {
                        MapMode->TilesetFileIndex = Index;
                        nk_combo_close(Nk);
                    }
                }
                nk_combo_end(Nk);
            }
            if(nk_button_label(Nk, "Add set")) {MapMode->AddSetRequested = true;}
            nk_layout_row_dynamic(Nk, 22, 1);
            for(u32 SetIndex = 0; SetIndex < MapMode->SetCount; ++SetIndex)
            {
                nk_bool Picked = ((s32)SetIndex == MapMode->CurrentSet);
                if(nk_selectable_label(Nk, MapMode->Sets[SetIndex].FileName, NK_TEXT_LEFT, &Picked))
                {
                    MapMode->CurrentSet = (s32)SetIndex;
                    MapMode->HasBrush = false;
                }
            }

            nk_layout_row_dynamic(Nk, 18, 1);
            nk_label(Nk, "", NK_TEXT_LEFT);
            nk_label(Nk, "Palette: drag over tiles to pick the brush.", NK_TEXT_LEFT);
            nk_label(Nk, "Map: left drag paints, right drag erases.", NK_TEXT_LEFT);
            nk_label(Nk, "Shift+drag fills a rectangle (right: clears it).", NK_TEXT_LEFT);
            nk_label(Nk, "Alt+click picks the tile under the mouse.", NK_TEXT_LEFT);
            nk_label(Nk, "Middle drag pans, the wheel zooms. Ctrl+Z / Ctrl+Y.", NK_TEXT_LEFT);
        }

        nk_group_end(Nk);
    }
    nk_layout_space_end(Nk);
}

internal b32
UpdateAndRenderMapEditor(editor_state *EditorState, transient_state *TranState, engine_input *Input,
                         platform_texture_op_queue *TextureOpQueue)
{
    TIMED_FUNCTION();

    editor_mode_map *MapMode = EditorState->MapMode;
    ui_state *UIState = &EditorState->UIState;
    render_group *RenderGroup = &UIState->RenderGroup;
    object_transform Flat = DefaultFlatTransform();
    v2 MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;

    if(!MapMode->EntityDefsLoaded)
    {
        LoadMapEntityDefs(EditorState, TextureOpQueue, MapMode);
    }
    if(MapMode->Exit && MapMode->Dirty && !MapMode->ExitArmed)
    {
        MapMode->Exit = false;
        MapMode->ExitArmed = true;
    }
    if(MapMode->Exit)
    {
        for(u32 SetIndex = 0; SetIndex < MapMode->SetCount; ++SetIndex)
        {
            ReleaseMapTileset(TextureOpQueue, MapMode->Sets + SetIndex);
        }
        for(u32 DefIndex = 0; DefIndex < MapMode->EntityDefCount; ++DefIndex)
        {
            DeallocateBitmap(TextureOpQueue, &MapMode->EntityDefs[DefIndex].Look);
        }
        if(MapMode->Entities)
        {
            Platform.DeallocateMemory(MapMode->Entities);
        }
        FreeMapLayers(MapMode);
        FreeMapHistory(MapMode);
        PlayTitleScreen(EditorState, TranState);
        return(true);
    }

    // NOTE(pvlso): The panel changed the selected placement (position, flip, overrides) last frame
    if((MapMode->SelectedEntity >= 0) && (MapMode->SelectedEntity == MapMode->LastSelectedIndex) &&
       !MapMode->DraggingEntity &&
       !StructuresAreEqual(MapMode->Entities[MapMode->SelectedEntity], MapMode->LastSelected))
    {
        MapMode->EntityEdited = true;
    }

    if(MapMode->NewMapRequested)
    {
        MapMode->NewMapRequested = false;
        char FileName[MAP_FILE_SET_NAME_LENGTH];
        if(MapFileNameFrom(MapMode->NameInput, FileName, sizeof(FileName)))
        {
            if(Platform.FileExists(FileName, PlatformFileType_Map))
            {
                LOG_ERROR("Map %s already exists: Open it, or pick another name", FileName);
            }
            else
            {
                NewMap(MapMode, MapMode->NewWidth, MapMode->NewHeight);
                FormatString(sizeof(MapMode->FileName), MapMode->FileName, "%s", FileName);
                MapMode->Dirty = true;
            }
        }
    }
    if(MapMode->OpenRequested)
    {
        MapMode->OpenRequested = false;
        if(MapMode->MapFileCount)
        {
            OpenMap(TextureOpQueue, MapMode, MapMode->MapFiles[MapMode->MapFileIndex], &TranState->TranArena);
        }
    }
    if((MapMode->SaveRequested || MapMode->SaveAsRequested) && MapMode->HasMap)
    {
        char FileName[MAP_FILE_SET_NAME_LENGTH];
        FormatString(sizeof(FileName), FileName, "%s", MapMode->FileName);
        b32 HaveName = MapMode->SaveRequested || MapFileNameFrom(MapMode->NameInput, FileName, sizeof(FileName));
        if(HaveName && SaveMap(MapMode, FileName, &TranState->TranArena))
        {
            ListMapFiles(MapMode, &EditorState->ModeArena);
        }
        MapMode->SaveRequested = MapMode->SaveAsRequested = false;
    }
    b32 Changed = false;
    if(MapMode->AddLayerRequested && MapMode->HasMap)
    {
        Changed = true;
        MapMode->AddLayerRequested = false;
        s32 Z = MapMode->LayerCount ? MapMode->Layers[MapMode->CurrentLayer].Z : 0;
        AddMapLayer(MapMode, MapMode->NewLayerNameLength ? MapMode->NewLayerName : (char *)"Layer", Z);
        MapMode->NewLayerNameLength = 0;
        MapMode->NewLayerName[0] = 0;
    }
    if(MapMode->RemoveLayerRequested && MapMode->LayerCount)
    {
        MapMode->RemoveLayerRequested = false;
        Changed = true;
        Platform.DeallocateMemory(MapMode->Layers[MapMode->CurrentLayer].Cells);
        for(u32 Index = MapMode->CurrentLayer + 1; Index < MapMode->LayerCount; ++Index)
        {
            MapMode->Layers[Index - 1] = MapMode->Layers[Index];
        }
        --MapMode->LayerCount;
        MapMode->CurrentLayer = Maximum(0, Minimum(MapMode->CurrentLayer, (s32)MapMode->LayerCount - 1));
    }
    if(MapMode->MoveLayerRequested && MapMode->LayerCount)
    {
        s32 To = MapMode->CurrentLayer + MapMode->MoveLayerRequested;
        if((To >= 0) && (To < (s32)MapMode->LayerCount))
        {
            map_layer Swap = MapMode->Layers[To];
            MapMode->Layers[To] = MapMode->Layers[MapMode->CurrentLayer];
            MapMode->Layers[MapMode->CurrentLayer] = Swap;
            MapMode->CurrentLayer = To;
            Changed = true;
        }
        MapMode->MoveLayerRequested = 0;
    }
    if(MapMode->ResizeRequested && MapMode->HasMap)
    {
        MapMode->ResizeRequested = false;
        ResizeMap(MapMode, MapMode->ResizeLeft, MapMode->ResizeRight, MapMode->ResizeTop, MapMode->ResizeBottom);
        MapMode->ResizeLeft = MapMode->ResizeRight = MapMode->ResizeTop = MapMode->ResizeBottom = 0;
        Changed = true;
    }
    if(MapMode->RemoveEntityRequested)
    {
        MapMode->RemoveEntityRequested = false;
        if(MapMode->SelectedEntity >= 0)
        {
            RemoveMapEntity(MapMode, MapMode->SelectedEntity);
            Changed = true;
        }
    }
    if(MapMode->AddSetRequested)
    {
        MapMode->AddSetRequested = false;
        if(MapMode->TilesetFileCount)
        {
            s32 Found = MapSetFor(TextureOpQueue, MapMode, MapMode->TilesetFiles[MapMode->TilesetFileIndex]);
            if(Found >= 0)
            {
                MapMode->CurrentSet = Found;
                MapMode->HasBrush = false;
            }
        }
    }

    // NOTE(pvlso): The canvas right of the panel: the map, then the palette
    nk_context *Nk = UIState->Nk;
    r32 Wheel = Nk->input.mouse.scroll_delta.y;
    rectangle2 MapRect = RectMinMax(V2(-530.0f, -530.0f), V2(330.0f, 470.0f));
    rectangle2 PaletteRect = RectMinMax(V2(340.0f, -530.0f), V2(950.0f, 470.0f));
    if(MapMode->HasMap)
    {
        Changed |= UpdateAndRenderMapView(MapMode, Input, RenderGroup, &Flat, MouseP, MapRect, Wheel);

        // NOTE(pvlso): A layer's name, Z or hidden flag goes in the history once the mouse lets go of the panel
        b32 MouseDown = (Input->MouseButtons[PlatformMouseButton_Left].EndedDown ||
                         Input->MouseButtons[PlatformMouseButton_Right].EndedDown);
        if(MapMode->LayerEdited && !MouseDown && !MapMode->TypingLayerName)
        {
            MapMode->LayerEdited = false;
            Changed = true;
        }
        if(MapMode->EntityEdited && !MouseDown && !MapMode->TypingLayerName)
        {
            MapMode->EntityEdited = false;
            Changed = true;
        }

        b32 Busy = MapMode->Painting || MapMode->Erasing || MapMode->RectActive || MapMode->DraggingEntity;
        if(!MapMode->TypingLayerName && !Busy && (MapMode->Tool == MapTool_Entities) &&
           (MapMode->SelectedEntity >= 0) && nk_input_is_key_pressed(&Nk->input, NK_KEY_DEL))
        {
            RemoveMapEntity(MapMode, MapMode->SelectedEntity);
            Changed = true;
        }
        if(Changed)
        {
            RecordMapState(MapMode);
            MapMode->Dirty = true;
        }

        if(!MapMode->TypingLayerName && !Busy)
        {
            if(nk_input_is_key_pressed(&Nk->input, NK_KEY_TEXT_UNDO) && (MapMode->HistoryAt > 0))
            {
                RestoreMapState(MapMode, MapMode->HistoryAt - 1);
                MapMode->Dirty = true;
            }
            else if(nk_input_is_key_pressed(&Nk->input, NK_KEY_TEXT_REDO) &&
                    (MapMode->HistoryAt + 1 < MapMode->HistoryCount))
            {
                RestoreMapState(MapMode, MapMode->HistoryAt + 1);
                MapMode->Dirty = true;
            }
        }
    }
    else
    {
        PushRect(RenderGroup, &Flat, MapRect, 4.0f, V4(0.12f, 0.12f, 0.14f, 1.0f));
    }
    if(MapMode->Tool == MapTool_Entities)
    {
        UpdateAndRenderEntityPalette(MapMode, Input, RenderGroup, &Flat, MouseP, PaletteRect, Wheel);
    }
    else
    {
        UpdateAndRenderPalette(MapMode, Input, RenderGroup, &Flat, MouseP, PaletteRect, Wheel);
    }

    MapMode->LastSelectedIndex = MapMode->SelectedEntity;
    if(MapMode->SelectedEntity >= 0)
    {
        MapMode->LastSelected = MapMode->Entities[MapMode->SelectedEntity];
    }
    DrawMapEditorUI(MapMode, UIState);

    return(false);
}

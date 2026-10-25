/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

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

internal void
AddMapLayer(editor_mode_map *MapMode, char *Name, s32 Z)
{
    if(MapMode->LayerCount < MAP_MAX_LAYERS)
    {
        map_layer *Layer = MapMode->Layers + MapMode->LayerCount++;
        *Layer = {};
        FormatString(sizeof(Layer->Name), Layer->Name, "%s", Name);
        Layer->Z = Z;
        Layer->Cells = PushArray(&MapMode->MapArena, MapMode->Width*MapMode->Height, map_cell);
        ZeroSize(MapMode->Width*MapMode->Height*sizeof(map_cell), Layer->Cells);
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
    Clear(&MapMode->MapArena);
    MapMode->Width = (u32)ClampCell(1, (s32)Width, MAP_MAX_SIDE);
    MapMode->Height = (u32)ClampCell(1, (s32)Height, MAP_MAX_SIDE);
    MapMode->LayerCount = 0;
    AddMapLayer(MapMode, "Ground", 0);
    MapMode->HasMap = true;
    MapMode->Pan = V2(0, 0);
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

internal void
UpdateAndRenderMapView(editor_mode_map *MapMode, engine_input *Input, render_group *RenderGroup, object_transform *Flat,
                       v2 MouseP, rectangle2 Rect, r32 Wheel)
{
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
    if(OnMap && CanPaint && WasPressed(Left))
    {
        MapMode->Painting = true;
        MapMode->PaintFromX = MapMode->LastStampX = CellX;
        MapMode->PaintFromY = MapMode->LastStampY = CellY;
        StampBrush(MapMode, CellX, CellY);
    }
    if(MapMode->Painting)
    {
        if(!Left.EndedDown)
        {
            MapMode->Painting = false;
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
    if(OnMap && Right.EndedDown && MapMode->LayerCount)
    {
        MapMode->Layers[MapMode->CurrentLayer].Cells[CellY*MapMode->Width + CellX] = {};
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
    if(OnMap)
    {
        s32 W = MapMode->HasBrush ? (AbsoluteValue(MapMode->BrushX1 - MapMode->BrushX0) + 1) : 1;
        s32 H = MapMode->HasBrush ? (AbsoluteValue(MapMode->BrushY1 - MapMode->BrushY0) + 1) : 1;
        v2 Min = View.TopLeft + V2((r32)CellX*View.Cell, -(r32)(CellY + H)*View.Cell);
        v2 Max = View.TopLeft + V2((r32)(CellX + W)*View.Cell, -(r32)CellY*View.Cell);
        PushRectOutline(RenderGroup, Flat, RectMinMax(Min, Max), 8.0f, V4(1, 0.85f, 0.2f, 0.9f), 1.0f);
    }
    RenderGroup->CurrentClipRectIndex = SavedClip;
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

        // NOTE(pvlso): A new map
        nk_layout_row_dynamic(Nk, 24, 1);
        nk_label(Nk, "New map (in memory until island files come):", NK_TEXT_LEFT);
        nk_layout_row_dynamic(Nk, 28, 3);
        nk_property_int(Nk, "#W", 1, &MapMode->NewWidth, MAP_MAX_SIDE, 1, 1.0f);
        nk_property_int(Nk, "#H", 1, &MapMode->NewHeight, MAP_MAX_SIDE, 1, 1.0f);
        if(nk_button_label(Nk, "New map")) {MapMode->NewMapRequested = true;}

        if(MapMode->HasMap)
        {
            nk_layout_row_dynamic(Nk, 24, 1);
            nk_labelf(Nk, NK_TEXT_LEFT, "Map %u x %u, cell %d,%d", MapMode->Width, MapMode->Height, MapMode->HoverX,
                      MapMode->HoverY);
            nk_layout_row_dynamic(Nk, 24, 2);
            nk_bool ShowGrid = MapMode->ShowGrid;
            if(nk_checkbox_label(Nk, "Grid", &ShowGrid)) {MapMode->ShowGrid = ShowGrid;}
            nk_bool Dim = MapMode->DimOtherLayers;
            if(nk_checkbox_label(Nk, "Dim other layers", &Dim)) {MapMode->DimOtherLayers = Dim;}

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
                nk_edit_string(Nk, NK_EDIT_FIELD, Layer->Name, &NameLength, MAP_NAME_LENGTH - 1, nk_filter_default);
                Layer->Name[NameLength] = 0;

                nk_layout_row_dynamic(Nk, 28, 2);
                nk_property_int(Nk, "#Z", -16, &Layer->Z, 16, 1, 0.2f);
                nk_bool Hidden = Layer->Hidden;
                if(nk_checkbox_label(Nk, "Hidden", &Hidden)) {Layer->Hidden = Hidden;}
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
        }

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
        nk_label(Nk, "Middle drag pans, the wheel zooms.", NK_TEXT_LEFT);

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

    if(MapMode->Exit)
    {
        for(u32 SetIndex = 0; SetIndex < MapMode->SetCount; ++SetIndex)
        {
            ReleaseMapTileset(TextureOpQueue, MapMode->Sets + SetIndex);
        }
        Clear(&MapMode->MapArena);
        PlayTitleScreen(EditorState, TranState);
        return(true);
    }

    if(MapMode->NewMapRequested)
    {
        MapMode->NewMapRequested = false;
        NewMap(MapMode, MapMode->NewWidth, MapMode->NewHeight);
    }
    if(MapMode->AddLayerRequested && MapMode->HasMap)
    {
        MapMode->AddLayerRequested = false;
        s32 Z = MapMode->LayerCount ? MapMode->Layers[MapMode->CurrentLayer].Z : 0;
        AddMapLayer(MapMode, MapMode->NewLayerNameLength ? MapMode->NewLayerName : (char *)"Layer", Z);
        MapMode->NewLayerNameLength = 0;
        MapMode->NewLayerName[0] = 0;
    }
    if(MapMode->RemoveLayerRequested && MapMode->LayerCount)
    {
        MapMode->RemoveLayerRequested = false;
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
        }
        MapMode->MoveLayerRequested = 0;
    }
    if(MapMode->AddSetRequested)
    {
        MapMode->AddSetRequested = false;
        if(MapMode->TilesetFileCount)
        {
            char *FileName = MapMode->TilesetFiles[MapMode->TilesetFileIndex];
            s32 Found = -1;
            for(u32 SetIndex = 0; SetIndex < MapMode->SetCount; ++SetIndex)
            {
                if(StringsAreEqual(MapMode->Sets[SetIndex].FileName, FileName)) {Found = (s32)SetIndex;}
            }
            if((Found < 0) && (MapMode->SetCount < MAP_MAX_TILESETS))
            {
                Found = (s32)MapMode->SetCount++;
                LoadMapTileset(TextureOpQueue, MapMode->Sets + Found, FileName);
            }
            else if(Found < 0)
            {
                LOG_ERROR("A map uses at most %d tile sets", MAP_MAX_TILESETS);
            }
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
        UpdateAndRenderMapView(MapMode, Input, RenderGroup, &Flat, MouseP, MapRect, Wheel);
    }
    else
    {
        PushRect(RenderGroup, &Flat, MapRect, 4.0f, V4(0.12f, 0.12f, 0.14f, 1.0f));
    }
    UpdateAndRenderPalette(MapMode, Input, RenderGroup, &Flat, MouseP, PaletteRect, Wheel);

    DrawMapEditorUI(MapMode, UIState);

    return(false);
}

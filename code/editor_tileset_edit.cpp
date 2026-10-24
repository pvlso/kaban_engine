/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): Editing a curated tile set in the assets mode (map editor plan, phase 1b). The canvas is split: on the
  left a sheet, any image under data/, panned with the middle button and zoomed with the wheel, cells selected by
  dragging; on the right the set's own grid, the same way. A click in the grid places the selection there with its top
  left on the cell, keeping its shape; a right click removes a tile; a click on a tile picks it for the panel (name,
  flags, frames, speed), Shift+click on an empty cell moves the picked tile there. Every change is a state in the
  history: Ctrl+Z, Ctrl+Y. Save writes the text file.
*/

internal void AllocateBitmap(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Bitmap);
internal void DeallocateBitmap(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Bitmap);

inline s32
ClampCell(s32 Min, s32 Value, s32 Max)
{
    s32 Result = (Value < Min) ? Min : ((Value > Max) ? Max : Value);
    return(Result);
}

struct tileset_snapshot
{
    u32 TileCount;
    u32 SheetCount;
    char Sheets[TILESET_MAX_SHEETS][TILESET_SHEET_PATH_LENGTH];
    tileset_file_tile Tiles[1];
};

// NOTE(pvlso): The view's own picture of a tile: its texture released, its pixels stay in the arena until it's cleared
internal void
ReleaseTileTexture(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Tile)
{
    if(Tile->TextureHandle)
    {
        texture_op Op = {};
        Op.IsAllocate = false;
        Op.Deallocate.Handle = Tile->TextureHandle;
        AddOp(TextureOpQueue, &Op);
    }
    *Tile = {};
}

internal void
CutTileForView(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode, u32 TileIndex)
{
    tileset_file *File = &TilesetMode->File;
    loaded_bitmap *Tile = TilesetMode->Tiles + TileIndex;
    ReleaseTileTexture(TextureOpQueue, Tile);
    tileset_file_cell Cell = File->Tiles[TileIndex].Frames[0];
    *Tile = CutTilesetCell(TilesetMode->Sheets + Cell.Sheet, File->TileSize, Cell.X, Cell.Y, &TilesetMode->Arena);
    if(Tile->Memory)
    {
        AllocateBitmap(TextureOpQueue, Tile);
    }
}

internal void
RecountTilesetGrid(tileset_file *File)
{
    File->GridWidth = File->GridHeight = 0;
    for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
    {
        File->GridWidth = Maximum(File->GridWidth, (u32)File->Tiles[TileIndex].GridX + 1);
        File->GridHeight = Maximum(File->GridHeight, (u32)File->Tiles[TileIndex].GridY + 1);
    }
}

internal void
RecordTilesetState(tileset_mode *TilesetMode)
{
    // NOTE(pvlso): A change after undoing drops what could have been redone
    while(TilesetMode->HistoryCount > TilesetMode->HistoryAt + 1)
    {
        Platform.DeallocateMemory(TilesetMode->History[--TilesetMode->HistoryCount]);
    }
    if(TilesetMode->HistoryCount == ArrayCount(TilesetMode->History))
    {
        Platform.DeallocateMemory(TilesetMode->History[0]);
        for(u32 Index = 1; Index < TilesetMode->HistoryCount; ++Index)
        {
            TilesetMode->History[Index - 1] = TilesetMode->History[Index];
        }
        --TilesetMode->HistoryCount;
    }

    tileset_file *File = &TilesetMode->File;
    umm Size = sizeof(tileset_snapshot) + (umm)Maximum(1, File->TileCount)*sizeof(tileset_file_tile);
    tileset_snapshot *Snapshot = (tileset_snapshot *)Platform.AllocateMemory(Size);
    Snapshot->TileCount = File->TileCount;
    Snapshot->SheetCount = File->SheetCount;
    Copy(sizeof(Snapshot->Sheets), File->Sheets, Snapshot->Sheets);
    Copy(File->TileCount*sizeof(tileset_file_tile), File->Tiles, Snapshot->Tiles);
    TilesetMode->History[TilesetMode->HistoryCount] = Snapshot;
    TilesetMode->HistoryAt = TilesetMode->HistoryCount++;
}

internal void
FreeTilesetHistory(tileset_mode *TilesetMode)
{
    for(u32 Index = 0; Index < TilesetMode->HistoryCount; ++Index)
    {
        Platform.DeallocateMemory(TilesetMode->History[Index]);
    }
    TilesetMode->HistoryCount = TilesetMode->HistoryAt = 0;
}

// NOTE(pvlso): Loads a sheet of the set (into the arena) when it isn't yet, to cut tiles from
internal void
LoadSetSheet(tileset_mode *TilesetMode, u32 SheetIndex)
{
    if(!TilesetMode->Sheets[SheetIndex].Memory)
    {
        TilesetMode->Sheets[SheetIndex] = LoadImageFile(TilesetMode->File.Sheets[SheetIndex], PlatformFileType_DataRoot,
                                                        &TilesetMode->Arena);
    }
}

internal void
RestoreTilesetState(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode, u32 HistoryIndex)
{
    tileset_snapshot *Snapshot = TilesetMode->History[HistoryIndex];
    tileset_file *File = &TilesetMode->File;
    for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
    {
        ReleaseTileTexture(TextureOpQueue, TilesetMode->Tiles + TileIndex);
    }
    File->TileCount = Snapshot->TileCount;
    File->SheetCount = Snapshot->SheetCount;
    Copy(sizeof(Snapshot->Sheets), Snapshot->Sheets, File->Sheets);
    Copy(File->TileCount*sizeof(tileset_file_tile), Snapshot->Tiles, File->Tiles);
    for(u32 SheetIndex = 0; SheetIndex < File->SheetCount; ++SheetIndex)
    {
        LoadSetSheet(TilesetMode, SheetIndex);
    }
    for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
    {
        CutTileForView(TextureOpQueue, TilesetMode, TileIndex);
    }
    RecountTilesetGrid(File);
    TilesetMode->HistoryAt = HistoryIndex;
    TilesetMode->SelectedTile = -1;
    TilesetMode->Dirty = true;
}

internal s32
FindTileAt(tileset_file *File, s32 GridX, s32 GridY)
{
    s32 Result = -1;
    for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
    {
        if((File->Tiles[TileIndex].GridX == GridX) && (File->Tiles[TileIndex].GridY == GridY)) {Result = (s32)TileIndex;}
    }

    return(Result);
}

// NOTE(pvlso): The index of the open sheet in the set's sheets, added when it isn't there; -1 when they're full
internal s32
SheetOfOpenImage(tileset_mode *TilesetMode)
{
    tileset_file *File = &TilesetMode->File;
    s32 Result = -1;
    for(u32 SheetIndex = 0; SheetIndex < File->SheetCount; ++SheetIndex)
    {
        if(StringsAreEqual(File->Sheets[SheetIndex], TilesetMode->OpenSheet)) {Result = (s32)SheetIndex;}
    }
    if((Result < 0) && (File->SheetCount < TILESET_MAX_SHEETS))
    {
        Result = (s32)File->SheetCount++;
        FormatString(TILESET_SHEET_PATH_LENGTH, File->Sheets[Result], "%s", TilesetMode->OpenSheet);
        LoadSetSheet(TilesetMode, Result);
    }
    else if(Result < 0)
    {
        LOG_ERROR("A tile set takes tiles from at most %d sheets", TILESET_MAX_SHEETS);
    }

    return(Result);
}

internal void
RemoveTile(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode, u32 TileIndex)
{
    tileset_file *File = &TilesetMode->File;
    ReleaseTileTexture(TextureOpQueue, TilesetMode->Tiles + TileIndex);
    // NOTE(pvlso): The rest keep their order, it's the tiles' numbering in the KEA
    for(u32 Index = TileIndex + 1; Index < File->TileCount; ++Index)
    {
        File->Tiles[Index - 1] = File->Tiles[Index];
        TilesetMode->Tiles[Index - 1] = TilesetMode->Tiles[Index];
    }
    --File->TileCount;
    TilesetMode->Tiles[File->TileCount] = {};
    if(TilesetMode->SelectedTile == (s32)TileIndex) {TilesetMode->SelectedTile = -1;}
    else if(TilesetMode->SelectedTile > (s32)TileIndex) {--TilesetMode->SelectedTile;}
}

// NOTE(pvlso): The selection on the sheet placed with its top left on (GridX, GridY), keeping its shape; tiles already
// there are replaced
internal void
PlaceSelection(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode, s32 GridX, s32 GridY)
{
    tileset_file *File = &TilesetMode->File;
    s32 Sheet = SheetOfOpenImage(TilesetMode);
    if(Sheet >= 0)
    {
        s32 X0 = Minimum(TilesetMode->SelectionX0, TilesetMode->SelectionX1);
        s32 X1 = Maximum(TilesetMode->SelectionX0, TilesetMode->SelectionX1);
        s32 Y0 = Minimum(TilesetMode->SelectionY0, TilesetMode->SelectionY1);
        s32 Y1 = Maximum(TilesetMode->SelectionY0, TilesetMode->SelectionY1);
        for(s32 Y = Y0; Y <= Y1; ++Y)
        {
            for(s32 X = X0; X <= X1; ++X)
            {
                s32 TileIndex = FindTileAt(File, GridX + X - X0, GridY + Y - Y0);
                if(TileIndex < 0)
                {
                    if(File->TileCount >= TILESET_MAX_TILES)
                    {
                        LOG_ERROR("A tile set holds at most %d tiles", TILESET_MAX_TILES);
                        continue;
                    }
                    TileIndex = (s32)File->TileCount++;
                }
                tileset_file_tile *Tile = File->Tiles + TileIndex;
                *Tile = {};
                Tile->GridX = (u16)(GridX + X - X0);
                Tile->GridY = (u16)(GridY + Y - Y0);
                Tile->Frames[0].Sheet = (u16)Sheet;
                Tile->Frames[0].X = (u16)X;
                Tile->Frames[0].Y = (u16)Y;
                Tile->FrameCount = 1;
                CutTileForView(TextureOpQueue, TilesetMode, TileIndex);
            }
        }
    }
}

// NOTE(pvlso): The picked tile's frames become the selection's cells, left to right then down; one cell means it isn't
// animated
internal void
SetFramesFromSelection(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode)
{
    s32 Sheet = SheetOfOpenImage(TilesetMode);
    if((Sheet >= 0) && (TilesetMode->SelectedTile >= 0))
    {
        tileset_file_tile *Tile = TilesetMode->File.Tiles + TilesetMode->SelectedTile;
        s32 X0 = Minimum(TilesetMode->SelectionX0, TilesetMode->SelectionX1);
        s32 X1 = Maximum(TilesetMode->SelectionX0, TilesetMode->SelectionX1);
        s32 Y0 = Minimum(TilesetMode->SelectionY0, TilesetMode->SelectionY1);
        s32 Y1 = Maximum(TilesetMode->SelectionY0, TilesetMode->SelectionY1);
        Tile->FrameCount = 0;
        for(s32 Y = Y0; Y <= Y1; ++Y)
        {
            for(s32 X = X0; (X <= X1) && (Tile->FrameCount < TILESET_MAX_TILE_FRAMES); ++X)
            {
                tileset_file_cell *Frame = Tile->Frames + Tile->FrameCount++;
                Frame->Sheet = (u16)Sheet;
                Frame->X = (u16)X;
                Frame->Y = (u16)Y;
            }
        }
        if((Tile->FrameCount > 1) && (Tile->FramesPerSecond <= 0.0f)) {Tile->FramesPerSecond = 8.0f;}
        CutTileForView(TextureOpQueue, TilesetMode, TilesetMode->SelectedTile);
    }
}

internal void
OpenSheetImage(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode, char *Path)
{
    DeallocateBitmap(TextureOpQueue, &TilesetMode->SheetBitmap);
    TilesetMode->SheetBitmap = {};
    FormatString(TILESET_SHEET_PATH_LENGTH, TilesetMode->OpenSheet, "%s", Path);
    TilesetMode->SheetBitmap = LoadImageFile(Path, PlatformFileType_DataRoot, 0);
    if(TilesetMode->SheetBitmap.Memory)
    {
        AllocateBitmap(TextureOpQueue, &TilesetMode->SheetBitmap);
    }
    else
    {
        LOG_ERROR("Can't read the sheet %s", Path);
    }
    TilesetMode->SheetPan = V2(0, 0);
    TilesetMode->HasSelection = false;
    TilesetMode->Selecting = false;
}

// NOTE(pvlso): What the view of a grid (the sheet's or the set's) needs: its rectangle on the canvas, where cell
// (0, 0)'s top left is, the size of a cell
struct tileset_grid_view
{
    rectangle2 Rect;
    v2 TopLeft;
    r32 Cell;
};

inline tileset_grid_view
GridView(rectangle2 Rect, v2 Pan, r32 Cell)
{
    tileset_grid_view Result;
    Result.Rect = Rect;
    Result.TopLeft = V2(Rect.Min.x, Rect.Max.y) + V2(Pan.x, -Pan.y);
    Result.Cell = Cell;

    return(Result);
}

inline v2
CellCenter(tileset_grid_view *View, s32 X, s32 Y)
{
    v2 Result = View->TopLeft + V2(((r32)X + 0.5f)*View->Cell, -((r32)Y + 0.5f)*View->Cell);
    return(Result);
}

inline void
CellUnder(tileset_grid_view *View, v2 P, s32 *X, s32 *Y)
{
    *X = FloorReal32ToInt32((P.x - View->TopLeft.x) / View->Cell);
    *Y = FloorReal32ToInt32((View->TopLeft.y - P.y) / View->Cell);
}

// NOTE(pvlso): The wheel zooms around the mouse: the point under it stays put
internal void
ZoomGridView(r32 *Zoom, v2 *Pan, rectangle2 Rect, v2 MouseP, r32 Wheel, r32 MinZoom, r32 MaxZoom)
{
    if(Wheel != 0.0f)
    {
        r32 Old = *Zoom;
        r32 New = Clamp(MinZoom, Old*((Wheel > 0.0f) ? 1.25f : 0.8f), MaxZoom);
        v2 FromTopLeft = V2(MouseP.x - Rect.Min.x, Rect.Max.y - MouseP.y) - *Pan;
        *Pan += FromTopLeft*(1.0f - New / Old);
        *Zoom = New;
    }
}

internal void
DrawGridLines(render_group *RenderGroup, object_transform *Flat, tileset_grid_view *View, r32 Z, v4 Color)
{
    if(View->Cell >= 6.0f)
    {
        r32 StartX = View->TopLeft.x + View->Cell*(r32)CeilReal32ToInt32((View->Rect.Min.x - View->TopLeft.x) / View->Cell);
        for(r32 X = StartX; X <= View->Rect.Max.x; X += View->Cell)
        {
            PushLine(RenderGroup, Flat, V3(X, View->Rect.Min.y, Z), V3(X, View->Rect.Max.y, Z), Color);
        }
        r32 StartY = View->TopLeft.y - View->Cell*(r32)CeilReal32ToInt32((View->TopLeft.y - View->Rect.Max.y) / View->Cell);
        for(r32 Y = StartY; Y >= View->Rect.Min.y; Y -= View->Cell)
        {
            PushLine(RenderGroup, Flat, V3(View->Rect.Min.x, Y, Z), V3(View->Rect.Max.x, Y, Z), Color);
        }
    }
}

internal void
ClearTilesetEditing(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode)
{
    DeallocateBitmap(TextureOpQueue, &TilesetMode->SheetBitmap);
    TilesetMode->SheetBitmap = {};
    FreeTilesetHistory(TilesetMode);
}

// NOTE(pvlso): The sheet on the left: middle drag pans, the wheel zooms, a left drag selects cells, a right click
// clears the selection
internal void
UpdateAndRenderSheetView(tileset_mode *TilesetMode, engine_input *Input, render_group *RenderGroup,
                         object_transform *Flat, v2 MouseP, rectangle2 Rect, r32 Wheel)
{
    tileset_file *File = &TilesetMode->File;
    loaded_bitmap *Sheet = &TilesetMode->SheetBitmap;
    b32 Inside = IsInRectangle(Rect, MouseP);
    engine_button_state Left = Input->MouseButtons[PlatformMouseButton_Left];
    engine_button_state Middle = Input->MouseButtons[PlatformMouseButton_Middle];

    if(Inside)
    {
        ZoomGridView(&TilesetMode->SheetZoom, &TilesetMode->SheetPan, Rect, MouseP, Wheel, 4.0f, 96.0f);
        if(WasPressed(Middle)) {TilesetMode->Panning = 1; TilesetMode->PanFrom = MouseP;}
    }
    if((TilesetMode->Panning == 1) && Middle.EndedDown)
    {
        TilesetMode->SheetPan += V2(MouseP.x - TilesetMode->PanFrom.x, TilesetMode->PanFrom.y - MouseP.y);
        TilesetMode->PanFrom = MouseP;
    }

    tileset_grid_view View = GridView(Rect, TilesetMode->SheetPan, TilesetMode->SheetZoom);
    s32 CellX, CellY;
    CellUnder(&View, MouseP, &CellX, &CellY);
    s32 CellsX = Sheet->Memory ? (Sheet->Width / (s32)File->TileSize) : 0;
    s32 CellsY = Sheet->Memory ? (Sheet->Height / (s32)File->TileSize) : 0;
    CellX = ClampCell(0, CellX, Maximum(0, CellsX - 1));
    CellY = ClampCell(0, CellY, Maximum(0, CellsY - 1));
    if(Inside && Sheet->Memory)
    {
        if(WasPressed(Left))
        {
            TilesetMode->Selecting = true;
            TilesetMode->HasSelection = true;
            TilesetMode->SelectionX0 = TilesetMode->SelectionX1 = CellX;
            TilesetMode->SelectionY0 = TilesetMode->SelectionY1 = CellY;
        }
        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
        {
            TilesetMode->HasSelection = false;
        }
    }
    if(TilesetMode->Selecting)
    {
        TilesetMode->SelectionX1 = CellX;
        TilesetMode->SelectionY1 = CellY;
        if(!Left.EndedDown) {TilesetMode->Selecting = false;}
    }

    u32 SavedClip = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
    PushRect(RenderGroup, Flat, Rect, 5.0f, V4(0.18f, 0.18f, 0.2f, 1.0f));
    if(Sheet->Memory)
    {
        r32 Scale = View.Cell / (r32)File->TileSize;
        r32 Height = Scale*(r32)Sheet->Height;
        v2 Center = View.TopLeft + V2(0.5f*Scale*(r32)Sheet->Width, -0.5f*Height);
        PushBitmap(RenderGroup, Flat, Sheet, Height, V3(Center, 6.0f));
        DrawGridLines(RenderGroup, Flat, &View, 7.0f, V4(1, 1, 1, 0.12f));
        if(TilesetMode->HasSelection)
        {
            s32 X0 = Minimum(TilesetMode->SelectionX0, TilesetMode->SelectionX1);
            s32 X1 = Maximum(TilesetMode->SelectionX0, TilesetMode->SelectionX1);
            s32 Y0 = Minimum(TilesetMode->SelectionY0, TilesetMode->SelectionY1);
            s32 Y1 = Maximum(TilesetMode->SelectionY0, TilesetMode->SelectionY1);
            v2 Min = View.TopLeft + V2((r32)X0*View.Cell, -(r32)(Y1 + 1)*View.Cell);
            v2 Max = View.TopLeft + V2((r32)(X1 + 1)*View.Cell, -(r32)Y0*View.Cell);
            PushRectOutline(RenderGroup, Flat, RectMinMax(Min, Max), 8.0f, V4(1, 0.85f, 0.2f, 1), 2.0f);
        }
        if(Inside)
        {
            PushRectOutline(RenderGroup, Flat, RectCenterDim(CellCenter(&View, CellX, CellY), V2(View.Cell, View.Cell)),
                            8.0f, V4(1, 1, 1, 0.6f), 1.0f);
        }
    }
    RenderGroup->CurrentClipRectIndex = SavedClip;
}

// NOTE(pvlso): The set's grid on the right: middle drag pans, the wheel zooms; a left click places the selection, or
// picks the tile there (Shift: moves the picked tile there), a right click removes the tile there. True when the set
// changed.
internal b32
UpdateAndRenderSetView(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode, engine_input *Input,
                       render_group *RenderGroup, object_transform *Flat, v2 MouseP, rectangle2 Rect, r32 Wheel)
{
    b32 Changed = false;
    tileset_file *File = &TilesetMode->File;
    b32 Inside = IsInRectangle(Rect, MouseP);
    engine_button_state Middle = Input->MouseButtons[PlatformMouseButton_Middle];

    if(Inside)
    {
        ZoomGridView(&TilesetMode->SetZoom, &TilesetMode->SetPan, Rect, MouseP, Wheel, 8.0f, 96.0f);
        if(WasPressed(Middle)) {TilesetMode->Panning = 2; TilesetMode->PanFrom = MouseP;}
    }
    if((TilesetMode->Panning == 2) && Middle.EndedDown)
    {
        TilesetMode->SetPan += V2(MouseP.x - TilesetMode->PanFrom.x, TilesetMode->PanFrom.y - MouseP.y);
        TilesetMode->PanFrom = MouseP;
    }
    if(!Middle.EndedDown) {TilesetMode->Panning = 0;}

    tileset_grid_view View = GridView(Rect, TilesetMode->SetPan, TilesetMode->SetZoom);
    s32 CellX, CellY;
    CellUnder(&View, MouseP, &CellX, &CellY);
    b32 OnGrid = Inside && (CellX >= 0) && (CellY >= 0);
    if(OnGrid && WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
    {
        s32 There = FindTileAt(File, CellX, CellY);
        if(TilesetMode->HasSelection && TilesetMode->SheetBitmap.Memory && !Input->ShiftDown)
        {
            PlaceSelection(TextureOpQueue, TilesetMode, CellX, CellY);
            TilesetMode->SelectedTile = FindTileAt(File, CellX, CellY);
            Changed = true;
        }
        else if(Input->ShiftDown && (TilesetMode->SelectedTile >= 0) && (There < 0))
        {
            File->Tiles[TilesetMode->SelectedTile].GridX = (u16)CellX;
            File->Tiles[TilesetMode->SelectedTile].GridY = (u16)CellY;
            Changed = true;
        }
        else
        {
            TilesetMode->SelectedTile = There;
        }
    }
    if(OnGrid && WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
    {
        s32 There = FindTileAt(File, CellX, CellY);
        if(There >= 0)
        {
            RemoveTile(TextureOpQueue, TilesetMode, There);
            Changed = true;
        }
    }

    u32 SavedClip = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
    PushRect(RenderGroup, Flat, Rect, 5.0f, V4(0.25f, 0.25f, 0.25f, 1.0f));
    DrawGridLines(RenderGroup, Flat, &View, 6.0f, V4(1, 1, 1, 0.1f));
    for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
    {
        tileset_file_tile *Tile = File->Tiles + TileIndex;
        v2 Center = CellCenter(&View, Tile->GridX, Tile->GridY);
        loaded_bitmap *Bitmap = TilesetMode->Tiles + TileIndex;
        if(Bitmap->Memory)
        {
            PushBitmap(RenderGroup, Flat, Bitmap, View.Cell, V3(Center, 7.0f));
        }
        rectangle2 TileRect = RectCenterDim(Center, V2(View.Cell, View.Cell));
        if((s32)TileIndex == TilesetMode->SelectedTile)
        {
            PushRectOutline(RenderGroup, Flat, TileRect, 8.0f, V4(1, 0.3f, 0.3f, 1), 2.0f);
        }
        else if(Tile->FrameCount > 1)
        {
            PushRectOutline(RenderGroup, Flat, TileRect, 8.0f, V4(0, 0.8f, 1, 1), 1.0f);
        }
    }
    if(OnGrid)
    {
        // NOTE(pvlso): Where the selection would go
        s32 W = TilesetMode->HasSelection ? (AbsoluteValue(TilesetMode->SelectionX1 - TilesetMode->SelectionX0) + 1) : 1;
        s32 H = TilesetMode->HasSelection ? (AbsoluteValue(TilesetMode->SelectionY1 - TilesetMode->SelectionY0) + 1) : 1;
        v2 Min = View.TopLeft + V2((r32)CellX*View.Cell, -(r32)(CellY + H)*View.Cell);
        v2 Max = View.TopLeft + V2((r32)(CellX + W)*View.Cell, -(r32)CellY*View.Cell);
        PushRectOutline(RenderGroup, Flat, RectMinMax(Min, Max), 8.0f, V4(1, 0.85f, 0.2f, 0.8f), 1.0f);
    }
    RenderGroup->CurrentClipRectIndex = SavedClip;

    if(Changed)
    {
        RecountTilesetGrid(File);
    }

    return(Changed);
}

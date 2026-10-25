#if !defined(EDITOR_MAP_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The map editor (map editor plan, phase 2), for the project chosen on the title screen. A map is a
  grid of Width x Height cells in layers; each layer has a Z (its level: 0 the ground, 1 a plateau...), layers draw
  by Z, those sharing a Z in the list's order. A cell holds a tile of one of the map's curated tile sets
  (data/tilesets), by its place in the set's grid, so editing a set doesn't shift a map's tiles. Each map is a text
  file in data/maps (engine_map_file.h), opened and saved here.
*/

#define MAP_MAX_LAYERS 32
#define MAP_MAX_TILESETS 16
#define MAP_MAX_SIDE 1024
#define MAP_NAME_LENGTH 32

struct map_layer
{
    char Name[MAP_NAME_LENGTH];
    s32 Z;
    b32 Hidden;
    map_cell *Cells; // NOTE(pvlso): Width*Height, row by row from the top; its own allocation
};

// NOTE(pvlso): A tile set loaded for painting: read from its file, each tile's first frame cut from its sheets, and its
// tiles found by their place in the set's grid. Everything but the textures lives in Arena.
struct map_tileset
{
    char FileName[64];
    memory_arena Arena;
    tile_flag_names FlagNames;
    tileset_file File;
    loaded_bitmap Sheets[TILESET_MAX_SHEETS];
    loaded_bitmap *Tiles;
    s32 *TileAt; // NOTE(pvlso): GridWidth*GridHeight, the tile index there or -1
};

struct editor_mode_map
{
    char ProjectName[PROJECT_NAME_COUNT];
    b32 Exit;

    // NOTE(pvlso): The map; each layer's cells are their own allocation (Platform.AllocateMemory), so undo, resizing
    // and removing layers give memory back
    b32 HasMap;
    char FileName[MAP_FILE_SET_NAME_LENGTH]; // NOTE(pvlso): in data/maps
    b32 Dirty;
    b32 ExitArmed; // NOTE(pvlso): Exit with unsaved changes asks for a second click
    u32 Width;
    u32 Height;
    u32 LayerCount;
    map_layer Layers[MAP_MAX_LAYERS];
    s32 CurrentLayer;

    u32 SetCount;
    map_tileset Sets[MAP_MAX_TILESETS];
    s32 CurrentSet; // NOTE(pvlso): the one in the palette, -1 for none

    // NOTE(pvlso): The brush: a block of the current set's grid, picked in the palette
    b32 HasBrush;
    b32 PickingBrush;
    s32 BrushX0, BrushY0, BrushX1, BrushY1;

    // NOTE(pvlso): The views (editor_tileset_edit.cpp's grid views): the map on the left, the palette on the right
    r32 Zoom;
    v2 Pan;
    r32 PaletteZoom;
    v2 PalettePan;
    s32 Panning; // NOTE(pvlso): 1 the map, 2 the palette
    v2 PanFrom;
    b32 Painting;
    s32 PaintFromX, PaintFromY;     // NOTE(pvlso): where the drag started, blocks are laid on a grid from there
    s32 LastStampX, LastStampY;
    b32 ShowGrid;
    b32 DimOtherLayers;
    s32 HoverX, HoverY;             // NOTE(pvlso): the map cell under the mouse, -1 outside
    b32 Erasing;
    b32 RectActive;                 // NOTE(pvlso): a Shift drag: fill (left) or clear (right) a rectangle
    b32 RectErase;
    s32 RectX0, RectY0;

    // NOTE(pvlso): The panel's inputs and requests, done by the next update
    char NameInput[MAP_FILE_SET_NAME_LENGTH]; // NOTE(pvlso): for New and Save as
    s32 NameInputLength;
    u32 MapFileCount;
    char **MapFiles;
    u32 MapFileIndex;
    b32 OpenRequested;
    b32 SaveRequested;
    b32 SaveAsRequested;
    s32 NewWidth;
    s32 NewHeight;
    b32 NewMapRequested;
    char NewLayerName[MAP_NAME_LENGTH];
    s32 NewLayerNameLength;
    b32 AddLayerRequested;
    b32 RemoveLayerRequested;
    s32 MoveLayerRequested; // NOTE(pvlso): -1 up the list, 1 down
    u32 TilesetFileCount;
    char **TilesetFiles;
    u32 TilesetFileIndex;
    b32 AddSetRequested;
    s32 ResizeLeft, ResizeRight, ResizeTop, ResizeBottom; // NOTE(pvlso): cells added (or cropped when negative)
    b32 ResizeRequested;
    b32 LayerEdited;     // NOTE(pvlso): the panel changed a layer's name, Z or hidden flag
    b32 TypingLayerName; // NOTE(pvlso): Ctrl+Z is the field's then

    // NOTE(pvlso): Undo: every state after a change, History[HistoryAt] the current one; the oldest go when they pass
    // MAP_HISTORY_BYTES
    u32 HistoryCount;
    u32 HistoryAt;
    umm HistoryBytes;
    struct map_snapshot *History[200];
};

#define MAP_HISTORY_BYTES Megabytes(256)

internal void PlayMapEditor(editor_state *EditorState, transient_state *TranState, char *ProjectName);

#define EDITOR_MAP_MODE_H
#endif

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
  (data/tilesets), by its place in the set's grid, so editing a set doesn't shift a map's tiles. Maps stay in memory
  until island files come (phase 3).
*/

#define MAP_MAX_LAYERS 32
#define MAP_MAX_TILESETS 16
#define MAP_MAX_SIDE 1024
#define MAP_NAME_LENGTH 32

struct map_cell
{
    u16 Set; // NOTE(pvlso): 0 for an empty cell, else the map's tile set + 1
    u16 X;   // NOTE(pvlso): the tile's place in its set's grid
    u16 Y;
};

struct map_layer
{
    char Name[MAP_NAME_LENGTH];
    s32 Z;
    b32 Hidden;
    map_cell *Cells; // NOTE(pvlso): Width*Height, row by row from the top
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

    // NOTE(pvlso): The map; its cells live in MapArena, cleared for a new map
    memory_arena MapArena;
    b32 HasMap;
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

    // NOTE(pvlso): The panel's inputs and requests, done by the next update
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
};

internal void PlayMapEditor(editor_state *EditorState, transient_state *TranState, char *ProjectName);

#define EDITOR_MAP_MODE_H
#endif

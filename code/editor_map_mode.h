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
  grid of Width x Height cells in layers. Each layer belongs to a level (phase 6: 0 the ground, 1 a bridge or plateau
  above it, -1 below) and has a Z, its order within the level. The map draws level by level from the lowest: a
  level's layers by Z (those sharing a Z in the list's order), then the entities placed on it. A cell holds a tile of one of the map's curated tile sets
  (data/tilesets), by its place in the set's grid, so editing a set doesn't shift a map's tiles. Each map is a text
  file in data/maps (engine_map_file.h), opened and saved here.

  The Entities tool (phase 5d) places the project's entity definitions (data/entities) on the current level, each
  standing on its foot, drawn front to back by it. A placement has a GUID of its own, its definition's, a position in
  cells, a flip and its level.

  The Navmesh tool (phase 6c) draws the navigation areas of each level: walkable polygons, and holes cut out of them,
  saved with the map and baked into navigation meshes by Write Assets (6d).
*/

#define MAP_MAX_LAYERS 32
#define MAP_MAX_TILESETS 16
#define MAP_MAX_SIDE 1024
#define MAP_NAME_LENGTH 32

struct map_layer
{
    char Name[MAP_NAME_LENGTH];
    s32 Z;     // NOTE(pvlso): its order within its level
    s32 Level;
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

// NOTE(pvlso): An entity placed on the map
struct map_placement
{
    u64 GUID;
    u64 Definition;
    v2 P; // NOTE(pvlso): the foot, in cells from the map's top left, y down
    b32 Flip;
    s32 Level;
};

// NOTE(pvlso): A definition to place, with the look of its first clip that has one (a sprite sheet shows its first
// frame), standing on the stored asset's alignment
struct map_entity_def
{
    char *FileName;
    entity_file File;
    entity_look_kind LookKind;
    loaded_bitmap Look;
    v2 Align;
    u32 FrameCount;
};

// NOTE(pvlso): A navigation area: a closed polygon where things on its level may walk, or a hole cut out of them
struct map_nav
{
    s32 Level;
    b32 Hole;
    u32 Count;
    v2 Points[MAP_NAV_MAX_POINTS]; // NOTE(pvlso): in cells from the map's top left, y down
};

enum map_tool
{
    MapTool_Tiles,
    MapTool_Entities,
    MapTool_Navmesh,
};

struct editor_mode_map
{
    char ProjectName[PROJECT_NAME_COUNT];
    b32 Exit;
    map_tool Tool;

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
    s32 CurrentLevel; // NOTE(pvlso): where entities are placed; picking a layer picks its level

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
    b32 DimOtherLevels;
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
    b32 TypingLayerName; // NOTE(pvlso): Ctrl+Z is the field's then (any of the panel's text fields)

    // NOTE(pvlso): Entities: the project's definitions, those placed (their own allocation, grown as needed), the one
    // picked to place and the placement selected to edit
    b32 EntityDefsLoaded; // NOTE(pvlso): by the first update, which has the texture queue
    entity_project *EntityProject;
    entity_file_list EntityFiles;
    u32 EntityDefCount;
    map_entity_def *EntityDefs;
    s32 CurrentDef; // NOTE(pvlso): -1 for none
    u32 EntityCount;
    u32 EntityCapacity;
    map_placement *Entities;
    s32 SelectedEntity; // NOTE(pvlso): -1 for none
    map_placement LastSelected; // NOTE(pvlso): how the selected one was, to tell when the panel changed it
    s32 LastSelectedIndex;
    r32 EntityPaletteScroll;
    b32 DraggingEntity;
    b32 EntityMoved;
    v2 DragOffset;
    b32 ShowEntities;
    b32 RemoveEntityRequested;
    b32 EntityEdited;

    // NOTE(pvlso): The scatter brush: a drag lays many of the picked definition inside a circle, none of them nearer
    // than Spacing to another of it; a right drag takes them away. A stroke is one step of the history.
    b32 Scatter;
    r32 ScatterRadius;  // NOTE(pvlso): in cells
    r32 ScatterSpacing; // NOTE(pvlso): in cells
    b32 ScatterFlip;    // NOTE(pvlso): each one flipped or not at random
    b32 Scattering;
    b32 ScatterErasing;
    b32 ScatterChanged;
    u64 ScatterRandom;

    // NOTE(pvlso): Navigation areas (their own allocation, grown as needed), the one selected, the one being drawn
    // (NewNav, closed into the list), a point being dragged
    u32 NavCount;
    u32 NavCapacity;
    map_nav *Navs;
    s32 SelectedNav;  // NOTE(pvlso): -1 for none
    b32 NavDrawing;
    map_nav NewNav;
    b32 NewNavHole;   // NOTE(pvlso): what the next area drawn is
    s32 DragVertex;   // NOTE(pvlso): of the selected area, -1 for none
    b32 NavMoved;
    b32 ShowNav;      // NOTE(pvlso): in the other tools
    b32 NavEnterPressed;
    b32 NavBackPressed;
    b32 RemoveNavRequested;
    b32 NavEdited;    // NOTE(pvlso): the panel changed the selected area's level or kind

    // NOTE(pvlso): "Test the bake": the areas baked as Write Assets would (the baked mesh in NavTestArena), a path
    // between two clicks. It ends with anything that could change the areas.
    b32 NavTest;
    memory_arena NavTestArena;
    map_nav_mesh NavTestMesh;
    map_nav_scratch NavTestScratch;
    b32 NavTestHasFrom;
    b32 NavTestHasTo;
    v2 NavTestFrom;
    v2 NavTestTo;
    s32 NavTestFromLevel;
    s32 NavTestToLevel;
    u32 NavTestPointCount;
    nav_path_point NavTestPoints[128];

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

#if !defined(ENGINE_TILESET_FILE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): Curated tile sets. A set is a text file in the project's data/tilesets, gathering tiles from any
  sheets (the asset packs' PNGs, wherever they are under data/) into a grid of its own:

    ; a comment
    size 32
    sheet data/game_assets/useful/asset_packs/RPGW_AncientForest_v1.0/_PNG/MainLev.png
    sheet data/game_assets/useful/asset_packs/RPGW_GrassLand_v2.01/MainLev2.0.png
    tile 0 0  0 5 30  name "Grass"  flags Walkable
    tile 1 0  1 40 39 name "Pond"   kind Water  flags Water InteractableWater  frames 1 41 39 1 42 39  fps 6

  Sheets are numbered in the order they come, their paths are from the directory holding data/. A tile is its place
  in the set's grid (x, y from the top left), then the sheet and the cell it's cut from (x, y in tiles from the
  sheet's top left), then optional parts in any order: its name, its kind (one of the project's
  data/txts/tile_kinds.txt, what the game takes the ground to be: Grass, Water...), its flags (names from the project's
  data/txts/tile_flags.txt, one per line), more animation frames after its own cell (sheet x y each), and their speed.
*/

#define TILESET_MAX_SHEETS 16
#define TILESET_MAX_TILES 2048
#define TILESET_MAX_TILE_FRAMES 16
#define TILESET_MAX_FLAGS 32
#define TILESET_MAX_KINDS 64
#define TILESET_FLAG_NAME_LENGTH 32
#define TILESET_SHEET_PATH_LENGTH 256

struct tileset_file_cell
{
    u16 Sheet;
    u16 X;
    u16 Y;
};

struct tileset_file_tile
{
    u16 GridX;
    u16 GridY;
    u32 Flags;
    u32 Kind; // NOTE(pvlso): 0 for none, else the n-th name in tile_kinds.txt, from 1
    r32 FramesPerSecond;
    char Name[KEA_TILE_NAME_LENGTH];

    // NOTE(pvlso): Frames[0] is the tile's own cell; more than one frame means it's animated
    u32 FrameCount;
    tileset_file_cell Frames[TILESET_MAX_TILE_FRAMES];
};

struct tileset_file
{
    b32 Loaded;
    u32 ErrorCount;

    u32 TileSize;
    u32 GridWidth;
    u32 GridHeight;

    u32 SheetCount;
    char Sheets[TILESET_MAX_SHEETS][TILESET_SHEET_PATH_LENGTH];

    u32 TileCount;
    tileset_file_tile *Tiles; // NOTE(pvlso): room for TILESET_MAX_TILES
};

// NOTE(pvlso): The project's names for tiles' flags (data/txts/tile_flags.txt) and kinds (data/txts/tile_kinds.txt)
struct tile_flag_names
{
    u32 Count;
    char Names[TILESET_MAX_FLAGS][TILESET_FLAG_NAME_LENGTH];
    u32 KindCount;
    char KindNames[TILESET_MAX_KINDS][TILESET_FLAG_NAME_LENGTH];
};

#define ENGINE_TILESET_FILE_H
#endif

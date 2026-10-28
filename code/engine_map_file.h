#if !defined(ENGINE_MAP_FILE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): A map (an island, in Spellweaver) is a text file in the project's data/maps, written by the map editor:

    ; a comment
    size 64 48
    set hearthmoor_ground.txt
    set hearthmoor_cliffs.txt
    layer "Ground" 0
    row . . 1:5:30 1:6:30 . ...
    row ...
    layer "Cliff tops" 1 hidden
    row ...

  The size is the width and height in cells. Sets are the curated tile sets it paints with (data/tilesets), numbered
  from 1 in the order they come. Each layer has a name, its Z and maybe "hidden", then a row line per row of the map
  from the top, a cell per column: "." when empty, else set:x:y, the set and the tile's place in that set's grid.
  Rows or cells left out are empty.

  Then the entities placed on it (map editor plan, 5d), over every layer:

    entity 4c1d09a7e2b35f80 2f78fe90238fbc4d 12.500 30.250 flip
    override Damageable MaxHealth 20

  The placement's own GUID (made when it's placed, kept across saves, for saves to name it by), its definition's GUID
  (data/entities, engine_entity_file.h), where its foot stands in cells from the map's top left (y down, like the
  rows), and maybe "flip" (mirrored left to right). The override lines under it start it with other values than its
  definition's for a few of its blocks' fields, the value written as an entity file writes it.
*/

#define MAP_FILE_MAX_LAYERS 32
#define MAP_FILE_MAX_SETS 16
#define MAP_FILE_MAX_SIDE 1024
#define MAP_FILE_NAME_LENGTH 32
#define MAP_FILE_SET_NAME_LENGTH 64
#define MAP_FILE_MAX_ENTITY_OVERRIDES 4

struct map_cell
{
    u16 Set; // NOTE(pvlso): 0 for an empty cell, else the map's tile set + 1
    u16 X;   // NOTE(pvlso): the tile's place in its set's grid
    u16 Y;
};

struct map_file_layer
{
    char Name[MAP_FILE_NAME_LENGTH];
    s32 Z;
    b32 Hidden;
    map_cell *Cells; // NOTE(pvlso): Width*Height, row by row from the top
};

// NOTE(pvlso): An override as the file has it, by the block's and field's names: whoever reads the map knows the
// project's blocks (ReadEntityFieldValue reads the value)
struct map_file_override
{
    char Block[ENTITY_NAME_LENGTH];
    char Field[ENTITY_NAME_LENGTH];
    char Value[ENTITY_TEXT_LENGTH];
};

struct map_file_entity
{
    u64 GUID;
    u64 Definition;
    v2 P; // NOTE(pvlso): the foot, in cells from the map's top left, y down
    b32 Flip;
    u32 OverrideCount;
    map_file_override Overrides[MAP_FILE_MAX_ENTITY_OVERRIDES];
};

struct map_file
{
    b32 Loaded;
    u32 ErrorCount;
    u32 Width;
    u32 Height;
    u32 SetCount;
    char Sets[MAP_FILE_MAX_SETS][MAP_FILE_SET_NAME_LENGTH];
    u32 LayerCount;
    map_file_layer Layers[MAP_FILE_MAX_LAYERS];
    u32 EntityCount;
    map_file_entity *Entities;
};

#define ENGINE_MAP_FILE_H
#endif

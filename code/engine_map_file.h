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
*/

#define MAP_FILE_MAX_LAYERS 32
#define MAP_FILE_MAX_SETS 16
#define MAP_FILE_MAX_SIDE 1024
#define MAP_FILE_NAME_LENGTH 32
#define MAP_FILE_SET_NAME_LENGTH 64

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
};

#define ENGINE_MAP_FILE_H
#endif

#if !defined(EDITOR_GAME_MODE_TILE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#define MAX_FILL_TILES 4096
struct tile_queue
{
    world_position Tiles[MAX_FILL_TILES];
    s32 Front;
    s32 Rear;
};

#define EDITOR_GAME_MODE_TILE_H
#endif

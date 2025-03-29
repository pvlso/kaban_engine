#if !defined(EDITOR_GAME_MODE_UNDO_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

enum action_type
{
    Action_None,
    Action_AddTile,
    Action_RemoveTile,
    Action_FloodFill,
};

struct flood_fill_action
{
    u32 TileCount;
    sswm_ground_tile *PrevTiles;
};

struct editor_action
{
    action_type Type;
    union
    {
        sswm_ground_tile PrevTile;
        flood_fill_action FloodFill;
    };
};

#define MAX_UNDO_ACTIONS 128
struct action_stack
{
    editor_action *Actions;
    s32 Start;
    s32 End;
    u32 Size;
};

#define EDITOR_GAME_MODE_UNDO_H
#endif

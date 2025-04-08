#if !defined(EDITOR_GAME_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#include "engine_game_mode_world.h"
#include "engine_game_mode_entity.h"
#include "engine_game_mode_sim_region.h"

#include "engine_game_mode_undo.h"

#include "engine_game_mode_tile.h"
#include "engine_game_mode_navmesh.h"

struct controlled_camera
{
    b32 SetObstacle;
};

enum edit_game_mode
{
    EditGameMode_None,
    EditGameMode_Terrain,
    EditGameMode_NavMeshes,

    EditGameMode_Count,
};

enum editor_game_mode_actions
{
    GMAction_None,

    GMAction_Exit,
    GMAction_WriteSSWM,
    GMAction_EditEnable,

    // NOTE(babykaban): Tiles
    GMAction_ShowCurrentLayer,
    GMAction_FillActive,

    // NOTE(babykaban): Nav meshes
    GMAction_WritePolygons,
    GMAction_StartNewPolygon,
    GMAction_ResetCurrentPolygon,
    GMAction_DeleteCurrentPolygon,
    GMAction_TriangulateAll,
    GMAction_SubtractRegion,
};

enum editor_game_mode_flags
{
    GMFlag_EditEnable = (1 << 0),
    GMFlag_ShowCurrentLayer = (1 << 1),
    GMFlag_FillActive = (1 << 2),
};

struct editor_mode_game
{
    u32 CurrentAction;
    u32 Flags;
    
    edit_game_mode GameEditMode;

    json_element *JsonStringsHead;    
    string_array *EnumStringArraysHash[4096];

    world *World;
    controlled_camera ControlledCameras[ArrayCount(((engine_input *)0)->Controllers)];

    s32 CameraMoveStep;
    world_position CameraP;

    u32 CreationBufferIndex;
    entity CreationBuffers[16];
    u32 LastUsedEntityStorageIndex;

    tileset_id CurrentTileset;
    loaded_tileset *Tileset;
    ssa_tileset *TilesetInfo;

    b32 FillActive;
    u32 MapGroundLayer;
    u32 LayerCount;
    s32 CurrentZLayer;
    ssa_tile Tile;
    array_cursor TileCursor;
    
    u32 CurrentPolygonIndex;
    world_polygon *CurrentPolygon;

//    r32 RunTestForSeconds;
//    r32 NextTriangle;
//    s32 NextIndex;
//    u8 States[3];
//    triangle ClipT;
//    triangle SubjectT;

    u32 PolygonCount;
    b32 Triangulated;
    world_polygon *Polies;
//    world_triangulated_poly *TPolies;
    world_position *ChosenVertex;

    s32 MeshTriangleCount;
    world_triangle *MeshTriangles;
    s32 FreeIndexCount;
    s32 *FreeTriangleIndices;
    
    r32 Time;
    r32 AutoWriteSeconds;
    
    action_stack UndoStack;
    action_stack RedoStack;
};

inline void
SetGameModeFlag(editor_mode_game *GameMode, u32 Flag)
{
    GameMode->Flags |= Flag;
}

inline void
ClearGameModeFlag(editor_mode_game *GameMode, u32 Flag)
{
    GameMode->Flags &= ~Flag;
}

inline b32
IsSetGameModeFlag(editor_mode_game *GameMode, u32 Flag)
{
    b32 Result = GameMode->Flags & Flag;
    return(Result);
}

inline void
ToggleGMFlag(editor_mode_game *GameMode, u32 Flag)
{
    if(IsSetGameModeFlag(GameMode, Flag))
    {
        ClearGameModeFlag(GameMode, Flag);
    }
    else
    {
        SetGameModeFlag(GameMode, Flag);
    }
}

internal void PlayGameMode(editor_state *EditorState, transient_state *TranState);

#define EDITOR_GAME_MODE_H
#endif

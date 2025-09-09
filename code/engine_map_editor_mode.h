#if !defined(ENGINE_MAP_EDITOR_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */
#include "engine_game_mode_undo.h"

#include "engine_game_mode_tile.h"

#include "engine_navigation_mesh.h"
#include "engine_game_mode_navmesh.h"

enum map_editor_mode
{
    MapEditorMode_None,
    MapEditorMode_Terrain,
    MapEditorMode_NavMeshes,
    MapEditorMode_Entity,

    MapEditorMode_Count,
};

enum map_editor__actions
{
    MEAction_None,

    MEAction_Exit,
    MEAction_WriteSSWM,
    MEAction_EditEnable,

    // NOTE(babykaban): Tiles
    MEAction_ShowCurrentLayer,
    MEAction_FillActive,

    // NOTE(babykaban): Nav meshes
    MEAction_WritePolygons,
    MEAction_StartNewPolygon,
    MEAction_ResetCurrentPolygon,
    MEAction_DeleteCurrentPolygon,
    MEAction_TriangulateAll,
    MEAction_SubtractRegion,

    MEAction_NavMeshPlaceStart,
    MEAction_NavMeshPlaceEnd,
};

enum map_editor_flags
{
    MEFlag_EditEnable = (1 << 0),
    MEFlag_ShowCurrentLayer = (1 << 1),
    MEFlag_FillActive = (1 << 2),
};

struct controlled_camera
{
    entity_id EntityIndex;
};

struct navigation_state
{
};

struct engine_map_editor
{
    u32 CurrentAction;
    u32 Flags;
    
    b32 HideUI;
    map_editor_mode MapEditorMode;

    s32 CameraMoveStep;
    controlled_camera ControlledCameras[ArrayCount(((engine_input *)0)->Controllers)];

    tileset_id CurrentTileset;
    loaded_tileset *Tileset;
    ssa_tileset *TilesetInfo;

    b32 ShowGrid;
    b32 FillActive;
    u32 MapGroundLayer;
    u32 LayerCount;
    s32 CurrentZLayer;
    ssa_tile Tile;
    array_cursor TileCursor;

// NOTE(paul): NAVIGATION MESHES --------------------------------------------------
    u32 CurrentPolygonIndex;
    world_polygon *CurrentPolygon;

    world_position *ChosenVertex;

    navigation_mesh NavMesh;

    b32 ShowNativePolies;
    b32 ShowNativeIds;
    b32 ShowPartition;
    b32 ShowColor;
    b32 ShowNeighbours;

    world_position StartNode;
    world_position EndNode;
    
//    s32 MeshTriangleCount;
//    world_triangle *MeshTriangles;
//    s32 FreeIndexCount;
//    s32 *FreeTriangleIndices;

// --------------------------------------------------------------------------------
    
    f32 AutoWriteSeconds;
    f32 Zoom;
    f32 Time;
    
    action_stack UndoStack;
    action_stack RedoStack;

    world_state *WorldState;
};

inline void
SetMapEditorFlag(engine_map_editor *MapEditor, u32 Flag)
{
    MapEditor->Flags |= Flag;
}

inline void
ClearMapEditorFlag(engine_map_editor *MapEditor, u32 Flag)
{
    MapEditor->Flags &= ~Flag;
}

inline b32
IsSetMapEditorFlag(engine_map_editor *MapEditor, u32 Flag)
{
    b32 Result = MapEditor->Flags & Flag;
    return(Result);
}

inline void
ToggleMEFlag(engine_map_editor *MapEditor, u32 Flag)
{
    if(IsSetMapEditorFlag(MapEditor, Flag))
    {
        ClearMapEditorFlag(MapEditor, Flag);
    }
    else
    {
        SetMapEditorFlag(MapEditor, Flag);
    }
}

internal void PlayMapEditor(editor_state *EditorState, transient_state *TranState);

#define ENGINE_MAP_EDITOR_MODE_H
#endif

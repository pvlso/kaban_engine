#if !defined(EDITOR_GAME_MODE_NAVMESH_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#define MAX_VERTEX_COUNT 128

struct triangle_map
{
    s32 Triangles[2];
    s32 Count;
};

struct world_triangle 
{
    union 
    {
        struct
        {
            world_position V1;
            world_position V2;
            world_position V3;
        };

        world_position Vertices[3]; 
    };

    triangle_adjs Adj;
    
    rectangle2i Bounds;
};

#define EDITOR_GAME_MODE_NAVMESH_H
#endif

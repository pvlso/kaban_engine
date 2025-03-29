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

struct world_polygon
{
    s32 VertexCount;
    world_position *Vertices;

    b32 HasHoles;
    s32 HoleCount;
    s32 *HoleVertexCounts;
    world_position *HoleVertices;
};

struct world_polygon_set
{
    s32 PolygonCount;
    world_polygon *Polygons;
};

#define EDITOR_GAME_MODE_NAVMESH_H
#endif

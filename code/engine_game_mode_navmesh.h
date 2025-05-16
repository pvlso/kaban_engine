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

struct neighbour_edge
{
    world_position A;
    world_position B;
};

struct nav_poly_node
{
    world_position TileP;

    s32 Index;
    s32 PolyIndex;
    
    b32 Visited;

    r32 GlobalGoal;
    r32 LocalGoal;

    rectangle2i Bounds;

    s32 NeighbourCount;
    nav_poly_node *Neighbours[8];
    neighbour_edge NEdge[8];
    
    nav_poly_node *Parent;
};

inline rectangle2i
CalculatePolygonBoundingBox(world_polygon *A)
{
    rectangle2i Result = InvertedInfinityRectangle2i();

    for(s32 I = 0;
        I < A->VertexCount;
        ++I)
    {
        if(A->Vertices[I].TileX < Result.Min.x) Result.Min.x = A->Vertices[I].TileX;
        if(A->Vertices[I].TileX > Result.Max.x) Result.Max.x = A->Vertices[I].TileX;
        if(A->Vertices[I].TileY < Result.Min.y) Result.Min.y = A->Vertices[I].TileY;
        if(A->Vertices[I].TileY > Result.Max.y) Result.Max.y = A->Vertices[I].TileY;
    }
    
    return(Result);
}

inline bool32
IsInRectangleMesh(rectangle2i Rectangle, v2i Test)
{
    bool32 Result = ((Test.x >= Rectangle.Min.x) &&
                     (Test.y >= Rectangle.Min.y) &&
                     (Test.x <= Rectangle.Max.x) &&
                     (Test.y <= Rectangle.Max.y));

    return(Result);
}

#define EDITOR_GAME_MODE_NAVMESH_H
#endif

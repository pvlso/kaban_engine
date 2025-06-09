#if !defined(ENGINE_NAVIGATION_MESH_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

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

struct world_polygon_list
{
    world_polygon Poly;
    polygon2 RealPoly;

    world_polygon_list *Next;
    world_polygon_list *Prev;
};

struct nav_poly_node
{
    world_position TileP;

    s32 Index;
    world_polygon_list *PolyPtr;
    
    b32 Visited;

    r32 GlobalGoal;
    r32 LocalGoal;

    rectangle2i Bounds;

    s32 NeighbourCount;
    nav_poly_node *Neighbours[8];
    neighbour_edge NEdge[8];
    
    nav_poly_node *Parent;
};

struct navigation_mesh
{
    b32 Partitioned;

    u32 PolygonCount;
    world_polygon *Polies;

    s32 MeshPolygonCount;
    world_polygon_list MeshPolygonsSentinal;
    world_polygon_list *FreePolygons;

    u32 PolyNodeCount;
    nav_poly_node *PolyNodes;
    heap MinPolyNodeHeap;

    memory_arena Arena;
    hash_table EdgeTable;
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

inline void
ConvertWorldPolygonToPolygon2(world *World, world_position *BaseP,
                              world_polygon *A, polygon2 *Dest)
{
    for(s32 VertexIndex = 0;
        VertexIndex < A->VertexCount;
        ++VertexIndex)
    {
        world_position *PolygonPoint0 = A->Vertices + VertexIndex;
        v2 P = Subtract(World, PolygonPoint0, BaseP);
        Dest->Vertices[VertexIndex] = P;
    }

    Dest->VertexCount = A->VertexCount;
}

inline void InitNavMesh(navigation_mesh *NavMesh, memory_arena *Arena);
internal s32 FindNavPolyNodeForPoint(navigation_mesh *NavMesh, world *World, sim_region *SimRegion, world_position P);
internal void PartitionNavigationMesh(navigation_mesh *NavMesh, world *World, sim_region *SimRegion, memory_arena *TempArena);
internal nav_poly_node *SolvePolyAStar(navigation_mesh *NavMesh, world *World, nav_poly_node *Start, nav_poly_node *End);
internal s32 StringPull(v2 *Portals, s32 PortalsCount, v2 *Points, s32 MaxPoints);

#define ENGINE_NAVIGATION_MESH_H
#endif

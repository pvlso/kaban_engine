
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline void
ConvertWorldPolygonToPolygon2(world *World, world_position *BaseP, world_polygon *A, polygon2 *Dest)
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

inline void
AddVertex(editor_mode_game *GameMode, world_polygon *Polygon, world *World, world_position TileP)
{
    if((Polygon->VertexCount + 1) < MAX_VERTEX_COUNT)
    {
        if(!GameMode->ChosenVertex)
        {
            r32 Tolerance = 0.002f;
            for(s32 VertexIndex = 0;
                VertexIndex < Polygon->VertexCount;
                ++VertexIndex)
            {
                world_position *Vertex = Polygon->Vertices + VertexIndex;
                v2 Delta = Subtract(World, Vertex, &TileP);
                r32 X = AbsoluteValue(Delta.x);
                r32 Y = AbsoluteValue(Delta.y);
                if((X < Tolerance) && (Y < Tolerance))
                {
                    GameMode->ChosenVertex = Vertex;
                    break;
                }
            }

            if(!GameMode->ChosenVertex)
            {
                Polygon->Vertices[Polygon->VertexCount++] = TileP;
            }
        }
        else
        {
            GameMode->ChosenVertex = 0;
        }
    }
}

inline void
RemoveVertex(editor_mode_game *GameMode, world_polygon *Polygon, world *World, world_position TileP)
{
    if(!GameMode->ChosenVertex)
    {
        r32 Tolerance = 0.002f;
        for(s32 VertexIndex = 0;
            VertexIndex < Polygon->VertexCount;
            ++VertexIndex)
        {
            world_position *Vertex = Polygon->Vertices + VertexIndex;
            v2 Delta = Subtract(World, Vertex, &TileP);
            r32 X = AbsoluteValue(Delta.x);
            r32 Y = AbsoluteValue(Delta.y);
            if((X < Tolerance) && (Y < Tolerance))
            {
                *Vertex = {};
                --Polygon->VertexCount;
                for(s32 Index = VertexIndex;
                    Index < Polygon->VertexCount;
                    ++Index)
                {
                    Polygon->Vertices[Index] = Polygon->Vertices[Index + 1];
                }

                break;
            }
        }
    }
}

inline void
StartNewPolygon(editor_mode_game *GameMode)
{
    GameMode->CurrentPolygon = GameMode->Polies + GameMode->PolygonCount;
    ++GameMode->CurrentPolygonIndex;
    ++GameMode->PolygonCount;
}

inline void
ResetPolygon(world_polygon *Polygon)
{
    ZeroArray(Polygon->VertexCount, Polygon->Vertices);
    Polygon->VertexCount = 0;
}

inline void
DeletePolygon(editor_mode_game *GameMode)
{
    if(GameMode->CurrentPolygonIndex != 0)
    {
        --GameMode->CurrentPolygonIndex;
        --GameMode->PolygonCount;

        GameMode->CurrentPolygon = GameMode->Polies + GameMode->CurrentPolygonIndex;
    }
}

inline void
CalculateTriangleTileBoundingBox(world_triangle *T)
{
    T->Bounds.MinX = T->Bounds.MaxX = T->Vertices[0].TileX;
    T->Bounds.MinY = T->Bounds.MaxY = T->Vertices[0].TileY;

    for(s32 I = 1;
        I < ArrayCount(T->Vertices);
        ++I)
    {
        if(T->Vertices[I].TileX < T->Bounds.MinX) T->Bounds.MinX = T->Vertices[I].TileX;
        if(T->Vertices[I].TileX > T->Bounds.MaxX) T->Bounds.MaxX = T->Vertices[I].TileX;
        if(T->Vertices[I].TileY < T->Bounds.MinY) T->Bounds.MinY = T->Vertices[I].TileY;
        if(T->Vertices[I].TileY > T->Bounds.MaxY) T->Bounds.MaxY = T->Vertices[I].TileY;
    }
}

inline edgefp22_10 *
CreateEdge(fp22_10_v2 V1, fp22_10_v2 V2, edgefp22_10 *Edges, s32 Index)
{
    edgefp22_10 *Result = Edges + Index;
    if((V1.x < V2.x) || ((V1.x == V2.x) && (V1.y < V2.y)))
    {
        Result->a = V1;
        Result->b = V2;
    }
    else
    {
        Result->a = V2;
        Result->b = V1;
    }

    return(Result);
}

inline b32
EdgesEqual(edge A, edge B)
{
    b32 Result = (TRISUBPointsAreEqual(A.a, B.a) && TRISUBPointsAreEqual(A.b, B.b));
    return(Result);
}

internal void
FindOrAddEdge(hash_table *Table, edgefp22_10 *Edge, s32 TriangleIndex, memory_arena *Arena)
{
    TIMED_FUNCTION();
    
    void *Data = GetHashElement(Table, Edge, HashKeyType_EDGEV2);
    if(Data)
    {
        triangle_map *Map = (triangle_map *)Data; 
        Map->Triangles[Map->Count++] = TriangleIndex;
    }
    else
    {
        triangle_map *Map = PushStruct(Arena, triangle_map);
        Map->Triangles[Map->Count++] = TriangleIndex;
        InsertKey(Table, Edge, Map, HashKeyType_EDGEV2, Arena);
    }
}

inline b32
WorldPointsAreEqual(world *World, world_position *A, world_position *B)
{
    b32 Result = false;
    if(AreInSameTile(World, A, B))
    {
        Result = PointsAreEqual(A->Offset, B->Offset, 0.01f);
    }

    return(Result);
}

inline b32
SameEdge(world *World, world_position *a, world_position *b, world_position *c, world_position *d)
{
    b32 Result = ((WorldPointsAreEqual(World, a, c) && WorldPointsAreEqual(World, b, d)) ||
                  (WorldPointsAreEqual(World, a, d) && WorldPointsAreEqual(World, b, c)));
    return(Result);
}

inline b32
ShareAnEdge(world *World, world_triangle *T0, world_triangle *T1)
{
    b32 Result = false;
    for(s32 I = 0; I < 3; ++I)
    {
        s32 INext = (I + 1) % 3;
        for(s32 J = 0; J < 3; ++J)
        {
            s32 JNext = (J + 1) % 3;
            if(SameEdge(World, &T0->Vertices[I], &T0->Vertices[INext],
                        &T1->Vertices[J], &T1->Vertices[JNext]))
            {
                Result = true;
            }
        }

        if(Result)
            break;
    }

    return(Result);
}

internal void
BuildAdjacenciesArray(editor_mode_game *GameMode, sim_region *SimRegion)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(&GameMode->World->Arena); 

    u32 *Counts = PushArray(TempMem.Arena, GameMode->MeshTriangleCount, u32);
    triangle_adjs *Adjs = PushArray(TempMem.Arena, GameMode->MeshTriangleCount, triangle_adjs);
    
    for(s32 I = 0;
        I < GameMode->MeshTriangleCount;
        ++I)
    {
        for(s32 J = I + 1;
            J < GameMode->MeshTriangleCount;
            ++J)
        {
            world_triangle *A = GameMode->MeshTriangles + I;
            world_triangle *B = GameMode->MeshTriangles + J;
            if((I != J) && IsValid(A->Vertices[0]) && IsValid(B->Vertices[0]))
            {
                if(ShareAnEdge(GameMode->World, A, B))
                {
                    Adjs[I].Adjacencies[Counts[I]++] = J;
                    Adjs[J].Adjacencies[Counts[J]++] = I;
                }
            }
        }    
    }    

    for(s32 I = 0;
        I < GameMode->MeshTriangleCount;
        ++I)
    {
        world_triangle *A = GameMode->MeshTriangles + I;
        A->Adj.Adjacencies[0] = -1;
        A->Adj.Adjacencies[1] = -1;
        A->Adj.Adjacencies[2] = -1;
        for(u32 J = 0; J < Counts[I]; ++J)
        {
            A->Adj.Adjacencies[J] = Adjs[I].Adjacencies[J];
        }
    }

    int a = 0;
    
#if 0
    hash_table EdgeHashTable = {};
    EdgeHashTable.Size = 3*GameMode->MeshTriangleCount;
    EdgeHashTable.Hash = PushArray(TempMem.Arena, EdgeHashTable.Size, hash_table_entry *);

    edgefp22_10 *Edges = PushArray(TempMem.Arena, 3*GameMode->MeshTriangleCount, edgefp22_10);
    
    for(s32 I = 0;
        I < GameMode->MeshTriangleCount;
        ++I)
    {
        world_triangle *T = GameMode->MeshTriangles + I;
        if(IsValid(T->V1))
        {
            fp22_10_v2 V1 = Fp22_10_V2(Subtract(GameMode->World, &T->V1, &SimRegion->Origin));
            fp22_10_v2 V2 = Fp22_10_V2(Subtract(GameMode->World, &T->V2, &SimRegion->Origin));
            fp22_10_v2 V3 = Fp22_10_V2(Subtract(GameMode->World, &T->V3, &SimRegion->Origin));
            
            s32 TIndex = 3*I;
            edgefp22_10 *Edge1 = CreateEdge(V1, V2, Edges, TIndex + 0);
            edgefp22_10 *Edge2 = CreateEdge(V2, V3, Edges, TIndex + 1);
            edgefp22_10 *Edge3 = CreateEdge(V3, V1, Edges, TIndex + 2);

            FindOrAddEdge(&EdgeHashTable, Edge1, I, TempMem.Arena);
            FindOrAddEdge(&EdgeHashTable, Edge2, I, TempMem.Arena);
            FindOrAddEdge(&EdgeHashTable, Edge3, I, TempMem.Arena);
        }
    }
    
    for(s32 I = 0;
        I < GameMode->MeshTriangleCount;
        ++I)
    {
        world_triangle *T = GameMode->MeshTriangles + I;
        if(IsValid(T->V1))
        {

            s32 TIndex = 3*I;
            edgefp22_10 *Edge1 = Edges + TIndex + 0;
            edgefp22_10 *Edge2 = Edges + TIndex + 1;
            edgefp22_10 *Edge3 = Edges + TIndex + 2;

            triangle_map *Map0 = (triangle_map *)GetHashElement(&EdgeHashTable, Edge1, HashKeyType_EDGEV2);
            triangle_map *Map1 = (triangle_map *)GetHashElement(&EdgeHashTable, Edge2, HashKeyType_EDGEV2);
            triangle_map *Map2 = (triangle_map *)GetHashElement(&EdgeHashTable, Edge3, HashKeyType_EDGEV2);

            if(Map0->Count == 2)
            {
                if(Map0->Triangles[0] == I)
                {
                    T->Adj.AdjV1V2 = Map0->Triangles[1];
                }
                else
                {
                    T->Adj.AdjV1V2 = Map0->Triangles[0];
                }
            }
            else
            {
                T->Adj.AdjV1V2 = -1;
            }
        
            if(Map1->Count == 2)
            {
                if(Map1->Triangles[0] == I)
                {
                    T->Adj.AdjV2V3 = Map1->Triangles[1];
                }
                else
                {
                    T->Adj.AdjV2V3 = Map1->Triangles[0];
                }
            }
            else
            {
                T->Adj.AdjV2V3 = -1;
            }

            if(Map2->Count == 2)
            {
                if(Map2->Triangles[0] == I)
                {
                    T->Adj.AdjV3V1 = Map2->Triangles[1];
                }
                else
                {
                    T->Adj.AdjV3V1 = Map2->Triangles[0];
                }
            }
            else
            {
                T->Adj.AdjV3V1 = -1;
            }
        }
    }
#endif
    
    EndTemporaryMemory(TempMem);
}

internal void
TriangulatePolygons(editor_mode_game *GameMode, world_position *BaseP, memory_arena *Arena)
{
    TIMED_FUNCTION();

    GameMode->MeshTriangleCount = 0;
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    polygon2 P = {};
    P.VertexCount = 0;
    P.Vertices = PushArray(TempMem.Arena, MAX_VERTEX_COUNT, v2);

    for(u32 Index = 0;
        Index < GameMode->PolygonCount;
        ++Index)
    {
        world_polygon *Poly = GameMode->Polies + Index;
        
        ConvertWorldPolygonToPolygon2(GameMode->World, BaseP, Poly, &P);
        triangulate_result TResult = ConstrainedDelaunayTriangulate(&P, Arena);

        for(s32 TIndex = GameMode->MeshTriangleCount;
            TIndex < (GameMode->MeshTriangleCount + TResult.TriangleCount);
            ++TIndex)
        {
            triangle *T = TResult.Triangles + (TIndex - GameMode->MeshTriangleCount);
            world_triangle *WorldT = GameMode->MeshTriangles + TIndex;
            WorldT->V1 = MapIntoTileSpace(GameMode->World, *BaseP, T->Vertices[0]);
            WorldT->V2 = MapIntoTileSpace(GameMode->World, *BaseP, T->Vertices[1]);
            WorldT->V3 = MapIntoTileSpace(GameMode->World, *BaseP, T->Vertices[2]);
        }
        GameMode->MeshTriangleCount += TResult.TriangleCount;

        Platform.DeallocateMemory(TResult.Triangles);
        Platform.DeallocateMemory(TResult.Adjacencies);
    }

    EndTemporaryMemory(TempMem);
}

struct sub_region_result
{
    s32 Count;
    triangle *Triangles;
};

inline void
TRemoveAt(triangle *Array, s32 Count, s32 Index)
{
    Array[Index] = {};
    for(s32 I = Index;
        I < Count - 1;
        ++I)
    {
        Array[I] = Array[I + 1];
    }

    Array[Count] = {};
}

internal sub_region_result
SubtractRegionFromMesh(editor_mode_game *GameMode, sim_region *SimRegion, triangle *Subtractor,
                       triangle *SubjectTris, s32 *SubjectIndices, s32 *SubjectCount, memory_arena *TempArena)
{
    subtract_result *SubResults = PushArray(TempArena, (*SubjectCount), subtract_result);

    s32 SubCount = 0;
    for(s32 SubjectIndex = 0;
        SubjectIndex < (*SubjectCount);
        ++SubjectIndex)
    {
        triangle *Subject = SubjectTris + SubjectIndex;
        SubResults[SubCount] = SubtractTriangels(Subject, Subtractor, 0.0f, 0.0f, 0.01f, TempArena);
        if((SubResults[SubCount].Set.PolygonCount > 0) || SubResults[SubCount].FullyRemoved)
        {
            world_triangle *WorldT = GameMode->MeshTriangles + SubjectIndices[SubjectIndex];
            *WorldT = {};
            WorldT->Vertices[0].TileX = TILE_CHUNK_UNINITIALIZED;
            SubCount++;
        }
        else
        {
            S32RemoveAt(SubjectIndices, (*SubjectCount), SubjectIndex);
            TRemoveAt(SubjectTris, (*SubjectCount), SubjectIndex);
            (*SubjectCount)--;
            --SubjectIndex;
        }
    }

    sub_region_result Result = {};
    Result.Count = 0;
    Result.Triangles = PushArray(TempArena, 1024, triangle);
    for(s32 SubIndex = 0;
        SubIndex < SubCount;
        ++SubIndex)
    {
        subtract_result *SubResult = SubResults + SubIndex;
        Assert(SubResult->Success);
        if(SubResult->Set.PolygonCount > 0)
        {
            for(s32 PolyIndex = 0;
                PolyIndex < SubResult->Set.PolygonCount;
                ++PolyIndex)
            {
                polygon2 *Poly = SubResult->Set.Polygons + PolyIndex; 
                if(Poly->VertexCount <= 3)
                {
                    if(Poly->HasHoles)
                    {
                        triangulate_result TriangulatedPoly = ConstrainedDelaunayTriangulate(Poly, TempArena);
                        for(s32 TriangleIndex = 0;
                            TriangleIndex < TriangulatedPoly.TriangleCount;
                            ++TriangleIndex)
                        {
                            Result.Triangles[Result.Count].Vertices[0] = TriangulatedPoly.Triangles[TriangleIndex].Vertices[0];
                            Result.Triangles[Result.Count].Vertices[1] = TriangulatedPoly.Triangles[TriangleIndex].Vertices[1];
                            Result.Triangles[Result.Count].Vertices[2] = TriangulatedPoly.Triangles[TriangleIndex].Vertices[2];
                            ++Result.Count;

                        }

                        Platform.DeallocateMemory(TriangulatedPoly.Triangles);
                        Platform.DeallocateMemory(TriangulatedPoly.Adjacencies);

                        Platform.DeallocateMemory(Poly->HoleVertexCounts);
                        Platform.DeallocateMemory(Poly->HolesVertices);
                    }
                    else
                    {
                        Result.Triangles[Result.Count].Vertices[0] = Poly->Vertices[0];
                        Result.Triangles[Result.Count].Vertices[1] = Poly->Vertices[1];
                        Result.Triangles[Result.Count].Vertices[2] = Poly->Vertices[2];
                        ++Result.Count;
                    }
                }
                else
                {
                    triangulate_result TriangulatedPoly = ConstrainedDelaunayTriangulate(Poly, TempArena);
                    for(s32 TriangleIndex = 0;
                        TriangleIndex < TriangulatedPoly.TriangleCount;
                        ++TriangleIndex)
                    {
                        Result.Triangles[Result.Count].Vertices[0] = TriangulatedPoly.Triangles[TriangleIndex].Vertices[0];
                        Result.Triangles[Result.Count].Vertices[1] = TriangulatedPoly.Triangles[TriangleIndex].Vertices[1];
                        Result.Triangles[Result.Count].Vertices[2] = TriangulatedPoly.Triangles[TriangleIndex].Vertices[2];
                        ++Result.Count;

                    }

                    Platform.DeallocateMemory(TriangulatedPoly.Triangles);
                    Platform.DeallocateMemory(TriangulatedPoly.Adjacencies);
                }

                Platform.DeallocateMemory(Poly->Vertices);
            }

            Platform.DeallocateMemory(SubResult->Set.Polygons);
        }
    }

    return(Result);
}

internal void
FindSubjectTris(editor_mode_game *GameMode, sim_region *SimRegion, rectangle2 SubBounds, triangle *SubjectTris,
                s32 *SubjectIndices, s32 *SubjectCount)
{
    for(s32 Index = 0;
        Index < GameMode->MeshTriangleCount;
        ++Index)
    {
        world_triangle *T = GameMode->MeshTriangles + Index; 
        r32 Z = 10.0f;

        triangle *ConvertedT = SubjectTris + (*SubjectCount);
        ConvertedT->Vertices[0] = Subtract(GameMode->World, &T->V1, &SimRegion->Origin);
        ConvertedT->Vertices[1] = Subtract(GameMode->World, &T->V2, &SimRegion->Origin);
        ConvertedT->Vertices[2] = Subtract(GameMode->World, &T->V3, &SimRegion->Origin);
        CalculateTriangleBoundingBox(ConvertedT);

        if(RectanglesIntersect(ConvertedT->Bounds, SubBounds))
        {
            SubjectIndices[(*SubjectCount)] = Index;
            (*SubjectCount)++;
        }
    }
}

internal void
SubtractPolyFromMesh(editor_mode_game *GameMode, sim_region *SimRegion, polygon2 *Region, memory_arena *Arena)
{
    triangulate_result TriangulatedRegion = DelaunayTriangulate(Region, Arena);

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    s32 SubjectCount = 0;
    s32 *SubjectIndices = PushArray(TempMem.Arena, 1024, s32);
    triangle *SubjectTris = PushArray(TempMem.Arena, 1024, triangle);

    for(s32 RegionTIndex = 0;
        RegionTIndex < TriangulatedRegion.TriangleCount;
        ++RegionTIndex)
    {
        triangle *RegionT = TriangulatedRegion.Triangles + RegionTIndex;

        CalculateTriangleBoundingBox(RegionT);
        FindSubjectTris(GameMode, SimRegion, RegionT->Bounds, SubjectTris, SubjectIndices, &SubjectCount);

        temporary_memory SubTempMem = BeginTemporaryMemory(TempMem.Arena);
        sub_region_result SubResult = SubtractRegionFromMesh(GameMode, SimRegion, RegionT, SubjectTris,
                                                             SubjectIndices, &SubjectCount, SubTempMem.Arena);

        s32 ReplacedCount = 0;
        for(s32 ResultTIndex = 0;
            ResultTIndex < SubResult.Count;
            ++ResultTIndex)
        {
            triangle *ResultT = SubResult.Triangles + ResultTIndex;
            world_triangle *WorldT = 0;
            if(ResultTIndex < SubjectCount)
            {
                s32 SubjectIndex = SubjectIndices[ResultTIndex];
                WorldT = GameMode->MeshTriangles + SubjectIndex; 
                ++ReplacedCount;
            }
            else
            {
                if(GameMode->FreeIndexCount > 0)
                {
                    WorldT = GameMode->MeshTriangles + GameMode->FreeTriangleIndices[--GameMode->FreeIndexCount]; 
                }
                else
                {
                    WorldT = GameMode->MeshTriangles + GameMode->MeshTriangleCount++; 
                }
            }

            WorldT->Vertices[0] = MapIntoTileSpace(GameMode->World, SimRegion->Origin, ResultT->Vertices[0]);
            WorldT->Vertices[1] = MapIntoTileSpace(GameMode->World, SimRegion->Origin, ResultT->Vertices[1]);
            WorldT->Vertices[2] = MapIntoTileSpace(GameMode->World, SimRegion->Origin, ResultT->Vertices[2]);
        }

        if(ReplacedCount < SubjectCount)
        {
            for(s32 I = ReplacedCount;
                I < SubjectCount;
                ++I)
            {
                GameMode->FreeTriangleIndices[GameMode->FreeIndexCount++] = SubjectIndices[I];
            }
        }

        SubjectCount = 0;
        
        EndTemporaryMemory(SubTempMem);
    }

    EndTemporaryMemory(TempMem);
}

struct t_adj_pair
{
    s32 T;
    s32 Adj;
};

inline b32
IsCollinear(v2 a, v2 b, v2 c, f32 Epsilon) 
{
    b32 Result = false;

    v2 ab = b - a;
    v2 bc = c - b;
    
    f32 CrossProduct = Cross(ab, bc);
    f32 L = Length(ab)*Length(bc);

    Result = (AbsoluteValue(CrossProduct) < Epsilon*L);

    return(Result);
}

inline void
SimplifyPolygon(polygon2 *Poly, memory_arena *TempArena)
{
    if(Poly->VertexCount > 3)
    {
        s32 PointCount = 0;
        v2 *Points = PushArray(TempArena, Poly->VertexCount, v2);

        Points[PointCount++] = Poly->Vertices[0];

        for(s32 I = 1;
            I < (Poly->VertexCount - 1);
            ++I)
        {
            v2 Prev = Poly->Vertices[I - 1];
            v2 Cur = Poly->Vertices[I];
            v2 Next = Poly->Vertices[I + 1];

            if(!IsCollinear(Prev, Cur, Next, 0.001f))
            {
                Points[PointCount++] = Cur;
            }
        }

        Poly->VertexCount = PointCount;
        Copy(sizeof(v2)*PointCount, Points, Poly->Vertices);
    }
}

internal void
MergeTriangels(render_group *RenderGroup, object_transform *Flat, editor_mode_game *GameMode, world_position *BaseP, memory_arena *Arena)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    s32 TCount = GameMode->MeshTriangleCount;
    triangle *Triangles = PushArray(TempMem.Arena, TCount, triangle);
    triangle_adjs *AdjArray = PushArray(TempMem.Arena, TCount, triangle_adjs);
    b32 *IsMerged = PushArray(TempMem.Arena, TCount, b32);
    for(s32 I = 0;
        I < TCount;
        ++I)
    {
        world_triangle *WorldT = GameMode->MeshTriangles + I;
        if(IsValid(WorldT->V1))
        {
            triangle *T = Triangles + I;
            triangle_adjs *Adj = AdjArray + I;

            T->Vertices[0] = Subtract(GameMode->World, &WorldT->V1, BaseP);
            T->Vertices[1] = Subtract(GameMode->World, &WorldT->V2, BaseP);
            T->Vertices[2] = Subtract(GameMode->World, &WorldT->V3, BaseP);

            Adj->AdjV1V2 = WorldT->Adj.AdjV1V2;
            Adj->AdjV2V3 = WorldT->Adj.AdjV2V3;
            Adj->AdjV3V1 = WorldT->Adj.AdjV3V1;
        }
        else
        {
            IsMerged[I] = true;
        }
    }

    s32 Count = 0;
    t_adj_pair *ToCheckStack = PushArray(TempMem.Arena, 512, t_adj_pair);

    s32 PolygonCount = 0;
    polygon2 *ResultPolygons = PushArray(TempMem.Arena, 512, polygon2);
    for(s32 I = 0;
        I < 512;
        ++I)
    {
        polygon2 *P = ResultPolygons + I;
        P->Vertices = PushArray(TempMem.Arena, MAX_VERTEX_COUNT, v2);
    }

    polygon2 TempPoly = {};
    TempPoly.Vertices = PushArray(TempMem.Arena, MAX_VERTEX_COUNT, v2);

    
    for(s32 TIndex = 0;
        TIndex < TCount;
        ++TIndex)
    {
        if(!IsMerged[TIndex])
        {
            IsMerged[TIndex] = true;
            s32 CurrentTIndex = TIndex;
            polygon2 *CurrentP = ResultPolygons + PolygonCount++;
            CurrentP->VertexCount = 3;
            CurrentP->Vertices[0] = Triangles[TIndex].Vertices[0];
            CurrentP->Vertices[1] = Triangles[TIndex].Vertices[1];
            CurrentP->Vertices[2] = Triangles[TIndex].Vertices[2];

            ToCheckStack[Count++] = {CurrentTIndex, AdjArray[TIndex].AdjV1V2};
            ToCheckStack[Count++] = {CurrentTIndex, AdjArray[TIndex].AdjV2V3};
            ToCheckStack[Count++] = {CurrentTIndex, AdjArray[TIndex].AdjV3V1};

            while(Count != 0)
            {
                t_adj_pair TAdjPair = ToCheckStack[--Count];
                if(TAdjPair.Adj >= 0)
                {
                    TempPoly.VertexCount = CurrentP->VertexCount;
                    Copy(sizeof(v2)*CurrentP->VertexCount, CurrentP->Vertices, TempPoly.Vertices);

                    triangle *TestT = Triangles + TAdjPair.Adj;
                    s32 SharedCount = 0;
                    s32 PolyEdgeVertices[2] = {-1, -1};
                    s32 TriEdgeVertices[2] = {-1, -1};
                    for(s32 I = 0; I < CurrentP->VertexCount; ++I)
                    {
                        for(s32 J = 0; J < 3; ++J)
                        {
                            if(PointsAreEqual(CurrentP->Vertices[I], TestT->Vertices[J], 0.01f))
                            {
                                PolyEdgeVertices[SharedCount] = I;
                                TriEdgeVertices[SharedCount++] = J;
                                break;
                            }
                        }

                        if(SharedCount == 2)
                        {
                            break;
                        }
                    }

                    Assert(SharedCount == 2);
                    
                    v2 p = {};
                    for(s32 I = 0; I < 3; ++I)
                    {
                        if((I != TriEdgeVertices[0]) && (I != TriEdgeVertices[1]))
                        {
                            p = TestT->Vertices[I];
                            break;
                        }
                    }

                    s32 Insert = (PolyEdgeVertices[0] > PolyEdgeVertices[1]) ? PolyEdgeVertices[0] : PolyEdgeVertices[1];
                    if(((PolyEdgeVertices[0] == 0) && (PolyEdgeVertices[1] == (CurrentP->VertexCount - 1))) ||
                       ((PolyEdgeVertices[1] == 0) && (PolyEdgeVertices[0] == (CurrentP->VertexCount - 1))))
                    {
                        Insert = CurrentP->VertexCount;
                    }

                    InsertPointBetween(&TempPoly, p, Insert);

                    for(s32 I = 0; I < 3; ++I)
                    {
                        if(AdjArray[TAdjPair.T].Adjacencies[I] == TAdjPair.Adj)
                        {
                            AdjArray[TAdjPair.T].Adjacencies[I] = -1;
                            break;
                        }
                    }

                    for(s32 I = 0; I < 3; ++I)
                    {
                        if(AdjArray[TAdjPair.Adj].Adjacencies[I] == TAdjPair.T)
                        {
                            AdjArray[TAdjPair.Adj].Adjacencies[I] = -1;
                            break;
                        }
                    }
            
                    if(IsConvex(&TempPoly))
                    {
                        CurrentP->VertexCount = TempPoly.VertexCount;
                        Copy(sizeof(v2)*TempPoly.VertexCount, TempPoly.Vertices, CurrentP->Vertices);

                        if(AdjArray[TAdjPair.Adj].AdjV1V2 > -1)
                        {
                            ToCheckStack[Count++] = {TAdjPair.Adj, AdjArray[TAdjPair.Adj].AdjV1V2};
                        }

                        if(AdjArray[TAdjPair.Adj].AdjV2V3 > -1)
                        {
                            ToCheckStack[Count++] = {TAdjPair.Adj, AdjArray[TAdjPair.Adj].AdjV2V3};
                        }

                        if(AdjArray[TAdjPair.Adj].AdjV3V1 > -1)
                        {
                            ToCheckStack[Count++] = {TAdjPair.Adj, AdjArray[TAdjPair.Adj].AdjV3V1};
                        }

                        IsMerged[TAdjPair.Adj] = true;
                    }
                }
            }
        }
    }

#if 1
//    GameMode->MeshTriangleCount = 0;
//    GameMode->FreeIndexCount = 0;
    world_polygon *NewPolygons = PushArray(TempMem.Arena, PolygonCount, world_polygon);
    for(s32 I = 0;
        I < PolygonCount;
        ++I)
    {
        polygon2 *Poly = ResultPolygons + I;
        TRISUBRemoveDublicatPoints(Poly);

        for(s32 I = 0;
            I < Poly->VertexCount;
            ++I)
        {
            v2 Prev = Poly->Vertices[(I - 1 + Poly->VertexCount) % Poly->VertexCount];
            v2 Cur = Poly->Vertices[I];
            v2 Next = Poly->Vertices[(I + 1) % Poly->VertexCount];

            f32 Area = TriangleSignedArea(Prev, Cur, Next);
            if(Area < 0.001f)
            {
                RemoveAt(Poly, I);
                --I;
            }
        }
#if 0
        triangulate_result TResult = ConstrainedDelaunayTriangulate(Poly, TempMem.Arena);

        for(s32 TIndex = GameMode->MeshTriangleCount;
            TIndex < (GameMode->MeshTriangleCount + TResult.TriangleCount);
            ++TIndex)
        {
            triangle *T = TResult.Triangles + (TIndex - GameMode->MeshTriangleCount);
            world_triangle *WorldT = GameMode->MeshTriangles + TIndex;
            WorldT->V1 = MapIntoTileSpace(GameMode->World, *BaseP, T->Vertices[0]);
            WorldT->V2 = MapIntoTileSpace(GameMode->World, *BaseP, T->Vertices[1]);
            WorldT->V3 = MapIntoTileSpace(GameMode->World, *BaseP, T->Vertices[2]);
        }
        GameMode->MeshTriangleCount += TResult.TriangleCount;

        Platform.DeallocateMemory(TResult.Triangles);
        Platform.DeallocateMemory(TResult.Adjacencies);
#endif
    }
#endif
    
#if 1
    for(s32 I = 0;
        I < PolygonCount;
        ++I)
    {
        polygon2 *Poly = ResultPolygons + I;
        
        triangulate_result TResult = ConstrainedDelaunayTriangulate(Poly, TempMem.Arena);
        for(s32 J = 0;
            J < TResult.TriangleCount;
            ++J)
        {
            PushTriangle(RenderGroup, Flat, TResult.Triangles[J], 70.0f, V4(DebugColorTable[I % ArrayCount(DebugColorTable)], 1.0f));
        }

        for(s32 M = 0; M < Poly->VertexCount; ++M)
        {
            PushRect(RenderGroup, Flat, V3(Poly->Vertices[M], 100.0f), V2(0.15f, 0.15f),
                     V4(DebugColorTable[(I + 1) % ArrayCount(DebugColorTable)], 1.0f));
        }
        
        Platform.DeallocateMemory(TResult.Triangles);
        Platform.DeallocateMemory(TResult.Adjacencies);
    }
#endif    
    EndTemporaryMemory(TempMem);
}

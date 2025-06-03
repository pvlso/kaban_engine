/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

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

inline void
ConvertWorldPolygonToPolygon2d(world *World, world_position *BaseP, world_polygon *A, polygon2d *Dest)
{
    for(s32 VertexIndex = 0;
        VertexIndex < A->VertexCount;
        ++VertexIndex)
    {
        world_position *PolygonPoint0 = A->Vertices + VertexIndex;
        v2d P = V2d(Subtract(World, PolygonPoint0, BaseP));
        Dest->Vertices[VertexIndex] = P;
    }

    Dest->VertexCount = A->VertexCount;
}

inline void
AddVertex(engine_map_editor *MapEditor, world_polygon *Polygon, world *World, world_position TileP)
{
    if((Polygon->VertexCount + 1) < MAX_VERTEX_COUNT)
    {
        if(!MapEditor->ChosenVertex)
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
                    MapEditor->ChosenVertex = Vertex;
                    break;
                }
            }

            if(!MapEditor->ChosenVertex)
            {
                Polygon->Vertices[Polygon->VertexCount++] = TileP;
            }
        }
        else
        {
            MapEditor->ChosenVertex = 0;
        }
    }
}

inline void
RemoveVertex(engine_map_editor *MapEditor, world_polygon *Polygon, world *World, world_position TileP)
{
    if(!MapEditor->ChosenVertex)
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
StartNewPolygon(engine_map_editor *MapEditor)
{
    MapEditor->CurrentPolygon = MapEditor->Polies + MapEditor->PolygonCount;
    ++MapEditor->CurrentPolygonIndex;
    ++MapEditor->PolygonCount;
}

inline void
ResetPolygon(world_polygon *Polygon)
{
    ZeroArray(Polygon->VertexCount, Polygon->Vertices);
    Polygon->VertexCount = 0;
}

inline void
DeletePolygon(engine_map_editor *MapEditor)
{
    if(MapEditor->CurrentPolygonIndex != 0)
    {
        --MapEditor->CurrentPolygonIndex;
        --MapEditor->PolygonCount;

        MapEditor->CurrentPolygon = MapEditor->Polies + MapEditor->CurrentPolygonIndex;
    }
}

#if 0
inline edge *
CreateEdge(v2 V1, v2 V2, edge *Edges, s32 Index)
{
    edge *Result = Edges + Index;
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
FindOrAddEdge(hash_table *Table, edge *Edge, s32 TriangleIndex, memory_arena *Arena)
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
ShareEdge(engine_map_editor *MapEditor, s32 T, s32 Adj)
{
    s32 SharedCount = 0;
    world_triangle *Tri = MapEditor->MeshTriangles + T;
    world_triangle *AdjTri = MapEditor->MeshTriangles + Adj;
    for(s32 I = 0; I < 3; ++I)
    {
        for(s32 J = 0; J < 3; ++J)
        {
            if(AreInSameTile(MapEditor->World, &Tri->Vertices[I], &AdjTri->Vertices[J]))
            {
                if(PointsAreEqual(Tri->Vertices[I].Offset, AdjTri->Vertices[J].Offset, 0.0001f))
                {
                    ++SharedCount;
                }
            }
        }
    }

    b32 Result = (SharedCount == 2);
    return(Result);
}

internal void
BuildAdjacenciesArray(engine_map_editor *MapEditor, sim_region *SimRegion)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(&MapEditor->World->Arena); 

    hash_table EdgeHashTable = {};
    EdgeHashTable.Size = 3*MapEditor->MeshTriangleCount;
    EdgeHashTable.Hash = PushArray(TempMem.Arena, EdgeHashTable.Size, hash_table_entry *);

    edge *Edges = PushArray(TempMem.Arena, 3*MapEditor->MeshTriangleCount, edge);
    
    for(s32 I = 0;
        I < MapEditor->MeshTriangleCount;
        ++I)
    {
        world_triangle *T = MapEditor->MeshTriangles + I;
        if(IsValid(T->V1))
        {
            v2 V1 = Subtract(MapEditor->World, &T->V1, &SimRegion->Origin);
            v2 V2 = Subtract(MapEditor->World, &T->V2, &SimRegion->Origin);
            v2 V3 = Subtract(MapEditor->World, &T->V3, &SimRegion->Origin);

            s32 TIndex = 3*I;
            edge *Edge1 = CreateEdge(V1, V2, Edges, TIndex + 0);
            edge *Edge2 = CreateEdge(V2, V3, Edges, TIndex + 1);
            edge *Edge3 = CreateEdge(V3, V1, Edges, TIndex + 2);

            FindOrAddEdge(&EdgeHashTable, Edge1, I, TempMem.Arena);
            FindOrAddEdge(&EdgeHashTable, Edge2, I, TempMem.Arena);
            FindOrAddEdge(&EdgeHashTable, Edge3, I, TempMem.Arena);
        }
    }
    
    for(s32 I = 0;
        I < MapEditor->MeshTriangleCount;
        ++I)
    {
        world_triangle *T = MapEditor->MeshTriangles + I;
        if(IsValid(T->V1))
        {

            s32 TIndex = 3*I;
            edge *Edge1 = Edges + TIndex + 0;
            edge *Edge2 = Edges + TIndex + 1;
            edge *Edge3 = Edges + TIndex + 2;

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

#if EDITOR_SLOW
    for(s32 I = 0; I < MapEditor->MeshTriangleCount; ++I)
    {
        for(s32 J = 0; J < 3; ++J)
        {
            s32 AdjIndex = MapEditor->MeshTriangles[I].Adj.Adjacencies[J];
            if(AdjIndex >= 0)
            {
                Assert(ShareEdge(MapEditor, I, AdjIndex))
            }
        }
    }
#endif
    
    EndTemporaryMemory(TempMem);
}
#endif

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

#if 0
internal sub_region_result
SubtractRegionFromMesh(engine_map_editor *MapEditor, sim_region *SimRegion, triangle *Subtractor,
                       triangle *SubjectTris, s32 *SubjectIndices, s32 *SubjectCount, memory_arena *TempArena)
{
    subtract_result *SubResults = PushArray(TempArena, (*SubjectCount), subtract_result);

    s32 SubCount = 0;
    for(s32 SubjectIndex = 0;
        SubjectIndex < (*SubjectCount);
        ++SubjectIndex)
    {
        triangle *Subject = SubjectTris + SubjectIndex;
        SubResults[SubCount] = SubtractTriangels(Subject, Subtractor, 0.0f, 0.0f, 0.0001f, TempArena);
        if((SubResults[SubCount].Set.PolygonCount > 0) || SubResults[SubCount].FullyRemoved)
        {
            world_triangle *WorldT = MapEditor->MeshTriangles + SubjectIndices[SubjectIndex];
            *WorldT = {};
            WorldT->Vertices[0].TileX = TILE_CHUNK_UNINITIALIZED;
            WorldT->Adj.Adjacencies[0] = -1;
            WorldT->Adj.Adjacencies[1] = -1;
            WorldT->Adj.Adjacencies[2] = -1;
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
FindSubjectTris(engine_map_editor *MapEditor, sim_region *SimRegion, rectangle2 SubBounds, triangle *SubjectTris,
                s32 *SubjectIndices, s32 *SubjectCount)
{
    game_mode_world *WorldState = MapEditor->WorldState;
    
    for(s32 Index = 0;
        Index < MapEditor->MeshTriangleCount;
        ++Index)
    {
        world_triangle *T = MapEditor->MeshTriangles + Index; 
        r32 Z = 10.0f;

        triangle *ConvertedT = SubjectTris + (*SubjectCount);
        ConvertedT->Vertices[0] = Subtract(WorldState->World, &T->V1, &SimRegion->Origin);
        ConvertedT->Vertices[1] = Subtract(WorldState->World, &T->V2, &SimRegion->Origin);
        ConvertedT->Vertices[2] = Subtract(WorldState->World, &T->V3, &SimRegion->Origin);
        CalculateTriangleBoundingBox(ConvertedT);

        if(RectanglesIntersect(ConvertedT->Bounds, SubBounds))
        {
            SubjectIndices[(*SubjectCount)] = Index;
            (*SubjectCount)++;
        }
    }
}

internal void
SubtractPolyFromMesh(engine_map_editor *MapEditor, sim_region *SimRegion, polygon2 *Region, memory_arena *Arena)
{
    triangulate_result TriangulatedRegion = DelaunayTriangulate(Region, Arena);

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    s32 SubjectCount = 0;
    s32 *SubjectIndices = PushArray(TempMem.Arena, 1024, s32);
    triangle *SubjectTris = PushArray(TempMem.Arena, 1024, triangle);

    for(s32 RegionTIndex = 0;
        RegionTIndex < 1;//TriangulatedRegion.TriangleCount;
        ++RegionTIndex)
    {
        triangle *RegionT = TriangulatedRegion.Triangles + RegionTIndex;
        
        CalculateTriangleBoundingBox(RegionT);
        FindSubjectTris(MapEditor, SimRegion, RegionT->Bounds, SubjectTris, SubjectIndices, &SubjectCount);

        temporary_memory SubTempMem = BeginTemporaryMemory(TempMem.Arena);
        sub_region_result SubResult = SubtractRegionFromMesh(MapEditor, SimRegion, RegionT, SubjectTris,
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
                WorldT = MapEditor->MeshTriangles + SubjectIndex; 
                ++ReplacedCount;
            }
            else
            {
                if(MapEditor->FreeIndexCount > 0)
                {
                    WorldT = MapEditor->MeshTriangles + MapEditor->FreeTriangleIndices[--MapEditor->FreeIndexCount]; 
                }
                else
                {
                    WorldT = MapEditor->MeshTriangles + MapEditor->MeshTriangleCount++; 
                }
            }

            WorldT->Vertices[0] = MapIntoTileSpace(MapEditor->WorldState->World, SimRegion->Origin, ResultT->Vertices[0]);
            WorldT->Vertices[1] = MapIntoTileSpace(MapEditor->WorldState->World, SimRegion->Origin, ResultT->Vertices[1]);
            WorldT->Vertices[2] = MapIntoTileSpace(MapEditor->WorldState->World, SimRegion->Origin, ResultT->Vertices[2]);
        }

        if(ReplacedCount < SubjectCount)
        {
            for(s32 I = ReplacedCount;
                I < SubjectCount;
                ++I)
            {
                MapEditor->FreeTriangleIndices[MapEditor->FreeIndexCount++] = SubjectIndices[I];
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

internal void
MergeTriangels(render_group *RenderGroup, object_transform *Flat, engine_map_editor *MapEditor, world_position *BaseP, memory_arena *Arena)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    s32 TCount = MapEditor->MeshTriangleCount;
    triangle *Triangles = PushArray(TempMem.Arena, TCount, triangle);
    triangle_adjs *AdjArray = PushArray(TempMem.Arena, TCount, triangle_adjs);
    b32 *IsMerged = PushArray(TempMem.Arena, TCount, b32);
    for(s32 I = 0;
        I < TCount;
        ++I)
    {
        world_triangle *WorldT = MapEditor->MeshTriangles + I;
        if(IsValid(WorldT->V1))
        {
            triangle *T = Triangles + I;
            triangle_adjs *Adj = AdjArray + I;

            T->Vertices[0] = Subtract(MapEditor->WorldState->World, &WorldT->V1, BaseP);
            T->Vertices[1] = Subtract(MapEditor->WorldState->World, &WorldT->V2, BaseP);
            T->Vertices[2] = Subtract(MapEditor->WorldState->World, &WorldT->V3, BaseP);

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
                    Copy(sizeof(v2d)*CurrentP->VertexCount, CurrentP->Vertices, TempPoly.Vertices);

                    triangle *TestT = Triangles + TAdjPair.Adj;
                    s32 SharedCount = 0;
                    s32 PolyEdgeVertices[2] = {-1, -1};
                    s32 TriEdgeVertices[2] = {-1, -1};
                    for(s32 I = 0; I < CurrentP->VertexCount; ++I)
                    {
                        for(s32 J = 0; J < 3; ++J)
                        {
                            if(PointsAreEqual(CurrentP->Vertices[I], TestT->Vertices[J], 0.0001f))
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

    world_polygon *NewPolygons = PushArray(TempMem.Arena, PolygonCount, world_polygon);
    for(s32 I = 0;
        I < PolygonCount;
        ++I)
    {
        polygon2 *Poly = ResultPolygons + I;
        world_polygon *WorldPoly = NewPolygons + I;

        WorldPoly->VertexCount = Poly->VertexCount;
        WorldPoly->Vertices = PushArray(TempMem.Arena, MAX_VERTEX_COUNT, world_position);
        for(s32 J = 0;
            J < Poly->VertexCount;
            ++J)
        {
            WorldPoly->Vertices[J] = MapIntoTileSpace(MapEditor->WorldState->World, *BaseP, Poly->Vertices[J]);
        }
    }

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

        Platform.DeallocateMemory(TResult.Triangles);
        Platform.DeallocateMemory(TResult.Adjacencies);
    }
#endif    
    EndTemporaryMemory(TempMem);
}

internal void
MergeTriangels(render_group *RenderGroup, object_transform *Flat, engine_map_editor *MapEditor,
               triangle *Triangles, triangle_adjs *AdjArray, s32 TriangleCount,
               world_position *BaseP, memory_arena *Arena)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    b32 *IsMerged = PushArray(TempMem.Arena, TriangleCount, b32);

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
        TIndex < TriangleCount;
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
                    Copy(sizeof(v2d)*CurrentP->VertexCount, CurrentP->Vertices, TempPoly.Vertices);

                    triangle *TestT = Triangles + TAdjPair.Adj;
                    s32 SharedCount = 0;
                    s32 PolyEdgeVertices[2] = {-1, -1};
                    s32 TriEdgeVertices[2] = {-1, -1};
                    for(s32 I = 0; I < CurrentP->VertexCount; ++I)
                    {
                        for(s32 J = 0; J < 3; ++J)
                        {
                            if(PointsAreEqual(CurrentP->Vertices[I], TestT->Vertices[J], 0.0001f))
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

    world_polygon *NewPolygons = PushArray(TempMem.Arena, PolygonCount, world_polygon);
    for(s32 I = 0;
        I < PolygonCount;
        ++I)
    {
        polygon2 *Poly = ResultPolygons + I;
        world_polygon *WorldPoly = NewPolygons + I;

        WorldPoly->VertexCount = Poly->VertexCount;
        WorldPoly->Vertices = PushArray(TempMem.Arena, MAX_VERTEX_COUNT, world_position);
        for(s32 J = 0;
            J < Poly->VertexCount;
            ++J)
        {
            WorldPoly->Vertices[J] = MapIntoTileSpace(MapEditor->WorldState->World, *BaseP, Poly->Vertices[J]);
        }
    }

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

        Platform.DeallocateMemory(TResult.Triangles);
        Platform.DeallocateMemory(TResult.Adjacencies);
    }
#endif    
    EndTemporaryMemory(TempMem);
}
 
inline void
RemoveDublicatPoints(v2 *Vertices, s32 *Count)
{
    for(s32 I = 0;
        I < (*Count);
        ++I)
    {
        for(s32 J = I + 1;
            J < (*Count);
            ++J)
        {
            if(TRISUBPointsAreEqual(Vertices[I], Vertices[J]))
            {
                Vertices[J] = {};
                for(s32 k = J;
                    k < ((*Count) - 1);
                    ++k)
                {
                    Vertices[k] = Vertices[k + 1]; 
                }

                (*Count)--;
                --J;
            }            
        }
    }

    Vertices[(*Count)] = {};
}

internal void
TriangulatePolygons(render_group *RenderGroup, object_transform *Flat, engine_map_editor *MapEditor, world_position *BaseP, memory_arena *Arena)
{
    TIMED_FUNCTION();

    MapEditor->MeshTriangleCount = 0;
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    polygon2 P = {};
    P.VertexCount = 0;
    P.Vertices = PushArray(TempMem.Arena, MAX_VERTEX_COUNT, v2);
#if 0
    s32 VertexMaxCount = 0;
    for(u32 I = 0;
        I < MapEditor->PolygonCount;
        ++I)
    {
        world_polygon *Poly = MapEditor->Polies + I;
        VertexMaxCount += Poly->VertexCount;
    }

    v2 *Vertices = PushArray(TempMem.Arena, VertexMaxCount, v2);
    v2 *At = Vertices;
    
    for(u32 Index = 0;
        Index < MapEditor->PolygonCount;
        ++Index)
    {
        world_polygon *Poly = MapEditor->Polies + Index;
        ConvertWorldPolygonToPolygon2(MapEditor->WorldState->World, BaseP, Poly, &P);

        Copy(sizeof(v2)*Poly->VertexCount, P.Vertices, At);
        At += Poly->VertexCount;
    }

    RemoveDublicatPoints(Vertices, &VertexMaxCount);
    
    int a = 0;
#endif
    
#if 1    
    for(u32 Index = 0;
        Index < MapEditor->PolygonCount;
        ++Index)
    {
        world_polygon *Poly = MapEditor->Polies + Index;
        ConvertWorldPolygonToPolygon2(MapEditor->WorldState->World, BaseP, Poly, &P);

//        triangulate_result TResult = ConstrainedDelaunayTriangulate(&P, Arena);
        triangulate_result TResult = DelaunayTriangulate(&P, Arena);

        for(s32 TIndex = MapEditor->MeshTriangleCount;
            TIndex < (MapEditor->MeshTriangleCount + TResult.TriangleCount);
            ++TIndex)
        {
            triangle *T = TResult.Triangles + (TIndex - MapEditor->MeshTriangleCount);
            world_triangle *WorldT = MapEditor->MeshTriangles + TIndex;
            WorldT->V1 = MapIntoTileSpace(MapEditor->WorldState->World, *BaseP, T->Vertices[0]);
            WorldT->V2 = MapIntoTileSpace(MapEditor->WorldState->World, *BaseP, T->Vertices[1]);
            WorldT->V3 = MapIntoTileSpace(MapEditor->WorldState->World, *BaseP, T->Vertices[2]);
        }

//        MergeTriangels(RenderGroup, Flat, MapEditor,
//                       TResult.Triangles, TResult.Adjacencies, TResult.TriangleCount,
//                       BaseP, TempMem.Arena);

        MapEditor->MeshTriangleCount += TResult.TriangleCount;

        Platform.DeallocateMemory(TResult.Triangles);
        Platform.DeallocateMemory(TResult.Adjacencies);
    }
#endif
    
    EndTemporaryMemory(TempMem);
}
#endif

inline void
InitNavPolyNode(nav_poly_node *Node, s32 Index, world_polygon_list *Ptr,
                world_position P, rectangle2i Bounds)
{
    Node->TileP = P;
    Node->Index = Index;
    Node->PolyPtr = Ptr;
    Node->Visited = false;
    Node->Bounds = Bounds;
    Node->Parent = 0;
}

internal void
PartitionPolies(engine_map_editor *MapEditor, sim_region *SimRegion,
                memory_arena *Arena)
{
    TIMED_BLOCK("PARTITION");

    polygon2 P = {};
    P.VertexCount = 0;
    P.Vertices = PushArray(Arena, MAX_VERTEX_COUNT, v2);

    epp_poly_list *Free = 0;
    epp_poly_list In = {};
    DLIST_INIT(&In);

    for(u32 Index = 0;
        Index < MapEditor->PolygonCount;
        ++Index)
    {
        world_polygon *Poly = MapEditor->Polies + Index;
        ConvertWorldPolygonToPolygon2(MapEditor->WorldState->World, &SimRegion->Origin, Poly, &P);

        epp_poly_list *New = 0;
        POLY_FREELIST_ALLOCATE(New, Free, PushStruct(Arena, epp_poly_list));
        epp_poly *poly = &New->Poly;
        poly->numpoints = Poly->VertexCount;
        poly->points = PushArray(Arena, poly->numpoints, epp_point);
        for(s32 I = 0;
            I < poly->numpoints;
            ++I)
        {
            poly->points[I].x = P.Vertices[I].x;
            poly->points[I].y = P.Vertices[I].y;
            poly->points[I].id = I;
        }

        EPPSetOrientation(poly, EPP_ORIENTATION_CCW);
        DLIST_INSERT(&In, New);                            
    }

    epp_poly_list Result = {};
    Result.Next = &Result;
    Result.Prev = &Result;
                                
    int r = EPPConvexPartitionHM(&In, &Result, &Free, Arena);
    for(epp_poly_list *Iter = Result.Next;
        Iter != &Result;
        Iter = Iter->Next)
    {
        epp_poly P = Iter->Poly;
        world_polygon_list *New = (world_polygon_list *)Platform.AllocateMemory(sizeof(world_polygon_list));
        DLIST_INSERT(&MapEditor->MeshPolygonsSentinal, New);                            
        New->Poly.VertexCount = P.numpoints;
        New->Poly.Vertices = (world_position *)Platform.AllocateMemory(sizeof(world_position)*New->Poly.VertexCount);

        for(s32 J = 0; J < P.numpoints; ++ J)
            New->Poly.Vertices[J] = MapIntoTileSpace(MapEditor->WorldState->World, SimRegion->Origin, V2(P.points[J].x, P.points[J].y));
    }
}


inline r32
DistanceBetween(world *World, nav_poly_node *NodeA, nav_poly_node *NodeB)
{
    v2 Delta = Subtract(World, &NodeA->TileP, &NodeB->TileP);

    r32 Result = SquareRoot(Square(Delta.x) + Square(Delta.y));

    return(Result);
}

internal nav_poly_node *
SolvePolyAStar(engine_map_editor *MapEditor, nav_poly_node *Start, nav_poly_node *End)
{
    TIMED_FUNCTION();

    if(Start && End)
    {
        for(u32 NodeIndex = 0;
            NodeIndex < MapEditor->PolyNodeCount;
            ++NodeIndex)
        {
            nav_poly_node *Node = MapEditor->PolyNodes + NodeIndex;
            Node->Visited = false;
            Node->GlobalGoal = Real32Maximum;
            Node->LocalGoal = Real32Maximum;
            Node->Parent = 0;
        }

        nav_poly_node *CurrentNode = Start;
        CurrentNode->LocalGoal = 0.0f;
        CurrentNode->GlobalGoal = DistanceBetween(MapEditor->WorldState->World, Start, End);

        heap *Heap = &MapEditor->MinPolyNodeHeap;

        sort_entry Key = {};
        Key.Index = Start->Index;
        Key.SortKey = Start->GlobalGoal;
        MinHeapInsertNode(Heap, Key);

        while((Heap->Size != 0) && (CurrentNode != End))
        {
            nav_poly_node *TestNode = MapEditor->PolyNodes + Heap->Nodes[0].Index;
            while((TestNode->Visited) && (Heap->Size != 0))
            {
                MinHeapExtractNode(Heap);
                TestNode = MapEditor->PolyNodes + Heap->Nodes[0].Index;
            }

            if(Heap->Size == 0)
            {
                break;
            }

            CurrentNode = MapEditor->PolyNodes + Heap->Nodes[0].Index; 
            CurrentNode->Visited = true;

            for(s32 NeighbourIndex = 0;
                NeighbourIndex < CurrentNode->NeighbourCount;
                ++NeighbourIndex)
            {
                nav_poly_node *NeighbourNode = CurrentNode->Neighbours[NeighbourIndex];
                if(NeighbourNode)
                {
                    if((!NeighbourNode->Visited))
                    {
                        sort_entry Key = {};
                        Key.Index = NeighbourNode->Index;
                        Key.SortKey = NeighbourNode->GlobalGoal;

                        r32 LowerGoal = CurrentNode->LocalGoal + DistanceBetween(MapEditor->WorldState->World, CurrentNode, NeighbourNode);
                        if(LowerGoal < NeighbourNode->LocalGoal)
                        {
                            NeighbourNode->Parent = CurrentNode;
                            NeighbourNode->LocalGoal = LowerGoal;

                            NeighbourNode->GlobalGoal = (NeighbourNode->LocalGoal +
                                                         DistanceBetween(MapEditor->WorldState->World, NeighbourNode, End));
                            Key.SortKey = NeighbourNode->GlobalGoal;
                        }

                        MinHeapInsertNode(Heap, Key);
                    }
                }
            }
        }

        ZeroArray(Heap->MaxSize, Heap->Nodes);
        Heap->Size = 0;
    }

    return(End);
}

internal s32
StringPull(v2 *Portals, s32 PortalsCount, v2 *Points, s32 MaxPoints)
{
    TIMED_FUNCTION();

    f32 Epsilon = 0.001f;
    // Find straight path.
    s32 PointsCount = 0;

    // Init scan state
    s32 ApexIndex = 0;
    v2 PortalApex = Portals[0];

    s32 LeftIndex = 0;
    v2 PortalLeft = Portals[0];

    s32 RightIndex = 0;
    v2 PortalRight = Portals[1];

    // Add start point.
    Points[0] = PortalApex;
    PointsCount++;

    for(int i = 1; i < PortalsCount && PointsCount < MaxPoints; ++i)
    {
        v2 left = Portals[i*2];
        v2 right = Portals[i*2 + 1];

        // Update right vertex.
        if(TriangleArea2(PortalApex, PortalRight, right) <= 0.0f)
        {
            if(PointsAreEqual(PortalApex, PortalRight, Epsilon) ||
               TriangleArea2(PortalApex, PortalLeft, right) > 0.0f)
            {
                // Tighten the funnel.
                PortalRight = right;
                RightIndex = i;
            }
            else
            {
                // Right over left, insert left to path and restart scan from portal left point.
                Points[PointsCount++] = PortalLeft;

                // Make current left the new apex.
                PortalApex = PortalLeft;
                ApexIndex = LeftIndex;
                // Reset portal
                PortalLeft = PortalApex;
                PortalRight = PortalApex;

                LeftIndex = ApexIndex;
                RightIndex = ApexIndex;

                // Restart scan
                i = ApexIndex;
                continue;
            }
        }

        // Update left vertex.
        if(TriangleArea2(PortalApex, PortalLeft, left) >= 0.0f)
        {
            if(PointsAreEqual(PortalApex, PortalLeft, Epsilon) ||
               TriangleArea2(PortalApex, PortalRight, left) < 0.0f)
            {
                // Tighten the funnel.
                PortalLeft = left;
                LeftIndex = i;
            }
            else
            {
                // Left over right, insert right to path and restart scan from portal right point.
                Points[PointsCount++] = PortalRight;

                // Make current right the new apex.
                PortalApex = PortalRight;
                ApexIndex = RightIndex;
                // Reset portal
                PortalLeft = PortalApex;
                PortalRight = PortalApex;

                LeftIndex = ApexIndex;
                RightIndex = ApexIndex;

                // Restart scan
                i = ApexIndex;
                continue;
            }
        }
    }

    // Append last point to path.
    if(PointsCount < MaxPoints)
        Points[PointsCount++] = Portals[(PortalsCount - 1)*2];

    return(PointsCount);
}

internal void
PartitionNavigationMesh(engine_map_editor *MapEditor, sim_region *SimRegion, memory_arena *TempArena)
{
    // NOTE(paul): Clear Mesh Polygon List
    for(world_polygon_list *Iter = MapEditor->MeshPolygonsSentinal.Next;
        Iter != &MapEditor->MeshPolygonsSentinal;
        )
    {
        world_polygon_list *T = Iter;
        Iter = T->Next;

        DLIST_REMOVE(T);
        Platform.DeallocateMemory(T->Poly.Vertices);
        Platform.DeallocateMemory(T);
    }
                            
    PartitionPolies(MapEditor, SimRegion, TempArena);

    // NOTE(paul): Clear node neighbours count
    for(u32 I = 0;
        I < MapEditor->PolyNodeCount;
        ++I)
    {
        nav_poly_node *Node = MapEditor->PolyNodes + I;
        Node->NeighbourCount = 0;
    }

    for(u32 I = 0;
        I < MapEditor->EdgeTable.Size;
        ++I)
    {
        hash_table_entry *Scan = MapEditor->EdgeTable.Hash[I];
        while(Scan)
        {
            hash_table_entry *Entry = Scan;
            Scan = Scan->Next;

            Entry->Next = MapEditor->EdgeTable.Free;
            MapEditor->EdgeTable.Free = Entry;
        }
    }
    
    MapEditor->PolyNodeCount = 0;                            

    polygon2 RealPoly = {};
    RealPoly.Vertices = PushArray(TempArena, 128, v2);

    s32 PIndex = 0;
    for(world_polygon_list *Iter = MapEditor->MeshPolygonsSentinal.Next;
        Iter != &MapEditor->MeshPolygonsSentinal;
        Iter = Iter->Next)
    {
        world_polygon *Poly = &Iter->Poly;
        ConvertWorldPolygonToPolygon2(MapEditor->WorldState->World, &SimRegion->Origin, Poly, &RealPoly);
        Iter->RealPoly = RealPoly;

        v2 Center = {};
        f32 SignedArea = 0.0f;
        for(s32 I = 0; I < RealPoly.VertexCount; ++I)
        {
            v2 p0 = RealPoly.Vertices[I];
            v2 p1 = RealPoly.Vertices[(I + 1) % RealPoly.VertexCount];

            f32 C = Cross(p0, p1);
            SignedArea += C;

            Center += (p0 + p1)*C;
        }

        SignedArea *= 0.5f;
        if(AbsoluteValue(SignedArea) > 0)
            Center *= 1.0f / (6.0f*SignedArea);

        rectangle2i Bounds = CalculatePolygonBoundingBox(Poly);

        nav_poly_node *Node = MapEditor->PolyNodes + MapEditor->PolyNodeCount++;                                
        InitNavPolyNode(Node, (MapEditor->PolyNodeCount - 1), Iter,
                        MapIntoTileSpace(MapEditor->WorldState->World, SimRegion->Origin, Center),
                        Bounds);
        PIndex += 1;
    }

    // NOTE(paul): Build conectivity graph
    s32 I1 = 0;
    for(world_polygon_list *Iter = MapEditor->MeshPolygonsSentinal.Next;
        Iter != &MapEditor->MeshPolygonsSentinal;
        Iter = Iter->Next)
    {
        world_polygon *Poly = &Iter->Poly;
        
        s32 Orientation = GetPolygonOrientation(&Iter->RealPoly); 
        Assert(Orientation == POLY_ORIENTATION_CCW)
                                
        s32 I2 = I1 + 1;
        for(world_polygon_list *Iter2 = Iter->Next;
            Iter2 != &MapEditor->MeshPolygonsSentinal;
            Iter2 = Iter2->Next)
        {
            world_polygon *Poly2 = &Iter2->Poly;

            s32 Orientation2 = GetPolygonOrientation(&Iter2->RealPoly); 
            Assert(Orientation2 == POLY_ORIENTATION_CCW)

            b32 Found = false;
            for(s32 I = 0; I < Poly->VertexCount; ++I)
            {
                world_position A1 = Poly->Vertices[I];
                world_position B1 = Poly->Vertices[(I + 1) % Poly->VertexCount];
                hash_key HashKey = {};
                HashKey.WorldEdge.A = A1;
                HashKey.WorldEdge.B = B1;
                
                for(s32 J = 0; J < Poly2->VertexCount; ++J)
                {
                    world_position A2 = Poly2->Vertices[J];
                    world_position B2 = Poly2->Vertices[(J + 1) % Poly2->VertexCount];

                    if(((A1.TileX == B2.TileX) && (A1.TileY == B2.TileY)) &&
                       ((B1.TileX == A2.TileX) && (B1.TileY == A2.TileY)) &&
                       ((A1.Offset.x == B2.Offset.x) && (A1.Offset.y == B2.Offset.y)) &&
                       ((B1.Offset.x == A2.Offset.x) && (B1.Offset.y == A2.Offset.y)))
                    {
                        hash_data Data = {};
                        Data.PolyMeshAdjacency = {(u32)I1, A1, B1, (u32)I2, A2, B2};
                        InsertKey(&MapEditor->EdgeTable, HashKey, Data, &MapEditor->NavMeshArena);

                        nav_poly_node *Poly1Node = MapEditor->PolyNodes + I1;
                        nav_poly_node *Poly2Node = MapEditor->PolyNodes + I2;
                        if(Poly1Node->NeighbourCount == 0)
                        {
                            Poly1Node->Neighbours[Poly1Node->NeighbourCount] = Poly2Node;
                            Poly1Node->NEdge[Poly1Node->NeighbourCount] = {A1, B1};
                            ++Poly1Node->NeighbourCount;
                        }
                        else
                        {
                            b32 IsNew = true;
                            for(s32 NI = 0;
                                NI < Poly1Node->NeighbourCount;
                                ++NI)
                            {
                                nav_poly_node *Test = Poly1Node->Neighbours[NI];
                                if(Test->Index == I2)
                                {
                                    IsNew = false;
                                    break;
                                }
                            }

                            if(IsNew)
                            {
                                Poly1Node->Neighbours[Poly1Node->NeighbourCount] = Poly2Node;
                                Poly1Node->NEdge[Poly1Node->NeighbourCount] = {A1, B1};
                                ++Poly1Node->NeighbourCount;
                            }
                        }

                        if(Poly2Node->NeighbourCount == 0)
                        {
                            Poly2Node->Neighbours[Poly2Node->NeighbourCount] = Poly1Node;
                            Poly2Node->NEdge[Poly2Node->NeighbourCount] = {A1, B1};
                            ++Poly2Node->NeighbourCount;
                        }
                        else
                        {
                            b32 IsNew = true;
                            for(s32 NI = 0;
                                NI < Poly2Node->NeighbourCount;
                                ++NI)
                            {
                                nav_poly_node *Test = Poly2Node->Neighbours[NI];
                                if(Test->Index == I1)
                                {
                                    IsNew = false;
                                    break;
                                }
                            }

                            if(IsNew)
                            {
                                Poly2Node->Neighbours[Poly2Node->NeighbourCount] = Poly1Node;
                                Poly2Node->NEdge[Poly2Node->NeighbourCount] = {A1, B1};
                                ++Poly2Node->NeighbourCount;
                            }
                        }

                        Found = true;
                        break;
                    }
                }

                if(Found)
                    break;
            }
                                    
            ++I2;
        }

        ++I1;
    }
}

internal void
WritePolygons(world_polygon *Polygons, s32 PolygonCount)
{
    FILE *Out;
    fopen_s(&Out, "polygons.nmp", "wb");
    if(Out)
    {
        fwrite(&PolygonCount, sizeof(u32), 1, Out);
        for(s32 PolygonIndex = 0;
            PolygonIndex < PolygonCount;
            ++PolygonIndex)
        {
            world_polygon *Poly = Polygons + PolygonIndex;
            u32 VerticesSize = Poly->VertexCount*sizeof(world_position);
            fwrite(&Poly->VertexCount, sizeof(u32), 1, Out);
            fwrite(Poly->Vertices, VerticesSize, 1, Out);
        }
    }

    fclose(Out);
}

internal void
DrawPolygons(engine_map_editor *MapEditor, render_group *RenderGroup, ui_state *UIState, world *World, world_polygon *Polygons, s32 Count,
             world_position BaseP, s32 CurrentPolygonIndex, memory_arena *Arena)
{
    object_transform Flat = DefaultFlatTransform();
    char Text[32];

    for(s32 Index = 0;
        Index < Count;
        ++Index)
    {
        world_polygon *Polygon = Polygons + Index;
        
        polygon2 TempPoly = {};
        TempPoly.VertexCount = Polygon->VertexCount;
        TempPoly.Vertices = PushArray(Arena, Polygon->VertexCount, v2);
        
        ConvertWorldPolygonToPolygon2(World, &BaseP, Polygon, &TempPoly);

        b32 Clockwise = (PolygonSignedArea2(&TempPoly) < 0.0f);
    
        s32 PrevOffset = Clockwise ? -1 : 1;
        s32 NextOffset = Clockwise ? 1 : -1;

        v4 Color = V4(0, 1, 0, 1);
        v4 VertexColor = V4(1, 0, 0, 1);
        r32 Z = 10.0f;
        if(Index == CurrentPolygonIndex)
        {
            Color = V4(1, 0, 1, 1);
            VertexColor = V4(0, 0.5f, 1, 1);
            Z = 12.0f;
        }

        for(s32 VertexIndex = 0;
            VertexIndex < Polygon->VertexCount;
            ++VertexIndex)
        {
            world_position *Vertex = Polygon->Vertices + VertexIndex;
            v2 Delta = Subtract(World, Vertex, &BaseP);
        
            if(VertexIndex == 0)
            {
                PushRect(RenderGroup, &Flat, V3(Delta, 52.0f), V2(0.1f, 0.1f), V4(1, 0, 1, 1));
            }
            else
            {
                PushRect(RenderGroup, &Flat, V3(Delta, Z), V2(0.1f, 0.1f),
                         (VertexIndex == Polygon->VertexCount - 1) ? V4(DebugColorTable[4], 1) : VertexColor);
            }
        }

        v2 Center = {};
        f32 SignedArea = 0.0f;
        if(MapEditor->ShowNativeIds)
        {
            for(s32 I = 0; I < TempPoly.VertexCount; ++I)
            {
                v2 p0 = TempPoly.Vertices[I];
                v2 p1 = TempPoly.Vertices[(I + 1) % TempPoly.VertexCount];

                f32 C = Cross(p0, p1);
                SignedArea += C;

                Center += (p0 + p1)*C;
            }

            SignedArea *= 0.5f;
            if(AbsoluteValue(SignedArea) > 0)
                Center *= 1.0f / (6.0f*SignedArea);

            PushRect(RenderGroup, &Flat, V3(Center, 30.0f), V2(0.1f, 0.1f), V4(1, 1, 0.5f, 1));

            FormatString(ArrayCount(Text), Text, "%d", Index);
            entity_basis_p_result BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform,
                                                                 &Flat, V3(Center, 0.0f));
            v3 P = Unproject(&UIState->RenderGroup, &Flat, BasisP.P);
            UITextOutAt(UIState, P.xy, Text, 0.8f);
        }
#if 0
        if(Index == CurrentPolygonIndex)
        {
            for(s32 VertexIndex = 0;
                VertexIndex < Polygon->VertexCount;
                ++VertexIndex)
            {
                world_position *VertexPrev = Polygon->Vertices + GetIndex(Polygon->VertexCount, VertexIndex + PrevOffset);
                world_position *VertexCur = Polygon->Vertices + VertexIndex;
                world_position *VertexNext = Polygon->Vertices + GetIndex(Polygon->VertexCount, VertexIndex + NextOffset);
                v2 Delta0 = Subtract(World, VertexPrev, &BaseP);
                v2 Delta1 = Subtract(World, VertexCur, &BaseP);
                v2 Delta2 = Subtract(World, VertexNext, &BaseP);
                
                v2 a = Delta1 - Delta0;
                v2 b = Delta2 - Delta1;
                
                if(Cross(a, b) > 0.0f)
                {

                    v2 Perpendicular = Perp(Delta0 - Delta1);
                    v2 NewP = Delta1 + Normalize(Perpendicular)*10.0f;
                    PushLine(RenderGroup, &Flat, V3(Delta1, Z), V3(NewP, Z));
                    PushRect(RenderGroup, &Flat, V3(Delta1, 50.0f), V2(0.1f, 0.1f), V4(0, 0, 0, 1));
                }
            }
        }
#endif
        if(Polygon->VertexCount > 1)
        {
            for(s32 VertexIndex = 0;
                VertexIndex < (Polygon->VertexCount - 1);
                ++VertexIndex)
            {
                world_position *Vertex0 = Polygon->Vertices + VertexIndex;
                world_position *Vertex1 = Polygon->Vertices + VertexIndex + 1;
                v2 Delta0 = Subtract(World, Vertex0, &BaseP);
                v2 Delta1 = Subtract(World, Vertex1, &BaseP);

                PushLine(RenderGroup, &Flat, V3(Delta0, Z), V3(Delta1, Z), V4(0, 0, 0, 1));
            }

            if(Polygon->VertexCount > 2)
            {
                world_position Vertex0 = Polygon->Vertices[Polygon->VertexCount - 1];
                world_position Vertex1 = Polygon->Vertices[0];
                v2 Delta0 = Subtract(World, &Vertex0, &BaseP);
                v2 Delta1 = Subtract(World, &Vertex1, &BaseP);
                PushLine(RenderGroup, &Flat, V3(Delta0, Z), V3(Delta1, Z), V4(0, 0, 0, 1));
            }
        }
    }
}

internal b32
IsPointInPolygon(render_group *RenderGroup, object_transform *Flat, polygon2 *Poly, v2 P)
{
    b32 Result = false;

    v2 RayP = P - V2(200.0f, 0.0f);
    for(s32 I = 0;
        I < (Poly->VertexCount);
        ++I)
    {
        v2 A = Poly->Vertices[I];
        v2 B = Poly->Vertices[(I + 1) % Poly->VertexCount];

        v2 SectP = {};
        if(LineIntersect(B, A, P, RayP, &SectP) == 1)
            Result = !Result;
    }

//    PushLine(RenderGroup, Flat, V3(P, 34.0f), V3(RayP, 34.0f), V4(1, 0, 1, 1));

    return(Result);
}

internal s32
FindNavPolyNodeForPoint(engine_map_editor *MapEditor, render_group *RenderGroup, object_transform *Flat,
                        sim_region *SimRegion, world_position P)
{
    s32 Result = -1;
    for(u32 I = 0;
        I < MapEditor->PolyNodeCount;
        ++I)
    {
        nav_poly_node *Node = MapEditor->PolyNodes + I;

        b32 IsInBounds = IsInRectangleMesh(Node->Bounds, {P.TileX, P.TileY});
        if(IsInBounds)
        {
            v2 RealP = Subtract(MapEditor->WorldState->World, &P, &SimRegion->Origin);
            if(IsPointInPolygon(RenderGroup, Flat, &Node->PolyPtr->RealPoly, RealP))
            {
                Result = I;
                break;
            }
        }
    }

    return(Result);
}

internal void
UpdateAndRenderNavMeshMode(engine_map_editor *MapEditor, ui_state *UIState, sim_region *SimRegion,
                           render_group *RenderGroup, object_transform *Flat,
                           engine_input *Input, v2 MouseP)
{
    world *World = MapEditor->WorldState->World;
    MapEditor->CurrentPolygon = MapEditor->Polies + MapEditor->CurrentPolygonIndex;

    v2 PointDim = V2(0.1f, 0.1f);
    v2 P = (MouseP - 0.5f*PointDim) * (1.0f / PointDim.x);
    s32 X = RoundReal32ToInt32(P.x);
    s32 Y = RoundReal32ToInt32(P.y);

    v2 PointP = PointDim.x*V2(X, Y) + 0.5f*PointDim;
    world_position TestP = MapIntoTileSpace(World, SimRegion->Origin, PointP);
    PushRect(RenderGroup, Flat, V3(PointP, 8.0f), PointDim);

    rectangle2 MouseRect = RectCenterDim(MouseP, V2(2.0f, 2.0f));

    polygon2 Poly = {};
    temporary_memory TempMem = BeginTemporaryMemory(&World->Arena);

    switch(MapEditor->CurrentAction)
    {
        case MEAction_StartNewPolygon:
        {
            StartNewPolygon(MapEditor);
        } break;

        case MEAction_ResetCurrentPolygon:
        {
            ResetPolygon(MapEditor->CurrentPolygon);
        } break;

        case MEAction_DeleteCurrentPolygon:
        {
            DeletePolygon(MapEditor);
        } break;

        case MEAction_WritePolygons:
        {
            WritePolygons(MapEditor->Polies, MapEditor->PolygonCount);
        } break;

        case MEAction_TriangulateAll:
        {
            PartitionNavigationMesh(MapEditor, SimRegion, TempMem.Arena);
            MapEditor->Partitioned = true;
        } break;

        case MEAction_SubtractRegion:
        {
//            SubtractPolyFromMesh(MapEditor, SimRegion, &Poly, &World->Arena);
        } break;

        case MEAction_NavMeshPlaceStart:
        {
            MapEditor->StartNode = MapIntoTileSpace(MapEditor->WorldState->World, SimRegion->Origin, MouseP);
        } break;

        case MEAction_NavMeshPlaceEnd:
        {
            MapEditor->EndNode = MapIntoTileSpace(MapEditor->WorldState->World, SimRegion->Origin, MouseP);
        } break;
    }

    if(IsSetMapEditorFlag(MapEditor, MEFlag_EditEnable))
    {
        if(MapEditor->ChosenVertex)
        {
            *MapEditor->ChosenVertex = TestP;
        }
                    
        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
        {
            AddVertex(MapEditor, MapEditor->CurrentPolygon, World, TestP);
        }
        else if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
        {
            RemoveVertex(MapEditor, MapEditor->CurrentPolygon, World, TestP);
        }
    }

    {
        TIMED_BLOCK("Polies Conversation");
        // NOTE(paul): Convert Polies for simulation
        for(world_polygon_list *Iter = MapEditor->MeshPolygonsSentinal.Next;
            Iter != &MapEditor->MeshPolygonsSentinal;
            Iter = Iter->Next)
        {
            world_polygon *Poly = &Iter->Poly;
            polygon2 *RealPoly = &Iter->RealPoly;
            RealPoly->Vertices = PushArray(TempMem.Arena, Poly->VertexCount, v2);
            ConvertWorldPolygonToPolygon2(MapEditor->WorldState->World, &SimRegion->Origin, Poly, RealPoly);
        }
    }

    if(MapEditor->ShowNativePolies)
    {
        DrawPolygons(MapEditor, RenderGroup, UIState, World, MapEditor->Polies, MapEditor->PolygonCount,
                     SimRegion->Origin, MapEditor->CurrentPolygonIndex, TempMem.Arena);
    }
                   
    b32 ShowStringPull = true;
    b32 ShowStringPullEdges = false;
    if(MapEditor->Partitioned)
    {
        char Text[32];
        // NOTE(paul): Show Partition
        if(MapEditor->ShowPartition)
        {
            s32 StartNodeIndex = -1;
            s32 EndNodeIndex = -1;
            if(IsValid(MapEditor->StartNode) && IsValid(MapEditor->EndNode))
            {
                v2 StartP = Subtract(MapEditor->WorldState->World, &MapEditor->StartNode, &SimRegion->Origin);
                v2 EndP = Subtract(MapEditor->WorldState->World, &MapEditor->EndNode, &SimRegion->Origin);
                PushRect(RenderGroup, Flat, V3(StartP, 42.0f), V2(0.5f, 0.5f), V4(1, 0, 0, 1));
                PushRect(RenderGroup, Flat, V3(EndP, 42.0f), V2(0.5f, 0.5f), V4(0, 1, 0, 1));
                PushRectOutline(RenderGroup, Flat, V3(StartP, 42.0f), V2(0.5f, 0.5f), V4(0, 0, 0, 1), 0.04f);
                PushRectOutline(RenderGroup, Flat, V3(EndP, 42.0f), V2(0.5f, 0.5f), V4(0, 0, 0, 1), 0.04f);

                StartNodeIndex = FindNavPolyNodeForPoint(MapEditor, RenderGroup, Flat, SimRegion, MapEditor->StartNode);
                EndNodeIndex = FindNavPolyNodeForPoint(MapEditor, RenderGroup, Flat, SimRegion, MapEditor->EndNode);

                nav_poly_node *Path = SolvePolyAStar(MapEditor, MapEditor->PolyNodes + StartNodeIndex,
                                                     MapEditor->PolyNodes + EndNodeIndex);
#if 0
                s32 nportals = 0;
                f32 *portals = PushArray(TempMem.Arena, 128, f32);
                vcpy(&portals[nportals*4 + 0], EndP.E);
                vcpy(&portals[nportals*4 + 2], EndP.E);
                ++nportals;                        

                for(nav_poly_node *Node = Path;
                    Node->Parent;
                    Node = Node->Parent)
                {
                    nav_poly_node *Parent = Node->Parent;
                    neighbour_edge E = {};
                    for(s32 I = 0;
                        I < Node->NeighbourCount;
                        ++I)
                    {
                        nav_poly_node *N = Node->Neighbours[I];
                        if(N->Index == Parent->Index)
                        {
                            E = Node->NEdge[I];
                            break;
                        }
                    }

                    u32 To = Node->Index;
                    u32 From = Parent->Index;
                    
                    hash_key Key = {};
                    Key.WorldEdge.A = E.A;
                    Key.WorldEdge.B = E.B;
                    hash_data Edge = GetHashElement(&MapEditor->EdgeTable, Key);

                    v2 A = {};
                    v2 B = {};
                    if(From == Edge.PolyMeshAdjacency.PolyAID)
                    {
                        A = Subtract(MapEditor->WorldState->World, &Edge.PolyMeshAdjacency.ALeft, &SimRegion->Origin);
                        B = Subtract(MapEditor->WorldState->World, &Edge.PolyMeshAdjacency.ARight, &SimRegion->Origin);
                    }
                    else
                    {
                        A = Subtract(MapEditor->WorldState->World, &Edge.PolyMeshAdjacency.BLeft, &SimRegion->Origin);
                        B = Subtract(MapEditor->WorldState->World, &Edge.PolyMeshAdjacency.BRight, &SimRegion->Origin);
                    }

                    vcpy(&portals[nportals*4 + 0], A.E);
                    vcpy(&portals[nportals*4 + 2], B.E);
                    ++nportals;                        

                    PushLine(RenderGroup, Flat, V3(A, 45.0f), V3(B, 45.0f), V4(1, 0, 1, 1));

                    PushRect(RenderGroup, Flat, V3(A, 50.0f), V2(0.25f, 0.25f), V4(0, 0, 1, 1));
                    PushRect(RenderGroup, Flat, V3(B, 50.0f), V2(0.1f, 0.25f), V4(0, 1, 1, 1));
                }

                vcpy(&portals[nportals*4 + 0], StartP.E);
                vcpy(&portals[nportals*4 + 2], StartP.E);
                ++nportals;                        

                s32 maxpts = 128;
                f32 *pts = PushArray(TempMem.Arena, maxpts, f32);
                s32 npts = stringPull(portals, nportals, pts, maxpts);

                for(s32 I = 0;
                    I < npts - 1;
                    ++I)
                {
                    v2 A = V2(pts[I*2 + 0], pts[I*2 + 1]);
                    v2 B = V2(pts[(I + 1)*2 + 0], pts[(I + 1)*2 + 1]);
                    PushLine(RenderGroup, Flat, V3(A, 45.0f), V3(B, 45.0f), V4(1, 1, 0, 1));
                }
#else
                s32 nportals = 0;
                v2 *portals = PushArray(TempMem.Arena, 64, v2);
                portals[nportals*2 + 0] = EndP;
                portals[nportals*2 + 1] = EndP;
                ++nportals;                        

                for(nav_poly_node *Node = Path;
                    Node->Parent;
                    Node = Node->Parent)
                {
                    nav_poly_node *Parent = Node->Parent;
                    neighbour_edge E = {};
                    for(s32 I = 0;
                        I < Node->NeighbourCount;
                        ++I)
                    {
                        nav_poly_node *N = Node->Neighbours[I];
                        if(N->Index == Parent->Index)
                        {
                            E = Node->NEdge[I];
                            break;
                        }
                    }

                    u32 To = Node->Index;
                    u32 From = Parent->Index;
                    
                    hash_key Key = {};
                    Key.WorldEdge.A = E.A;
                    Key.WorldEdge.B = E.B;
                    hash_data Edge = GetHashElement(&MapEditor->EdgeTable, Key);

                    v2 A = {};
                    v2 B = {};
                    if(From == Edge.PolyMeshAdjacency.PolyAID)
                    {
                        A = Subtract(MapEditor->WorldState->World, &Edge.PolyMeshAdjacency.ALeft, &SimRegion->Origin);
                        B = Subtract(MapEditor->WorldState->World, &Edge.PolyMeshAdjacency.ARight, &SimRegion->Origin);
                    }
                    else
                    {
                        A = Subtract(MapEditor->WorldState->World, &Edge.PolyMeshAdjacency.BLeft, &SimRegion->Origin);
                        B = Subtract(MapEditor->WorldState->World, &Edge.PolyMeshAdjacency.BRight, &SimRegion->Origin);
                    }

                    portals[nportals*2] = A;
                    portals[nportals*2 + 1] = B;
                    ++nportals;                        

                    if(ShowStringPullEdges)
                    {
                        PushLine(RenderGroup, Flat, V3(A, 45.0f), V3(B, 45.0f), V4(1, 0, 1, 1));

                        PushRect(RenderGroup, Flat, V3(A, 50.0f), V2(0.25f, 0.25f), V4(0, 0, 1, 1));
                        PushRect(RenderGroup, Flat, V3(B, 50.0f), V2(0.1f, 0.25f), V4(0, 1, 1, 1));
                    }

                    v2 CenterA = Subtract(MapEditor->WorldState->World, &Node->TileP, &SimRegion->Origin);
                    v2 CenterB = Subtract(MapEditor->WorldState->World, &Parent->TileP, &SimRegion->Origin);
                    PushLine(RenderGroup, Flat, V3(CenterA, 50.0f), V3(CenterB, 50.0f), V4(1, 0, 0, 1));
                }

                portals[nportals*2] = StartP;
                portals[nportals*2 + 1] = StartP;
                ++nportals;                        

                s32 maxpts = 64;
                v2 *pts = PushArray(TempMem.Arena, maxpts, v2);
                s32 npts = StringPull(portals, nportals, pts, maxpts);

                if(ShowStringPull)
                {
                    for(s32 I = 0;
                        I < npts - 1;
                        ++I)
                    {
                        v2 A = pts[I];
                        v2 B = pts[I + 1];
                        PushLine(RenderGroup, Flat, V3(A, 45.0f), V3(B, 45.0f), V4(1, 1, 0, 1));
                        PushLine(RenderGroup, Flat, V3(A - V2(0.0f, 0.02f), 44.0f), V3(B - V2(0.0f, 0.02f), 44.0f), V4(0, 0, 0, 1));
                    }
                }

#endif
                
#if 0
                for(nav_poly_node *Node = Path;
                    Node->Parent;
                    Node = Node->Parent)
                {
                    nav_poly_node *Parent = Node->Parent;
                    neighbour_edge E = {};
                    for(s32 I = 0;
                        I < Node->NeighbourCount;
                        ++I)
                    {
                        nav_poly_node *N = Node->Neighbours[I];
                        if(N->Index == Parent->Index)
                        {
                            E = Node->NEdge[I];
                            break;
                        }
                    }
                            
                    v2 A = Subtract(MapEditor->WorldState->World, &E.A, &SimRegion->Origin);
                    v2 B = Subtract(MapEditor->WorldState->World, &E.B, &SimRegion->Origin);

                    PushLine(RenderGroup, Flat, V3(A, 45.0f), V3(B, 45.0f), V4(1, 0, 1, 1));

                    vcpy(&portals[nportals*4 + 0], A.E);
                    vcpy(&portals[nportals*4 + 2], B.E);
                    ++nportals;                        

                    PushRect(RenderGroup, Flat, V3(A, 50.0f), V2(0.1f, 0.25f), V4(0, 1, 1, 1));
                    PushRect(RenderGroup, Flat, V3(B, 50.0f), V2(0.1f, 0.25f), V4(0, 1, 1, 1));
                }
#endif
            }

            int C = 0;
            for(u32 I = 0;
                I < MapEditor->PolyNodeCount;
                ++I)
            {
                nav_poly_node *Node = MapEditor->PolyNodes + I;
                polygon2 *RealPoly = &Node->PolyPtr->RealPoly;
                v2 Center = Subtract(MapEditor->WorldState->World, &Node->TileP, &SimRegion->Origin);

                if(MapEditor->ShowColor)
                {
                    triangulate_result TResult = DelaunayTriangulate(RealPoly, TempMem.Arena);
                    for(s32 TIndex = 0;
                        TIndex < TResult.TriangleCount;
                        ++TIndex)
                    {
                        triangle *T = TResult.Triangles + TIndex;
                        if((s32)I == StartNodeIndex)
                            PushTriangle(RenderGroup, Flat, *T, 24.0f, V4(0, 1, 0, 0.5f));
                        else if((s32)I == EndNodeIndex)
                            PushTriangle(RenderGroup, Flat, *T, 24.0f, V4(1, 0, 0, 0.5f));
                        else
                            PushTriangle(RenderGroup, Flat, *T, 24.0f, V4(DebugColorTable[(C + 4) % ArrayCount(DebugColorTable)], 0.5f));
                            
                        PushTriangle(RenderGroup, Flat, *T, 24.0f, V4(0, 0, 0, 0.5f));
                    }
                    Platform.DeallocateMemory(TResult.Triangles);
                    Platform.DeallocateMemory(TResult.Adjacencies);
                }

                for(s32 PI = 0;
                    PI < RealPoly->VertexCount;
                    ++PI)
                {
                    v2 A = RealPoly->Vertices[PI];
                    v2 B = RealPoly->Vertices[(PI + 1) % RealPoly->VertexCount];

                    PushLine(RenderGroup, Flat, V3(A, 24.0f), V3(B, 24.0f), V4(0, 0, 0, 1));
                }
                    
                PushRect(RenderGroup, Flat, V3(Center, 30.0f), V2(0.1f, 0.1f), V4(1, 1, 0.5f, 1));

                FormatString(ArrayCount(Text), Text, "%d", I);
                entity_basis_p_result BasisP = GetRenderEntityBasisP(RenderGroup->CameraTransform,
                                                                     Flat, V3(Center, 0.0f));
                v3 P = Unproject(&UIState->RenderGroup, Flat, BasisP.P);
                UITextOutAt(UIState, P.xy, Text, 0.8f);

                if(MapEditor->ShowNeighbours)
                {
                    for(s32 J = 0;
                        J < Node->NeighbourCount;
                        ++J)
                    {
                        nav_poly_node *NNode = Node->Neighbours[J];
                        v2 NCenter = Subtract(MapEditor->WorldState->World, &NNode->TileP, &SimRegion->Origin);
                        PushLine(RenderGroup, Flat, V3(Center, 32.0f), V3(NCenter, 32.0f), V4(0, 0, 1, 1));
                    }
                }

                ++C;
            }
        }
#if 0
                        
//                        BuildAdjacenciesArray(MapEditor, SimRegion);
//                        MergeTriangels(RenderGroup, &Flat, MapEditor, &SimRegion->Origin, &World->Arena);
//                        DrawMeshTriangles(UIState, RenderGroup, World, MapEditor->MeshTriangles, MapEditor->MeshTriangleCount, SimRegion, MouseRect);

#if 0
        world_position TileP = {18, 4};
        world_position ETileP = {21, 19};
        v2 P = Subtract(MapEditor->WorldState->World, &TileP, &SimRegion->Origin);
//                        v2 EP = Subtract(MapEditor->WorldState->World, &ETileP, &SimRegion->Origin);
        v2 EP = MouseP;
        PushRect(RenderGroup, &Flat, V3(P, 42.0f), V2(0.5f, 0.5f), V4(0, 0, 1, 1));
        PushRect(RenderGroup, &Flat, V3(EP, 42.0f), V2(0.5f, 0.5f), V4(1, 0, 0, 1));

        s32 nportals = 0;
        f32 *portals = PushArray(TempMem.Arena, 128, f32);
        vcpy(&portals[nportals*4 + 0], P.E);
        vcpy(&portals[nportals*4 + 2], P.E);
        ++nportals;                        

        for(nav_poly_node *Node = Path;
            Node->Parent;
            Node = Node->Parent)
        {
            nav_poly_node *Parent = Node->Parent;
            neighbour_edge E = {};
            for(s32 I = 0;
                I < Node->NeighbourCount;
                ++I)
            {
                nav_poly_node *N = Node->Neighbours[I];
                if(N->Index == Parent->Index)
                {
                    E = Node->NEdge[I];
                    break;
                }
            }
                            
            v2 A = Subtract(MapEditor->WorldState->World, &E.A, &SimRegion->Origin);
            v2 B = Subtract(MapEditor->WorldState->World, &E.B, &SimRegion->Origin);

            PushLine(RenderGroup, &Flat, V3(A, 45.0f), V3(B, 45.0f), V4(1, 0, 1, 1));

            vcpy(&portals[nportals*4 + 0], B.E);
            vcpy(&portals[nportals*4 + 2], A.E);
            ++nportals;                        

            PushRect(RenderGroup, &Flat, V3(A, 50.0f), V2(0.1f, 0.25f), V4(0, 1, 0, 1));
            PushRect(RenderGroup, &Flat, V3(B, 50.0f), V2(0.1f, 0.25f), V4(0, 1, 0, 1));
#if 0
            v2 d = B - A;
            v2 ap = P - A;
            f32 Invdot = 1.0f / Inner(d, d);
            f32 t = Inner(ap, d) *Invdot;
            v2 Q = A + t*d;
            if(t < 0.0f)
            {
                Q = A;
            }
            else if(t > 1.0f)
            {
                Q = B;
            }

            PushRect(RenderGroup, &Flat, V3(Q, 42.0f), V2(0.25f, 0.25f), V4(1, 1, 1, 1));
            P = Q;
#endif
        }

        vcpy(&portals[nportals*4 + 0], EP.E);
        vcpy(&portals[nportals*4 + 2], EP.E);
        ++nportals;                        

        s32 maxpts = 128;
        f32 *pts = PushArray(TempMem.Arena, maxpts, f32);
        s32 npts = stringPull(portals, nportals, pts, maxpts);

        for(s32 I = 0;
            I < npts - 1;
            ++I)
        {
            v2 A = V2(pts[I*2 + 0], pts[I*2 + 1]);
            v2 B = V2(pts[(I + 1)*2 + 0], pts[(I + 1)*2 + 1]);
            PushLine(RenderGroup, &Flat, V3(A, 45.0f), V3(B, 45.0f), V4(1, 1, 0, 1));
        }
                        
        for(nav_poly_node *Node = Path;
            Node->Parent;
            Node = Node->Parent)
        {
            nav_poly_node *Next = Node->Parent;

            v2 A = Subtract(MapEditor->WorldState->World, &Node->TileP, &SimRegion->Origin);
            v2 B = Subtract(MapEditor->WorldState->World, &Next->TileP, &SimRegion->Origin);

            PushLine(RenderGroup, &Flat, V3(A, 42.0f), V3(B, 42.0f), V4(1, 0, 0, 1));

            rectangle2 Rect = {};
            world_position Min = {Node->Bounds.Min.x, Node->Bounds.Min.y};
            world_position Max = {Node->Bounds.Max.x, Node->Bounds.Max.y};
            Rect.Min = Subtract(MapEditor->WorldState->World, &Min, &SimRegion->Origin);
            Rect.Max = Subtract(MapEditor->WorldState->World, &Max, &SimRegion->Origin);

            world_position MP = MapIntoTileSpace(MapEditor->WorldState->World, SimRegion->Origin, MouseP);
    
            b32 IsInside = IsInRectangleMesh(Node->Bounds, {MP.TileX, MP.TileY});
            v4 Color = V4(1, 0, 1, 1);
            if(IsInside)
                Color = V4(1, 0, 0, 1);

            PushRectOutline(RenderGroup, &Flat, Rect, 50.0f, Color, 0.02f);
        }
#endif                        
#endif
    }

    EndTemporaryMemory(TempMem);
}

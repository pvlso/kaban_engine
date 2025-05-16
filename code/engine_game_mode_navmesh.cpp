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
ShareEdge(editor_mode_game *GameMode, s32 T, s32 Adj)
{
    s32 SharedCount = 0;
    world_triangle *Tri = GameMode->MeshTriangles + T;
    world_triangle *AdjTri = GameMode->MeshTriangles + Adj;
    for(s32 I = 0; I < 3; ++I)
    {
        for(s32 J = 0; J < 3; ++J)
        {
            if(AreInSameTile(GameMode->World, &Tri->Vertices[I], &AdjTri->Vertices[J]))
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
BuildAdjacenciesArray(editor_mode_game *GameMode, sim_region *SimRegion)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(&GameMode->World->Arena); 

    hash_table EdgeHashTable = {};
    EdgeHashTable.Size = 3*GameMode->MeshTriangleCount;
    EdgeHashTable.Hash = PushArray(TempMem.Arena, EdgeHashTable.Size, hash_table_entry *);

    edge *Edges = PushArray(TempMem.Arena, 3*GameMode->MeshTriangleCount, edge);
    
    for(s32 I = 0;
        I < GameMode->MeshTriangleCount;
        ++I)
    {
        world_triangle *T = GameMode->MeshTriangles + I;
        if(IsValid(T->V1))
        {
            v2 V1 = Subtract(GameMode->World, &T->V1, &SimRegion->Origin);
            v2 V2 = Subtract(GameMode->World, &T->V2, &SimRegion->Origin);
            v2 V3 = Subtract(GameMode->World, &T->V3, &SimRegion->Origin);

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
        I < GameMode->MeshTriangleCount;
        ++I)
    {
        world_triangle *T = GameMode->MeshTriangles + I;
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
    for(s32 I = 0; I < GameMode->MeshTriangleCount; ++I)
    {
        for(s32 J = 0; J < 3; ++J)
        {
            s32 AdjIndex = GameMode->MeshTriangles[I].Adj.Adjacencies[J];
            if(AdjIndex >= 0)
            {
                Assert(ShareEdge(GameMode, I, AdjIndex))
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
        SubResults[SubCount] = SubtractTriangels(Subject, Subtractor, 0.0f, 0.0f, 0.0001f, TempArena);
        if((SubResults[SubCount].Set.PolygonCount > 0) || SubResults[SubCount].FullyRemoved)
        {
            world_triangle *WorldT = GameMode->MeshTriangles + SubjectIndices[SubjectIndex];
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
FindSubjectTris(editor_mode_game *GameMode, sim_region *SimRegion, rectangle2 SubBounds, triangle *SubjectTris,
                s32 *SubjectIndices, s32 *SubjectCount)
{
    game_mode_world *WorldState = GameMode->WorldState;
    
    for(s32 Index = 0;
        Index < GameMode->MeshTriangleCount;
        ++Index)
    {
        world_triangle *T = GameMode->MeshTriangles + Index; 
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
SubtractPolyFromMesh(editor_mode_game *GameMode, sim_region *SimRegion, polygon2 *Region, memory_arena *Arena)
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

            WorldT->Vertices[0] = MapIntoTileSpace(GameMode->WorldState->World, SimRegion->Origin, ResultT->Vertices[0]);
            WorldT->Vertices[1] = MapIntoTileSpace(GameMode->WorldState->World, SimRegion->Origin, ResultT->Vertices[1]);
            WorldT->Vertices[2] = MapIntoTileSpace(GameMode->WorldState->World, SimRegion->Origin, ResultT->Vertices[2]);
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

            T->Vertices[0] = Subtract(GameMode->WorldState->World, &WorldT->V1, BaseP);
            T->Vertices[1] = Subtract(GameMode->WorldState->World, &WorldT->V2, BaseP);
            T->Vertices[2] = Subtract(GameMode->WorldState->World, &WorldT->V3, BaseP);

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
            WorldPoly->Vertices[J] = MapIntoTileSpace(GameMode->WorldState->World, *BaseP, Poly->Vertices[J]);
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
MergeTriangels(render_group *RenderGroup, object_transform *Flat, editor_mode_game *GameMode,
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
            WorldPoly->Vertices[J] = MapIntoTileSpace(GameMode->WorldState->World, *BaseP, Poly->Vertices[J]);
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
TriangulatePolygons(render_group *RenderGroup, object_transform *Flat, editor_mode_game *GameMode, world_position *BaseP, memory_arena *Arena)
{
    TIMED_FUNCTION();

    GameMode->MeshTriangleCount = 0;
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    polygon2 P = {};
    P.VertexCount = 0;
    P.Vertices = PushArray(TempMem.Arena, MAX_VERTEX_COUNT, v2);
#if 0
    s32 VertexMaxCount = 0;
    for(u32 I = 0;
        I < GameMode->PolygonCount;
        ++I)
    {
        world_polygon *Poly = GameMode->Polies + I;
        VertexMaxCount += Poly->VertexCount;
    }

    v2 *Vertices = PushArray(TempMem.Arena, VertexMaxCount, v2);
    v2 *At = Vertices;
    
    for(u32 Index = 0;
        Index < GameMode->PolygonCount;
        ++Index)
    {
        world_polygon *Poly = GameMode->Polies + Index;
        ConvertWorldPolygonToPolygon2(GameMode->WorldState->World, BaseP, Poly, &P);

        Copy(sizeof(v2)*Poly->VertexCount, P.Vertices, At);
        At += Poly->VertexCount;
    }

    RemoveDublicatPoints(Vertices, &VertexMaxCount);
    
    int a = 0;
#endif
    
#if 1    
    for(u32 Index = 0;
        Index < GameMode->PolygonCount;
        ++Index)
    {
        world_polygon *Poly = GameMode->Polies + Index;
        ConvertWorldPolygonToPolygon2(GameMode->WorldState->World, BaseP, Poly, &P);

//        triangulate_result TResult = ConstrainedDelaunayTriangulate(&P, Arena);
        triangulate_result TResult = DelaunayTriangulate(&P, Arena);

        for(s32 TIndex = GameMode->MeshTriangleCount;
            TIndex < (GameMode->MeshTriangleCount + TResult.TriangleCount);
            ++TIndex)
        {
            triangle *T = TResult.Triangles + (TIndex - GameMode->MeshTriangleCount);
            world_triangle *WorldT = GameMode->MeshTriangles + TIndex;
            WorldT->V1 = MapIntoTileSpace(GameMode->WorldState->World, *BaseP, T->Vertices[0]);
            WorldT->V2 = MapIntoTileSpace(GameMode->WorldState->World, *BaseP, T->Vertices[1]);
            WorldT->V3 = MapIntoTileSpace(GameMode->WorldState->World, *BaseP, T->Vertices[2]);
        }

//        MergeTriangels(RenderGroup, Flat, GameMode,
//                       TResult.Triangles, TResult.Adjacencies, TResult.TriangleCount,
//                       BaseP, TempMem.Arena);

        GameMode->MeshTriangleCount += TResult.TriangleCount;

        Platform.DeallocateMemory(TResult.Triangles);
        Platform.DeallocateMemory(TResult.Adjacencies);
    }
#endif
    
    EndTemporaryMemory(TempMem);
}

inline void
InitNavPolyNode(nav_poly_node *Node, s32 Index, s32 PolyIndex,
                world_position P, rectangle2i Bounds)
{
    Node->TileP = P;
    Node->Index = Index;
    Node->PolyIndex = PolyIndex;
    Node->Visited = false;
    Node->Bounds = Bounds;
    Node->Parent = 0;
}

internal void
PartitionPolies(editor_mode_game *GameMode, sim_region *SimRegion,
                render_group *RenderGroup, object_transform *Flat,
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
        Index < GameMode->PolygonCount;
        ++Index)
    {
        world_polygon *Poly = GameMode->Polies + Index;
        ConvertWorldPolygonToPolygon2(GameMode->WorldState->World, &SimRegion->Origin, Poly, &P);

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

        EPPSetOrientation(poly, EPP_ORIENTATION_CCW); // Ensure clockwise
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
        DLIST_INSERT(&GameMode->MeshPolygonsSentinal, New);                            
        New->Poly.VertexCount = P.numpoints;
        New->Poly.Vertices = (world_position *)Platform.AllocateMemory(sizeof(world_position)*New->Poly.VertexCount);

        for(s32 J = 0; J < P.numpoints; ++ J)
            New->Poly.Vertices[J] = MapIntoTileSpace(GameMode->WorldState->World, SimRegion->Origin, V2(P.points[J].x, P.points[J].y));
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
SolvePolyAStar(editor_mode_game *GameMode, nav_poly_node *Start, nav_poly_node *End)
{
    TIMED_FUNCTION();

    if(Start && End)
    {
        for(u32 NodeIndex = 0;
            NodeIndex < GameMode->PolyNodeCount;
            ++NodeIndex)
        {
            nav_poly_node *Node = GameMode->PolyNodes + NodeIndex;
            Node->Visited = false;
            Node->GlobalGoal = Real32Maximum;
            Node->LocalGoal = Real32Maximum;
            Node->Parent = 0;
        }

        nav_poly_node *CurrentNode = Start;
        CurrentNode->LocalGoal = 0.0f;
        CurrentNode->GlobalGoal = DistanceBetween(GameMode->WorldState->World, Start, End);

        heap *Heap = &GameMode->MinPolyNodeHeap;

        sort_entry Key = {};
        Key.Index = Start->Index;
        Key.SortKey = Start->GlobalGoal;
        MinHeapInsertNode(Heap, Key);

        while((Heap->Size != 0) && (CurrentNode != End))
        {
            nav_poly_node *TestNode = GameMode->PolyNodes + Heap->Nodes[0].Index;
            while((TestNode->Visited) && (Heap->Size != 0))
            {
                MinHeapExtractNode(Heap);
                TestNode = GameMode->PolyNodes + Heap->Nodes[0].Index;
            }

            if(Heap->Size == 0)
            {
                break;
            }

            CurrentNode = GameMode->PolyNodes + Heap->Nodes[0].Index; 
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

                        r32 LowerGoal = CurrentNode->LocalGoal + DistanceBetween(GameMode->WorldState->World, CurrentNode, NeighbourNode);
                        if(LowerGoal < NeighbourNode->LocalGoal)
                        {
                            NeighbourNode->Parent = CurrentNode;
                            NeighbourNode->LocalGoal = LowerGoal;

                            NeighbourNode->GlobalGoal = (NeighbourNode->LocalGoal +
                                                         DistanceBetween(GameMode->WorldState->World, NeighbourNode, End));
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
inline float triarea2(const float* a, const float* b, const float* c)
{
    const float ax = b[0] - a[0];
    const float ay = b[1] - a[1];
    const float bx = c[0] - a[0];
    const float by = c[1] - a[1];
    return bx*ay - ax*by;
}

inline bool vequal(const float* a, const float* b)
{
    static const float eq = 0.001f*0.001f;
    f32 ax = b[0] - a[0];
    f32 ay = b[1] - a[1];
    f32 Result = ax*ax + ay*ay;

    return Result < eq;
}

inline void
vcpy(float *Dest, const float *Source)
{
    Copy(sizeof(f32)*2, (void *)Source, Dest);
}

int stringPull(const float* portals, int nportals,
               float* pts, const int maxPts)
{
    TIMED_FUNCTION();
    // Find straight path.
    int npts = 0;
    // Init scan state
    float portalApex[2], portalLeft[2], portalRight[2];
    int apexIndex = 0, leftIndex = 0, rightIndex = 0;
    vcpy(portalApex, &portals[0]);
    vcpy(portalLeft, &portals[0]);
    vcpy(portalRight, &portals[2]);

    // Add start point.
    vcpy(&pts[npts*2], portalApex);
    npts++;

    for (int i = 1; i < nportals && npts < maxPts; ++i)
    {
        const float* left = &portals[i*4+0];
        const float* right = &portals[i*4+2];

        // Update right vertex.
        if (triarea2(portalApex, portalRight, right) <= 0.0f)
        {
            if (vequal(portalApex, portalRight) || triarea2(portalApex, portalLeft, right) > 0.0f)
            {
                // Tighten the funnel.
                vcpy(portalRight, right);
                rightIndex = i;
            }
            else
            {
                // Right over left, insert left to path and restart scan from portal left point.
                vcpy(&pts[npts*2], portalLeft);
                npts++;
                // Make current left the new apex.
                vcpy(portalApex, portalLeft);
                apexIndex = leftIndex;
                // Reset portal
                vcpy(portalLeft, portalApex);
                vcpy(portalRight, portalApex);
                leftIndex = apexIndex;
                rightIndex = apexIndex;
                // Restart scan
                i = apexIndex;
                continue;
            }
        }

        // Update left vertex.
        if (triarea2(portalApex, portalLeft, left) >= 0.0f)
        {
            if (vequal(portalApex, portalLeft) || triarea2(portalApex, portalRight, left) < 0.0f)
            {
                // Tighten the funnel.
                vcpy(portalLeft, left);
                leftIndex = i;
            }
            else
            {
                // Left over right, insert right to path and restart scan from portal right point.
                vcpy(&pts[npts*2], portalRight);
                npts++;
                // Make current right the new apex.
                vcpy(portalApex, portalRight);
                apexIndex = rightIndex;
                // Reset portal
                vcpy(portalLeft, portalApex);
                vcpy(portalRight, portalApex);
                leftIndex = apexIndex;
                rightIndex = apexIndex;
                // Restart scan
                i = apexIndex;
                continue;
            }
        }
    }
    // Append last point to path.
    if (npts < maxPts)
    {
        vcpy(&pts[npts*2], &portals[(nportals-1)*4+0]);
        npts++;
    }

    return npts;
}

internal void
CalculatePath()
{
}

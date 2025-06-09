/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline void
InitNavMesh(navigation_mesh *NavMesh, memory_arena *Arena)
{
    SubArena(&NavMesh->Arena, Arena, Megabytes(1));

    DLIST_INIT(&NavMesh->MeshPolygonsSentinal);

    NavMesh->PolyNodeCount = 0;
    NavMesh->PolyNodes = PushArray(&NavMesh->Arena, 512, nav_poly_node);
    NavMesh->MinPolyNodeHeap.MaxSize = 256;
    NavMesh->MinPolyNodeHeap.Size = 0;
    NavMesh->MinPolyNodeHeap.Nodes =
        PushArray(&NavMesh->Arena, NavMesh->MinPolyNodeHeap.MaxSize, sort_entry);

    NavMesh->EdgeTable.Size = 4096;
    NavMesh->EdgeTable.KeyType = HashKeyType_WORLD_EDGE;
    NavMesh->EdgeTable.DataType = HashDataType_POLY_MESH_ADJACENCY;
    NavMesh->EdgeTable.Hash =
        PushArray(&NavMesh->Arena, NavMesh->EdgeTable.Size, hash_table_entry *);

}

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
PartitionPolies(navigation_mesh *NavMesh, world *World, sim_region *SimRegion,
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
        Index < NavMesh->PolygonCount;
        ++Index)
    {
        world_polygon *Poly = NavMesh->Polies + Index;
        ConvertWorldPolygonToPolygon2(World, &SimRegion->Origin, Poly, &P);

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
        DLIST_INSERT(&NavMesh->MeshPolygonsSentinal, New);                            
        New->Poly.VertexCount = P.numpoints;
        New->Poly.Vertices = (world_position *)Platform.AllocateMemory(sizeof(world_position)*New->Poly.VertexCount);

        for(s32 J = 0; J < P.numpoints; ++ J)
            New->Poly.Vertices[J] = MapIntoTileSpace(World, SimRegion->Origin, V2(P.points[J].x, P.points[J].y));
    }
}


internal void
PartitionNavigationMesh(navigation_mesh *NavMesh, world *World,
                        sim_region *SimRegion, memory_arena *TempArena)
{
    // NOTE(paul): Clear Mesh Polygon List
    for(world_polygon_list *Iter = NavMesh->MeshPolygonsSentinal.Next;
        Iter != &NavMesh->MeshPolygonsSentinal;
        )
    {
        world_polygon_list *T = Iter;
        Iter = T->Next;

        DLIST_REMOVE(T);
        Platform.DeallocateMemory(T->Poly.Vertices);
        Platform.DeallocateMemory(T);
    }
                            
    PartitionPolies(NavMesh, World, SimRegion, TempArena);

    // NOTE(paul): Clear node neighbours count
    for(u32 I = 0;
        I < NavMesh->PolyNodeCount;
        ++I)
    {
        nav_poly_node *Node = NavMesh->PolyNodes + I;
        Node->NeighbourCount = 0;
    }

    for(u32 I = 0;
        I < NavMesh->EdgeTable.Size;
        ++I)
    {
        hash_table_entry *Scan = NavMesh->EdgeTable.Hash[I];
        while(Scan)
        {
            hash_table_entry *Entry = Scan;
            Scan = Scan->Next;

            Entry->Next = NavMesh->EdgeTable.Free;
            NavMesh->EdgeTable.Free = Entry;
        }
    }
    
    NavMesh->PolyNodeCount = 0;                            

    polygon2 RealPoly = {};
    RealPoly.Vertices = PushArray(TempArena, 128, v2);

    s32 PIndex = 0;
    for(world_polygon_list *Iter = NavMesh->MeshPolygonsSentinal.Next;
        Iter != &NavMesh->MeshPolygonsSentinal;
        Iter = Iter->Next)
    {
        world_polygon *Poly = &Iter->Poly;
        ConvertWorldPolygonToPolygon2(World, &SimRegion->Origin, Poly, &RealPoly);
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

        nav_poly_node *Node = NavMesh->PolyNodes + NavMesh->PolyNodeCount++;                                
        InitNavPolyNode(Node, (NavMesh->PolyNodeCount - 1), Iter,
                        MapIntoTileSpace(World, SimRegion->Origin, Center),
                        Bounds);
        PIndex += 1;
    }

    // NOTE(paul): Build conectivity graph
    s32 I1 = 0;
    for(world_polygon_list *Iter = NavMesh->MeshPolygonsSentinal.Next;
        Iter != &NavMesh->MeshPolygonsSentinal;
        Iter = Iter->Next)
    {
        world_polygon *Poly = &Iter->Poly;
        
        s32 Orientation = GetPolygonOrientation(&Iter->RealPoly); 
        Assert(Orientation == POLY_ORIENTATION_CCW)
                                
        s32 I2 = I1 + 1;
        for(world_polygon_list *Iter2 = Iter->Next;
            Iter2 != &NavMesh->MeshPolygonsSentinal;
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
                        InsertKey(&NavMesh->EdgeTable, HashKey, Data, &NavMesh->Arena);

                        nav_poly_node *Poly1Node = NavMesh->PolyNodes + I1;
                        nav_poly_node *Poly2Node = NavMesh->PolyNodes + I2;
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

inline r32
DistanceBetween(world *World, nav_poly_node *NodeA, nav_poly_node *NodeB)
{
    v2 Delta = Subtract(World, &NodeA->TileP, &NodeB->TileP);

    r32 Result = SquareRoot(Square(Delta.x) + Square(Delta.y));

    return(Result);
}

internal nav_poly_node *
SolvePolyAStar(navigation_mesh *NavMesh, world *World,
               nav_poly_node *Start, nav_poly_node *End)
{
    TIMED_FUNCTION();

    if(Start && End)
    {
        for(u32 NodeIndex = 0;
            NodeIndex < NavMesh->PolyNodeCount;
            ++NodeIndex)
        {
            nav_poly_node *Node = NavMesh->PolyNodes + NodeIndex;
            Node->Visited = false;
            Node->GlobalGoal = Real32Maximum;
            Node->LocalGoal = Real32Maximum;
            Node->Parent = 0;
        }

        nav_poly_node *CurrentNode = Start;
        CurrentNode->LocalGoal = 0.0f;
        CurrentNode->GlobalGoal = DistanceBetween(World, Start, End);

        heap *Heap = &NavMesh->MinPolyNodeHeap;

        sort_entry Key = {};
        Key.Index = Start->Index;
        Key.SortKey = Start->GlobalGoal;
        MinHeapInsertNode(Heap, Key);

        while((Heap->Size != 0) && (CurrentNode != End))
        {
            nav_poly_node *TestNode = NavMesh->PolyNodes + Heap->Nodes[0].Index;
            while((TestNode->Visited) && (Heap->Size != 0))
            {
                MinHeapExtractNode(Heap);
                TestNode = NavMesh->PolyNodes + Heap->Nodes[0].Index;
            }

            if(Heap->Size == 0)
            {
                break;
            }

            CurrentNode = NavMesh->PolyNodes + Heap->Nodes[0].Index; 
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

                        r32 LowerGoal = CurrentNode->LocalGoal + DistanceBetween(World, CurrentNode, NeighbourNode);
                        if(LowerGoal < NeighbourNode->LocalGoal)
                        {
                            NeighbourNode->Parent = CurrentNode;
                            NeighbourNode->LocalGoal = LowerGoal;

                            NeighbourNode->GlobalGoal = (NeighbourNode->LocalGoal +
                                                         DistanceBetween(World, NeighbourNode, End));
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
FindNavPolyNodeForPoint(navigation_mesh *NavMesh, render_group *RenderGroup, object_transform *Flat,
                        world *World, sim_region *SimRegion, world_position P)
{
    s32 Result = -1;
    for(u32 I = 0;
        I < NavMesh->PolyNodeCount;
        ++I)
    {
        nav_poly_node *Node = NavMesh->PolyNodes + I;

        b32 IsInBounds = IsInRectangleMesh(Node->Bounds, {P.TileX, P.TileY});
        if(IsInBounds)
        {
            v2 RealP = Subtract(World, &P, &SimRegion->Origin);
            if(IsPointInPolygon(RenderGroup, Flat, &Node->PolyPtr->RealPoly, RealP))
            {
                Result = I;
                break;
            }
        }
    }

    return(Result);
}

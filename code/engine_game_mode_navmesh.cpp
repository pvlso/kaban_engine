/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

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
    MapEditor->CurrentPolygon =
        MapEditor->NavMesh.Polies + MapEditor->NavMesh.PolygonCount;
    ++MapEditor->CurrentPolygonIndex;
    ++MapEditor->NavMesh.PolygonCount;
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
        --MapEditor->NavMesh.PolygonCount;

        MapEditor->CurrentPolygon = MapEditor->NavMesh.Polies + MapEditor->CurrentPolygonIndex;
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

internal void
UpdateAndRenderNavMeshMode(engine_map_editor *MapEditor, ui_state *UIState, sim_region *SimRegion,
                           render_group *RenderGroup, object_transform *Flat,
                           engine_input *Input, v2 MouseP)
{
    world *World = MapEditor->WorldState->World;
    MapEditor->CurrentPolygon = MapEditor->NavMesh.Polies + MapEditor->CurrentPolygonIndex;

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
            WritePolygons(MapEditor->NavMesh.Polies, MapEditor->NavMesh.PolygonCount);
        } break;

        case MEAction_TriangulateAll:
        {
            PartitionNavigationMesh(&MapEditor->NavMesh, World, SimRegion, TempMem.Arena);
            MapEditor->NavMesh.Partitioned = true;
        } break;

        case MEAction_SubtractRegion:
        {
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

    if(MapEditor->ShowNativePolies)
    {
        DrawPolygons(MapEditor, RenderGroup, UIState, World, MapEditor->NavMesh.Polies, MapEditor->NavMesh.PolygonCount,
                     SimRegion->Origin, MapEditor->CurrentPolygonIndex, TempMem.Arena);
    }
                   
    b32 ShowStringPull = true;
    b32 ShowStringPullEdges = false;
    if(MapEditor->NavMesh.Partitioned)
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

                StartNodeIndex = FindNavPolyNodeForPoint(&MapEditor->NavMesh, RenderGroup, Flat, World, SimRegion, MapEditor->StartNode);
                EndNodeIndex = FindNavPolyNodeForPoint(&MapEditor->NavMesh, RenderGroup, Flat, World, SimRegion, MapEditor->EndNode);

                nav_poly_node *Path = SolvePolyAStar(&MapEditor->NavMesh, World, MapEditor->NavMesh.PolyNodes + StartNodeIndex,
                                                     MapEditor->NavMesh.PolyNodes + EndNodeIndex);
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
                    hash_data Edge = GetHashElement(&MapEditor->NavMesh.EdgeTable, Key);

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
            }

            int C = 0;
            for(u32 I = 0;
                I < MapEditor->NavMesh.PolyNodeCount;
                ++I)
            {
                nav_poly_node *Node = MapEditor->NavMesh.PolyNodes + I;
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
    }

    EndTemporaryMemory(TempMem);
}

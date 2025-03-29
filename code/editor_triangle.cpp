
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

// NOTE(babykaban): Triangle Subtraction =====================================================================================================================

inline void
TRISUBProjectVerticesOntoNormal(v2 *Vertices, v2 Normal, f32 *Min, f32 *Max)
{
    *Min = *Max = Inner(Vertices[0], Normal);
    for(s32 I = 1;
        I < 3;
        ++I)
    {
        f32 Projection = Inner(Vertices[I], Normal);
        if(Projection < *Min) *Min = Projection;
        if(Projection > *Max) *Max = Projection;
    }
}

inline b32
TRISUBOverlapExcludingVerticesAndEdges(f32 Min1, f32 Max1, f32 Min2, f32 Max2)
{
    b32 Result = !((Max1 < Min2) || (Max2 < Min1) ||
                   (Max1 == Min2) || (Max2 == Min1));

    return(Result);
}

internal b32
TRISUBTrianglesOverlapExcludingVerticesAndEdges(triangle *A, triangle *B)
{
    b32 Result = true;

    v2 Edges1[3] = 
        {
            A->Vertices[1] - A->Vertices[0],
            A->Vertices[2] - A->Vertices[1],
            A->Vertices[0] - A->Vertices[2],
        };

    v2 Edges2[3] =
        {
            B->Vertices[1] - B->Vertices[0],
            B->Vertices[2] - B->Vertices[1],
            B->Vertices[0] - B->Vertices[2],
        };

    v2 Normals[6];
    for(s32 I = 0;
        I < 3;
       ++I)
    {
        Normals[I] = Perp(Edges1[I]);
        Normals[I + 3] = Perp(Edges2[I]);
    }

    for(s32 I = 0;
        I < 6;
        ++I)
    {
        f32 Min1, Max1, Min2, Max2;

        TRISUBProjectVerticesOntoNormal(A->Vertices, Normals[I], &Min1, &Max1);
        TRISUBProjectVerticesOntoNormal(B->Vertices, Normals[I], &Min2, &Max2);

        if(!TRISUBOverlapExcludingVerticesAndEdges(Min1, Max1, Min2, Max2))
        {
            Result = false;
            break;
        }
    }

    return(Result);
}

inline b32
TRISUBLineSect(v2 x0, v2 x1, v2 y0, v2 y1, v2 *res)
{
    b32 Result = true;

    v2 dx = x1 - x0;
    v2 dy = y1 - y0;
    v2 d = x0 - y0;

    f32 dyx = Cross(dy, dx);
    if(AbsoluteValue(dyx) < TRISUB_EPSILON_F32)
    {
        Result = false;
    }
    else
    {
        dyx = Cross(d, dx) / dyx;
        if(dyx <= 0 || dyx >= 1)
        {
            Result = false;
        }
        else
        {
            *res = y0 + dyx * dy;
        }
    }

    return(Result);
}

inline b32
TRISUBInside(v2 p, v2 Clipper1, v2 Clipper2)
{
    b32 Result = false;
    f32 CrossProduct = (Clipper2.x - Clipper1.x) * (p.y - Clipper1.y) - (Clipper2.y - Clipper1.y) * (p.x - Clipper1.x);
    Result = CrossProduct < TRISUB_EPSILON_F32;

    return(Result);
}

inline s32
TRISUBLeftOf(v2 a, v2 b, v2 c)
{
    s32 Result = 0;

    v2 ba = b - a;
    v2 cb = c - b;

    f32 x = Cross(ba, cb);

    Result = (x < TRISUB_EPSILON_F32) ? -1 : (x > 0);
    return(Result);
}

inline void
TRISUBPolyEdgeClip(polygon2 *Sub, v2 x0, v2 x1, s32 Left, polygon2 *Res)
{
    v2 v0 = Sub->Vertices[Sub->VertexCount- 1];
    v2 v1 = {};

    Res->VertexCount = 0;

    s32 Side0 = TRISUBLeftOf(x0, x1, v0);
    if(Side0 != -Left)
    {
        Res->Vertices[Res->VertexCount++] = v0;
    }

    for(s32 I = 0;
        I < Sub->VertexCount;
        ++I)
    {
        v1 = Sub->Vertices[I];
        s32 Side1 = TRISUBLeftOf(x0, x1, v1);
        if(((Side0 + Side1) == 0) && Side0)
        {
            v2 Intersect;
//            if(TRISUBLineSect(x0, x1, v0, v1, &Intersect))
            if(LineIntersect(x0, x1, v0, v1, &Intersect))
            {
                Res->Vertices[Res->VertexCount++] = Intersect;
            }
        }

        if(I == (Sub->VertexCount - 1))
        {
            break;
        }

        if(Side1 != -Left)
        {
            Res->Vertices[Res->VertexCount++] = v1;
        }

        v0 = v1;
        Side0 = Side1;
    }
}

internal void
TRISUBSutherlandHodgman(polygon2 *Subject, triangle *Clip, polygon2 *Result, memory_arena *Arena)
{
    polygon2 Temp;
    Temp.VertexCount = 0;
    Temp.Vertices = PushArray(Arena, MAX_VERTEX_COUNT, v2);

    polygon2 New0;
    New0.VertexCount = 0;
    New0.Vertices = PushArray(Arena, MAX_VERTEX_COUNT, v2);

    s32 dir = TRISUBLeftOf(Clip->Vertices[0], Clip->Vertices[1], Clip->Vertices[2]);
    TRISUBPolyEdgeClip(Subject, Clip->Vertices[2], Clip->Vertices[0], dir, Result);
    for(s32 I = 0;
        I < 2;
        ++I)
    {
        Temp.VertexCount = Result->VertexCount;
        Copy(sizeof(v2)*Result->VertexCount, Result->Vertices, Temp.Vertices);

        Result->VertexCount = New0.VertexCount;
        Copy(sizeof(v2)*New0.VertexCount, New0.Vertices, Result->Vertices);

        New0.VertexCount = Temp.VertexCount;
        Copy(sizeof(v2)*Temp.VertexCount, Temp.Vertices, New0.Vertices);

        if(Temp.VertexCount == 0)
        {
            Result->VertexCount = 0;
            break;
        }

        TRISUBPolyEdgeClip(&New0, Clip->Vertices[I], Clip->Vertices[I + 1], dir, Result);
    }
}

inline void
TRISUBCleanUpOverlapPolygon(polygon2 *Poly)
{
    for(s32 I = 0;
        I < Poly->VertexCount;
        ++I)
    {
        v2 Cur = Poly->Vertices[I];
        v2 Next = Poly->Vertices[(I + 1) % Poly->VertexCount];
        r32 l = Length(Cur - Next);
        if(l < TRISUB_MINIMAL_POINT_DISTANCE)
        {
            Poly->Vertices[I + 1] = {};
            for(s32 J = I + 1;
                J < (Poly->VertexCount - 1);
                ++J)
            {
                Poly->Vertices[J] = Poly->Vertices[J + 1];                
            }
            --Poly->VertexCount;
            --I;
        }
    }

    Poly->Vertices[Poly->VertexCount] = {};
}

internal void
TRISUBHandleVertexCase(polygon2 *A, polygon2 *B)
{
    b32 RotateVerticies = false;
    for(s32 VertexIndex = 0;
        VertexIndex < B->VertexCount;
        ++VertexIndex)
    {
        if(TRISUBPointsAreEqual(A->Vertices[0], B->Vertices[VertexIndex]))
        {
            RotateVerticies = true;
            break;
        }
    }
    
    if(RotateVerticies)
    {
        v2 Temp = A->Vertices[2];
        A->Vertices[2] = A->Vertices[1];
        A->Vertices[1] = A->Vertices[0];
        A->Vertices[0] = Temp;
    }
}

inline void
TRISUBFindOutsidePointsFor(polygon2 *A, polygon2 *B, v2 *OutsidePoints, s32 *OutsideCount)
{
    for(s32 VertextIndex = 0;
        VertextIndex < A->VertexCount;
        ++VertextIndex)
    {
        b32 Inside = false;
        v2 p = A->Vertices[VertextIndex];
        for(s32 VertextIndexB = 0;
            VertextIndexB < B->VertexCount;
            ++VertextIndexB)
        {
            v2 p0 = B->Vertices[VertextIndexB];
            if(TRISUBPointsAreEqual(p, p0))
            {
                Inside = true;
                break;
            }
        }

        if(!Inside)
        {
            OutsidePoints[*OutsideCount] = p;
            (*OutsideCount)++;
        }
    }
}

internal b32
TRISUBIntersectLineSegment(polygon2 *A, v2 p1, v2 p2, v2 *Points, s32 *PointCount, memory_arena *Arena)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    b32 Intersect = false;
    s32 TempPointCount = 0;
    v2 *TempPoints = PushArray(TempMem.Arena, TRISUB_MAX_POINTS_PER_POLYGON, v2);

    for(s32 I = 0;
        I < (A->VertexCount - 1);
        ++I)
    {
        s32 J = (I + 1) % (A->VertexCount - 1);
        v2 p3 = A->Vertices[I];
        v2 p4 = A->Vertices[J];

        b32 Ends = false;
        if(DistanceToSegment(p3, p1, p2) < TRISUB_EPSILON_F32)
        {
            TempPoints[TempPointCount++] = p3;
            Intersect = true;
            Ends = true;
        }

        if(DistanceToSegment(p4, p1, p2) < TRISUB_EPSILON_F32)
        {
            TempPoints[TempPointCount++] = p4;
            Intersect = true;
            Ends = true;
        }
    }

    if(TempPointCount > 0)
    {
        ZeroArray((*PointCount)*sizeof(v2), Points);
        if(TempPointCount == 1)
        {
            Points[(*PointCount)++] = TempPoints[0];
        }
        else
        {
            s32 SortCount = TempPointCount;
            sort_entry *SortArray = PushArray(TempMem.Arena, TempPointCount, sort_entry);
            sort_entry *Temp = PushArray(TempMem.Arena, TempPointCount, sort_entry);
            for(s32 I = 0;
                I < TempPointCount;
                ++I)
            {
                sort_entry *Entry = SortArray + I;
                Entry->Index = I;
                Entry->SortKey = Length(p1 - TempPoints[I]);
            }

            RadixSort(SortCount, SortArray, Temp);

            v2 Last = TempPoints[SortArray[0].Index];
            Points[(*PointCount)++] = Last;
            for(s32 I = 1;
                I < TempPointCount;
                ++I)
            {
                v2 This = TempPoints[SortArray[I].Index];
                if(!TRISUBPointsAreEqual(This, Last))
                {
                    Points[(*PointCount)++] = This;
                    Last = This;
                }
            }
        }
    }

    EndTemporaryMemory(TempMem);
    
    return(Intersect);
}

internal points_between *
TRISUBGetIntersectionPoints(polygon2 *A, polygon2 *B, v2 *OutputPoints, s32 *OutputCount, memory_arena *Arena)
{
    points_between *Result = PushArray(Arena, 3, points_between);

    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    s32 TempCount = 0;
    v2 *Temp = PushArray(TempMem.Arena, TRISUB_MAX_POINTS_PER_POLYGON, v2);

    for(s32 I = 0;
        I < A->VertexCount;
        ++I)
    {
        v2 Prv = A->Vertices[I];
        v2 Cur = A->Vertices[(I + 1) % A->VertexCount];
        if(!TRISUBPointsAreEqual(Prv, Cur))
        {
            TRISUBIntersectLineSegment(B, Prv, Cur, Temp, &TempCount, Arena);
            s32 AddedCount = 0;
            for(s32 J = 0;
                J < TempCount;
                ++J)
            {
                if(TRISUBIsNewPoint(OutputPoints, *OutputCount, Temp[J]))
                {
                    OutputPoints[(*OutputCount)++] = Temp[J];
                    ++AddedCount;
                }
            }

            Result[I].Count = AddedCount;
            Result[I].I0 = I;
            Result[I].I1 = (I + 1) % A->VertexCount;
            
            TempCount = 0;
        }
    }

    EndTemporaryMemory(TempMem);

    return(Result);
}

internal void
TRISUBBuildVertexTableFor(polygon2 *A, polygon2 *B, v2 *OutsidePoints, s32 OutsideCount, vertex_info *Table)
{
    for(s32 VertexIndex = 0;
        VertexIndex < A->VertexCount;
        ++VertexIndex)
    {
        v2 p = A->Vertices[VertexIndex];

        vertex_info *Info = Table + VertexIndex;
        Info->VertexIndex = VertexIndex;
        Info->Point = p;
        Info->Processed = false;

        for(s32 OutsideIndex = 0;
            OutsideIndex < OutsideCount;
            ++OutsideIndex)
        {
            v2 OutsidePoint = OutsidePoints[OutsideIndex];
            if(TRISUBPointsAreEqual(p, OutsidePoint))
            {
                Info->Outside = true;
                Info->Cross = -1;
                break;
            }
        }

        if(!Info->Outside)
        {
            Info->Cross = -1;
        }
    }
}

internal void
TRISUBRecordCrossingPoints(vertex_info *A, s32 CountA, vertex_info *B, s32 CountB)
{
    for(s32 I = 0;
        I < CountA;
        ++I)
    {
        vertex_info *VertexA = A + I;
        if(!VertexA->Outside)
        {
            for(s32 J = 0;
                J < CountB;
                ++J)
            {
                vertex_info *VertexB = B + J;
                if(TRISUBPointsAreEqual(VertexA->Point, VertexB->Point))
                {
                    VertexA->Cross = J;
                    VertexB->Cross = I;
                }
            }
        }
    }
}

internal b32
TRISUBConstructPolygonsFor(polygon2 *A, polygon2 *B, vertex_info *InfoA, vertex_info *InfoB, polygon2_set *Set)
{
    b32 Result = true;
    for(;;)
    {
        if(!Result)
        {
            break;
        }
        
        s32 IndexA = -1;
        for(s32 VertexInfoIndexA = 0;
            VertexInfoIndexA < A->VertexCount;
            ++VertexInfoIndexA)
        {
            vertex_info *PolygonAInfo = InfoA + VertexInfoIndexA;
            if(PolygonAInfo->Outside && !PolygonAInfo->Processed)
            {
                IndexA = PolygonAInfo->VertexIndex;
                polygon2 *Polygon = Set->Polygons + Set->PolygonCount;
                Polygon->Vertices = (v2 *)Platform.AllocateMemory(sizeof(v2)*TRISUB_MAX_POINTS_PER_POLYGON);

                ++Set->PolygonCount;
                break;
            }
        }

        polygon2 *CurrentPolygon = Set->Polygons + (Set->PolygonCount - 1);
        if(IndexA != -1)
        {
            for(;;)
            {
                if(CurrentPolygon->VertexCount >= TRISUB_MAX_POINTS_PER_POLYGON)
                {
                    Assert(!"FAIL");
                    Result = false;
                    break;
                }
                
                CurrentPolygon->Vertices[CurrentPolygon->VertexCount++] = InfoA[IndexA].Point; 
                InfoA[IndexA].Processed = true;

                // NOTE(babykaban): Check if polygon completed
                v2 FirstOutputPoint = v2(CurrentPolygon->Vertices[0]);
                v2 ThisPoint = InfoA[IndexA].Point;
                if(CurrentPolygon->VertexCount > 1)
                {
                    if(TRISUBPointsAreEqual(FirstOutputPoint, ThisPoint))
                    {
                        break;
                    }
                }

                s32 Cross = InfoA[IndexA].Cross;
                if(Cross != -1)
                {
                    s32 Next = ((Cross - 1) + B->VertexCount) % B->VertexCount; 
                    if(InfoB[Next].Outside)
                    {
                        Cross = -1;
                    }
                    else
                    {
                        s32 CrossBack = InfoB[Next].Cross;
                        if(CrossBack == (IndexA + 1) % A->VertexCount)
                        {
                            Cross = -1;
                        }
                    }
                }

                if(Cross != -1)
                {
                    s32 IndexB = Cross;
                    for(;;)
                    {
                        if(CurrentPolygon->VertexCount >= TRISUB_MAX_POINTS_PER_POLYGON)
                        {
                            Assert(!"FAIL");
                            Result = false;
                            break;
                        }

                        IndexB = ((IndexB - 1) + B->VertexCount) % B->VertexCount;
                        CurrentPolygon->Vertices[CurrentPolygon->VertexCount++] = InfoB[IndexB].Point; 

                        Cross = InfoB[IndexB].Cross;
                        if(Cross != -1)
                        {
                            s32 Next = (Cross + 1) % A->VertexCount;
                            if(!InfoA[Next].Outside)
                            {
                                Cross = -1;
                            }
                            else
                            {
                                s32 CrossBack = InfoA[Next].Cross;
                                if(CrossBack == ((IndexB - 1) + B->VertexCount) % B->VertexCount)
                                {
                                    Cross = -1;
                                }
                            }
                        }

                        if(Cross != -1)
                        {
                            IndexA = Cross + 1;
                            break;
                        }
                    }
                }
                else
                {
                    ++IndexA;
                }
            }
        }
        else
        {
            break;
        }
    }

    return(Result);
}
 
inline void
TRISUBRemoveDublicatPoints(polygon2 *Poly)
{
    for(s32 I = 0;
        I < Poly->VertexCount;
        ++I)
    {
        for(s32 J = I + 1;
            J < Poly->VertexCount;
            ++J)
        {
            if(TRISUBPointsAreEqual(Poly->Vertices[I], Poly->Vertices[J]))
            {
                Poly->Vertices[J] = {};
                for(s32 k = J;
                    k < (Poly->VertexCount - 1);
                    ++k)
                {
                    Poly->Vertices[k] = Poly->Vertices[k + 1]; 
                }

                --Poly->VertexCount;
                --J;
            }            
        }
    }
    Poly->Vertices[Poly->VertexCount] = {};
}

inline between_indecies
FindPolygon2EdgeIndeciesFor(polygon2 *A, v2 p)
{
    between_indecies Result = {};

    f32 Closest = Real32Maximum;
    for(s32 I = 0;
        I < A->VertexCount;
        I++)
    {
        s32 K = (I + 1) % A->VertexCount;

        v2 vertex1 = A->Vertices[I];
        v2 vertex2 = A->Vertices[K];

        f32 D = DistanceToSegment(p, vertex1, vertex2);
        if(D < Closest)
        {
            Closest = D;
            Result.Index0 = I;
            Result.Index1 = K;
        }
    }

    return(Result);
}

internal subtract_result
SubtractTriangels(triangle *Subject, triangle *Subtractor, f32 MinimalOverlapArea, f32 CollinearValue, f32 MinimalResultArea, memory_arena *Arena)
{
    TIMED_FUNCTION();

    subtract_result Result = {};
    Result.Set.Polygons = (polygon2 *)Platform.AllocateMemory(sizeof(polygon2)*TRISUB_MAX_POLYGON_COUNT);

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    b32 ExcludingOverlap = TRISUBTrianglesOverlapExcludingVerticesAndEdges(Subject, Subtractor);
    f32 AreaA = TriangleSignedArea(Subject);
    f32 AreaB = TriangleSignedArea(Subtractor);

    // NOTE(babykaban): Changing orrientation to be clockwise
    if(AreaA < 0.0f)
    {
        v2 Temp = Subject->Vertices[1];
        Subject->Vertices[1] = Subject->Vertices[2];
        Subject->Vertices[2] = Temp;
    }

    if(AreaB < 0.0f)
    {
        v2 Temp = Subtractor->Vertices[1];
        Subtractor->Vertices[1] = Subtractor->Vertices[2];
        Subtractor->Vertices[2] = Temp;
    }

    // NOTE(babykaban): Checks to ensure that both triangles are valid
    if(!IsTriangleCollinear(Subject, CollinearValue) && !IsTriangleCollinear(Subtractor, CollinearValue))
    {
        polygon2 PolygonA = {};
        PolygonA.Vertices = PushArray(TempMem.Arena, TRISUB_MAX_POINTS_PER_POLYGON, v2);

        PolygonA.Vertices[0] = Subject->Vertices[0];
        PolygonA.Vertices[1] = Subject->Vertices[1];
        PolygonA.Vertices[2] = Subject->Vertices[2];
        PolygonA.VertexCount = 3;

        // NOTE(babykaban): Constracting the overlap polygon
        polygon2 Poly = {};
        Poly.Vertices = PushArray(TempMem.Arena, TRISUB_MAX_POINTS_PER_POLYGON, v2);
        TRISUBSutherlandHodgman(&PolygonA, Subtractor, &Poly, TempMem.Arena);
        TRISUBCleanUpOverlapPolygon(&Poly);

        f32 OverlappingArea = AbsoluteValue(PolygonSignedArea(&Poly));
        if((OverlappingArea > MinimalOverlapArea) && ((AbsoluteValue(AreaA) - OverlappingArea) > MinimalResultArea))
        {
            // NOTE(babykaban): Checks if any points of a subtractor lies on first vertex of subject if so rotate points in array
            TRISUBHandleVertexCase(&PolygonA, &Poly);

            // NOTE(babykaban): Adding first vertices to the end, for algorithm to work properly
            PolygonA.Vertices[3] = PolygonA.Vertices[0];
            Poly.Vertices[Poly.VertexCount] = Poly.Vertices[0];
            ++PolygonA.VertexCount;
            ++Poly.VertexCount;

            // NOTE(babykaban): Find outside points for Subject polygon, because Overlap polygon will always be inside (on edge points are inside)
            s32 OutsidePointCount = 0;
            v2 *OutsidePoints = PushArray(TempMem.Arena, TRISUB_MAX_POINTS_PER_POLYGON, v2);
            TRISUBFindOutsidePointsFor(&PolygonA, &Poly, OutsidePoints, &OutsidePointCount);

            // NOTE(babykaban): Find intersection points for subject polygon
            s32 NewPointCount = 0;
            v2 *PointsToAdd = PushArray(TempMem.Arena, TRISUB_MAX_POINTS_PER_POLYGON, v2);
            points_between *PointTable = TRISUBGetIntersectionPoints(&PolygonA, &Poly, PointsToAdd, &NewPointCount, TempMem.Arena);

            // NOTE(babykaban): If no intersections was found record subtractor as a hole
            if(NewPointCount != 0)
            {
                // NOTE(babykaban): Insert points avoiding dublicats
                s32 PointIndex = 0;
                for(s32 I = 0;
                    I < 3;
                    ++I)
                {
                    points_between Between = PointTable[I];
                    while(Between.Count)
                    {
                        v2 p = PointsToAdd[PointIndex];
                        if(TRISUBIsNewPoint(&PolygonA, p))
                        {
                            TRISUBInsertPointBetween(&PolygonA, p, {Between.I0, Between.I1});

                            if(I == 0)
                            {
                                ++Between.I0;
                                ++Between.I1;
                                ++PointTable[1].I0;
                                ++PointTable[1].I1;
                                ++PointTable[2].I0;
                                ++PointTable[2].I1;
                            }
                            else if(I == 1)
                            {
                                ++Between.I0;
                                ++Between.I1;
                                ++PointTable[I + 1].I0;
                                ++PointTable[I + 1].I1;
                            }
                            else
                            {
                                ++Between.I0;
                                ++Between.I1;
                            }
                        }

                        Between.Count -= 1;
                        PointIndex += 1;
                    }
                }
                
                // NOTE(babykaban): Constract vertex table for both polygons
                vertex_info *PolygonAVertexInfo = PushArray(TempMem.Arena, PolygonA.VertexCount, vertex_info);
                TRISUBBuildVertexTableFor(&PolygonA, &Poly, OutsidePoints, OutsidePointCount, PolygonAVertexInfo);

                vertex_info *PolygonBVertexInfo = PushArray(TempMem.Arena, Poly.VertexCount, vertex_info);
                TRISUBBuildVertexTableFor(&Poly, &PolygonA, 0, 0, PolygonBVertexInfo);

                TRISUBRecordCrossingPoints(PolygonAVertexInfo, PolygonA.VertexCount, PolygonBVertexInfo, Poly.VertexCount);

                // NOTE(babykaban): Construct resulting polygons
                Result.Success = TRISUBConstructPolygonsFor(&PolygonA, &Poly, PolygonAVertexInfo, PolygonBVertexInfo, &Result.Set);

                for(s32 PolygonIndex = 0;
                    PolygonIndex < Result.Set.PolygonCount;
                    ++PolygonIndex)
                {
                    polygon2 *P = Result.Set.Polygons + PolygonIndex;
                    TRISUBRemoveDublicatPoints(P);

                    if((P->VertexCount < 3) || (AbsoluteValue(PolygonSignedArea(P)) < MinimalResultArea))
                    {
                        for(s32 J = PolygonIndex;
                            J < (Result.Set.PolygonCount - 1);
                            ++J)
                        {
                            Result.Set.Polygons[J].VertexCount = Result.Set.Polygons[J + 1].VertexCount;
                            Copy(sizeof(v2)*TRISUB_MAX_POINTS_PER_POLYGON, Result.Set.Polygons[J + 1].Vertices, Result.Set.Polygons[J].Vertices);
                        }

                        polygon2 *Remove = Result.Set.Polygons + (Result.Set.PolygonCount - 1);
                        Platform.DeallocateMemory(Remove->Vertices);
                        --Result.Set.PolygonCount;
                        --PolygonIndex;
                    }
                }
            }
            else
            {
                Result.Set.Polygons[0].Vertices = (v2 *)Platform.AllocateMemory(sizeof(v2)*3);

                Result.Set.Polygons[0].Vertices[0] = Subject->Vertices[0];
                Result.Set.Polygons[0].Vertices[1] = Subject->Vertices[1];
                Result.Set.Polygons[0].Vertices[2] = Subject->Vertices[2];
                Result.Set.Polygons[0].VertexCount = 3;

                Result.Set.Polygons[0].HasHoles = true;
                Result.Set.Polygons[0].HoleVertexCounts = (s32 *)Platform.AllocateMemory(sizeof(s32));
                Result.Set.Polygons[0].HolesVertices = (v2 *)Platform.AllocateMemory(sizeof(v2)*3);
                Result.Set.Polygons[0].HolesVertices[0] = Subtractor->Vertices[0];
                Result.Set.Polygons[0].HolesVertices[1] = Subtractor->Vertices[1];
                Result.Set.Polygons[0].HolesVertices[2] = Subtractor->Vertices[2];
                Result.Set.Polygons[0].HoleVertexCounts[0] = 3;

                Result.Set.Polygons[0].HoleCount = 1;
                Result.Set.PolygonCount = 1;

                Result.Success = true;
            }
        }
        else
        {
            if((AbsoluteValue(AreaA) - OverlappingArea) <= MinimalResultArea)
            {
                Result.FullyRemoved = true;
            }

            Result.Success = true;
        }
    }
    else
    {
        Result.Success = true;
    }

    EndTemporaryMemory(TempMem);

    return(Result);
}

// ===========================================================================================================================================================

// NOTE(babykaban): Triangulations ===========================================================================================================================

internal b32
PlanarPointWithinTriangle(v2d P, v2d A, v2d B, v2d C)
{
    v2d AB = A - B;
    v2d BC = B - C;
    v2d CA = C - A;
    v2d AP = A - P;
    v2d BP = B - P;
    v2d CP = C - P;

    v2d N1 = V2d(AB.y, -AB.x);
    v2d N2 = V2d(BC.y, -BC.x);
    v2d N3 = V2d(CA.y, -CA.x);
    
    f64 S1 = Inner(N1, AP);
    f64 S2 = Inner(N2, BP);
    f64 S3 = Inner(N3, CP);

    f64 Tolerance = COMMON_EPSILON_F64;

    b32 Result = (((S1 < 0) && (S2 < 0) && (S3 < 0)) ||
                  ((S1 < Tolerance) && (S2 < 0) && (S3 < 0)) ||
                  ((S2 < Tolerance) && (S1 < 0) && (S3 < 0)) ||
                  ((S3 < Tolerance) && (S1 < 0) && (S2 < 0)));

    return(Result);
}

internal b32
PlanarPointWithinTriangle(v2 P, v2 A, v2 B, v2 C)
{
    v2 AB = A - B;
    v2 BC = B - C;
    v2 CA = C - A;
    v2 AP = A - P;
    v2 BP = B - P;
    v2 CP = C - P;

    v2 N1 = V2(AB.y, -AB.x);
    v2 N2 = V2(BC.y, -BC.x);
    v2 N3 = V2(CA.y, -CA.x);
    
    f32 S1 = Inner(N1, AP);
    f32 S2 = Inner(N2, BP);
    f32 S3 = Inner(N3, CP);

    f32 Tolerance = 0.0001f;

    b32 Result = (((S1 < 0) && (S2 < 0) && (S3 < 0))||
                  ((S1 < Tolerance) && (S2 < 0) && (S3 < 0))||
                  ((S2 < Tolerance) && (S1 < 0) && (S3 < 0))||
                  ((S3 < Tolerance) && (S1 < 0) && (S2 < 0)));

    return(Result);
}

inline void
SplitTriangle(triangulate_triangle *TriangleArray, s32 SubjectIndex, s32 FirstNewIndex, s32 SecondNewIndex, s32 CurrentPointIndex)
{
    // vertices of new triangles
    triangulate_triangle *Subject = TriangleArray + SubjectIndex;
    triangulate_triangle *FirstNewT = TriangleArray + FirstNewIndex;
    triangulate_triangle *SecondNewT = TriangleArray + SecondNewIndex;

    FirstNewT->V1 = CurrentPointIndex;
    FirstNewT->V2 = Subject->V2;
    FirstNewT->V3 = Subject->V3;

    SecondNewT->V1 = CurrentPointIndex;
    SecondNewT->V2 = Subject->V3;
    SecondNewT->V3 = Subject->V1;

    // update adjacencies of triangles surrounding the old triangle
    // fix adjacency of Subject(old) triangle
    s32 Adj1 = Subject->AdjV1V2;
    s32 Adj2 = Subject->AdjV2V3;
    s32 Adj3 = Subject->AdjV3V1;

    if(Adj1 >= 0)
    {
        if(TriangleArray[Adj1].AdjV1V2 == SubjectIndex)
            TriangleArray[Adj1].AdjV1V2 = SubjectIndex;
        else if(TriangleArray[Adj1].AdjV2V3 == SubjectIndex)
            TriangleArray[Adj1].AdjV2V3 = SubjectIndex;
        else if(TriangleArray[Adj1].AdjV3V1 == SubjectIndex)
            TriangleArray[Adj1].AdjV3V1 = SubjectIndex;
    }

    if(Adj2 >= 0)
    {
        if(TriangleArray[Adj2].AdjV1V2 == SubjectIndex)
            TriangleArray[Adj2].AdjV1V2 = FirstNewIndex;
        else if(TriangleArray[Adj2].AdjV2V3 == SubjectIndex)
            TriangleArray[Adj2].AdjV2V3 = FirstNewIndex;
        else if(TriangleArray[Adj2].AdjV3V1 == SubjectIndex)
            TriangleArray[Adj2].AdjV3V1 = FirstNewIndex;
    }

    if(Adj3 >= 0)
    {
        if(TriangleArray[Adj3].AdjV1V2 == SubjectIndex)
            TriangleArray[Adj3].AdjV1V2 = SecondNewIndex;
        else if(TriangleArray[Adj3].AdjV2V3 == SubjectIndex)
            TriangleArray[Adj3].AdjV2V3 = SecondNewIndex;
        else if(TriangleArray[Adj3].AdjV3V1 == SubjectIndex)
            TriangleArray[Adj3].AdjV3V1 = SecondNewIndex;
    }

    // adjacencies of new triangles 
    FirstNewT->AdjV1V2 = SubjectIndex;
    FirstNewT->AdjV2V3 = Subject->AdjV2V3;
    FirstNewT->AdjV3V1 = SecondNewIndex;

    SecondNewT->AdjV1V2 = FirstNewIndex;
    SecondNewT->AdjV2V3 = Subject->AdjV3V1;
    SecondNewT->AdjV3V1 = SubjectIndex;

    // replace v3 of containing triangle with P and rotate to v1
    Subject->V3 = Subject->V2;
    Subject->V2 = Subject->V1;
    Subject->V1 = CurrentPointIndex;

    // replace 1st and 3rd adjacencies of containing triangle with new triangles
    Subject->AdjV2V3 = Subject->AdjV1V2;
    Subject->AdjV3V1 = FirstNewIndex;
    Subject->AdjV1V2 = SecondNewIndex;
}

inline void
AdjacenciesFindWhichEqualAndSetTo(triangulate_triangle *Test, s32 Compare, s32 Set)
{
    /*
      NOTE(babykaban):
      The function checks all three adjacency entries (AdjV1V2, AdjV2V3, AdjV3V1) of the triangle it's called on.
      It searches for the adjacency value that equals the old neighbor's index (Compare).
      It replaces that old neighbor index with the new one (Set), reflecting the new connection after the flip.
    */

    for(s32 m = 0; m < 3; m++)
    {
        if(Test->Adjacencies[m] == Compare)
        {
            Test->Adjacencies[m] = Set;
            break;
        }
    }
}

inline rectangle2d
CalculateBoundsForPoints(v2d *Points, s32 PointCount)
{
    rectangle2d Bounds = InvertedInfinityRectangle2d();
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
        if(Points[I].x < Bounds.Min.x) Bounds.Min.x = Points[I].x;
        if(Points[I].x > Bounds.Max.x) Bounds.Max.x = Points[I].x;
        if(Points[I].y < Bounds.Min.y) Bounds.Min.y = Points[I].y;
        if(Points[I].y > Bounds.Max.y) Bounds.Max.y = Points[I].y;
    }

    return(Bounds);
}

inline rectangle2
CalculateBoundsForPoints(v2 *Points, s32 PointCount)
{
    rectangle2 Bounds = InvertedInfinityRectangle2();
    for(s32 I = 0;
        I < PointCount;
        ++I)
    {
        if(Points[I].x < Bounds.Min.x) Bounds.Min.x = Points[I].x;
        if(Points[I].x > Bounds.Max.x) Bounds.Max.x = Points[I].x;
        if(Points[I].y < Bounds.Min.y) Bounds.Min.y = Points[I].y;
        if(Points[I].y > Bounds.Max.y) Bounds.Max.y = Points[I].y;
    }

    return(Bounds);
}

internal triangulate_result
DelaunayTriangulate(polygon2 *Poly, memory_arena *Arena)
{
    TIMED_FUNCTION();

    triangulate_result Result = {};
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    if(Poly->VertexCount > 3)
    {
        s32 PointCount = Poly->VertexCount;
        v2 *Points = PushArray(TempMem.Arena, PointCount + 3, v2);
        Copy(sizeof(v2)*PointCount, Poly->Vertices, Points);

        // NOTE(babykaban): Find boundaries of Poly
        rectangle2 Bounds = CalculateBoundsForPoints(Points, PointCount);

        // NOTE(babykaban): Remap everything (preserving the aspect ratio) to between (0,0)-(1,1)
        v2 BoundsDim = GetDim(Bounds);
        f32 d = BoundsDim.y; // d = largest dimension
        if(BoundsDim.x > d) d = BoundsDim.x;
        f32 OneOverd = 1.0f / d;
    
        for(s32 I = 0;
            I < PointCount;
            ++I)
        {
            Points[I] = OneOverd*(Points[I] - Bounds.Min);
        }

        // NOTE(babykaban): Sort points by proximity
        s32 BinRowsCount = CeilReal32ToInt32((f32)pow(PointCount, 0.25f));
        s32 *Bins = PushArray(TempMem.Arena, PointCount, s32);
        for(s32 I = 0;
            I < PointCount;
            ++I)
        {
            s32 p = (s32)(Points[I].y*BinRowsCount*0.999f); // bin row
            s32 q = (int)(Points[I].x*BinRowsCount*0.999f); // bin column
            if(p % 2) Bins[I] = (p + 1)*BinRowsCount - q;
            else Bins[I] = p*BinRowsCount + (q + 1);
        }

        // NOTE(babykaban): Will be used in after processing
        s32 *PointOrder = PushArray(TempMem.Arena, Poly->VertexCount, s32);
        for(s32 I = 0; I < Poly->VertexCount; ++I)
            PointOrder[I] = I;

        // NOTE(babykaban): Insertion sort
        s32 Key;
        for(s32 I = 1;
            I < PointCount;
            ++I)
        {
            Key = Bins[I];
            v2 Temp = Points[I];
            s32 TempI = PointOrder[I];
            s32 J = I - 1;
            while((J >= 0) && (Bins[J] > Key))
            {
                Bins[J + 1] = Bins[J];
                Points[J + 1] = Points[J];
                PointOrder[J + 1] = PointOrder[J];
                --J;
            }

            Bins[J + 1] = Key;
            Points[J + 1] = Temp;
            PointOrder[J + 1] = TempI;
        }

        // NOTE(babykaban): Add big triangle around point cloud
        Points[PointCount] = V2(-100.0f, -100.0f);
        Points[PointCount + 1] = V2(100.0f, -100.0f);
        Points[PointCount + 2] = V2(0.0f, 100.0f);
        PointCount += 3;

        triangulate_triangle *Triangles = PushArray(TempMem.Arena, PointCount*3, triangulate_triangle);
        Triangles[0].Vertices[0] = PointCount - 3;
        Triangles[0].Vertices[1] = PointCount - 2;
        Triangles[0].Vertices[2] = PointCount - 1;
        Triangles[0].Adjacencies[0] = -1;
        Triangles[0].Adjacencies[1] = -1;
        Triangles[0].Adjacencies[2] = -1;

        s32 TriangleCount = 1;
        s32 *TriangleStack = PushArray(TempMem.Arena, (PointCount - 3), s32); // is this a big enough stack?
        s32 Tos = -1;

        // NOTE(babykaban): Insert all points and triangulate one by one
        for(s32 CurrentPointIndex = 0;
            CurrentPointIndex < (PointCount - 3);
            ++CurrentPointIndex)
        {
            // NOTE(babykaban): Find triangle T which contains Points[CurrentPointIndex]
            s32 LastCreatedIndex = TriangleCount - 1; // Last triangle created
            while(1)
            {
                triangulate_triangle *LastCreatedT = Triangles + LastCreatedIndex; // Triangle T
                if(PlanarPointWithinTriangle(Points[CurrentPointIndex], Points[LastCreatedT->V1],
                                             Points[LastCreatedT->V2], Points[LastCreatedT->V3]))
                {
                    TriangleCount += 2;

                    s32 FirstNewIndex = TriangleCount - 2;
                    s32 SecondNewIndex = TriangleCount - 1;

                    // NOTE(babykaban): Delete triangle T and replace it with three sub-triangles touching P 
                    SplitTriangle(Triangles, LastCreatedIndex, FirstNewIndex, SecondNewIndex, CurrentPointIndex);

                    // NOTE(babykaban): Place each triangle containing P(Points[CurrentPointIndex]) onto a stack,
                    // if the edge opposite P(Points[CurrentPointIndex]) has an adjacent triangle
                    if(LastCreatedT->AdjV2V3 >= 0)
                        TriangleStack[++Tos] = LastCreatedIndex;
                    if(Triangles[FirstNewIndex].AdjV2V3 >= 0)
                        TriangleStack[++Tos] = FirstNewIndex;
                    if(Triangles[SecondNewIndex].AdjV2V3 >=0)
                        TriangleStack[++Tos]= SecondNewIndex;

                    while(Tos >= 0)
                    {
                        v2 P = Points[CurrentPointIndex];

                        // NOTE(babykaban): Looping thru the stack
                        s32 TestTIndex = TriangleStack[Tos--];
                        // NOTE(babykaban): TestT, triangle at the top of the stack that is being tested.
                        triangulate_triangle *TestT = Triangles + TestTIndex;
                        v2 Vertex1 = Points[TestT->V3];
                        v2 Vertex2 = Points[TestT->V2];

                        s32 OpositeTIndex = TestT->AdjV2V3;
                        // NOTE(babykaban): OpositeT, triangle adjacent to TestT across the edge formed by Vertex1 and Vertex2.
                        triangulate_triangle *OpositeT = Triangles + OpositeTIndex;

                        s32 OppVert = -1;
                        s32 OppVertID = -1;
                        for(s32 k = 0; k < 3; ++k)
                        {
                            if((OpositeT->Vertices[k] != TestT->V2)
                               && (OpositeT->Vertices[k] != TestT->V3))
                            {
                                OppVert = OpositeT->Vertices[k];
                                OppVertID = k;
                                break;
                            }
                        }

                        v2 Vertex3 = Points[OppVert];
                    
                        // NOTE(babykaban): Check if P in circumcircle of TestT
                        f32 cosa = Inner(Vertex1 - Vertex3, Vertex2 - Vertex3);
                        f32 cosb = Inner(Vertex2 - P, Vertex1 - P);
                        f32 sina = Cross(Vertex1 - Vertex3, Vertex2 - Vertex3);
                        f32 sinb = Cross(Vertex2 - P, Vertex1 - P);

                        if(((cosa < 0) && (cosb < 0)) || ((-cosa*sinb) > (cosb*sina)))
                        {
                            s32 AdjacencentToTestTIndex = TestT->AdjV3V1;
                            s32 AdjacencentToOpositeTIndex = OpositeT->Adjacencies[(OppVertID + 2) % 3]; // A

                            triangulate_triangle *AdjacencentToTestT = Triangles + AdjacencentToTestTIndex;
                            triangulate_triangle *AdjacencentToOpositeT = Triangles + AdjacencentToOpositeTIndex; // C
                        
                            // NOTE(babykaban): Fix adjacency of A
                            if(AdjacencentToOpositeTIndex >= 0)
                            {
                                AdjacenciesFindWhichEqualAndSetTo(AdjacencentToOpositeT, OpositeTIndex, TestTIndex);
                            }

                            // NOTE(babykaban): Fix adjacency of C
                            if(AdjacencentToTestTIndex >= 0)
                            {
                                AdjacenciesFindWhichEqualAndSetTo(AdjacencentToTestT, TestTIndex, OpositeTIndex);
                            }

                            // NOTE(babykaban): Fix vertices and adjacency of OpositeT
                            for(s32 m = 0; m < 3; m++)
                            {
                                if(OpositeT->Vertices[m] == OppVert)
                                {
                                    OpositeT->Vertices[(m + 2) % 3] = CurrentPointIndex;
                                    break;
                                }
                            }
                            AdjacenciesFindWhichEqualAndSetTo(OpositeT, TestTIndex, AdjacencentToTestTIndex);
                            AdjacenciesFindWhichEqualAndSetTo(OpositeT, AdjacencentToOpositeTIndex, TestTIndex);

                            for(s32 m = 0; m < 3; m++)
                            {
                                if(OpositeT->V1 != CurrentPointIndex)
                                {
                                    s32 Temp1 = OpositeT->V1;
                                    s32 Temp2 = OpositeT->AdjV1V2;

                                    OpositeT->V1 = OpositeT->V2;
                                    OpositeT->V2 = OpositeT->V3;
                                    OpositeT->V3 = Temp1;

                                    OpositeT->AdjV1V2 = OpositeT->AdjV2V3;
                                    OpositeT->AdjV2V3 = OpositeT->AdjV3V1;
                                    OpositeT->AdjV3V1 = Temp2;
                                }
                            }
                        
                            // NOTE(babykaban): Fix vertices and adjacency of TestT
                            TestT->V3 = OppVert;
                            AdjacenciesFindWhichEqualAndSetTo(TestT, AdjacencentToTestTIndex, OpositeTIndex);
                            AdjacenciesFindWhichEqualAndSetTo(TestT, OpositeTIndex, AdjacencentToOpositeTIndex);

                            // NOTE(babykaban): Add TestT and OpositeT to stack if they have triangles opposite P;
                            if(TestT->AdjV2V3 >= 0) TriangleStack[++Tos] = TestTIndex;
                            if(OpositeT->AdjV2V3 >= 0) TriangleStack[++Tos] = OpositeTIndex;
                        }
                    }

                    break;
                }
        
                // NOTE(babykaban): Adjust LastCreatedIndex in the direction of target point CurrentPointIndex
                v2 AB = Points[LastCreatedT->V2] - Points[LastCreatedT->V1];
                v2 BC = Points[LastCreatedT->V3] - Points[LastCreatedT->V2];
                v2 CA = Points[LastCreatedT->V1] - Points[LastCreatedT->V3];

                v2 AP = Points[CurrentPointIndex] - Points[LastCreatedT->V1];
                v2 BP = Points[CurrentPointIndex] - Points[LastCreatedT->V2];
                v2 CP = Points[CurrentPointIndex] - Points[LastCreatedT->V3];

                v2 N1 = V2(AB.y, -AB.x);
                v2 N2 = V2(BC.y, -BC.x);
                v2 N3 = V2(CA.y, -CA.x);

                f32 S1 = Inner(AP, N1);
                f32 S2 = Inner(BP, N2);
                f32 S3 = Inner(CP, N3);

                if((S1 >= 0) && (S1 >= S2) && (S1 >= S3)) LastCreatedIndex = LastCreatedT->AdjV1V2;
                else if((S2 >= 0) && (S2 >= S1) && (S2 >= S3)) LastCreatedIndex = LastCreatedT->AdjV2V3;
                else if((S3 >= 0) && (S3 >= S1) && (S3 >= S2)) LastCreatedIndex = LastCreatedT->AdjV3V1;
            }
        }

        {    
            TIMED_BLOCK("TRIANGULATION After Processing");

            // NOTE(babykaban): Count how many triangles there are that dont involve supertriangle vertices
            s32 PreFinalTriangleCount = TriangleCount;
            s32 *RenumberAdj = PushArray(TempMem.Arena, TriangleCount, s32);
            b32 *DeadTris = PushArray(TempMem.Arena, TriangleCount, b32);
            PointCount -= 3;
            for(s32 I = 0; I < TriangleCount; I++)
            {
                if((Triangles[I].V1 >= PointCount) ||
                   (Triangles[I].V2 >= PointCount) ||
                   (Triangles[I].V3 >= PointCount))
                {
                    DeadTris[I] = 1;
                    RenumberAdj[I] = TriangleCount - (PreFinalTriangleCount--);
                }
                else
                {
                    RenumberAdj[I] = TriangleCount - PreFinalTriangleCount;
                }
            }

            // NOTE(babykaban): Delete any triangles that contain the supertriangle vertices 
            triangulate_triangle *PreFinalTriangles = PushArray(TempMem.Arena, PreFinalTriangleCount, triangulate_triangle);

            s32 CurrentIndex = 0;
            for(s32 I = 0; I < TriangleCount; I++)
            {
                if((Triangles[I].V1 < PointCount) &&
                   (Triangles[I].V2 < PointCount) &&
                   (Triangles[I].V3 < PointCount))
                {
                    PreFinalTriangles[CurrentIndex] = Triangles[I];

                    PreFinalTriangles[CurrentIndex].AdjV1V2 = (1 - DeadTris[Triangles[I].AdjV1V2])*Triangles[I].AdjV1V2 - DeadTris[Triangles[I].AdjV1V2];
                    PreFinalTriangles[CurrentIndex].AdjV2V3 = (1 - DeadTris[Triangles[I].AdjV2V3])*Triangles[I].AdjV2V3 - DeadTris[Triangles[I].AdjV2V3];
                    PreFinalTriangles[CurrentIndex].AdjV3V1 = (1 - DeadTris[Triangles[I].AdjV3V1])*Triangles[I].AdjV3V1 - DeadTris[Triangles[I].AdjV3V1];
                    ++CurrentIndex;
                }

            }

            // NOTE(babykaban): Fix adjacencies of PreFinalTriangles
            for(s32 i = 0; i < PreFinalTriangleCount; i++)
            {
                if (PreFinalTriangles[i].AdjV1V2 >= 0)
                    PreFinalTriangles[i].AdjV1V2 -= RenumberAdj[PreFinalTriangles[i].AdjV1V2];
                if (PreFinalTriangles[i].AdjV2V3 >= 0)
                    PreFinalTriangles[i].AdjV2V3 -= RenumberAdj[PreFinalTriangles[i].AdjV2V3];
                if (PreFinalTriangles[i].AdjV3V1 >= 0)
                    PreFinalTriangles[i].AdjV3V1 -= RenumberAdj[PreFinalTriangles[i].AdjV3V1];
            }

            // NOTE(babykaban): Undo the mapping
            if(BoundsDim.x > d)
                d = BoundsDim.x;

            for(s32 i = 0; i < PointCount; i++)
            {
                Points[i] = Points[i]*d + Bounds.Min;
            }
        
            Result.TriangleCount = PreFinalTriangleCount;
            Result.Triangles = (triangle *)Platform.AllocateMemory(sizeof(triangle)*PreFinalTriangleCount);
            Result.Adjacencies = (triangle_adjs *)Platform.AllocateMemory(sizeof(triangle_adjs)*PreFinalTriangleCount);

            // NOTE(babykaban): Finilize result
            for(s32 I = 0;
                I < PreFinalTriangleCount;
                ++I)
            {
                triangulate_triangle Triangle = PreFinalTriangles[I];
                triangle *T = Result.Triangles + I;
                triangle_adjs *Adjs = Result.Adjacencies + I;

                T->Vertices[0] = Points[Triangle.V1];
                T->Vertices[1] = Points[Triangle.V2];
                T->Vertices[2] = Points[Triangle.V3];
            
                Adjs->Adjacencies[0] = Triangle.Adjacencies[0];
                Adjs->Adjacencies[1] = Triangle.Adjacencies[1];
                Adjs->Adjacencies[2] = Triangle.Adjacencies[2];
            }
        }
    }
    else
    {
        if(Poly->VertexCount == 3)
        {
            Result.TriangleCount = 1;
            Result.Triangles = (triangle *)Platform.AllocateMemory(sizeof(triangle));
            Result.Adjacencies = (triangle_adjs *)Platform.AllocateMemory(sizeof(triangle_adjs));
            triangle *T = Result.Triangles + 0;
            triangle_adjs *Adjs = Result.Adjacencies + 0;

            T->Vertices[0] = Poly->Vertices[0];
            T->Vertices[1] = Poly->Vertices[1];
            T->Vertices[2] = Poly->Vertices[2];
            
            Adjs->Adjacencies[0] = -1;
            Adjs->Adjacencies[1] = -1;
            Adjs->Adjacencies[2] = -1;
        }
    }
    
    EndTemporaryMemory(TempMem);

    return(Result);
}

inline s32
GetIndex(s32 Count, s32 Index)
{
    s32 Result = Index;
    if(Index >= Count)
    {
        Result = Index % Count;
    }
    else if(Index < 0)
    {
        Result = (Index + Count) % Count;
    }

    return(Result);
}

union tri_verts
{
    struct
    {
        s32 V1, V2, V3;
    };

    s32 Vertices[3];
};

internal void
EarClipTriangulate(render_group *RenderGroup, object_transform *Flat, polygon2 *Poly, memory_arena *Arena)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    if(Poly->VertexCount > 3)
    {
        s32 IndexCount = Poly->VertexCount;
        s32 *IndexArray = PushArray(TempMem.Arena, IndexCount, s32);
        for(s32 I = 0; I < IndexCount; ++I)
        {
            IndexArray[I] = I;
        }

        s32 TriangleCount = Poly->VertexCount - 2;
        tri_verts *Triangles = PushArray(TempMem.Arena, TriangleCount, tri_verts);

        b32 Clockwise = (PolygonSignedArea(Poly) < 0.0f);
    
        s32 PrevOffset = Clockwise ? -1 : 1;
        s32 NextOffset = Clockwise ? 1 : -1;
    
        s32 TriangleIndex = 0;
        while(IndexCount > 3)
        {
            for(s32 I = 0; I < IndexCount; ++I)
            {
                s32 Cur = IndexArray[I];
                s32 Prev = IndexArray[GetIndex(IndexCount, I + PrevOffset)];
                s32 Next = IndexArray[GetIndex(IndexCount, I + NextOffset)];

                v2 a = Poly->Vertices[Cur];
                v2 b = Poly->Vertices[Prev];
                v2 c = Poly->Vertices[Next];

                v2 ab = b - a;
                v2 ac = c - a;
            
                if(Cross(ab, ac) > 0.0f)
                {
                    b32 IsEar = true;
                    for(s32 J = 0; J < Poly->VertexCount; ++J)
                    {
                        if((J != Cur) && (J != Prev) && (J != Next))
                        {
                            if(IsInTriangle(Poly->Vertices[J], b, a, c))
                            {
                                IsEar = false;
                                break;
                            }
                        }
                    }

                    if(IsEar)
                    {
                        Triangles[TriangleIndex].V1 = Prev;
                        Triangles[TriangleIndex].V2 = Cur;
                        Triangles[TriangleIndex].V3 = Next;
                        ++TriangleIndex;

                        S32RemoveAt(IndexArray, IndexCount, I);
                        --IndexCount;
                        break;
                    }
                }
            }
        }

        if(Clockwise)
        {
            Triangles[TriangleIndex].V1 = IndexArray[0];
            Triangles[TriangleIndex].V2 = IndexArray[1];
            Triangles[TriangleIndex].V3 = IndexArray[2];
            ++TriangleIndex;
        }
        else
        {
            Triangles[TriangleIndex].V1 = IndexArray[2];
            Triangles[TriangleIndex].V2 = IndexArray[1];
            Triangles[TriangleIndex].V3 = IndexArray[0];
            ++TriangleIndex;
        }
        
        Assert(TriangleCount == TriangleIndex);
        
        for(s32 I = 0;
            I < TriangleCount;
            ++I)
        {
            triangle T = {};
            T.Vertices[0] = Poly->Vertices[Triangles[I].V1];
            T.Vertices[1] = Poly->Vertices[Triangles[I].V2];
            T.Vertices[2] = Poly->Vertices[Triangles[I].V3];

            PushTriangle(RenderGroup, Flat, T, 30.0f, V4(DebugColorTable[I + 2], 1));
        }
    }
    
    EndTemporaryMemory(TempMem);
}

struct edge_list 
{
    edge_list *Next;
    s32 Count;
    s32 Points[2];
};

// return 1 if p3 is left of p1->p2, 0 if right
inline b32
pointDirectionFromLineSegment2D(v2d p1, v2d p2, v2d p3)
{
    f64 CrossProduct = Cross(p2 - p1, p3 - p1);
    b32 Result = CrossProduct > 0.0f;

    return(Result);
}

// return 1 if p1->p2 crosses p3->p4
inline b32
LineSegmentsCross2D(v2d p1, v2d p2, v2d p3, v2d p4)
{
    b32 Result = true;
    
    v2d p12 = p2 - p1;
    v2d p23 = p3 - p2;
    v2d p24 = p4 - p2;

    f64 cp1 = Cross(p12, p23);
    f64 cp2 = Cross(p12, p24);

    if((cp1*cp2) >= 0.0f) // 1st orientation test proves no intersection possible
    {
        Result = false;
    }
    else
    {
        v2d p34 = p4 - p3;
        v2d p41 = p1 - p4;
        v2d p42 = p2 - p4; // this is the opposite of v24 above, can consolidate for performance

        cp1 = Cross(p34, p41);
        cp2 = Cross(p34, p42);

        if((cp1*cp2) >= 0.0f) // 2nd orientation test proves no intersection possible
        {
            Result = false;
        }
    }

    return(Result);
}


// returns 1 if quadrilateral p1-p2-p3-p4 is convex
inline b32
QuadrilateralConvex2D(v2d p1, v2d p2, v2d p3, v2d p4)
{
    v2d sides[4]={p2 - p1, p3 - p2, p4 - p3, p1 - p4};

    f64 cp;
    b32 sign = false;
    for(s32 i = 0; i < 4;i++)
    {
        cp = Cross(sides[i], sides[(i + 1) % 4]);
        if(i == 0)
            sign = (cp > 0);
        else if(sign != (cp > 0))
            return 0;
    }

    return 1;
}

union v2_m256d
{
    struct
    {
        __m256d x, y;
    };

    __m256d E[2];
};

inline __m256d
Cross(v2_m256d A, v2_m256d B)
{
    __m256d Result = _mm256_sub_pd(_mm256_mul_pd(A.x, B.y), _mm256_mul_pd(A.y, B.x));

    return(Result);
}

// return 1 if p1->p2 crosses p3->p4
inline __m256d
SIMDLineSegmentsCross2D(v2_m256d p1, v2_m256d p2, v2_m256d p3, v2_m256d p4)
{
    __m256d Zero_4x = _mm256_set1_pd(0.0f);
    __m256d Neg1_4x = _mm256_set1_pd(-1.0f);
    
    v2_m256d p12 = {_mm256_sub_pd(p2.x, p1.x), _mm256_sub_pd(p2.y, p1.y)};
    v2_m256d p23 = {_mm256_sub_pd(p3.x, p2.x), _mm256_sub_pd(p3.y, p2.y)};
    v2_m256d p24 = {_mm256_sub_pd(p4.x, p2.x), _mm256_sub_pd(p4.y, p2.y)};

    v2_m256d p34 = {_mm256_sub_pd(p4.x, p3.x), _mm256_sub_pd(p4.y, p3.y)};
    v2_m256d p41 = {_mm256_sub_pd(p1.x, p4.x), _mm256_sub_pd(p1.y, p4.y)};
    v2_m256d p42 = {_mm256_sub_pd(p2.x, p4.x), _mm256_sub_pd(p2.y, p4.y)};

    __m256d cp1 = Cross(p12, p23);
    __m256d cp2 = Cross(p12, p24);

    __m256d FirstResult = _mm256_cmp_pd(_mm256_mul_pd(cp1,cp2), Zero_4x, _CMP_GE_OQ);

    cp1 = Cross(p34, p41);
    cp2 = Cross(p34, p42);

    __m256d SecondResult = _mm256_cmp_pd(_mm256_mul_pd(cp1,cp2), Zero_4x, _CMP_GE_OQ);

    FirstResult = _mm256_or_pd(FirstResult, SecondResult);
    
    return(FirstResult);
}

struct lined
{
    v2d a;
    v2d b;
};

struct line
{
    v2 a;
    v2 b;
};

internal s32
MarkOutsideTriangles(polygon2 *Poly, v2d *Points, triangulate_triangle *Triangles, s32 TriangleCount, b32 *IsOutside, memory_arena *Arena)
{
    TIMED_FUNCTION();

    s32 Result = TriangleCount;
    
    u32 *Counts = PushArray(Arena, TriangleCount, u32, Align(16, true));
    lined *TestLines = PushArray(Arena, TriangleCount, lined);

    __m128i Zero_4x = _mm_set1_epi32(0);
    __m128i One_4x = _mm_set1_epi32(1);
    f32 OneOverThree = 1.0f / 3.0f;
    for(s32 I = 0;
        I < TriangleCount;
        ++I)
    {
        triangulate_triangle *T = Triangles + I;
        lined *Line = TestLines + I;
        Line->a = OneOverThree*(Points[T->V1] + Points[T->V2] + Points[T->V3]);
        Line->b = V2d(13.0f*Line->a.x, 17.0f*Line->a.y);//V2(17.0f*(AbsoluteValue(Line->a.x) + 3.0f), 33.0f*Line->a.y);
    }

    for(s32 VertexIndex = 0;
        VertexIndex < Poly->VertexCount;
        ++VertexIndex)
    {
        v2 p1 = Poly->Vertices[VertexIndex];
        v2 p2 = Poly->Vertices[(VertexIndex + 1) % Poly->VertexCount];

        v2_m256d p1_4x = {_mm256_set1_pd(p1.x), _mm256_set1_pd(p1.y)};
        v2_m256d p2_4x = {_mm256_set1_pd(p2.x), _mm256_set1_pd(p2.y)};
        
        for(s32 LineIndex = 0;
            LineIndex < TriangleCount;
            LineIndex += 4)
        {
            s32 I0 = LineIndex;
            s32 I1 = LineIndex + 1;
            s32 I2 = LineIndex + 2;
            s32 I3 = LineIndex + 3;

            v2_m256d p3 =
                {
                    _mm256_set_pd(TestLines[I3].a.x, TestLines[I2].a.x, TestLines[I1].a.x, TestLines[I0].a.x),
                    _mm256_set_pd(TestLines[I3].a.y, TestLines[I2].a.y, TestLines[I1].a.y, TestLines[I0].a.y),
                };

            v2_m256d p4 =
                {
                    _mm256_set_pd(TestLines[I3].b.x, TestLines[I2].b.x, TestLines[I1].b.x, TestLines[I0].b.x),
                    _mm256_set_pd(TestLines[I3].b.y, TestLines[I2].b.y, TestLines[I1].b.y, TestLines[I0].b.y),
                };

            __m128i Result = _mm256_cvtpd_epi32(SIMDLineSegmentsCross2D(p1_4x, p2_4x, p3, p4));

            __m128i Mask = _mm_cmpeq_epi32(Result, Zero_4x);
            __m128i Inc = _mm_and_si128(Mask, One_4x);
            __m128i Counts_4x = _mm_load_si128((__m128i *)&Counts[LineIndex]);
            Counts_4x = _mm_add_epi32(Counts_4x, Inc);
            _mm_store_si128((__m128i *)&Counts[LineIndex], Counts_4x);
        }
    }

    s32 Offset = 0; 
    for(s32 HoleIndex = 0;
        HoleIndex < Poly->HoleCount;
        ++HoleIndex)
    {
        s32 VertexCount = Poly->HoleVertexCounts[HoleIndex];
        for(s32 VertexIndex = 0;
            VertexIndex < VertexCount;
            ++VertexIndex)
        {
            v2 p1 = Poly->HolesVertices[Offset + VertexIndex];
            v2 p2 = Poly->HolesVertices[Offset + ((VertexIndex + 1) % VertexCount)];

            v2_m256d p1_4x = {_mm256_set1_pd(p1.x), _mm256_set1_pd(p1.y)};
            v2_m256d p2_4x = {_mm256_set1_pd(p2.x), _mm256_set1_pd(p2.y)};
        
            for(s32 LineIndex = 0;
                LineIndex < TriangleCount;
                LineIndex += 4)
            {
                s32 I0 = LineIndex;
                s32 I1 = LineIndex + 1;
                s32 I2 = LineIndex + 2;
                s32 I3 = LineIndex + 3;

                v2_m256d p3 =
                    {
                        _mm256_set_pd(TestLines[I3].a.x, TestLines[I2].a.x, TestLines[I1].a.x, TestLines[I0].a.x),
                        _mm256_set_pd(TestLines[I3].a.y, TestLines[I2].a.y, TestLines[I1].a.y, TestLines[I0].a.y),
                    };

                v2_m256d p4 =
                    {
                        _mm256_set_pd(TestLines[I3].b.x, TestLines[I2].b.x, TestLines[I1].b.x, TestLines[I0].b.x),
                        _mm256_set_pd(TestLines[I3].b.y, TestLines[I2].b.y, TestLines[I1].b.y, TestLines[I0].b.y),
                    };

                __m128i Result = _mm256_cvtpd_epi32(SIMDLineSegmentsCross2D(p1_4x, p2_4x, p3, p4));

                __m128i Mask = _mm_cmpeq_epi32(Result, Zero_4x);
                __m128i Inc = _mm_and_si128(Mask, One_4x);
                __m128i Counts_4x = _mm_load_si128((__m128i *)&Counts[LineIndex]);
                Counts_4x = _mm_add_epi32(Counts_4x, Inc);
                _mm_store_si128((__m128i *)&Counts[LineIndex], Counts_4x);
            }
        }

        Offset += VertexCount;
    }

    for(s32 I = 0;
        I < TriangleCount;
        ++I)
    {
        if((Counts[I] % 2) == 0)
        {
            triangulate_triangle *T = Triangles + I;
            AdjacenciesFindWhichEqualAndSetTo(Triangles + T->AdjV1V2, I, -1);
            AdjacenciesFindWhichEqualAndSetTo(Triangles + T->AdjV2V3, I, -1);
            AdjacenciesFindWhichEqualAndSetTo(Triangles + T->AdjV3V1, I, -1);
            
            IsOutside[I] = true;
            --Result;
        }
    }

    return(Result);
}

internal triangulate_result
ConstrainedDelaunayTriangulate(polygon2 *Poly, memory_arena *Arena)
{
    TIMED_FUNCTION();

    triangulate_result Result = {};
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    if((Poly->VertexCount > 3) || (Poly->HasHoles))
    {
    
        s32 PointCount = Poly->VertexCount;
        v2 *PrePoints = 0;
        if(Poly->HasHoles)
        {
            s32 HolesPointCount = 0;
            for(s32 I = 0; I < Poly->HoleCount; ++I)
                HolesPointCount += Poly->HoleVertexCounts[I];

            PointCount += HolesPointCount;
            PrePoints = PushArray(TempMem.Arena, PointCount + 3, v2);

            v2 *At = PrePoints;
            Copy(sizeof(v2)*(Poly->VertexCount), Poly->Vertices, At);
            At += Poly->VertexCount;

            v2 *HolesVertices = Poly->HolesVertices;
            for(s32 I = 0; I < Poly->HoleCount; ++I)
            {
                s32 Count = Poly->HoleVertexCounts[I];
                Copy(sizeof(v2)*(Count), HolesVertices, At);
                HolesVertices += Count;
                At += Count;
            }
        }
        else
        {
            PrePoints = PushArray(TempMem.Arena, PointCount + 3, v2);
            Copy(sizeof(v2)*PointCount, Poly->Vertices, PrePoints);
        }

        edge_list* ConstraintEdges = 0;
        ConstraintEdges = PushStruct(TempMem.Arena, edge_list);
        edge_list *Current = ConstraintEdges;
        s32 At = 0;
        for(s32 I = 0;
            I < Poly->VertexCount;
            ++I)
        {
            Current->Count = 2;
            Current->Points[0] = I;
            Current->Points[1] = (I + 1) % Poly->VertexCount;
            Current->Next = PushStruct(TempMem.Arena, edge_list);
            Current = Current->Next;
        }
        At += Poly->VertexCount;
        
        if(Poly->HasHoles)
        {
            for(s32 HoleIndex = 0;
                HoleIndex < Poly->HoleCount;
                ++HoleIndex)
            {
                s32 VertexCount = Poly->HoleVertexCounts[HoleIndex];
                for(s32 I = 0;
                    I < VertexCount;
                    ++I)
                {
                    Current->Count = 2;
                    Current->Points[0] = At + I;
                    Current->Points[1] = At + (I + 1) % VertexCount;
                    Current->Next = PushStruct(TempMem.Arena, edge_list);
                    Current = Current->Next;
                }

                At += VertexCount;
            }
        }

        // NOTE(babykaban): Will be used in after processing
        s32 *PointOrder = PushArray(TempMem.Arena, PointCount + 3, s32);
        for(s32 I = 0; I < PointCount; ++I)
            PointOrder[I] = I;

        v2d *Points = PushArray(TempMem.Arena, PointCount + 3, v2d);
        for(s32 I = 0; I < PointCount; ++ I)
        {
            Points[I].x = PrePoints[I].x;
            Points[I].y = PrePoints[I].y;
        }
            
        // NOTE(babykaban): Find boundaries of Poly
        rectangle2d Bounds = CalculateBoundsForPoints(Points, PointCount);

        // NOTE(babykaban): Remap everything (preserving the aspect ratio) to between (0,0)-(1,1)
        v2d BoundsDim = GetDim(Bounds);
        f64 d = BoundsDim.y; // d = largest dimension
        if(BoundsDim.x > d) d = BoundsDim.x;
        f64 OneOverd = 1.0f / d;

        for(s32 I = 0;
            I < PointCount;
            ++I)
        {
            Points[I].V = (OneOverd*(Points[I] - Bounds.Min)).V;
        }

        // NOTE(babykaban): Sort points by proximity
        s32 BinRowsCount = CeilReal32ToInt32((f32)pow(PointCount, 0.25f));
        s32 *Bins = PushArray(TempMem.Arena, PointCount, s32);
        for(s32 I = 0;
            I < PointCount;
            ++I)
        {
            s32 p = (s32)(Points[I].y*BinRowsCount*0.999f); // bin row
            s32 q = (int)(Points[I].x*BinRowsCount*0.999f); // bin column
            if(p % 2) Bins[I] = (p + 1)*BinRowsCount - q;
            else Bins[I] = p*BinRowsCount + (q + 1);
        }

        // NOTE(babykaban): Insertion sort
        s32 Key;
        for(s32 I = 1;
            I < PointCount;
            ++I)
        {
            Key = Bins[I];
            v2d Temp = Points[I];
            v2 PreTemp = PrePoints[I];
            s32 TempI = PointOrder[I];
            s32 J = I - 1;
            while((J >= 0) && (Bins[J] > Key))
            {
                Bins[J + 1] = Bins[J];
                Points[J + 1] = Points[J];
                PrePoints[J + 1] = PrePoints[J];
                PointOrder[J + 1] = PointOrder[J];
                --J;
            }

            Bins[J + 1] = Key;
            Points[J + 1] = Temp;
            PrePoints[J + 1] = PreTemp;
            PointOrder[J + 1] = TempI;
        }

        // NOTE(babykaban): Add big triangle around point cloud
        Points[PointCount] = V2d(-100.0f, -100.0f);
        Points[PointCount + 1] = V2d(100.0f, -100.0f);
        Points[PointCount + 2] = V2d(0.0f, 100.0f);

        PointOrder[PointCount] = PointCount;
        PointOrder[PointCount + 1] = PointCount + 1;
        PointOrder[PointCount + 2] = PointCount + 2;

        PointCount += 3;

        triangulate_triangle *Triangles = PushArray(TempMem.Arena, PointCount*3, triangulate_triangle);
        Triangles[0].Vertices[0] = PointCount - 3;
        Triangles[0].Vertices[1] = PointCount - 2;
        Triangles[0].Vertices[2] = PointCount - 1;
        Triangles[0].Adjacencies[0] = -1;
        Triangles[0].Adjacencies[1] = -1;
        Triangles[0].Adjacencies[2] = -1;

        s32 TriangleCount = 1;
        s32 *TriangleStack = PushArray(TempMem.Arena, (PointCount - 3), s32); // is this a big enough stack?
        s32 Tos = -1;

        // NOTE(babykaban): Insert all points and triangulate one by one
        for(s32 CurrentPointIndex = 0;
            CurrentPointIndex < (PointCount - 3);
            ++CurrentPointIndex)
        {
            // NOTE(babykaban): Find triangle T which contains Points[CurrentPointIndex]
            s32 LastCreatedIndex = TriangleCount - 1; // Last triangle created
            while(1)
            {
                triangulate_triangle *LastCreatedT = Triangles + LastCreatedIndex; // Triangle T
                if(PlanarPointWithinTriangle(Points[CurrentPointIndex], Points[LastCreatedT->V1],
                                             Points[LastCreatedT->V2], Points[LastCreatedT->V3]))
                {
                    TriangleCount += 2;

                    s32 FirstNewIndex = TriangleCount - 2;
                    s32 SecondNewIndex = TriangleCount - 1;

                    // NOTE(babykaban): Delete triangle T and replace it with three sub-triangles touching P 
                    SplitTriangle(Triangles, LastCreatedIndex, FirstNewIndex, SecondNewIndex, CurrentPointIndex);

                    // NOTE(babykaban): Place each triangle containing P(Points[CurrentPointIndex]) onto a stack,
                    // if the edge opposite P(Points[CurrentPointIndex]) has an adjacent triangle
                    if(LastCreatedT->AdjV2V3 >= 0)
                        TriangleStack[++Tos] = LastCreatedIndex;
                    if(Triangles[FirstNewIndex].AdjV2V3 >= 0)
                        TriangleStack[++Tos] = FirstNewIndex;
                    if(Triangles[SecondNewIndex].AdjV2V3 >=0)
                        TriangleStack[++Tos]= SecondNewIndex;

                    while(Tos >= 0)
                    {
                        v2d P = Points[CurrentPointIndex];

                        // NOTE(babykaban): Looping thru the stack
                        s32 TestTIndex = TriangleStack[Tos--];
                        // NOTE(babykaban): TestT, triangle at the top of the stack that is being tested.
                        triangulate_triangle *TestT = Triangles + TestTIndex;
                        v2d Vertex1 = Points[TestT->V3];
                        v2d Vertex2 = Points[TestT->V2];

                        s32 OpositeTIndex = TestT->AdjV2V3;
                        // NOTE(babykaban): OpositeT, triangle adjacent to TestT across the edge formed by Vertex1 and Vertex2.
                        triangulate_triangle *OpositeT = Triangles + OpositeTIndex;

                        s32 OppVert = -1;
                        s32 OppVertID = -1;
                        for(s32 k = 0; k < 3; ++k)
                        {
                            if((OpositeT->Vertices[k] != TestT->V2)
                               && (OpositeT->Vertices[k] != TestT->V3))
                            {
                                OppVert = OpositeT->Vertices[k];
                                OppVertID = k;
                                break;
                            }
                        }

                        v2d Vertex3 = Points[OppVert];

                        // NOTE(babykaban): Check if P in circumcircle of TestT
                        f64 cosa = Inner(Vertex1 - Vertex3, Vertex2 - Vertex3);
                        f64 cosb = Inner(Vertex2 - P, Vertex1 - P);
                        f64 sina = Cross(Vertex1 - Vertex3, Vertex2 - Vertex3);
                        f64 sinb = Cross(Vertex2 - P, Vertex1 - P);

                        if(((cosa < 0) && (cosb < 0)) || ((-cosa*sinb) > (cosb*sina)))
                        {
                            s32 AdjacencentToTestTIndex = TestT->AdjV3V1;
                            s32 AdjacencentToOpositeTIndex = OpositeT->Adjacencies[(OppVertID + 2) % 3]; // A

                            triangulate_triangle *AdjacencentToTestT = Triangles + AdjacencentToTestTIndex;
                            triangulate_triangle *AdjacencentToOpositeT = Triangles + AdjacencentToOpositeTIndex; // C

                            // NOTE(babykaban): Fix adjacency of A
                            if(AdjacencentToOpositeTIndex >= 0)
                            {
                                AdjacenciesFindWhichEqualAndSetTo(AdjacencentToOpositeT, OpositeTIndex, TestTIndex);
                            }

                            // NOTE(babykaban): Fix adjacency of C
                            if(AdjacencentToTestTIndex >= 0)
                            {
                                AdjacenciesFindWhichEqualAndSetTo(AdjacencentToTestT, TestTIndex, OpositeTIndex);
                            }

                            // NOTE(babykaban): Fix vertices and adjacency of OpositeT
                            for(s32 m = 0; m < 3; m++)
                            {
                                if(OpositeT->Vertices[m] == OppVert)
                                {
                                    OpositeT->Vertices[(m + 2) % 3] = CurrentPointIndex;
                                    break;
                                }
                            }
                            AdjacenciesFindWhichEqualAndSetTo(OpositeT, TestTIndex, AdjacencentToTestTIndex);
                            AdjacenciesFindWhichEqualAndSetTo(OpositeT, AdjacencentToOpositeTIndex, TestTIndex);

                            for(s32 m = 0; m < 3; m++)
                            {
                                if(OpositeT->V1 != CurrentPointIndex)
                                {
                                    s32 Temp1 = OpositeT->V1;
                                    s32 Temp2 = OpositeT->AdjV1V2;

                                    OpositeT->V1 = OpositeT->V2;
                                    OpositeT->V2 = OpositeT->V3;
                                    OpositeT->V3 = Temp1;

                                    OpositeT->AdjV1V2 = OpositeT->AdjV2V3;
                                    OpositeT->AdjV2V3 = OpositeT->AdjV3V1;
                                    OpositeT->AdjV3V1 = Temp2;
                                }
                            }

                            // NOTE(babykaban): Fix vertices and adjacency of TestT
                            TestT->V3 = OppVert;
                            AdjacenciesFindWhichEqualAndSetTo(TestT, AdjacencentToTestTIndex, OpositeTIndex);
                            AdjacenciesFindWhichEqualAndSetTo(TestT, OpositeTIndex, AdjacencentToOpositeTIndex);

                            // NOTE(babykaban): Add TestT and OpositeT to stack if they have triangles opposite P;
                            if(TestT->AdjV2V3 >= 0) TriangleStack[++Tos] = TestTIndex;
                            if(OpositeT->AdjV2V3 >= 0) TriangleStack[++Tos] = OpositeTIndex;
                        }
                    }

                    break;
                }

                // NOTE(babykaban): Adjust LastCreatedIndex in the direction of target point CurrentPointIndex
                v2d AB = Points[LastCreatedT->V2] - Points[LastCreatedT->V1];
                v2d BC = Points[LastCreatedT->V3] - Points[LastCreatedT->V2];
                v2d CA = Points[LastCreatedT->V1] - Points[LastCreatedT->V3];

                v2d AP = Points[CurrentPointIndex] - Points[LastCreatedT->V1];
                v2d BP = Points[CurrentPointIndex] - Points[LastCreatedT->V2];
                v2d CP = Points[CurrentPointIndex] - Points[LastCreatedT->V3];

                v2d N1 = V2d(AB.y, -AB.x);
                v2d N2 = V2d(BC.y, -BC.x);
                v2d N3 = V2d(CA.y, -CA.x);

                f64 S1 = Inner(AP, N1);
                f64 S2 = Inner(BP, N2);
                f64 S3 = Inner(CP, N3);

                if((S1 >= 0) && (S1 >= S2) && (S1 >= S3)) LastCreatedIndex = LastCreatedT->AdjV1V2;
                else if((S2 >= 0) && (S2 >= S1) && (S2 >= S3)) LastCreatedIndex = LastCreatedT->AdjV2V3;
                else if((S3 >= 0) && (S3 >= S1) && (S3 >= S2)) LastCreatedIndex = LastCreatedT->AdjV3V1;
            }
        }


        if(ConstraintEdges)
        {
            edge_list *CurrentEdge = ConstraintEdges;

            // NOTE(babykaban): Array storing the indices of triangles that contain the vertex _EdgeVertex1_
            s32 *EdgeVertex1Tris = PushArray(Arena, (PointCount + 3), s32);

            // NOTE(babykaban): Array that track the index of the _EdgeVertex1_ in the found triangles
            s32 *EdgeVertex1TrisInd = PushArray(Arena, (PointCount + 3), s32);
        
            while(CurrentEdge)
            {
                // NOTE(babykaban): Loop thru all constraint edge loops
                for(s32 i = 0; i < (CurrentEdge->Count - 1); i++)
                {
                    // NOTE(babykaban): Loop thru edge in the constraint edge loop
                    s32 EdgeVertex1 = 0;
                    s32 EdgeVertex2 = 0;
                    for(s32 j = 0; j < PointCount; j++)
                    { 
                        if(PointOrder[j] == CurrentEdge->Points[i])
                            EdgeVertex1 = j;
                        if(PointOrder[j] == CurrentEdge->Points[i + 1])
                            EdgeVertex2 = j;
                    }

                    s32 IndexCount1 = 0;
                    for(s32 j = 0; j < TriangleCount; j++)
                    {
                        // NOTE(babykaban): Loop thru all triangles to see which triangles contains our important vertices
                        if(Triangles[j].Vertices[0] == EdgeVertex1)
                        {
                            EdgeVertex1TrisInd[IndexCount1] = 0;
                            EdgeVertex1Tris[IndexCount1++] = j;
                        }
                        else if(Triangles[j].Vertices[1] == EdgeVertex1)
                        {
                            EdgeVertex1TrisInd[IndexCount1] = 1;
                            EdgeVertex1Tris[IndexCount1++] = j;
                        }
                        else if(Triangles[j].Vertices[2] == EdgeVertex1)
                        {
                            EdgeVertex1TrisInd[IndexCount1] = 2;
                            EdgeVertex1Tris[IndexCount1++] = j;
                        }
                    }

                    b32 EdgeFound = false;
                    s32 AdjacentTriangle = -1;
                    for(s32 j = 0; j < IndexCount1; j++)
                    {
                        // NOTE(babykaban): Search through all triangles containing EdgeVertex1, looking for either EdgeVertex2
                        // or an intersecting edge with the segment EdgeVertex1-EdgeVertex2.
                        triangulate_triangle *SearchT = Triangles + EdgeVertex1Tris[j];
                        if((SearchT->Vertices[0] == EdgeVertex2) ||
                           (SearchT->Vertices[1] == EdgeVertex2) ||
                           (SearchT->Vertices[2] == EdgeVertex2))
                        {
                            // NOTE(babykaban): Exact edge found
                            EdgeFound = 1;
                            break;
                        }       
                        else if(pointDirectionFromLineSegment2D(Points[EdgeVertex1], 
                                                                Points[SearchT->Vertices[(EdgeVertex1TrisInd[j] + 1) % 3]],
                                                                Points[EdgeVertex2]) &&
                                pointDirectionFromLineSegment2D(Points[SearchT->Vertices[(EdgeVertex1TrisInd[j] + 2) % 3]],
                                                                Points[EdgeVertex1],
                                                                Points[EdgeVertex2]))
                        {
                            // NOTE(babykaban): EdgeVertex2 is to the left of both edges adjacent to EdgeVertex1 on triangle j
                            AdjacentTriangle = EdgeVertex1Tris[j];
                            break;
                        }
                    }

                    if(EdgeFound)
                    {
                        // NOTE(babykaban): Constraint edge already in vertex array, do nothing
                    }
                    else
                    {
                        // NOTE(babykaban): constraint edge NOT found in vertex array
                        // start looking for intersections in triangle triTraj
                        edge_list *Intersections = PushStruct(TempMem.Arena, edge_list);
                        edge_list *CurrentIntersectEdge = Intersections;
                        CurrentIntersectEdge->Next = 0;
                        CurrentIntersectEdge->Count = -1;

                        while(1)
                        {
                            triangulate_triangle *TestTri = Triangles + AdjacentTriangle;
                            if((TestTri->Vertices[0] == EdgeVertex2) || // we made it to the triangle containing EdgeVertex2
                               (TestTri->Vertices[1] == EdgeVertex2) ||
                               (TestTri->Vertices[2] == EdgeVertex2))
                            {
                                break;
                            }

                            for(s32 j = 0; j < 3; j++)
                            {
                                // NOTE(babykaban): Check if EdgeVertex2 is to the right of any edge in the current triangle,
                                // if it is, it's a candidate for intersection because the entry edge will fail this test,
                                // so no double counting
                                s32 Next = (j + 1) % 3;
                                if(!pointDirectionFromLineSegment2D(Points[TestTri->Vertices[j]], 
                                                                    Points[TestTri->Vertices[Next]],
                                                                    Points[EdgeVertex2]))
                                {
                                    if (LineSegmentsCross2D(Points[EdgeVertex1],Points[EdgeVertex2],
                                                            Points[TestTri->Vertices[j]],
                                                            Points[TestTri->Vertices[Next]]))
                                    {
                                        if(CurrentIntersectEdge->Count == -1)
                                        {
                                            CurrentIntersectEdge->Next = 0;
                                            CurrentIntersectEdge->Count = 2;
                                            CurrentIntersectEdge->Points[0] = TestTri->Vertices[j];
                                            CurrentIntersectEdge->Points[1] = TestTri->Vertices[Next];
                                        }
                                        else
                                        {
                                            edge_list * nextIntersection = PushStruct(TempMem.Arena, edge_list);
                                            CurrentIntersectEdge->Next = nextIntersection;
                                            CurrentIntersectEdge = CurrentIntersectEdge->Next;
                                            CurrentIntersectEdge->Next = 0;
                                            CurrentIntersectEdge->Count = 2;
                                            CurrentIntersectEdge->Points[0] = TestTri->Vertices[j];
                                            CurrentIntersectEdge->Points[1] = TestTri->Vertices[Next];
                                        }

                                        AdjacentTriangle = Triangles[AdjacentTriangle].Adjacencies[j];
                                        break;
                                    }
                                }
                            }
                        }

                        // NOTE(babykaban): Triangles that share _TestIntersectEdge_
                        s32 Triangle0 = -1;
                        s32 Triangle1 = -1;

                        // NOTE(babykaban): New edges that we are adding to the triangulation
                        edge_list *NewEdges = PushStruct(TempMem.Arena, edge_list);
                        edge_list *CurrentNewEdge = NewEdges;
                        NewEdges->Count = -1;

                        // NOTE(babykaban): Loop thru edges that need to be removed
                        edge_list *TestIntersect = Intersections;
                        while(TestIntersect)
                        {
                            // NOTE(babykaban): Stores the index of the third vertex that is not part of
                            // an edge for each triangle
                            s32 T0OtherVertex = -1;
                            s32 T1OtherVertex = -1;

                            // NOTE(babykaban): Find triangles that share _TestIntersectEdge_
                            for(s32 j = 0; j < TriangleCount; j++)
                            {
                                triangulate_triangle *TestT = Triangles + j;
                                Triangle1 = j;

                                if((TestT->Vertices[0] == TestIntersect->Points[1]) &&
                                   (TestT->Vertices[1] == TestIntersect->Points[0]))
                                {
                                    T1OtherVertex = 2;
                                    break;
                                }                           

                                if((TestT->Vertices[1] == TestIntersect->Points[1]) &&
                                   (TestT->Vertices[2] == TestIntersect->Points[0]))
                                {
                                    T1OtherVertex = 0;
                                    break;
                                }                           

                                if((TestT->Vertices[2] == TestIntersect->Points[1]) &&
                                   (TestT->Vertices[0] == TestIntersect->Points[0]))
                                {
                                    T1OtherVertex = 1;
                                    break;
                                }                           
                            }

                            for(s32 j = 0; j < TriangleCount; j++)
                            {
                                triangulate_triangle *TestT = Triangles + j;
                                Triangle0 = j;

                                if((TestT->Vertices[0] == TestIntersect->Points[0]) &&
                                   (TestT->Vertices[1] == TestIntersect->Points[1]))
                                {
                                    T0OtherVertex = 2;
                                    break;
                                }                           

                                if((TestT->Vertices[1] == TestIntersect->Points[0]) &&
                                   (TestT->Vertices[2] == TestIntersect->Points[1]))
                                {
                                    T0OtherVertex = 0;
                                    break;
                                }                           

                                if((TestT->Vertices[2] == TestIntersect->Points[0]) &&
                                   (TestT->Vertices[0] == TestIntersect->Points[1]))
                                {
                                    T0OtherVertex = 1;
                                    break;
                                }                           
                            }

                            triangulate_triangle *T0 = Triangles + Triangle0;
                            triangulate_triangle *T1 = Triangles + Triangle1;
                            s32 NextFromOther0 = (T0OtherVertex + 1) % 3;
                            s32 PrevFromOther0 = (T0OtherVertex + 2) % 3;
                            s32 NextFromOther1 = (T1OtherVertex + 1) % 3;
                            s32 PrevFromOther1 = (T1OtherVertex + 2) % 3;

                            if(QuadrilateralConvex2D(Points[T0->Vertices[PrevFromOther0]],
                                                     Points[T0->Vertices[T0OtherVertex]],
                                                     Points[T0->Vertices[NextFromOther0]],
                                                     Points[T1->Vertices[T1OtherVertex]]))
                            {
                                // NOTE(babykaban): Quad convex, swap diagonal
                                T0->Vertices[PrevFromOther0] = T1->Vertices[T1OtherVertex];
                                T1->Vertices[PrevFromOther1] = T0->Vertices[T0OtherVertex];

                                // NOTE(babykaban): Fix primary triangle adjacencies
                                T0->Adjacencies[NextFromOther0] = T1->Adjacencies[PrevFromOther1];
                                T1->Adjacencies[NextFromOther1] = T0->Adjacencies[PrevFromOther0];


                                T0->Adjacencies[PrevFromOther0] = Triangle1;
                                T1->Adjacencies[PrevFromOther1] = Triangle0;

                                // NOTE(babykaban): Fix 2 nearby triangle adjacencies
                                AdjacenciesFindWhichEqualAndSetTo(Triangles + T0->Adjacencies[T0OtherVertex], Triangle0, Triangle1);
                                AdjacenciesFindWhichEqualAndSetTo(Triangles + T1->Adjacencies[T1OtherVertex], Triangle1, Triangle0);

                                T0OtherVertex++; // NOTE(babykaban): rotate around 1 vtx
                                T1OtherVertex++; // NOTE(babykaban): rotate around 1 vtx

                                NextFromOther0 = (T0OtherVertex + 1) % 3;
                                PrevFromOther0 = (T0OtherVertex + 2) % 3;
                                NextFromOther1 = (T1OtherVertex + 1) % 3;
                                PrevFromOther1 = (T1OtherVertex + 2) % 3;

                                // NOTE(babykaban): If the new diagonal still intersects the constraint edge, add it to the list
                                if(LineSegmentsCross2D(Points[EdgeVertex1], Points[EdgeVertex2],
                                                       Points[T0->Vertices[NextFromOther0]],
                                                       Points[T1->Vertices[NextFromOther1]]))
                                {
                                    edge_list *NextIntersection = PushStruct(TempMem.Arena, edge_list);
                                    CurrentIntersectEdge->Next = NextIntersection;
                                    CurrentIntersectEdge = CurrentIntersectEdge->Next;
                                    CurrentIntersectEdge->Next = 0;
                                    CurrentIntersectEdge->Count = 2;
                                    CurrentIntersectEdge->Points[0] = T0->Vertices[NextFromOther0];
                                    CurrentIntersectEdge->Points[1] = T1->Vertices[NextFromOther1];
                                }
                                else
                                {
                                    // NOTE(babykaban): If diagonal doesnt intersect the constraint edge, add to newEdge list
                                    if(CurrentNewEdge->Count == -1)
                                    {
                                        // NOTE(babykaban): First new edge
                                        CurrentNewEdge->Next = 0;
                                        CurrentNewEdge->Count = 2;
                                        CurrentNewEdge->Points[0] = T0->Vertices[NextFromOther0];
                                        CurrentNewEdge->Points[1] = T1->Vertices[NextFromOther1];
                                    }
                                    else
                                    {
                                        edge_list *nextEdge = PushStruct(TempMem.Arena, edge_list);
                                        CurrentNewEdge->Next = nextEdge;
                                        CurrentNewEdge = CurrentNewEdge->Next;
                                        CurrentNewEdge->Next = 0;
                                        CurrentNewEdge->Count = 2;
                                        CurrentNewEdge->Points[0] = T0->Vertices[NextFromOther0];
                                        CurrentNewEdge->Points[1] = T1->Vertices[NextFromOther1];                          
                                    }
                                }
                            }
                            else
                            {
                                // NOTE(babykaban): Quad not convex, put diagonal on the end of the linked list
                                edge_list *Search = Intersections;
                                edge_list *SearchPrev = 0;
                                while(Search->Next)
                                {
                                    if(Search == TestIntersect)
                                    {
                                        // remove the old diagonal from the linked list
                                        if(SearchPrev == 0)
                                        {
                                            Intersections = TestIntersect->Next;
                                        }
                                        else
                                        {
                                            SearchPrev->Next = TestIntersect->Next;
                                        }
                                    }

                                    SearchPrev = Search;
                                    Search = Search->Next;
                                }

                                edge_list *NextIntersection = PushStruct(TempMem.Arena, edge_list);
                                Search->Next = NextIntersection;
                                Search = Search->Next;
                                Search->Next = 0;
                                Search->Count = 2;
                                Search->Points[0] = T0->Vertices[NextFromOther0];
                                Search->Points[1] = T1->Vertices[NextFromOther1];

                            }

                            TestIntersect = TestIntersect->Next;
                        }

                        s32 SwapCount = 1;
                        while(SwapCount > 0)
                        {
                            // until no further swaps take place
                            SwapCount = 0;

                            // NOTE(babykaban): Restore the delaunay triangulation on the newEdges
                            edge_list *TestEdge = NewEdges;
                            while(TestEdge)
                            {
                                // NOTE(babykaban): If the new edge is NOT the constraint edge
                                if(!(((TestEdge->Points[0] == EdgeVertex1) && (TestEdge->Points[1] == EdgeVertex2)) ||
                                     ((TestEdge->Points[0] == EdgeVertex2) && (TestEdge->Points[1] == EdgeVertex1))))
                                {
                                    // NOTE(babykaban): identify lone vertex indices for the triangles which share the new edge,
                                    // can keep an edge adjacency array to speed this up

                                    // NOTE(babykaban): Find triangles that contain _TestEdge_
                                    for(s32 j = 0; j < TriangleCount; j++)
                                    {
                                        if(((Triangles[j].Vertices[0] == TestEdge->Points[1]) &&
                                            (Triangles[j].Vertices[1] == TestEdge->Points[0])) ||
                                           ((Triangles[j].Vertices[1] == TestEdge->Points[1]) &&
                                            (Triangles[j].Vertices[2] == TestEdge->Points[0])) ||
                                           ((Triangles[j].Vertices[2] == TestEdge->Points[1]) &&
                                            (Triangles[j].Vertices[0] == TestEdge->Points[0])))
                                        {
                                            Triangle0 = j;
                                            break;
                                        }
                                    }

                                    for(s32 j = 0; j < TriangleCount; j++)
                                    {
                                        if(((Triangles[j].Vertices[0] == TestEdge->Points[0]) &&
                                            (Triangles[j].Vertices[1] == TestEdge->Points[1])) ||
                                           ((Triangles[j].Vertices[1] == TestEdge->Points[0]) &&
                                            (Triangles[j].Vertices[2] == TestEdge->Points[1])) ||
                                           ((Triangles[j].Vertices[2] == TestEdge->Points[0]) &&
                                            (Triangles[j].Vertices[0] == TestEdge->Points[1])))
                                        {
                                            Triangle1 = j;
                                            break;
                                        }
                                    }

                                    triangulate_triangle *T0 = Triangles + Triangle0;
                                    triangulate_triangle *T1 = Triangles + Triangle1;

                                    s32 Triangle0LoneVertexIndex = -1;
                                    s32 Triangle1LoneVertexIndex = -1;

                                    // NOTE(babykaban): Find the lone vertex in triangle0
                                    for(s32 j = 0; j < 3;j++)
                                        if((T0->Vertices[j] != TestEdge->Points[0]) &&
                                           (T0->Vertices[j] != TestEdge->Points[1]))
                                        {
                                            Triangle0LoneVertexIndex = j;
                                        }

                                    // NOTE(babykaban): Find the lone vertex in triangle1
                                    for(s32 j = 0; j < 3; j++)
                                        if((T1->Vertices[j] != TestEdge->Points[0]) &&
                                           (T1->Vertices[j] != TestEdge->Points[1]))
                                        {
                                            Triangle1LoneVertexIndex = j;
                                        }

                                    s32 NextVertexFromLone0 = (Triangle0LoneVertexIndex + 1) % 3;
                                    s32 PrevVertexFromLone0 = (Triangle0LoneVertexIndex + 2) % 3;
                                    s32 NextVertexFromLone1 = (Triangle1LoneVertexIndex + 1) % 3;
                                    s32 PrevVertexFromLone1 = (Triangle1LoneVertexIndex + 2) % 3;
                                
                                    v2d v1a = Points[T0->Vertices[Triangle0LoneVertexIndex]];
                                    v2d v1b = Points[T0->Vertices[NextVertexFromLone0]];
                                    v2d v1c = Points[T0->Vertices[PrevVertexFromLone0]];  
                                    v2d v2a = Points[T1->Vertices[Triangle1LoneVertexIndex]];

                                    f64 cosa = Inner(v1b - v1a, v1c - v1a);
                                    f64 cosb = Inner(v1c - v2a, v1b - v2a);
                                    f64 sina = (v1c.x - v2a.x)*(v1b.y - v2a.y) - (v1c.y - v2a.y)*(v1b.x - v2a.x);
                                    f64 sinb = (v1b.x - v1a.x)*(v1c.y - v1a.y) - (v1b.y - v1a.y)*(v1c.x - v1a.x);
                                
                                    // NOTE(babykaban): if triangle 0 vertex is inside triangle 1 circumcircle or
                                    // if triangle 1 vertex is inside triangle 0 circumcircle
                                    if((((cosa < 0) && (cosb < 0)) || ((-cosa*sina) > (cosb*sinb))) ||
                                       (((cosb < 0) && (cosa < 0)) || ((-cosb*sinb) > (cosa*sina))))
                                    {
                                        // NOTE(babykaban): If delaunay condition is not satisfied for triangles that
                                        // share this edge, swap the diagonal
                                        T0->Vertices[PrevVertexFromLone0] = T1->Vertices[Triangle1LoneVertexIndex];
                                        T1->Vertices[PrevVertexFromLone1] = T0->Vertices[Triangle0LoneVertexIndex];

                                        // NOTE(babykaban): Update triangle0 and triangle1 adjacencies
                                        T0->Adjacencies[NextVertexFromLone0] = T1->Adjacencies[PrevVertexFromLone1];
                                        T1->Adjacencies[NextVertexFromLone1] = T0->Adjacencies[PrevVertexFromLone0];
                                        T0->Adjacencies[PrevVertexFromLone0] = Triangle1;
                                        T1->Adjacencies[PrevVertexFromLone1] = Triangle0;

                                        // NOTE(babykaban): Fix 2 nearby triangle adjacencies
                                        AdjacenciesFindWhichEqualAndSetTo(Triangles + T0->Adjacencies[Triangle0LoneVertexIndex], Triangle0, Triangle1);
                                        AdjacenciesFindWhichEqualAndSetTo(Triangles + T1->Adjacencies[Triangle1LoneVertexIndex], Triangle1, Triangle0);

                                        // NOTE(babykaban): Swap diagonal in new edge list
                                        TestEdge->Points[0] = T0->Vertices[Triangle0LoneVertexIndex];
                                        TestEdge->Points[1] = T1->Vertices[Triangle1LoneVertexIndex];

                                        // NOTE(babykaban): Rotate the lone vtx + 1
                                        Triangle0LoneVertexIndex++;
                                        Triangle1LoneVertexIndex++;

                                        // NOTE(babykaban): Keep track of number of swaps
                                        SwapCount++;
                                    }
                                }

                                TestEdge = TestEdge->Next;
                            }
                        }
                    }
                }

                CurrentEdge = CurrentEdge->Next;
            }
        }
    
        {    
            TIMED_BLOCK("TRIANGULATION After Processing");
            PointCount -= 3;

            // NOTE(babykaban): Count how many triangles there are that dont involve supertriangle vertices
            s32 PreFinalTriangleCount = TriangleCount;
            s32 *RenumberAdj = PushArray(TempMem.Arena, TriangleCount, s32);
            b32 *DeadTris = PushArray(TempMem.Arena, TriangleCount, b32);

            f32 OneOverThree = 1.0f / 3.0f;
            for(s32 I = 0; I < TriangleCount; I++)
            {
                if((Triangles[I].V1 >= PointCount) ||
                   (Triangles[I].V2 >= PointCount) ||
                   (Triangles[I].V3 >= PointCount))
                {
                    DeadTris[I] = 1;
                    RenumberAdj[I] = TriangleCount - (PreFinalTriangleCount--);
                }
                else
                {
                    RenumberAdj[I] = TriangleCount - PreFinalTriangleCount;
                }
            }

            // NOTE(babykaban): Delete any triangles that contain the supertriangle vertices 
            triangulate_triangle *PreFinalTriangles = PushArray(TempMem.Arena, PreFinalTriangleCount, triangulate_triangle);

            s32 CurrentIndex = 0;
            for(s32 I = 0; I < TriangleCount; I++)
            {
                if((Triangles[I].V1 < PointCount) &&
                   (Triangles[I].V2 < PointCount) &&
                   (Triangles[I].V3 < PointCount))
                {
                    PreFinalTriangles[CurrentIndex] = Triangles[I];

                    PreFinalTriangles[CurrentIndex].AdjV1V2 = (1 - DeadTris[Triangles[I].AdjV1V2])*Triangles[I].AdjV1V2 - DeadTris[Triangles[I].AdjV1V2];
                    PreFinalTriangles[CurrentIndex].AdjV2V3 = (1 - DeadTris[Triangles[I].AdjV2V3])*Triangles[I].AdjV2V3 - DeadTris[Triangles[I].AdjV2V3];
                    PreFinalTriangles[CurrentIndex].AdjV3V1 = (1 - DeadTris[Triangles[I].AdjV3V1])*Triangles[I].AdjV3V1 - DeadTris[Triangles[I].AdjV3V1];
                    ++CurrentIndex;
                }

            }

            // NOTE(babykaban): Fix adjacencies of PreFinalTriangles
            for(s32 i = 0; i < PreFinalTriangleCount; i++)
            {
                if (PreFinalTriangles[i].AdjV1V2 >= 0)
                    PreFinalTriangles[i].AdjV1V2 -= RenumberAdj[PreFinalTriangles[i].AdjV1V2];
                if (PreFinalTriangles[i].AdjV2V3 >= 0)
                    PreFinalTriangles[i].AdjV2V3 -= RenumberAdj[PreFinalTriangles[i].AdjV2V3];
                if (PreFinalTriangles[i].AdjV3V1 >= 0)
                    PreFinalTriangles[i].AdjV3V1 -= RenumberAdj[PreFinalTriangles[i].AdjV3V1];
            }

            // NOTE(babykaban): Undo the mapping
            if(BoundsDim.x > d)
                d = BoundsDim.x;

            for(s32 i = 0; i < PointCount; i++)
            {
                Points[i] = Points[i]*d + Bounds.Min;
            }


            b32 *IsOutside = PushArray(TempMem.Arena, PreFinalTriangleCount, b32);
            s32 FinalCount = MarkOutsideTriangles(Poly, Points, PreFinalTriangles, PreFinalTriangleCount, IsOutside, TempMem.Arena);

            Result.TriangleCount = FinalCount;
            Result.Triangles = (triangle *)Platform.AllocateMemory(sizeof(triangle)*FinalCount);
            Result.Adjacencies = (triangle_adjs *)Platform.AllocateMemory(sizeof(triangle_adjs)*FinalCount);

            // NOTE(babykaban): Finilize result
            s32 TIndex = 0;
            for(s32 I = 0;
                I < PreFinalTriangleCount;
                ++I)
            {
                if(!IsOutside[I])
                {
                    triangulate_triangle Triangle = PreFinalTriangles[I];
                    triangle *T = Result.Triangles + TIndex;
                    triangle_adjs *Adjs = Result.Adjacencies + I;

                    T->Vertices[0] = V2(Points[Triangle.V1]);
                    T->Vertices[1] = V2(Points[Triangle.V2]);
                    T->Vertices[2] = V2(Points[Triangle.V3]);

                    Adjs->Adjacencies[0] = Triangle.Adjacencies[0];
                    Adjs->Adjacencies[1] = Triangle.Adjacencies[1];
                    Adjs->Adjacencies[2] = Triangle.Adjacencies[2];
                    ++TIndex;
                }
            }
        }
    }
    else
    {
        if(Poly->VertexCount == 3)
        {
            Result.TriangleCount = 1;
            Result.Triangles = (triangle *)Platform.AllocateMemory(sizeof(triangle));
            Result.Adjacencies = (triangle_adjs *)Platform.AllocateMemory(sizeof(triangle_adjs));
            triangle *T = Result.Triangles + 0;
            triangle_adjs *Adjs = Result.Adjacencies + 0;

            T->Vertices[0] = Poly->Vertices[0];
            T->Vertices[1] = Poly->Vertices[1];
            T->Vertices[2] = Poly->Vertices[2];
            
            Adjs->Adjacencies[0] = -1;
            Adjs->Adjacencies[1] = -1;
            Adjs->Adjacencies[2] = -1;
        }
    }

    EndTemporaryMemory(TempMem);

    return(Result);
}
// ===========================================================================================================================================================

inline void
RemoveAt(polygon2 *Poly, s32 Index)
{
    Poly->Vertices[Index] = {};
    for(s32 I = Index;
        I < (Poly->VertexCount - 1);
        ++I)
    {
        Poly->Vertices[I] = Poly->Vertices[I + 1];
    }

    Poly->Vertices[Poly->VertexCount] = {};
    --Poly->VertexCount;
}

inline void
RemoveAt(line *Array, s32 Count, s32 Index)
{
    Array[Index] = {};
    for(s32 I = Index;
        I < Count - 1;
        ++I)
    {
        Array[I] = Array[I + 1];
    }
}

internal void
SplitPolygon(polygon2 *ResultArray, s32 *ResultCount, s32 *NotConvexIndices, s32 *NotConvexCount, s32 SubjectIndex, memory_arena *TempArena)
{
    s32 ArrayEnd = (*ResultCount) - 1;
    polygon2 *Poly = ResultArray + SubjectIndex;
    if((Poly->VertexCount > 3) && !IsConvex(Poly))
    {
        b32 Clockwise = (PolygonSignedArea(Poly) < 0.0f);
    
        s32 PrevOffset = Clockwise ? -1 : 1;
        s32 NextOffset = Clockwise ? 1 : -1;

        s32 ReflexCount = 0;
        line *PerpLines = PushArray(TempArena, Poly->VertexCount - 2, line);
    
        // TODO(babykaban): Cash calculated cross products
        // NOTE(babykaban): Found all reflex points and their perpendiculars
        for(s32 VertexIndex = 0;
            VertexIndex < Poly->VertexCount;
            ++VertexIndex)
        {
            v2 VertexPrev = Poly->Vertices[GetIndex(Poly->VertexCount, VertexIndex + PrevOffset)];
            v2 VertexCur = Poly->Vertices[VertexIndex];
            v2 VertexNext = Poly->Vertices[GetIndex(Poly->VertexCount, VertexIndex + NextOffset)];
                
            v2 a = VertexCur - VertexPrev;
            v2 b = VertexNext - VertexCur;
                
            if(Cross(a, b) > 0.0f)
            {
                v2 Perpendicular = Perp(VertexPrev - VertexCur);
                v2 NewP = VertexCur + Normalize(Perpendicular)*100.0f;

                line *Line = PerpLines + ReflexCount;
                Line->a = VertexCur;
                Line->b = NewP;
                ++ReflexCount;
            }
        }

        polygon2 *NewPoly = ResultArray + (*ResultCount);
        // NOTE(babykaban): Loop through Reflex points and split polygon by its intersection with perpendicular
        for(s32 ReflexIndex = 0;
            ReflexIndex < ReflexCount;
            ++ReflexIndex)
        {
            line *ReflexPerp = PerpLines + ReflexIndex;
            v2 p;
            for(s32 VertexIndex = 0;
                VertexIndex < Poly->VertexCount;
                ++VertexIndex)
            {
                v2 p1 = Poly->Vertices[VertexIndex];
                v2 p2 = Poly->Vertices[(VertexIndex + 1) % Poly->VertexCount];
                if(LineIntersect(p1, p2, ReflexPerp->a, ReflexPerp->b, &p) > 0)
                {
                    if(!SPLITPointsAreEqual(p, ReflexPerp->a))
                    {
                        if(!SPLITPointsAreEqual(p, p1) && !SPLITPointsAreEqual(p, p2))
                        {
                            TRISUBInsertPointBetween(Poly, p, {VertexIndex, (VertexIndex + 1)});
                        }

                        s32 Reflex = 0;
                        for(s32 I = 0;
                            I < Poly->VertexCount;
                            ++I)
                        {
                            if(SPLITPointsAreEqual(ReflexPerp->a, Poly->Vertices[I]))
                            {
                                Reflex = I;
                                break;
                            }
                        }

                        s32 Backward = (Reflex - VertexIndex - 1 + Poly->VertexCount) % Poly->VertexCount;
                        s32 Forward = (VertexIndex + 1 - Reflex + Poly->VertexCount) % Poly->VertexCount;
                        s32 Offset = (Forward > Backward) ? -1 : 1;                    
                        // NOTE(babykaban): Construct resulting polygon
                        v2 TestP = ReflexPerp->a;
                        while(!SPLITPointsAreEqual(p, TestP))
                        {
                            TestP = Poly->Vertices[Reflex];
                            NewPoly->Vertices[NewPoly->VertexCount++] = TestP;
                            Reflex = GetIndex(Poly->VertexCount, Reflex + Offset);
                        }

                        // NOTE(babykaban): Remove vertices from TempPoly that are clipped by NewPoly 
                        for(s32 I = 0;
                            I < NewPoly->VertexCount;
                            ++I)
                        {
                            v2 Vertex = NewPoly->Vertices[I];
                            if(!SPLITPointsAreEqual(p, Vertex) && !SPLITPointsAreEqual(ReflexPerp->a, Vertex))
                            {
                                for(s32 J = 0;
                                    J < Poly->VertexCount;
                                    ++J)
                                {
                                    v2 TVertex = Poly->Vertices[J];
                                    if(SPLITPointsAreEqual(TVertex, Vertex))
                                    {
                                        RemoveAt(Poly, J);
                                        break;
                                    }
                                }
                            }
                        }
                        
                        (*ResultCount)++;

                        s32 Increase = 0;
                        s32 Next = ReflexIndex + 1;
                        for(s32 I = 0;
                            I < NewPoly->VertexCount;
                            ++I)
                        {
                            v2 Vertex = NewPoly->Vertices[I];
                            if(SPLITPointsAreEqual(Vertex, PerpLines[Next].a))
                            {
                                ++Next;
                                ++Increase;
                            }
                        }

                        ReflexIndex += Increase;
                            
                        NewPoly = ResultArray + (*ResultCount);
                        break;
                    }
                }
            }
        }
    }

    // NOTE(babykaban): Check if there are not-convex polygons present if so record the indices
    for(s32 I = ArrayEnd;
        I < *ResultCount;
        ++I)
    {
        polygon2 *P = ResultArray + I;
        if(!IsConvex(P))
        {
            NotConvexIndices[(*NotConvexCount)++] = I;
        }
    }
}

inline void
SplitPolygonIntoConvexParts(polygon2 *Polygons, s32 *Count, s32 *NotConvexIndices, s32 *NotConvexCount, memory_arena *Arena)
{
    TIMED_FUNCTION();

    s32 Start = (*Count) - 1;
    SplitPolygon(Polygons, Count, NotConvexIndices, NotConvexCount, Start, Arena);
    while((*NotConvexCount) > 0)
    {
        s32 NotConvexIndex = NotConvexIndices[(*NotConvexCount) - 1];
        (*NotConvexCount)--;
        SplitPolygon(Polygons, Count, NotConvexIndices, NotConvexCount, NotConvexIndex, Arena);
    }
}



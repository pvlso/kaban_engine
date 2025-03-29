/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#define SUBTRACTION_MAX_POINTS_PER_POLYGON 16 // NOTE(babykaban): Actually 10 but to make sure that there is enough spece 16 should be good
#define SUBTRACTION_MAX_POLYGON_COUNT 4 // NOTE(babykaban): Actually 3 

#include "subtruct_poly.h"
#define SUBTRACT_EPSILON_F64 0.0000000000001


struct vertex_info
{
    s32 VertexIndex;
    dv2 Point;

    b32 Outside;
    s32 Cross;
    b32 Processed;
};

inline b32
PointsAreEqualF64(dv2 a, dv2 b)
{
    b32 Result = ((AbsoluteValue(a.x - b.x) < SUBTRACT_EPSILON_F64) && (AbsoluteValue(a.y - b.y) < SUBTRACT_EPSILON_F64));
    return(Result);
}

inline f64
DistanceToSegmentF64(dv2 p, dv2 a, dv2 b)
{
    f64 Result = 0.0;
    
    f64 l2 = LengthSq(a - b);
    if(l2 == 0.0)
    {
        Result = Length(p - a);
    }
    else
    {
        dv2 pa = p - a;
        dv2 ba = b - a;
        f64 t = Inner(pa, ba) / l2;
        t = Clamp(0, t, 1);

        dv2 Closest = Lerp(a, t, b);

        Result = Length(p - Closest);
    }

    return(Result);
}

inline b32
LineSectF64(dv2 x0, dv2 x1, dv2 y0, dv2 y1, dv2 *res)
{
    b32 Result = true;

    dv2 dx = x1 - x0;
    dv2 dy = y1 - y0;
    dv2 d = x0 - y0;

    f64 dyx = Cross(dy, dx);
    if(!dyx)
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

inline f64
PolygonSignedAreaF64(dpolygon2 *Polygon)
{
    f64 Result = 0.0;
    for(s32 Index = 0;
        Index < Polygon->VertexCount;
        ++Index)
    {
        s32 Next = (Index + 1) % Polygon->VertexCount;
        dv2 FirstVertex = Polygon->Vertices[Index];
        dv2 SecondVertex = Polygon->Vertices[Next];

        Result += Cross(FirstVertex, SecondVertex);
    }

    Result = 0.5*Result;
    
    return(Result);
}

inline void
ProjectVerticesOntoNormalF32(v2 *Vertices, v2 Normal, r32 *Min, r32 *Max)
{
    *Min = *Max = Inner(Vertices[0], Normal);
    for(s32 I = 1;
        I < 3;
        ++I)
    {
        r32 Projection = Inner(Vertices[I], Normal);
        if(Projection < *Min) *Min = Projection;
        if(Projection > *Max) *Max = Projection;
    }
}

inline b32
OverlapExcludingVerticesAndEdgesF32(r32 Min1, r32 Max1, r32 Min2, r32 Max2)
{
    b32 Result = !((Max1 < Min2) || (Max2 < Min1) ||
                   (Max1 == Min2) || (Max2 == Min1));

    return(Result);
}

internal b32
TrianglesOverlapExcludingVerticesAndEdgesF64(triangle *A, triangle *B)
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
        r32 Min1, Max1, Min2, Max2;

        ProjectVerticesOntoNormalF32(A->Vertices, Normals[I], &Min1, &Max1);
        ProjectVerticesOntoNormalF32(B->Vertices, Normals[I], &Min2, &Max2);

        if(!OverlapExcludingVerticesAndEdgesF32(Min1, Max1, Min2, Max2))
        {
            Result = false;
            break;
        }
    }

    return(Result);
}

inline f64
TriangleSignedAreaF64(dv2 a, dv2 b, dv2 c)
{
    f64 Result = 0.5*((a.x - c.x)*(b.y - c.y) - (b.x - c.x)*(a.y - c.y));
    return(Result);
}

inline r32
TriangleSignedAreaF32(triangle *T)
{
    r32 Result = 0.5f*(T->Vertices[0].x*(T->Vertices[1].y - T->Vertices[2].y) +
                       T->Vertices[1].x*(T->Vertices[2].y - T->Vertices[0].y) +
                       T->Vertices[2].x*(T->Vertices[0].y - T->Vertices[1].y));
    return(Result);
}

inline b32
IsTriangleCollinearF32(triangle *A)
{
    b32 Result = (AbsoluteValue(Cross(A->Vertices[1] - A->Vertices[0], A->Vertices[2] - A->Vertices[0])) < 0.16f);
    return(Result);
}

inline b32
InsideF64(dv2 p, dv2 Clipper1, dv2 Clipper2)
{
    b32 Result = false;
    f64 CrossProduct = (Clipper2.x - Clipper1.x) * (p.y - Clipper1.y) - (Clipper2.y - Clipper1.y) * (p.x - Clipper1.x);
    Result = CrossProduct < 0;

    return(Result);
}

inline s32
LeftOfF64(dv2 a, dv2 b, dv2 c)
{
    s32 Result = 0;

    dv2 ba = b - a;
    dv2 cb = c - b;

    f64 x = Cross(ba, cb);

    Result = (x < 0) ? -1 : (x > 0);
    return(Result);
}

inline void
PolyEdgeClipF64(dpolygon2 *Sub, dv2 x0, dv2 x1, s32 Left, dpolygon2 *Res)
{
    dv2 v0 = Sub->Vertices[Sub->VertexCount- 1];
    dv2 v1 = {};

    Res->VertexCount = 0;

    s32 Side0 = LeftOfF64(x0, x1, v0);
    if(Side0 != -Left)
    {
        Res->Vertices[Res->VertexCount++] = v0;
    }

    for(s32 I = 0;
        I < Sub->VertexCount;
        ++I)
    {
        v1 = Sub->Vertices[I];
        s32 Side1 = LeftOfF64(x0, x1, v1);
        if(((Side0 + Side1) == 0) && Side0)
        {
            dv2 Intersect;
            if(LineSectF64(x0, x1, v0, v1, &Intersect))
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
SutherlandHodgmanForTrianglesF64(dpolygon2 *Subject, dpolygon2 *Clip, dpolygon2 *Result, memory_arena *Arena)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    dpolygon2 Temp;
    Temp.VertexCount = 0;
    Temp.Vertices = PushArray(TempMem.Arena, 6, dv2);

    s32 dir = LeftOfF64(Clip->Vertices[0], Clip->Vertices[1], Clip->Vertices[2]);
    PolyEdgeClipF64(Subject, Clip->Vertices[2], Clip->Vertices[0], dir, Result);
    for(s32 I = 0;
        (I < 2) && (Result->VertexCount > 0);
        ++I)
    {
        Temp.VertexCount = Result->VertexCount;
        Copy(sizeof(dv2)*Result->VertexCount, Result->Vertices, Temp.Vertices);
        Result->VertexCount = 0;
        
        PolyEdgeClipF64(&Temp, Clip->Vertices[I], Clip->Vertices[I + 1], dir, Result);
    }

    EndTemporaryMemory(TempMem);
}

inline b32
IsPointInTriangleByAreaF64(dv2 p, dv2 a, dv2 b, dv2 c)
{
    b32 Result = true;

    f64 TotalArea = AbsoluteValue(TriangleSignedAreaF64(a, b, c));
    f64 Area0 = AbsoluteValue(TriangleSignedAreaF64(p, b, c));
    f64 Area1 = AbsoluteValue(TriangleSignedAreaF64(p, a, c));
    f64 Area2 = AbsoluteValue(TriangleSignedAreaF64(p, a, b));

    if((Area0 + Area1 + Area2) > TotalArea)
    {
        Result = false;
    }

    return(Result);
}

internal void
HandleVertexCaseF64(dpolygon2 *A, dpolygon2 *B)
{
    b32 RotateVerticies = false;
    for(s32 VertexIndex = 0;
        VertexIndex < B->VertexCount;
        ++VertexIndex)
    {
        if(PointsAreEqualF64(A->Vertices[0], B->Vertices[VertexIndex]))
        {
            RotateVerticies = true;
            break;
        }
    }
    
    if(RotateVerticies)
    {
        dv2 Temp = A->Vertices[2];
        A->Vertices[2] = A->Vertices[1];
        A->Vertices[1] = A->Vertices[0];
        A->Vertices[0] = Temp;
    }
}

internal void
HandleEdgesF64(dpolygon2 *A, dpolygon2 *B)
{
    b32 RotateVerticies = false;
    for(s32 Index = 0;
        Index < B->VertexCount;
        ++Index)
    {
        dv2 a = B->Vertices[Index];
        dv2 b = B->Vertices[(Index + 1) % B->VertexCount];
        dv2 p = A->Vertices[0];

        if(DistanceToSegmentF64(p, a, b) < SUBTRACT_EPSILON_F64)
        {
            RotateVerticies = true;
            break;
        }
    }

    if(RotateVerticies)
    {
        dv2 Temp = A->Vertices[2];
        A->Vertices[2] = A->Vertices[1];
        A->Vertices[1] = A->Vertices[0];
        A->Vertices[0] = Temp;
    }
}

inline b32
IsNewPointF64(dpolygon2 *A, dv2 p)
{
    b32 Result = true;
    for(s32 I = 0;
        I < A->VertexCount;
        ++I)
    {
        if(PointsAreEqualF64(A->Vertices[I], p))
        {
            Result = false;
            break;
        }
    }

    return(Result);
}

inline b32
PointIsOnEdgeOfPolygon2F64(dpolygon2 *A, dv2 p)
{
    b32 IsOnEdge = false;
    for(s32 I = 0;
        I < A->VertexCount;
        I++)
    {
        s32 K = (I + 1) % A->VertexCount;
        dv2 vertex1 = A->Vertices[I];
        dv2 vertex2 = A->Vertices[K];
        if(!PointsAreEqualF64(vertex1, vertex2))
        {
            if(DistanceToSegmentF64(p, vertex1, vertex2) <= SUBTRACT_EPSILON_F64)
            {
                IsOnEdge = true;
                break;
            }
        }
    }

    return(IsOnEdge);
}

inline void
FindOutsidePointsForF64(dpolygon2 *A, dpolygon2 *B, dv2 *OutsidePoints, s32 *OutsideCount)
{
    for(s32 VertextIndex = 0;
        VertextIndex < A->VertexCount;
        ++VertextIndex)
    {
        dv2 p = A->Vertices[VertextIndex];
        if(!IsPointInTriangleByAreaF64(p, B->Vertices[0], B->Vertices[1], B->Vertices[2]) &&
           !PointIsOnEdgeOfPolygon2F64(B, p))
        {
            OutsidePoints[*OutsideCount] = p;
            (*OutsideCount)++;
        }
    }
}

internal void
BuildVertexTableForF64(dpolygon2 *A, dpolygon2 *B, dv2 *OutsidePoints, s32 OutsideCount, vertex_info *Table)
{
    for(s32 VertexIndex = 0;
        VertexIndex < A->VertexCount;
        ++VertexIndex)
    {
        dv2 p = A->Vertices[VertexIndex];

        vertex_info *Info = Table + VertexIndex;
        Info->VertexIndex = VertexIndex;
        Info->Point = p;
        Info->Processed = false;

        for(s32 OutsideIndex = 0;
            OutsideIndex < OutsideCount;
            ++OutsideIndex)
        {
            dv2 OutsidePoint = OutsidePoints[OutsideIndex];
            if(PointsAreEqualF64(p, OutsidePoint))
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
RecordCrossingPointsF64(vertex_info *A, s32 CountA, vertex_info *B, s32 CountB)
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
                if(PointsAreEqualF64(VertexA->Point, VertexB->Point))
                {
                    VertexA->Cross = J;
                    VertexB->Cross = I;
                }
            }
        }
    }
}

internal b32
ConstructPolygonsForF64(dpolygon2 *A, dpolygon2 *B, vertex_info *InfoA, vertex_info *InfoB, polygon2_set *Set)
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
                Polygon->Vertices = (v2 *)Platform.AllocateMemory(sizeof(v2)*SUBTRACTION_MAX_POINTS_PER_POLYGON);

                ++Set->PolygonCount;
                break;
            }
        }

        polygon2 *CurrentPolygon = Set->Polygons + (Set->PolygonCount - 1);
        if(IndexA != -1)
        {
            for(;;)
            {
                if(CurrentPolygon->VertexCount >= SUBTRACTION_MAX_POINTS_PER_POLYGON)
                {
                    Result = false;
                    break;
                }
                
                CurrentPolygon->Vertices[CurrentPolygon->VertexCount++] = V2((r32)InfoA[IndexA].Point.x, (r32)InfoA[IndexA].Point.y); 
                InfoA[IndexA].Processed = true;

                // NOTE(babykaban): Check if polygon completed
                dv2 FirstOutputPoint = dV2(CurrentPolygon->Vertices[0]);
                dv2 ThisPoint = InfoA[IndexA].Point;
                if(CurrentPolygon->VertexCount > 1)
                {
                    if(PointsAreEqualF64(FirstOutputPoint, ThisPoint))
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
                        if(CurrentPolygon->VertexCount >= SUBTRACTION_MAX_POINTS_PER_POLYGON)
                        {
                            Result = false;
                            break;
                        }

                        IndexB = ((IndexB - 1) + B->VertexCount) % B->VertexCount;
                        CurrentPolygon->Vertices[CurrentPolygon->VertexCount++] = V2((r32)InfoB[IndexB].Point.x, (r32)InfoB[IndexB].Point.y); 

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

internal b32
IntersectLineSegmentF64(dpolygon2 *A, dv2 p1, dv2 p2, dv2 *Points, s32 *PointCount, memory_arena *Arena)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    b32 Intersect = false;
    s32 TempPointCount = 0;
    dv2 *TempPoints = PushArray(TempMem.Arena, SUBTRACTION_MAX_POINTS_PER_POLYGON, dv2);

    dv2 p;
    for(s32 I = 0;
        I < (A->VertexCount - 1);
        ++I)
    {
        s32 J = (I + 1) % (A->VertexCount - 1);
        dv2 p3 = A->Vertices[I];
        dv2 p4 = A->Vertices[J];

        b32 Ends = false;
        if(DistanceToSegmentF64(p3, p1, p2) < SUBTRACT_EPSILON_F64)
        {
            TempPoints[TempPointCount++] = p3;
            Intersect = true;
            Ends = true;
        }

        if(DistanceToSegmentF64(p4, p1, p2) < SUBTRACT_EPSILON_F64)
        {
            TempPoints[TempPointCount++] = p4;
            Intersect = true;
            Ends = true;
        }

        if(!Ends && LineSectF64(p1, p2, p3, p4, &p))
        {
            TempPoints[TempPointCount++] = p;
            Intersect = true;
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
                Entry->SortKey = (r32)Length(p1 - TempPoints[I]);
            }

            RadixSort(SortCount, SortArray, Temp);

            dv2 Last = TempPoints[SortArray[0].Index];
            Points[(*PointCount)++] = Last;
            for(s32 I = 1;
                I < TempPointCount;
                ++I)
            {
                dv2 This = TempPoints[SortArray[I].Index];
                if(!PointsAreEqualF64(This, Last))
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

internal void
GetIntersectionPointsF64(dpolygon2 *A, dpolygon2 *B, dv2 *OutputPoints, s32 *OutputCount, memory_arena *Arena)
{
    temporary_memory TempMem = BeginTemporaryMemory(Arena);
    s32 TempCount = 0;
    dv2 *Temp = PushArray(TempMem.Arena, SUBTRACTION_MAX_POINTS_PER_POLYGON, dv2);

    for(s32 I = 0;
        I < A->VertexCount;
        ++I)
    {
        dv2 Prv = A->Vertices[I];
        dv2 Cur = A->Vertices[(I + 1) % A->VertexCount];
        if(!PointsAreEqualF64(Prv, Cur))
        {
            IntersectLineSegmentF64(B, Prv, Cur, Temp, &TempCount, Arena);
            for(s32 J = 0;
                J < TempCount;
                ++J)
            {
                OutputPoints[(*OutputCount)++] = Temp[J];
            }
            TempCount = 0;
        }
    }

    EndTemporaryMemory(TempMem);
}

inline between_indecies
FindPolygon2EdgeIndeciesForF64(dpolygon2 *A, dv2 p)
{
    between_indecies Result = {};

    for(s32 I = 0;
        I < A->VertexCount;
        I++)
    {
        s32 K = (I + 1) % A->VertexCount;

        dv2 vertex1 = A->Vertices[I];
        dv2 vertex2 = A->Vertices[K];

        if(DistanceToSegmentF64(p, vertex1, vertex2) < SUBTRACT_EPSILON_F64)
        {
            Result.Index0 = I;
            Result.Index1 = K;
            break;
        }
    }

    return(Result);
}

inline void
InsertPointBetweenF64(dpolygon2 *A, dv2 p, between_indecies Indecies)
{
    for(s32 I = A->VertexCount;
        I > Indecies.Index1;
        --I)
    {
        A->Vertices[I] = A->Vertices[I - 1];
    }

    ++A->VertexCount;
    A->Vertices[Indecies.Index1] = p;
}

inline void
InsertPointBetweenF64(dpolygon2 *A, dv2 p)
{
    between_indecies Indecies = FindPolygon2EdgeIndeciesForF64(A, p); 
    InsertPointBetweenF64(A, p, Indecies);
}

inline void
RemoveDublicatsF64(dpolygon2 *Poly)
{
    for(s32 I = 0;
        I < Poly->VertexCount;
        ++I)
    {
        dv2 Cur = Poly->Vertices[I];
        dv2 Next = Poly->Vertices[(I + 1) % Poly->VertexCount];
        if(PointsAreEqualF64(Cur, Next))
        {
            Poly->Vertices[I + 1] = {};
            for(s32 J = I + 1;
                J < (Poly->VertexCount - 1);
                ++J)
            {
                Poly->Vertices[J] = Poly->Vertices[J + 1];                
            }
            --Poly->VertexCount;
        }
    }
}

internal subtract_result
SubtractTriangelsF64(triangle *Subject, triangle *Subtractor, r32 MinimalOverlapArea, r32 ColliniarValue, memory_arena *Arena)
{
    TIMED_FUNCTION();

    subtract_result Result = {};
    Result.Set.Polygons = (polygon2 *)Platform.AllocateMemory(sizeof(polygon2)*SUBTRACTION_MAX_POLYGON_COUNT);

    temporary_memory TempMem = BeginTemporaryMemory(Arena);

    b32 ExcludingOverlap = TrianglesOverlapExcludingVerticesAndEdgesF64(Subject, Subtractor);
    r32 AreaA = TriangleSignedAreaF32(Subject);
    r32 AreaB = TriangleSignedAreaF32(Subtractor);

    // NOTE(babykaban): Changing orrientation to be clockwise
    if(AreaA > 0.0f)
    {
        v2 Temp = Subject->Vertices[1];
        Subject->Vertices[1] = Subject->Vertices[2];
        Subject->Vertices[2] = Temp;
    }

    if(AreaB > 0.0f)
    {
        v2 Temp = Subtractor->Vertices[1];
        Subtractor->Vertices[1] = Subtractor->Vertices[2];
        Subtractor->Vertices[2] = Temp;
    }

    // NOTE(babykaban): Checks to ensure that both triangles are valid
    if(ExcludingOverlap && !IsTriangleCollinearF32(Subject) && !IsTriangleCollinearF32(Subtractor))
    {

        dpolygon2 PolygonA = {};
        PolygonA.Vertices = PushArray(TempMem.Arena, SUBTRACTION_MAX_POINTS_PER_POLYGON, dv2);
        dpolygon2 PolygonB = {};
        PolygonB.Vertices = PushArray(TempMem.Arena, SUBTRACTION_MAX_POINTS_PER_POLYGON, dv2);

        PolygonA.Vertices[0] = dV2(Subject->Vertices[0]);
        PolygonA.Vertices[1] = dV2(Subject->Vertices[1]);
        PolygonA.Vertices[2] = dV2(Subject->Vertices[2]);
        PolygonA.VertexCount = 3;

        PolygonB.Vertices[0] = dV2(Subtractor->Vertices[0]);
        PolygonB.Vertices[1] = dV2(Subtractor->Vertices[1]);
        PolygonB.Vertices[2] = dV2(Subtractor->Vertices[2]);
        PolygonB.VertexCount = 3;

        // NOTE(babykaban): Constracting the overlap polygon
        dpolygon2 Poly = {};
        Poly.Vertices = PushArray(TempMem.Arena, SUBTRACTION_MAX_POINTS_PER_POLYGON, dv2);
        SutherlandHodgmanForTrianglesF64(&PolygonA, &PolygonB, &Poly, TempMem.Arena);
        RemoveDublicatsF64(&Poly);

        f64 OverlappingArea = AbsoluteValue(PolygonSignedAreaF64(&Poly));
        if(OverlappingArea > 0.0f)
        {
            // NOTE(babykaban): Checks if any points of a subtractor lies on first vertex of subject if so rotate points in array
            HandleVertexCaseF64(&PolygonA, &PolygonB);

            // NOTE(babykaban): Same as above but for edges
            HandleEdgesF64(&PolygonA, &PolygonB);

            // NOTE(babykaban): Adding first vertices to the end, for algorithm to work properly
            PolygonA.Vertices[3] = PolygonA.Vertices[0];
            Poly.Vertices[Poly.VertexCount] = Poly.Vertices[0];
            ++PolygonA.VertexCount;
            ++Poly.VertexCount;

            // NOTE(babykaban): Find outside points for Subject polygon, because Overlap polygon will always be inside (on edge points are inside)
            s32 OutsidePointCount = 0;
            dv2 *OutsidePoints = PushArray(TempMem.Arena, SUBTRACTION_MAX_POINTS_PER_POLYGON, dv2);
            FindOutsidePointsForF64(&PolygonA, &Poly, OutsidePoints, &OutsidePointCount);

            // NOTE(babykaban): Find intersection points for subject polygon
            s32 NewPointCount = 0;
            dv2 *PointsToAdd = PushArray(TempMem.Arena, SUBTRACTION_MAX_POINTS_PER_POLYGON, dv2);
            GetIntersectionPointsF64(&PolygonA, &Poly, PointsToAdd, &NewPointCount, TempMem.Arena);

            // NOTE(babykaban): If no intersections was found record subtractor as a hole
            if(NewPointCount != 0)
            {

                // NOTE(babykaban): Insert points avoiding dublicats
                for(s32 IntersectIndex = 0; 
                    IntersectIndex < NewPointCount;
                    ++IntersectIndex)
                {
                    dv2 p = PointsToAdd[IntersectIndex];
                    if(IsNewPointF64(&PolygonA, p))
                    {
                        InsertPointBetweenF64(&PolygonA, p);
                    }
                }

                // NOTE(babykaban): Constract vertex table for both polygons
                vertex_info *PolygonAVertexInfo = PushArray(TempMem.Arena, PolygonA.VertexCount, vertex_info);
                BuildVertexTableForF64(&PolygonA, &Poly, OutsidePoints, OutsidePointCount, PolygonAVertexInfo);

                vertex_info *PolygonBVertexInfo = PushArray(TempMem.Arena, Poly.VertexCount, vertex_info);
                BuildVertexTableForF64(&Poly, &PolygonA, 0, 0, PolygonBVertexInfo);

                RecordCrossingPointsF64(PolygonAVertexInfo, PolygonA.VertexCount, PolygonBVertexInfo, Poly.VertexCount);

                // NOTE(babykaban): Construct resulting polygons
                Result.Fail = ConstructPolygonsForF64(&PolygonA, &Poly, PolygonAVertexInfo, PolygonBVertexInfo, &Result.Set);
            }
            else
            {
                Result.Set.Polygons[0].Vertices = (v2 *)Platform.AllocateMemory(sizeof(v2)*3);

                Result.Set.Polygons[0].Vertices[0] = Subject->Vertices[0];
                Result.Set.Polygons[0].Vertices[1] = Subject->Vertices[1];
                Result.Set.Polygons[0].Vertices[2] = Subject->Vertices[2];
                Result.Set.Polygons[0].VertexCount = 3;

                Result.Set.Polygons[0].HasHole = true;
                Result.Set.Polygons[0].HoleVertices = (v2 *)Platform.AllocateMemory(sizeof(v2)*3);
                Result.Set.Polygons[0].HoleVertices[0] = Subtractor->Vertices[0];
                Result.Set.Polygons[0].HoleVertices[1] = Subtractor->Vertices[1];
                Result.Set.Polygons[0].HoleVertices[2] = Subtractor->Vertices[2];
                Result.Set.Polygons[0].HoleVertexCount = 3;
                Result.Fail = true;

                Result.Set.PolygonCount = 1;
            }
        }
        else
        {
            Result.Fail = true;
        }
    }
    else
    {
        Result.Fail = true;
    }

    EndTemporaryMemory(TempMem);

    return(Result);
}

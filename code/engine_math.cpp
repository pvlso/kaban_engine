/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#include "engine_triangle.cpp"
#include "engine_triangle_f64.cpp"


internal void
SplitPolygon(polygon2 *ResultArray, s32 *ResultCount, s32 *NotConvexIndices, s32 *NotConvexCount, s32 SubjectIndex, memory_arena *TempArena)
{
    s32 ArrayEnd = (*ResultCount) - 1;
    polygon2 *Poly = ResultArray + SubjectIndex;
    if((Poly->VertexCount > 3) && !IsConvex(Poly))
    {
        b32 Clockwise = (PolygonSignedArea2(Poly) < 0.0f);
    
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

/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal b32
IsPointInPolygon(render_group *RenderGroup, object_transform *Flat, v2 p3, v2 p4, polygon2 *Poly)
{
    b32 Result = false;

    s32 Count = 0;
    v2 p;
    for(s32 VertexIndex = 0;
        VertexIndex < Poly->VertexCount;
        ++VertexIndex)
    {
        v2 p1 = Poly->Vertices[VertexIndex];
        v2 p2 = Poly->Vertices[(VertexIndex + 1) % Poly->VertexCount];
        if(LineIntersect(p1, p2, p3, p4, &p) > 0)
        {
            ++Count;
        }

//        PushLine(RenderGroup, Flat, V3(p3, 20.0f), V3(p, 20.0f), V4(1, 1, 0, 1));
        PushLine(RenderGroup, Flat, V3(p1, 20.0f), V3(p2, 20.0f), V4(1, 1, 0, 1));
    }

    if(Count % 2)
    {
        Result = true;
    }
    
    return(Result);
}

inline fp22_10
FixedArea2(fp22_10_v2 a, fp22_10_v2 b, fp22_10_v2 c)
{
    fp22_10 Result = Cross(a, b) + Cross(c, a) + Cross(b, c);

    return(Result);
}

inline b32
FixedLeft(fp22_10 Area2)
{
    b32 Result = (Area2 > 0);
    return(Result);
}

inline b32
FixedCollinear(fp22_10 Area2)
{
    b32 Result = (Area2 == 0);
    return(Result);
}

inline b32
FixedIntersectProp(fp22_10_v2 a, fp22_10_v2 b, fp22_10_v2 c, fp22_10_v2 d)
{
    b32 Result = false;

    fp22_10 Area2abc = FixedArea2(a, b, c);
    fp22_10 Area2abd = FixedArea2(a, b, d);
    fp22_10 Area2cda = FixedArea2(c, d, a);
    fp22_10 Area2cdb = FixedArea2(c, d, b);

    if(!FixedCollinear(Area2abc) && !FixedCollinear(Area2abd) &&
       !FixedCollinear(Area2cda) && !FixedCollinear(Area2cdb))
    {
        Result = ((FixedLeft(Area2abc) != FixedLeft(Area2abd)) &&
                  (FixedLeft(Area2cda) != FixedLeft(Area2cdb)));
    }

    return(Result);
}

internal s32
MarkOutsideTriangles(render_group *RenderGroup, object_transform *Flat, polygonfp22_10 *Poly, fp22_10_v2 *Points,
                     triangulate_triangle *Triangles, s32 TriangleCount, b32 *IsOutside, memory_arena *Arena)
{
    TIMED_FUNCTION();

    s32 Result = TriangleCount;

    u32 *Counts = PushArray(Arena, TriangleCount, u32, Align(32, true));
    linefp22_10 *TestLines = PushArray(Arena, TriangleCount, linefp22_10);

    fp22_10 OneOverThree = F32ToFixed(1.0f / 3.0f);
    for(s32 I = 0;
        I < TriangleCount;
        ++I)
    {
        triangulate_triangle *T = Triangles + I;
        linefp22_10 *Line = TestLines + I;
        Line->a = OneOverThree*(Points[T->V1] + Points[T->V2] + Points[T->V3]);
        Line->b.x = fixed_mul(F32ToFixed(13.0f), Line->a.x);
        Line->b.y = fixed_mul(F32ToFixed(17.0f), Line->a.y);//V2(17.0f*(AbsoluteValue(Line->a.x) + 3.0f), 33.0f*Line->a.y);

        PushLine(RenderGroup, Flat, V3(FixedToF32(Line->a.x), FixedToF32(Line->a.y), 20.0f),
                 V3(FixedToF32(Line->b.x), FixedToF32(Line->b.y), 20.0f), V4(1, 1, 0, 1));
    }

    for(s32 VertexIndex = 0;
        VertexIndex < Poly->VertexCount;
        ++VertexIndex)
    {
        fp22_10_v2 p1 = Poly->Vertices[VertexIndex];
        fp22_10_v2 p2 = Poly->Vertices[(VertexIndex + 1) % Poly->VertexCount];

        for(s32 LineIndex = 0;
            LineIndex < TriangleCount;
            ++LineIndex)
        {
            fp22_10_v2 p3 = TestLines[LineIndex].a;
            fp22_10_v2 p4 = TestLines[LineIndex].b;

            fp22_10 Area2abc = FixedArea2(p1, p2, p3);
            fp22_10 Area2abd = FixedArea2(p1, p2, p4);
            fp22_10 Area2cda = FixedArea2(p3, p4, p1);
            fp22_10 Area2cdb = FixedArea2(p3, p4, p2);

            if(!FixedCollinear(Area2abc) && !FixedCollinear(Area2abd) &&
               !FixedCollinear(Area2cda) && !FixedCollinear(Area2cdb))
            {
                if((FixedLeft(Area2abc) != FixedLeft(Area2abd)) &&
                   (FixedLeft(Area2cda) != FixedLeft(Area2cdb)))
                {
                    Counts[LineIndex] += 1;
                }
            }
        }
    }

#if 0
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
            fp22_10_v2 p1 = Poly->HolesVertices[Offset + VertexIndex];
            fp22_10_v2 p2 = Poly->HolesVertices[Offset + ((VertexIndex + 1) % VertexCount)];

            for(s32 LineIndex = 0;
                LineIndex < TriangleCount;
                ++LineIndex)
            {
                fp22_10_v2 p3 = TestLines[LineIndex].a;
                fp22_10_v2 p4 = TestLines[LineIndex].b;
                if(FixedIntersectProp(p1, p2, p3, p4))
                {
                    Counts[LineIndex] += 1;
                }
            }
        }

        Offset += VertexCount;
    }
#endif
    
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

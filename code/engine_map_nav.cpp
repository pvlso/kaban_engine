/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

struct map_nav_bake_poly
{
    s32 Level;
    u32 Count;
    v2 *Points;
    map_nav_bake_poly *Next;
};

struct map_nav_bake_link
{
    u32 Poly;
    u32 To;
    v2 A;
    v2 B;
    map_nav_bake_link *Next;
};

inline r32
MapNavSignedArea(u32 Count, v2 *Points)
{
    r32 Result = 0.0f;
    for(u32 Index = 0; Index < Count; ++Index)
    {
        Result += Cross(Points[Index], Points[(Index + 1) % Count]);
    }

    return(0.5f*Result);
}

inline void
ReverseMapNavPoints(u32 Count, v2 *Points)
{
    for(u32 Index = 0; Index < Count / 2; ++Index)
    {
        v2 Swap = Points[Index];
        Points[Index] = Points[Count - 1 - Index];
        Points[Count - 1 - Index] = Swap;
    }
}

// NOTE(pvlso): A level's areas of one kind, all winding one way: the fill rule would cancel overlaps that wind apart
internal clip_polygon_set
MapNavClipSet(memory_arena *Arena, u32 NavCount, map_file_nav *Navs, s32 Level, b32 Hole)
{
    clip_polygon_set Result = {};
    Result.Polygons = PushArray(Arena, Maximum(1u, NavCount), clip_polygon);
    for(u32 NavIndex = 0; NavIndex < NavCount; ++NavIndex)
    {
        map_file_nav *Nav = Navs + NavIndex;
        if((Nav->Level == Level) && ((Nav->Hole != 0) == (Hole != 0)) && (Nav->PointCount >= 3))
        {
            clip_polygon *Polygon = Result.Polygons + Result.Count++;
            Polygon->Count = Nav->PointCount;
            Polygon->Points = PushArray(Arena, Nav->PointCount, v2);
            Copy(Nav->PointCount*sizeof(v2), Nav->Points, Polygon->Points);
            if(MapNavSignedArea(Polygon->Count, Polygon->Points) < 0.0f)
            {
                ReverseMapNavPoints(Polygon->Count, Polygon->Points);
            }
        }
    }

    return(Result);
}

// NOTE(pvlso): One level's walkable ground as convex polygons, put on the front of *Bake. The polygon count comes back.
internal u32
BakeMapNavLevel(memory_arena *Arena, u32 NavCount, map_file_nav *Navs, s32 Level, map_nav_bake_poly **Bake)
{
    u32 Result = 0;
    clip_polygon_set Walks = MapNavClipSet(Arena, NavCount, Navs, Level, false);
    if(Walks.Count)
    {
        clip_polygon_set Holes = MapNavClipSet(Arena, NavCount, Navs, Level, true);
        clip_polygon_set Cut = ClipPolygons(Arena, ClipType_Difference, &Walks, &Holes);

        epp_poly_list In = {};
        DLIST_INIT(&In);
        epp_poly_list *Free = 0;
        for(u32 PolygonIndex = 0; PolygonIndex < Cut.Count; ++PolygonIndex)
        {
            clip_polygon *Polygon = Cut.Polygons + PolygonIndex;
            epp_poly_list *New = 0;
            POLY_FREELIST_ALLOCATE(New, Free, PushStruct(Arena, epp_poly_list));
            epp_poly *Poly = &New->Poly;
            *Poly = {};
            Poly->points = PushArray(Arena, Maximum(1u, Polygon->Count), epp_point);
            for(u32 PointIndex = 0; PointIndex < Polygon->Count; ++PointIndex)
            {
                // NOTE(pvlso): Clipper's results are near the points it was given: back onto them, so a tenth of a cell
                // stays one and levels' edges meet
                epp_point Point = {};
                Point.x = RoundReal32ToInt32(Polygon->Points[PointIndex].x*MAP_NAV_ROUNDING) / (f64)MAP_NAV_ROUNDING;
                Point.y = RoundReal32ToInt32(Polygon->Points[PointIndex].y*MAP_NAV_ROUNDING) / (f64)MAP_NAV_ROUNDING;
                if((Poly->numpoints == 0) || (Poly->points[Poly->numpoints - 1].x != Point.x) ||
                   (Poly->points[Poly->numpoints - 1].y != Point.y))
                {
                    Point.id = Poly->numpoints;
                    Poly->points[Poly->numpoints++] = Point;
                }
            }
            // NOTE(pvlso): Clipper's outlines have a positive area, its holes a negative one
            Poly->hole = (MapNavSignedArea(Polygon->Count, Polygon->Points) < 0.0f);
            if(Poly->numpoints >= 3)
            {
                EPPSetOrientation(Poly, Poly->hole ? EPP_ORIENTATION_CW : EPP_ORIENTATION_CCW);
                DLIST_INSERT(&In, New);
            }
        }

        epp_poly_list Out = {};
        DLIST_INIT(&Out);
        if(!EPPConvexPartitionHM(&In, &Out, &Free, Arena))
        {
            LOG_ERROR("Navigation level %d couldn't be split into convex polygons: do its areas cross themselves?", Level);
        }
        for(epp_poly_list *Iter = Out.Next; Iter != &Out; Iter = Iter->Next)
        {
            epp_poly *Poly = &Iter->Poly;
            map_nav_bake_poly *New = PushStruct(Arena, map_nav_bake_poly);
            New->Level = Level;
            New->Count = (u32)Poly->numpoints;
            New->Points = PushArray(Arena, New->Count, v2);
            for(u32 PointIndex = 0; PointIndex < New->Count; ++PointIndex)
            {
                New->Points[PointIndex] = V2((r32)Poly->points[PointIndex].x, (r32)Poly->points[PointIndex].y);
            }
            if(MapNavSignedArea(New->Count, New->Points) < 0.0f)
            {
                ReverseMapNavPoints(New->Count, New->Points);
            }
            New->Next = *Bake;
            *Bake = New;
            ++Result;
        }
    }

    return(Result);
}

// NOTE(pvlso): The part two edges share when they lie on one line, running opposite ways (so the polygons they wind
// around are on opposite sides), as From to To along A0 to A1
internal b32
MapNavEdgesTouch(v2 A0, v2 A1, v2 B0, v2 B1, v2 *From, v2 *To)
{
    b32 Result = false;
    v2 Edge = A1 - A0;
    r32 Length = SquareRoot(LengthSq(Edge));
    if(Length > MAP_NAV_EPSILON)
    {
        v2 Along = (1.0f / Length)*Edge;
        r32 Distance0 = Cross(Along, B0 - A0);
        r32 Distance1 = Cross(Along, B1 - A0);
        r32 T0 = Inner(Along, B0 - A0);
        r32 T1 = Inner(Along, B1 - A0);
        if((AbsoluteValue(Distance0) <= MAP_NAV_EPSILON) && (AbsoluteValue(Distance1) <= MAP_NAV_EPSILON) && (T1 < T0))
        {
            r32 Min = Maximum(0.0f, T1);
            r32 Max = Minimum(Length, T0);
            if((Max - Min) > MAP_NAV_EPSILON)
            {
                *From = A0 + Min*Along;
                *To = A0 + Max*Along;
                Result = true;
            }
        }
    }

    return(Result);
}

// NOTE(pvlso): Links between two polygons, one in each's list, over the first edges that touch (convex polygons touch
// over one line at most). Across levels only the edges nothing on their own level is linked over count: those are
// where a level stops. The count of links comes back.
internal u32
LinkMapNavPolys(memory_arena *Arena, map_nav_mesh *Mesh, u32 PolyA, u32 PolyB, b32 *EdgeLinked, map_nav_bake_link **Links)
{
    u32 Result = 0;
    kea_nav_poly *A = Mesh->Polys + PolyA;
    kea_nav_poly *B = Mesh->Polys + PolyB;
    b32 SameLevel = (A->Level == B->Level);
    for(u32 IndexA = 0; !Result && (IndexA < A->PointCount); ++IndexA)
    {
        u32 EdgeA = A->FirstPoint + IndexA;
        if(!SameLevel && EdgeLinked[EdgeA]) {continue;}
        v2 A0 = Mesh->Points[EdgeA];
        v2 A1 = Mesh->Points[A->FirstPoint + (IndexA + 1) % A->PointCount];
        for(u32 IndexB = 0; !Result && (IndexB < B->PointCount); ++IndexB)
        {
            u32 EdgeB = B->FirstPoint + IndexB;
            if(!SameLevel && EdgeLinked[EdgeB]) {continue;}
            v2 B0 = Mesh->Points[EdgeB];
            v2 B1 = Mesh->Points[B->FirstPoint + (IndexB + 1) % B->PointCount];
            v2 From = {};
            v2 To = {};
            if(MapNavEdgesTouch(A0, A1, B0, B1, &From, &To))
            {
                map_nav_bake_link *ToB = PushStruct(Arena, map_nav_bake_link);
                ToB->Poly = PolyA;
                ToB->To = PolyB;
                ToB->A = From;
                ToB->B = To;
                ToB->Next = *Links;
                *Links = ToB;

                map_nav_bake_link *ToA = PushStruct(Arena, map_nav_bake_link);
                ToA->Poly = PolyB;
                ToA->To = PolyA;
                ToA->A = To;
                ToA->B = From;
                ToA->Next = *Links;
                *Links = ToA;

                if(SameLevel)
                {
                    EdgeLinked[EdgeA] = true;
                    EdgeLinked[EdgeB] = true;
                }
                Result = 2;
            }
        }
    }

    return(Result);
}

// NOTE(pvlso): The mesh of a map's areas, all of it in Arena with what the bake worked in, so Arena should be
// temporary memory that lives as long as the mesh is needed
internal map_nav_mesh
BakeMapNav(memory_arena *Arena, u32 NavCount, map_file_nav *Navs)
{
    map_nav_mesh Result = {};
    map_nav_bake_poly *Bake = 0;
    for(s32 Level = MAP_MIN_LEVEL; Level <= MAP_MAX_LEVEL; ++Level)
    {
        Result.PolyCount += BakeMapNavLevel(Arena, NavCount, Navs, Level, &Bake);
    }
    for(map_nav_bake_poly *Poly = Bake; Poly; Poly = Poly->Next)
    {
        Result.PointCount += Poly->Count;
    }

    Result.Polys = PushArray(Arena, Maximum(1u, Result.PolyCount), kea_nav_poly);
    Result.Points = PushArray(Arena, Maximum(1u, Result.PointCount), v2);
    u32 PolyIndex = 0;
    u32 PointAt = 0;
    for(map_nav_bake_poly *Poly = Bake; Poly; Poly = Poly->Next)
    {
        kea_nav_poly *Dest = Result.Polys + PolyIndex++;
        *Dest = {};
        Dest->Level = Poly->Level;
        Dest->FirstPoint = PointAt;
        Dest->PointCount = Poly->Count;
        Copy(Poly->Count*sizeof(v2), Poly->Points, Result.Points + PointAt);
        PointAt += Poly->Count;
    }

    // NOTE(pvlso): A level's own links first: they tell the edges where it stops, for the links across levels
    b32 *EdgeLinked = PushArray(Arena, Maximum(1u, Result.PointCount), b32);
    ZeroSize(Maximum(1u, Result.PointCount)*sizeof(b32), EdgeLinked);
    map_nav_bake_link *Links = 0;
    for(u32 Pass = 0; Pass < 2; ++Pass)
    {
        for(u32 PolyA = 0; PolyA < Result.PolyCount; ++PolyA)
        {
            for(u32 PolyB = PolyA + 1; PolyB < Result.PolyCount; ++PolyB)
            {
                b32 SameLevel = (Result.Polys[PolyA].Level == Result.Polys[PolyB].Level);
                if(SameLevel == (Pass == 0))
                {
                    Result.LinkCount += LinkMapNavPolys(Arena, &Result, PolyA, PolyB, EdgeLinked, &Links);
                }
            }
        }
    }

    Result.Links = PushArray(Arena, Maximum(1u, Result.LinkCount), kea_nav_link);
    u32 LinkAt = 0;
    for(u32 Index = 0; Index < Result.PolyCount; ++Index)
    {
        kea_nav_poly *Poly = Result.Polys + Index;
        Poly->FirstLink = LinkAt;
        for(map_nav_bake_link *Link = Links; Link; Link = Link->Next)
        {
            if(Link->Poly == Index)
            {
                kea_nav_link *Dest = Result.Links + LinkAt++;
                Dest->Poly = Link->To;
                Dest->A[0] = Link->A.x;
                Dest->A[1] = Link->A.y;
                Dest->B[0] = Link->B.x;
                Dest->B[1] = Link->B.y;
            }
        }
        Poly->LinkCount = LinkAt - Poly->FirstLink;
    }

    return(Result);
}

// NOTE(pvlso): The polygon under P: on Level if there's one, else the highest one there; -1 when P isn't on the mesh
internal s32
FindMapNavPoly(map_nav_mesh *Mesh, v2 P, s32 Level)
{
    s32 Result = -1;
    for(u32 PolyIndex = 0; PolyIndex < Mesh->PolyCount; ++PolyIndex)
    {
        kea_nav_poly *Poly = Mesh->Polys + PolyIndex;
        b32 Inside = true;
        for(u32 Index = 0; Inside && (Index < Poly->PointCount); ++Index)
        {
            v2 A = Mesh->Points[Poly->FirstPoint + Index];
            v2 B = Mesh->Points[Poly->FirstPoint + (Index + 1) % Poly->PointCount];
            Inside = (Cross(B - A, P - A) >= -MAP_NAV_EPSILON*MAP_NAV_EPSILON);
        }
        if(Inside)
        {
            if(Poly->Level == Level)
            {
                Result = (s32)PolyIndex;
                break;
            }
            if((Result < 0) || (Poly->Level > Mesh->Polys[Result].Level)) {Result = (s32)PolyIndex;}
        }
    }

    return(Result);
}

// NOTE(pvlso): A* over the polygons, from the middle of one link to the next. True when ToPoly can be reached; each
// polygon on the way then has its Parent and Link.
internal b32
SearchMapNav(map_nav_mesh *Mesh, map_nav_search_node *Nodes, v2 From, s32 FromPoly, v2 To, s32 ToPoly)
{
    for(u32 Index = 0; Index < Mesh->PolyCount; ++Index)
    {
        Nodes[Index] = {};
        Nodes[Index].Parent = -1;
    }
    map_nav_search_node *Start = Nodes + FromPoly;
    Start->Entry = From;
    Start->Total = SquareRoot(LengthSq(To - From));
    Start->Open = true;

    b32 Result = false;
    for(;;)
    {
        s32 Best = -1;
        for(u32 Index = 0; Index < Mesh->PolyCount; ++Index)
        {
            if(Nodes[Index].Open && ((Best < 0) || (Nodes[Index].Total < Nodes[Best].Total))) {Best = (s32)Index;}
        }
        if(Best < 0) {break;}
        if(Best == ToPoly)
        {
            Result = true;
            break;
        }

        map_nav_search_node *Node = Nodes + Best;
        Node->Open = false;
        Node->Closed = true;
        kea_nav_poly *Poly = Mesh->Polys + Best;
        for(u32 Index = 0; Index < Poly->LinkCount; ++Index)
        {
            kea_nav_link *Link = Mesh->Links + Poly->FirstLink + Index;
            map_nav_search_node *Next = Nodes + Link->Poly;
            if(!Next->Closed)
            {
                v2 Middle = 0.5f*(V2(Link->A[0], Link->A[1]) + V2(Link->B[0], Link->B[1]));
                r32 Cost = Node->Cost + SquareRoot(LengthSq(Middle - Node->Entry));
                if(!Next->Open || (Cost < Next->Cost))
                {
                    Next->Cost = Cost;
                    Next->Total = Cost + SquareRoot(LengthSq(To - Middle));
                    Next->Entry = Middle;
                    Next->Parent = Best;
                    Next->Link = Poly->FirstLink + Index;
                    Next->Open = true;
                }
            }
        }
    }

    return(Result);
}

// NOTE(pvlso): The funnel over Portals (left, right per portal; the first and last both a path's end): the path's
// corners, and which portal each is on. The old navigation mesh code's StringPull (after Mikko Mononen's).
internal u32
PullMapNavPath(v2 *Portals, u32 PortalCount, v2 *Corners, s32 *CornerPortals, u32 MaxCorners)
{
    u32 Result = 0;
    s32 Apex = 0;
    s32 LeftIndex = 0;
    s32 RightIndex = 0;
    v2 PortalApex = Portals[0];
    v2 PortalLeft = Portals[0];
    v2 PortalRight = Portals[1];

    Corners[Result] = PortalApex;
    CornerPortals[Result++] = 0;

    for(s32 Index = 1; (Index < (s32)PortalCount) && ((Result + 1) < MaxCorners); ++Index)
    {
        v2 Left = Portals[Index*2];
        v2 Right = Portals[Index*2 + 1];

        if(TriangleArea2(PortalApex, PortalRight, Right) <= 0.0f)
        {
            if(PointsAreEqual(PortalApex, PortalRight, 0.001f) || (TriangleArea2(PortalApex, PortalLeft, Right) > 0.0f))
            {
                PortalRight = Right;
                RightIndex = Index;
            }
            else
            {
                Corners[Result] = PortalLeft;
                CornerPortals[Result++] = LeftIndex;
                PortalApex = PortalLeft;
                Apex = LeftIndex;
                PortalLeft = PortalApex;
                PortalRight = PortalApex;
                LeftIndex = Apex;
                RightIndex = Apex;
                Index = Apex;
                continue;
            }
        }

        if(TriangleArea2(PortalApex, PortalLeft, Left) >= 0.0f)
        {
            if(PointsAreEqual(PortalApex, PortalLeft, 0.001f) || (TriangleArea2(PortalApex, PortalRight, Left) < 0.0f))
            {
                PortalLeft = Left;
                LeftIndex = Index;
            }
            else
            {
                Corners[Result] = PortalRight;
                CornerPortals[Result++] = RightIndex;
                PortalApex = PortalRight;
                Apex = RightIndex;
                PortalLeft = PortalApex;
                PortalRight = PortalApex;
                LeftIndex = Apex;
                RightIndex = Apex;
                Index = Apex;
                continue;
            }
        }
    }

    // NOTE(pvlso): The funnel may have ended on the goal already
    v2 Goal = Portals[(PortalCount - 1)*2];
    if(PointsAreEqual(Corners[Result - 1], Goal, 0.001f)) {--Result;}
    Corners[Result] = Goal;
    CornerPortals[Result++] = (s32)PortalCount - 1;

    return(Result);
}

// NOTE(pvlso): The way from one place to another: its corners and where it crosses from one level to another, each
// with the level from that point on. The count comes back (points past MaxPoints are left out, what's there is still
// a good start), 0 when there's no way or From or To isn't on the mesh.
internal u32
FindMapNavPath(map_nav_mesh *Mesh, map_nav_scratch *Scratch, v2 From, s32 FromLevel, v2 To, s32 ToLevel,
               nav_path_point *Points, u32 MaxPoints)
{
    u32 Result = 0;
    s32 FromPoly = FindMapNavPoly(Mesh, From, FromLevel);
    s32 ToPoly = FindMapNavPoly(Mesh, To, ToLevel);
    if((FromPoly >= 0) && (ToPoly >= 0) && SearchMapNav(Mesh, Scratch->Nodes, From, FromPoly, To, ToPoly))
    {
        // NOTE(pvlso): The links on the way, counted back from the goal, then laid out from the start
        u32 LinkCount = 0;
        for(s32 Poly = ToPoly; Poly != FromPoly; Poly = Scratch->Nodes[Poly].Parent) {++LinkCount;}

        u32 PortalCount = LinkCount + 2;
        v2 *Portals = Scratch->Portals;
        s32 *Levels = Scratch->Levels;
        u32 At = LinkCount;
        for(s32 Poly = ToPoly; Poly != FromPoly; Poly = Scratch->Nodes[Poly].Parent)
        {
            kea_nav_link *Link = Mesh->Links + Scratch->Nodes[Poly].Link;
            // NOTE(pvlso): A to B winds around the polygon being left, which puts B on the left going out of it
            Portals[At*2] = V2(Link->B[0], Link->B[1]);
            Portals[At*2 + 1] = V2(Link->A[0], Link->A[1]);
            Levels[At] = Mesh->Polys[Poly].Level;
            --At;
        }
        Portals[0] = Portals[1] = From;
        Levels[0] = Mesh->Polys[FromPoly].Level;
        Portals[(PortalCount - 1)*2] = Portals[(PortalCount - 1)*2 + 1] = To;
        Levels[PortalCount - 1] = Mesh->Polys[ToPoly].Level;

        u32 CornerCount = PullMapNavPath(Portals, PortalCount, Scratch->Corners, Scratch->CornerPortals,
                                         Mesh->PolyCount + 2);
        for(u32 Corner = 0; (Corner < CornerCount) && (Result < MaxPoints); ++Corner)
        {
            s32 Portal = Scratch->CornerPortals[Corner];
            v2 A = Scratch->Corners[Corner];
            Points[Result].P = A;
            Points[Result].Level = Levels[Portal];
            ++Result;

            if((Corner + 1) < CornerCount)
            {
                // NOTE(pvlso): Levels change on a straight stretch too, where it crosses their portal
                v2 B = Scratch->Corners[Corner + 1];
                s32 NextPortal = Scratch->CornerPortals[Corner + 1];
                for(s32 Crossed = Portal + 1; (Crossed < NextPortal) && (Result < MaxPoints); ++Crossed)
                {
                    if(Levels[Crossed] != Levels[Crossed - 1])
                    {
                        v2 Left = Portals[Crossed*2];
                        v2 Along = Portals[Crossed*2 + 1] - Left;
                        r32 Across = Cross(B - A, Along);
                        r32 T = (AbsoluteValue(Across) > 0.000001f) ? Clamp01(Cross(Left - A, Along) / Across) : 0.0f;
                        Points[Result].P = A + T*(B - A);
                        Points[Result].Level = Levels[Crossed];
                        ++Result;
                    }
                }
            }
        }
    }

    return(Result);
}

// NOTE(pvlso): The point of the mesh closest to P, a little inside its polygon, and that polygon's level in Level. Where
// P is on the mesh it stays, on Level if there's a polygon of that level there. False on an empty mesh.
internal b32
ClosestMapNavPoint(map_nav_mesh *Mesh, v2 *P, s32 *Level)
{
    b32 Result = false;
    s32 Poly = FindMapNavPoly(Mesh, *P, *Level);
    if(Poly >= 0)
    {
        *Level = Mesh->Polys[Poly].Level;
        Result = true;
    }
    else
    {
        v2 Closest = {};
        r32 ClosestSq = 0.0f;
        for(u32 PolyIndex = 0; PolyIndex < Mesh->PolyCount; ++PolyIndex)
        {
            kea_nav_poly *Candidate = Mesh->Polys + PolyIndex;
            for(u32 Index = 0; Index < Candidate->PointCount; ++Index)
            {
                v2 A = Mesh->Points[Candidate->FirstPoint + Index];
                v2 B = Mesh->Points[Candidate->FirstPoint + (Index + 1) % Candidate->PointCount];
                v2 Along = B - A;
                r32 AlongSq = Inner(Along, Along);
                r32 T = (AlongSq > 0.0f) ? Clamp01(Inner(*P - A, Along) / AlongSq) : 0.0f;
                v2 OnEdge = A + T*Along;
                r32 DistanceSq = LengthSq(*P - OnEdge);
                if((Poly < 0) || (DistanceSq < ClosestSq))
                {
                    ClosestSq = DistanceSq;
                    Closest = OnEdge;
                    Poly = (s32)PolyIndex;
                }
            }
        }

        if(Poly >= 0)
        {
            kea_nav_poly *Found = Mesh->Polys + Poly;
            v2 Center = {};
            for(u32 Index = 0; Index < Found->PointCount; ++Index)
            {
                Center += Mesh->Points[Found->FirstPoint + Index];
            }
            Center *= 1.0f / (r32)Found->PointCount;
            *P = Closest + 2.0f*MAP_NAV_EPSILON*Normalize(Center - Closest);
            *Level = Found->Level;
            Result = true;
        }
    }

    return(Result);
}

// NOTE(pvlso): Walks the straight way from From to To over the mesh: across links, where Level follows the polygons it
// comes into, and sliding along the walls it meets. Gives where it got to. A From off the mesh goes to the closest
// point first.
internal v2
MoveMapNav(map_nav_mesh *Mesh, v2 From, s32 *Level, v2 To)
{
    v2 At = From;
    s32 Poly = FindMapNavPoly(Mesh, At, *Level);
    if((Poly < 0) && ClosestMapNavPoint(Mesh, &At, Level))
    {
        From = At;
        Poly = FindMapNavPoly(Mesh, At, *Level);
    }

    s32 EndPoly = Poly;
    v2 Goal = To;
    // NOTE(pvlso): Each turn crosses a polygon or meets a wall; a step is short, so this is plenty
    for(u32 Turn = 0; (Poly >= 0) && (Turn < 32); ++Turn)
    {
        kea_nav_poly *Current = Mesh->Polys + Poly;
        EndPoly = Poly;

        // NOTE(pvlso): The edge the way leaves this convex polygon over: the first one it reaches
        r32 ExitT = 2.0f;
        v2 ExitA = {};
        v2 ExitB = {};
        for(u32 Index = 0; Index < Current->PointCount; ++Index)
        {
            v2 A = Mesh->Points[Current->FirstPoint + Index];
            v2 B = Mesh->Points[Current->FirstPoint + (Index + 1) % Current->PointCount];
            r32 Past = Cross(B - A, Goal - A);
            if(Past < -MAP_NAV_EPSILON*MAP_NAV_EPSILON)
            {
                r32 Before = Maximum(Cross(B - A, At - A), 0.0f);
                r32 T = Before / (Before - Past);
                if(T < ExitT)
                {
                    ExitT = T;
                    ExitA = A;
                    ExitB = B;
                }
            }
        }

        if(ExitT > 1.0f)
        {
            At = Goal;
            break;
        }

        v2 Hit = At + ExitT*(Goal - At);
        v2 EdgeDir = Normalize(ExitB - ExitA);
        s32 Next = -1;
        for(u32 Index = 0; Index < Current->LinkCount; ++Index)
        {
            kea_nav_link *Link = Mesh->Links + Current->FirstLink + Index;
            v2 A = V2(Link->A[0], Link->A[1]);
            v2 B = V2(Link->B[0], Link->B[1]);
            v2 Along = B - A;
            r32 AlongSq = Inner(Along, Along);
            r32 T = (AlongSq > 0.0f) ? Clamp01(Inner(Hit - A, Along) / AlongSq) : 0.0f;
            v2 Nearest = A + T*Along;
            // NOTE(pvlso): On this edge, not on another that meets it at the same corner
            b32 OnEdge = (AbsoluteValue(Cross(EdgeDir, A - ExitA)) <= MAP_NAV_EPSILON) &&
                         (AbsoluteValue(Cross(EdgeDir, B - ExitA)) <= MAP_NAV_EPSILON);
            if(OnEdge && (LengthSq(Hit - Nearest) <= MAP_NAV_EPSILON*MAP_NAV_EPSILON))
            {
                Next = (s32)Link->Poly;
                break;
            }
        }

        At = Hit;
        if(Next >= 0)
        {
            Poly = Next;
        }
        else
        {
            // NOTE(pvlso): A wall: what's left of the way goes along it, kept a little inside so the next step starts
            // on the mesh
            v2 Inward = V2(-EdgeDir.y, EdgeDir.x);
            Goal = At + Inner(Goal - At, EdgeDir)*EdgeDir + MAP_NAV_EPSILON*Inward;
            At += MAP_NAV_EPSILON*Inward;
        }
    }

    if(EndPoly >= 0)
    {
        *Level = Mesh->Polys[EndPoly].Level;
    }
    // NOTE(pvlso): A tight corner can leave it a hair outside by rounding; then it doesn't move this step
    if(FindMapNavPoly(Mesh, At, *Level) < 0)
    {
        At = From;
    }

    return(At);
}

#if EDITOR_SLOW
// NOTE(pvlso): Run once at startup in debug builds. A room of two areas sharing an edge with a pillar across it on level
// 0, stairs up to level 1 past part of its east wall, and a platform on level 2 that only overlaps the room.
internal void
TestMapNav(memory_arena *Arena)
{
    temporary_memory Temp = BeginTemporaryMemory(Arena);
    v2 RoomNorth[] = {V2(0, 0), V2(10, 0), V2(10, 5), V2(0, 5)};
    v2 RoomSouth[] = {V2(0, 5), V2(10, 5), V2(10, 10), V2(0, 10)};
    v2 Pillar[] = {V2(4, 4), V2(4, 6), V2(6, 6), V2(6, 4)};
    v2 Stairs[] = {V2(10, 4), V2(14, 4), V2(14, 6), V2(10, 6)};
    v2 Ledge[] = {V2(1, 1), V2(3, 1), V2(3, 3), V2(1, 3)};
    map_file_nav Navs[5] = {};
    Navs[0].PointCount = ArrayCount(RoomNorth);
    Navs[0].Points = RoomNorth;
    Navs[1].PointCount = ArrayCount(RoomSouth);
    Navs[1].Points = RoomSouth;
    Navs[2].Hole = true;
    Navs[2].PointCount = ArrayCount(Pillar);
    Navs[2].Points = Pillar;
    Navs[3].Level = 1;
    Navs[3].PointCount = ArrayCount(Stairs);
    Navs[3].Points = Stairs;
    Navs[4].Level = 2;
    Navs[4].PointCount = ArrayCount(Ledge);
    Navs[4].Points = Ledge;

    map_nav_mesh Mesh = BakeMapNav(Arena, ArrayCount(Navs), Navs);
    map_nav_scratch Scratch = MapNavScratchOf(PushSize(Arena, MapNavScratchSize(Mesh.PolyCount)), Mesh.PolyCount);
    nav_path_point Path[32];
    nav_path_point Other[8];
    u32 Across = FindMapNavPath(&Mesh, &Scratch, V2(1, 5), 0, V2(13, 5), 1, Path, ArrayCount(Path));
    u32 Apart = FindMapNavPath(&Mesh, &Scratch, V2(5, 1), 0, V2(2, 2), 2, Other, ArrayCount(Other));

    // NOTE(pvlso): A step up the stairs, one into the pillar that slides along it, one out of the room that stops at
    // its wall, one that stays under the platform, and a point off the mesh
    s32 StairsLevel = 0;
    v2 UpStairs = MoveMapNav(&Mesh, V2(9.5f, 5.0f), &StairsLevel, V2(11, 5));
    s32 PillarLevel = 0;
    v2 AlongPillar = MoveMapNav(&Mesh, V2(3, 5), &PillarLevel, V2(4.5f, 5.5f));
    s32 WallLevel = 0;
    v2 AtWall = MoveMapNav(&Mesh, V2(1, 8), &WallLevel, V2(-3, 8));
    s32 UnderLevel = 0;
    v2 Under = MoveMapNav(&Mesh, V2(0.5f, 2.0f), &UnderLevel, V2(2, 2));
    s32 ClosestLevel = 0;
    v2 Closest = V2(16, 5);
    ClosestMapNavPoint(&Mesh, &Closest, &ClosestLevel);
    b32 Moved = ((StairsLevel == 1) && PointsAreEqual(UpStairs, V2(11, 5), 0.001f) && (PillarLevel == 0) &&
                 (AbsoluteValue(AlongPillar.x - 4.0f) < 0.05f) && (AlongPillar.y > 5.1f) && (WallLevel == 0) &&
                 (AbsoluteValue(AtWall.x) < 0.05f) && (UnderLevel == 0) && PointsAreEqual(Under, V2(2, 2), 0.001f) &&
                 (ClosestLevel == 1) && (AbsoluteValue(Closest.x - 14.0f) < 0.05f));

    // NOTE(pvlso): Around the pillar over its corners, then a point on the stairs' edge where the level changes
    b32 AroundPillar = false;
    b32 ChangesAtEdge = false;
    for(u32 Index = 0; Index < Across; ++Index)
    {
        if((Path[Index].P.x == 4.0f) || (Path[Index].P.x == 6.0f)) {AroundPillar = true;}
        if((AbsoluteValue(Path[Index].P.x - 10.0f) < 0.01f) && (Path[Index].Level == 1)) {ChangesAtEdge = true;}
    }
    b32 Passed = ((Across == 5) && AroundPillar && ChangesAtEdge && (Apart == 0) && (Path[0].Level == 0) &&
                  (Path[Across - 1].Level == 1) && PointsAreEqual(Path[Across - 1].P, V2(13, 5), 0.001f) && Moved);
    if(Passed)
    {
        LOG("Navigation self-test passed: %u polygons, %u links, a path of %u points", Mesh.PolyCount, Mesh.LinkCount,
            Across);
    }
    else
    {
        LOG_ERROR("Navigation self-test failed: %u polygons, %u links, %u points across, %u apart", Mesh.PolyCount,
                  Mesh.LinkCount, Across, Apart);
        LOG_ERROR("  stairs %.3f,%.3f on %d, pillar %.3f,%.3f, wall %.3f,%.3f, under %.3f,%.3f on %d, closest %.3f,%.3f on %d",
                  UpStairs.x, UpStairs.y, StairsLevel, AlongPillar.x, AlongPillar.y, AtWall.x, AtWall.y, Under.x, Under.y,
                  UnderLevel, Closest.x, Closest.y, ClosestLevel);
        for(u32 Index = 0; Index < Across; ++Index)
        {
            LOG_ERROR("  %.3f %.3f level %d", Path[Index].P.x, Path[Index].P.y, Path[Index].Level);
        }
    }
    EndTemporaryMemory(Temp);
}
#endif

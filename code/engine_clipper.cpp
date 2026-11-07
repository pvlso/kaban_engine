/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The user's Clipper-2D (data/Clipper-2d-main/code/clipper, a rewrite of part of Angus Johnson's
  Clipper2: boolean operations on polygons), brought into the unity build: its types that the engine also has are
  renamed (rect_f64, rect_s64, clipper_heap, clipper_sort_entry, clipper_active, clipper_vertex), its memory comes
  from ClipperArena, its copies of the engine's Copy and ZeroSize are gone. ClipPolygons (engine_clipper.cpp) is
  what the engine uses.
*/

/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline u32
GetPathsVertexCount(paths_s64 *Paths)
{
    u32 Result = 0;
    for(s32 I = 0;
        I < Paths->PathCount;
        ++I)
    {
        Result += Paths->Paths[I].Count;
    }

    return(Result);
}

inline void
InitClipper(clipper *Clipper, s32 Precision)
{
    Clipper->Scale = pow((f64)FLT_RADIX, ilogb(pow(10, Precision)) + 1);
    Clipper->InvScale = 1.0 / Clipper->Scale;

    InitHeap(&Clipper->ScanLineMaxHeap, BASIC_ALLOCATE_COUNT, HeapType_S64);
    Clipper->OutputRectCount = 1;
    Clipper->OutRecList = MallocArray(BASIC_ALLOCATE_COUNT, output_rectangle);

    Clipper->MinimaList = MallocArray(BASIC_ALLOCATE_COUNT, local_minima);

    Clipper->HorzSegList = MallocArray(BASIC_ALLOCATE_COUNT, horz_segment);
    Clipper->HorzJoinList = MallocArray(BASIC_ALLOCATE_COUNT, horz_join);
    Clipper->IntersectNodes = MallocArray(BASIC_ALLOCATE_COUNT, intersect_node);

    Clipper->VertexLists = MallocArray(BASIC_ALLOCATE_COUNT, vertex_list);
    Clipper->DeallocActivesList = MallocArray(BASIC_ALLOCATE_COUNT, void *);
    Clipper->DeallocOutPtsList = MallocArray(BASIC_ALLOCATE_COUNT, void *);
    
#if RECORD_MEMORY_USEAGE
    for(u32 I = 0;
        I < ArrayType_Count;
        ++I)
    {
        ArrayMaxSizes[I] = BASIC_ALLOCATE_COUNT;
    }
#endif
}

internal void
AddLocMin(clipper *Clipper, clipper_vertex *vert, path_type Type, b32 IsOpen)
{
    //make sure the clipper_vertex is added only once ...
    if(!IsVertexFlagSet(vert->Flags, VertexFlag_LocalMin))
    {
        AddVertexFlag(&vert->Flags, VertexFlag_LocalMin);
        if(NeedIncrease(Clipper->MinimaListCount))
        {
            IncreaseMinimaList(Clipper);
        }

        InitLocalMinima(Clipper->MinimaList + Clipper->MinimaListCount, vert, Type, IsOpen);
        Clipper->MinimaListCount++;
    }
}

internal void
AddPathsInternal(clipper *Clipper, paths_s64 *Paths, path_type Type, b32 IsOpen)
{
    u32 TotalVertexCout = GetPathsVertexCount(Paths);
    
    if(TotalVertexCout == 0)
        return;

    if(NeedIncrease(Clipper->VertexListCount))
    {
        IncreaseVertexLists(Clipper);
    }

    Clipper->VertexLists[Clipper->VertexListCount].Vertices = MallocArray(TotalVertexCout, clipper_vertex);
    Clipper->VertexLists[Clipper->VertexListCount].VertexCount = TotalVertexCout;
    ++Clipper->VertexListCount;


    clipper_vertex *Vertices = Clipper->VertexLists[Clipper->VertexListCount - 1].Vertices;
    clipper_vertex *VIter = Vertices;

    for(s32 PathIndex = 0;
        PathIndex < Paths->PathCount;
        ++PathIndex)
    {
        path_s64 *Path = Paths->Paths + PathIndex;
        clipper_vertex *VIter0 = VIter;
        clipper_vertex *curr_v = VIter;
        clipper_vertex *prev_v = 0;

        if(!Path)
            continue;

        VIter->Prev = 0;
        s32 Count = 0;

        for(s32 PointIndex = 0;
            PointIndex < Path->Count;
            ++PointIndex)
        {
            v2_s64 Point = Path->Points[PointIndex];
            if(prev_v)
            {
                if(PointsAreEqual(prev_v->P, Point))
                    continue; // ie skips duplicates
                prev_v->Next = curr_v;
            }

            curr_v->Prev = prev_v;
            curr_v->P = Point;
            curr_v->Flags = 0;
            prev_v = curr_v++;
            Count++;
        }

        if(!prev_v || !prev_v->Prev)
            continue;

        if(!IsOpen && PointsAreEqual(prev_v->P, VIter0->P))
            prev_v = prev_v->Prev;

        prev_v->Next = VIter0;
        VIter0->Prev = prev_v;
        VIter = curr_v; // ie get ready for next path

        if((Count < 2) || ((Count == 2) && !IsOpen))
            continue;
        
        //now find and assign local minima
        b32 going_up = false;
        b32 going_up0 = false;
        if(IsOpen)
        {
            curr_v = VIter0->Next;
            while((curr_v != VIter0) && (curr_v->P.y == VIter0->P.y))
                curr_v = curr_v->Next;

            going_up = (curr_v->P.y <= VIter0->P.y);
            if(going_up)
            {
                VIter0->Flags = VertexFlag_OpenStart;
                AddLocMin(Clipper, VIter, Type, true);
            }
            else
            {
                VIter0->Flags = VertexFlag_OpenStart | VertexFlag_LocalMax;
            }
        }
        else // closed path
        {
            prev_v = VIter0->Prev;
            while((prev_v != VIter0) && (prev_v->P.y == VIter0->P.y))
                prev_v = prev_v->Prev;

            if(prev_v == VIter0)
                continue; // only open paths can be completely flat

            going_up = (prev_v->P.y > VIter0->P.y);
        }

        going_up0 = going_up;
        prev_v = VIter0;
        curr_v = VIter0->Next;
        while (curr_v != VIter0)
        {
            if((curr_v->P.y > prev_v->P.y) && going_up)
            {
                AddVertexFlag(&prev_v->Flags, VertexFlag_LocalMax);
                going_up = false;
            }
            else if((curr_v->P.y < prev_v->P.y) && !going_up)
            {
                going_up = true;
                AddLocMin(Clipper, prev_v, Type, IsOpen);
            }

            prev_v = curr_v;
            curr_v = curr_v->Next;
        }

        if(IsOpen)
        {
            AddVertexFlag(&prev_v->Flags, VertexFlag_OpenEnd);
            if(going_up)
                AddVertexFlag(&prev_v->Flags, VertexFlag_LocalMax);
            else
                AddLocMin(Clipper, prev_v, Type, IsOpen);
        }
        else if(going_up != going_up0)
        {
            if (going_up0)
                AddLocMin(Clipper, prev_v, Type, false);
            else
                AddVertexFlag(&prev_v->Flags, VertexFlag_LocalMax);
        }
    }
}

inline void
AddPaths(clipper *Clipper, paths_s64 *Paths, path_type Type, b32 IsOpen)
{
    if(IsOpen)
        Clipper->HasOpenPaths = true;

    Clipper->MinimaListSorted = false;
    AddPathsInternal(Clipper, Paths, Type, IsOpen);
}

inline void
AddSubjects(clipper *Clipper, paths_f64 *Subjects)
{
    paths_s64 Paths = ScaleConvertPathsToS64(Subjects, Clipper->Scale);
    AddPaths(Clipper, &Paths, PathType_Subject, false);
    FreePaths(&Paths);
}

inline void
AddOpenSubjects(clipper *Clipper, paths_f64 *Subjects)
{
    paths_s64 Paths = ScaleConvertPathsToS64(Subjects, Clipper->Scale);
    AddPaths(Clipper, &Paths, PathType_Subject, true);
    FreePaths(&Paths);
}

inline void
AddClips(clipper *Clipper, paths_f64 *Clips)
{
    paths_s64 Paths = ScaleConvertPathsToS64(Clips, Clipper->Scale);
    AddPaths(Clipper, &Paths, PathType_Clip, false);
    FreePaths(&Paths);
}

inline void
ResetClipper(clipper *Clipper)
{
    
    if (!Clipper->MinimaListSorted)
    {
        if(Clipper->MinimaListCount < 32)
        {
//            TimeBlock("ISort Reset");
            InsertionSort(Clipper->MinimaListCount, Clipper->MinimaList);
        }
        else
        {
//            TimeBlock("MSort Reset");
            MergeSort(Clipper->MinimaListCount, Clipper->MinimaList); //#594
        }

        Clipper->MinimaListSorted = true;
    }

    for(u32 I = 0;
        I < Clipper->MinimaListCount;
        ++I)
    {
        local_minima *Minima = Clipper->MinimaList + I;
        clipper_sort_entry Entry = {};
        Entry.Value_S64 = Minima->Vertex->P.y;
        MaxHeapInsertNode(&Clipper->ScanLineMaxHeap, Entry);
    }

    Clipper->CurrentLocalMinima = Clipper->MinimaList + 0;
    Clipper->ActiveEdgeList = 0;
    Clipper->StoredEdgeList = 0;
    Clipper->Succeeded = true;
}

inline b32
PopScanline(clipper *Clipper, s64 *y)
{
    if(Clipper->ScanLineMaxHeap.Size == 0)
        return false;

    clipper_sort_entry Entry = MaxHeapExtractNode(&Clipper->ScanLineMaxHeap);
    *y = Entry.Value_S64;

    while ((Clipper->ScanLineMaxHeap.Size > 0) && *y == Clipper->ScanLineMaxHeap.Nodes[0].Value_S64)
        MaxHeapExtractNode(&Clipper->ScanLineMaxHeap); // Pop duplicates.

    return true;
}

inline b32
PopLocalMinima(clipper *Clipper, s64 y, local_minima **Minima)
{
    if((Clipper->CurrentLocalMinima == (Clipper->MinimaList + Clipper->MinimaListCount)) ||
       (Clipper->CurrentLocalMinima->Vertex->P.y != y))
        return false;

    *Minima = Clipper->CurrentLocalMinima;
    Clipper->CurrentLocalMinima += 1;
    return true;
}

inline f64
GetDx(v2_s64 pt1, v2_s64 pt2)
{
    f64 Result = DBL_MAX;

    f64 dy = (f64)(pt2.y - pt1.y);
    if (dy != 0)
    {
        Result = (f64)(pt2.x - pt1.x) / dy;
    }
    else if (pt2.x > pt1.x)
    {
        Result = -Result;
    }

    return(Result);
}

inline void
SetDx(clipper_active *e)
{
//    TimeFunction;
    
    e->dx = GetDx(e->bot, e->top);
}

inline b32
IsHorizontal(clipper_active *e)
{
    b32 Result = (e->top.y == e->bot.y);
    return(Result);
}

inline b32
IsHeadingRightHorz(clipper_active *e)
{
    b32 Result = (e->dx == -DBL_MAX);
    return(Result);
}


inline b32
IsHeadingLeftHorz(clipper_active *e)
{
    b32 Result = (e->dx == DBL_MAX);
    return(Result);
}


inline void
SwapActives(clipper_active **e1, clipper_active **e2)
{
    clipper_active *e = *e1;
    *e1 = *e2;
    *e2 = e;
}

inline clipper_vertex *
NextVertex(clipper_active *e)
{
    if (e->wind_dx > 0)
        return e->vertex_top->Next;
    else
        return e->vertex_top->Prev;
}

//PrevPrevVertex: useful to get the (inverted Y-axis) top of the
//alternate edge (ie left or right bound) during edge insertion.
inline clipper_vertex *
PrevPrevVertex(clipper_active *ae)
{
    if (ae->wind_dx > 0)
        return ae->vertex_top->Prev->Prev;
    else
        return ae->vertex_top->Next->Next;
}

internal b32
IsValidAelOrder(clipper_active *resident, clipper_active *newcomer)
{
    b32 Result = false;
    if (newcomer->curr_x != resident->curr_x)
        return newcomer->curr_x > resident->curr_x;

    //get the turning direction  a1.top, a2.bot, a2.top
    f64 d = Cross(resident->top, newcomer->bot, newcomer->top);
    if (d != 0)
        return d < 0;

    //edges must be collinear to get here
    //for starting open paths, place them according to
    //the direction they're about to turn
    if (!IsVertexFlagSet(resident->vertex_top->Flags, VertexFlag_LocalMax) && (resident->top.y > newcomer->top.y))
    {
        return Cross(newcomer->bot, resident->top, NextVertex(resident)->P) <= 0;
    }
    else if (!IsVertexFlagSet(newcomer->vertex_top->Flags, VertexFlag_LocalMax) && (newcomer->top.y > resident->top.y))
    {
        return Cross(newcomer->bot, newcomer->top, NextVertex(newcomer)->P) >= 0;
    }

    s64 y = newcomer->bot.y;
    b32 newcomerIsLeft = newcomer->IsLeftBound;

    if ((resident->bot.y != y) || (resident->local_min->Vertex->P.y != y))
        return newcomer->IsLeftBound;
    //resident must also have just been inserted
    else if (resident->IsLeftBound != newcomerIsLeft)
        return newcomerIsLeft;
    else if (IsCollinear(PrevPrevVertex(resident)->P, resident->bot, resident->top))
        return true;
    else
        //compare turning direction of the alternate bound
        return (Cross(PrevPrevVertex(resident)->P, newcomer->bot, PrevPrevVertex(newcomer)->P) > 0) == newcomerIsLeft;
}

inline void
InsertLeftEdge(clipper *Clipper, clipper_active *e)
{
    clipper_active *e2;
    if(!Clipper->ActiveEdgeList)
    {
        e->prev_in_ael = 0;
        e->next_in_ael = 0;
        Clipper->ActiveEdgeList = e;
    }
    else if(!IsValidAelOrder(Clipper->ActiveEdgeList, e))
    {
        e->prev_in_ael = 0;
        e->next_in_ael = Clipper->ActiveEdgeList;
        Clipper->ActiveEdgeList->prev_in_ael = e;
        Clipper->ActiveEdgeList = e;
    }
    else
    {
        e2 = Clipper->ActiveEdgeList;
        while(e2->next_in_ael && IsValidAelOrder(e2->next_in_ael, e))
            e2 = e2->next_in_ael;

        if(e2->JoinWith == JoinWith_Right)
            e2 = e2->next_in_ael;

        if(!e2)
            return; // should never happen and stops compiler warning :)
        e->next_in_ael = e2->next_in_ael;

        if(e2->next_in_ael)
            e2->next_in_ael->prev_in_ael = e;

        e->prev_in_ael = e2;
        e2->next_in_ael = e;
    }
}

inline b32
IsOpen(clipper_active *e)
{
    return(e->local_min->IsOpen);
}

inline path_type
GetPolyType(clipper_active *e)
{
    return e->local_min->PolyType;
}

internal void
SetWindCountForOpenPathEdge(clipper *Clipper, clipper_active *e)
{
    clipper_active *e2 = Clipper->ActiveEdgeList;
    if (Clipper->FillRule == FillRule_EvenOdd)
    {
        s32 cnt1 = 0, cnt2 = 0;
        while(e2 != e)
        {
            if(GetPolyType(e2) == PathType_Clip)
                cnt2++;
            else if(!IsOpen(e2))
                cnt1++;
            e2 = e2->next_in_ael;
        }
        e->wind_cnt = ((cnt1 & 1) ? 1 : 0);
        e->wind_cnt2 = ((cnt2 & 1) ? 1 : 0);
    }
    else
    {
        while(e2 != e)
        {
            if (GetPolyType(e2) == PathType_Clip)
                e->wind_cnt2 += e2->wind_dx;
            else if (!IsOpen(e2))
                e->wind_cnt += e2->wind_dx;
            e2 = e2->next_in_ael;
        }
    }
}

inline b32
IsContributingOpen(clipper *Clipper, clipper_active *e)
{
    b32 is_in_clip, is_in_subj;
    switch(Clipper->FillRule)
    {
        case FillRule_Positive:
            is_in_clip = e->wind_cnt2 > 0;
            is_in_subj = e->wind_cnt > 0;
            break;
        case FillRule_Negative:
            is_in_clip = e->wind_cnt2 < 0;
            is_in_subj = e->wind_cnt < 0;
            break;
        default:
            is_in_clip = e->wind_cnt2 != 0;
            is_in_subj = e->wind_cnt != 0;
    }

    switch(Clipper->ClipType)
    {
        case ClipType_Intersection:
            return is_in_clip;
        case ClipType_Union:
            return (!is_in_subj && !is_in_clip);
        default:
            return !is_in_clip;
    }
}

internal void
SetWindCountForClosedPathEdge(clipper *Clipper, clipper_active *e)
{
    //Wind counts refer to polygon regions not edges, so here an edge's WindCnt
    //indicates the higher of the wind counts for the two regions touching the
    //edge. (NB Adjacent regions can only ever have their wind counts differ by
    //one. Also, open paths have no meaningful wind directions or counts.)

    clipper_active *e2 = e->prev_in_ael;
    //find the nearest closed path edge of the same PolyType in AEL (heading left)
    path_type PolyType = GetPolyType(e);
    while(e2 && (GetPolyType(e2) != PolyType || IsOpen(e2)))
        e2 = e2->prev_in_ael;

    if (!e2)
    {
        e->wind_cnt = e->wind_dx;
        e2 = Clipper->ActiveEdgeList;
    }
    else if(Clipper->FillRule == FillRule_EvenOdd)
    {
        e->wind_cnt = e->wind_dx;
        e->wind_cnt2 = e2->wind_cnt2;
        e2 = e2->next_in_ael;
    }
    else
    {
        //NonZero, positive, or negative filling here ...
        //if e's WindCnt is in the SAME direction as its WindDx, then polygon
        //filling will be on the right of 'e'.
        //NB neither e2.WindCnt nor e2.WindDx should ever be 0.
        if (e2->wind_cnt * e2->wind_dx < 0)
        {
            //opposite directions so 'e' is outside 'e2' ...
            if (abs(e2->wind_cnt) > 1)
            {
                //outside prev poly but still inside another.
                if(e2->wind_dx * e->wind_dx < 0)
                    //reversing direction so use the same WC
                    e->wind_cnt = e2->wind_cnt;
                else
                    //otherwise keep 'reducing' the WC by 1 (ie towards 0) ...
                    e->wind_cnt = e2->wind_cnt + e->wind_dx;
            }
            else
                //now outside all polys of same polytype so set own WC ...
                e->wind_cnt = (IsOpen(e) ? 1 : e->wind_dx);
        }
        else
        {
            //'e' must be inside 'e2'
            if(e2->wind_dx * e->wind_dx < 0)
                //reversing direction so use the same WC
                e->wind_cnt = e2->wind_cnt;
            else
                //otherwise keep 'increasing' the WC by 1 (ie away from 0) ...
                e->wind_cnt = e2->wind_cnt + e->wind_dx;
        }

        e->wind_cnt2 = e2->wind_cnt2;
        e2 = e2->next_in_ael;  // ie get ready to calc WindCnt2
    }

    //update wind_cnt2 ...
    if(Clipper->FillRule == FillRule_EvenOdd)
        while(e2 != e)
        {
            if(GetPolyType(e2) != PolyType && !IsOpen(e2))
                e->wind_cnt2 = (e->wind_cnt2 == 0 ? 1 : 0);
            e2 = e2->next_in_ael;
        }
    else
        while(e2 != e)
        {
            if (GetPolyType(e2) != PolyType && !IsOpen(e2))
                e->wind_cnt2 += e2->wind_dx;
            e2 = e2->next_in_ael;
        }
}


internal b32
IsContributingClosed(clipper *Clipper, clipper_active *e)
{
     

    switch(Clipper->FillRule)
    {
        case FillRule_EvenOdd:
            break;
        case FillRule_NonZero:
            if (abs(e->wind_cnt) != 1)
                return false;
            break;
        case FillRule_Positive:
            if (e->wind_cnt != 1)
                return false;
            break;
        case FillRule_Negative:
            if (e->wind_cnt != -1)
                return false;
            break;
    }

    switch(Clipper->ClipType)
    {
        case ClipType_NoClip:
            return false;
        case ClipType_Intersection:
            switch(Clipper->FillRule)
            {
                case FillRule_Positive:
                    return (e->wind_cnt2 > 0);
                case FillRule_Negative:
                    return (e->wind_cnt2 < 0);
                default:
                    return (e->wind_cnt2 != 0);
            }
            break;

        case ClipType_Union:
            switch (Clipper->FillRule)
            {
                case FillRule_Positive:
                    return (e->wind_cnt2 <= 0);
                case FillRule_Negative:
                    return (e->wind_cnt2 >= 0);
                default:
                    return (e->wind_cnt2 == 0);
            }
            break;

        case ClipType_Difference:
            b32 result;
            switch (Clipper->FillRule)
            {
                case FillRule_Positive:
                    result = (e->wind_cnt2 <= 0);
                    break;
                case FillRule_Negative:
                    result = (e->wind_cnt2 >= 0);
                    break;
                default:
                    result = (e->wind_cnt2 == 0);
            }

            if (GetPolyType(e) == PathType_Subject)
                return result;
            else
                return !result;
            break;

        case ClipType_Xor:
            return true;
            break;
    }

    Assert(!"we should never get here");
    return false;  // we should never get here
}

internal void
InsertRightEdge(clipper_active *e, clipper_active *e2)
{
    e2->next_in_ael = e->next_in_ael;
    if(e->next_in_ael)
        e->next_in_ael->prev_in_ael = e2;
    e2->prev_in_ael = e;
    e->next_in_ael = e2;
}

inline void
SetSides(output_rectangle *OutRec, clipper_active *start_edge, clipper_active *end_edge)
{
    OutRec->FrontEdge = start_edge;
    OutRec->BackEdge = end_edge;
}

inline bool
IsHotEdge(clipper_active *e)
{
    return(e->outrecIndex != 0);
}

inline clipper_active *
GetPrevHotEdge(clipper_active *e)
{
    clipper_active *prev = e->prev_in_ael;
    while (prev && (IsOpen(prev) || !IsHotEdge(prev)))
        prev = prev->prev_in_ael;
    return prev;
}

inline b32
OutrecIsAscending(clipper *Clipper, clipper_active *hotEdge)
{
    output_rectangle *outrec = GetOutRec(Clipper, hotEdge->outrecIndex);
    return (hotEdge == outrec->FrontEdge);
}

internal output_point *
AddLocalMinPoly(clipper *Clipper, clipper_active *e1, clipper_active *e2, v2_s64 pt, b32 is_new = false)
{
    u32 outrecIndex = NewOutRec(Clipper);
    output_rectangle *outrec = GetOutRec(Clipper, outrecIndex);
    e1->outrecIndex = outrecIndex;
    e2->outrecIndex = outrecIndex;

    if (IsOpen(e1))
    {
        outrec->OwnerIndex = 0;
        outrec->IsOpen = true;
        if(e1->wind_dx > 0)
            SetSides(outrec, e1, e2);
        else
            SetSides(outrec, e2, e1);
    }
    else
    {
        clipper_active *prevHotEdge = GetPrevHotEdge(e1);
        //e.windDx is the winding direction of the **input** paths
        //and unrelated to the winding direction of output polygons.
        //Output orientation is determined by e.outrec.frontE which is
        //the ascending edge (see AddLocalMinPoly).
        if(prevHotEdge)
        {
            if (OutrecIsAscending(Clipper, prevHotEdge) == is_new)
                SetSides(outrec, e2, e1);
            else
                SetSides(outrec, e1, e2);
        }
        else
        {
            outrec->OwnerIndex = 0;
            if (is_new)
                SetSides(outrec, e1, e2);
            else
                SetSides(outrec, e2, e1);
        }
    }

    output_point *op = GetOutPt(Clipper, pt, outrecIndex);
    outrec->Points = op;
    return op;
}

inline b32
IsJoined(clipper_active *e)
{
    return e->JoinWith != JoinWith_NoJoin;
}

internal void
Split(clipper *Clipper, clipper_active *e, v2_s64 pt)
{
    if (e->JoinWith == JoinWith_Right)
    {
        e->JoinWith = JoinWith_NoJoin;
        e->next_in_ael->JoinWith = JoinWith_NoJoin;
        AddLocalMinPoly(Clipper, e, e->next_in_ael, pt, true);
    }
    else
    {
        e->JoinWith = JoinWith_NoJoin;
        e->prev_in_ael->JoinWith = JoinWith_NoJoin;
        AddLocalMinPoly(Clipper, e->prev_in_ael, e, pt, true);
    }
}

inline b32
IsFront(clipper *Clipper, clipper_active *e)
{
    output_rectangle *outrec = GetOutRec(Clipper, e->outrecIndex);
    return (e == outrec->FrontEdge);
}

inline bool
IsOpenEnd(clipper_vertex *v)
{
    b32 Result = (IsVertexFlagSet(v->Flags, VertexFlag_OpenStart) &&
                  IsVertexFlagSet(v->Flags, VertexFlag_OpenEnd));
    return(Result);
}

inline bool
IsOpenEnd(clipper_active *e)
{
    return(IsOpenEnd(e->vertex_top));
}

inline void
SwapFrontBackSides(output_rectangle *outrec)
{
    clipper_active *tmp = outrec->FrontEdge;
    outrec->FrontEdge = outrec->BackEdge;
    outrec->BackEdge = tmp;
    outrec->Points = outrec->Points->Next;
}

inline output_point *
AddOutPt(clipper *Clipper, clipper_active *e, v2_s64 pt)
{
    
    output_point *new_op = 0;

    //Outrec.OutPts: a circular doubly-linked-list of POutPt where ...
    //op_front[.Prev]* ~~~> op_back & op_back == op_front.Next
    u32 outrecIndex = e->outrecIndex;
    output_rectangle *outrec = GetOutRec(Clipper, outrecIndex);
    b32 to_front = IsFront(Clipper, e);
    output_point *op_front = outrec->Points;
    output_point *op_back = op_front->Next;

    if (to_front)
    {
        if (PointsAreEqual(pt, op_front->P))
            return op_front;
    }
    else if (PointsAreEqual(pt, op_back->P))
        return op_back;

    new_op = GetOutPt(Clipper, pt, outrecIndex);
    op_back->Prev = new_op;
    new_op->Prev = op_front;
    new_op->Next = op_back;
    op_front->Next = new_op;
    if (to_front)
        outrec->Points = new_op;
    return new_op;
}

inline void
UncoupleOutRec(clipper *Clipper, clipper_active *ae)
{
    output_rectangle *outrec = GetOutRec(Clipper, ae->outrecIndex);
    if (!outrec)
        return;

    outrec->FrontEdge->outrecIndex = 0;
    outrec->BackEdge->outrecIndex = 0;
    outrec->FrontEdge = 0;
    outrec->BackEdge = 0;
}

inline u32
GetRealOutRec(clipper *Clipper, output_rectangle *outrec)
{
    while(outrec && !outrec->Points)
        outrec = GetOutRec(Clipper, outrec->OwnerIndex);

    return(outrec->Index);
}

inline void
SetOwner(clipper *Clipper, output_rectangle *outrec, output_rectangle *new_owner)
{
    //precondition1: new_owner is never null
    output_rectangle *owner = GetOutRec(Clipper, new_owner->OwnerIndex);
    while (new_owner->OwnerIndex && !owner->Points)
    {
        new_owner->OwnerIndex = owner->OwnerIndex;
        owner = GetOutRec(Clipper, new_owner->OwnerIndex);
    }

    output_rectangle *tmp = new_owner;
    while (tmp->Index && (tmp->Index != outrec->Index))
        tmp = GetOutRec(Clipper, tmp->OwnerIndex);

    if (tmp->Index)
        new_owner->OwnerIndex = outrec->OwnerIndex;

    outrec->OwnerIndex = new_owner->Index;
}

internal void
JoinOutrecPaths(clipper *Clipper, clipper_active *e1, clipper_active *e2)
{
    //join e2 outrec path onto e1 outrec path and then delete e2 outrec path
    //pointers. (NB Only very rarely do the joining ends share the same coords.)

    output_rectangle *e1outrec = GetOutRec(Clipper, e1->outrecIndex);
    output_rectangle *e2outrec = GetOutRec(Clipper, e2->outrecIndex);

    output_point *p1_st = e1outrec->Points;
    output_point *p2_st = e2outrec->Points;
    output_point *p1_end = p1_st->Next;
    output_point *p2_end = p2_st->Next;
    if(IsFront(Clipper, e1))
    {
        p2_end->Prev = p1_st;
        p1_st->Next = p2_end;
        p2_st->Next = p1_end;
        p1_end->Prev = p2_st;
        e1outrec->Points = p2_st;
        e1outrec->FrontEdge = e2outrec->FrontEdge;
        if (e1outrec->FrontEdge)
            e1outrec->FrontEdge->outrecIndex = e1outrec->Index;
    }
    else
    {
        p1_end->Prev = p2_st;
        p2_st->Next = p1_end;
        p1_st->Next = p2_end;
        p2_end->Prev = p1_st;
        e1outrec->BackEdge = e2outrec->BackEdge;
        if(e1outrec->BackEdge)
            e1outrec->BackEdge->outrecIndex = e1outrec->Index;
    }

    // NOTE(babykaban): Not sure if necessary test!
//    e1outrec = GetOutRec(Clipper, e1->outrecIndex);
//    e2outrec = GetOutRec(Clipper, e2->outrecIndex);

    //after joining, the e2.OutRec must contains no vertices ...
    e2outrec->FrontEdge = 0;
    e2outrec->BackEdge = 0;
    e2outrec->Points = 0;

    if (IsOpenEnd(e1))
    {
        e2outrec->Points = e1outrec->Points;
        e1outrec->Points = 0;
    }
    else
        SetOwner(Clipper, e2outrec, e1outrec);

    //and e1 and e2 are maxima and are about to be dropped from the Actives list.
    e1->outrecIndex = 0;
    e2->outrecIndex = 0;
}

internal output_point *
AddLocalMaxPoly(clipper *Clipper, clipper_active *e1, clipper_active *e2, v2_s64 pt)
{
    if (IsJoined(e1))
        Split(Clipper, e1, pt);
    if (IsJoined(e2))
        Split(Clipper, e2, pt);

    if (IsFront(Clipper, e1) == IsFront(Clipper, e2))
    {
        if (IsOpenEnd(e1))
            SwapFrontBackSides(GetOutRec(Clipper, e1->outrecIndex));
        else if (IsOpenEnd(e2))
            SwapFrontBackSides(GetOutRec(Clipper, e2->outrecIndex));
        else
        {
            Clipper->Succeeded = false;
            return 0;
        }
    }

    output_point *result = AddOutPt(Clipper, e1, pt);
    if(e1->outrecIndex == e2->outrecIndex)
    {
        output_rectangle *outrec = GetOutRec(Clipper, e1->outrecIndex);
        outrec->Points = result;

        UncoupleOutRec(Clipper, e1);
        result = outrec->Points;
        if (outrec->OwnerIndex && !GetOutRec(Clipper, outrec->OwnerIndex)->FrontEdge)
            outrec->OwnerIndex = GetRealOutRec(Clipper, GetOutRec(Clipper, outrec->OwnerIndex));
    }
    //and to preserve the winding orientation of outrec ...
    else if (IsOpen(e1))
    {
        if (e1->wind_dx < 0)
            JoinOutrecPaths(Clipper, e1, e2);
        else
            JoinOutrecPaths(Clipper, e2, e1);
    }
    else if (e1->outrecIndex < e2->outrecIndex)
        JoinOutrecPaths(Clipper, e1, e2);
    else
        JoinOutrecPaths(Clipper, e2, e1);
    return result;
}

internal void
CheckJoinLeft(clipper *Clipper, clipper_active *e, v2_s64 pt, b32 check_curr_x = false)
{
    clipper_active *prev = e->prev_in_ael;
    if (!prev ||
        !IsHotEdge(e) || !IsHotEdge(prev) ||
        IsHorizontal(e) || IsHorizontal(prev) ||
        IsOpen(e) || IsOpen(prev) )
        return;

    if ((pt.y < e->top.y + 2 || pt.y < prev->top.y + 2) &&
        ((e->bot.y > pt.y) || (prev->bot.y > pt.y)))
        return; // avoid trivial joins

    if (check_curr_x)
    {
        if (PerpDistFromLineSq(pt, prev->bot, prev->top) > 0.25)
            return;
    }
    else if (e->curr_x != prev->curr_x)
        return;

    if (!IsCollinear(e->top, pt, prev->top))
        return;

    if (e->outrecIndex == prev->outrecIndex)
        AddLocalMaxPoly(Clipper, prev, e, pt);
    else if (e->outrecIndex < prev->outrecIndex)
        JoinOutrecPaths(Clipper, e, prev);
    else
        JoinOutrecPaths(Clipper, prev, e);

    prev->JoinWith = JoinWith_Right;
    e->JoinWith = JoinWith_Left;
}

internal void
CheckJoinRight(clipper *Clipper, clipper_active *e, v2_s64 pt, b32 check_curr_x = false)
{
    clipper_active *next = e->next_in_ael;
    if (!next ||
        !IsHotEdge(e) || !IsHotEdge(next) ||
        IsHorizontal(e) || IsHorizontal(next) ||
        IsOpen(e) || IsOpen(next))
        return;
    if ((pt.y < e->top.y +2 || pt.y < next->top.y +2) &&
        ((e->bot.y > pt.y) || (next->bot.y > pt.y)))
        return; // avoid trivial joins

    if (check_curr_x)
    {
        if (PerpDistFromLineSq(pt, next->bot, next->top) > 0.35)
            return;
    }
    else if (e->curr_x != next->curr_x)
        return;

    if (!IsCollinear(e->top, pt, next->top))
        return;

    if (e->outrecIndex == next->outrecIndex)
        AddLocalMaxPoly(Clipper, e, next, pt);
    else if (e->outrecIndex < next->outrecIndex)
        JoinOutrecPaths(Clipper, e, next);
    else
        JoinOutrecPaths(Clipper, next, e);

    e->JoinWith = JoinWith_Right;
    next->JoinWith = JoinWith_Left;
}

inline clipper_active *
FindEdgeWithMatchingLocMin(clipper_active *e)
{
     

    clipper_active *result = e->next_in_ael;
    while(result)
    {
        if (result->local_min == e->local_min)
            return result;
        else if (!IsHorizontal(result) && !PointsAreEqual(e->bot, result->bot))
            result = 0;
        else result = result->next_in_ael;
    }
    result = e->prev_in_ael;
    while (result)
    {
        if (result->local_min == e->local_min) return result;
        else if (!IsHorizontal(result) && !PointsAreEqual(e->bot, result->bot))
            return nullptr;
        else result = result->prev_in_ael;
    }
    return result;
}

inline output_point *
StartOpenPath(clipper *Clipper, clipper_active *e, v2_s64 pt)
{
    u32 outrecIndex = NewOutRec(Clipper);
    output_rectangle *outrec = GetOutRec(Clipper, outrecIndex);
    outrec->IsOpen = true;

    if(e->wind_dx > 0)
    {
        outrec->FrontEdge = e;
        outrec->BackEdge = 0;
    }
    else
    {
        outrec->FrontEdge = 0;
        outrec->BackEdge = e;
    }

    e->outrecIndex = outrecIndex;

    output_point *op = GetOutPt(Clipper, pt, outrecIndex);
    outrec->Points = op;
    return op;
}

inline void
SwapOutrecs(clipper *Clipper, clipper_active *e1, clipper_active *e2)
{
    output_rectangle *or1 = GetOutRec(Clipper, e1->outrecIndex);
    output_rectangle *or2 = GetOutRec(Clipper, e2->outrecIndex);;

    if (or1 == or2)
    {
        clipper_active *e = or1->FrontEdge;
        or1->FrontEdge = or1->BackEdge;
        or1->BackEdge = e;
        return;
    }
    if (or1)
    {
        if(e1 == or1->FrontEdge)
            or1->FrontEdge = e2;
        else
            or1->BackEdge = e2;
    }
    if (or2)
    {
        if(e2 == or2->FrontEdge)
            or2->FrontEdge = e1;
        else
            or2->BackEdge = e1;
    }

    e1->outrecIndex = or2->Index;
    e2->outrecIndex = or1->Index;
}

inline b32
IsSamePolyType(clipper_active *e1, clipper_active *e2)
{
    return e1->local_min->PolyType == e2->local_min->PolyType;
}

internal void
IntersectEdges(clipper *Clipper, clipper_active *e1, clipper_active *e2, v2_s64 pt)
{

    //MANAGE OPEN PATH INTERSECTIONS SEPARATELY ...
    if(Clipper->HasOpenPaths && (IsOpen(e1) || IsOpen(e2)))
    {
        if (IsOpen(e1) && IsOpen(e2)) return;
        clipper_active *edge_o, *edge_c;
        if (IsOpen(e1))
        {
            edge_o = e1;
            edge_c = e2;
        }
        else
        {
            edge_o = e2;
            edge_c = e1;
        }
        if(IsJoined(edge_c))
            Split(Clipper, edge_c, pt); // needed for safety

        if (abs(edge_c->wind_cnt) != 1)
            return;

        switch(Clipper->ClipType)
        {
            case ClipType_Union:
                if (!IsHotEdge(edge_c)) return;
                break;
            default:
                if (edge_c->local_min->PolyType == PathType_Subject)
                    return;
        }

        switch(Clipper->FillRule)
        {
            case FillRule_Positive: 
                if (edge_c->wind_cnt != 1)
                    return; 
                break;
            case FillRule_Negative: 
                if (edge_c->wind_cnt != -1)
                    return; 
                break;
            default: 
                if (abs(edge_c->wind_cnt) != 1)
                    return; 
        }

        //toggle contribution ...
        if(IsHotEdge(edge_o))
        {
            AddOutPt(Clipper, edge_o, pt);

            output_rectangle *outrec = GetOutRec(Clipper, edge_o->outrecIndex);
            if(IsFront(Clipper, edge_o))
                outrec->FrontEdge = 0;
            else
                outrec->BackEdge = 0;

            edge_o->outrecIndex = 0;
        }

        //horizontal edges can pass under open paths at a LocMins
        else if (PointsAreEqual(pt, edge_o->local_min->Vertex->P) &&
                 !IsOpenEnd(edge_o->local_min->Vertex))
        {
            //find the other side of the LocMin and
            //if it's 'hot' join up with it ...
            clipper_active *e3 = FindEdgeWithMatchingLocMin(edge_o);
            if (e3 && IsHotEdge(e3))
            {
                edge_o->outrecIndex = e3->outrecIndex;
                if (edge_o->wind_dx > 0)
                    SetSides(GetOutRec(Clipper, e3->outrecIndex), edge_o, e3);
                else
                    SetSides(GetOutRec(Clipper, e3->outrecIndex), e3, edge_o);
                return;
            }
            else
                StartOpenPath(Clipper, edge_o, pt);
        }
        else
            StartOpenPath(Clipper, edge_o, pt);

        return;
    } // end of an open path intersection

    //MANAGING CLOSED PATHS FROM HERE ON

    if (IsJoined(e1))
        Split(Clipper, e1, pt);
    if (IsJoined(e2))
        Split(Clipper, e2, pt);

    //UPDATE WINDING COUNTS...

    int old_e1_windcnt, old_e2_windcnt;
    if (e1->local_min->PolyType == e2->local_min->PolyType)
    {
        if(Clipper->FillRule == FillRule_EvenOdd)
        {
            old_e1_windcnt = e1->wind_cnt;
            e1->wind_cnt = e2->wind_cnt;
            e2->wind_cnt = old_e1_windcnt;
        }
        else
        {
            if (e1->wind_cnt + e2->wind_dx == 0)
                e1->wind_cnt = -e1->wind_cnt;
            else
                e1->wind_cnt += e2->wind_dx;
            if (e2->wind_cnt - e1->wind_dx == 0)
                e2->wind_cnt = -e2->wind_cnt;
            else
                e2->wind_cnt -= e1->wind_dx;
        }
    }
    else
    {
        if(Clipper->FillRule != FillRule_EvenOdd)
        {
            e1->wind_cnt2 += e2->wind_dx;
            e2->wind_cnt2 -= e1->wind_dx;
        }
        else
        {
            e1->wind_cnt2 = (e1->wind_cnt2 == 0 ? 1 : 0);
            e2->wind_cnt2 = (e2->wind_cnt2 == 0 ? 1 : 0);
        }
    }

    switch (Clipper->FillRule)
    {
        case FillRule_EvenOdd:
        case FillRule_NonZero:
            old_e1_windcnt = abs(e1->wind_cnt);
            old_e2_windcnt = abs(e2->wind_cnt);
            break;
        default:
            if(Clipper->FillRule == FillRule_Positive)
            {
                old_e1_windcnt = e1->wind_cnt;
                old_e2_windcnt = e2->wind_cnt;
            }
            else
            {
                old_e1_windcnt = -e1->wind_cnt;
                old_e2_windcnt = -e2->wind_cnt;
            }
            break;
    }

    const bool e1_windcnt_in_01 = old_e1_windcnt == 0 || old_e1_windcnt == 1;
    const bool e2_windcnt_in_01 = old_e2_windcnt == 0 || old_e2_windcnt == 1;

    if ((!IsHotEdge(e1) && !e1_windcnt_in_01) || 
        (!IsHotEdge(e2) && !e2_windcnt_in_01))
        return;

    //NOW PROCESS THE INTERSECTION ...
    //if both edges are 'hot' ...
    if (IsHotEdge(e1) && IsHotEdge(e2))
    {
        if ((old_e1_windcnt != 0 && old_e1_windcnt != 1) || (old_e2_windcnt != 0 && old_e2_windcnt != 1) ||
            (e1->local_min->PolyType != e2->local_min->PolyType && Clipper->ClipType != ClipType_Xor))
        {
            AddLocalMaxPoly(Clipper, e1, e2, pt);
        }
        else if (IsFront(Clipper, e1) || (e1->outrecIndex == e2->outrecIndex))
        {
            //this 'else if' condition isn't strictly needed but
            //it's sensible to split polygons that ony touch at
            //a common clipper_vertex (not at common edges).

            AddLocalMaxPoly(Clipper, e1, e2, pt);
            AddLocalMinPoly(Clipper, e1, e2, pt);
        }
        else
        {
            AddOutPt(Clipper, e1, pt);
            AddOutPt(Clipper, e2, pt);
            SwapOutrecs(Clipper, e1, e2);
        }
    }
    else if (IsHotEdge(e1))
    {
        AddOutPt(Clipper, e1, pt);
        SwapOutrecs(Clipper, e1, e2);
    }
    else if (IsHotEdge(e2))
    {
        AddOutPt(Clipper, e2, pt);
        SwapOutrecs(Clipper, e1, e2);
    }
    else
    {
        s64 e1Wc2, e2Wc2;
        switch(Clipper->FillRule)
        {
            case FillRule_EvenOdd:
            case FillRule_NonZero:
                e1Wc2 = abs(e1->wind_cnt2);
                e2Wc2 = abs(e2->wind_cnt2);
                break;
            default:
                if(Clipper->FillRule == FillRule_Positive)
                {
                    e1Wc2 = e1->wind_cnt2;
                    e2Wc2 = e2->wind_cnt2;
                }
                else
                {
                    e1Wc2 = -e1->wind_cnt2;
                    e2Wc2 = -e2->wind_cnt2;
                }
                break;
        }

        if(!IsSamePolyType(e1, e2))
        {
            AddLocalMinPoly(Clipper, e1, e2, pt, false);
        }
        else if (old_e1_windcnt == 1 && old_e2_windcnt == 1)
        {
            switch(Clipper->ClipType)
            {
                case ClipType_Union:
                    if (e1Wc2 <= 0 && e2Wc2 <= 0)
                        AddLocalMinPoly(Clipper, e1, e2, pt, false);
                    break;
                case ClipType_Difference:
                    if (((GetPolyType(e1) == PathType_Clip) && (e1Wc2 > 0) && (e2Wc2 > 0)) ||
                        ((GetPolyType(e1) == PathType_Subject) && (e1Wc2 <= 0) && (e2Wc2 <= 0)))
                    {
                        AddLocalMinPoly(Clipper, e1, e2, pt, false);
                    }
                    break;
                case ClipType_Xor:
                    AddLocalMinPoly(Clipper, e1, e2, pt, false);
                    break;
                default:
                    if (e1Wc2 > 0 && e2Wc2 > 0)
                        AddLocalMinPoly(Clipper, e1, e2, pt, false);
                    break;
            }
        }
    }
}

inline void
SwapPositionsInAEL(clipper *Clipper, clipper_active *e1, clipper_active *e2)
{
    //preconditon: e1 must be immediately to the left of e2
    clipper_active *next = e2->next_in_ael;
    if(next)
        next->prev_in_ael = e1;

    clipper_active *prev = e1->prev_in_ael;
    if(prev)
        prev->next_in_ael = e2;

    e2->prev_in_ael = prev;
    e2->next_in_ael = e1;
    e1->prev_in_ael = e2;
    e1->next_in_ael = next;

    if(!e2->prev_in_ael)
        Clipper->ActiveEdgeList = e2;
}

inline void
PushHorz(clipper *Clipper, clipper_active *e)
{
    e->next_in_sel = (Clipper->StoredEdgeList ? Clipper->StoredEdgeList : 0);
    Clipper->StoredEdgeList = e;
}

inline void
InsertScanline(clipper *Clipper, s64 y)
{
    clipper_sort_entry Entry = {};
    Entry.Value_S64 = y;
    MaxHeapInsertNode(&Clipper->ScanLineMaxHeap, Entry);
}

inline void
InsertLocalMinimaIntoAEL(clipper *Clipper, s64 bot_y)
{
//    TimeFunction;
//    RecordFunction(0, Clipper->ClipType, Clipper->FillRule);
    
    local_minima *Minima = 0;
    clipper_active *left_bound;
    clipper_active *right_bound;
    //Add any local minima (if any) at BotY ...
    //nb: horizontal local minima edges should contain locMin.clipper_vertex.prev

    while(PopLocalMinima(Clipper, bot_y, &Minima))
    {
        if(IsVertexFlagSet(Minima->Vertex->Flags, VertexFlag_OpenStart))
        {
            left_bound = 0;
        }
        else
        {
            left_bound = GetNewActive(Clipper);
            {
//                TimeBlock("Check for existence Left");

                left_bound->bot = Minima->Vertex->P;
                left_bound->curr_x = left_bound->bot.x;
                left_bound->wind_dx = -1;
                left_bound->vertex_top = Minima->Vertex->Prev;  // ie descending
                left_bound->top = left_bound->vertex_top->P;
                left_bound->local_min = Minima;
                SetDx(left_bound);
            }
        }

        if (IsVertexFlagSet(Minima->Vertex->Flags, VertexFlag_OpenEnd))
        {
            right_bound = 0;
        }
        else
        {
            right_bound = GetNewActive(Clipper);
            {
//                TimeBlock("Check for existence Right");

                right_bound->bot = Minima->Vertex->P;
                right_bound->curr_x = right_bound->bot.x;
                right_bound->wind_dx = 1;
                right_bound->vertex_top = Minima->Vertex->Next;  // ie ascending
                right_bound->top = right_bound->vertex_top->P;
                right_bound->local_min = Minima;
                SetDx(right_bound);
            }
        }
        
        //Currently LeftB is just the descending bound and RightB is the ascending.
        //Now if the LeftB isn't on the left of RightB then we need swap them.
        {
//            TimeBlock("Swap bounds");
            if(left_bound && right_bound)
            {
                if(IsHorizontal(left_bound))
                {
                    if(IsHeadingRightHorz(left_bound))
                        SwapActives(&left_bound, &right_bound);
                }
                else if (IsHorizontal(right_bound))
                {
                    if (IsHeadingLeftHorz(right_bound))
                        SwapActives(&left_bound, &right_bound);
                }
                else if (left_bound->dx < right_bound->dx)
                    SwapActives(&left_bound, &right_bound);
            }
            else if (!left_bound)
            {
                left_bound = right_bound;
                right_bound = 0;
            }
        }

        b32 contributing;
        left_bound->IsLeftBound = true;
        InsertLeftEdge(Clipper, left_bound);
        {
//            TimeBlock("Check Contribution");
            if(IsOpen(left_bound))
            {
                SetWindCountForOpenPathEdge(Clipper, left_bound);
                contributing = IsContributingOpen(Clipper, left_bound);
            }
            else
            {
                SetWindCountForClosedPathEdge(Clipper, left_bound);
                contributing = IsContributingClosed(Clipper, left_bound);
            }
        }

        if(right_bound)
        {
            right_bound->IsLeftBound = false;
            right_bound->wind_cnt = left_bound->wind_cnt;
            right_bound->wind_cnt2 = left_bound->wind_cnt2;
            InsertRightEdge(left_bound, right_bound);  ///////
            if (contributing)
            {
//                TimeBlock("Contribution");
                AddLocalMinPoly(Clipper, left_bound, right_bound, left_bound->bot, true);
                if (!IsHorizontal(left_bound))
                    CheckJoinLeft(Clipper, left_bound, left_bound->bot);
            }

            while (right_bound->next_in_ael &&
                   IsValidAelOrder(right_bound->next_in_ael, right_bound))
            {
                IntersectEdges(Clipper, right_bound, right_bound->next_in_ael, right_bound->bot);
                SwapPositionsInAEL(Clipper, right_bound, right_bound->next_in_ael);
            }
                
            if(IsHorizontal(right_bound))
                PushHorz(Clipper, right_bound);
            else
            {
                CheckJoinRight(Clipper, right_bound, right_bound->bot);
                InsertScanline(Clipper, right_bound->top.y);
            }
        }
        else if (contributing)
        {
            StartOpenPath(Clipper, left_bound, left_bound->bot);
        }
        
        {
//            TimeBlock("Final Check");
            if (IsHorizontal(left_bound))
                PushHorz(Clipper, left_bound);
            else
                InsertScanline(Clipper, left_bound->top.y);
        }
    }
}

inline b32
PopHorz(clipper *Clipper, clipper_active **e)
{
    *e = Clipper->StoredEdgeList;
    if(!(*e))
        return false;
    Clipper->StoredEdgeList = Clipper->StoredEdgeList->next_in_sel;
    return true;
}

inline b32
IsMaxima(clipper_vertex *v)
{
    return IsVertexFlagSet(v->Flags, VertexFlag_LocalMax);
}

inline b32
IsMaxima(clipper_active *e)
{
    return IsMaxima(e->vertex_top);
}

inline clipper_vertex *
GetCurrYMaximaVertex_Open(clipper_active *e)
{
     

    clipper_vertex *result = e->vertex_top;
    if (e->wind_dx > 0)
        while ((result->Next->P.y == result->P.y) &&
               ((result->Flags & (VertexFlag_OpenEnd | VertexFlag_LocalMax)) == 0))
            result = result->Next;
    else
        while(result->Prev->P.y == result->P.y &&
               ((result->Flags & (VertexFlag_OpenEnd |
                                  VertexFlag_LocalMax)) == 0))
            result = result->Prev;

    if (!IsMaxima(result))
        result = 0; // not a maxima

    return result;
}

inline clipper_vertex *
GetCurrYMaximaVertex(clipper_active *e)
{
     

    clipper_vertex *result = e->vertex_top;
    if (e->wind_dx > 0)
        while (result->Next->P.y == result->P.y)
            result = result->Next;
    else
        while (result->Prev->P.y == result->P.y)
            result = result->Prev;

    if (!IsMaxima(result))
        result = 0; // not a maxima

    return result;
}

inline b32
ResetHorzDirection(clipper_active *horz, clipper_vertex *max_vertex, s64 *horz_left, s64 *horz_right)
{
     

    if(horz->bot.x == horz->top.x)
    {
        //the horizontal edge is going nowhere ...
        *horz_left = horz->curr_x;
        *horz_right = horz->curr_x;

        clipper_active *e = horz->next_in_ael;
        while(e && e->vertex_top != max_vertex)
            e = e->next_in_ael;
        return e != nullptr;
    }
    else if(horz->curr_x < horz->top.x)
    {
        *horz_left = horz->curr_x;
        *horz_right = horz->top.x;
        return true;
    }
    else
    {
        *horz_left = horz->top.x;
        *horz_right = horz->curr_x;
        return false;  // right to left
    }
}

inline void
AddTrialHorzJoin(clipper *Clipper, output_point *op)
{
    if(GetOutRec(Clipper, op->OutRectIndex)->IsOpen)
        return;

    if(NeedIncrease(Clipper->HorzCount))
    {
        IncreaseHorzSegList(Clipper);
    }
    
    Clipper->HorzSegList[Clipper->HorzCount].left_to_right = true;
    Clipper->HorzSegList[Clipper->HorzCount].left_op = op;
    Clipper->HorzCount++;    
}

inline void
TrimHorz(clipper_active *horzEdge, b32 preserveCollinear)
{
     

    b32 wasTrimmed = false;
    v2_s64 pt = NextVertex(horzEdge)->P;
    while (pt.y == horzEdge->top.y)
    {
        //always trim 180 deg. spikes (in closed paths)
        //but otherwise break if preserveCollinear = true
        if (preserveCollinear &&
            ((pt.x < horzEdge->top.x) != (horzEdge->bot.x < horzEdge->top.x)))
            break;

        horzEdge->vertex_top = NextVertex(horzEdge);
        horzEdge->top = pt;
        wasTrimmed = true;
        if(IsMaxima(horzEdge))
            break;
        pt = NextVertex(horzEdge)->P;
    }

    if (wasTrimmed)
        SetDx(horzEdge); // +/-infinity
}

inline void
UpdateEdgeIntoAEL(clipper *Clipper, clipper_active *e)
{

    e->bot = e->top;
    e->vertex_top = NextVertex(e);
    e->top = e->vertex_top->P;
    e->curr_x = e->bot.x;
    SetDx(e);

    if(IsJoined(e))
        Split(Clipper, e, e->bot);

    if(IsHorizontal(e))
    {
        if (!IsOpen(e))
            TrimHorz(e, Clipper->PreserveCollinear);
        return;
    }

    InsertScanline(Clipper, e->top.y);
    CheckJoinLeft(Clipper, e, e->bot);
    CheckJoinRight(Clipper, e, e->bot, true); // (#500)
}

inline void
DeleteFromAEL(clipper *Clipper, clipper_active *e)
{
//    TimeFunction;

    clipper_active *prev = e->prev_in_ael;
    clipper_active *next = e->next_in_ael;
    if (!prev && !next && (e != Clipper->ActiveEdgeList))
        return;  // already deleted

    if (prev)
        prev->next_in_ael = next;
    else
        Clipper->ActiveEdgeList = next;
    if (next)
        next->prev_in_ael = prev;

    FreeActive(Clipper, e);
//    Free(e, sizeof(clipper_active));
}

inline s64
TopX(clipper_active ae, s64 currentY)
{
    if ((currentY == ae.top.y) || (ae.top.x == ae.bot.x))
        return ae.top.x;
    else if (currentY == ae.bot.y)
        return ae.bot.x;
    else
        return ae.bot.x + ClipperNearbyInt(ae.dx * (currentY - ae.bot.y));
    // nb: std::nearbyint (or std::round) substantially *improves* performance here
    // as it greatly improves the likelihood of edge adjacency in ProcessIntersectList().
}
 
inline output_point *
GetLastOp(clipper *Clipper, clipper_active *hot_edge)
{
    output_rectangle *outrec = GetOutRec(Clipper, hot_edge->outrecIndex);
    output_point *result = outrec->Points;
    if (hot_edge != outrec->FrontEdge)
        result = result->Next;
    return result;
}

inline void
DoHorizontal(clipper *Clipper, clipper_active *horz)
/*******************************************************************************
 * Notes: Horizontal edges (HEs) at scanline intersections (ie at the top or    *
 * bottom of a scanbeam) are processed as if layered.The order in which HEs     *
 * are processed doesn't matter. HEs intersect with the bottom vertices of      *
 * other HEs[#] and with non-horizontal edges [*]. Once these intersections     *
 * are completed, intermediate HEs are 'promoted' to the next edge in their     *
 * bounds, and they in turn may be intersected[%] by other HEs.                 *
 *                                                                              *
 * eg: 3 horizontals at a scanline:    /   |                     /           /  *
 *              |                     /    |     (HE3)o ========%========== o   *
 *              o ======= o(HE2)     /     |         /         /                *
 *          o ============#=========*======*========#=========o (HE1)           *
 *         /              |        /       |       /                            *
 *******************************************************************************/
{
//    TimeFunction;
//    RecordFunction(0, Clipper->ClipType, Clipper->FillRule);
    
    v2_s64 pt;
    b32 horzIsOpen = IsOpen(horz);
    s64 y = horz->bot.y;
    clipper_vertex *vertex_max;

    if (horzIsOpen)
        vertex_max = GetCurrYMaximaVertex_Open(horz);
    else
        vertex_max = GetCurrYMaximaVertex(horz);

    //// remove 180 deg.spikes and also simplify
    //// consecutive horizontals when PreserveCollinear = true
    //if (!horzIsOpen && vertex_max != horz.vertex_top)
    //  TrimHorz(horz, PreserveCollinear);

    s64 horz_left, horz_right;
    b32 is_left_to_right = ResetHorzDirection(horz, vertex_max, &horz_left, &horz_right);

    if(IsHotEdge(horz))
    {
        output_point *op = AddOutPt(Clipper, horz, V2S64(horz->curr_x, y));
        AddTrialHorzJoin(Clipper, op);
    }

    while (true) // loop through consec. horizontal edges
    {
        clipper_active *e;
        if(is_left_to_right)
            e = horz->next_in_ael;
        else
            e = horz->prev_in_ael;

        while (e)
        {
            if (e->vertex_top == vertex_max)
            {
                if(IsHotEdge(horz) && IsJoined(e))
                    Split(Clipper, e, e->top);

                //if (IsHotEdge(horz) != IsHotEdge(*e))
                //    DoError(undefined_error_i);

                if (IsHotEdge(horz))
                {
                    while(horz->vertex_top != vertex_max)
                    {
                        AddOutPt(Clipper, horz, horz->top);
                        UpdateEdgeIntoAEL(Clipper, horz);
                    }
                    if (is_left_to_right)
                        AddLocalMaxPoly(Clipper, horz, e, horz->top);
                    else
                        AddLocalMaxPoly(Clipper, e, horz, horz->top);
                }

                DeleteFromAEL(Clipper, e);
                DeleteFromAEL(Clipper, horz);

                return;
            }

            //if horzEdge is a maxima, keep going until we reach
            //its maxima pair, otherwise check for break conditions
            if(vertex_max != horz->vertex_top || IsOpenEnd(horz))
            {
                //otherwise stop when 'ae' is beyond the end of the horizontal line
                if ((is_left_to_right && e->curr_x > horz_right) ||
                    (!is_left_to_right && e->curr_x < horz_left))
                    break;

                if (e->curr_x == horz->top.x && !IsHorizontal(e))
                {
                    pt = NextVertex(horz)->P;
                    if (is_left_to_right)
                    {
                        //with open paths we'll only break once past horz's end
                        if(IsOpen(e) && !IsSamePolyType(e, horz) && !IsHotEdge(e))
                        {
                            if (TopX(*e, pt.y) > pt.x)
                                break;
                        }
                        //otherwise we'll only break when horz's outslope is greater than e's
                        else if (TopX(*e, pt.y) >= pt.x)
                            break;
                    }
                    else
                    {
                        if (IsOpen(e) && !IsSamePolyType(e, horz) && !IsHotEdge(e))
                        {
                            if (TopX(*e, pt.y) < pt.x)
                                break;
                        }
                        else if (TopX(*e, pt.y) <= pt.x)
                            break;
                    }
                }
            }

            pt = V2S64(e->curr_x, horz->bot.y);
            if (is_left_to_right)
            {
                IntersectEdges(Clipper, horz, e, pt);
                SwapPositionsInAEL(Clipper, horz, e);
                CheckJoinLeft(Clipper, e, pt);
                horz->curr_x = e->curr_x;
                e = horz->next_in_ael;
            }
            else
            {
                IntersectEdges(Clipper, e, horz, pt);
                SwapPositionsInAEL(Clipper, e, horz);
                CheckJoinRight(Clipper, e, pt);
                horz->curr_x = e->curr_x;
                e = horz->prev_in_ael;
            }

            if(horz->outrecIndex != 0)
            {
                //nb: The outrec containining the op returned by IntersectEdges
                //above may no longer be associated with horzEdge.
                AddTrialHorzJoin(Clipper, GetLastOp(Clipper, horz));
            }
        }

        //check if we've finished with (consecutive) horizontals ...
        if (horzIsOpen && IsOpenEnd(horz)) // ie open at top
        {
            if (IsHotEdge(horz))
            {
                AddOutPt(Clipper, horz, horz->top);
                output_rectangle *outrec = GetOutRec(Clipper, horz->outrecIndex);
                if (IsFront(Clipper, horz))
                    outrec->FrontEdge = 0;
                else
                    outrec->BackEdge = 0;

                horz->outrecIndex = 0;
            }

            DeleteFromAEL(Clipper, horz);
            return;
        }
        else if (NextVertex(horz)->P.y != horz->top.y)
            break;

        //still more horizontals in bound to process ...
        if (IsHotEdge(horz))
            AddOutPt(Clipper, horz, horz->top);
        UpdateEdgeIntoAEL(Clipper, horz);

        is_left_to_right =
            ResetHorzDirection(horz, vertex_max, &horz_left, &horz_right);
    }

    if (IsHotEdge(horz))
    {
        output_point *op = AddOutPt(Clipper, horz, horz->top);
        AddTrialHorzJoin(Clipper, op);
    }

    UpdateEdgeIntoAEL(Clipper, horz); // end of an intermediate horiz.
}

inline b32
SetHorzSegHeadingForward(horz_segment *hs, output_point *opP, output_point *opN)
{
     

    if (opP->P.x == opN->P.x)
        return false;

    if(opP->P.x < opN->P.x)
    {
        hs->left_op = opP;
        hs->right_op = opN;
        hs->left_to_right = true;
    }
    else
    {
        hs->left_op = opN;
        hs->right_op = opP;
        hs->left_to_right = false;
    }
    return true;
}

inline b32
UpdateHorzSegment(clipper *Clipper, horz_segment *hs)
{
    output_point *op = hs->left_op;
    output_rectangle *outrec = GetOutRec(Clipper, GetRealOutRec(Clipper, GetOutRec(Clipper, op->OutRectIndex)));
    b32 outrecHasEdges = (outrec->FrontEdge) ? 1 : 0;

    s64 curr_y = op->P.y;

    output_point *opP = op;
    output_point *opN = op;

    if(outrecHasEdges)
    {
        output_point *opA = outrec->Points;
        output_point *opZ = opA->Next;

        while (opP != opZ && opP->Prev->P.y == curr_y)
            opP = opP->Prev;
        while (opN != opA && opN->Next->P.y == curr_y)
            opN = opN->Next;
    }
    else
    {
        while (opP->Prev != opN && opP->Prev->P.y == curr_y)
            opP = opP->Prev;
        while (opN->Next != opP && opN->Next->P.y == curr_y)
            opN = opN->Next;
    }

    b32 result = (SetHorzSegHeadingForward(hs, opP, opN) &&
                  !hs->left_op->Horz);

    if(result)
        hs->left_op->Horz = hs;
    else
        hs->right_op = 0; // (for sorting)

    return result;
}

inline output_point *
DuplicateOp(clipper *Clipper, output_point *op, b32 insert_after)
{
    output_point *result = GetOutPt(Clipper, op->P, op->OutRectIndex);

    if(insert_after)
    {
        result->Next = op->Next;
        result->Next->Prev = result;
        result->Prev = op;
        op->Next = result;
    }
    else
    {
        result->Prev = op->Prev;
        result->Prev->Next = result;
        result->Next = op;
        op->Prev = result;
    }
    return result;
}

inline void
ConvertHorzSegsToJoins(clipper *Clipper)
{
//    TimeFunction;
    
    u32 J = 0;
    for(u32 I = 0;
        I < Clipper->HorzCount;
        ++I)
    {
        horz_segment *hs = Clipper->HorzSegList + I;
        if(UpdateHorzSegment(Clipper, hs))
        {
            ++J;
        }
    }

    if(J < 2)
        return;

    MergeSort(Clipper->HorzCount, Clipper->HorzSegList);
        
    horz_segment *hs1 = Clipper->HorzSegList + 0;
    horz_segment *hs2;
    horz_segment *hs_end = hs1 + J;
    horz_segment *hs_end1 = hs_end - 1;

    for (; hs1 != hs_end1; ++hs1)
    {
        for(hs2 = hs1 + 1; hs2 != hs_end; ++hs2)
        {
            if ((hs2->left_op->P.x >= hs1->right_op->P.x) ||
                (hs2->left_to_right == hs1->left_to_right) ||
                (hs2->right_op->P.x <= hs1->left_op->P.x))
                continue;

            s64 curr_y = hs1->left_op->P.y;

            if(NeedIncrease(Clipper->JointCount))
            {
                IncreaseHorzJoinList(Clipper);
            }
            
            if (hs1->left_to_right)
            {
                while (hs1->left_op->Next->P.y == curr_y &&
                       hs1->left_op->Next->P.x <= hs2->left_op->P.x)
                    hs1->left_op = hs1->left_op->Next;

                while (hs2->left_op->Prev->P.y == curr_y &&
                       hs2->left_op->Prev->P.x <= hs1->left_op->P.x)
                    hs2->left_op = hs2->left_op->Prev;

                horz_join Join = HorzJoin(DuplicateOp(Clipper, hs1->left_op, true),
                                          DuplicateOp(Clipper, hs2->left_op, false));

                Clipper->HorzJoinList[Clipper->JointCount++] = Join;
            }
            else
            {
                while (hs1->left_op->Prev->P.y == curr_y &&
                       hs1->left_op->Prev->P.x <= hs2->left_op->P.x)
                    hs1->left_op = hs1->left_op->Prev;

                while (hs2->left_op->Next->P.y == curr_y &&
                       hs2->left_op->Next->P.x <= hs1->left_op->P.x)
                    hs2->left_op = hs2->left_op->Next;

                horz_join Join = HorzJoin(DuplicateOp(Clipper, hs2->left_op, true),
                                          DuplicateOp(Clipper, hs1->left_op, false));

                Clipper->HorzJoinList[Clipper->JointCount++] = Join;
            }
        }
    }
}

inline void
AdjustCurrXAndCopyToSEL(clipper *Clipper, s64 top_y)
{
    clipper_active *e = Clipper->ActiveEdgeList;
    Clipper->StoredEdgeList = e;
    while(e)
    {
        e->prev_in_sel = e->prev_in_ael;
        e->next_in_sel = e->next_in_ael;
        e->jump = e->next_in_sel;
        if (e->JoinWith == JoinWith_Left)
            e->curr_x = e->prev_in_ael->curr_x; // also avoids complications
        else
            e->curr_x = TopX(*e, top_y);

        e = e->next_in_ael;
    }
}

internal void
AddNewIntersectNode(clipper *Clipper, clipper_active *e1, clipper_active *e2, s64 top_y)
{
//    TimeFunction;
    
    v2_s64 ip;
    if(!GetSegmentIntersectPt(e1->bot, e1->top, e2->bot, e2->top, &ip))
        ip = V2S64(e1->curr_x, top_y); //parallel edges

    //rounding errors can occasionally place the calculated intersection
    //point either below or above the scanbeam, so check and correct ...
    if(ip.y > Clipper->BottomY || ip.y < top_y)
    {
        f64 abs_dx1 = fabs(e1->dx);
        f64 abs_dx2 = fabs(e2->dx);
        if(abs_dx1 > 100 && abs_dx2 > 100)
        {
            if (abs_dx1 > abs_dx2)
                ip = GetClosestPointOnSegment(ip, e1->bot, e1->top);
            else
                ip = GetClosestPointOnSegment(ip, e2->bot, e2->top);
        }
        else if (abs_dx1 > 100)
            ip = GetClosestPointOnSegment(ip, e1->bot, e1->top);
        else if (abs_dx2 > 100)
            ip = GetClosestPointOnSegment(ip, e2->bot, e2->top);
        else
        {
            if (ip.y < top_y)
                ip.y = top_y;
            else
                ip.y = Clipper->BottomY;

            if (abs_dx1 < abs_dx2)
                ip.x = TopX(*e1, ip.y);
            else
                ip.x = TopX(*e2, ip.y);
        }
    }

    if(NeedIncrease(Clipper->IntersectNodeCount))
    {
        IncreaseIntersectNodes(Clipper);
    }
    
    Clipper->IntersectNodes[Clipper->IntersectNodeCount++] = IntersectNode(e1, e2, ip);
}

inline clipper_active *
ExtractFromSEL(clipper_active *ae)
{
    clipper_active *res = ae->next_in_sel;

    if(res)
        res->prev_in_sel = ae->prev_in_sel;

    ae->prev_in_sel->next_in_sel = res;

    return res;
}

inline void
Insert1Before2InSEL(clipper_active *ae1, clipper_active *ae2)
{
    ae1->prev_in_sel = ae2->prev_in_sel;
    if (ae1->prev_in_sel)
        ae1->prev_in_sel->next_in_sel = ae1;

    ae1->next_in_sel = ae2;
    ae2->prev_in_sel = ae1;
}

internal b32
BuildIntersectList(clipper *Clipper, s64 top_y)
{
//    RecordFunction(0, Clipper->ClipType, Clipper->FillRule);

    if (!Clipper->ActiveEdgeList || !Clipper->ActiveEdgeList->next_in_ael)
        return false;

    //Calculate edge positions at the top of the current scanbeam, and from this
    //we will determine the intersections required to reach these new positions.
    AdjustCurrXAndCopyToSEL(Clipper, top_y);
    //Find all edge intersections in the current scanbeam using a stable merge
    //sort that ensures only adjacent edges are intersecting. Intersect info is
    //stored in FIntersectList ready to be processed in ProcessIntersectList.
    //Re merge sorts see https://stackoverflow.com/a/46319131/359538

    clipper_active *left = Clipper->StoredEdgeList;
    clipper_active *right;
    clipper_active *l_end;
    clipper_active *r_end;
    clipper_active *curr_base;
    clipper_active *tmp;

    while(left && left->jump)
    {
        clipper_active *prev_base = 0;
        while(left && left->jump)
        {
            curr_base = left;
            right = left->jump;
            l_end = right;
            r_end = right->jump;
            left->jump = r_end;
            while (left != l_end && right != r_end)
            {
                if (right->curr_x < left->curr_x)
                {
                    tmp = right->prev_in_sel;
                    for (; ; )
                    {
                        AddNewIntersectNode(Clipper, tmp, right, top_y);
                        if (tmp == left)
                            break;

                        tmp = tmp->prev_in_sel;
                    }

                    tmp = right;
                    right = ExtractFromSEL(tmp);
                    l_end = right;
                    Insert1Before2InSEL(tmp, left);
                    if (left == curr_base)
                    {
                        curr_base = tmp;
                        curr_base->jump = r_end;
                        if(!prev_base)
                            Clipper->StoredEdgeList = curr_base;
                        else
                            prev_base->jump = curr_base;
                    }
                }
                else
                    left = left->next_in_sel;
            }

            prev_base = curr_base;
            left = r_end;
        }

        left = Clipper->StoredEdgeList;
    }

    return (Clipper->IntersectNodeCount > 0);
}

inline b32
EdgesAdjacentInAEL(intersect_node *inode)
{
    return((inode->edge1->next_in_ael == inode->edge2) ||
           (inode->edge1->prev_in_ael == inode->edge2));
}

inline void
ProcessIntersectList(clipper *Clipper)
{
//    RecordFunction(0, Clipper->ClipType, Clipper->FillRule);

    //We now have a list of intersections required so that edges will be
    //correctly positioned at the top of the scanbeam. However, it's important
    //that edge intersections are processed from the bottom up, but it's also
    //crucial that intersections only occur between adjacent edges.

    //First we do a quicksort so intersections proceed in a bottom up order ...
    if(Clipper->IntersectNodeCount < 32)
    {
        InsertionSort(Clipper->IntersectNodeCount, Clipper->IntersectNodes);
    }
    else
    {
        MergeSort(Clipper->IntersectNodeCount, Clipper->IntersectNodes);
    }

    //Now as we process these intersections, we must sometimes adjust the order
    //to ensure that intersecting edges are always adjacent ...

    intersect_node *node_iter;
    intersect_node *node_iter2;
    
    for (node_iter = (Clipper->IntersectNodes + 0);
         node_iter != (Clipper->IntersectNodes + Clipper->IntersectNodeCount);
         ++node_iter)
    {
        if (!EdgesAdjacentInAEL(node_iter))
        {
            node_iter2 = node_iter + 1;
            while (!EdgesAdjacentInAEL(node_iter2))
                ++node_iter2;

            intersect_node Temp = *node_iter;
            *node_iter = *node_iter2;
            *node_iter2 = Temp;
        }

        intersect_node node = *node_iter;
        IntersectEdges(Clipper, node.edge1, node.edge2, node.pt);
        SwapPositionsInAEL(Clipper, node.edge1, node.edge2);

        node.edge1->curr_x = node.pt.x;
        node.edge2->curr_x = node.pt.x;
        CheckJoinLeft(Clipper, node.edge2, node.pt, true);
        CheckJoinRight(Clipper, node.edge1, node.pt, true);
    }
}

inline void
DoIntersections(clipper *Clipper, s64 top_y)
{
    if(BuildIntersectList(Clipper, top_y))
    {
        ProcessIntersectList(Clipper);
        Clipper->IntersectNodeCount = 0;
    }
}

inline clipper_active *
GetMaximaPair(clipper_active *e)
{
    clipper_active *e2;
    e2 = e->next_in_ael;
    while(e2)
    {
        if (e2->vertex_top == e->vertex_top)
            return e2;  // Found!

        e2 = e2->next_in_ael;
    }

    return nullptr;
}

inline clipper_active *
DoMaxima(clipper *Clipper, clipper_active *e)
{
    clipper_active *next_e;
    clipper_active *prev_e;
    clipper_active *max_pair;

    prev_e = e->prev_in_ael;
    next_e = e->next_in_ael;
    if(IsOpenEnd(e))
    {
        if(IsHotEdge(e))
            AddOutPt(Clipper, e, e->top);

        if (!IsHorizontal(e))
        {
            if (IsHotEdge(e))
            {
                output_rectangle *outrec = GetOutRec(Clipper, e->outrecIndex);
                if (IsFront(Clipper, e))
                    outrec->FrontEdge = 0;
                else
                    outrec->BackEdge = 0;
                e->outrecIndex = 0;
            }

            DeleteFromAEL(Clipper, e);
        }
        return next_e;
    }

    max_pair = GetMaximaPair(e);
    if (!max_pair) return next_e;  // eMaxPair is horizontal

    if (IsJoined(e))
        Split(Clipper, e, e->top);
    if (IsJoined(max_pair))
        Split(Clipper, max_pair, max_pair->top);

    //only non-horizontal maxima here.
    //process any edges between maxima pair ...
    while (next_e != max_pair)
    {
        IntersectEdges(Clipper, e, next_e, e->top);
        SwapPositionsInAEL(Clipper, e, next_e);
        next_e = e->next_in_ael;
    }

    if (IsOpen(e))
    {
        if (IsHotEdge(e))
            AddLocalMaxPoly(Clipper, e, max_pair, e->top);
        DeleteFromAEL(Clipper, max_pair);
        DeleteFromAEL(Clipper, e);
        return (prev_e ? prev_e->next_in_ael : Clipper->ActiveEdgeList);
    }

    // e.next_in_ael== max_pair ...
    if (IsHotEdge(e))
        AddLocalMaxPoly(Clipper, e, max_pair, e->top);

    DeleteFromAEL(Clipper, e);
    DeleteFromAEL(Clipper, max_pair);
    return (prev_e ? prev_e->next_in_ael : Clipper->ActiveEdgeList);
}

inline void
DoTopOfScanbeam(clipper *Clipper, s64 y)
{
    Clipper->StoredEdgeList = 0;  // StoredEdgeList is reused to flag horizontals (see PushHorz below)
    clipper_active *e = Clipper->ActiveEdgeList;
    while(e)
    {
        //nb: 'e' will never be horizontal here
        if (e->top.y == y)
        {
            e->curr_x = e->top.x;
            if(IsMaxima(e))
            {
                e = DoMaxima(Clipper, e);  // TOP OF BOUND (MAXIMA)
                continue;
            }
            else
            {
                //INTERMEDIATE VERTEX ...
                if(IsHotEdge(e))
                    AddOutPt(Clipper, e, e->top);

                UpdateEdgeIntoAEL(Clipper, e);

                if (IsHorizontal(e))
                    PushHorz(Clipper, e);  // horizontals are processed later
            }
        }
        else // i.e. not the top of the edge
            e->curr_x = TopX(*e, y);

        e = e->next_in_ael;
    }
}

inline void
FixOutRecPts(output_rectangle *outrec)
{
    output_point *op = outrec->Points;
    do {
        op->OutRectIndex = outrec->Index;
        op = op->Next;
    } while (op != outrec->Points);
}

inline void
ProcessHorzJoins(clipper *Clipper)
{

    for(u32 I = 0;
        I < Clipper->JointCount;
        ++I)
    {
        horz_join *j = Clipper->HorzJoinList + I;
        output_rectangle *or1 = GetOutRec(Clipper, GetRealOutRec(Clipper, GetOutRec(Clipper, j->op1->OutRectIndex)));
        output_rectangle *or2 = GetOutRec(Clipper, GetRealOutRec(Clipper, GetOutRec(Clipper, j->op2->OutRectIndex)));

        output_point *op1b = j->op1->Next;
        output_point *op2b = j->op2->Prev;
        j->op1->Next = j->op2;
        j->op2->Prev = j->op1;
        op1b->Prev = op2b;
        op2b->Next = op1b;

        if (or1 == or2) // 'join' is really a split
        {
            or2 = GetOutRec(Clipper, NewOutRec(Clipper));
            or2->Points = op1b;
            FixOutRecPts(or2);

            //if or1->pts has moved to or2 then update or1->pts!!
            if(or1->Points->OutRectIndex == or2->Index)
            {
                or1->Points = j->op1;
                or1->Points->OutRectIndex = or1->Index;
            }

                or2->OwnerIndex = or1->Index;
        }
        else
        {
            or2->Points = 0;
            or2->OwnerIndex = or1->Index;
        }
    }
}

internal b32
ExecuteInternal(clipper *Clipper, clip_type ClipType, fill_rule FillRule)
{
    Clipper->ClipType = ClipType;
    Clipper->FillRule = FillRule;

    ResetClipper(Clipper);

    s64 y = 0;
    if((ClipType == ClipType_NoClip) || !PopScanline(Clipper, &y))
        return true;

    while(Clipper->Succeeded)
    {
        InsertLocalMinimaIntoAEL(Clipper, y);

        clipper_active *e = 0;
        while (PopHorz(Clipper, &e))
            DoHorizontal(Clipper, e);

        if(Clipper->HorzCount > 0)
        {
            ConvertHorzSegsToJoins(Clipper);
            Clipper->HorzCount = 0;
        }

        Clipper->BottomY = y;  // bot_y_ == bottom of scanbeam
        if (!PopScanline(Clipper, &y))
            break;  // y new top of scanbeam

        DoIntersections(Clipper, y);
        DoTopOfScanbeam(Clipper, y);

        while(PopHorz(Clipper, &e))
            DoHorizontal(Clipper, e);
    }

    if (Clipper->Succeeded)
        ProcessHorzJoins(Clipper);

    b32 Result = Clipper->Succeeded;
    return(Result);
}

inline b32
PtsReallyClose(v2_s64 pt1, v2_s64 pt2)
{
    b32 Result = (llabs(pt1.x - pt2.x) < 2) && (llabs(pt1.y - pt2.y) < 2);
    return(Result);
}

inline b32
IsVerySmallTriangle(output_point *op)
{
    b32 Result = ( op->Next->Next == op->Prev &&
                   (PtsReallyClose(op->Prev->P, op->Next->P) ||
                    PtsReallyClose(op->P, op->Next->P) ||
                    PtsReallyClose(op->P, op->Prev->P)));
    return(Result);
}

internal b32
BuildPathD(output_point *op, b32 reverse, b32 isOpen, path_f64 *path, f64 inv_scale)
{
    if (!op || op->Next == op || (!isOpen && op->Next == op->Prev))
        return false;

    *path = GetPathF64(BASIC_ALLOCATE_COUNT);

    v2_s64 lastPt;
    output_point *op2;
    if(reverse)
    {
        lastPt = op->P;
        op2 = op->Prev;
    }
    else
    {
        op = op->Next;
        lastPt = op->P;
        op2 = op->Next;
    }

    path->Points[path->Count++] = V2F64(lastPt.x * inv_scale, lastPt.y * inv_scale);

    while (op2 != op)
    {
        if(!PointsAreEqual(op2->P, lastPt))
        {
            lastPt = op2->P;

            if(NeedIncrease(path->Count))
            {
                IncreasePathF64(path);
            }

            path->Points[path->Count++] = V2F64(lastPt.x * inv_scale, lastPt.y * inv_scale);
        }

        if (reverse)
            op2 = op2->Prev;
        else
            op2 = op2->Next;
    }

    if ((path->Count == 3) && IsVerySmallTriangle(op2))
    {
#if RECORD_MEMORY_USEAGE
        Free(path->Points, sizeof(v2_f64)*path->AllocateCount);
        PathMemoryUsed -= sizeof(v2_f64)*path->AllocateCount;
#else
        Free(path->Points, sizeof(v2_f64)*path->AllocateCount);
#endif
        return false;
    }

    return true;
}

inline b32
IsValidClosedPath(output_point *op)
{
    return op && (op->Next != op) && (op->Next != op->Prev) &&
        !IsVerySmallTriangle(op);
}

inline void
DisposeOutPts(clipper *Clipper, output_rectangle *outrec)
{
    output_point *op = outrec->Points;
    op->Prev->Next = 0;
    while(op)
    {
        output_point *tmp = op;
        op = op->Next;

        FreeOutPt(Clipper, tmp);
    };

    outrec->Points = 0;
}

inline output_point *
DisposeOutPt(clipper *Clipper, output_point *op)
{
    output_point *result = op->Next;
    op->Prev->Next = op->Next;
    op->Next->Prev = op->Prev;

    FreeOutPt(Clipper, op);

    return result;
}

inline f64
Area(output_point *op)
{
    //https://en.wikipedia.org/wiki/Shoelace_formula
    f64 result = 0.0;
    output_point *op2 = op;
    do
    {
        result += ((f64)(op2->Prev->P.y + op2->P.y) *
                   (f64)(op2->Prev->P.x - op2->P.x));

        op2 = op2->Next;
    } while (op2 != op);

    return result * 0.5;
}

inline f64
AreaTriangle(v2_s64 pt1, v2_s64 pt2, v2_s64 pt3)
{
    return ((f64)(pt3.y + pt1.y) * (f64)(pt3.x - pt1.x) +
            (f64)(pt1.y + pt2.y) * (f64)(pt1.x - pt2.x) +
            (f64)(pt2.y + pt3.y) * (f64)(pt2.x - pt3.x));
}

internal void
DoSplitOp(clipper *Clipper, output_rectangle *outrec, output_point *splitOp)
{
    // splitOp.prev -> splitOp &&
    // splitOp.next -> splitOp.next.next are intersecting
    output_point *prevOp = splitOp->Prev;
    output_point *nextNextOp = splitOp->Next->Next;
    outrec->Points = prevOp;

    v2_s64 ip;
    GetSegmentIntersectPt(prevOp->P, splitOp->P,
                          splitOp->Next->P, nextNextOp->P, &ip);

    f64 area1 = Area(outrec->Points);
    f64 absArea1 = fabs(area1);
    if (absArea1 < 2)
    {
        DisposeOutPts(Clipper, outrec);
        return;
    }

    f64 area2 = AreaTriangle(ip, splitOp->P, splitOp->Next->P);
    f64 absArea2 = fabs(area2);

    // de-link splitOp and splitOp.next from the path
    // while inserting the intersection point
    if(PointsAreEqual(ip, prevOp->P) || PointsAreEqual(ip, nextNextOp->P))
    {
        nextNextOp->Prev = prevOp;
        prevOp->Next = nextNextOp;
    }
    else
    {
        output_point *newOp2 = GetOutPt(Clipper, ip, prevOp->OutRectIndex);
        newOp2->Prev = prevOp;
        newOp2->Next = nextNextOp;
        nextNextOp->Prev = newOp2;
        prevOp->Next = newOp2;
    }

    // area1 is the path's area *before* splitting, whereas area2 is
    // the area of the triangle containing splitOp & splitOp.next.
    // So the only way for these areas to have the same sign is if
    // the split triangle is larger than the path containing prevOp or
    // if there's more than one self-intersection.
    if (absArea2 >= 1 &&
        (absArea2 > absArea1 || (area2 > 0) == (area1 > 0)))
    {
        output_rectangle *newOr = GetOutRec(Clipper, NewOutRec(Clipper));
        newOr->OwnerIndex = outrec->OwnerIndex;

        splitOp->OutRectIndex = newOr->Index;
        splitOp->Next->OutRectIndex = newOr->Index;

        output_point *newOp = GetOutPt(Clipper, ip, newOr->Index);
        newOp->Prev = splitOp->Next;
        newOp->Next = splitOp;
        newOr->Points = newOp;
        splitOp->Prev = newOp;
        splitOp->Next->Next = newOp;
    }
    else
    {
        Free(splitOp->Next, sizeof(output_point));
        Free(splitOp, sizeof(output_point));
    }
}

internal void
FixSelfIntersects(clipper *Clipper, output_rectangle *outrec)
{
     

    output_point *op2 = outrec->Points;
    for (; ; )
    {
        // triangles can't self-intersect
        if (op2->Prev == op2->Next->Next)
            break;

        if(SegmentsIntersect(op2->Prev->P,
                             op2->P, op2->Next->P, op2->Next->Next->P))
        {
            if (op2 == outrec->Points || op2->Next == outrec->Points)
                outrec->Points = outrec->Points->Prev;
            DoSplitOp(Clipper, outrec, op2);
            if (!outrec->Points)
                break;

            op2 = outrec->Points;
            continue;
        }
        else
            op2 = op2->Next;

        if (op2 == outrec->Points)
            break;
    }
}

internal void
CleanCollinear(clipper *Clipper, output_rectangle *outrec)
{
    outrec = GetOutRec(Clipper, GetRealOutRec(Clipper, outrec));
    if (!outrec || outrec->IsOpen)
        return;

    if (!IsValidClosedPath(outrec->Points))
    {
        DisposeOutPts(Clipper, outrec);
        return;
    }

    output_point *startOp = outrec->Points;
    output_point *op2 = startOp;
    for (; ; )
    {
        //NB if preserveCollinear == true, then only remove 180 deg. spikes
        if (IsCollinear(op2->Prev->P, op2->P, op2->Next->P) &&
            (PointsAreEqual(op2->P, op2->Prev->P) ||
             PointsAreEqual(op2->P, op2->Next->P) || !Clipper->PreserveCollinear ||
             Inner(op2->Prev->P, op2->P, op2->Next->P) < 0))
        {

            if (op2 == outrec->Points)
                outrec->Points = op2->Prev;

            op2 = DisposeOutPt(Clipper, op2);
            if (!IsValidClosedPath(op2))
            {
                DisposeOutPts(Clipper, outrec);
                return;
            }
            startOp = op2;
            continue;
        }
        op2 = op2->Next;

        if (op2 == startOp)
            break;
    }

    FixSelfIntersects(Clipper, outrec);
}

inline u32
CountPathCount(clipper *Clipper)
{
    u32 Result = 0;
    for(u32 i = 1; i < Clipper->OutputRectCount; ++i)
    {
        output_rectangle *outrec = Clipper->OutRecList + i;
        if(outrec->Points)
            ++Result;
    }

    return(Result);
}

internal void
BuildPathsD(clipper *Clipper, paths_f64 *solutionClosed, paths_f64 *solutionOpen)
{
    *solutionClosed = GetPathsF64(CountPathCount(Clipper));
    if(solutionOpen)
    {
        *solutionOpen = GetPathsF64(Clipper->OutputRectCount);
        solutionOpen->PathCount = Clipper->OutputRectCount;
    }

    // OutRecList.size() is not static here because
    // CleanCollinear below can indirectly add additional
    // OutRec (via FixOutRecPts)
    u32 PrevCount = Clipper->OutputRectCount;
    for(u32 i = 1; i < Clipper->OutputRectCount; ++i)
    {
        output_rectangle *outrec = Clipper->OutRecList + i;
        if (outrec->Points == nullptr)
            continue;

        path_f64 path;
        if(solutionOpen && outrec->IsOpen)
        {
            if (BuildPathD(outrec->Points, Clipper->ReverseSolution, true, &path, Clipper->InvScale))
                solutionOpen->Paths[solutionOpen->PathCount++] = path;
        }
        else
        {
            CleanCollinear(Clipper, outrec);
            //closed paths should always return a Positive orientation
            if (BuildPathD(outrec->Points, Clipper->ReverseSolution, false, &path, Clipper->InvScale))
            {
                solutionClosed->Paths[solutionClosed->PathCount++] = path;
            }
        }

        if(PrevCount != Clipper->OutputRectCount)
        {
            PrevCount = Clipper->OutputRectCount;
            paths_f64 New = GetAndCopyPathsF64(solutionClosed,
                                               (Clipper->OutputRectCount - 1));

#if RECORD_MEMORY_USEAGE
            Free(solutionClosed->Paths, solutionClosed->AllocateCount*sizeof(path_s64));
            PathMemoryUsed -= solutionClosed->AllocateCount*sizeof(path_s64);
#else
            Free(solutionClosed->Paths, solutionClosed->AllocateCount*sizeof(path_s64));
#endif
            *solutionClosed = New;
        }
    }
}

inline void
DeleteEdges(clipper *Clipper)
{
    for(u32 I = 0;
        I < Clipper->DeallocActivesCount;
        ++I)
    {
        void *Address = Clipper->DeallocActivesList[I];
        Free(Address, sizeof(clipper_active)*BASIC_ALLOCATE_COUNT);
    }
}

inline void
DeleteEdges(clipper_active *e)
{
    while(e)
    {
        clipper_active *e2 = e;
        e = e->next_in_ael;
        Free(e2, sizeof(clipper_active));
    }
}

inline void
DisposeOutPtsCleanUp(clipper *Clipper)
{
    for(u32 I = 0;
        I < Clipper->DeallocOutPtsCount;
        ++I)
    {
        void *Address = Clipper->DeallocOutPtsList[I];
        Free(Address, sizeof(output_point)*BASIC_ALLOCATE_COUNT);
    }
}

inline void
DisposeAllOutRecs(clipper *Clipper)
{
    DisposeOutPtsCleanUp(Clipper);

#if RECORD_MEMORY_USEAGE
    Free(Clipper->OutRecList, ArrayMaxSizes[ArrayType_OutRec]*sizeof(output_rectangle));
#else
    Free(Clipper->OutRecList, 0);
#endif
}

void
CleanUp(clipper *Clipper)
{
    DeleteEdges(Clipper);

#if RECORD_MEMORY_USEAGE
    Free(Clipper->ScanLineMaxHeap.Nodes, Clipper->ScanLineMaxHeap.MaxSize*sizeof(clipper_sort_entry));
    Free(Clipper->IntersectNodes, ArrayMaxSizes[ArrayType_IntersectNode]*sizeof(intersect_node));

    DisposeAllOutRecs(Clipper);

    Free(Clipper->HorzJoinList, ArrayMaxSizes[ArrayType_HorzJoinList]*sizeof(horz_join));
    Free(Clipper->HorzSegList, ArrayMaxSizes[ArrayType_HorzSegList]*sizeof(horz_segment));

    Free(Clipper->MinimaList, ArrayMaxSizes[ArrayType_MinimaList]*sizeof(local_minima));
    for(u32 I = 0;
        I < Clipper->VertexListCount;
        ++I)
    {
        clipper_vertex *List = Clipper->VertexLists[I].Vertices;
        Free(List, Clipper->VertexLists[I].VertexCount*sizeof(clipper_vertex));
    }

    Free(Clipper->VertexLists, ArrayMaxSizes[ArrayType_VertexLists]*sizeof(vertex_list));

    Free(Clipper->DeallocActivesList, ArrayMaxSizes[ArrayType_DeallocateActivesList]*sizeof(void *));
    Free(Clipper->DeallocOutPtsList, ArrayMaxSizes[ArrayType_DeallocateOutPtsList]*sizeof(void *));
#else
    Free(Clipper->ScanLineMaxHeap.Nodes, 0);
    Free(Clipper->IntersectNodes, 0);

    Free(Clipper->HorzJoinList, 0);
    Free(Clipper->HorzSegList, 0);

    Free(Clipper->MinimaList, 0);
    for(u32 I = 0;
        I < Clipper->VertexListCount;
        ++I)
    {
        clipper_vertex *List = Clipper->VertexLists[I].Vertices;
        Free(List, Clipper->VertexLists[I].VertexCount*sizeof(clipper_vertex));
    }

    Free(Clipper->VertexLists, 0);
    Free(Clipper->DeallocActivesList, 0);
    Free(Clipper->DeallocOutPtsList, 0);
#endif
}

inline b32
Execute(clipper *Clipper, clip_type ClipType, fill_rule FillRule, paths_f64 *ClosedPaths, paths_f64 *OpenPaths)
{
    if(ExecuteInternal(Clipper, ClipType, FillRule))
    {
        BuildPathsD(Clipper, ClosedPaths, OpenPaths);
    }

    CleanUp(Clipper);

    b32 Result = Clipper->Succeeded;
    return(Result);
}

inline paths_f64
BooleanOp(clip_type ClipType, fill_rule FillRule,
          paths_f64 *Subjects, paths_f64 *Clips, s32 Precision = 2)
{
     

    CheckPrecisionRange(&Precision);
    
    paths_f64 Result = {};

    clipper Clipper = {};

    InitClipper(&Clipper, Precision);
    AddSubjects(&Clipper, Subjects);
    AddClips(&Clipper, Clips);

    Execute(&Clipper, ClipType, FillRule, &Result, 0);

    return(Result);
}


internal s32
BooleanOpD(u32 cliptype, u32 fillrule, paths_f64 *subjects, paths_f64 *subjects_open,
           paths_f64 *clips, paths_f64 *solution, paths_f64 *solution_open,
           b32 preserve_collinear, b32 reverse_solution)
{
    if (cliptype > ClipType_Xor)
    {
        Assert(!"Out of range");
        return -4;
    }

    if (fillrule > FillRule_Negative)
    {
        Assert(!"Out of range");
        return -3;
    }

    clipper Clipper = {};
    // NOTE(pvlso): 2 rounds points to 1/128 and 4 to 1/16384: the map editor's points are kept to 1/1000 of a cell
    InitClipper(&Clipper, 4);
    
    Clipper.PreserveCollinear = preserve_collinear;
    Clipper.ReverseSolution = reverse_solution;

    if(subjects->PathCount > 0)
        AddSubjects(&Clipper, subjects);

    if(subjects_open)
    {
        if(subjects_open->PathCount > 0)
            AddOpenSubjects(&Clipper, subjects_open);
    }

    if(clips->PathCount > 0)
        AddClips(&Clipper, clips);

    if(!Execute(&Clipper, (clip_type)cliptype, (fill_rule)fillrule, solution, solution_open))
    {
        Assert(!"clipping bug - should never happen :)");
        return -1; // clipping bug - should never happen :)
    }

    return 0; //success !!
}

// ===========================================================================================================================================================
// NOTE(pvlso): What the engine uses

internal paths_f64
ClipperPathsOf(clip_polygon_set *Set)
{
    u32 Count = Set ? Set->Count : 0;
    paths_f64 Result = GetPathsF64(Maximum(1, (s32)Count));
    Result.PathCount = (s32)Count;
    for(u32 PolygonIndex = 0; PolygonIndex < Count; ++PolygonIndex)
    {
        clip_polygon *Polygon = Set->Polygons + PolygonIndex;
        path_f64 *Path = Result.Paths + PolygonIndex;
        *Path = GetPathF64(Maximum(1, (s32)Polygon->Count));
        Path->Count = (s32)Polygon->Count;
        for(u32 PointIndex = 0; PointIndex < Polygon->Count; ++PointIndex)
        {
            Path->Points[PointIndex].x = Polygon->Points[PointIndex].x;
            Path->Points[PointIndex].y = Polygon->Points[PointIndex].y;
        }
    }

    return(Result);
}

// NOTE(pvlso): Subjects joined with, cut by, crossed with or xor'ed with Clips (Clips may be 0): the result's
// outlines and holes, in Arena. Clipper works in Arena too, so give it temporary memory and keep only the result.
internal clip_polygon_set
ClipPolygons(memory_arena *Arena, clip_type Type, clip_polygon_set *Subjects, clip_polygon_set *Clips,
             fill_rule Fill = FillRule_NonZero)
{
    memory_arena *SavedArena = ClipperArena;
    ClipperArena = Arena;

    paths_f64 SubjectPaths = ClipperPathsOf(Subjects);
    paths_f64 ClipPaths = ClipperPathsOf(Clips);
    paths_f64 Solution = {};
    clip_polygon_set Result = {};
    if(BooleanOpD(Type, Fill, &SubjectPaths, 0, &ClipPaths, &Solution, 0, false, false) == 0)
    {
        Result.Count = (u32)Solution.PathCount;
        Result.Polygons = PushArray(Arena, Maximum(1u, Result.Count), clip_polygon);
        for(u32 PolygonIndex = 0; PolygonIndex < Result.Count; ++PolygonIndex)
        {
            path_f64 *Path = Solution.Paths + PolygonIndex;
            clip_polygon *Polygon = Result.Polygons + PolygonIndex;
            Polygon->Count = (u32)Path->Count;
            Polygon->Points = PushArray(Arena, Maximum(1u, Polygon->Count), v2);
            for(u32 PointIndex = 0; PointIndex < Polygon->Count; ++PointIndex)
            {
                Polygon->Points[PointIndex] = V2((r32)Path->Points[PointIndex].x, (r32)Path->Points[PointIndex].y);
            }
        }
    }
    else
    {
        LOG_ERROR("Clipper couldn't do the operation");
    }

    ClipperArena = SavedArena;
    return(Result);
}

#if EDITOR_SLOW
// NOTE(pvlso): Run once at startup in debug builds: two overlapping squares join into one outline, a square with a
// smaller one cut out of its middle is an outline and a hole, two rectangles sharing an edge join into one, and two
// that share edges around a gap leave an outline and a hole (the map editor's snapped areas meet like these)
internal void
TestClipper(memory_arena *Arena)
{
    temporary_memory Temp = BeginTemporaryMemory(Arena);
    v2 SquareA[] = {V2(0, 0), V2(4, 0), V2(4, 4), V2(0, 4)};
    v2 SquareB[] = {V2(2, 2), V2(6, 2), V2(6, 6), V2(2, 6)};
    v2 Inner[] = {V2(1, 1), V2(3, 1), V2(3, 3), V2(1, 3)};
    clip_polygon PolygonA = {ArrayCount(SquareA), SquareA};
    clip_polygon PolygonB = {ArrayCount(SquareB), SquareB};
    clip_polygon PolygonInner = {ArrayCount(Inner), Inner};
    clip_polygon Both[] = {PolygonA, PolygonB};
    clip_polygon_set Subjects = {ArrayCount(Both), Both};
    clip_polygon_set Outer = {1, &PolygonA};
    clip_polygon_set Hole = {1, &PolygonInner};

    v2 TopHalf[] = {V2(0, 0), V2(10, 0), V2(10, 5), V2(0, 5)};
    v2 LowHalf[] = {V2(0, 5), V2(10, 5), V2(10, 10), V2(0, 10)};
    clip_polygon Halves[] = {{ArrayCount(TopHalf), TopHalf}, {ArrayCount(LowHalf), LowHalf}};
    clip_polygon_set HalvesSet = {ArrayCount(Halves), Halves};

    v2 TopGap[] = {V2(0, 0), V2(20, 0), V2(20, 5), V2(12, 5), V2(12, 3), V2(8, 3), V2(8, 5), V2(0, 5)};
    v2 LowGap[] = {V2(0, 5), V2(8, 5), V2(8, 7), V2(12, 7), V2(12, 5), V2(20, 5), V2(20, 10), V2(0, 10)};
    clip_polygon Gaps[] = {{ArrayCount(TopGap), TopGap}, {ArrayCount(LowGap), LowGap}};
    clip_polygon_set GapsSet = {ArrayCount(Gaps), Gaps};

    clip_polygon_set Joined = ClipPolygons(Arena, ClipType_Union, &Subjects, 0);
    clip_polygon_set Cut = ClipPolygons(Arena, ClipType_Difference, &Outer, &Hole);
    clip_polygon_set Shared = ClipPolygons(Arena, ClipType_Union, &HalvesSet, 0);
    clip_polygon_set AroundGap = ClipPolygons(Arena, ClipType_Union, &GapsSet, 0);
    b32 Passed = ((Joined.Count == 1) && (Joined.Polygons[0].Count == 8) && (Cut.Count == 2) &&
                  (Shared.Count == 1) && (Shared.Polygons[0].Count == 4) &&
                  (AroundGap.Count == 2) && (AroundGap.Polygons[0].Count == 4) && (AroundGap.Polygons[1].Count == 4));
    if(Passed)
    {
        LOG("Clipper self-test passed: unions, shared edges and a difference came out right");
    }
    else
    {
        LOG_ERROR("Clipper self-test failed: union %u polygons (%u points), difference %u polygons, shared edge %u "
                  "polygons (%u points), around a gap %u polygons", Joined.Count,
                  Joined.Count ? Joined.Polygons[0].Count : 0, Cut.Count, Shared.Count,
                  Shared.Count ? Shared.Polygons[0].Count : 0, AroundGap.Count);
    }
    EndTemporaryMemory(Temp);
}
#endif

#if !defined(SPELLWEAVER_SHARED_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

struct heap
{
    sort_entry *Nodes;
    u32 MaxSize;
    u32 Size;
};

internal void
MinHeapifyDown(heap *Heap, u32 Index)
{
    u32 Smallest = Index;
    u32 Left = 2*Index + 1;
    u32 Right = 2*Index + 2;

    if((Left < Heap->Size) && (Heap->Nodes[Left].SortKey < Heap->Nodes[Smallest].SortKey))
    {
        Smallest = Left;
    }

    if((Right < Heap->Size) && (Heap->Nodes[Right].SortKey < Heap->Nodes[Smallest].SortKey))
    {
        Smallest = Right;
    }

    if(Smallest != Index)
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Smallest);
        MinHeapifyDown(Heap, Smallest);
    }
}

internal void
MaxHeapifyDown(heap *Heap, u32 Index)
{
    u32 Largest = Index;
    u32 Left = 2*Index + 1;
    u32 Right = 2*Index + 2;

    if((Left < Heap->Size) && (Heap->Nodes[Left].SortKey > Heap->Nodes[Largest].SortKey))
    {
        Largest = Left;
    }

    if((Right < Heap->Size) && (Heap->Nodes[Right].SortKey > Heap->Nodes[Largest].SortKey))
    {
        Largest = Right;
    }

    if(Largest != Index)
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Largest);
        MaxHeapifyDown(Heap, Largest);
    }
}

internal void
MinHeapifyUp(heap *Heap, u32 Index)
{
    u32 Parent = (Index - 1) / 2;

    if((Index) && (Heap->Nodes[Parent].SortKey > Heap->Nodes[Index].SortKey))
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Parent);
        MinHeapifyUp(Heap, Parent);
    }
}

internal void
MaxHeapifyUp(heap *Heap, u32 Index)
{
    u32 Parent = (Index - 1) / 2;

    if((Index) && (Heap->Nodes[Parent].SortKey < Heap->Nodes[Index].SortKey))
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Parent);
        MaxHeapifyUp(Heap, Parent);
    }
}

internal void
MinHeapInsertNode(heap *Heap, sort_entry Key)
{
    Assert(Heap->Size != Heap->MaxSize);

    Heap->Nodes[Heap->Size] = Key;
    ++Heap->Size;
    MinHeapifyUp(Heap, Heap->Size - 1);
}

internal void
MaxHeapInsertNode(heap *Heap, sort_entry Key)
{
    Assert(Heap->Size != Heap->MaxSize);

    Heap->Nodes[Heap->Size] = Key;
    ++Heap->Size;
    MaxHeapifyUp(Heap, Heap->Size - 1);
}

internal sort_entry
MinHeapExtractNode(heap *Heap)
{
    Assert(Heap->Size > 0);

    sort_entry Result = {};
    Result = Heap->Nodes[0];
    Heap->Nodes[0] = Heap->Nodes[Heap->Size - 1];
    --Heap->Size;

    MinHeapifyDown(Heap, 0);

    return(Result);
}

internal sort_entry
MaxHeapExtractNode(heap *Heap)
{
    Assert(Heap->Size > 0);

    sort_entry Result = {};
    Result = Heap->Nodes[0];
    Heap->Nodes[0] = Heap->Nodes[Heap->Size - 1];
    --Heap->Size;

    MaxHeapifyDown(Heap, 0);

    return(Result);
}

internal b32
InsidePolygon(v2 Point, polygon2 *Polygon, random_series *Series, r32 Tolerance = 0.0f)
{
    b32 Result = false;

    /* should assert p->n > 1 */
    r32 min_x, max_x, min_y, max_y;

    // NOTE(paul): Check if point is on the edge
    b32 IsOnEdge = false;
    for(s32 I = 0;
         I < Polygon->VertexCount;
         I++)
    {
        u32 k = (I + 1) % Polygon->VertexCount;
        min_x = Distance(Point, Polygon->Vertices[I], Polygon->Vertices[k]);
        if(min_x < 0.0f)
        {
            IsOnEdge = true;
        }
    }

    if(!IsOnEdge)
    {
        max_x = min_x = Polygon->Vertices[0].x;
        max_y = min_y = Polygon->Vertices[1].y;

        // NOTE(paul): Check if point is in poligon box
        /* calculate extent of polygon */
        for(s32 I = 0;
            I < Polygon->VertexCount;
            ++I)
        {
            v2 *Vertex = Polygon->Vertices + I;
        
            if (Vertex->x > max_x) max_x = Vertex->x;
            if (Vertex->x < min_x) min_x = Vertex->x;
            if (Vertex->y > max_y) max_y = Vertex->y;
            if (Vertex->y < min_y) min_y = Vertex->y;
        }

        if((Point.x > min_x) && (Point.x < max_x) &&
           (Point.y > min_y) && (Point.y < max_y))
        {
            max_x -= min_x;
            max_x *= 2;
            max_y -= min_y;
            max_y *= 2;
            max_x += max_y;

            v2 e;
            while(1)
            {
                s32 Count = 0;
                u32 Crosses = 0;

                /* pick a rand point far enough to be outside polygon */
                e.x = Point.x + (1.0f + RandomUnilateral(Series)) * max_x;
                e.y = Point.y + (1.0f + RandomUnilateral(Series)) * max_y;

                for (s32 I = 0;
                     I < Polygon->VertexCount;
                     ++I, ++Count)
                {
                    u32 k = (I + 1) % Polygon->VertexCount;
                    s32 Intersect = LineIntersect(Point, e, Polygon->Vertices[I],
                                                  Polygon->Vertices[k], 0);
                    
                    if(Intersect == 1)
                    {
                        Crosses++;
                    }
                }

                if(Count == Polygon->VertexCount)
                {
                    Result = (Crosses & 1) ? true : false;
                    break;
                }
            }
        }
    }    

    return Result;
}

#define SPELLWEAVER_SHARED_H
#endif

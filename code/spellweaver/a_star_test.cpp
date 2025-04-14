/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
#if 0
internal void
MinHeapifyDown(min_heap *Heap, u32 Index)
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
MinHeapifyUp(min_heap *Heap, u32 Index)
{
    u32 Parent = (Index - 1) / 2;

    if((Index) && (Heap->Nodes[Parent].SortKey > Heap->Nodes[Index].SortKey))
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Parent);
        MinHeapifyUp(Heap, Parent);
    }
}

internal void
MinHeapInsertNode(min_heap *Heap, sort_entry Key)
{
    Assert(Heap->Size != Heap->MaxSize);

    Heap->Nodes[Heap->Size] = Key;
    ++Heap->Size;
    MinHeapifyUp(Heap, Heap->Size - 1);
}

internal sort_entry
MinHeapExtractNode(min_heap *Heap)
{
    Assert(Heap->Size > 0);

    sort_entry Result = {};
    Result = Heap->Nodes[0];
    Heap->Nodes[0] = Heap->Nodes[Heap->Size - 1];
    --Heap->Size;

    MinHeapifyDown(Heap, 0);

    return(Result);
}
#endif

internal void
FindNeighbors(game_mode_a_star_test *AStarTest, as_node *Node)
{
    s32 X = Node->X;
    s32 Y = Node->Y;

    
    s32 Indecies[8][2] =
        {
            {Y, X - 1},
            {Y, X + 1},
            {Y - 1, X},
            {Y + 1, X},
            {Y + 1, X + 1},
            {Y - 1, X - 1},
            {Y + 1, X - 1},
            {Y - 1, X + 1},
        };

    for(u32 I = 0;
        I < ArrayCount(Indecies);
        ++I)
    {
        s32 IndexX = Indecies[I][1];
        s32 IndexY = Indecies[I][0];

        if((IndexX >= 0) && (IndexX < 32) &&
           (IndexY >= 0) && (IndexY < 32))
        {
            s32 Index = IndexY*32 + IndexX;
            Node->Neighbours[I] = AStarTest->Nodes + Index;
        }
    }
}

inline r32
DistanceBetween(as_node *NodeA, as_node *NodeB)
{
    r32 X = (r32)(NodeA->X - NodeB->X);
    r32 Y = (r32)(NodeA->Y - NodeB->Y);

    r32 Result = SquareRoot(Square(X) + Square(Y));

    return(Result);
}

internal void
SolveAStar(game_mode_a_star_test *AStarTest, u32 GridDim)
{
    TIMED_FUNCTION();

    if(AStarTest->StartNode && AStarTest->EndNode)
    {
        for(u32 NodeIndex = 0;
            NodeIndex < AStarTest->NodeCount;
            ++NodeIndex)
        {
            as_node *Node = AStarTest->Nodes + NodeIndex;
            Node->Visited = false;
            Node->GlobalGoal = Real32Maximum;
            Node->LocalGoal = Real32Maximum;
            Node->Parent = 0;
        }

        as_node *CurrentNode = AStarTest->StartNode;
        CurrentNode->LocalGoal = 0.0f;
        CurrentNode->GlobalGoal = DistanceBetween(AStarTest->StartNode, AStarTest->EndNode);

        heap *Heap = &AStarTest->Heap;

        sort_entry Key = {};
        Key.Index = AStarTest->StartNode->Y*GridDim + AStarTest->StartNode->X;
        Key.SortKey = AStarTest->StartNode->GlobalGoal;
        MinHeapInsertNode(Heap, Key);

        while((Heap->Size != 0) && (CurrentNode != AStarTest->EndNode))
        {
            as_node *TestNode = AStarTest->Nodes + Heap->Nodes[0].Index;
            while((TestNode->Visited) && (Heap->Size != 0))
            {
                MinHeapExtractNode(Heap);
                TestNode = AStarTest->Nodes + Heap->Nodes[0].Index;
            }

            if(Heap->Size == 0)
            {
                break;
            }

            CurrentNode = AStarTest->Nodes + Heap->Nodes[0].Index; 
            CurrentNode->Visited = true;

            for(u32 NeighbourIndex = 0;
                NeighbourIndex < ArrayCount(CurrentNode->Neighbours);
                ++NeighbourIndex)
            {
                as_node *NeighbourNode = CurrentNode->Neighbours[NeighbourIndex];
                if(NeighbourNode)
                {
                    if((!NeighbourNode->Visited) && (!NeighbourNode->Obstacle))
                    {
                        sort_entry Key = {};
                        Key.Index = NeighbourNode->Y*GridDim + NeighbourNode->X;
                        Key.SortKey = NeighbourNode->GlobalGoal;

                        r32 LowerGoal = CurrentNode->LocalGoal + DistanceBetween(CurrentNode, NeighbourNode);
                        if(LowerGoal < NeighbourNode->LocalGoal)
                        {
                            NeighbourNode->Parent = CurrentNode;
                            NeighbourNode->LocalGoal = LowerGoal;

                            NeighbourNode->GlobalGoal = (NeighbourNode->LocalGoal +
                                                         DistanceBetween(NeighbourNode, AStarTest->EndNode));
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
    
}

internal b32
UpdateAndRenderAStarTest(game_state *GameState, transient_state *TranState, render_group *RenderGroup,
                         game_input *Input, game_mode_a_star_test *AStarTest, loaded_bitmap *DrawBuffer)
{
    b32 Result = CheckForMetaInput(GameState, TranState, Input);
    if(!Result)
    {
        Orthographic(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, 1.0f);
        Clear(RenderGroup, V4(0.5f, 0.5f, 0.5f, 1.0f));

        object_transform Transform = DefaultFlatTransform();
        
        v2 MouseP = V2(Input->MouseX, Input->MouseY);
        v3 LocalMouseP = Unproject(RenderGroup, Transform, MouseP);

        r32 NodeSize = 25.0f;
        r32 RectDim = (NodeSize)*32.0f + 31.0f*7.0f;

        r32 RectHalfDim = 0.5f*RectDim;
        r32 NodeSpacing = 7.0f;

        SolveAStar(AStarTest, 32);
        for(u32 NodeIndex = 0;
            NodeIndex < AStarTest->NodeCount;
            ++NodeIndex)
        {
            as_node *Node = AStarTest->Nodes + NodeIndex;
            FindNeighbors(AStarTest, Node);

            v4 Color = V4(0, 0, 1, 1);

            v2 P = V2(-RectHalfDim + 0.5f*NodeSize, -RectHalfDim + 0.5f*NodeSize) +
                V2(Node->X*(NodeSize + NodeSpacing), Node->Y*(NodeSize + NodeSpacing));

            rectangle2 NodeRect = RectCenterDim(P, V2(NodeSize, NodeSize));
            if(IsInRectangle(NodeRect, LocalMouseP.xy))
            {
                if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
                {
                    if(Input->ShiftDown)
                    {
                        AStarTest->StartNode = Node;
                    }
                    else if(Input->ControlDown)
                    {
                        AStarTest->EndNode = Node;
                    }
                    else if((Node != AStarTest->StartNode) && (Node != AStarTest->EndNode))
                    {
                        Node->Obstacle = !Node->Obstacle;
                    }
                }
            }

            for(u32 NIndex = 0;
                NIndex < ArrayCount(Node->Neighbours);
                ++NIndex)
            {
                as_node *NeighborNode = Node->Neighbours[NIndex];
                if(NeighborNode)
                {
                    v2 NP = V2(-RectHalfDim + 0.5f*NodeSize, -RectHalfDim + 0.5f*NodeSize) +
                        V2(NeighborNode->X*(NodeSize + NodeSpacing), NeighborNode->Y*(NodeSize + NodeSpacing));
                    PushLine(RenderGroup, DefaultFlatTransform(), V3(NP, 0), V3(P, 0), V4(0, 0, 1, 1));
                }
            }
            
            if(Node->Obstacle)
            {
                Color = V4(0.1f, 0.1f, 0.1f, 1);
            }

            if(Node->Visited)
            {
                Color = V4(1, 0, 1, 1);
            }

            if(Node == AStarTest->StartNode)
            {
                Color = V4(0, 1, 0, 1);
            }

            if(Node == AStarTest->EndNode)
            {
                Color = V4(1, 0, 0, 1);
            }
            
            Transform.OffsetP = V3(P, 0);
            PushRect(RenderGroup, Transform, V3(0, 0, 1.0f), V2(NodeSize, NodeSize), Color);
        }

        if(AStarTest->EndNode)
        {
            as_node *Node = AStarTest->EndNode;
            while(Node->Parent)
            {
                v2 P = V2(-RectHalfDim + 0.5f*NodeSize, -RectHalfDim + 0.5f*NodeSize) +
                    V2(Node->X*(NodeSize + NodeSpacing), Node->Y*(NodeSize + NodeSpacing));

                v2 NP = V2(-RectHalfDim + 0.5f*NodeSize, -RectHalfDim + 0.5f*NodeSize) +
                    V2(Node->Parent->X*(NodeSize + NodeSpacing), Node->Parent->Y*(NodeSize + NodeSpacing));
                PushLine(RenderGroup, DefaultFlatTransform(), V3(NP, 3.0f), V3(P, 3.0f), V4(1, 1, 0, 1));

                Node = Node->Parent;
            }
        }
        
        Transform.OffsetP = V3(0, 0, 0);
        PushRectOutline(RenderGroup, Transform, V3(0, 0, 0), V2(RectDim, RectDim), V4(1, 0, 0, 1), 1.0f);
    }

    return(Result);
}

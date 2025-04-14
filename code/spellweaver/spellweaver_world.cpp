/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

inline world_position
NullPosition(void)
{
    world_position Result = {};

    Result.TileX = TILE_CHUNK_UNINITIALIZED;

    return(Result);
}

inline bool32
IsValid(world_position P)
{
    bool32 Result = (P.TileX != TILE_CHUNK_UNINITIALIZED);
    return(Result);
}

inline bool32
IsCanonical(real32 TileDim, real32 TileRel)
{
    // TODO(casey): Fix floating point math so this can be exact?
    real32 Epsilon = 0.01f;
    bool32 Result = ((TileRel >= -(0.5f*TileDim + Epsilon)) &&
                     (TileRel <= (0.5f*TileDim + Epsilon)));

    return(Result);
}

inline bool32
IsCanonical(world *World, v2 Offset)
{
    bool32 Result = (IsCanonical(World->TileSideInMeters, Offset.x) &&
                     IsCanonical(World->TileSideInMeters, Offset.y));

    return(Result);
}

inline bool32
AreInSameTile(world *World, world_position *A, world_position *B)
{
    Assert(IsCanonical(World, A->Offset));
    Assert(IsCanonical(World, B->Offset));
    
    bool32 Result = ((A->TileX == B->TileX) &&
                     (A->TileY == B->TileY));

    return(Result);
}

inline void
ClearWorldEntityBlock(world_entity_block *Block)
{
    Block->EntityCount = 0;
    Block->EntityDataSize = 0;
    Block->Next = 0;
}

inline world_chunk **
GetWorldChunkInternal(world *World, int32 ChunkX, int32 ChunkY)
{
    TIMED_FUNCTION();

    Assert(ChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY < TILE_CHUNK_SAFE_MARGIN);
        
    
    // TODO(casey): BETTER HASH FUNCTION!!!!
    uint32 HashValue = 19*ChunkX + 7*ChunkY;
    uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
    Assert(HashSlot < ArrayCount(World->ChunkHash));

    world_chunk **Chunk = &World->ChunkHash[HashSlot];
    while(*Chunk &&
          !((ChunkX == (*Chunk)->ChunkX) &&
            (ChunkY == (*Chunk)->ChunkY)))
    {
        Chunk = &(*Chunk)->NextInHash;
    }
    
    return(Chunk);
}

inline world_chunk *
GetWorldChunk(world *World, int32 TileX, int32 TileY, memory_arena *Arena = 0)
{
    TIMED_FUNCTION();

    int32 ChunkX = TileX / TILES_PER_CHUNK_DIM;
    int32 ChunkY = TileY / TILES_PER_CHUNK_DIM;
    
    world_chunk **ChunkPtr = GetWorldChunkInternal(World, ChunkX, ChunkY);
    world_chunk *Result = *ChunkPtr;
    if(!Result && Arena)
    {
        if(!World->FirstFreeChunk)
        {
            World->FirstFreeChunk = PushStruct(Arena, world_chunk, NoClear());
            World->FirstFreeChunk->NextInHash = 0;
        }

        Result = World->FirstFreeChunk;
        World->FirstFreeChunk = Result->NextInHash;

        Result->FirstBlock = 0;
        Result->ChunkX = ChunkX;
        Result->ChunkY = ChunkY;

        Result->NextInHash = *ChunkPtr;
        *ChunkPtr = Result;
    }

    return(Result);
}

internal world_chunk *
RemoveWorldChunk(world *World, int32 ChunkX, int32 ChunkY)
{
    TIMED_FUNCTION();

    world_chunk **ChunkPtr = GetWorldChunkInternal(World, ChunkX, ChunkY);
    world_chunk *Result = *ChunkPtr;
    if(Result)
    {
        *ChunkPtr = Result->NextInHash;
    }

    return(Result);
}

inline void
RecanonicalizeCoord(real32 ChunkDim, int32 *Tile, real32 *TileRel)
{
    // TODO(casey): Need to do something that doesn't use the divide/multiply method
    // for recanonicalizing because this can end up rounding back on to the tile
    // you just came from.

    // NOTE(casey): Wrapping IS NOT ALLOWED, so all coordinates are assumed to be
    // within the safe margin!
    // TODO(casey): Assert that we are nowhere near the edges of the world.
    
    int32 Offset = RoundReal32ToInt32(*TileRel / ChunkDim);
    *Tile += Offset;
    *TileRel -= Offset*ChunkDim;

    Assert(IsCanonical(ChunkDim, *TileRel));
}

inline world_position
MapIntoTileSpace(world *World, world_position BasePos, v2 Offset)
{
    world_position Result = BasePos;

    Result.Offset += Offset;
    RecanonicalizeCoord(World->TileSideInMeters, &Result.TileX, &Result.Offset.x);
    RecanonicalizeCoord(World->TileSideInMeters, &Result.TileY, &Result.Offset.y);
    
    return(Result);
}

inline world_position
TilePositionFromChunkPosition(world_position *Pos)
{
    world_position Result = {};

    u32 HalfChunkCount = TILES_PER_CHUNK_DIM / 2;
    Result.TileX = Pos->TileX*TILES_PER_CHUNK_DIM + HalfChunkCount;
    Result.TileY = Pos->TileY*TILES_PER_CHUNK_DIM + HalfChunkCount; 

    r32 TileX = Result.TileX + Pos->Offset.x;
    r32 TileY = Result.TileY + Pos->Offset.y;
    Result.TileX = (int32)TileX;
    Result.TileY = (int32)TileY;
    Result.Offset.x = TileX - Result.TileX;
    Result.Offset.y = TileY - Result.TileY;

    Result.Offset -= V2(0.5f, 0.5f);
    return(Result);
}

inline world_position
ChunkPositionFromTilePosition(world *World, int32 AbsTileX, int32 AbsTileY)
{
    world_position BasePos = {};
    
    v2 TileDim = V2(World->TileSideInMeters, World->TileSideInMeters);
    v2 Offset = Hadamard(TileDim, V2((real32)AbsTileX, (real32)AbsTileY));
    world_position Result = MapIntoTileSpace(World, BasePos, Offset);
    
    Assert(IsCanonical(World, Result.Offset));
    
    return(Result);
}

inline v2
Subtract(world *World, world_position *A, world_position *B)
{
    v2 dTile = {(real32)A->TileX - (real32)B->TileX,
                (real32)A->TileY - (real32)B->TileY};
    
    v2 Result = World->TileSideInMeters*dTile + (A->Offset - B->Offset);

    return(Result);
}

#if 0 
inline v2
Subtract(world *World, world_tile_position *A, world_tile_position *B)
{
    v2 dTile = {(real32)A->TileX - (real32)B->TileX,
                (real32)A->TileY - (real32)B->TileY};
    
    v2 Result = World->TileSideInMeters*dTile + (A->Offset - B->Offset);

    return(Result);
}
#endif

inline world_position
CenteredTilePoint(uint32 TileX, uint32 TileY)
{
    world_position Result = {};

    Result.TileX = TileX;
    Result.TileY = TileY;

    return(Result);
}

#if 0
inline world_position
CenteredChunkPoint(world_chunk *Chunk)

{
    world_position Result = CenteredChunkPoint(Chunk->ChunkX, Chunk->ChunkY);

    return(Result);
}
#endif

inline b32
HasRoomFor(world_entity_block *Block, u32 Size)
{
    b32 Result = ((Block->EntityDataSize + Size) <= sizeof(Block->EntityData));

    return(Result);
}

inline void
PackEntityIntoChunk(memory_arena *Arena, world *World, entity *Source, world_chunk *Chunk)
{
    TIMED_FUNCTION();
    u32 PackSize = sizeof(*Source);

    if(!Chunk->FirstBlock || !HasRoomFor(Chunk->FirstBlock, PackSize))
    {
        if(!World->FirstFreeBlock)
        {
            World->FirstFreeBlock = PushStruct(Arena, world_entity_block);
            World->FirstFreeBlock->Next = 0;
        }

        Chunk->FirstBlock = World->FirstFreeBlock;
        World->FirstFreeBlock = Chunk->FirstBlock->Next;

        ClearWorldEntityBlock(Chunk->FirstBlock);
    }

    world_entity_block *Block = Chunk->FirstBlock;
    
    Assert(HasRoomFor(Block, PackSize));
    u8 *Dest = Block->EntityData + Block->EntityDataSize;
    Block->EntityDataSize += PackSize; 
    ++Block->EntityCount;
    
    *(entity *)Dest = *Source;
}

internal void
PackEntityIntoWorld(memory_arena *Arena, world *World, entity *Source, world_position At)
{
    TIMED_FUNCTION();
    world_chunk *Chunk = GetWorldChunk(World, At.TileX, At.TileY, Arena);
    
    Source->TileP = At;
    PackEntityIntoChunk(Arena, World, Source, Chunk);
}

inline void
AddBlockToFreeList(world *World, world_entity_block *Old)
{
    Old->Next = World->FirstFreeBlock;
    World->FirstFreeBlock = Old;
}

inline void
AddChunkToFreeList(world *World, world_chunk *Old)
{
    Old->NextInHash = World->FirstFreeChunk;
    World->FirstFreeChunk = Old;
}

inline void
InitASTileNode(world *World, as_tile_node *Node, v2 P, s32 X, s32 Y)
{
    world_position BaseP = {};
    Node->TileP = MapIntoTileSpace(World, BaseP, P);
    Node->X = X;
    Node->Y = Y;
    Node->Obstacle = false;
    Node->Visited = false;
    Node->Parent = 0;
}

inline void
FindTileNodeNeighbors(world *World, as_tile_node *Node)
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

        if((IndexX >= 0) && (IndexX < WORLD_TILE_NODE_COUNT_PER_DIM) &&
           (IndexY >= 0) && (IndexY < WORLD_TILE_NODE_COUNT_PER_DIM))
        {
            s32 Index = IndexY*WORLD_TILE_NODE_COUNT_PER_DIM + IndexX;
            Node->Neighbours[I] = World->TileNodes + Index;
        }
    }
}

internal as_tile_node *
GetTileNode(world *World, s32 NodeX, s32 NodeY)
{
    as_tile_node *Result = 0;

    Result = World->TileNodes + NodeY*WORLD_TILE_NODE_COUNT_PER_DIM + NodeX;

    return(Result);
}

internal as_tile_node *
GetTileNode(world *World, world_position TileP)
{
    as_tile_node *Result = 0;

    r32 a = 1.0f / 0.25f;

    r32 X = (r32)TileP.TileX*4.0f + 2.0f;
    r32 Y = (r32)TileP.TileY*4.0f + 2.0f; 
    r32 OffsetX = X + (TileP.Offset.x*4.0f);
    r32 OffsetY = Y + (TileP.Offset.y*4.0f);
    s32 IndexX = (s32)OffsetX;
    s32 IndexY = (s32)OffsetY;

    Result = World->TileNodes + IndexY*WORLD_TILE_NODE_COUNT_PER_DIM + IndexX;

    return(Result);
}

inline r32
DistanceBetween(world *World, as_tile_node *NodeA, as_tile_node *NodeB)
{
    v2 Delta = Subtract(World, &NodeA->TileP, &NodeB->TileP);
//    r32 X = (r32)(NodeA->TileP.TileX - NodeB->TileP.TileX);
//    r32 Y = (r32)(NodeA->TileP.TileY - NodeB->TileP.TileY);

    r32 Result = SquareRoot(Square(Delta.x) + Square(Delta.y));

    return(Result);
}

internal void
SolveAStarForTileNodes(world *World, rectangle2 SimBounds, world_position Origin,
                       as_tile_node *StartNode, as_tile_node *EndNode)
{
    TIMED_FUNCTION();

    if(StartNode && EndNode)
    {
        if(!EndNode->Obstacle)
        {
            world_position MinTileP = MapIntoTileSpace(World, Origin, GetMinCorner(SimBounds));
            world_position MaxTileP = MapIntoTileSpace(World, Origin, GetMaxCorner(SimBounds));

            for(int32 NodeY = MinTileP.TileY*TILE_NODE_PER_TILE;
                NodeY <= MaxTileP.TileY*TILE_NODE_PER_TILE;
                ++NodeY)
            {
                for(int32 NodeX = MinTileP.TileX*TILE_NODE_PER_TILE;
                    NodeX <= MaxTileP.TileX*TILE_NODE_PER_TILE;
                    ++NodeX)
                {
                    if((NodeX < WORLD_TILE_NODE_COUNT_PER_DIM) && (NodeY < WORLD_TILE_NODE_COUNT_PER_DIM))
                    {
                        as_tile_node *Node = GetTileNode(World, NodeX, NodeY);

                        Node->Visited = false;
                        Node->GlobalGoal = Real32Maximum;
                        Node->LocalGoal = Real32Maximum;
                        Node->Parent = 0;
                    }
                }
            }

            heap *Heap = &World->MinTileNodeHeap;
            
            as_tile_node *CurrentNode = StartNode;
            CurrentNode->LocalGoal = 0.0f;
            CurrentNode->GlobalGoal = DistanceBetween(World, StartNode, EndNode);

            sort_entry Key = {};
            Key.Index = StartNode->Y*WORLD_TILE_NODE_COUNT_PER_DIM + StartNode->X;
            Key.SortKey = StartNode->GlobalGoal;
            MinHeapInsertNode(Heap, Key);

            while((Heap->Size != 0) && (CurrentNode != EndNode) && (Heap->Size != Heap->MaxSize))
            {
                as_tile_node *TestNode = World->TileNodes + Heap->Nodes[0].Index;
                while((Heap->Size != 0) && (TestNode->Visited))
                {
                    MinHeapExtractNode(Heap);
                    TestNode = World->TileNodes + Heap->Nodes[0].Index;
                }
            
                if(Heap->Size == 0)
                {
                    break;
                }

                CurrentNode = World->TileNodes + Heap->Nodes[0].Index; 
                CurrentNode->Visited = true;

                for(u32 NeighbourIndex = 0;
                    NeighbourIndex < ArrayCount(CurrentNode->Neighbours);
                    ++NeighbourIndex)
                {
                    as_tile_node *NeighbourNode = CurrentNode->Neighbours[NeighbourIndex];
                    if(NeighbourNode)
                    {
                        if((!NeighbourNode->Visited) && (!NeighbourNode->Obstacle))
                        {
                            sort_entry NeighbourKey = {};
                            NeighbourKey.Index = NeighbourNode->Y*WORLD_TILE_NODE_COUNT_PER_DIM + NeighbourNode->X;
                            NeighbourKey.SortKey = NeighbourNode->GlobalGoal;

                            r32 LowerGoal = (CurrentNode->LocalGoal +
                                             DistanceBetween(World, CurrentNode, NeighbourNode));
                            if(LowerGoal < NeighbourNode->LocalGoal)
                            {
                                NeighbourNode->Parent = CurrentNode;
                                NeighbourNode->LocalGoal = LowerGoal;

                                NeighbourNode->GlobalGoal = (NeighbourNode->LocalGoal +
                                                             DistanceBetween(World, NeighbourNode, EndNode));
                                NeighbourKey.SortKey = NeighbourNode->GlobalGoal;
                            }

                            if(Heap->Size != Heap->MaxSize)
                            {
                                MinHeapInsertNode(Heap, NeighbourKey);
                            }
                        }
                    }
                }
            }

            if(Heap->Size == Heap->MaxSize)
            {
                EndNode = 0;
            }
            
            ZeroArray(Heap->MaxSize, Heap->Nodes);
            Heap->Size = 0;
        }
    }
}

internal world *
CreateWorld(transient_state *TranState, v2 ChunkDimInMeters, r32 TileSideInMeters, memory_arena *ParentArena)
{
    world *World = PushStruct(ParentArena, world);
    
    World->ChunkDimInMeters = ChunkDimInMeters;
    World->FirstFree = 0;
    World->TileSideInMeters = TileSideInMeters;
    World->TileDepthInMeters = TileSideInMeters;
    SubArena(&World->Arena, ParentArena, GetArenaSizeRemaining(ParentArena));

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    WeightVector.E[Tag_DataType] = 1.0f;

    World->TileCount = WORLD_TILE_COUNT_PER_DIM*WORLD_TILE_COUNT_PER_DIM;

    MatchVector.E[Tag_DataType] = (r32)FileData_Decorations;
    file_id FileID = GetBestMatchFileFrom(TranState->Assets, Asset_BinaryFile, &MatchVector, &WeightVector);
    loaded_file *BinaryFile = PushFile(TranState, FileID, true);
    Assert(BinaryFile->Data);
    World->Decorations = PushArray(&World->Arena, World->TileCount, decoration);
    World->Decorations = (decoration *)BinaryFile->Data;

    MatchVector.E[Tag_DataType] = (r32)FileData_Tiles;
    FileID = GetBestMatchFileFrom(TranState->Assets, Asset_BinaryFile, &MatchVector, &WeightVector);
    BinaryFile = PushFile(TranState, FileID, true);
    Assert(BinaryFile->Data);
    World->Tiles = PushArray(&World->Arena, World->TileCount, world_tile);
    World->Tiles = (world_tile *)BinaryFile->Data;

    MatchVector.E[Tag_DataType] = (r32)FileData_Collisions;
    FileID = GetBestMatchFileFrom(TranState->Assets, Asset_BinaryFile, &MatchVector, &WeightVector);
    BinaryFile = PushFile(TranState, FileID, true);
    Assert(BinaryFile->Data);
    World->Collisions = PushArray(&World->Arena, World->TileCount, collision);
    World->Collisions = (collision *)BinaryFile->Data;

    World->TileNodeCount = WORLD_TILE_NODE_COUNT_PER_DIM*WORLD_TILE_NODE_COUNT_PER_DIM;
    World->TileNodes = PushArray(&World->Arena, World->TileNodeCount, as_tile_node);
    World->MinTileNodeHeap.MaxSize = World->TileNodeCount / 4;
    World->MinTileNodeHeap.Size = 0;
    World->MinTileNodeHeap.Nodes = PushArray(&World->Arena, World->MinTileNodeHeap.MaxSize, sort_entry);
    v2 P = {};//V2(0.125f, 0.125f);
    for(s32 Y = 0;
        Y < WORLD_TILE_NODE_COUNT_PER_DIM;
        ++Y)
    {
        for(s32 X = 0;
            X < WORLD_TILE_NODE_COUNT_PER_DIM;
            ++X)
        {
            P = V2(-0.375f, -0.375f) + 0.25f*V2i(X, Y);
            as_tile_node *Node = World->TileNodes + Y*WORLD_TILE_NODE_COUNT_PER_DIM + X;
            InitASTileNode(World, Node, P, X, Y);
            FindTileNodeNeighbors(World, Node);
        }
    }

    return(World);
}

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
    bool32 Result = (IsCanonical(World->TileDimInMeters.x, Offset.x) &&
                     IsCanonical(World->TileDimInMeters.y, Offset.y));

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
    world_chunk **ChunkPtr = GetWorldChunkInternal(World, ChunkX, ChunkY);
    world_chunk *Result = *ChunkPtr;
    if(Result)
    {
        *ChunkPtr = Result->NextInHash;
    }

    return(Result);
}

inline void
RecanonicalizeCoord(real32 TileDim, int32 *Tile, real32 *TileRel)
{
    // TODO(casey): Need to do something that doesn't use the divide/multiply method
    // for recanonicalizing because this can end up rounding back on to the tile
    // you just came from.

    // NOTE(casey): Wrapping IS NOT ALLOWED, so all coordinates are assumed to be
    // within the safe margin!
    // TODO(casey): Assert that we are nowhere near the edges of the world.
    
    int32 Offset = RoundReal32ToInt32(*TileRel / TileDim);
    *Tile += Offset;
    *TileRel -= Offset*TileDim;

    Assert(IsCanonical(TileDim, *TileRel));
}

inline world_position
MapIntoTileSpace(world *World, world_position BasePos, v2 Offset)
{
    world_position Result = BasePos;

    Result.Offset += Offset;
    RecanonicalizeCoord(World->TileDimInMeters.x, &Result.TileX, &Result.Offset.x);
    RecanonicalizeCoord(World->TileDimInMeters.y, &Result.TileY, &Result.Offset.y);
    
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
    
    v2 TileDim = World->TileDimInMeters.xy;
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
    
    v2 Result = Hadamard(World->TileDimInMeters.xy, dTile) + (A->Offset - B->Offset);

    return(Result);
}

inline world_position
CenteredTilePoint(world *World, uint32 TileX, uint32 TileY)
{
    world_position Result = {};

    Result.TileX = (TileX > World->TileWidth) ? (World->TileWidth - 1) : TileX;
    Result.TileY = (TileY > World->TileHeight) ? (World->TileHeight - 1) : TileY;

    return(Result);
}

inline b32
HasRoomFor(world_entity_block *Block, u32 Size)
{
    b32 Result = ((Block->EntityDataSize + Size) <= sizeof(Block->EntityData));

    return(Result);
}

inline void
PackEntityIntoChunk(memory_arena *Arena, world *World, entity *Source, world_chunk *Chunk)
{
    u32 PackSize = sizeof(*Source);

    if(!Chunk->FirstBlock || !HasRoomFor(Chunk->FirstBlock, PackSize))
    {
        if(!World->FirstFreeBlock)
        {
            World->FirstFreeBlock = PushStruct(Arena, world_entity_block);
            World->FirstFreeBlock->Next = 0;
        }

        world_entity_block *NewBlock = World->FirstFreeBlock;
        World->FirstFreeBlock = NewBlock->Next;

        ClearWorldEntityBlock(NewBlock);

        NewBlock->Next = Chunk->FirstBlock;
        Chunk->FirstBlock = NewBlock;
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

internal world *
CreateWorld(r32 TileSideInMeters, loaded_world_map *Map)
{
    world *World = BootstrapPushStruct(world, Arena);
    
    World->FirstFree = 0;
    World->TileDimInMeters = V3(TileSideInMeters, TileSideInMeters, TileSideInMeters);

    World->TileWidth = Map->Header->MapWidth;
    World->TileHeight = Map->Header->MapHeight;
    World->TileCount = World->TileWidth*World->TileHeight;
    
    return(World);
}

internal sswm_ground_tile *
EDITORGetWorldMapGroundTile(world *World, u32 TileX, u32 TileY)
{
    sswm_ground_tile *Result = 0;
    if((TileX < World->TileWidth) && (TileY < World->TileWidth) &&
       (TileX >= 0) && (TileY >= 0))
    {
        u32 TileIndex = TileY*World->Map->Header->MapWidth + TileX;
        Result = World->Map->GroundTiles + TileIndex;
    }

    return(Result);
}

inline sswm_ground_tile *
EDITORGetWorldMapGroundTile(world *World, world_position P)
{
    sswm_ground_tile *Result = EDITORGetWorldMapGroundTile(World, P.TileX, P.TileY);
    return(Result);
}

inline b32
EDITORTileIsValid(world *World, u32 TileX, u32 TileY)
{
    b32 Result = EDITORGetWorldMapGroundTile(World, TileX, TileY) ? true : false;
    return(Result);
}

internal world *
EDITORCreateWorld(transient_state *TranState, r32 TileSideInMeters, u32 WorldTileWidth, u32 WorldTileHeight, u32 NodesPerTile, loaded_world_map *Map)
{
    world *World = BootstrapPushStruct(world, Arena);
    
    World->FirstFree = 0;
    World->TileDimInMeters = V3(TileSideInMeters, TileSideInMeters, TileSideInMeters);

    World->TileWidth = WorldTileWidth;
    World->TileHeight = WorldTileHeight;
    World->TileCount = World->TileWidth*World->TileHeight;

    if(Map)
    {
        World->Map = PushStruct(&World->Arena, loaded_world_map);
        World->Map->Header = PushStruct(&World->Arena, sswm_header);
        *World->Map->Header = *Map->Header;
        World->Map->GroundTiles = PushArray(&World->Arena, World->Map->Header->MapWidth*World->Map->Header->MapHeight, sswm_ground_tile);
        World->Map->Entities = PushArray(&World->Arena, World->Map->Header->EntityCount, sswm_entity);

        u32 TilesSize = World->Map->Header->MapWidth*World->Map->Header->MapHeight*sizeof(sswm_ground_tile);
        Copy(TilesSize, Map->GroundTiles, World->Map->GroundTiles);

        u32 EntitiesSize = World->Map->Header->EntityCount*sizeof(sswm_entity);
        Copy(EntitiesSize, Map->Entities, World->Map->Entities);

        editor_assets *Assets = TranState->Assets;
        u32 ZLayerCount = World->Map->Header->GroundLayer_ZLayerCount & 0xFFFF;
        for(u32 TileIndex = 0;
            TileIndex < World->Map->Header->MapWidth*World->Map->Header->MapHeight;
            ++TileIndex)
        {
            sswm_ground_tile *Tile = World->Map->GroundTiles + TileIndex;
            for(u32 BitmapIndex = 0;
                BitmapIndex < ZLayerCount;
                ++BitmapIndex)
            {
                u32 CheckSum = Tile->CheckSum[BitmapIndex];
                bitmap_id ID = GetTileBitmapByChecksumTag(TranState->Assets, CheckSum);
                Tile->BitmapID[BitmapIndex] = ID.Value;
            }
        }
    }
    else
    {
        World->Map = PushStruct(&World->Arena, loaded_world_map);
        World->Map->Header = PushStruct(&World->Arena, sswm_header);
        FormatString(ArrayCount(World->Map->Header->Name), World->Map->Header->Name, "sswm");
        World->Map->Header->MapWidth = WorldTileWidth;        
        World->Map->Header->MapHeight = WorldTileHeight;        
        World->Map->Header->EntityCount = 0;        
        
        World->Map->GroundTiles = PushArray(&World->Arena, World->Map->Header->MapWidth*World->Map->Header->MapHeight, sswm_ground_tile);
        World->Map->Entities = PushArray(&World->Arena, World->Map->Header->EntityCount, sswm_entity);
    }

    for(u32 Y = 0;
        Y < World->TileHeight;
        ++Y)
    {
        for(u32 X = 0;
            X < World->TileWidth;
            ++X)
        {
            sswm_ground_tile *Tile = World->Map->GroundTiles + Y*World->TileWidth + X;
            Tile->TileX = X;
            Tile->TileY = Y;
        }
    }
    
    return(World);
}

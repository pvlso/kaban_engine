/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal entity_hash *
GetHashFromID(sim_region *SimRegion, entity_id ID)
{
//    TIMED_FUNCTION();
    Assert(ID.Value);

    entity_hash *Result = 0;
    uint32 HashValue = ID.Value;
    for(uint32 Offset = 0;
        Offset < ArrayCount(SimRegion->Hash);
        ++Offset)
    {
        uint32 HashMask = (ArrayCount(SimRegion->Hash) - 1);
        uint32 HashIndex = ((HashValue + Offset) & HashMask);
        entity_hash *Entry = SimRegion->Hash + HashIndex;

        if((Entry->Index.Value == 0) || (Entry->Index.Value == ID.Value))
        {
            Result = Entry;
            break;
        }
    }

    return(Result);
}

inline entity *
GetEntityByID(sim_region *SimRegion, entity_id ID)
{
    entity_hash *Entry = GetHashFromID(SimRegion, ID);
    entity *Result = Entry ? Entry->Ptr : 0;

    return(Result);
}

inline void
LoadEntityreference(sim_region *SimRegion, entity_reference *Ref)
{
    if(Ref->ID.Value)
    {
        Ref->Ptr = GetEntityByID(SimRegion, Ref->ID);
    }
}

inline v2
GetSimSpaceP(sim_region *SimRegion, entity *Stored)
{
    // NOTE(casey): Map the entity into camera space
    // TODO(casey): Do we want to set this to signaling NAN in
    // debug mode to make sure nobody ever uses the position
    // of a nonspatial entity?
    v2 Result = InvalidP;
    Result = Subtract(SimRegion->World, &Stored->TileP, &SimRegion->Origin);

    return(Result);
}

inline void
StoreEntityreference(entity_reference *Ref)
{
    if(Ref->Ptr != 0)
    {
        Ref->ID = Ref->Ptr->ID;
    }
}

inline bool32
EntityOverlapsRectangle(v2 P, entity_collision *Collision, rectangle2 Rect)
{
    rectangle2 Grown = AddRadiusTo(Rect, 0.5f*GetDim(Collision->CollisionRect).xy);
    bool32 Result = IsInRectangle(Grown, (P + Collision->OffsetP.xy));

    return(Result);
}

internal void
AddEntity(sim_region *SimRegion, entity *Source, v2 SimP)
{
//    TIMED_FUNCTION();
    entity_id ID = Source->ID;
    
    entity_hash *Entry = GetHashFromID(SimRegion, ID); 
    Assert(Entry->Ptr == 0);
    if(SimRegion->EntityCount < SimRegion->MaxEntityCount)
    {
        entity *Dest = SimRegion->Entities + SimRegion->EntityCount++;

        Entry->Index = ID;
        Entry->Ptr = Dest;

        if(Source)
        {
            // TODO(casey): This should really be a decompression step, not
            // a copy!
            *Dest = *Source;

            // TODO(paul): Decide what to use it for
            Dest->ZLayer = 0;
        }

        Dest->ID = ID;
        Dest->P = V3(SimP, 0.0f);
        Dest->Updatable = EntityOverlapsRectangle(Dest->P.xy, Dest->Collision,
                                                  SimRegion->UpdatableBounds);
    }
    else
    {
        InvalidCodePath;
    }
}

internal void
ConnectEntityPointers(sim_region *SimRegion)
{
//    TIMED_FUNCTION();

    for(u32 EntityIndex = 0;
        EntityIndex < SimRegion->EntityCount;
        ++EntityIndex)
    {
        entity *Entity = SimRegion->Entities + EntityIndex;
        if(IsCreationFlagSet(Entity, CreationFlag_HaveReferences))
        {
            entity_references *Refs = Entity->References;
            for(uint32 RefIndex = 0;
                RefIndex < Refs->RefCount;
                ++RefIndex)
            {
                entity_reference *Ref = Refs->References + RefIndex;
                LoadEntityreference(SimRegion, Ref);
            }
        }
    }
}

internal sim_region *
BeginSim(memory_arena *SimArena, world *World, world_position Origin, rectangle2 Bounds, real32 dt)
{
    TIMED_FUNCTION();

// TODO(casey): If entities were stored in the world, we wouldn't need the game state here!

    sim_region *SimRegion = PushStruct(SimArena, sim_region);
    ZeroStruct(SimRegion->Hash);

    // TODO(casey): Try to make these get enforced more rigorously
    SimRegion->MaxEntityRadius = 2.0f;
    SimRegion->MaxEntityVelocity = 100.0f;
    real32 UpdateSafetyMargin = SimRegion->MaxEntityRadius + dt*SimRegion->MaxEntityVelocity;
    
    SimRegion->World = World;
    SimRegion->Origin = Origin;
    SimRegion->UpdatableBounds = AddRadiusTo(Bounds, V2(SimRegion->MaxEntityRadius,
                                                        SimRegion->MaxEntityRadius));
    SimRegion->Bounds = AddRadiusTo(SimRegion->UpdatableBounds,
                                    V2(UpdateSafetyMargin, UpdateSafetyMargin));

    // TODO(casey): Need to be more specific about entity counts
    SimRegion->MaxEntityCount = 4096;
    SimRegion->EntityCount = 0;
    SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, entity);

    world_position MinTileP = MapIntoTileSpace(World, SimRegion->Origin,
                                                 GetMinCorner(SimRegion->Bounds));

    world_position MaxTileP = MapIntoTileSpace(World, SimRegion->Origin,
                                                GetMaxCorner(SimRegion->Bounds));

    int32 MinChunkX = MinTileP.TileX / TILES_PER_CHUNK_DIM;
    int32 MaxChunkX = MaxTileP.TileX / TILES_PER_CHUNK_DIM;
    int32 MinChunkY = MinTileP.TileY / TILES_PER_CHUNK_DIM;
    int32 MaxChunkY = MaxTileP.TileY / TILES_PER_CHUNK_DIM;

    for(int32 ChunkY = MinChunkY;
        ChunkY <= MaxChunkY;
        ++ChunkY)
    {
        for(int32 ChunkX = MinChunkX;
            ChunkX <= MaxChunkX;
            ++ChunkX)
        {
            world_chunk *Chunk = RemoveWorldChunk(World, ChunkX, ChunkY);
            if(Chunk)
            {
                world_entity_block *Block = Chunk->FirstBlock;
                while(Block)
                {
                    for(uint32 EntityIndex = 0;
                        EntityIndex < Block->EntityCount;
                        ++EntityIndex)
                    {
                        entity *Entity = (entity *)Block->EntityData + EntityIndex;

                        v2 SimSpaceP = GetSimSpaceP(SimRegion, Entity);
                        AddEntity(SimRegion, Entity, SimSpaceP);
                    }

                    world_entity_block *NextBlock = Block->Next;
                    AddBlockToFreeList(World, Block);
                    Block = NextBlock;
                }

                AddChunkToFreeList(World, Chunk);
            }
        }
    }

    ConnectEntityPointers(SimRegion);

    return(SimRegion);
}

internal void
EndSim(sim_region *Region, rectangle2 CameraBoundsInMeters)
{
    TIMED_FUNCTION();

    entity *Entity = Region->Entities;
    for(uint32 EntityIndex = 0;
        EntityIndex < Region->EntityCount;
        ++EntityIndex, ++Entity)
    {
        if(!IsSet(Entity, EntityFlag_Deleted))
        {
            world_position TileP = MapIntoTileSpace(Region->World, Region->Origin, Entity->P.xy);

            if(IsCreationFlagSet(Entity, CreationFlag_HaveReferences))
            {
                entity_references *Refs = Entity->References;
                for(uint32 RefIndex = 0;
                    RefIndex < Refs->RefCount;
                    ++RefIndex)
                {
                    entity_reference *Ref = Refs->References + RefIndex;
                    StoreEntityreference(Ref);
                }
            }

            PackEntityIntoWorld(&Region->World->Arena, Region->World, Entity, TileP);
        }
    }
}

/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

internal entity_hash *
GetHashFromID(sim_region *SimRegion, entity_id ID)
{
    TIMED_FUNCTION();
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
EntityOverlapsRectangle(v2 P, entity_collision_volume *Collision, rectangle2 Rect)
{
    rectangle2 Grown = AddRadiusTo(Rect, 0.5f*GetDim(Collision->CollisionRect).xy);
    bool32 Result = IsInRectangle(Grown, (P + Collision->OffsetP.xy));

    return(Result);
}

internal void
AddEntity(game_mode_world *WorldMode, sim_region *SimRegion, entity *Source, v2 SimP)
{
    TIMED_FUNCTION();
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
    TIMED_FUNCTION();

    for(u32 EntityIndex = 0;
        EntityIndex < SimRegion->EntityCount;
        ++EntityIndex)
    {
        entity *Entity = SimRegion->Entities + EntityIndex;
        for(uint32 RefIndex = 0;
            RefIndex < Entity->RefCount;
            ++RefIndex)
        {
            entity_reference *Ref = Entity->References + RefIndex;
            LoadEntityreference(SimRegion, Ref);
        }
    }
}

internal sim_region *
BeginSim(memory_arena *SimArena, game_mode_world *WorldMode, world *World, world_position Origin,
         rectangle2 Bounds, real32 dt)
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
                        AddEntity(WorldMode, SimRegion, Entity, SimSpaceP);
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
EndSim(game_mode_world *WorldMode, sim_region *Region, rectangle2 CameraBoundsInMeters)
{
    TIMED_FUNCTION();

    entity *Entity = Region->Entities;
    for(uint32 EntityIndex = 0;
        EntityIndex < Region->EntityCount;
        ++EntityIndex, ++Entity)
    {
        if(!IsSet(Entity, EntityFlag_Deleted))
        {
            world_position TileP = MapIntoTileSpace(WorldMode->World, Region->Origin, Entity->P.xy);

            for(uint32 RefIndex = 0;
                RefIndex < Entity->RefCount;
                ++RefIndex)
            {
                entity_reference *Ref = Entity->References + RefIndex;
                StoreEntityreference(Ref);
            }

            if(Entity->ID.Value == WorldMode->CameraFollowingEntityIndex.Value)
            {
                world_position NewCameraP = WorldMode->CameraP;

                world_position MinTileP = MapIntoTileSpace(WorldMode->World, Entity->TileP,
                                                             GetMinCorner(CameraBoundsInMeters));

                world_position MaxTileP = MapIntoTileSpace(WorldMode->World, Entity->TileP,
                                                             GetMaxCorner(CameraBoundsInMeters));

                if((MinTileP.TileX >= WorldMode->CameraBoundsMin.TileX) &&
                   (MaxTileP.TileX < WorldMode->CameraBoundsMax.TileX))
                {
                    NewCameraP.TileX = Entity->TileP.TileX;
                    NewCameraP.Offset.x = Entity->TileP.Offset.x;
                }

                if((MinTileP.TileY >= WorldMode->CameraBoundsMin.TileY) &&
                   (MaxTileP.TileY < WorldMode->CameraBoundsMax.TileY))
                {
                    NewCameraP.TileY = Entity->TileP.TileY;
                    NewCameraP.Offset.y = Entity->TileP.Offset.y;
                }

                WorldMode->CameraP = NewCameraP;
            }

            PackEntityIntoWorld(&WorldMode->World->Arena, WorldMode->World, Entity, TileP);
        }
    }
}

struct test_wall
{
    real32 X;
    real32 RelX;
    real32 RelY;
    real32 DeltaX;
    real32 DeltaY;
    real32 MinY;
    real32 MaxY;
    v3 Normal;
};

internal bool32
TestWall(real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY,
         real32 *tMin, real32 MinY, real32 MaxY)
{
    bool32 Hit = false;
    real32 tEpsilon = 0.001f;
    if(PlayerDeltaX != 0.0f)
    {
        real32 tResult = (WallX - RelX) / PlayerDeltaX;
        real32 Y = RelY + tResult*PlayerDeltaY;
        if((tResult >= 0.0f) && (*tMin > tResult))
        {
            if((Y >= MinY) && (Y <= MaxY))
            {
                *tMin = Maximum(0.0f, tResult - tEpsilon);
                Hit = true;
            }
        }
    }

    return(Hit);
}

internal bool32
CanCollide(game_mode_world *WorldMode, entity *A, entity *B)
{
    bool32 Result = false;

    if(A != B)
    {
        if(A->ID.Value > B->ID.Value)
        {
            entity *Temp = A;
            A = B;
            B = Temp;
        }

        if(IsSet(A, EntityFlag_Collides) && IsSet(B, EntityFlag_Collides))
        {
            // TODO(casey): Property-based logic goes here
            Result = true;

            // TODO(casey): BETTER HASH FUNCTION
            uint32 HashBucket = A->ID.Value & (ArrayCount(WorldMode->CollisionRuleHash) - 1);
            for(pairwise_collision_rule *Rule = WorldMode->CollisionRuleHash[HashBucket];
                Rule;
                Rule = Rule->NextInHash)
            {
                if((Rule->IDA.Value == A->ID.Value) &&
                   (Rule->IDB.Value == B->ID.Value))
                {
                    Result = Rule->CanCollide;
                    break;
                }
            }
        }
    }    
    
    return(Result);
}
 
internal bool32
HandleCollision(game_mode_world *WorldMode, entity *A, entity *B)
{
    bool32 StopsOnCollision = true;

    if(A->Type > B->Type)
    {
        entity *Temp = A;
        A = B;
        B = Temp;
    }

    if(((A->GeneralType == GeneralType_Enemy) && (B->GeneralType == GeneralType_Spell)) ||
       ((A->Type == EntityType_Hero) && (B->GeneralType == GeneralType_Spell)))
    {
        // NOTE(paul): Do something
        if(B->State != EntityState_Dieing)
        {
            flyingspell_entity *SpellData = (flyingspell_entity *)B->Data;

            A->HealthMax_Health -= SpellData->Damage;
            if((s16)(A->HealthMax_Health & 0xffff) <= 0)
            {
                A->HealthMax_Health &= 0xffff0000;
            }

            ClearCollisionRulesFor(WorldMode, B->ID);
            ChangeEntityState(B, EntityState_Dieing);
            ChangeAnimationType(B, AnimationType_Death);
        }
    }
    else if((A->GeneralType == GeneralType_Spell) &&
            (B->GeneralType == GeneralType_Object))
    {
        ClearCollisionRulesFor(WorldMode, A->ID);
        ChangeEntityState(A, EntityState_Dieing);
        ChangeAnimationType(A, AnimationType_Death);
    }
    else if((A->GeneralType == GeneralType_Spell) &&
            (B->GeneralType == GeneralType_Spell))
    {
        ClearCollisionRulesFor(WorldMode, A->ID);
        ChangeEntityState(A, EntityState_Dieing);
        ChangeAnimationType(A, AnimationType_Death);

        ClearCollisionRulesFor(WorldMode, B->ID);
        ChangeEntityState(B, EntityState_Dieing);
        ChangeAnimationType(B, AnimationType_Death);
    }
    
    // TODO(casey): Stairs
    // Entity->AbsTileZ += HitLow->Sim.dAbsTileZ;

    return(StopsOnCollision);
}

internal bool32
CanOverlap(game_mode_world *WorldMode, entity *Mover, entity *Region)
{
    bool32 Result = false;
    
    if(Mover != Region)
    {
        Result = true;
    }

    return(Result);
}

internal void
HandleOverlap(game_mode_world *WorldMode, entity *Mover, entity *Region, real32 dt,
              real32 *Ground)
{
    if(((Region->Type == EntityType_Item) && (Mover->Type == EntityType_Hero)) ||
       ((Region->Type == EntityType_Hero) && (Mover->Type == EntityType_Item)))
    {
        item_entity *ItemData = (item_entity *)Region->Data;

        if(ItemData->Name != ItemName_HealPotion)
        {
            hero_entity *HeroData = (hero_entity *)Mover->Data;
            HeroData->Inventory[HeroData->ItemCount++] = ItemData->Name;
        }
        else
        {
            u32 HealAmount = 15;
            
            u32 MaxHealth = Mover->HealthMax_Health >> 16;
            u32 Health = Mover->HealthMax_Health & 0xffff;
            if((Health + HealAmount) > MaxHealth)
            {
                Mover->HealthMax_Health = (u32)((MaxHealth << 16) | MaxHealth);
            }
            else
            {
                Mover->HealthMax_Health += HealAmount;
            }

            u32 MaxMana = Mover->ManaMax_Mana >> 16;
            u32 Mana = Mover->ManaMax_Mana & 0xffff;
            if((Mana + HealAmount) > MaxMana)
            {
                Mover->ManaMax_Mana = (u32)((MaxMana << 16) | MaxMana);
            }
            else
            {
                Mover->ManaMax_Mana += HealAmount;
            }
        }

        ChangeEntityState(Region, EntityState_Dieing);
    }    
}

internal bool32
SpeculativeCollide(entity *Mover, entity *Region, v3 TestP)
{
    bool32 Result = true;

#if 0
    if(Region->Type == EntityType_Stairwell)
    {
        real32 StepHeight = 0.1f;
        Result = ((AbsoluteValue(GetEntityGroundPoint(Mover).Z - Ground) > StepHeight) ||
                  ((Bary.Y > 0.1f) && (Bary.Y < 0.9f)));
        v3 MoverGroundPoint = GetEntityGroundPoint(Mover, TestP);
        real32 Ground = GetStairGround(Region, MoverGroundPoint);
        Result = (AbsoluteValue(MoverGroundPoint.z - Ground) > StepHeight);
    }    
#endif

    return(Result);
}

internal bool32
EntitiesOverlap(entity *Entity, entity *TestEntity, v3 Epsilon = V3(0, 0, 0))
{
    bool32 Result = false;
    
    entity_collision_volume *EntityCollision = Entity->Collision;
    entity_collision_volume *TestCollision = TestEntity->Collision;

    rectangle3 EntityRect = RectCenterDim(Entity->P + EntityCollision->OffsetP,
                                          GetDim(EntityCollision->CollisionRect) + Epsilon);
    rectangle3 TestEntityRect = RectCenterDim(TestEntity->P + TestCollision->OffsetP,
                                              GetDim(TestCollision->CollisionRect) + Epsilon);

    Result = RectanglesIntersect(EntityRect, TestEntityRect);

    return(Result);
}

internal void
MoveEntity(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity, real32 dt,
           move_spec *MoveSpec, v3 ddP)
{
    world *World = SimRegion->World;

    if(MoveSpec->UnitMaxAccelVector)
    {
        // TODO(casey): Valid acceleration passed in!
        real32 ddPLength = LengthSq(ddP);
        if(ddPLength > 1.0f)
        {
            ddP *= (1.0f / SquareRoot(ddPLength));
        }
    }

    ddP *= MoveSpec->Speed;

    // TODO(casey): ODE here!
    v3 Drag = -MoveSpec->Drag * Entity->dP;
    Drag.z = 0.0f;
    ddP += Drag;

    if(!IsSet(Entity, EntityFlag_ZSupported))
    {
        ddP += V3(0, 0, -9.8f); // NOTE(casey): Gravity!
    }
    
    v3 PlayerDelta = (0.5f * ddP * Square(dt) +
                      Entity->dP * dt);
    Entity->dP = ddP * dt + Entity->dP;
    // TODO(casey): Upgrade physical motion routines to handle capping the
    // maximum velocity?
    Assert(LengthSq(Entity->dP) <= Square(SimRegion->MaxEntityVelocity));

    real32 DistanceRemaining = Entity->DistanceLimit;
    if(DistanceRemaining == 0.0f)
    {
        DistanceRemaining = 10000.0f;
    }

    for(uint32 Iteration = 0;
        Iteration < 4;
        ++Iteration)
    {
        real32 tMin = 1.0f;

        real32 PlayerDeltaLength = Length(PlayerDelta);
        // TODO(casey): What do we want to do for epsilons here?
        // Think this through for the final collision code
        if(PlayerDeltaLength > 0.0f)
        {
            if(PlayerDeltaLength > DistanceRemaining)
            {
                tMin = (DistanceRemaining / PlayerDeltaLength);
            }
        
            v3 WallNormal = {};
            entity *HitEntity = 0;

            v3 DesiredPosition = Entity->P + PlayerDelta;
            for(uint32 TestHighEntityIndex = 0;
                TestHighEntityIndex < SimRegion->EntityCount;
                ++TestHighEntityIndex)
            {
                entity *TestEntity = SimRegion->Entities + TestHighEntityIndex;
                if(Entity->GeneralType == GeneralType_Enemy && TestEntity->GeneralType == GeneralType_Enemy)
                {
                    break;
                }

                if((IsSet(TestEntity, EntityFlag_Traversable) &&
                    EntitiesOverlap(Entity, TestEntity, V3(1, 1, 1))) ||
                   CanCollide(WorldMode, Entity, TestEntity))
                {
                    entity_collision_volume *EntityCollision = Entity->Collision;
                    entity_collision_volume *TestCollision = TestEntity->Collision;

                    v2 EntityVolumeDim = GetDim(EntityCollision->CollisionRect).xy;
                    v2 TestVolumeDim = GetDim(TestCollision->CollisionRect).xy;
                        
                    v2 MinkowskiDiameter = V2(TestVolumeDim.x + EntityVolumeDim.x,
                                              TestVolumeDim.y + EntityVolumeDim.y);

                    v2 MinCorner = -0.5f*MinkowskiDiameter;
                    v2 MaxCorner = 0.5f*MinkowskiDiameter;

                    v3 Rel = ((Entity->P + EntityCollision->OffsetP) -
                              (TestEntity->P + TestCollision->OffsetP));

                    // TODO(paul): Make collision depending on height of an entity with "flying" objects
                    if(TestWall(MinCorner.x, Rel.x, Rel.y, PlayerDelta.x, PlayerDelta.y,
                                &tMin, MinCorner.y, MaxCorner.y))
                    {
                        WallNormal = V3(-1, 0, 0);
                        HitEntity = TestEntity;
                    }
                
                    if(TestWall(MaxCorner.x, Rel.x, Rel.y, PlayerDelta.x, PlayerDelta.y,
                                &tMin, MinCorner.y, MaxCorner.y))
                    {
                        WallNormal = V3(1, 0, 0);
                        HitEntity = TestEntity;
                    }
                
                    if(TestWall(MinCorner.y, Rel.y, Rel.x, PlayerDelta.y, PlayerDelta.x,
                                &tMin, MinCorner.x, MaxCorner.x))
                    {
                        WallNormal = V3(0, -1, 0);
                        HitEntity = TestEntity;
                    }
                
                    if(TestWall(MaxCorner.y, Rel.y, Rel.x, PlayerDelta.y, PlayerDelta.x,
                                &tMin, MinCorner.x, MaxCorner.x))
                    {
                        WallNormal = V3(0, 1, 0);
                        HitEntity = TestEntity;
                    }

                    if(IsSet(TestEntity, EntityFlag_ZSupported) &&
                       !IsSet(Entity, EntityFlag_ZSupported))
                    {
                        rectangle2 HeightRect =
                            RectCenterDim(V2(0, 0), V2(GetDim(TestEntity->Collision->CollisionRect).x,
                                                       TestEntity->Collision->Height));

                        v2 HeightTestVolumeDim = GetDim(HeightRect);
                        
                        v2 MinkowskiDiameterH = V2(HeightTestVolumeDim.x + EntityVolumeDim.x,
                                                   HeightTestVolumeDim.y + EntityVolumeDim.y);

                        v2 HeightMinCorner = -0.5f*MinkowskiDiameterH;
                        v2 HeightMaxCorner = 0.5f*MinkowskiDiameterH;

                        v3 RelHeight = ((Entity->P + EntityCollision->OffsetP) -
                                        (TestEntity->P + TestCollision->OffsetP +
                                         V3(0, 0.5f*TestEntity->Collision->Height, 0)));

                        // TODO(paul): Make collision depending on height of an entity with "flying" objects
                        if(TestWall(HeightMinCorner.x, RelHeight.x, RelHeight.y, PlayerDelta.x, PlayerDelta.y,
                                    &tMin, HeightMinCorner.y, HeightMaxCorner.y))
                        {
                            WallNormal = V3(-1, 0, 0);
                            HitEntity = TestEntity;
                        }
                
                        if(TestWall(HeightMaxCorner.x, RelHeight.x, RelHeight.y, PlayerDelta.x, PlayerDelta.y,
                                    &tMin, HeightMinCorner.y, HeightMaxCorner.y))
                        {
                            WallNormal = V3(1, 0, 0);
                            HitEntity = TestEntity;
                        }
                
                        if(TestWall(HeightMinCorner.y, RelHeight.y, RelHeight.x, PlayerDelta.y, PlayerDelta.x,
                                    &tMin, HeightMinCorner.x, HeightMaxCorner.x))
                        {
                            WallNormal = V3(0, -1, 0);
                            HitEntity = TestEntity;
                        }
                
                        if(TestWall(HeightMaxCorner.y, RelHeight.y, RelHeight.x, PlayerDelta.y, PlayerDelta.x,
                                    &tMin, HeightMaxCorner.x, HeightMaxCorner.x))
                        {
                            WallNormal = V3(0, 1, 0);
                            HitEntity = TestEntity;
                        }
                    }
                }
            }
               
            Entity->P += tMin*PlayerDelta;
            DistanceRemaining -= tMin*PlayerDeltaLength;            
            if(HitEntity)
            {
                PlayerDelta = DesiredPosition - Entity->P;
                bool32 StopsOnCollision = HandleCollision(WorldMode, Entity, HitEntity);
                if(StopsOnCollision)
                {
                    PlayerDelta = PlayerDelta - 1*Inner(PlayerDelta, WallNormal)*WallNormal;
                    Entity->dP = Entity->dP - 1*Inner(Entity->dP, WallNormal)*WallNormal;
                }
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }    

    real32 Ground = 0.0f;

    // NOTE(casey): Handle events based on area overlapping
    // TODO(casey): Handle overlapping precisely by moving it into the collision loop?
    {
        // TODO(casey): Spatial partition here!
        for(uint32 TestHighEntityIndex = 0;
            TestHighEntityIndex < SimRegion->EntityCount;
            ++TestHighEntityIndex)
        {
            entity *TestEntity = SimRegion->Entities + TestHighEntityIndex;
            if(CanOverlap(WorldMode, Entity, TestEntity) &&
               EntitiesOverlap(Entity, TestEntity))
            {
                HandleOverlap(WorldMode, Entity, TestEntity, dt, &Ground);
            }
        }
    }    

#if 0
    Ground += Entity->P.z - GetEntityGroundPoint(Entity).z;
    if((Entity->P.z <= Ground) ||
       (IsSet(Entity, EntityFlag_ZSupported) &&
        (Entity->dP.z == 0.0f)))
    {
        Entity->P.z = Ground;
        Entity->dP.z = 0;
        AddFlags(Entity, EntityFlag_ZSupported);
    }
    else
    {
        ClearFlags(Entity, EntityFlag_ZSupported);
    }
#endif

    if(Entity->DistanceLimit != 0.0f)
    {
        Entity->DistanceLimit = DistanceRemaining;
    }

    if(Entity->AnimationType != AnimationType_Death)
    {
        // TODO(casey): Change to use acceleration vector
        if((AbsoluteValue(Entity->dP.x) <= 0.5f) && (AbsoluteValue(Entity->dP.y) <= 0.5f))
        {
            // NOTE(casey): Leave FacingDirection whatever it was
            if((Entity->AnimationType != AnimationType_Idle))
            {
                ChangeAnimationType(Entity, AnimationType_Idle);
            }
        }
        else if(AbsoluteValue(Entity->dP.x) > AbsoluteValue(Entity->dP.y))
        {
            if(Entity->AnimationType != AnimationType_Walk)
            {
                ChangeAnimationType(Entity, AnimationType_Walk);
            }

            if(Entity->dP.x > 0)
            {
                Entity->FacingDirection = 0;
            }
            else
            {
                Entity->FacingDirection = 2;
            }
        }
        else
        {
            if(Entity->AnimationType != AnimationType_Walk)
            {
                ChangeAnimationType(Entity, AnimationType_Walk);
            }

            if(Entity->dP.y > 0)
            {
                Entity->FacingDirection = 1;
            }
            else
            {
                Entity->FacingDirection = 3;
            }
        }
    }
}




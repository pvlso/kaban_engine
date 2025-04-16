/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

struct updated_entity
{
    move_spec MoveSpec;
    v3 ddP;
};

inline void
HitEntitiesInRectangle(sim_region *SimRegion, audio_state *AudioState, random_series *EffectsEntropy,
                       object_transform *Transform, entity *AttackingEntity, rectangle2 AttackSurface, u32 Damage)
{
#if 0
    for(u32 TestEntityIndex = 0;
        TestEntityIndex < SimRegion->EntityCount;
        ++TestEntityIndex)
    {
        entity *TestEntity = SimRegion->Entities + TestEntityIndex;
        entity_stats *TestEntityStats = TestEntity->Stats;
        if(!(AttackingEntity->GeneralType == GeneralType_Enemy && TestEntity->GeneralType == GeneralType_Enemy))
        {
            if((TestEntityStats->HealthMax_Health >> 16) && (TestEntity->ID.Value != AttackingEntity->ID.Value))
            {
                rectangle2 TestEntityRect = RectCenterDim((TestEntity->P + TestEntity->Collision->OffsetP -
                                                           Transform->OffsetP).xy,
                                                          GetDim(TestEntity->Collision->CollisionRect).xy);
                if(RectanglesIntersect(AttackSurface, TestEntityRect))
                {
                    u32 RandomSound = RandomBetween(EffectsEntropy, 0, 2);
                    sound_id SoundID = AttackingEntity->SoundEffects->AttackImpactSound[RandomSound];
                    PlaySound(AudioState, SoundID);
                    TestEntityStats->HealthMax_Health -= Damage;
                    if((s16)(TestEntityStats->HealthMax_Health & 0xffff) <= 0)
                    {
                        TestEntityStats->HealthMax_Health &= 0xffff0000;
                    }
                }
            }
        }
    }
#endif
}

inline void
HealEntitiesInRectangle(sim_region *SimRegion, object_transform *Transform, entity *AttackingEntity,
                        rectangle2 AttackSurface, u32 HealAmount)
{
    for(u32 TestEntityIndex = 0;
        TestEntityIndex < SimRegion->EntityCount;
        ++TestEntityIndex)
    {
        entity *TestEntity = SimRegion->Entities + TestEntityIndex;
        entity_stats *TestEntityStats = TestEntity->Stats;
        if((TestEntityStats->HealthMax_Health >> 16) && (TestEntity->ID.Value != AttackingEntity->ID.Value))
        {
            rectangle2 TestEntityRect = RectCenterDim((TestEntity->P + TestEntity->Collision->OffsetP -
                                                       Transform->OffsetP).xy,
                                                      GetDim(TestEntity->Collision->CollisionRect).xy);
            if(RectanglesIntersect(AttackSurface, TestEntityRect))
            {
                u32 MaxHealth = TestEntityStats->HealthMax_Health >> 16;
                u32 Health = TestEntityStats->HealthMax_Health & 0xffff;
                if((Health + HealAmount) > MaxHealth)
                {
                    TestEntityStats->HealthMax_Health = (u32)((MaxHealth << 16) | MaxHealth);
                }
                else
                {
                    TestEntityStats->HealthMax_Health += HealAmount;
                }

                u32 MaxMana = TestEntityStats->ManaMax_Mana >> 16;
                u32 Mana = TestEntityStats->ManaMax_Mana & 0xffff;
                if((Mana + HealAmount) > MaxMana)
                {
                    TestEntityStats->ManaMax_Mana = (u32)((MaxMana << 16) | MaxMana);
                }
                else
                {
                    TestEntityStats->ManaMax_Mana += HealAmount;
                }
            }
        }
    }
}

// NOTE(paul): ==================================== Hero Update ======================================================

inline void
CheckSpellCombination(entity *Entity, hero_entity *HeroData, sphere_type SphereNewType)
{
    sphere_type TempType;
    for(uint32 SphereIndex = 0;
        SphereIndex < ArrayCount(HeroData->SpheresRefIndex);
        ++SphereIndex)
    {
        uint32 RefIndex = HeroData->SpheresRefIndex[SphereIndex];
        entity *Sphere = Entity->References->References[RefIndex].Ptr;
        hero_sphere_entity *SphereData = (hero_sphere_entity *)Sphere->Data;
        SphereData->CircleCenter = Entity->P + V3(0.0f, 1.0f, 0.0f);
                                        
        if(SphereNewType != SphereType_Null)
        {
            TempType = SphereData->Type;
            SphereData->Type = SphereNewType;
            SphereNewType = TempType;                                    
        }
        switch(SphereData->Type)
        {
            case SphereType_Water:
            {
                ++HeroData->Combination[0];
                Sphere->BitmapID = HeroData->SphereBitmapIDs[0];
            } break;

            case SphereType_Wind:
            {
                ++HeroData->Combination[1];
                Sphere->BitmapID = HeroData->SphereBitmapIDs[1];
            } break;

            case SphereType_Fire:
            {
                ++HeroData->Combination[2];
                Sphere->BitmapID = HeroData->SphereBitmapIDs[2];
            } break;
        }
    }

    HeroData->CurrentSpell = HeroData->Combination[0];
    HeroData->CurrentSpell |= HeroData->Combination[1] << 2;
    HeroData->CurrentSpell |= HeroData->Combination[2] << 4;
}

inline void
ClearCombination(hero_entity *HeroData)
{
    HeroData->Combination[0] = 0;
    HeroData->Combination[1] = 0;
    HeroData->Combination[2] = 0;
}

inline u32
SpellTypeToSpellIndex(u8 SpellType)
{
    u32 Result = 0;
    switch(SpellType)
    {
        case SpellType_MagicSword: {Result = 0;} break;
        case SpellType_WaterBall:  {Result = 1;} break;
        case SpellType_IceBall:    {Result = 2;} break;
        case SpellType_Heal:       {Result = 3;} break;
        case SpellType_LightBall:  {Result = 4;} break;
        case SpellType_IceSword:   {Result = 5;} break;
        case SpellType_FireSword:  {Result = 6;} break;
        case SpellType_FireBall:   {Result = 7;} break;
        case SpellType_EnergyBall: {Result = 8;} break;
        case SpellType_BirdStrike: {Result = 9;} break;

            InvalidDefaultCase;
    }

    return(Result);
}

inline void
HeroCastSpell(game_mode_world *WorldMode, audio_state *AudioState, editor_assets *Assets, entity *Entity,
              hero_entity *HeroData, v2 MouseP, render_group *RenderGroup)
{
    hero_spell *Spell = HeroData->Spells + SpellTypeToSpellIndex(HeroData->CurrentSpell);
    PlaySound(AudioState, Spell->Config.CastSpellEffect);
    switch(HeroData->CurrentSpell)
    {
        case SpellType_FireBall:
        case SpellType_WaterBall:
        case SpellType_IceBall:
        case SpellType_LightBall:
        case SpellType_EnergyBall:
        case SpellType_BirdStrike:
        {

            if((s16)((Entity->Stats->ManaMax_Mana & 0xffff) - Spell->Config.ManaCost) >= 0)
            {
                Entity->Stats->ManaMax_Mana -= Spell->Config.ManaCost;

                casted_spell CastedSpell = Spell->Config;
                CastedSpell.Direction = V3(Normalize(HeroData->CastMouseP - Entity->P.xy - V2(0, 0.5f)), 0.0f);
                CastedSpell.BaseP = Entity->TileP;
                CastedSpell.OffsetP = V3(0, 0.5f, 0);
                CastedSpell.dP = V3(0, 0, 0);
            
                entity_id SpellID = AddFlyingSpell(WorldMode, Assets, CastedSpell, Entity->ZLayer);
                AddCollisionRule(WorldMode, SpellID, Entity->ID, false);

                ResetTimer(&Spell->Timer);
            }
        } break;

        case SpellType_Heal:
        {
            if((s16)((Entity->Stats->ManaMax_Mana & 0xffff) - Spell->Config.ManaCost) >= 0)
            {
                Entity->Stats->ManaMax_Mana -= Spell->Config.ManaCost;

                casted_spell CastedSpell = Spell->Config;
                CastedSpell.BaseP = Entity->TileP;
                CastedSpell.OffsetP = V3(0, 0, 0);
                CastedSpell.dP = V3(0, 0, 0);
            
                entity_id SpellID = AddImmidiateSpell(WorldMode, Assets, CastedSpell);
                AddCollisionRule(WorldMode, SpellID, Entity->ID, false);

                ResetTimer(&Spell->Timer);
            }
        } break;
        
        case SpellType_MagicSword:
        {
            HeroData->SwordType = SwordType_Magic;
            ResetTimer(&HeroData->SwordCharmTimer);
            ResetTimer(&Spell->Timer);
        } break;

        case SpellType_IceSword:
        {
            HeroData->SwordType = SwordType_Ice;
            ResetTimer(&HeroData->SwordCharmTimer);
            ResetTimer(&Spell->Timer);
        } break;

        case SpellType_FireSword:
        {
            HeroData->SwordType = SwordType_Fire;
            ResetTimer(&HeroData->SwordCharmTimer);
            ResetTimer(&Spell->Timer);
        } break;
    }

    ClearCombination(HeroData);
}

internal void
HeroAttack(game_mode_world *WorldMode, sim_region *SimRegion, audio_state *AudioState, entity *Entity,
           render_group *RenderGroup, object_transform *Transform, v3 LocalMouseP)
{
    hero_entity *HeroData = (hero_entity *)Entity->Data;
    if(Entity->State == EntityState_CastingSpell)
    {
        HeroCastSpell(WorldMode, AudioState, RenderGroup->Assets, Entity, HeroData, LocalMouseP.xy,
                      RenderGroup);
        ChangeEntityState(Entity, EntityState_Staying);
    }
    else
    {
        rectangle2 AttackSurface = RectCenterDim(Normalize(LocalMouseP.xy - Entity->P.xy), V2(1.0f, 1.0f));

#if EDITOR_INTERNAL
        PushRectOutline(RenderGroup, Transform, AttackSurface, 0.0f, V4(0, 1, 0, 1), 0.03f);
#endif

        u32 Damage = 10;
        switch(HeroData->SwordType)
        {
            case SwordType_Magic: {Damage = 20;} break;
            case SwordType_Ice:   {Damage = 15;} break;
            case SwordType_Fire:  {Damage = 25;} break;
        }

        HitEntitiesInRectangle(SimRegion, AudioState, &WorldMode->EffectsEntropy, Transform,
                               Entity, AttackSurface, Damage);

        ChangeEntityState(Entity, EntityState_Staying);
    }
}

internal as_tile_node *
FindClosestOpenNode(as_tile_node *Node)
{
    as_tile_node *Result = Node;

    u32 UncheckedCount = 0;
    as_tile_node *Unchecked[64] = {};
    Unchecked[UncheckedCount++] = Node;
    
    b32 Found = false;
    as_tile_node *CurrentNode = Node;
    while(!Found && (UncheckedCount < 64))
    {
        as_tile_node *CurrentNode = Unchecked[UncheckedCount - 1];
        --UncheckedCount;

        if(CurrentNode->Obstacle)
        {
            for(u32 NeighbourIndex = 0;
                NeighbourIndex < ArrayCount(Result->Neighbours);
                ++NeighbourIndex)
            {
                as_tile_node *Neighbour = CurrentNode->Neighbours[NeighbourIndex];
                if(Neighbour->Obstacle)
                {
                    Unchecked[UncheckedCount++] = Neighbour;
                }
                else
                {
                    Result = Neighbour;
                    Found = true;
                    break;
                }
            }
        }
        else
        {
            Result = CurrentNode;
            break;
        }
    }

    if(UncheckedCount >= 64)
    {
        Result = Node;
    }
    
    return(Result);
}

inline b32
IsDiagonalNeighbor(s32 X1, s32 Y1, s32 X2, s32 Y2)
{
    b32 Result = false;
    s32 Dx = AbsoluteValue(X2 - X1);
    s32 Dy = AbsoluteValue(Y2 - Y1);

    if((Dx == 1) && (Dy == 1))
    {
        Result = true;
    }

    return(Result);
}

enum diagonal_direction
{
    Direction_None,
    Direction_TopLeft,
    Direction_TopRight,
    Direction_BottomLeft,
    Direction_BottomRight,
};

inline diagonal_direction
GetDiagonalDirection(s32 X1, s32 Y1, s32 X2, s32 Y2)
{
    diagonal_direction Result = Direction_None;
    s32 Dx = X2 - X1;
    s32 Dy = Y2 - Y1;

    if(IsDiagonalNeighbor(X1, Y1, X2, Y2))
    {
        if((Dx == 1) && (Dy == 1))        {Result = Direction_TopRight;}
        else if((Dx == -1) && (Dy == 1))  {Result = Direction_TopLeft;}
        else if((Dx == 1) && (Dy == -1))  {Result = Direction_BottomRight;}
        else if((Dx == -1) && (Dy == -1)) {Result = Direction_BottomLeft;}
    }

    return(Result);
}

internal void
SolveAStar(world *World, entity_move_state *MoveState, sim_region *SimRegion)
{
    TIMED_FUNCTION();

    if(MoveState->StartNode && MoveState->EndNode)
    {
        for(u32 NodeIndex = 0;
            NodeIndex < World->TileNodeCount;
            ++NodeIndex)
        {
            as_tile_node *Node = World->TileNodes + NodeIndex;
            Node->Visited = false;
            Node->GlobalGoal = Real32Maximum;
            Node->LocalGoal = Real32Maximum;
            Node->Parent = 0;
        }

        as_tile_node *CurrentNode = MoveState->StartNode;
        CurrentNode->LocalGoal = 0.0f;
        CurrentNode->GlobalGoal = DistanceBetween(World, MoveState->StartNode, MoveState->EndNode);

        heap *Heap = &World->MinTileNodeHeap;

        sort_entry Key = {};
        Key.Index = MoveState->StartNode->Y*World->TileNodeWidth + MoveState->StartNode->X;
        Key.SortKey = MoveState->StartNode->GlobalGoal;
        MinHeapInsertNode(Heap, Key);

        while((Heap->Size != 0) && (CurrentNode != MoveState->EndNode))
        {
            as_tile_node *TestNode = World->TileNodes + Heap->Nodes[0].Index;
            while((TestNode->Visited) && (Heap->Size != 0))
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
                    b32 Avaliable = true;
                    diagonal_direction Direction = GetDiagonalDirection(CurrentNode->X, CurrentNode->Y, NeighbourNode->X, NeighbourNode->Y);
                    if(Direction)
                    {
                        as_tile_node *FirstAdjacent = 0;
                        as_tile_node *SecondAdjacent = 0;
                        switch(Direction)
                        {
                            case Direction_TopLeft:
                            case Direction_BottomLeft:
                            {
                                FirstAdjacent = World->TileNodes + NeighbourNode->Y*World->TileNodeWidth + (NeighbourNode->X + 1);
                                SecondAdjacent = World->TileNodes + CurrentNode->Y*World->TileNodeWidth + (CurrentNode->X - 1);
                            } break;

                            case Direction_BottomRight:
                            case Direction_TopRight:
                            {
                                FirstAdjacent = World->TileNodes + NeighbourNode->Y*World->TileNodeWidth + (NeighbourNode->X - 1);
                                SecondAdjacent = World->TileNodes + CurrentNode->Y*World->TileNodeWidth + (CurrentNode->X + 1);
                            } break;

                            InvalidDefaultCase;
                        }

                        v2 TileCollisionDim = World->TileDimInMeters.xy;

                        v2 EntityCollisionSimP = Subtract(World, &CurrentNode->TileP, &SimRegion->Origin); 
                        v2 NeighborCollisionSimP = Subtract(World, &NeighbourNode->TileP, &SimRegion->Origin); 

                        if(FirstAdjacent->Obstacle)
                        {
                            v2 TileCollisionSimP0 = Subtract(World, &FirstAdjacent->TileP, &SimRegion->Origin); 
                            rectangle2 TileRect0 = RectCenterDim(TileCollisionSimP0, TileCollisionDim);
                            Avaliable = !LineIntersectsRectangle(EntityCollisionSimP, NeighborCollisionSimP, TileRect0);
                        }

                        if(SecondAdjacent->Obstacle)
                        {
                            v2 TileCollisionSimP1 = Subtract(World, &SecondAdjacent->TileP, &SimRegion->Origin); 
                            rectangle2 TileRect1 = RectCenterDim(TileCollisionSimP1, TileCollisionDim);
                            Avaliable = !LineIntersectsRectangle(EntityCollisionSimP, NeighborCollisionSimP, TileRect1);
                        }
                    }
                        
                    if((!NeighbourNode->Visited) && (!NeighbourNode->Obstacle) && Avaliable)
                    {
                        sort_entry Key = {};
                        Key.Index = NeighbourNode->Y*World->TileNodeWidth + NeighbourNode->X;
                        Key.SortKey = NeighbourNode->GlobalGoal;

                        r32 LowerGoal = CurrentNode->LocalGoal + DistanceBetween(World, CurrentNode, NeighbourNode);
                        if(LowerGoal < NeighbourNode->LocalGoal)
                        {
                            NeighbourNode->Parent = CurrentNode;
                            NeighbourNode->LocalGoal = LowerGoal;

                            NeighbourNode->GlobalGoal = (NeighbourNode->LocalGoal +
                                                         DistanceBetween(World, NeighbourNode, MoveState->EndNode));
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

internal void
Chaikin(v2* InputPoints, s32 InputPointCount, v2** OutputPoints, s32* OutputPointCount)
{
    TIMED_FUNCTION();

    if(InputPointCount < 2)
    {
        *OutputPointCount = InputPointCount;
        *OutputPoints = (v2 *)Platform.AllocateMemory(InputPointCount * sizeof(v2));
        if(*OutputPoints)
        {
            for (s32 I = 0;
                 I < InputPointCount;
                 I++)
            {
                (*OutputPoints)[I] = InputPoints[I];
            }
        }
    }
    else
    {
        *OutputPointCount = (2*(InputPointCount - 1)) + 1;
        *OutputPoints = (v2*)Platform.AllocateMemory(*OutputPointCount * sizeof(v2));

        if(*OutputPoints)
        {
            s32 J = 0;
            for(s32 I = 0;
                I < (InputPointCount - 1);
                I++)

            {
                v2 P0 = InputPoints[I];
                v2 P1 = InputPoints[I + 1];

                (*OutputPoints)[J++] = V2((0.75f*P0.x) + (0.25f*P1.x), (0.75f*P0.y) + (0.25f*P1.y));
                (*OutputPoints)[J++] = V2((0.25f*P0.x) + (0.75f*P1.x), (0.25f*P0.y) + (0.75f*P1.y));
            }

            (*OutputPoints)[*OutputPointCount - 1] = InputPoints[InputPointCount - 1];
        }
        else
        {
            *OutputPointCount = 0;
        }
    }
}

internal void
CalculatePath(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity)
{
    world *World = WorldMode->World;
    entity_move_state *MoveState = Entity->MoveState;    
    heap *MovePointMinHeap = &MoveState->MovePointMinHeap;

    MinHeapExtractNode(MovePointMinHeap);
    MoveState->PointCount = MovePointMinHeap->Size + 1;
    MoveState->Points = (v2 *)Platform.AllocateMemory(MoveState->PointCount*sizeof(v2));
    for(u32 Index = 0;
        Index < MoveState->PointCount;
        ++Index)
    {
        if(Index)
        {
            sort_entry Key = MinHeapExtractNode(MovePointMinHeap);
            as_tile_node *Node = World->TileNodes + Key.Index;
            MoveState->Points[Index] = Subtract(World, &Node->TileP, &SimRegion->Origin);             
        }
        else
        {
            MoveState->Points[Index] = Entity->P.xy;             
        }
    }

    v2 *SmoothedPoints = 0;
    s32 SmoothedPointCount;

    Chaikin(MoveState->Points, MoveState->PointCount, &SmoothedPoints, &SmoothedPointCount);
    Platform.DeallocateMemory(MoveState->Points);
    MoveState->Points = SmoothedPoints;
    MoveState->PointCount = SmoothedPointCount;

    Chaikin(MoveState->Points, MoveState->PointCount, &SmoothedPoints, &SmoothedPointCount);
    Platform.DeallocateMemory(MoveState->Points);
    MoveState->Points = SmoothedPoints;
    MoveState->PointCount = SmoothedPointCount;

    Chaikin(MoveState->Points, MoveState->PointCount, &SmoothedPoints, &SmoothedPointCount);
    Platform.DeallocateMemory(MoveState->Points);
    MoveState->Points = SmoothedPoints;
    MoveState->PointCount = SmoothedPointCount;

    if(MoveState->TilePoints)
    {
        Platform.DeallocateMemory(MoveState->TilePoints);
        MoveState->TilePoints = (world_position *)Platform.AllocateMemory(MoveState->PointCount * sizeof(world_position));
    }
    else
    {
        MoveState->TilePoints = (world_position *)Platform.AllocateMemory(MoveState->PointCount * sizeof(world_position));
    }

    for(u32 Index = 0;
        Index < MoveState->PointCount;
        ++Index)
    {
        v2 Point = MoveState->Points[Index];
        MoveState->TilePoints[Index] = MapIntoTileSpace(World, SimRegion->Origin, Point);
    }
}

internal updated_entity
UpdateHero(game_mode_world *WorldMode, sim_region *SimRegion, controlled_hero *ConHero, entity *Entity,
           v3 LocalMouseP, render_group *RenderGroup)
{
    updated_entity Result = {};
    hero_entity *HeroData = (hero_entity *)Entity->Data;

    heap *MovePointMinHeap = &Entity->MoveState->MovePointMinHeap;
    if(ConHero->Move)
    {
        world_position MouseP = MapIntoTileSpace(WorldMode->World, SimRegion->Origin, LocalMouseP.xy);
        Entity->MoveState->EndNode = GetTileNode(WorldMode->World, MouseP);
        Entity->MoveState->StartNode = GetTileNode(WorldMode->World, Entity->TileP);

        SolveAStar(WorldMode->World, Entity->MoveState, SimRegion);

        if(Entity->MoveState->EndNode)
        {
            ZeroArray(MovePointMinHeap->MaxSize, MovePointMinHeap->Nodes);
            MovePointMinHeap->Size = 0;

            as_tile_node *Node = Entity->MoveState->EndNode;
            while((Node) && (MovePointMinHeap->Size != MovePointMinHeap->MaxSize))
            {
                as_tile_node *ParentNode = Node;
                sort_entry Key = {};
                Key.Index = ParentNode->Y*WorldMode->World->TileNodeWidth + ParentNode->X;
                Key.SortKey = ParentNode->LocalGoal;
                MinHeapInsertNode(MovePointMinHeap, Key);

                Node = Node->Parent;
            }
        }
        
        CalculatePath(WorldMode, SimRegion, Entity);
        
        ChangeEntityState(Entity, EntityState_Moving);
    }

    entity_move_state *MoveState = Entity->MoveState;
    if(Entity->State == EntityState_Moving)
    {
#if 1
        object_transform Flat = DefaultFlatTransform();
        Flat.ChunkZ = 10000;
        for(u32 Index = 0;
            Index < MoveState->PointCount;
            ++Index)
        {
            world_position Point = MoveState->TilePoints[Index];
            PushRect(RenderGroup, &Flat, V3(Subtract(WorldMode->World, &Point, &SimRegion->Origin), 0.0f), V2(0.2f, 0.2f), V4(0, 0, 1, 1));
        }
#endif
    
        if(MoveState->PointCount)
        {
            world_position ClosestP = MoveState->TilePoints[0];

            v2 Delta = Subtract(WorldMode->World, &ClosestP, &Entity->TileP);

            if(MoveState->PointCount < 8)
            {
                Result.ddP = V3(Delta, 0);
            }
            else
            {
                Result.ddP = V3(Normalize(Delta), 0);
            }

            Result.MoveSpec.UnitMaxAccelVector = true;
            Result.MoveSpec.Speed = 50.0f;
            Result.MoveSpec.Drag = 12.0f;

            v2 OffsetDifference = ClosestP.Offset - Entity->TileP.Offset;
            if((ClosestP.TileX == Entity->TileP.TileX) && (ClosestP.TileY == Entity->TileP.TileY))
            {
                OffsetDifference.x = (OffsetDifference.x < 0) ? -OffsetDifference.x : OffsetDifference.x;
                OffsetDifference.y = (OffsetDifference.y < 0) ? -OffsetDifference.y : OffsetDifference.y;

                if((OffsetDifference.x < 0.08f) && (OffsetDifference.y < 0.08f))
                {
                    for(u32 Index = 0;
                        Index < MoveState->PointCount - 1;
                        ++Index)
                    {
                        MoveState->TilePoints[Index] = MoveState->TilePoints[Index + 1];
                    }
                    --MoveState->PointCount;

                    ClosestP = MoveState->TilePoints[0];
                    Delta = Subtract(WorldMode->World, &ClosestP, &Entity->TileP);
                    Result.ddP = V3(Normalize(Delta), 0);
                }
            }
        }
        else
        {
            MoveState->dP = {};
            ChangeEntityState(Entity, EntityState_Staying);
        }
    }

    if(ConHero->Attack)
    {
        ChangeEntityState(Entity, EntityState_Attacking);
        if(Entity->PrevState == EntityState_Moving)
        {
            MoveState->dP = {};
        }
    }
                                
    CheckSpellCombination(Entity, HeroData, ConHero->SphereNewType);
    ClearCombination(HeroData);

    if(ConHero->InvokeAndCastSpell)
    {
        hero_spell *Spell = HeroData->Spells + SpellTypeToSpellIndex(HeroData->CurrentSpell);
        if(CheckTimer(&Spell->Timer))
        {
            HeroData->CastMouseP = LocalMouseP.xy;
            ChangeEntityState(Entity, EntityState_CastingSpell);
            if(Entity->PrevState == EntityState_Moving)
            {
                MoveState->dP = {};
            }
        }
    }
    
    s32 NewFacingDirection = 0;
    if(Entity->State == EntityState_Moving)
    {
        NewFacingDirection = FacingDirectionFromVector(Result.ddP.xy);
    }
    else
    {
        NewFacingDirection = FacingDirectionFromVector(Normalize(LocalMouseP.xy - Entity->P.xy));
    }

    if(NewFacingDirection >= 0)
    {
        Entity->FacingDirection = NewFacingDirection;
    }

    // TODO(paul): Find out how to make it better
#if 0
    if(Entity->FacingDirection == 1)
    {
        for(u32 SphereIndex = 0;
            SphereIndex < ArrayCount(HeroData->SpheresRefIndex);
            ++SphereIndex)
        {
            entity *SphereEntity = Entity->References->References[HeroData->SpheresRefIndex[SphereIndex]].Ptr;
            hero_sphere_entity *SphereData = (hero_sphere_entity *)SphereEntity->Data;
            SphereData->SortBias = 2.0f;
        }
    }
    else
    {
        for(u32 SphereIndex = 0;
            SphereIndex < ArrayCount(HeroData->SpheresRefIndex);
            ++SphereIndex)
        {
            entity *SphereEntity = Entity->References->References[HeroData->SpheresRefIndex[SphereIndex]].Ptr;
            hero_sphere_entity *SphereData = (hero_sphere_entity *)SphereEntity->Data;
            SphereData->SortBias = 0.0f;
        }
    }

    if(WorldMode->GameFinished)
    {
        Entity->FacingDirection = 3;
        Entity->State = EntityState_Staying;
        Entity->Animation->AnimationType = AnimationType_Idle;
        Result.ddP = V3(0, 0, 0);
    }
#endif    
    
    return(Result);
}
// ===================================================================================================================

#if 0
internal void
MonsterDeathEvent(game_mode_world *WorldMode, editor_assets *Assets, entity *Entity)
{
    r32 RandomNumber = RandomBetween(&WorldMode->EffectsEntropy, 0.0f, 1.0f);
    if(RandomNumber > 0.5f)
    {
        AddItem(WorldMode, Assets, Entity->TileP, ItemName_HealPotion);
    }
}

// NOTE(paul):====================================== Necromancer Update ==============================================

internal void
NecromancerAttack(game_mode_world *WorldMode, editor_assets *Assets, sim_region *SimRegion, entity *Entity)
{
    necromancer_entity *EntityData = (necromancer_entity *)Entity->Data;
    switch(Entity->CastSpellType)
    {
        case CastSpellType_0:
        {
            entity *HeroEntity = GetEntityByID(SimRegion, EntityData->ClosestHeroID);

            casted_spell CastedSpell = {};
            if(HeroEntity)
            {
                CastedSpell.Effect = SpellEffect_Dark;
                CastedSpell.ManaCost = 30;
                CastedSpell.Damage = 20;

                CastedSpell.SpellName = Spell_DarkBolt;
                CastedSpell.MagicElement = MagicElement_Dark;
                CastedSpell.ImmidiateAnimationFinishIndex = 6;
                CastedSpell.ProjectileAnimationSpeed = 10;
                CastedSpell.DeathAnimationSpeed = 10;

                CastedSpell.BaseP = HeroEntity->TileP;
                CastedSpell.OffsetP = V3(0, 0, 0);
                CastedSpell.dP = V3(0, 0, 0);
                CastedSpell.RenderHeight = 2.5f;

                CastedSpell.CastSpellEffect.Value = 0;
                CastedSpell.ImpactEffect = 
                    GetSoundEffectForType(Assets, SoundEffect_ThunderImpact);

                entity_id SpellID = AddImmidiateSpell(WorldMode, Assets, CastedSpell);
            }
        } break;

        case CastSpellType_1:
        {
            u32 MaxHealth = Entity->HealthMax_Health >> 16;
            s32 Health = Entity->HealthMax_Health & 0x0000ffff;
            if((Health - 30) <= 0)
            {
                Entity->HealthMax_Health = (u32)((MaxHealth << 16) | 0);
            }
            else
            {
                Entity->HealthMax_Health -= 30;
            }

            // TODO(paul): Add sound effect
            
            AddPossesed(WorldMode, Assets, Entity->TileP, V2(2.0f, 0.0f));
            AddPossesed(WorldMode, Assets, Entity->TileP, V2(-2.0f, 0.0f));
            
        } break;

        case CastSpellType_2:
        {
            // TODO(paul): Add sound effect

            u32 MaxHealth = Entity->HealthMax_Health >> 16;
            u32 Health = Entity->HealthMax_Health & 0x0000ffff;
            if((Health + 10) > MaxHealth)
            {
                Entity->HealthMax_Health = (u32)((MaxHealth << 16) | MaxHealth);
            }
            else
            {
                Entity->HealthMax_Health += 10;
            }
        } break;
    }

    ChangeEntityState(Entity, EntityState_Staying);
    ChangeAnimationType(Entity, AnimationType_Idle);

    timer *Timer = Entity->Timers + GetTimerIndexForCastType(Entity);
    ResetTimer(Timer);
}

internal updated_entity
UpdateNecromancer(game_mode_world *WorldMode, editor_assets *Assets, audio_state *AudioState, sim_region *SimRegion,
                  entity *Entity)
{
    updated_entity Result = {};

    necromancer_entity *EntityData = (necromancer_entity *)Entity->Data;
    if(Entity->State != EntityState_CastingSpell)
    {
        found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
                    
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(8.0f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            s32 NewFacingDirection = FacingDirectionFromVector(Normalize(ClosestHero.Entity->P.xy - Entity->P.xy));
            if(NewFacingDirection >= 0)
            {
                Entity->FacingDirection = NewFacingDirection;
            }

            Entity->dP = V3(0, 0, 0);
            Entity->CastSpellType = CastSpellType_1;
            if(CheckTimerForCastspell(Entity))
            {
                PlaySound(AudioState, GetSoundEffectForType(Assets, SoundEffect_NecromancerSummons));
                ChangeAnimationType(Entity, GetAnimationTypeForCastType(Entity->CastSpellType));
                Entity->State = EntityState_CastingSpell;
            }
            else
            {
                Entity->CastSpellType = CastSpellType_0;
                u32 MaxHealth = Entity->HealthMax_Health >> 16;
                u32 Health = Entity->HealthMax_Health & 0x0000ffff;
                if(CheckTimerForCastspell(Entity) &&
                   (Health > (MaxHealth / 2)))
                {
                    EntityData->ClosestHeroID = ClosestHero.Entity->ID;
                    ChangeAnimationType(Entity, GetAnimationTypeForCastType(Entity->CastSpellType));
                    Entity->State = EntityState_CastingSpell;
                }
                else if(Health <= (MaxHealth / 2))
                {
                    Entity->CastSpellType = CastSpellType_2;
                    if(CheckTimerForCastspell(Entity))
                    {
                        ChangeAnimationType(Entity, GetAnimationTypeForCastType(Entity->CastSpellType));
                        Entity->State = EntityState_CastingSpell;
                    }
                    else
                    {
                        Entity->CastSpellType = CastSpellType_0;
                        if(CheckTimerForCastspell(Entity))
                        {
                            EntityData->ClosestHeroID = ClosestHero.Entity->ID;
                            ChangeAnimationType(Entity, GetAnimationTypeForCastType(Entity->CastSpellType));
                            Entity->State = EntityState_CastingSpell;
                        }
                        else
                        {
                            ChangeAnimationType(Entity, AnimationType_Idle);
                            Entity->State = EntityState_Staying;
                        }
                    }
                }
                else if(Health > (MaxHealth / 2))
                {
                    Entity->CastSpellType = CastSpellType_0;
                    if(CheckTimerForCastspell(Entity))
                    {
                        EntityData->ClosestHeroID = ClosestHero.Entity->ID;
                        ChangeAnimationType(Entity, GetAnimationTypeForCastType(Entity->CastSpellType));
                        Entity->State = EntityState_CastingSpell;
                    }
                    else
                    {
                        Entity->CastSpellType = CastSpellType_2;
                        if(CheckTimerForCastspell(Entity))
                        {
                            ChangeAnimationType(Entity, GetAnimationTypeForCastType(Entity->CastSpellType));
                            Entity->State = EntityState_CastingSpell;
                        }
                        else
                        {
                            ChangeAnimationType(Entity, AnimationType_Idle);
                            Entity->State = EntityState_Staying;
                        }
                    }
                }
                else
                {
                    ChangeAnimationType(Entity, AnimationType_Idle);
                    Entity->State = EntityState_Staying;
                }
            }
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 15.0f;
    Result.MoveSpec.Drag = 4.0f;

    return(Result);
}

// ===================================================================================================================
#endif

// NOTE(paul):====================================== FlyingSpell Update ==============================================
internal updated_entity
UpdateFlyingSpell(game_mode_world *WorldMode, entity *Entity)
{
    updated_entity Result = {};

    flyingspell_entity *SpellData = (flyingspell_entity *)Entity->Data;
    Result.MoveSpec.UnitMaxAccelVector = false;
    Result.MoveSpec.Speed = 100.0f;
    Result.MoveSpec.Drag = 0.0f;

    Result.ddP = SpellData->Direction;
    if(Entity->MoveState->DistanceLimit == 0.0f)
    {
        ClearCollisionRulesFor(WorldMode, Entity->ID);
        ChangeEntityState(Entity, EntityState_Dieing);
        Result.ddP = {};
    }
    else
    {
        ChangeEntityState(Entity, EntityState_Moving);
    }
    
    s32 NewFacingDirection = FacingDirectionFromVector(Result.ddP.xy);
    if(NewFacingDirection >= 0)
    {
        Entity->FacingDirection = NewFacingDirection;
    }
    
    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Immidiate Spell Update ==========================================

internal void
ImmidiateSpellAttack(game_mode_world *WorldMode, sim_region *SimRegion, audio_state *AudioState,
                     entity *Entity, render_group *RenderGroup, object_transform *Transform)
{
    rectangle2 AttackSurface = RectCenterDim(V2(0, 0), V2(1.0f, 1.0f));
    PushRectOutline(RenderGroup, Transform, AttackSurface, 0.0f, V4(0, 1, 0, 1), 0.03f);

    immidiatespell_entity *EntityData = (immidiatespell_entity *)Entity->Data;
    if(EntityData->Type == SpellType_Heal)
    {
//        HealEntitiesInRectangle(SimRegion, Transform, Entity, AttackSurface, EntityData->Damage_Heal);
    }
    else
    {
//        HitEntitiesInRectangle(SimRegion, AudioState, &WorldMode->EffectsEntropy, Transform,
//                               Entity, AttackSurface, EntityData->Damage_Heal);
    }
    
    ChangeEntityState(Entity, EntityState_Dieing);
}

internal updated_entity
UpdateImmidiateSpell(entity *Entity)
{
    updated_entity Result = {};

    immidiatespell_entity *SpellData = (immidiatespell_entity *)Entity->Data;
    Result.MoveSpec.UnitMaxAccelVector = false;
    Result.MoveSpec.Speed = 0.0f;
    Result.MoveSpec.Drag = 0.0f;

    ChangeEntityState(Entity, EntityState_Attacking);

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Golem Update ====================================================
#if 0
internal void
MonsterAttack(sim_region *SimRegion, audio_state *AudioState, random_series *EffectsEntropy, entity *Entity,
              render_group *RenderGroup, object_transform Transform, u32 Damage, v2 Dim)
{
    v2 AttackDirection = FacingDirectionToUnitVector(Entity->FacingDirection);
    v2 RectCenter = 1.5f*AttackDirection;
    rectangle2 AttackSurface = RectCenterDim(AttackDirection, Dim);
//    PushRectOutline(RenderGroup, Transform, AttackSurface, 0.0f, V4(0, 1, 0, 1), 0.03f);

    HitEntitiesInRectangle(SimRegion, AudioState, EffectsEntropy, Transform, Entity, AttackSurface, Damage);

    ChangeEntityState(Entity, EntityState_Staying);
    ChangeAnimationType(Entity, AnimationType_Idle);
}

internal updated_entity
UpdateGolem(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity)
{
    updated_entity Result = {};

    found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
    if(Entity->State != EntityState_Attacking)
    {
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(1.0f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            Entity->dP = V3(0, 0, 0);
            Entity->AttackType = AttackType_0;
            ChangeAnimationType(Entity, AnimationType_Attack0);
            Entity->State = EntityState_Attacking;
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 10.0f;
    Result.MoveSpec.Drag = 4.0f;

    return(Result);
}
#endif
// ===================================================================================================================

// NOTE(paul):====================================== Goblin Beast Update =============================================
#if 0
internal updated_entity
UpdateGoblinBeast(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity, random_series *Series)
{
    updated_entity Result = {};

    if(Entity->State != EntityState_Attacking)
    {
        found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(1.2f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            Entity->dP = V3(0, 0, 0);
            Entity->AttackType = (attack_type)RandomBetween(Series, 0, 1);
            if(Entity->AttackType)
            {
                ChangeAnimationType(Entity, AnimationType_Attack1);
            }
            else
            {
                ChangeAnimationType(Entity, AnimationType_Attack0);
            }
            Entity->State = EntityState_Attacking;
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
                    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 15.0f;
    Result.MoveSpec.Drag = 4.0f;

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Goblin Berserker Update =========================================

internal updated_entity
UpdateGoblinBerserker(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity)
{
    updated_entity Result = {};

    if(Entity->State != EntityState_Attacking)
    {
        found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
                    
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(1.2f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            s32 NewFacingDirection = FacingDirectionFromVector(Normalize(ClosestHero.Entity->P.xy - Entity->P.xy));
            if(NewFacingDirection >= 0)
            {
                Entity->FacingDirection = NewFacingDirection;
            }

            if(Entity->State != EntityState_Attacking)
            {
                Entity->dP = V3(0, 0, 0);
                Entity->AttackType = AttackType_0;
                ChangeAnimationType(Entity, AnimationType_Attack0);
                Entity->State = EntityState_Attacking;
                Entity->EndNode = 0;
            }
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 15.0f;
    Result.MoveSpec.Drag = 4.0f;

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Goblin Rider Update =============================================

internal updated_entity
UpdateGoblinRider(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity, random_series *Series)
{
    updated_entity Result = {};

    if(Entity->State != EntityState_Attacking)
    {
        found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(1.2f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            s32 NewFacingDirection = FacingDirectionFromVector(Normalize(ClosestHero.Entity->P.xy - Entity->P.xy));
            if(NewFacingDirection >= 0)
            {
                Entity->FacingDirection = NewFacingDirection;
            }

            Entity->dP = V3(0, 0, 0);
            Entity->AttackType = (attack_type)RandomBetween(Series, 0, 2);
            if(Entity->AttackType == 1)
            {
                ChangeAnimationType(Entity, AnimationType_Attack1);
            }
            else if(Entity->AttackType == 2)
            {
                ChangeAnimationType(Entity, AnimationType_Attack2);
            }
            else
            {
                ChangeAnimationType(Entity, AnimationType_Attack0);
            }
            Entity->State = EntityState_Attacking;
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
                    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 20.0f;
    Result.MoveSpec.Drag = 4.5f;

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Skeleton Grunt Update ===========================================

internal updated_entity
UpdateSkeletonGrunt(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity)
{
    updated_entity Result = {};

    if(Entity->State != EntityState_Attacking)
    {
        found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 8.0f);
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(1.2f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            s32 NewFacingDirection = FacingDirectionFromVector(Normalize(ClosestHero.Entity->P.xy - Entity->P.xy));
            if(NewFacingDirection >= 0)
            {
                Entity->FacingDirection = NewFacingDirection;
            }

            Entity->dP = V3(0, 0, 0);
            Entity->AttackType = AttackType_0;
            ChangeAnimationType(Entity, AnimationType_Attack0);
            Entity->State = EntityState_Attacking;
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
                    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 10.0f;
    Result.MoveSpec.Drag = 4.0f;

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Skeleton King Update ============================================

internal void
SkeletonKingDeathEvent(game_mode_world *WorldMode, editor_assets *Assets, entity *Entity)
{
    skeleton_king_entity *SkeletonKingData = (skeleton_king_entity *)Entity->Data;
    AddItem(WorldMode, Assets, Entity->TileP, ItemName_MapToTheTrees);

    for(u32 ObstacleIndex = 0;
        ObstacleIndex < SkeletonKingData->ObstacleCount;
        ++ObstacleIndex)
    {
        for(u32 EntityIndex = 0;
            EntityIndex < ArrayCount(WorldMode->EntitiesToDestroy);
            ++EntityIndex)
        {
            entity_id ID = WorldMode->EntitiesToDestroy[EntityIndex];
            if(ID.Value == 0)
            {
                WorldMode->EntitiesToDestroy[EntityIndex] = SkeletonKingData->Obstacles[ObstacleIndex];
                break;
            }
        }
    }
}

internal void
SkeletonKingAttack(game_mode_world *WorldMode, audio_state *AudioState, sim_region *SimRegion, entity *Entity,
                   render_group *RenderGroup, object_transform Transform)
{
    if(Entity->State == EntityState_CastingSpell)
    {
        u32 HealAmount = 20;
        u32 MaxHealth = Entity->HealthMax_Health >> 16;
        u32 Health = Entity->HealthMax_Health & 0xffff;
        if((Health + HealAmount) > MaxHealth)
        {
            PlaySound(AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_SkeletonKingHeal));
            Entity->HealthMax_Health = (u32)((MaxHealth << 16) | MaxHealth);
        }
        else
        {
            Entity->HealthMax_Health += HealAmount;
        }

        ChangeAnimationType(Entity, AnimationType_Idle);
        ChangeEntityState(Entity, EntityState_Staying);

        timer *Timer = Entity->Timers + GetTimerIndexForCastType(Entity);
        Timer->Finished = false;
        Timer->CurrentTime = 0.0f;
    }
    else
    {
        if(Entity->AttackType == AttackType_0)
        {
            MonsterAttack(SimRegion, AudioState, &WorldMode->EffectsEntropy,
                          Entity, RenderGroup, Transform, 35, V2(1.2f, 1.2f));
            Entity->AttackType = AttackType_1;
            ChangeAnimationType(Entity, AnimationType_Attack1);
            Entity->State = EntityState_Attacking;
        }
        else
        {
            MonsterAttack(SimRegion, AudioState, &WorldMode->EffectsEntropy, Entity,
                          RenderGroup, Transform, 35, V2(1.2f, 1.2f));
            ChangeAnimationType(Entity, AnimationType_Idle);
            ChangeEntityState(Entity, EntityState_Staying);
        }
    }
}

internal updated_entity
UpdateSkeletonKing(game_mode_world *WorldMode, editor_assets *Assets, sim_region *SimRegion, entity *Entity)
{
    updated_entity Result = {};

    found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
    skeleton_king_entity *SkeletonKingData = (skeleton_king_entity *)Entity->Data;
    if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(8.0f)))
    {
        Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
        ChangeEntityState(Entity, EntityState_Moving);

        Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
    }
    else if(ClosestHero.Entity)
    {
        s32 NewFacingDirection = FacingDirectionFromVector(Normalize(ClosestHero.Entity->P.xy - Entity->P.xy));
        if(NewFacingDirection >= 0)
        {
            Entity->FacingDirection = NewFacingDirection;
        }

        if((Entity->State != EntityState_Attacking) &&
           (Entity->State != EntityState_CastingSpell))
        {
            Entity->AttackType = AttackType_0;
            Entity->CastSpellType = CastSpellType_0;
            if(CheckTimerForCastspell(Entity))
            {
                Entity->dP = V3(0, 0, 0);
                ChangeAnimationType(Entity, AnimationType_CastSpell0);
                Entity->State = EntityState_CastingSpell;
            }
            else
            {
                if((ClosestHero.DistanceSq > Square(1.2f)))
                {
                    real32 Acceleration = 0.5f;
                    real32 OneOverLength = Acceleration / SquareRoot(ClosestHero.DistanceSq);
                    Result.ddP = OneOverLength*(ClosestHero.Entity->P - Entity->P);
                }
                else
                {
                    Entity->dP = V3(0, 0, 0);
                    ChangeAnimationType(Entity, AnimationType_Attack0);
                    Entity->State = EntityState_Attacking;
                }
            }
        }

        if(!SkeletonKingData->ObstaclesPresent)
        {
            if((ClosestHero.Entity->TileP.TileX < 30) && (ClosestHero.Entity->TileP.TileY < 14))
            {

                asset_vector ObstacleMatchVector = {};
                ObstacleMatchVector.E[Tag_BiomeType] = (r32)BiomeType_AncientForest;
                ObstacleMatchVector.E[Tag_SizeLevel] = (r32)SizeLevel_0;
                asset_vector ObstacleWeightVector = {};
                ObstacleWeightVector.E[Tag_BiomeType] = 1.0f;
                ObstacleWeightVector.E[Tag_SizeLevel] = 1.0f;

                SkeletonKingData->Obstacles[SkeletonKingData->ObstacleCount++] =
                    AddObstacle(WorldMode, Assets, 16, 15, Asset_Stone,
                                &ObstacleMatchVector, &ObstacleWeightVector, 2.0f, V2(2.0f, 1.0f), V3(0.5f, 0, 0));
                SkeletonKingData->Obstacles[SkeletonKingData->ObstacleCount++] =
                    AddObstacle(WorldMode, Assets, 18, 15, Asset_Stone,
                                &ObstacleMatchVector, &ObstacleWeightVector, 2.0f, V2(2.0f, 1.0f), V3(0.5f, 0, 0));
                SkeletonKingData->ObstaclesPresent = true;
            }
        }                    
    }
    else
    {
        Entity->dP = V3(0, 0, 0);
    }

    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 25.0f;
    Result.MoveSpec.Drag = 4.5f;

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Cultist Update ==================================================

internal void
CultistAttack(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity,
              render_group *RenderGroup, object_transform Transform, v3 LocalMouseP)
{
    cultist_entity *EntityData = (cultist_entity *)Entity->Data;
    entity *HeroEntity = GetEntityByID(SimRegion, EntityData->ClosestHeroID);
    
    casted_spell CastedSpell = {};
    if(HeroEntity)
    {
        CastedSpell.Type = SpellType_FireBall;
        CastedSpell.SpellName = (r32)Spell_FireBall;
        CastedSpell.MagicElement = (r32)MagicElement_Fire;

        CastedSpell.Effect = SpellEffect_Fire;
        CastedSpell.ManaCost = 20;
        CastedSpell.Damage = 15;

        CastedSpell.Distance = 10.0f;
        CastedSpell.Direction = V3(Normalize(HeroEntity->P.xy - Entity->P.xy - V2(0, 0.5f)), 0.0f);

        CastedSpell.ImmidiateAnimationFinishIndex = 0;
        CastedSpell.ProjectileAnimationSpeed = 12;
        CastedSpell.DeathAnimationSpeed = 12;

        CastedSpell.RenderHeight = 2.5f;

        CastedSpell.BaseP = Entity->TileP;
        CastedSpell.OffsetP = V3(0, 0.5f, 0);
        CastedSpell.dP = V3(0, 0, 0);
        CastedSpell.CastSpellEffect = GetSoundEffectForType(RenderGroup->Assets, SoundEffect_FireBallCast);
        CastedSpell.ImpactEffect = GetSoundEffectForType(RenderGroup->Assets, SoundEffect_FireBallImpact);

        entity_id SpellID = AddFlyingSpell(WorldMode, RenderGroup->Assets, CastedSpell);
        AddCollisionRule(WorldMode, SpellID, Entity->ID, false);

        ChangeEntityState(Entity, EntityState_Staying);
        ChangeAnimationType(Entity, AnimationType_Idle);

        timer *Timer = Entity->Timers + GetTimerIndexForCastType(Entity);
        Timer->Finished = false;
        Timer->CurrentTime = 0.0f;
    }
}

internal updated_entity
UpdateCultist(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity)
{
    updated_entity Result = {};
    cultist_entity *EntityData = (cultist_entity *)Entity->Data;

    if(Entity->State != EntityState_CastingSpell)
    {
        found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
                    
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(8.0f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            s32 NewFacingDirection = FacingDirectionFromVector(Normalize(ClosestHero.Entity->P.xy - Entity->P.xy));
            if(NewFacingDirection >= 0)
            {
                Entity->FacingDirection = NewFacingDirection;
            }

            Entity->dP = V3(0, 0, 0);
            Entity->CastSpellType = CastSpellType_0;
            if(CheckTimerForCastspell(Entity))
            {
                EntityData->ClosestHeroID = ClosestHero.Entity->ID;
                ChangeAnimationType(Entity, GetAnimationTypeForCastType(Entity->CastSpellType));
                Entity->State = EntityState_CastingSpell;
            }
            else
            {
                ChangeAnimationType(Entity, AnimationType_Idle);
                Entity->State = EntityState_Staying;
            }
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 10.0f;
    Result.MoveSpec.Drag = 4.0f;

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Cultist Update ==================================================

internal void
SkeletonHunterAttack(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity,
                     render_group *RenderGroup, object_transform Transform)
{
    skeleton_hunter_entity *EntityData = (skeleton_hunter_entity *)Entity->Data;
    entity *HeroEntity = GetEntityByID(SimRegion, EntityData->ClosestHeroID);
    
    casted_spell CastedSpell = {};
    if(HeroEntity)
    {
        CastedSpell.Damage = 15;

        CastedSpell.Distance = 6.0f;
        CastedSpell.Direction = V3(Normalize(HeroEntity->P.xy - Entity->P.xy - V2(0, 0.5f)), 0.0f);

        CastedSpell.ImmidiateAnimationFinishIndex = 0;
        CastedSpell.ProjectileAnimationSpeed = 0;
        CastedSpell.DeathAnimationSpeed = 0;

        CastedSpell.RenderHeight = 1.0f;

        CastedSpell.BaseP = Entity->TileP;
        CastedSpell.OffsetP = V3(0, 0.5f, 0);
        CastedSpell.dP = V3(0, 0, 0);

        entity_id SpellID = AddArrowProjectile(WorldMode, RenderGroup->Assets, CastedSpell);
        AddCollisionRule(WorldMode, SpellID, Entity->ID, false);

        ChangeEntityState(Entity, EntityState_Staying);
        ChangeAnimationType(Entity, AnimationType_Idle);

        timer *Timer = Entity->Timers + GetTimerIndexForAttackType(Entity);
        Timer->Finished = false;
        Timer->CurrentTime = 0.0f;
    }
}

internal updated_entity
UpdateSkeletonHunter(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity)
{
    updated_entity Result = {};
    skeleton_hunter_entity *EntityData = (skeleton_hunter_entity *)Entity->Data;

    if(Entity->State != EntityState_Attacking)
    {
        found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
                    
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(5.0f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            s32 NewFacingDirection = FacingDirectionFromVector(Normalize(ClosestHero.Entity->P.xy - Entity->P.xy));
            if(NewFacingDirection >= 0)
            {
                Entity->FacingDirection = NewFacingDirection;
            }

            Entity->dP = V3(0, 0, 0);
            Entity->AttackType = AttackType_0;
            if(CheckTimerForAttack(Entity))
            {
                EntityData->ClosestHeroID = ClosestHero.Entity->ID;
                ChangeAnimationType(Entity, GetAnimationTypeForAttackType(Entity->AttackType));
                Entity->State = EntityState_Attacking;
            }
            else
            {
                ChangeAnimationType(Entity, AnimationType_Idle);
                Entity->State = EntityState_Staying;
            }
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 10.0f;
    Result.MoveSpec.Drag = 4.0f;

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Possesed Update =================================================
internal updated_entity
UpdatePossesed(game_mode_world *WorldMode, sim_region *SimRegion, entity *Entity)
{
    updated_entity Result = {};
    if(Entity->State != EntityState_Attacking)
    {
        found_entity ClosestHero = FindClosestEntityOfType(SimRegion, Entity, EntityType_Hero, 10.0f);
                    
        if(ClosestHero.Entity && (ClosestHero.DistanceSq > Square(1.0f)))
        {
            Entity->EndNode = GetTileNode(WorldMode->World, ClosestHero.Entity->TileP);
            ChangeEntityState(Entity, EntityState_Moving);

            Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity);
        }
        else if(ClosestHero.Entity)
        {
            s32 NewFacingDirection = FacingDirectionFromVector(Normalize(ClosestHero.Entity->P.xy - Entity->P.xy));
            if(NewFacingDirection >= 0)
            {
                Entity->FacingDirection = NewFacingDirection;
            }

            Entity->dP = V3(0, 0, 0);
            Entity->AttackType = AttackType_0;
            ChangeAnimationType(Entity, AnimationType_Attack0);
            Entity->State = EntityState_Attacking;
        }
        else
        {
            Entity->dP = V3(0, 0, 0);
        }
    }
    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 10.0f;
    Result.MoveSpec.Drag = 4.0f;

    return(Result);
}

// ===================================================================================================================
#endif
// NOTE(paul):====================================== MagicSphere Update ==============================================
internal updated_entity
UpdateMagicSphere(entity *Entity, r32 dt)
{
    updated_entity Result = {};

    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 0.0f;
    Result.MoveSpec.Drag = 0.0f;

    hero_sphere_entity *Data = (hero_sphere_entity *)Entity->Data;
    Data->tMove += dt;
    if(Data->tMove > 2.0f*Pi32)
    {
        Data->tMove -= 2.0f*Pi32;
    }

    real32 Radius = 0.5f;                     
    v3 P = V3((Data->CircleCenter.x + Radius*Cos(4.0f*Data->tMove)),
              (Data->CircleCenter.y + Radius*Sin(4.0f*Data->tMove)),
              0.0f);
    Entity->P = P;

    return(Result);
}

// ===================================================================================================================




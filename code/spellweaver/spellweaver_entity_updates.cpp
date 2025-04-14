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
    for(u32 TestEntityIndex = 0;
        TestEntityIndex < SimRegion->EntityCount;
        ++TestEntityIndex)
    {
        entity *TestEntity = SimRegion->Entities + TestEntityIndex;
        if(!(AttackingEntity->GeneralType == GeneralType_Enemy && TestEntity->GeneralType == GeneralType_Enemy))
        {
            if((TestEntity->HealthMax_Health >> 16) && (TestEntity->ID.Value != AttackingEntity->ID.Value))
            {
                rectangle2 TestEntityRect = RectCenterDim((TestEntity->P + TestEntity->Collision->OffsetP -
                                                           Transform->OffsetP).xy,
                                                          GetDim(TestEntity->Collision->CollisionRect).xy);
                if(RectanglesIntersect(AttackSurface, TestEntityRect))
                {
                    u32 RandomSound = RandomBetween(EffectsEntropy, 0, 2);
                    sound_id SoundID = AttackingEntity->AttackImpactSound[RandomSound];
                    PlaySound(AudioState, SoundID);
                    TestEntity->HealthMax_Health -= Damage;
                    if((s16)(TestEntity->HealthMax_Health & 0xffff) <= 0)
                    {
                        TestEntity->HealthMax_Health &= 0xffff0000;
                    }
                }
            }
        }
    }
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
        if((TestEntity->HealthMax_Health >> 16) && (TestEntity->ID.Value != AttackingEntity->ID.Value))
        {
            rectangle2 TestEntityRect = RectCenterDim((TestEntity->P + TestEntity->Collision->OffsetP -
                                                       Transform->OffsetP).xy,
                                                      GetDim(TestEntity->Collision->CollisionRect).xy);
            if(RectanglesIntersect(AttackSurface, TestEntityRect))
            {
                u32 MaxHealth = TestEntity->HealthMax_Health >> 16;
                u32 Health = TestEntity->HealthMax_Health & 0xffff;
                if((Health + HealAmount) > MaxHealth)
                {
                    TestEntity->HealthMax_Health = (u32)((MaxHealth << 16) | MaxHealth);
                }
                else
                {
                    TestEntity->HealthMax_Health += HealAmount;
                }

                u32 MaxMana = TestEntity->ManaMax_Mana >> 16;
                u32 Mana = TestEntity->ManaMax_Mana & 0xffff;
                if((Mana + HealAmount) > MaxMana)
                {
                    TestEntity->ManaMax_Mana = (u32)((MaxMana << 16) | MaxMana);
                }
                else
                {
                    TestEntity->ManaMax_Mana += HealAmount;
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
        entity *Sphere = Entity->References[RefIndex].Ptr;
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
DrawHeroHealthBar(render_group *RenderGroup, entity *Entity, text_config TextConfig)
{
    object_transform BarTransform_ = DefaultFlatTransform();
    object_transform *BarTransform = &BarTransform_;
    
    u16 MaxHealth = (u16)(Entity->HealthMax_Health >> 16);
    u16 Health = (u16)(Entity->HealthMax_Health & 0xffff);

    u16 MaxMana = (u16)(Entity->ManaMax_Mana >> 16);
    u16 Mana = (u16)(Entity->ManaMax_Mana & 0xffff);

    r32 Len = 5.5f;
    r32 HealthRatio = (r32)Health / (r32)MaxHealth;
    r32 HealthHalfRatio = 0.5f*HealthRatio*Len;

    r32 ManaRatio = (r32)Mana / (r32)MaxMana;
    r32 ManaHalfRatio = 0.5f*ManaRatio*Len;

    char Buffer[16];
    _snprintf_s(Buffer, sizeof(Buffer), "%u/%u", Health, MaxHealth);
    TextConfig.TextTransform.OffsetP = V3(-13.5f, 8.17f, 0.0f);
    TextConfig.TextShadowTransform.OffsetP = V3(-15.465f, 10.178f, 0.0f);
    TextConfig.FontScale = 0.014;
    TextConfig.Color = V4(1, 1, 1, 1);
    TextOutAt(RenderGroup, TextConfig, Buffer, 0);

    BarTransform->OffsetP = V3(-11.24f, 7.48f, 1.0f);
    BarTransform->ChunkZ = 10100;
    PushRectOutline(RenderGroup, BarTransform, V3(-0.5f*Len + HealthHalfRatio, 0, 0),
                    V2(Len*HealthRatio, 0.5f), V4(0.13f, 0.69f, 0.298f, 0.9f), 0.15f);
    BarTransform->ChunkZ = 10110;
    PushRect(RenderGroup, BarTransform, V3(-0.5f*Len + HealthHalfRatio, 0, 0),
             V2(Len*HealthRatio, 0.5f), V4(0.71f, 0.9f, 0.11f, 0.8f));

    if(Mana == 0)
    {
        BarTransform->ChunkZ = 10110;
        PushRect(RenderGroup, BarTransform, V3(-0.5f*Len + ManaHalfRatio, 0, 0),
                 V2(Len*ManaRatio, 0.5f), V4(0.25f, 0.28f, 0.8f, 1));

        TextConfig.TextTransform.OffsetP = V3(-13.5f, 7.12f, 0.0f);
        TextConfig.TextShadowTransform.OffsetP = V3(-15.465f, 9.128f, 0.0f);
        TextConfig.FontScale = 0.016;
        TextConfig.Color = V4(1, 1, 1, 1);
        TextOutAt(RenderGroup, TextConfig, "No Mana", 0);
    }
    else
    {

        _snprintf_s(Buffer, sizeof(Buffer), "%u/%u", Mana, MaxMana);
        TextConfig.TextTransform.OffsetP = V3(-13.5f, 7.22f, 0.0f);
        TextConfig.TextShadowTransform.OffsetP = V3(-15.465f, 9.228f, 0.0f);
        TextConfig.FontScale = 0.014;
        TextConfig.Color = V4(1, 1, 1, 1);
        TextOutAt(RenderGroup, TextConfig, Buffer, 0);

        BarTransform->OffsetP = V3(-11.24f, 6.625f, 1.0f);
        BarTransform->ChunkZ = 10100;
        PushRectOutline(RenderGroup, BarTransform, V3(-0.5f*Len + ManaHalfRatio, 0, 0),
                        V2(Len*ManaRatio, 0.5f), V4(0.0f, 0.16f, 0.91f, 0.9f), 0.15f);

        BarTransform->ChunkZ = 10110;
        PushRect(RenderGroup, BarTransform, V3(-0.5f*Len + ManaHalfRatio, 0, 0),
                 V2(Len*ManaRatio, 0.5f), V4(0.25f, 0.28f, 0.8f, 0.8f));
    }
}

inline void
DrawHeroSpellBar(render_group *RenderGroup, entity *Entity, text_config TextConfig)
{
    object_transform Transform_ = DefaultFlatTransform();
    Transform_.ChunkZ = 900;

    object_transform *Transform = &Transform_;
    
    hero_entity *HeroData = (hero_entity *)Entity->Data;
    Transform->OffsetP = V3(-0.6f, -6.4f, 1.0f);
    Transform->ChunkZ = 10000;

    for(u32 SphereIndex = 0;
        SphereIndex < ArrayCount(HeroData->SpheresRefIndex);
        ++SphereIndex)
    {
        uint32 RefIndex = HeroData->SpheresRefIndex[SphereIndex];
        entity *Sphere = Entity->References[RefIndex].Ptr;
        hero_sphere_entity *SphereData = (hero_sphere_entity *)Sphere->Data;
                    
        bitmap_id BitmapID = HeroData->SphereBitmapIDs[0];
        if(SphereData->Type == SphereType_Water)
        {
            BitmapID = HeroData->SphereBitmapIDs[0];
        }
        else if(SphereData->Type == SphereType_Wind)
        {
            BitmapID = HeroData->SphereBitmapIDs[1];
        }
        else if(SphereData->Type == SphereType_Fire)
        {
            BitmapID = HeroData->SphereBitmapIDs[2];
        }

        PushBitmap(RenderGroup, Transform, BitmapID, 0.5f, V3(0, 0, 0));
        Transform->OffsetP.x += 0.6f;
    }

    Transform->OffsetP = V3(-1.35f, -7.2f, 1.0f);
    PushBitmap(RenderGroup, Transform, HeroData->SphereBitmapIDs[0], 0.75f, V3(0, 0, 0));
    PushBitmap(RenderGroup, Transform, HeroData->SphereBitmapIDs[1], 0.75f, V3(0.9f, 0, 0));
    PushBitmap(RenderGroup, Transform, HeroData->SphereBitmapIDs[2], 0.75f, V3(1.8f, 0, 0));

    TextConfig.TextTransform.OffsetP = Transform->OffsetP + V3(-0.45f, 0.3f, 0);
    TextConfig.TextShadowTransform.OffsetP = Transform->OffsetP + V3(-2.43f, 2.28f, 0.0f);
    TextConfig.Color = V4(1, 1, 1, 1);
    TextConfig.FontScale = 0.008f;
    TextConfig.TextTransform.ChunkZ = 10010;
    TextConfig.TextShadowTransform.ChunkZ = 10000;

    object_transform RectTransform = DefaultFlatTransform();
    RectTransform.OffsetP = TextConfig.TextTransform.OffsetP;
    RectTransform.ChunkZ = TextConfig.TextTransform.ChunkZ - 1000;
    PushRect(RenderGroup, &RectTransform, V3(0.06f, 0.0725f, 0), V2(0.3f, 0.3f), V4(0, 0, 0, 0.4f));
    TextOutAt(RenderGroup, TextConfig, "1", 0);

    TextConfig.TextTransform.OffsetP += V3(0.9f, 0, 0);
    TextConfig.TextShadowTransform.OffsetP += V3(0.9f, 0, 0);
    RectTransform.OffsetP = TextConfig.TextTransform.OffsetP;
    PushRect(RenderGroup, &RectTransform, V3(0.06f, 0.0725f, 0), V2(0.3f, 0.3f), V4(0, 0, 0, 0.4f));
    TextOutAt(RenderGroup, TextConfig, "2", 0);

    TextConfig.TextTransform.OffsetP += V3(0.9f, 0, 0);
    TextConfig.TextShadowTransform.OffsetP += V3(0.9f, 0, 0);
    RectTransform.OffsetP = TextConfig.TextTransform.OffsetP;
    PushRect(RenderGroup, &RectTransform, V3(0.06f, 0.0725f, 0), V2(0.3f, 0.3f), V4(0, 0, 0, 0.4f));
    TextOutAt(RenderGroup, TextConfig, "3", 0);

    TextConfig.TextTransform.OffsetP += V3(0.9f, 0, 0);
    TextConfig.TextShadowTransform.OffsetP += V3(0.9f, 0, 0);
    RectTransform.OffsetP = TextConfig.TextTransform.OffsetP;
    PushRect(RenderGroup, &RectTransform, V3(0.06f, 0.0725f, 0), V2(0.3f, 0.3f), V4(0, 0, 0, 0.4f));
    TextOutAt(RenderGroup, TextConfig, "Q", 0);
    
    Transform->OffsetP = V3(-13.65f, -2.35f, 1.0f);
    TextConfig.FontScale = 0.008f;
    for(u32 SpellIndex = 0;
        SpellIndex < ArrayCount(HeroData->Spells);
        ++SpellIndex)
    {
        hero_spell *Spell = HeroData->Spells + SpellIndex;
        u8 WaterSphereCount = (u8)(Spell->Type & 0x3);
        u8 WindSphereCount = (u8)(Spell->Type & 0x0c) >> 2;
        u8 FireSphereCount = (u8)(Spell->Type & 0x30) >> 4;

        v4 Color = V4(1, 1, 1, 1);
        if(!CheckTimer(&Spell->Timer))
        {
            Color = V4(0.5f, 0.5f, 0.5f, 1);
        }
        
        for(u32 WaterSphere = 0;
            WaterSphere < WaterSphereCount;
            ++WaterSphere)
        {
            PushBitmap(RenderGroup, Transform, HeroData->SphereBitmapIDs[0], 0.4f, V3(0, 0, 0), Color);
            Transform->OffsetP.x += 0.5f;
        }

        for(u32 WindSphere = 0;
            WindSphere < WindSphereCount;
            ++WindSphere)
        {
            PushBitmap(RenderGroup, Transform, HeroData->SphereBitmapIDs[1], 0.4f, V3(0, 0, 0), Color);
            Transform->OffsetP.x += 0.5f;
        }

        for(u32 FireSphere = 0;
            FireSphere < FireSphereCount;
            ++FireSphere)
        {
            PushBitmap(RenderGroup, Transform, HeroData->SphereBitmapIDs[2], 0.4f, V3(0, 0, 0), Color);
            Transform->OffsetP.x += 0.5f;
        }

        TextConfig.TextTransform.OffsetP = Transform->OffsetP - V3(0.15f, 0.1f, 0);
        TextConfig.TextShadowTransform.OffsetP = Transform->OffsetP + V3(-2.12f, 1.88f, 0.0f);
        TextConfig.Color = Color;
        
        switch(Spell->Type)
        {
            case SpellType_FireBall:
            {
                TextOutAt(RenderGroup, TextConfig, "Fire Ball", 0);
            } break;

            case SpellType_WaterBall:
            {
                TextOutAt(RenderGroup, TextConfig, "Water Ball", 0);
            } break;

            case SpellType_IceBall:
            {
                TextOutAt(RenderGroup, TextConfig, "Ice Ball", 0);
            } break;

            case SpellType_LightBall:
            {
                TextOutAt(RenderGroup, TextConfig, "Light Ball", 0);
            } break;

            case SpellType_EnergyBall:
            {
                TextOutAt(RenderGroup, TextConfig, "Energy Ball", 0);
            } break;

            case SpellType_BirdStrike:
            {
                TextOutAt(RenderGroup, TextConfig, "Bird Strike", 0);
            } break;

            case SpellType_Heal:
            {
                TextOutAt(RenderGroup, TextConfig, "Heal", 0);
            } break;
        
            case SpellType_MagicSword:
            {
                TextOutAt(RenderGroup, TextConfig, "Sword Charm Magic", 0);
            } break;

            case SpellType_IceSword:
            {
                TextOutAt(RenderGroup, TextConfig, "Sword Charm Ice", 0);
            } break;

            case SpellType_FireSword:
            {
                TextOutAt(RenderGroup, TextConfig, "Sword Charm Fire", 0);
            } break;
        }

        Transform->OffsetP.x -= 3.0f*0.5f;
        Transform->OffsetP.y -= 0.5f;
    }

    bitmap_id MouseLeftButton = GetFirstBitmapFrom(RenderGroup->Assets, Asset_MouseLeftButton);
    TextConfig.TextTransform.OffsetP = Transform->OffsetP + V3(0.45f, -0.1f, 0);
    TextConfig.TextShadowTransform.OffsetP = Transform->OffsetP + V3(-1.52f, 1.88f, 0.0f);
    PushBitmap(RenderGroup, Transform, MouseLeftButton, 0.45f, V3(0, 0, 0));
    TextOutAt(RenderGroup, TextConfig, "Melee Attack", 0);
    
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

            if((s16)((Entity->ManaMax_Mana & 0xffff) - Spell->Config.ManaCost) >= 0)
            {
                Entity->ManaMax_Mana -= Spell->Config.ManaCost;

                casted_spell CastedSpell = Spell->Config;
                CastedSpell.Direction = V3(Normalize(HeroData->CastMouseP - Entity->P.xy - V2(0, 0.5f)), 0.0f);
                CastedSpell.BaseP = Entity->TileP;
                CastedSpell.OffsetP = V3(0, 0.5f, 0);
                CastedSpell.dP = V3(0, 0, 0);
            
                entity_id SpellID = AddFlyingSpell(WorldMode, Assets, CastedSpell);
                AddCollisionRule(WorldMode, SpellID, Entity->ID, false);

                ResetTimer(&Spell->Timer);
            }
        } break;

        case SpellType_Heal:
        {
            if((s16)((Entity->ManaMax_Mana & 0xffff) - Spell->Config.ManaCost) >= 0)
            {
                Entity->ManaMax_Mana -= Spell->Config.ManaCost;

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
        ChangeAnimationType(Entity, AnimationType_Idle);
        ChangeEntityState(Entity, EntityState_Staying);
    }
    else
    {
        rectangle2 AttackSurface = RectCenterDim(Normalize(LocalMouseP.xy - Entity->P.xy), V2(1.0f, 1.0f));
//        PushRectOutline(RenderGroup, Transform, AttackSurface, 0.0f, V4(0, 1, 0, 1), 0.03f);

        u32 Damage = 10;
        switch(HeroData->SwordType)
        {
            case SwordType_Magic: {Damage = 20;} break;
            case SwordType_Ice:   {Damage = 15;} break;
            case SwordType_Fire:  {Damage = 25;} break;
        }

        HitEntitiesInRectangle(SimRegion, AudioState, &WorldMode->EffectsEntropy, Transform,
                               Entity, AttackSurface, Damage);

        ChangeAnimationType(Entity, AnimationType_Idle);
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

internal v3
UpdateEntityMovement(world *World, sim_region *SimRegion, entity *Entity, render_group *RenderGroup = 0)
{
    v3 Result = {};

    heap *MovePointMaxHeap = &Entity->MovePointMaxHeap;
    if((Entity->State == EntityState_Moving) && (Entity->EndNode))
    {
        Entity->StartNode = GetTileNode(World, Entity->TileP);
        Entity->StartNode = FindClosestOpenNode(Entity->StartNode);

        SolveAStarForTileNodes(World, SimRegion->Bounds, SimRegion->Origin,
                               Entity->EndNode, Entity->StartNode);

        if(Entity->EndNode)
        {
            ZeroArray(MovePointMaxHeap->MaxSize, MovePointMaxHeap->Nodes);
            MovePointMaxHeap->Size = 0;

            as_tile_node *Node = Entity->StartNode;
            while((Node->Parent) && (MovePointMaxHeap->Size != MovePointMaxHeap->MaxSize))
            {
                as_tile_node *ParentNode = Node->Parent;
                sort_entry Key = {};
                Key.Index = ParentNode->Y*WORLD_TILE_NODE_COUNT_PER_DIM + ParentNode->X;
                Key.SortKey = ParentNode->LocalGoal;
                MaxHeapInsertNode(MovePointMaxHeap, Key);

                Node = ParentNode;
            }
        }
    }

    if(Entity->EndNode && MovePointMaxHeap->Size)
    {
        sort_entry NodeKey = MovePointMaxHeap->Nodes[0];
        as_tile_node *NextNode = World->TileNodes + NodeKey.Index;
        
        v2 Delta = Subtract(World, &NextNode->TileP, &SimRegion->Origin);
        rectangle2 NodeRect = RectCenterDim(Delta, V2(0.25f, 0.25f));

#if SPELLWEAVER_INTERNAL
        if(RenderGroup)
        {
            PushRect(RenderGroup, DefaultFlatTransform(), NodeRect, 3.0f);
        }
#endif       
        if(IsInRectangle(NodeRect, Entity->P.xy))
//        if(RectanglesIntersect(EntityRect, NodeRect))
        {
            MaxHeapExtractNode(MovePointMaxHeap);
        }
        else
        {
            v2 ddP = Normalize(Subtract(World, &NextNode->TileP, &Entity->TileP));
            Result = V3(ddP, 0);
        }
    }
    else
    {
        Entity->dP = V3(0, 0, 0);
        Entity->EndNode = 0;
    }
    
    return(Result);
}

internal updated_entity
UpdateHero(game_mode_world *WorldMode, sim_region *SimRegion, controlled_hero *ConHero, entity *Entity,
           v3 LocalMouseP, render_group *RenderGroup)
{
    updated_entity Result = {};
    hero_entity *HeroData = (hero_entity *)Entity->Data;

    if(ConHero->Move)
    {
        Entity->EndNode = GetTileNode(WorldMode->World, MapIntoTileSpace(WorldMode->World, WorldMode->CameraP, LocalMouseP.xy));
        Entity->EndNode = FindClosestOpenNode(Entity->EndNode);
        
        ChangeEntityState(Entity, EntityState_Moving);
        ConHero->Move = false;
    }

//    rectangle2 EntityRect = RectCenterDim(Entity->P.xy, V2(0.25f, 0.25f));
//    PushRect(RenderGroup, DefaultFlatTransform(), EntityRect, 3.0f, V4(0, 1, 0, 1));

    Result.ddP = UpdateEntityMovement(WorldMode->World, SimRegion, Entity, RenderGroup);
    
    Result.MoveSpec.UnitMaxAccelVector = true;
    Result.MoveSpec.Speed = 40.0f;
    Result.MoveSpec.Drag = 5.8f;

    if(ConHero->Attack || ConHero->InvokeAndCastSpell)
    {
        Result.ddP = V3(0, 0, 0);
        Entity->EndNode = 0;
    }

    if(Entity->FacingDirection == 1)
    {
        for(u32 SphereIndex = 0;
            SphereIndex < ArrayCount(HeroData->SpheresRefIndex);
            ++SphereIndex)
        {
            entity *SphereEntity = Entity->References[HeroData->SpheresRefIndex[SphereIndex]].Ptr;
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
            entity *SphereEntity = Entity->References[HeroData->SpheresRefIndex[SphereIndex]].Ptr;
            hero_sphere_entity *SphereData = (hero_sphere_entity *)SphereEntity->Data;
            SphereData->SortBias = 0.0f;
        }
    }

    CheckSpellCombination(Entity, HeroData, ConHero->SphereNewType);
    ClearCombination(HeroData);
                                
    if(ConHero->InvokeAndCastSpell)
    {
        hero_spell *Spell =
            HeroData->Spells + SpellTypeToSpellIndex(HeroData->CurrentSpell);

        if(CheckTimer(&Spell->Timer))
        {
            HeroData->CastMouseP = LocalMouseP.xy;
            ChangeAnimationType(Entity, AnimationType_CastSpell0);
            ChangeEntityState(Entity, EntityState_CastingSpell);
        }
    }

    if(ConHero->Attack)
    {
        ChangeAnimationType(Entity, AnimationType_Attack0);
        ChangeEntityState(Entity, EntityState_Attacking);
    }

    found_entity FoundNPC = FindClosestEntityOfType(SimRegion, Entity, EntityType_NPC, 5.0f);
                    
    if(FoundNPC.Entity)
    {
        HeroData->ClosestNPC = FoundNPC;
        if(ConHero->Action)
        {
            WorldMode->TalkingEntityID = FoundNPC.Entity->ID;
            WorldMode->UpdateMode = UpdateMode_Conversation;
        }
    }
    else
    {
        HeroData->ClosestNPC = {};
    }
    
    
    s32 NewFacingDirection = FacingDirectionFromVector(Normalize(LocalMouseP.xy - Entity->P.xy));
    if(NewFacingDirection >= 0)
    {
        Entity->FacingDirection = NewFacingDirection;
    }

    if(WorldMode->GameFinished)
    {
        Entity->FacingDirection = 3;
        Entity->State = EntityState_Staying;
        Entity->AnimationType = AnimationType_Idle;
        Result.ddP = V3(0, 0, 0);
    }
    
    
    return(Result);
}
// ===================================================================================================================

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
    if(Entity->DistanceLimit == 0.0f)
    {
        ClearCollisionRulesFor(WorldMode, Entity->ID);
        ChangeEntityState(Entity, EntityState_Dieing);
        ChangeAnimationType(Entity, AnimationType_Death);
        Result.ddP = V3(0, 0, 0);
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
//    PushRectOutline(RenderGroup, Transform, AttackSurface, 0.0f, V4(0, 1, 0, 1), 0.03f);

    immidiatespell_entity *EntityData = (immidiatespell_entity *)Entity->Data;
    if(EntityData->Type == SpellType_Heal)
    {
        HealEntitiesInRectangle(SimRegion, Transform, Entity, AttackSurface, EntityData->Damage_Heal);
    }
    else
    {
        HitEntitiesInRectangle(SimRegion, AudioState, &WorldMode->EffectsEntropy, Transform,
                               Entity, AttackSurface, EntityData->Damage_Heal);
    }
    
    ChangeEntityState(Entity, EntityState_Dieing);
    ChangeAnimationType(Entity, AnimationType_Death);
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
    ChangeAnimationType(Entity, AnimationType_Attack0);

    return(Result);
}

// ===================================================================================================================

// NOTE(paul):====================================== Golem Update ====================================================

internal void
MonsterAttack(sim_region *SimRegion, audio_state *AudioState, random_series *EffectsEntropy, entity *Entity,
              render_group *RenderGroup, object_transform *Transform, u32 Damage, v2 Dim)
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

// ===================================================================================================================

// NOTE(paul):====================================== Goblin Beast Update =============================================

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
                   render_group *RenderGroup, object_transform *Transform)
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
                ObstacleMatchVector.E[Tag_BiomeType] = BiomeType_AncientForest;
                ObstacleMatchVector.E[Tag_SizeLevel] = SizeLevel_0;
                asset_vector ObstacleWeightVector = {};
                ObstacleWeightVector.E[Tag_BiomeType] = 1;
                ObstacleWeightVector.E[Tag_SizeLevel] = 1;

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
              render_group *RenderGroup, object_transform *Transform, v3 LocalMouseP)
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
                     render_group *RenderGroup, object_transform *Transform)
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




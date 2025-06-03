/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
#include "spellweaver_entity_utilities.cpp"
#include "spellweaver_entity_collision.cpp"
#include "spellweaver_entity_create.cpp"
#include "spellweaver_entity_updates.cpp"
//#include "spellweaver_entity_quests.cpp"

inline u32
UpdateSpriteIndex(entity *Entity, r32 Time, u32 SpriteCount, u32 Speed)
{
    u32 Result = 0;
    if(Entity->Animation->AnimationTypeHaveChanged)
    {
        s32 Index = GetSpriteIndex(Time, SpriteCount, 0, Speed);
        Entity->Animation->SpriteSheetOffset = Index;
        Entity->Animation->AnimationTypeHaveChanged = false;
    }

    if(Entity->Animation->SpriteSheetOffset > SpriteCount)
    {
        s32 Index = GetSpriteIndex(Time, SpriteCount, 0, Speed);
        Entity->Animation->SpriteSheetOffset = Index;
    }
                        
    s32 SpriteIndex = GetSpriteIndex(Time, SpriteCount, Entity->Animation->SpriteSheetOffset, Speed);
    Assert(SpriteIndex >= 0);

    Result = SpriteIndex;
    return(Result);
}

internal void
EntityAttack(world_state *WorldState, audio_state *AudioState, sim_region *SimRegion, entity *Entity,
             render_group *RenderGroup, object_transform *Transform, v3 LocalMouseP)
{
    // TODO(paul): Add more different entities that can attack
    random_series *EffectsEntropy = &WorldState->EffectsEntropy;
    switch(Entity->Type)
    {
        case EntityType_Hero:
        {
            HeroAttack(WorldState, SimRegion, AudioState, Entity, RenderGroup, Transform, LocalMouseP);
        } break;

        case EntityType_ImmidiateSpell:
        {
            ImmidiateSpellAttack(WorldState, SimRegion, AudioState, Entity, RenderGroup, Transform);
        } break;

        default:
        {
        } break;
    }
}

internal void
UpdateTimers(entity *Entity, r32 dt)
{
    if(Entity->Type == EntityType_Hero)
    {
        hero_entity *HeroData = (hero_entity *)Entity->Data;
        for(u32 SpellIndex = 0;
            SpellIndex < ArrayCount(HeroData->Spells);
            ++SpellIndex)
        {
            hero_spell *Spell = HeroData->Spells + SpellIndex;
            timer *Timer = &Spell->Timer;
            if(Timer->DurationSeconds > 0.0f)
            {
                Timer->CurrentTime += dt;
                if(Timer->CurrentTime >= Timer->DurationSeconds)
                {
                    Timer->Finished = true;
                    Timer->CurrentTime = Timer->DurationSeconds;
                }
                else
                {
                    Timer->Finished = false;
                }
            }
        }

        timer *Timer = &HeroData->SwordCharmTimer;
        if(Timer->DurationSeconds > 0.0f)
        {
            Timer->CurrentTime += dt;
            if(Timer->CurrentTime >= Timer->DurationSeconds)
            {
                Timer->Finished = true;
                Timer->CurrentTime = Timer->DurationSeconds;
                HeroData->SwordType = SwordType_Null;
            }
            else
            {
                Timer->Finished = false;
            }
        }
    }
    else
    {
        for(u32 TimerIndex = 0;
            TimerIndex < Entity->Timers->TimerCount;
            ++TimerIndex)
        {
            timer *Timer = Entity->Timers->Timers + TimerIndex;
            if(Timer->DurationSeconds > 0.0f)
            {
                Timer->CurrentTime += dt;
                if(Timer->CurrentTime >= Timer->DurationSeconds)
                {
                    Timer->Finished = true;
                    Timer->CurrentTime = Timer->DurationSeconds;
                }
                else
                {
                    Timer->Finished = false;
                }
            }
        }
    }
}

internal void
RenderEntities(world_state *WorldState, sim_region *SimRegion, render_group *RenderGroup,
               object_transform *EntityTransform, entity *Entity, r32 dt, render_entity *RenderEntity)
{
   loaded_spritesheet *SpriteSheet = RenderEntity->SpriteSheet;
   u32 EntitySpriteIndex = RenderEntity->EntitySpriteIndex;

   if(Entity->Type != EntityType_Tile)
   {
       EntityTransform->ChunkZ = Entity->ZLayer;
   }
   
   switch(Entity->Type)
   {
       case EntityType_Hero:
       {
           bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
           SpriteID.Value += SpriteSheet->BitmapIDOffset;

           EntityTransform->ChunkZ = Entity->ZLayer;
           PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1), true);
       } break;

        case EntityType_MagicSphere:
        {
            hero_sphere_entity *Data = (hero_sphere_entity *)Entity->Data;

//            EntityTransform->SortBiase += Data->SortBias;
            PushBitmap(RenderGroup, EntityTransform, Entity->BitmapID, Entity->RenderHeight, V3(0, 0, 0));
//            EntityTransform->SortBiase -= Data->SortBias;
        } break;

        case EntityType_FlyingSpell:
        {
            if(SpriteSheet)
            {
                EntityTransform->ChunkZ = Entity->ZLayer;
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1));
            }
        } break;

        case EntityType_ImmidiateSpell:
        {
            if(SpriteSheet)
            {
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1));
            }
        } break;

       case EntityType_Golem:
       {
           PushBitmap(RenderGroup, EntityTransform, Entity->BitmapID, 3.5f, V3(0, 0, 0), V4(1, 1, 1, 1));
#if 0
           if(SpriteSheet)
            {
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1));
            }
#endif
       } break;

        case EntityType_Decoration:
        {
//            PushBitmap(RenderGroup, EntityTransform, Entity->BitmapID, Entity->Collision->Height,
//                       -V3(2.0f, 2.0f, 0));
        } break;

        case EntityType_AnimatedDecoration:
        {
            if(SpriteSheet)
            {
//                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
//                SpriteID.Value += SpriteSheet->BitmapIDOffset;
//                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->Collision->Height,
//                           -V3(2.0f, 2.0f, 0));
            }
        } break;
        
        case EntityType_Collision:
        {
        } break;
        
       case EntityType_Tile:
       {
           tile_entity *Tile = (tile_entity *)Entity->Data;
           for(s32 BitmapIndex = 0;
               BitmapIndex < 11;
               ++BitmapIndex)
           {
               EntityTransform->ChunkZ = BitmapIndex;
               bitmap_id ID = Tile->BitmapID[BitmapIndex];
               if(ID.Value)
               {
                   PushBitmap(RenderGroup, EntityTransform, ID, Entity->RenderHeight, V3(0, 0, 0));
               }
           }

//            PushRectOutline(RenderGroup, EntityTransform, V3(0, 0, 0), V2(1.0f, 1.0f),
//                            V4(1.0f, 0.0f, 1.0f, 1), 0.04f);

            if(Tile->Occupied)
            {
//                PushRect(RenderGroup, EntityTransform, V3(-0.25f, -0.25f, 0), V2(0.1f, 0.1f), V4(1.0f, 0.0f, 0.0f, 1));
//                PushRect(RenderGroup, EntityTransform, V3(-0.25f, 0.25f, 0), V2(0.1f, 0.1f), V4(1.0f, 0.0f, 0.0f, 1));
//                PushRect(RenderGroup, EntityTransform, V3(0.25f, 0.25f, 0), V2(0.1f, 0.1f), V4(1.0f, 0.0f, 0.0f, 1));
//                PushRect(RenderGroup, EntityTransform, V3(0.25f, -0.25f, 0), V2(0.1f, 0.1f), V4(1.0f, 0.0f, 0.0f, 1));
                PushRect(RenderGroup, EntityTransform, V3(0, 0, 0), V2(0.1f, 0.1f), V4(1.0f, 0.0f, 0.0f, 1));
            }
            else
            {
//                PushRect(RenderGroup, EntityTransform, V3(0, 0, 0), V2(0.1f, 0.1f), V4(0.5f, 1.0f, 0.5f, 1));
//                PushRect(RenderGroup, EntityTransform, V3(-0.25f, -0.25f, 0), V2(0.1f, 0.1f), V4(0.5f, 1.0f, 0.5f, 1));
//                PushRect(RenderGroup, EntityTransform, V3(-0.25f, 0.25f, 0), V2(0.1f, 0.1f), V4(0.5f, 1.0f, 0.5f, 1));
//                PushRect(RenderGroup, EntityTransform, V3(0.25f, 0.25f, 0), V2(0.1f, 0.1f), V4(0.5f, 1.0f, 0.5f, 1));
                PushRect(RenderGroup, EntityTransform, V3(0, 0, 0), V2(0.1f, 0.1f), V4(0.5f, 1.0f, 0.5f, 1));
            }
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }

#if EDITOR_INTERNAL
   if(Entity->Type != EntityType_Tile)
   {
       PushRectOutline(RenderGroup, EntityTransform, Entity->Collision->OffsetP,
                       GetDim(Entity->Collision->CollisionRect).xy, V4(0, 1, 1, 1), 0.05f);
#if 1
       rectangle2 HeightRect = RectCenterDim(V2(0, 0), V2(GetDim(Entity->Collision->CollisionRect).x,
                                                          Entity->Collision->Height));
       PushRectOutline(RenderGroup, EntityTransform,
                       Entity->Collision->OffsetP + V3(0, Entity->Collision->Height, 0),
                       GetDim(HeightRect),
                       V4(0, 1, 1, 1), 0.05f);
   }
#endif
#endif    
}

internal void
UpdateAndRenderEntities(world_state *WorldState, sim_region *SimRegion, audio_state *AudioState, controlled_hero *ConHero,
                        render_group *RenderGroup, real32 dt, v2 MouseP)
{
    TIMED_FUNCTION();
    object_transform EntityTransform = DefaultUprightTransform();
    v3 LocalMouseP = Unproject(RenderGroup, &EntityTransform, MouseP);

    for(uint32 EntityIndex = 0;
        EntityIndex < SimRegion->EntityCount;
        ++EntityIndex)
    {
        entity *Entity = SimRegion->Entities + EntityIndex;
        EntityTransform.OffsetP = GetEntityGroundPoint(Entity);
        if(Entity->Updatable)
        {
            render_entity RenderEntity = {};
            RenderEntity.AnimationFinished = false;
            RenderEntity.AnimationSpeed = 1;
            RenderEntity.SpriteSheetInfo = 0;

            entity_animation *Animation = 0;
            if(IsCreationFlagSet(Entity, CreationFlag_Animated))
            {
                Animation = Entity->Animation;
#if 0
                if(Animation->AnimationTypeHaveChanged && IsCreationFlagSet(Entity, CreationFlag_SoundEffects))
                {
//                    u32 RandomSound = RandomBetween(&WorldState->EffectsEntropy, 0, 2);
//                    sound_id SoundID = Entity->SoundEffects->AnimationSoundEffect[Animation->AnimationType][RandomSound];
//                    PlaySound(AudioState, SoundID);
                }
#endif
                RenderEntity.AnimationSpeed = Animation->SpriteSheetSpeed[Animation->AnimationType][Entity->FacingDirection];
                spritesheet_id ID = Animation->SpriteSheets[Animation->AnimationType][Entity->FacingDirection];
                if(IsValid(ID))
                {
                    RenderEntity.SpriteSheet = PushSpriteSheet(RenderGroup, ID, true);

                    if(IsValid(RenderEntity.SpriteSheet->SpriteIDs[0]))
                    {
                        RenderEntity.SpriteSheetInfo = GetSpriteSheetInfo(RenderGroup->Assets, ID);
                        RenderEntity.EntitySpriteIndex = UpdateSpriteIndex(Entity, WorldState->Time,
                                                                           RenderEntity.SpriteSheetInfo->SpriteCount,
                                                                           RenderEntity.AnimationSpeed);
                        RenderEntity.AnimationFinished =
                            AnimationHasComleted(WorldState->Time, Animation->SpriteSheetOffset, RenderEntity.EntitySpriteIndex,
                                                 RenderEntity.SpriteSheetInfo->SpriteCount, dt, RenderEntity.AnimationSpeed);
                        for(u32 SpriteIndex = 0;
                            SpriteIndex < RenderEntity.SpriteSheetInfo->SpriteCount;
                            ++SpriteIndex)
                        {
                            bitmap_id SpriteID = RenderEntity.SpriteSheet->SpriteIDs[SpriteIndex];
                            SpriteID.Value += RenderEntity.SpriteSheet->BitmapIDOffset;
                            PrefetchBitmap(RenderGroup->Assets, SpriteID, true);
                        }
                    }
                    else
                    {
                        RenderEntity.SpriteSheet = 0;
                    }
                }
            }
            
            // NOTE(paul): Render Entity
            RenderEntities(WorldState, SimRegion, RenderGroup, &EntityTransform, Entity, dt, &RenderEntity);

            // NOTE(paul): Update Timers
            if(IsCreationFlagSet(Entity, CreationFlag_NeedsTimers))
            {
                UpdateTimers(Entity, dt);
            }
                
            if(IsCreationFlagSet(Entity, CreationFlag_Stats))
            {
                if(Entity->Stats->HealthMax_Health >> 16)
                {
                    if((s16)(Entity->Stats->HealthMax_Health & 0xffff) <= 0)
                    {
                        ChangeEntityState(Entity, EntityState_Dieing);
                    }
                }

            }

            // NOTE(paul): Update Entity
            updated_entity UpdatedEntity = {};
            UpdatedEntity.MoveSpec = DefaultMoveSpec();
            UpdatedEntity.ddP = {};
            
            if(Entity->State != EntityState_Dieing)
            {
                switch(Entity->Type)
                {
                    case EntityType_Hero:
                    {
                        UpdatedEntity = UpdateHero(WorldState, SimRegion, ConHero, Entity, LocalMouseP, RenderGroup);
                    } break;

                    case EntityType_FlyingSpell:
                    {
                        UpdatedEntity = UpdateFlyingSpell(WorldState, Entity);
                    } break;

                    case EntityType_ImmidiateSpell:
                    {
                        UpdatedEntity = UpdateImmidiateSpell(Entity);
                    } break;

                    case EntityType_MagicSphere:
                    {
                        UpdatedEntity = UpdateMagicSphere(Entity, dt);
                    } break;

                    case EntityType_Golem:
                    {
                    } break;

                    case EntityType_Tile:
                    {
                    } break;

                    InvalidDefaultCase;
                }
            }

            if((Entity->Type != EntityType_Tile) && Entity->StandardZUpdate)
            {
                entity_id TileID = WorldState->TileMap[Entity->TileP.TileY*WorldState->World->TileWidth + Entity->TileP.TileX];
                entity *GroundTile = GetEntityByID(SimRegion, TileID);
                Entity->ZLayer = GroundTile->ZLayer + 1;
            }

            switch(Entity->State)
            {
                case EntityState_Staying:
                {
                    // NOTE(paul): Nothing to do
                } break;

                case EntityState_Moving:
                {
                    Assert(IsSet(Entity, EntityFlag_Moveable));
                    MoveEntity(WorldState, SimRegion, Entity, dt, &UpdatedEntity.MoveSpec, UpdatedEntity.ddP);
                } break;

                case EntityState_Attacking:
                {
                    u32 StopSpriteIndex = Entity->Animation->AttackSpriteFinishIndex[Entity->Animation->AttackType];
                    b32 AbleToAttack = (AnimationFinishedOnSprite(WorldState->Time, Animation->SpriteSheetOffset,
                                                                  RenderEntity.EntitySpriteIndex, RenderEntity.SpriteSheetInfo->SpriteCount, dt,
                                                                  StopSpriteIndex, RenderEntity.AnimationSpeed) && !Animation->AnimationTypeHaveChanged); 
                    if(AbleToAttack)
                    {
                        EntityAttack(WorldState, AudioState, SimRegion, Entity, RenderGroup, &EntityTransform, LocalMouseP);
                    }
                } break;

                case EntityState_CastingSpell:
                {
                    u32 StopSpriteIndex = Entity->Animation->CastSpellSpriteFinishIndex[Entity->Animation->CastSpellType];
                    b32 AbleToCast = (AnimationFinishedOnSprite(WorldState->Time, Animation->SpriteSheetOffset,
                                                                RenderEntity.EntitySpriteIndex, RenderEntity.SpriteSheetInfo->SpriteCount, dt,
                                                                StopSpriteIndex, RenderEntity.AnimationSpeed) && !Animation->AnimationTypeHaveChanged); 
                    if(AbleToCast)
                    {
                        EntityAttack(WorldState, AudioState, SimRegion, Entity, RenderGroup, &EntityTransform, LocalMouseP);
                    }
                } break;

                case EntityState_Dieing:
                {
                    Assert(!Entity->Animation->AnimationTypeHaveChanged);
                    if(RenderEntity.AnimationFinished)
                    {
                        switch(Entity->Type)
                        {
                            case EntityType_Hero:
                            {
                                WorldState->HeroExist = false;
//                                PlaySound(AudioState, GameState->GameEndDeathFX);
//                                GameState->MusicState = MusicState_DarkAmbient;
                            } break;
                        }

                        AddFlags(Entity, EntityFlag_Deleted);
                    }
                    else if(Entity->Type == EntityType_Item)
                    {
                        item_entity *ItemData = (item_entity *)Entity->Data;
#if 0
                        if(ItemData->Name == ItemName_HealPotion)
                        {
                            PlaySound(AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_HealPickUp));
                        }
                        else
                        {
                            PlaySound(AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_ItemPickUp));
                        }
#endif
                        AddFlags(Entity, EntityFlag_Deleted);
                    }
                } break;
            }
        }
    }
}

#if 0
        if(DEBUG_UI_ENABLED)
        {
            debug_id EntityDebugID_ = DEBUG_POINTER_ID(SimRegion->Entities + Entity->ID.Value);

            for(uint32 VolumeIndex = 0;
                VolumeIndex < Entity->Collision->VolumeCount;
                ++VolumeIndex)
            {
                sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;                        

                v3 LocalMouseP = Unproject(RenderGroup, EntityTransform, MouseP);

                if((LocalMouseP.x > -0.5f*Volume->Dim.x) && (LocalMouseP.x < 0.5f*Volume->Dim.x) &&
                   (LocalMouseP.y > -0.5f*Volume->Dim.y) && (LocalMouseP.y < 0.5f*Volume->Dim.y))
                {
                    DEBUG_HIT(EntityDebugID_, LocalMouseP.z);
                }

                v4 OutlineColor;
                if(DEBUG_HIGHLIGHTED(EntityDebugID_, &OutlineColor))
                {
                    PushRectOutline(RenderGroup, EntityTransform, Volume->OffsetP - V3(0, 0, 0.5f*Volume->Dim.z), Volume->Dim.xy, OutlineColor, 0.05f);
                }
            }
                
            if(DEBUG_REQUESTED(EntityDebugID))
            {
                DEBUG_VALUE(Entity->Updatable);
                DEBUG_VALUE(Entity->Type);
                DEBUG_VALUE(Entity->P);
                DEBUG_VALUE(Entity->FacingDirection);

                DEBUG_END_DATA_BLOCK("Simulation/Entity");
            }
        }
#endif

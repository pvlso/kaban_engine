/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
#include <stdio.h>

#include "spellweaver_entity_utilities.cpp"
#include "spellweaver_entity_collision.cpp"
#include "spellweaver_entity_create.cpp"
#include "spellweaver_entity_updates.cpp"
#include "spellweaver_entity_quests.cpp"

inline void
DrawEntityHealthBar(render_group *RenderGroup, object_transform *Transform, entity *Entity, text_config TextConfig)
{
    u16 MaxHealth = (u16)(Entity->HealthMax_Health >> 16);
    u16 Health = (u16)(Entity->HealthMax_Health & 0xffff);

    r32 Len = 2.0f;
    r32 Ratio = (r32)Health / (r32)MaxHealth;
    r32 HalfRatio = 0.5f*Ratio*Len;


    PushRect(RenderGroup, Transform, V3(0, 2.0f, 0), V2(Len, 0.2f), V4(0.5f, 0.5f, 0.5f, 1));
    Transform->ChunkZ = 1;
    PushRect(RenderGroup, Transform, V3(-0.5f*Len + HalfRatio, 2.0f, 0), V2(Len*Ratio, 0.2f), V4(1.0f, 0, 0, 1));

    char Buffer[16];
    _snprintf_s(Buffer, sizeof(Buffer), "%u/%u", Health, MaxHealth);

    rectangle2 TextRect = GetTextSize(RenderGroup, TextConfig, Buffer, 0);
    v2 TextDim = GetDim(TextRect);
    
    TextConfig.TextTransform.OffsetP = Transform->OffsetP + V3(-0.5f*(TextDim.x) + 0.5f, 1.93f, 0);
    TextConfig.TextShadowTransform.OffsetP = TextConfig.TextTransform.OffsetP + V3(-1.975f, 1.975f, 0);
    TextConfig.FontScale = 0.0065;
    TextOutAt(RenderGroup, TextConfig, Buffer, 0);
}

inline u32
UpdateSpriteIndex(entity *Entity, r32 Time, u32 SpriteCount, u32 Speed)
{
    u32 Result = 0;
    if(Entity->AnimationTypeHaveChanged)
    {
        s32 Index = GetSpriteIndex(Time, SpriteCount, 0, Speed);
        Entity->SpriteSheetOffset = Index;
        Entity->AnimationTypeHaveChanged = false;
    }

    if(Entity->SpriteSheetOffset > SpriteCount)
    {
        s32 Index = GetSpriteIndex(Time, SpriteCount, 0, Speed);
        Entity->SpriteSheetOffset = Index;
    }
                        
    s32 SpriteIndex = GetSpriteIndex(Time, SpriteCount, Entity->SpriteSheetOffset, Speed);
    Assert(SpriteIndex >= 0);

    Result = SpriteIndex;
    return(Result);
}

internal void
EntityAttack(game_mode_world *WorldMode, audio_state *AudioState, sim_region *SimRegion, entity *Entity,
             render_group *RenderGroup, object_transform *Transform, v3 LocalMouseP)
{
    // TODO(paul): Add more different entities that can attack
    random_series *EffectsEntropy = &WorldMode->EffectsEntropy;
    switch(Entity->Type)
    {
        case EntityType_Hero:
        {
            HeroAttack(WorldMode, SimRegion, AudioState, Entity, RenderGroup, Transform, LocalMouseP);
        } break;

        case EntityType_Golem:
        {
            MonsterAttack(SimRegion, AudioState, EffectsEntropy, Entity, RenderGroup, Transform, 30, V2(1.6f, 1.6f));
        } break;

        case EntityType_Cultist:
        {
            CultistAttack(WorldMode, SimRegion, Entity, RenderGroup, Transform, LocalMouseP);
        } break;

        case EntityType_Necromancer:
        {
            NecromancerAttack(WorldMode, RenderGroup->Assets, SimRegion, Entity);
        } break;

        case EntityType_Possesed:
        {
            MonsterAttack(SimRegion, AudioState, EffectsEntropy, Entity, RenderGroup, Transform, 10, V2(1.2f, 1.2f));
        } break;

        case EntityType_GoblinBeast:
        {
            MonsterAttack(SimRegion, AudioState, EffectsEntropy, Entity, RenderGroup, Transform, 20, V2(1.2f, 1.2f));
        } break;

        case EntityType_GoblinBerserker:
        {
            MonsterAttack(SimRegion, AudioState, EffectsEntropy, Entity, RenderGroup, Transform, 15, V2(1.2f, 1.2f));
        } break;

        case EntityType_GoblinRider:
        {
            MonsterAttack(SimRegion, AudioState, EffectsEntropy, Entity, RenderGroup, Transform, 25, V2(1.2f, 1.2f));
        } break;

        case EntityType_SkeletonGrunt:
        {
            MonsterAttack(SimRegion, AudioState, EffectsEntropy, Entity, RenderGroup, Transform, 10, V2(1.0f, 1.0f));
        } break;

        case EntityType_SkeletonHunter:
        {
            SkeletonHunterAttack(WorldMode, SimRegion, Entity, RenderGroup, Transform);
        } break;

        case EntityType_SkeletonKing:
        {
            SkeletonKingAttack(WorldMode, AudioState, SimRegion, Entity, RenderGroup, Transform);
        } break;

        case EntityType_ImmidiateSpell:
        {
            ImmidiateSpellAttack(WorldMode, SimRegion, AudioState, Entity, RenderGroup, Transform);
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
            TimerIndex < Entity->TimerCount;
            ++TimerIndex)
        {
            timer *Timer = Entity->Timers + TimerIndex;
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
RenderEntities(game_mode_world *WorldMode, sim_region *SimRegion, render_group *RenderGroup,
               object_transform *EntityTransform, entity *Entity, r32 dt, render_entity *RenderEntity)
{
    loaded_spritesheet *SpriteSheet = RenderEntity->SpriteSheet;
    u32 EntitySpriteIndex = RenderEntity->EntitySpriteIndex;

    switch(Entity->Type)
    {
        case EntityType_Hero:
        {
            if(SpriteSheet)
            {
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1), true);
            }

            DrawHeroSpellBar(RenderGroup, Entity, WorldMode->GeneralTextConfig);

            hero_entity *HeroData = (hero_entity *)Entity->Data;
            if(HeroData->ClosestNPC.Entity && (HeroData->ClosestNPC.DistanceSq < Square(2.5f)))
            {
                EntityTransform->ChunkZ = 1000;
                PushRect(RenderGroup, EntityTransform, V3(0.6f, 1.0f, 0), V2(0.5f, 0.5f), V4(0, 0, 0, 0.5f));
                text_config TextConfig = WorldMode->GeneralTextConfig;
                TextConfig.TextTransform.OffsetP = EntityTransform->OffsetP + V3(0.52f, 0.85f, 0); 
                TextConfig.TextShadowTransform.OffsetP = EntityTransform->OffsetP + V3(-1.46f, 2.87f, 0); 
                TextConfig.FontScale = 0.013f; 
                TextOutAt(RenderGroup, TextConfig, "E", 0);
            }

        } break;

        case EntityType_MagicSphere:
        {
            hero_sphere_entity *Data = (hero_sphere_entity *)Entity->Data;

            EntityTransform->ChunkZ += (s32)Data->SortBias;
            PushBitmap(RenderGroup, EntityTransform, Entity->BitmapID, Entity->RenderHeight, V3(0, 0, 0));
            EntityTransform->ChunkZ -= (s32)Data->SortBias;
                    
        } break;

        case EntityType_FlyingSpell:
        {
            if(SpriteSheet)
            {
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                EntityTransform->ChunkZ += 1;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1));
                EntityTransform->ChunkZ -= 1;
            }
        } break;

        case EntityType_ImmidiateSpell:
        {
            if(SpriteSheet)
            {
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                EntityTransform->ChunkZ += 1;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1));
                EntityTransform->ChunkZ -= 1;
            }
        } break;

        case EntityType_NPC:
        {
            if(SpriteSheet)
            {
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1));

                talkingnpc_entity *EntityData = (talkingnpc_entity *)Entity->Data;


                text_config TextConfig = WorldMode->GeneralTextConfig;
                TextConfig.FontScale = 0.008f;
                rectangle2 TextRect = GetTextSize(RenderGroup, TextConfig, EntityData->NPCName, 0);
                v2 TextDim = GetDim(TextRect);

                TextConfig.TextTransform.OffsetP = EntityTransform->OffsetP + V3(-0.5f*TextDim.x, 1.6f, 0);
                TextConfig.TextShadowTransform.OffsetP = EntityTransform->OffsetP + V3(-0.5f*TextDim.x - 1.98f, 3.57f, 0);
                TextOutAt(RenderGroup, TextConfig, EntityData->NPCName, 0);

                bitmap_id QuestMarkID = EntityData->QuestMark[EntityData->TalkingState];
                if(IsValid(QuestMarkID))
                {
                    r32 Vp = 0.2f*Sin(EntityData->Count);
                    EntityTransform->ChunkZ += 1000;
                    PushBitmap(RenderGroup, EntityTransform, QuestMarkID, 0.8f, V3(0, 2.1f + Vp, 0), V4(1, 1, 1, 1));
                    EntityTransform->ChunkZ -= 1000;
                    EntityData->Count += 2.0f*dt;
                }
            }
        } break;

        case EntityType_Obelisk:
        case EntityType_Item:
        case EntityType_Golem:
        case EntityType_Cultist:
        case EntityType_Necromancer:
        case EntityType_Possesed:
        case EntityType_GoblinBeast:
        case EntityType_GoblinBerserker:
        case EntityType_GoblinRider:
        case EntityType_SkeletonGrunt:
        case EntityType_SkeletonHunter:
        case EntityType_SkeletonKing:
        {
            if(SpriteSheet)
            {
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->RenderHeight, V3(0, 0, 0), V4(1, 1, 1, 1));
            }
        } break;

        case EntityType_Decoration:
        {
            PushBitmap(RenderGroup, EntityTransform, Entity->BitmapID, Entity->Collision->Height,
                       -V3(2.0f, 2.0f, 0));
        } break;

        case EntityType_AnimatedDecoration:
        {
            if(SpriteSheet)
            {
                bitmap_id SpriteID = SpriteSheet->SpriteIDs[EntitySpriteIndex];
                SpriteID.Value += SpriteSheet->BitmapIDOffset;
                PushBitmap(RenderGroup, EntityTransform, SpriteID, Entity->Collision->Height,
                           -V3(2.0f, 2.0f, 0));
            }
        } break;

        case EntityType_Obstacle:
        {
            PushBitmap(RenderGroup, EntityTransform, Entity->BitmapID, Entity->RenderHeight, -V3(0.5f, 0.5f, 0));
        } break;
        
        case EntityType_Collision:
        {
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }

#if SPELLWEAVER_INTERNAL
    PushRectOutline(RenderGroup, EntityTransform, Entity->Collision->OffsetP,
                    GetDim(Entity->Collision->CollisionRect).xy,
                    V4(0, 1, 1, 1), 0.05f);
#if 0
    rectangle2 HeightRect = RectCenterDim(V2(0, 0), V2(GetDim(Entity->Collision->CollisionRect).x,
                                                       Entity->Collision->Height));
    PushRectOutline(RenderGroup, EntityTransform,
                    Entity->Collision->OffsetP + V3(0, 0.5f*Entity->Collision->Height, 0),
                    GetDim(HeightRect),
                    V4(0, 1, 1, 1), 0.05f);
#endif
#endif    

    if(Entity->HealthMax_Health >> 16)
    {
        if(Entity->Type == EntityType_Hero)
        {
            DrawHeroHealthBar(RenderGroup, Entity, WorldMode->GeneralTextConfig);
        }
        else
        {
            DrawEntityHealthBar(RenderGroup, EntityTransform, Entity, WorldMode->GeneralTextConfig);
        }
    }
}

internal void
UpdateAndRenderEntities(game_mode_world *WorldMode, game_state *GameState, sim_region *SimRegion,
                        render_group *RenderGroup, real32 dt, v2 MouseP)
{
    TIMED_FUNCTION();

    audio_state *AudioState = GameState->AudioState;

    object_transform EntityTransform_ = DefaultUprightTransform();
    object_transform *EntityTransform = &EntityTransform_;
    v3 LocalMouseP = Unproject(RenderGroup, EntityTransform, MouseP);

    for(uint32 EntityIndex = 0;
        EntityIndex < SimRegion->EntityCount;
        ++EntityIndex)
    {
        entity *Entity = SimRegion->Entities + EntityIndex;
        EntityTransform->OffsetP = GetEntityGroundPoint(Entity);

        if(Entity->Updatable)
        {
            v3 LastP = Entity->P;

            if(Entity->AnimationTypeHaveChanged)
            {
                u32 RandomSound = RandomBetween(&WorldMode->EffectsEntropy, 0, 2);
                sound_id SoundID = Entity->AnimationSoundEffect[Entity->AnimationType][RandomSound];
                PlaySound(AudioState, SoundID);
            }

            b32 AnimationFinished = false;
            b32 PlayMoveSound = false;
            ssa_spritesheet *SpriteSheetInfo = 0;
            u32 AnimationSpeed = Entity->SpriteSheetSpeed[Entity->AnimationType][Entity->FacingDirection];
            spritesheet_id ID = Entity->SpriteSheets[Entity->AnimationType][Entity->FacingDirection];

            render_entity RenderEntity = {};
            if(IsValid(ID))
            {
                RenderEntity.SpriteSheet = PushSpriteSheet(RenderGroup, ID, true);

                if(IsValid(RenderEntity.SpriteSheet->SpriteIDs[0]))
                {
                    SpriteSheetInfo = GetSpriteSheetInfo(RenderGroup->Assets, ID);
                    RenderEntity.EntitySpriteIndex = UpdateSpriteIndex(Entity, WorldMode->Time,
                                                                       SpriteSheetInfo->SpriteCount, AnimationSpeed);
                    AnimationFinished = AnimationHasComleted(WorldMode->Time, Entity->SpriteSheetOffset,
                                                             RenderEntity.EntitySpriteIndex,
                                                             SpriteSheetInfo->SpriteCount, dt, AnimationSpeed);
                    for(u32 SpriteIndex = 0;
                        SpriteIndex < SpriteSheetInfo->SpriteCount;
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
        
            // NOTE(paul): Render Entity
            RenderEntities(WorldMode, SimRegion, RenderGroup, EntityTransform, Entity, dt, &RenderEntity);
            
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
                        for(uint32 ControlIndex = 0;
                            ControlIndex < ArrayCount(GameState->ControlledHeroes);
                            ++ControlIndex)
                        {
                            controlled_hero *ConHero = GameState->ControlledHeroes + ControlIndex;

                            if(ConHero->EntityIndex.Value)
                            {
                                if(Entity->ID.Value == ConHero->EntityIndex.Value)
                                {
                                    UpdatedEntity = UpdateHero(WorldMode, SimRegion, ConHero, Entity, LocalMouseP,
                                                               RenderGroup);
                                }
                            }
                        }
                    } break;

                    case EntityType_FlyingSpell:
                    {
                        UpdatedEntity = UpdateFlyingSpell(WorldMode, Entity);
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
                        UpdatedEntity = UpdateGolem(WorldMode, SimRegion, Entity);
                    } break;

                    case EntityType_Cultist:
                    {
                        UpdatedEntity = UpdateCultist(WorldMode, SimRegion, Entity);
                    } break;

                    case EntityType_Necromancer:
                    {
                        UpdatedEntity = UpdateNecromancer(WorldMode, RenderGroup->Assets, AudioState, SimRegion, Entity);
                    } break;

                    case EntityType_Possesed:
                    {
                        UpdatedEntity = UpdatePossesed(WorldMode, SimRegion, Entity);
                    } break;

                    case EntityType_GoblinBeast:
                    {
                        UpdatedEntity = UpdateGoblinBeast(WorldMode, SimRegion, Entity, &WorldMode->EffectsEntropy);
                    } break;

                    case EntityType_GoblinBerserker:
                    {
                        UpdatedEntity = UpdateGoblinBerserker(WorldMode, SimRegion, Entity);
                    } break;

                    case EntityType_GoblinRider:
                    {
                        UpdatedEntity = UpdateGoblinRider(WorldMode, SimRegion, Entity, &WorldMode->EffectsEntropy);
                    } break;

                    case EntityType_SkeletonGrunt:
                    {
                        UpdatedEntity = UpdateSkeletonGrunt(WorldMode, SimRegion, Entity);
                    } break;

                    case EntityType_SkeletonHunter:
                    {
                        UpdatedEntity = UpdateSkeletonHunter(WorldMode, SimRegion, Entity);
                    } break;

                    case EntityType_SkeletonKing:
                    {
                        UpdatedEntity = UpdateSkeletonKing(WorldMode, RenderGroup->Assets, SimRegion, Entity);
                    } break;

                    case EntityType_Item:
                    case EntityType_Obelisk:
                    case EntityType_Obstacle:
                    case EntityType_Decoration:
                    case EntityType_AnimatedDecoration:
                    case EntityType_NPC:
                    case EntityType_Collision:
                    {
                    } break;

                    InvalidDefaultCase;
                }

                // TODO(paul): This is stupid remove this
                if((UpdatedEntity.ddP.x != 0.0f) || (UpdatedEntity.ddP.y != 0.0f) || (UpdatedEntity.ddP.z != 0.0f))
                {
                    if(Entity->Type == EntityType_GoblinRider)
                    {
                        Entity->State = EntityState_Moving;
                        PlayMoveSound = !(FloorReal32ToInt32(WorldMode->Time * (1.0f / dt)) % 50);
                    }
                    else
                    {
                        Entity->State = EntityState_Moving;
                        PlayMoveSound = !(FloorReal32ToInt32(WorldMode->Time * (1.0f / dt)) % 18);
                    }
                }
                
                if(Entity->HealthMax_Health >> 16)
                {
                    if((s16)(Entity->HealthMax_Health & 0xffff) <= 0)
                    {
                        ChangeEntityState(Entity, EntityState_Dieing);
                        ChangeAnimationType(Entity, AnimationType_Death);
                    }
                }

            }

            // NOTE(paul): Update Timers
            UpdateTimers(Entity, dt);
            
            // NOTE(paul): Do something depending on entity state
            if(IsSet(Entity, EntityFlag_Moveable) &&
               (Entity->State == EntityState_Moving))
            {
                MoveEntity(WorldMode, SimRegion, Entity, dt, &UpdatedEntity.MoveSpec, UpdatedEntity.ddP);
                if((PlayMoveSound) && (Entity->dP.x != 0.0f) && (Entity->dP.y != 0.0f))
                {
                    u32 RandomSound = RandomBetween(&WorldMode->EffectsEntropy, 0, 2);
                    sound_id SoundID = Entity->AnimationSoundEffect[AnimationType_Move][RandomSound];
                    PlaySound(AudioState, SoundID);
                }
            }
            else if(Entity->State == EntityState_Attacking)
            {
                u32 StopSpriteIndex = Entity->AttackSpriteFinishIndex[Entity->AttackType];

                if((AnimationFinishedOnSprite(WorldMode->Time, Entity->SpriteSheetOffset,
                                              RenderEntity.EntitySpriteIndex, SpriteSheetInfo->SpriteCount, dt,
                                              StopSpriteIndex, AnimationSpeed)) &&
                   !(Entity->AnimationTypeHaveChanged))
                {
                    EntityAttack(WorldMode, AudioState, SimRegion, Entity, RenderGroup, EntityTransform, LocalMouseP);
                }
                else
                {
                    // TODO(paul): animation independent attack
                }
            }
            else if(Entity->State == EntityState_CastingSpell)
            {
                u32 StopSpriteIndex = Entity->CastSpellSpriteFinishIndex[Entity->CastSpellType];

                if((AnimationFinishedOnSprite(WorldMode->Time, Entity->SpriteSheetOffset,
                                              RenderEntity.EntitySpriteIndex, SpriteSheetInfo->SpriteCount, dt,
                                              StopSpriteIndex, AnimationSpeed)) &&
                   !(Entity->AnimationTypeHaveChanged))
                {
                    EntityAttack(WorldMode, AudioState, SimRegion, Entity, RenderGroup, EntityTransform, LocalMouseP);
                }
                else
                {
                    // TODO(paul): animation independent attack
                }
            }
            else if(Entity->State == EntityState_Dieing)
            {
                if((AnimationFinished) && !(Entity->AnimationTypeHaveChanged))
                {
                    switch(Entity->Type)
                    {
                        case EntityType_Hero:
                        {
                            WorldMode->HeroExist = false;
                            PlaySound(AudioState, GameState->GameEndDeathFX);
                            GameState->MusicState = MusicState_DarkAmbient;
                        } break;

                        case EntityType_SkeletonKing:
                        {
                            SkeletonKingDeathEvent(WorldMode, RenderGroup->Assets, Entity);
                        } break;
                        
                        case EntityType_Golem:
                        case EntityType_Cultist:
                        case EntityType_Necromancer:
                        case EntityType_Possesed:
                        case EntityType_GoblinBeast:
                        case EntityType_GoblinBerserker:
                        case EntityType_GoblinRider:
                        case EntityType_SkeletonGrunt:
                        case EntityType_SkeletonHunter:
                        {
                            MonsterDeathEvent(WorldMode, RenderGroup->Assets, Entity);
                        } break;
                    }

                    AddFlags(Entity, EntityFlag_Deleted);
                }
                else if(Entity->Type == EntityType_Item)
                {
                    item_entity *ItemData = (item_entity *)Entity->Data;
                    if(ItemData->Name == ItemName_HealPotion)
                    {
                        PlaySound(AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_HealPickUp));
                    }
                    else
                    {
                        PlaySound(AudioState, GetSoundEffectForType(RenderGroup->Assets, SoundEffect_ItemPickUp));
                    }

                    AddFlags(Entity, EntityFlag_Deleted);
                }
            }
        }
    }
}


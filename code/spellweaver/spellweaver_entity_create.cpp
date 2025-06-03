/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

inline void
SetMaxHealthAndMana(entity_stats *Stats, u16 Health, u16 Mana)
{
    Stats->HealthMax_Health = (u32)((Health << 16) | Health);
    Stats->ManaMax_Mana = (u32)((Mana << 16) | Mana);
}

internal entity *
BeginEntity(world_state *WorldState, entity_general_type GeneralType, entity_type Type, u32 CreationFlags)
{
    Assert(WorldState->CreationBufferIndex < ArrayCount(WorldState->CreationBuffers));
    entity *Entity = WorldState->CreationBuffers + WorldState->CreationBufferIndex++;

    ZeroStruct(*Entity);
    Entity->ID.Value = ++WorldState->LastUsedEntityStorageIndex;

    Entity->CreationFlags = CreationFlags;
    Entity->GeneralType = GeneralType;
    Entity->Type = Type;
    
    Entity->State = EntityState_Staying;
    Entity->Collision = WorldState->NullCollision;

    Entity->RenderHeight = WorldState->World->TileDimInMeters.y;
    Entity->StandardZUpdate = true;


    if(CreationFlags & CreationFlag_Stats)
    {
        Entity->Stats = PushStruct(&WorldState->World->Arena, entity_stats);
        Entity->Stats->HealthMax_Health = (u32)((100 << 16) | 100);
        Entity->Stats->ManaMax_Mana = (u32)((100 << 16) | 100);
    }

    if(CreationFlags & CreationFlag_Movable)
    {
        Entity->MoveState = PushStruct(&WorldState->World->Arena, entity_move_state);
        Entity->MoveState->MovePointMinHeap.MaxSize = 64;
        Entity->MoveState->MovePointMinHeap.Size = 0;
        Entity->MoveState->MovePointMinHeap.Nodes =
            PushArray(&WorldState->World->Arena, Entity->MoveState->MovePointMinHeap.MaxSize, sort_entry);
    }

    if(CreationFlags & CreationFlag_Animated)
    {
        Entity->Animation = PushStruct(&WorldState->World->Arena, entity_animation);
        Entity->Animation->AnimationType = AnimationType_Idle;
        Entity->Animation->AnimationTypeHaveChanged = true;
    }

    if(CreationFlags & CreationFlag_HaveReferences)
    {
        Entity->References = PushStruct(&WorldState->World->Arena, entity_references);
    }

    if(CreationFlags & CreationFlag_NeedsTimers)
    {
        Entity->Timers = PushStruct(&WorldState->World->Arena, entity_timers);
    }

    if(CreationFlags & CreationFlag_SoundEffects)
    {
        Entity->SoundEffects = PushStruct(&WorldState->World->Arena, entity_sound_effects);
    }

    if(CreationFlags & CreationFlag_DataNeeded)
    {
        switch(Entity->Type)
        {
            case EntityType_Hero:
            {
                Entity->Data = PushSize(&WorldState->World->Arena, sizeof(hero_entity));
            } break;

            case EntityType_FlyingSpell:
            {
                Entity->Data = PushSize(&WorldState->World->Arena, sizeof(flyingspell_entity));
            } break;

            case EntityType_ImmidiateSpell:
            {
                Entity->Data = PushSize(&WorldState->World->Arena, sizeof(immidiatespell_entity));
            } break;

            case EntityType_MagicSphere:
            {
                Entity->Data = PushSize(&WorldState->World->Arena, sizeof(hero_sphere_entity));
            } break;

            case EntityType_Tile:
            {
                Entity->Data = PushSize(&WorldState->World->Arena, sizeof(tile_entity));
            } break;

            InvalidDefaultCase;
        }
    }
    
    return(Entity);
}

internal void
EndEntity(world_state *WorldState, entity *Entity, world_position P)
{
    --WorldState->CreationBufferIndex;
    Assert(Entity == (WorldState->CreationBuffers + WorldState->CreationBufferIndex));

    PackEntityIntoWorld(&WorldState->World->Arena, WorldState->World, Entity, P);
}

internal entity *
BeginGroundedEntity(world_state *WorldState, entity_general_type GeneralType, entity_type Type, u32 CreationFlags,
                    entity_collision *Collision)
{
    entity *Entity = BeginEntity(WorldState, GeneralType, Type, CreationFlags);
    Entity->Collision = Collision;
    return(Entity);
}

internal entity_id
AddTile(world_state *WorldState, entity_collision *Collision, b32 Occupied, sswm_ground_tile *Source, s32 ZLayer)
{
    entity *Entity = BeginGroundedEntity(WorldState, GeneralType_Object, EntityType_Tile, CreationFlag_DataNeeded, Collision);
    Entity->ZLayer = ZLayer;

    tile_entity *Tile = (tile_entity *)Entity->Data;
    Tile->Occupied = Occupied;
    Copy(sizeof(bitmap_id)*16, Source->BitmapID, Tile->BitmapID);
    
    entity_id Result = Entity->ID;
    EndEntity(WorldState, Entity, CenteredTilePoint(WorldState->World, Source->TileX, Source->TileY));

    return(Result);
}

inline void
AddEntitySpriteSheetsForType(editor_assets *Assets, entity *Entity, u32 AnimationType,
                             asset_vector *MatchVector, asset_vector *WeightVector)
{
    u32 Angles[4] = {0, 1, 2, 3};

    for(u32 AngleIndex = 0;
        AngleIndex < ArrayCount(Angles);
        ++AngleIndex)
    {
        u32 Angle = Angles[AngleIndex];
        MatchVector->E[Tag_FacingDirection] = Angle;
        MatchVector->E[Tag_AnimationType] = AnimationType;
        Entity->Animation->SpriteSheets[AnimationType][AngleIndex] =
            GetBestMatchSpriteSheetFrom(Assets, Asset_SpriteSheet, MatchVector, WeightVector);
    }
}

inline void
AddEntitySpriteSheets(editor_assets *Assets, entity *Entity, asset_vector *MatchVector, asset_vector *WeightVector)
{
    for(u32 SheetIndex = 0;
        SheetIndex < AnimationType_Count;
        ++SheetIndex)
    {
        AddEntitySpriteSheetsForType(Assets, Entity, SheetIndex, MatchVector, WeightVector);
    }
}

internal entity_id
AddGolem(world_state *WorldState, editor_assets *Assets, world_position P)
{
    u32 CreationFlags = CreationFlag_Stats|CreationFlag_Animated|CreationFlag_Movable;
    entity *Entity = BeginGroundedEntity(WorldState, GeneralType_Enemy, EntityType_Golem,
                                         CreationFlags, WorldState->GolemCollision);
    Entity->RenderHeight = 1.5f;

    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable);

    Entity->BitmapID = GetFirstBitmapFrom(Assets, Asset_Golem);

    entity_id Result = Entity->ID;
    EndEntity(WorldState, Entity, P);

    return(Result);
}

internal entity_id
AddFlyingSpell(world_state *WorldState, editor_assets *Assets, casted_spell Spell, s32 ZLayer = 0)
{
    entity *Entity = BeginEntity(WorldState, GeneralType_Spell, EntityType_FlyingSpell,
                                 CreationFlag_Animated|CreationFlag_Movable|CreationFlag_SoundEffects|
                                 CreationFlag_DataNeeded);
    Entity->ZLayer = ZLayer;
    Entity->StandardZUpdate = false;
    Entity->RenderHeight = Spell.RenderHeight;
    Entity->Collision = WorldState->SpellCollision;

    Entity->MoveState->DistanceLimit = Spell.Distance;
    Entity->MoveState->dP = Spell.dP;

    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable);

    flyingspell_entity *Data = (flyingspell_entity *)Entity->Data;
    Data->Type = Spell.Type;
    Data->Effect = Spell.Effect;
    Data->Damage = Spell.Damage;
    Data->Direction = Spell.Direction;
    
    asset_vector WeightVector = {};
    InitWeightVector(&WeightVector);
    WeightVector.E[Tag_AssetType] = 2;
    WeightVector.E[Tag_FacingDirection] = 2;
    WeightVector.E[Tag_AnimationType] = 2;
    WeightVector.E[Tag_SpellName] = 2;
    WeightVector.E[Tag_MagicElement] = 2;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = Asset_Spell;
    MatchVector.E[Tag_SpellName] = Spell.SpellName;
    MatchVector.E[Tag_MagicElement] = Spell.MagicElement;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    Entity->Animation->SpriteSheetSpeed[AnimationType_Move][0] = Spell.ProjectileAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Move][1] = Spell.ProjectileAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Move][2] = Spell.ProjectileAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Move][3] = Spell.ProjectileAnimationSpeed;

    Entity->Animation->SpriteSheetSpeed[AnimationType_Death][0] = Spell.DeathAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Death][1] = Spell.DeathAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Death][2] = Spell.DeathAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Death][3] = Spell.DeathAnimationSpeed;

    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Death][0] = Spell.ImpactEffect;
    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Death][1] = Spell.ImpactEffect;
    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Death][2] = Spell.ImpactEffect;

    entity_id Result = Entity->ID;
    
    world_position Pos = MapIntoTileSpace(WorldState->World, Spell.BaseP, Spell.OffsetP.xy);
    EndEntity(WorldState, Entity, Pos);

    return(Result);
}

internal entity_id
AddImmidiateSpell(world_state *WorldState, editor_assets *Assets, casted_spell Spell)
{
    entity *Entity = BeginEntity(WorldState, GeneralType_Spell, EntityType_ImmidiateSpell, CreationFlag_Animated|
                                 CreationFlag_SoundEffects|CreationFlag_DataNeeded);

    Entity->Collision = WorldState->SpellCollision;
    Entity->RenderHeight = Spell.RenderHeight;

    Entity->Animation->AttackSpriteFinishIndex[0] = Spell.ImmidiateAnimationFinishIndex;

    immidiatespell_entity *Data = (immidiatespell_entity *)Entity->Data;
    Data->Type = Spell.Type;
    Data->Damage_Heal = Spell.Damage;
    
    asset_vector WeightVector = {};
    InitWeightVector(&WeightVector);
    WeightVector.E[Tag_AssetType] = 2;
    WeightVector.E[Tag_FacingDirection] = 2;
    WeightVector.E[Tag_AnimationType] = 2;
    WeightVector.E[Tag_SpellName] = 2;
    WeightVector.E[Tag_MagicElement] = 2;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = Asset_Spell;
    MatchVector.E[Tag_SpellName] = Spell.SpellName;
    MatchVector.E[Tag_MagicElement] = Spell.MagicElement;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    Entity->Animation->SpriteSheetSpeed[AnimationType_Attack0][0] = Spell.ProjectileAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Attack0][1] = Spell.ProjectileAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Attack0][2] = Spell.ProjectileAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Attack0][3] = Spell.ProjectileAnimationSpeed;

    Entity->Animation->SpriteSheetSpeed[AnimationType_Death][0] = Spell.DeathAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Death][1] = Spell.DeathAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Death][2] = Spell.DeathAnimationSpeed;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Death][3] = Spell.DeathAnimationSpeed;

    entity_id Result = Entity->ID;
    
    world_position Pos = MapIntoTileSpace(WorldState->World, Spell.BaseP, Spell.OffsetP.xy);
    EndEntity(WorldState, Entity, Pos);

    return(Result);
}

internal entity *
AddSphere(world_state *WorldState, v3 P, world_position BasePos)
{
    world_position Pos = MapIntoTileSpace(WorldState->World, BasePos, P.xy);
    entity *Entity = BeginEntity(WorldState, GeneralType_Object, EntityType_MagicSphere, CreationFlag_DataNeeded);
    Entity->Collision = WorldState->SphereCollision;
    Entity->RenderHeight = 0.7f;

    hero_sphere_entity *Data = (hero_sphere_entity *)Entity->Data;
    Data->Type = SphereType_Null;
    Data->tMove = 0.0f;
    Data->CircleCenter = V3(0.0f, 0.0f, 0.0f);
    
    EndEntity(WorldState, Entity, Pos);
        
    return(Entity);
}

inline void
AddHeroSpell(hero_spell *Spell, spell_type Type, r32 TimerDurationSeconds, effect SpellEffect,
             u32 ManaCost, u32 Damage, r32 Distance, u32 SpellName, u32 MagicElement, u32 ImAnimationFinishIndex,
             u8 ProjectileAnimationSpeed, u8 DeathAnimationSpeed, r32 RenderHeight,
             sound_id CastEffect, sound_id ImpactEffect)
{
    Spell->Type = Type;

    Spell->Timer.Finished = true;
    Spell->Timer.DurationSeconds = TimerDurationSeconds;
    Spell->Timer.CurrentTime = TimerDurationSeconds;

    Spell->Config.Type = Type;
    Spell->Config.SpellName = SpellName;
    Spell->Config.MagicElement = MagicElement;

    Spell->Config.Effect = SpellEffect;
    Spell->Config.ManaCost = ManaCost;
    Spell->Config.Damage = Damage;

    Spell->Config.OffsetP = V3(0, 0, 0);
    Spell->Config.BaseP = NullPosition();

    Spell->Config.ImmidiateAnimationFinishIndex = ImAnimationFinishIndex;
    Spell->Config.ProjectileAnimationSpeed = ProjectileAnimationSpeed;
    Spell->Config.DeathAnimationSpeed = DeathAnimationSpeed;
    
    Spell->Config.Distance = Distance;
    Spell->Config.Direction = V3(0, 0, 0);
    Spell->Config.dP = V3(0, 0, 0);

    Spell->Config.RenderHeight = RenderHeight;
    Spell->Config.CastSpellEffect = CastEffect;
    Spell->Config.ImpactEffect = ImpactEffect;
}

internal entity_id
AddPlayer(world_state *WorldState, editor_assets *Assets)
{
    world_position P = WorldState->CameraP;

    u32 CreationFlags = (CreationFlag_Stats|CreationFlag_Movable|CreationFlag_Animated|
                         CreationFlag_HaveReferences|CreationFlag_NeedsTimers|CreationFlag_SoundEffects|
                         CreationFlag_DataNeeded);

    entity *Entity = BeginGroundedEntity(WorldState, GeneralType_Hero, EntityType_Hero, CreationFlags,
                                             WorldState->PlayerCollision);
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_OnTheGround);

    hero_entity *Data = (hero_entity *)Entity->Data;

    Entity->RenderHeight = 2.2f;
    SetMaxHealthAndMana(Entity->Stats, 100, 150);

    Entity->Animation->AttackSpriteFinishIndex[AttackType_0] = 4;
    Entity->Animation->CastSpellSpriteFinishIndex[CastSpellType_0] = 9;

    asset_vector SphereMatchVector = {};
    asset_vector SphereWeightVector = {};
    SphereWeightVector.E[Tag_MagicElement] = 1;
    
    u32 MagicElements[3] = {MagicElement_Water, MagicElement_Wind, MagicElement_Fire};
    real32 CircleOffset = (2.0f*Pi32) / 3.0f;
    real32 Radius = 0.5f;
    v3 OffsetP = V3(0.0f, 1.0f, 0.0f);

    for(uint32 SphereIndex = 0;
        SphereIndex < 3;
        ++SphereIndex)
    {
        SphereMatchVector.E[Tag_MagicElement] = MagicElements[SphereIndex];
        bitmap_id SphereBitmapID = GetBestMatchBitmapFrom(Assets, Asset_MagicSphere,
                                                          &SphereMatchVector, &SphereWeightVector);
        
        real32 tMove = CircleOffset * SphereIndex;
        v3 Pos = V3((Entity->P.x + Radius*Cos(tMove)),
                    (Entity->P.y + Radius*Sin(tMove)),
                    0.0f);

        entity *Sphere = AddSphere(WorldState, Pos + OffsetP, P);
        Entity->References->References[Entity->References->RefCount].ID = Sphere->ID;
        Data->SpheresRefIndex[SphereIndex] = Entity->References->RefCount;
        ++Entity->References->RefCount;

        hero_sphere_entity *SphereData = (hero_sphere_entity *)Sphere->Data;
        SphereData->CircleCenter = Entity->P + OffsetP;
        SphereData->Type = (sphere_type)(SphereIndex + 1);
        SphereData->tMove = tMove;

        Data->SphereBitmapIDs[SphereIndex] = SphereBitmapID;
        ++Data->Combination[SphereIndex];
    }
    
    Assert(Entity->References->RefCount < ArrayCount(Entity->References->References));
    
    asset_vector WeightVector = {};
    InitWeightVector(&WeightVector);
    WeightVector.E[Tag_AssetType] = 2;
    WeightVector.E[Tag_FacingDirection] = 2;
    WeightVector.E[Tag_AnimationType] = 2;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = Asset_Hero;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    Entity->Animation->SpriteSheetSpeed[AnimationType_Attack0][0] = 10;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Attack0][1] = 10;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Attack0][2] = 10;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Attack0][3] = 10;

    Entity->Animation->SpriteSheetSpeed[AnimationType_CastSpell0][0] = 16;
    Entity->Animation->SpriteSheetSpeed[AnimationType_CastSpell0][1] = 16;
    Entity->Animation->SpriteSheetSpeed[AnimationType_CastSpell0][2] = 16;
    Entity->Animation->SpriteSheetSpeed[AnimationType_CastSpell0][3] = 16;

    Entity->Animation->SpriteSheetSpeed[AnimationType_Move][0] = 16;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Move][1] = 16;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Move][2] = 16;
    Entity->Animation->SpriteSheetSpeed[AnimationType_Move][3] = 16;

    hero_spell *Spell = Data->Spells;
    sound_id NullSoundID = {};
    sound_id CastEffectID = GetSoundEffectForType(Assets, SoundEffect_HeroCastSpellDefault);
    sound_id ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_WaterImpact);

    AddHeroSpell((Spell + 0), SpellType_MagicSword,    5.0f, SpellEffect_Dark, 20, 0,  0.0f,
                 Spell_MagicSword, MagicElement_Dark, 0, 0, 0, 0.0f, CastEffectID, NullSoundID);

    AddHeroSpell((Spell + 1), SpellType_WaterBall,     2.0f, SpellEffect_Water, 15, 10, 10.0f,
                 Spell_WaterBall, MagicElement_Water, 0, 18, 18, 2.0f, CastEffectID, ImpactEffectID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_IceBallCast);
    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_IceImpact);
    AddHeroSpell((Spell + 2), SpellType_IceBall,       2.0f, SpellEffect_Ice, 25, 20, 10.0f,
                 Spell_IceBall, MagicElement_Ice, 0, 12, 12, 2.0f, CastEffectID, ImpactEffectID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_HealCast);
    AddHeroSpell((Spell + 3), SpellType_Heal,          3.0f, SpellEffect_Heal, 0, 20,  0.0f,
                 Spell_HealCross, MagicElement_Light, 4, 5, 10, 2.0f, CastEffectID, NullSoundID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_HeroCastSpellDefault);
    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_ThunderImpact);
    AddHeroSpell((Spell + 4), SpellType_LightBall,     2.0f, SpellEffect_Light, 20, 15, 10.0f,
                 Spell_LightBall, MagicElement_Light, 0, 8, 8, 1.0f, CastEffectID, ImpactEffectID);

    AddHeroSpell((Spell + 5), SpellType_IceSword,      5.0f, SpellEffect_Ice, 20, 0,  0.0f,
                 Spell_IceSword, MagicElement_Ice, 0, 0, 0, 0.0f, CastEffectID, NullSoundID);

    AddHeroSpell((Spell + 6), SpellType_FireSword,     5.0f, SpellEffect_Fire, 25, 0,  0.0f,
                 Spell_FireSword, MagicElement_Fire, 0, 0, 0, 0.0f, CastEffectID, NullSoundID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_FireBallCast);
    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_FireBallImpact);
    AddHeroSpell((Spell + 7), SpellType_FireBall,      3.0f, SpellEffect_Fire, 30, 25, 10.0f,
                 Spell_FireBall, MagicElement_Fire, 0, 12, 12, 2.5f, CastEffectID, ImpactEffectID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_HeroCastSpellDefault);
    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_EnergyImpact);
    AddHeroSpell((Spell + 8), SpellType_EnergyBall,    2.0f, SpellEffect_Energy, 15, 10,  10.0f,
                 Spell_EnergyBall, MagicElement_Energy, 0, 12, 12, 2.5f, CastEffectID, ImpactEffectID);

    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_ThunderImpact);
    AddHeroSpell((Spell + 9), SpellType_BirdStrike,    3.0f, SpellEffect_Light, 35, 40, 12.0f,
                 Spell_BirdStrike, MagicElement_Light, 0, 10, 10, 2.0f, CastEffectID, ImpactEffectID);

    Data->SwordCharmTimer = {};
    Data->SwordCharmTimer.Finished = true;
    Data->SwordCharmTimer.DurationSeconds = 25.0f;
    Data->SwordCharmTimer.CurrentTime = 25.0f;
    Data->SwordType = SwordType_Null;
    Data->CurrentQuests[Data->QuestCount++] = QuestName_FindTavor;

    // NOTE(paul): Attack
    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_0);
    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_1);
    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_2);

    // NOTE(paul): Impact
    Entity->SoundEffects->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_0);
    Entity->SoundEffects->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_1);
    Entity->SoundEffects->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_2);

    // NOTE(paul): Move
    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Move][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Move][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->SoundEffects->AnimationSoundEffect[AnimationType_Move][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);
    
    if(WorldState->CameraFollowingEntityIndex.Value == 0)
    {
        WorldState->CameraFollowingEntityIndex = Entity->ID;
    }
    
    entity_id Result = Entity->ID;

    EndEntity(WorldState, Entity, P);
    
    return(Result);
}

#if !defined(SPELLWEAVER_ENTITY_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

#define InvalidP V2(100000.0f, 100000.0f)

struct entity;

struct entity_id
{
    u32 Value;
};

struct timer
{
    b32 Finished;
    r32 DurationSeconds;
    r32 CurrentTime;
};

// NOTE(paul): NPC ===================================================================================================
struct item_entity
{
    item_name Name;
};

enum talking_state
{
    TalkingState_None,
    TalkingState_QuestGiver,
    TalkingState_QuestObjective,
    TalkingState_QuestComleted,
    TalkingState_General,

    TalkingState_Count,
};

struct talkingnpc_entity
{
    b32 GiverTalkCompleted;
    b32 ObjectiveTalkCompleted;
    talking_state TalkingState;
    
    u32 ParagraphIndex;
    text_id GeneralText;

    r32 Count;
    bitmap_id QuestMark[TalkingState_Count];
    char *NPCName;

    u32 QuestID;
};
// ===================================================================================================================

// NOTE(paul): Enemies ===============================================================================================
struct skeleton_hunter_entity
{
    entity_id ClosestHeroID;
};

struct cultist_entity
{
    entity_id ClosestHeroID;
};

struct necromancer_entity
{
    entity_id ClosestHeroID;
};

struct skeleton_king_entity
{
    b32 ObstaclesPresent;
    u32 ObstacleCount;
    entity_id Obstacles[8];
};

// ===================================================================================================================

// NOTE(paul): Spells and Swords =====================================================================================

enum spell_type
{
    SpellType_Null = 0x00,

    // 0001 0101 sword
    SpellType_MagicSword = 0x15,

    // 0000 0011
    SpellType_WaterBall = 0x03,
    // 0000 0110
    SpellType_IceBall = 0x06,
    // 0001 0010 sword
    SpellType_Heal = 0x12,

    // 0000 1100
    SpellType_LightBall = 0x0c,
    // 0000 1001 sword
    SpellType_IceSword = 0x09,
    // 0001 1000 sword
    SpellType_FireSword = 0x18,

    // 0011 0000
    SpellType_FireBall = 0x30,
    // 0010 0001 sword
    SpellType_EnergyBall = 0x21,
    // 0010 0100
    SpellType_BirdStrike = 0x24,
};

enum effect
{
    SpellEffect_Null,

    SpellEffect_Fire,
    SpellEffect_Wind,
    SpellEffect_Water,
    SpellEffect_Light,
    SpellEffect_Ice,
    SpellEffect_Dark,
    SpellEffect_Heal,
    SpellEffect_Energy,
};

struct casted_spell
{
    spell_type Type;

    u32 SpellName;
    u32 MagicElement;

    effect Effect;
    u32 ManaCost;
    u32 Damage;

    v3 OffsetP;
    world_position BaseP;

    u32 ImmidiateAnimationFinishIndex;

    u8 ProjectileAnimationSpeed;
    u8 DeathAnimationSpeed;

    r32 Distance;
    v3 Direction;
    v3 dP;

    r32 RenderHeight;
    sound_id CastSpellEffect;
    sound_id ImpactEffect;
};

struct magic_sword_entity
{
    effect Effect;
    uint32 Damage;

    spell_type Type;
};

struct immidiatespell_entity
{
    spell_type Type;
    effect Effect;
    u32 Damage_Heal;
};

struct flyingspell_entity
{
    spell_type Type;

    effect Effect;
    u32 Damage;
    v3 Direction;
};
// ===================================================================================================================


// NOTE(paul): Hero ==================================================================================================

struct found_entity
{
    entity *Entity;
    r32 DistanceSq;
};

enum sphere_type
{
    SphereType_Null,

    SphereType_Water,
    SphereType_Wind,
    SphereType_Fire,
};

enum sword_type
{
    SwordType_Null,

    SwordType_Magic,
    SwordType_Ice,
    SwordType_Fire,
};

struct hero_sphere_entity
{
    sphere_type Type;
    real32 tMove;
    v3 CircleCenter;
    r32 SortBias;
};

struct hero_spell
{
    spell_type Type;
    timer Timer;
    casted_spell Config;
};

struct hero_entity
{
    uint32 SpheresRefIndex[3];
    bitmap_id SphereBitmapIDs[3];
    u8 Combination[3];

    found_entity ClosestNPC;

    sword_type SwordType;
    timer SwordCharmTimer;

    v2 CastMouseP;
    u8 CurrentSpell;
    hero_spell Spells[10];

    u32 QuestCount;
    u32 CurrentQuests[8];

    u32 ItemCount;
    u32 Inventory[8];

    world_position MoveP;
};
// ===================================================================================================================

struct tile_entity
{
    b32 Occupied;
    bitmap_id BitmapID[16];
};

// NOTE(paul): Entity General ========================================================================================
struct move_spec
{
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

enum entity_type
{
    EntityType_Null,
    
    EntityType_Hero,
    EntityType_NPC,
    EntityType_Golem,

    EntityType_Cultist,
    EntityType_Necromancer,
    EntityType_Possesed,
    EntityType_GoblinBeast,
    EntityType_GoblinBerserker,
    EntityType_GoblinRider,
    EntityType_SkeletonGrunt,
    EntityType_SkeletonHunter,
    EntityType_SkeletonKing,
    EntityType_MagicSphere,

    EntityType_Item,
    EntityType_Obelisk,

    EntityType_FlyingSpell,
    EntityType_ImmidiateSpell,

    EntityType_Decoration,
    EntityType_AnimatedDecoration,
    EntityType_Collision,


    EntityType_Tile,
};

enum entity_general_type
{
    GeneralType_Null,
    
    GeneralType_Hero,
    GeneralType_Allay,
    GeneralType_Enemy,
    GeneralType_Object,
    GeneralType_Item,
    GeneralType_Spell,
};

enum entity_state
{
    EntityState_None,
    EntityState_Moving,
    EntityState_Staying,
    EntityState_Attacking,
    EntityState_CastingSpell,
    EntityState_Dieing,
};

enum attack_type
{
    AttackType_0,
    AttackType_1,
    AttackType_2,

    AttackType_Count,
};

enum castspell_type
{
    CastSpellType_0,
    CastSpellType_1,
    CastSpellType_2,

    CastSpellType_Count,
};

union entity_reference
{
    entity_id ID;
    entity *Ptr;
};

struct entity_collision
{
    rectangle3 CollisionRect;
    r32 Height;
    v3 OffsetP;
};

struct entity_stats
{
    u32 HealthMax_Health;
    u32 ManaMax_Mana;

    u32 Bufs;
    u32 Debufs;

    u32 Vulnerability;
    u32 Invulnerability;
};

struct entity_move_state
{
    struct nav_poly_node *StartNode;
    struct nav_poly_node *EndNode;
    heap MovePointMinHeap;

    u32 PointCount;
    v2 *Points;
    world_position *TilePoints;
    
    real32 DistanceLimit;
    
    v3 dP;
};

struct entity_animation
{

    b32 AnimationTypeHaveChanged;
    u32 SpriteSheetOffset;
    u32 AnimationType;
    
    // NOTE(paul): For each facing direction
    u8 SpriteSheetSpeed[AnimationType_Count][4];
    spritesheet_id SpriteSheets[AnimationType_Count][4];

    // NOTE(paul): Fighting, effects and spells
    attack_type AttackType;
    u32 AttackSpriteFinishIndex[AttackType_Count];

    castspell_type CastSpellType;
    u32 CastSpellSpriteFinishIndex[AttackType_Count];
};

struct entity_references
{
    u32 RefCount;
    entity_reference References[8];
};

struct entity_timers
{
    u32 TimerCount;
    timer Timers[8];

    u32 CastSpellTimerIndex[AttackType_Count];
    u32 AttackTimerIndex[AttackType_Count];
};

struct entity_sound_effects
{
    // NOTE(paul): Three similar sounds
    sound_id AnimationSoundEffect[AnimationType_Count][3];
    sound_id AttackImpactSound[3];
};

enum entity_flags
{
    EntityFlag_Collides = (1 << 0),
    EntityFlag_Moveable = (1 << 1),
    EntityFlag_Deleted = (1 << 2),
    EntityFlag_OnTheGround = (1 << 3),
};

enum entity_creation_flags
{
    CreationFlag_Stats = (1 << 0),
    CreationFlag_Movable = (1 << 1),
    CreationFlag_Animated = (1 << 2),
    CreationFlag_HaveReferences = (1 << 3),
    CreationFlag_NeedsTimers = (1 << 4),
    CreationFlag_SoundEffects = (1 << 5),
    CreationFlag_DataNeeded = (1 << 6),
};

struct entity
{
    bool32 Updatable;
    entity_id ID;
    
    u32 CreationFlags;
    entity_general_type GeneralType;
    entity_type Type;

    world_position TileP;

    b32 StandardZUpdate;
    s32 ZLayer;

    u32 Flags;
    entity_state PrevState;
    entity_state State;
    entity_collision *Collision;

    v3 P;

    bitmap_id BitmapID;
    r32 RenderHeight;
    u32 FacingDirection;

    attack_type AttackType;
    castspell_type CastType;
    
    entity_stats *Stats;
    entity_move_state *MoveState;
    entity_animation *Animation;
    entity_references *References;
    entity_timers *Timers;
    entity_sound_effects *SoundEffects;

    void *Data;
};
// ===================================================================================================================

struct render_entity
{
    u32 EntitySpriteIndex;
    b32 AnimationFinished;
    u32 AnimationSpeed;

    ssa_spritesheet *SpriteSheetInfo;
    loaded_spritesheet *SpriteSheet;
};

inline bool32
IsCreationFlagSet(entity *Entity, u32 Flag)
{
    bool32 Result = Entity->CreationFlags & Flag;

    return(Result);
}

inline void
AddCreationFlags(entity *Entity, uint32 Flag)
{
    Entity->CreationFlags |= Flag;
}

inline void
ClearCreationFlags(entity *Entity, uint32 Flag)
{
    Entity->CreationFlags &= ~Flag;
}

inline bool32
IsSet(entity *Entity, uint32 Flag)
{
    bool32 Result = Entity->Flags & Flag;

    return(Result);
}

inline void
AddFlags(entity *Entity, uint32 Flag)
{
    Entity->Flags |= Flag;
}

inline void
ClearFlags(entity *Entity, uint32 Flag)
{
    Entity->Flags &= ~Flag;
}

inline v3
GetEntityGroundPoint(entity *Entity, v3 ForEntityP)
{
    v3 Result = ForEntityP;

    return(Result);
}

inline v3
GetEntityGroundPoint(entity *Entity)
{
    v3 Result = GetEntityGroundPoint(Entity, Entity->P);

    return(Result);
}

inline u32
GetAnimationTypeForCastType(castspell_type Type)
{
    u32 Result = 0;
    switch(Type)
    {
        case CastSpellType_0: {Result = AnimationType_CastSpell0;} break;
        case CastSpellType_1: {Result = AnimationType_CastSpell1;} break;
        case CastSpellType_2: {Result = AnimationType_CastSpell2;} break;

            InvalidDefaultCase;
    }

    return(Result);
}

inline u32
GetAnimationTypeForAttackType(attack_type Type)
{
    u32 Result = 0;
    switch(Type)
    {
        case AttackType_0: {Result = AnimationType_Attack0;} break;
        case AttackType_1: {Result = AnimationType_Attack1;} break;
        case AttackType_2: {Result = AnimationType_Attack2;} break;

            InvalidDefaultCase;
    }

    return(Result);
}

inline u32
EntityStateToAnimationType(entity *Entity, u32 DesiredState)
{
    u32 Result = 0;
    switch(DesiredState)
    {
        case EntityState_None:
        {
            Assert(!"State should always be asigned!");
        } break;

        case EntityState_Moving:
        {
            // TODO(paul): If weapon pull in and out will be allowed should be modified to accout on that
            Result = AnimationType_Move;
        } break;

        case EntityState_Staying:
        {
            // TODO(paul): If weapon pull in and out will be allowed should be modified to accout on that
            Result = AnimationType_Idle;
        } break;

        case EntityState_Attacking:
        {
            Result = GetAnimationTypeForAttackType(Entity->AttackType);
        } break;

        case EntityState_CastingSpell:
        {
            Result = GetAnimationTypeForCastType(Entity->CastType);
        } break;

        case EntityState_Dieing:
        {
            Result = AnimationType_Death;
        } break;

        InvalidDefaultCase;
    }

    return(Result);
}
    
inline void
ChangeEntityState(entity *Entity, entity_state DesiredState)
{
    Entity->PrevState = Entity->State;
    Entity->State = DesiredState;

    if(IsCreationFlagSet(Entity, CreationFlag_Animated))
    {
        u32 DesiredAnimationType = EntityStateToAnimationType(Entity, DesiredState);
        if(Entity->Animation->AnimationType != DesiredAnimationType)
        {
            Entity->Animation->AnimationType = DesiredAnimationType;
            Entity->Animation->AnimationTypeHaveChanged = true;
        }
    }
}

#define SPELLWEAVER_ENTITY_H
#endif

#if !defined(EDITOR_GAME_MODE_ENTITY_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
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

struct tile_entity
{
    b32 Occupied;
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
    v3 OffsetP;
    r32 Height;
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
    EntityFlag_ZSupported = (1 << 3),
    EntityFlag_Traversable = (1 << 4),
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

    s32 ZLayer;

    u32 Flags;
    entity_state State;
    entity_collision *Collision;

    v3 P;

    bitmap_id BitmapID;
    r32 RenderHeight;
    u32 FacingDirection;

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

inline void
ChangeAnimationType(entity *Entity, u32 DesiredAnimationType)
{
    if(Entity->Animation->AnimationType != DesiredAnimationType)
    {
        Entity->Animation->AnimationType = DesiredAnimationType;
        Entity->Animation->AnimationTypeHaveChanged = true;
    }
}

inline void
ChangeEntityState(entity *Entity, entity_state DesiredState)
{
    Entity->State = DesiredState;
}

#define EDITOR_GAME_MODE_ENTITY_H
#endif

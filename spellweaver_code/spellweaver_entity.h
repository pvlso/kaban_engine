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

// NOTE(paul): Entity General ========================================================================================
struct move_spec
{
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

struct entity_id
{
    u32 Value;
};

enum entity_type
{
    EntityType_Null,
    
    EntityType_Hero,
    EntityType_NPC,

    EntityType_Wall,
    EntityType_Familiar,
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
    EntityType_Obstacle,
    EntityType_Obelisk,

    EntityType_FlyingSpell,
    EntityType_ImmidiateSpell,

    EntityType_Decoration,
    EntityType_AnimatedDecoration,
    EntityType_Collision,
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

enum entity_flags
{
    EntityFlag_Collides = (1 << 0),
    EntityFlag_Moveable = (1 << 1),
    EntityFlag_Deleted = (1 << 2),
    EntityFlag_ZSupported = (1 << 3),
    EntityFlag_Traversable = (1 << 4),
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

union entity_reference
{
    entity_id ID;
    entity *Ptr;
};

struct entity_collision_volume
{
    rectangle3 CollisionRect;
    v3 OffsetP;
    r32 Height;
};

struct timer
{
    b32 Finished;
    r32 DurationSeconds;
    r32 CurrentTime;
};

struct entity
{
    // NOTE(casey): This are only for the sim region
    world_position TileP;

    entity_id ID;
    bool32 Updatable;
    
    // NOTE (paul): General

    u32 HealthMax_Health;
    u32 ManaMax_Mana;
    
    entity_general_type GeneralType;
    entity_type Type;

    u32 Flags;
    entity_state State;

    as_tile_node *StartNode;
    as_tile_node *EndNode;
    heap MovePointMaxHeap;
    
    real32 DistanceLimit;
    entity_collision_volume *Collision;

    u32 RefCount;
    entity_reference References[8];
    
    v3 P;
    v3 dP;

    r32 RenderHeight;
    u32 FacingDirection;

    bitmap_id BitmapID;
    
    b32 AnimationTypeHaveChanged;
    u32 SpriteSheetOffset;
    u32 AnimationType;
    
    // NOTE(paul): For each facing direction
    u8 SpriteSheetSpeed[AnimationType_Count][4];
    spritesheet_id SpriteSheets[AnimationType_Count][4];

    // NOTE(paul): Fighting, effects and spells
    attack_type AttackType;
    u32 AttackTimerIndex[AttackType_Count];
    u32 AttackSpriteFinishIndex[AttackType_Count];

    castspell_type CastSpellType;
    u32 CastSpellTimerIndex[AttackType_Count];
    u32 CastSpellSpriteFinishIndex[AttackType_Count];

    u32 TimerCount;
    timer Timers[8];

    // NOTE(paul): Three similar sounds
    sound_id AnimationSoundEffect[AnimationType_Count][3];
    sound_id AttackImpactSound[3];
    
    void *Data;
};
// ===================================================================================================================

// NOTE(paul): Quests ================================================================================================
struct quest;
struct kill_monsters
{
    u32 MonsterCount;
    entity_id MonstersToKill[16];
};

struct find_item
{
    u32 Name;
};

struct talk_to_npc
{
    entity_id NPCToTalk;
};

struct finished_quest
{
    u32 QuestID;
};

enum complition_type
{
    ComplitionType_Kill,
    ComplitionType_Find,
    ComplitionType_Talk,
    ComplitionType_Quest,
};

union complition_requirements
{
    kill_monsters KillMonsters;
    find_item FindItem;
    talk_to_npc TalkToNPC;
    finished_quest FinishedQuest;
};

enum reward_type
{
    RewardType_Quest,
    RewardType_TalkingGiver,
    RewardType_DestroyObstacle,
    RewardType_GameEnd,
};

enum reward_condition
{
    RewardCondition_WhenCompleted,
    RewardCondition_TalkToGiver,
};

union reward
{
    u32 QuestID;
    entity_id TalkingGiverID;
    entity_id Obstacles[8];
    // TODO(paul): Add others rewards.
};

struct quest
{
    quest_type Type;
    quest_name QuestName;
    char *UnCompletedText;
    char *CompletedText;

    world_position Location;
    world_position GiverLocation;

    quest_id QuestTextID;
    b32 FullyComleted;
    
    entity_id QuestGiverNPC;

    u32 RequirementsCount;
    b32 Completed;
    b32 IsCompleted[8];
    complition_type ComplitionType[8];
    complition_requirements CompRequirements[8];

    reward_condition RewardCondition;
    u32 RewardCount;
    reward_type RewardType[8];
    reward Reward[8];
};
// ===================================================================================================================

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

struct casted_spell
{
    spell_type Type;

    r32 SpellName;
    r32 MagicElement;

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

};
// ===================================================================================================================

struct render_entity
{
    u32 EntitySpriteIndex;
    loaded_spritesheet *SpriteSheet;
};

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

inline void
MakeEntitySpatial(entity *Entity, v3 P, v3 dP)
{
    Entity->P = P;
    Entity->dP = dP;
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
    if(Entity->AnimationType != DesiredAnimationType)
    {
        Entity->AnimationType = DesiredAnimationType;
        Entity->AnimationTypeHaveChanged = true;
    }
}

inline void
ChangeEntityState(entity *Entity, entity_state DesiredState)
{
    Entity->State = DesiredState;
}

#define SPELLWEAVER_ENTITY_H
#endif

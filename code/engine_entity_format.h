#if !defined(ENGINE_ENTITY_FORMAT_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

enum entity_vulnerabilities
{
    Vulner_Fire = (1 << 0),
    Vulner_Water = (1 << 1),
    Vulner_Air = (1 << 2),
    Vulner_Earth = (1 << 3),
};

enum entity_loot
{
    Loot_Potion = (1 << 0),
    Loot_QuestItem = (1 << 1),
    Loo_Element = (1 << 2),
};

struct entity_stats
{
    u16 Health;
    u16 Mana;

    u32 Vulnerabilities;
    u32 InVulnerabilities;

    u16 Damage;
    f32 MoveSpeed;
    u16 Loot;
};

enum entity_ability_type
{
    EntityAbility_MeleeAtack,
    EntityAbility_SpellCast,
    EntityAbility_Spawn,
};

struct entity_melee_ability
{
    u32 Damage;
};

struct entity_spell_ability
{
    u32 SpellID;
};

struct entity_other_ability
{
//    u32 AbilityID;
};

struct entity_ability
{
    entity_ability_type Type;
    u32 AbilityEffects;
    u32 AbilitySound;
    u32 AbilitySpriteSheets[4];
    union
    {
        melee_ability MeleeAb;
        spell_ability SpellAb;
        other_ability OtherAb;
    };
};

struct entity_sprites
{
    u32 IdleSpriteSheet[4];
    u32 RunSpriteSheet[4];
    u32 DeathSpriteSheet[4];
};

struct entity_sounds
{
    u32 IdleSound;
    u32 RunSound;
    u32 DeathSound;
};

enum entity_death_effect
{
    EntityDeath_None,
    EntityDeath_Explode,
    EntityDeath_Spawn,
};

struct editor_entity
{
    entity_type Type;
    entity_class Class;

    entity_stats Stats;

    entity_ability Abilities[16];
    entity_sprites Sprites;
    entity_sounds Sounds;
    entity_death_effect DeathEffect;
};

#define ENGINE_ENTITY_FORMAT_H
#endif

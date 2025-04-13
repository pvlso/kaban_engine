#if !defined(ENGINE_ENTITY_FORMAT_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

enum stored_entity_type
{
    EntityType_Hero,
    EntityType_Monster,
};

#pragma pack(push, 1)

#define SSEF_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
struct ssef_header
{
#define SSEF_MAGIC_VALUE SSEF_CODE('s', 's', 'e', 'f')
    u32 MagicValue;
    u32 Version;

    u32 StoredEntitySize;
    u32 StoredEntityCount;

    u64 StoredEntities;
};

struct stored_entity_stats
{
    u32 MaxHealth_Health;
    u32 MaxMana_Mana;

    u32 Bufs;
    u32 Debufs;

    u32 Vulnerabilities;
    u32 Invulnerabilities;
};

struct stored_entity_animation
{
    u8 SpriteSheetSpeedIdle[4];
    spritesheet_id IdleSpriteSheet[4];

    u8 SpriteSheetSpeedRun[4];
    spritesheet_id RunSpriteSheet[4];

    u8 SpriteSheetSpeedDeath[4];
    spritesheet_id RunSpriteSheet[4];
};

struct stored_entity_ability
{
    entity_ability_type Type;

    u8 AbilitySpriteSheetSpeed[4];
    spritesheet_id AbilitySpriteSheet[4];

    u8 AbilitySpriteFinishIndex;
    
};

struct stored_entity
{
    u32 StoredEntityID;

    stored_entity_type Type;
    stored_entity_stats Stats;

    
};

#pragma pack(pop)

#define ENGINE_ENTITY_FORMAT_H
#endif

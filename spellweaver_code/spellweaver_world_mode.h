#if !defined(SPELLWEAVER_WORLD_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
struct game_mode_world;

struct pairwise_collision_rule
{
    bool32 CanCollide;
    entity_id IDA;
    entity_id IDB;

    pairwise_collision_rule *NextInHash;
};

struct game_state;
internal void AddCollisionRule(game_mode_world *WorldMode, entity_id StorageIndexA, entity_id StorageIndexB, bool32 CanCollide);
internal void ClearCollisionRulesFor(game_mode_world *WorldMode, entity_id StorageIndex);

struct particle_cel
{
    real32 Density;
    v3 VelocityTimesDensity;
};
struct particle
{
    bitmap_id BitmapID;
    v3 P;
    v3 dP;
    v3 ddP;
    v4 Color;
    v4 dColor;
};

enum world_update_mode
{
    UpdateMode_Entities,
    UpdateMode_Conversation,
};

struct game_mode_world
{
    world *World;
    real32 TypicalFloorHeight;

    text_config GeneralTextConfig;
    
    b32 HeroExist;
    b32 QuitRequested;
    b32 GameFinished;
    entity_id CameraFollowingEntityIndex;
    world_position CameraP;
    world_position LastCameraP;

    world_position CameraBoundsMin;
    world_position CameraBoundsMax;

    entity_id TalkingEntityID;
    world_update_mode UpdateMode;

    loaded_bitmap MiniMapBitmap;
    
    u32 QuestCount;
    quest Quests[16];

    entity_id EntitiesToDestroy[64];

    as_tile_node *StartNode;
    as_tile_node *EndNode;

//    u32 MoveNodeCount;
//    move_point MovePoints[256];

    heap MovePointMaxHeap;
    
    // TODO(casey): Must be power of two
    pairwise_collision_rule *CollisionRuleHash[256];
    pairwise_collision_rule *FirstFreeCollisionRule;

    entity_collision_volume *NullCollision;
    entity_collision_volume *SphereCollision;
    entity_collision_volume *ItemCollision;
    entity_collision_volume *SwordCollision;
    entity_collision_volume *PlayerCollision;
    entity_collision_volume *MonsterCollision;
    entity_collision_volume *FamiliarCollision;
    entity_collision_volume *WallCollision;
    entity_collision_volume *NPCCollision;

    entity_collision_volume *SpellCollision;
    entity_collision_volume *MagicSwordCollision;

    u32 CreationBufferIndex;
    entity CreationBuffers[16];
    u32 LastUsedEntityStorageIndex;

    real32 Time;

    playing_sound *GameEndMusic;
    playing_sound *BirdSound;
    playing_sound *RiverSound;
    polygon2 BirdSoundPolygon;
    polygon2 RiverPolygon0;
    polygon2 RiverPolygon1;

    random_series EffectsEntropy; // NOTE(casey): This is entropy that doesn't affect the gameplay
    random_series MathEntropy;
    real32 tSine;

#define PARTICLE_CEL_DIM 32
    u32 NextParticle;
    particle Particles[256];
    
    particle_cel ParticleCels[PARTICLE_CEL_DIM][PARTICLE_CEL_DIM];
};

internal void PlayWorld(game_state *GameState, transient_state *TranState);

#define SPELLWEAVER_WORLD_MODE_H
#endif

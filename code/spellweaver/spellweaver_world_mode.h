#if !defined(SPELLWEAVER_WORLD_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
struct world_state;

struct pairwise_collision_rule
{
    bool32 CanCollide;
    entity_id IDA;
    entity_id IDB;

    pairwise_collision_rule *NextInHash;
};

internal void AddCollisionRule(world_state *WorldMode, entity_id StorageIndexA, entity_id StorageIndexB, bool32 CanCollide);
internal void ClearCollisionRulesFor(world_state *WorldMode, entity_id StorageIndex);

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

struct world_state
{
    world *World;
    
    b32 HeroExist;
    b32 QuitRequested;
    b32 GameFinished;

    entity_id CameraFollowingEntityIndex;
    world_position CameraP;
    world_position LastCameraP;

    world_position CameraBoundsMin;
    world_position CameraBoundsMax;

    loaded_bitmap MiniMapBitmap;

    entity_id EntitiesToDestroy[64];
    
    // TODO(casey): Must be power of two
    pairwise_collision_rule *CollisionRuleHash[256];
    pairwise_collision_rule *FirstFreeCollisionRule;

    entity_collision *NullCollision;
    entity_collision *SphereCollision;
    entity_collision *PlayerCollision;
    entity_collision *TileCollision;
    entity_collision *SpellCollision;
    entity_collision *GolemCollision;
    
    u32 CreationBufferIndex;
    entity CreationBuffers[16];
    u32 LastUsedEntityStorageIndex;

    entity_id *TileMap;

    real32 Time;

    playing_sound *GameEndMusic;

    random_series EffectsEntropy; // NOTE(casey): This is entropy that doesn't affect the gameplay
    random_series MathEntropy;
    real32 tSine;

#define PARTICLE_CEL_DIM 32
    u32 NextParticle;
    particle Particles[256];
    
    particle_cel ParticleCels[PARTICLE_CEL_DIM][PARTICLE_CEL_DIM];
};

internal void PlayWorld(game_state *GameState, struct game_transient_state *TranState);

#define SPELLWEAVER_WORLD_MODE_H
#endif

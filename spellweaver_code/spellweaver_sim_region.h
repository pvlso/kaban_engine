#if !defined(SPELLWEAVER_SIM_REGION_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
struct entity_hash
{
    entity_id Index;
    entity *Ptr;
};

struct sim_region
{
    // TODO(casey): Need a hash table here to map stored entity indices
    // to sim entities!

    world *World;
    real32 MaxEntityRadius;
    real32 MaxEntityVelocity;
    
    world_position Origin;
    rectangle2 Bounds;
    rectangle2 UpdatableBounds;

    uint32 MaxEntityCount;
    uint32 EntityCount;
    entity *Entities;

    real32 GroundZBase;
    
    // TODO(casey): Do I really want a hash for this?
    // NOTE(casey): Must be a power of two!
    entity_hash Hash[4096];
};

#define SPELLWEAVER_SIM_REGION_H
#endif

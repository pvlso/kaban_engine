/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
entity_collision_volume *
MakeSimpleGroundedCollision(game_mode_world *WorldMode, real32 DimX, real32 DimY, real32 DimZ)
{
    // TODO(casey): NOT WORLD ARENA!  Change to using the fundamental types arena, etc.
    entity_collision_volume *Collision = PushStruct(&WorldMode->World->Arena, entity_collision_volume);
    Collision->OffsetP = V3(0, 0, 0);
    Collision->CollisionRect = RectCenterDim(Collision->OffsetP, V3(DimX, DimY, DimZ));
    Collision->Height = DimZ;
    
    return(Collision);
}

entity_collision_volume *
MakeNullCollision(game_mode_world *WorldMode)
{
    // TODO(casey): NOT WORLD ARENA!  Change to using the fundamental types arena, etc.
    entity_collision_volume *Collision = PushStruct(&WorldMode->World->Arena, entity_collision_volume);
    Collision->OffsetP = V3(0, 0, 0);
    Collision->CollisionRect = InvertedInfinityRectangle3();
    Collision->Height = 0;

    return(Collision);
}

internal void
ClearCollisionRulesFor(game_mode_world *WorldMode, entity_id ID)
{
    // TODO(casey): Need to make a better data structure that allows
    // removal of collision rules without searching the entire table
    // NOTE(casey): One way to make removal easy would be to always
    // add _both_ orders of the pairs of storage indices to the
    // hash table, so no matter which position the entity is in,
    // you can always find it.  Then, when you do your first pass
    // through for removal, you just remember the original top
    // of the free list, and when you're done, do a pass through all
    // the new things on the free list, and remove the reverse of
    // those pairs.
    for(uint32 HashBucket = 0;
        HashBucket < ArrayCount(WorldMode->CollisionRuleHash);
        ++HashBucket)
    {
        for(pairwise_collision_rule **Rule = &WorldMode->CollisionRuleHash[HashBucket];
            *Rule;
            )
        {
            if(((*Rule)->IDA.Value == ID.Value) ||
               ((*Rule)->IDB.Value == ID.Value))
            {
                pairwise_collision_rule *RemovedRule = *Rule;
                *Rule = (*Rule)->NextInHash;

                RemovedRule->NextInHash = WorldMode->FirstFreeCollisionRule;
                WorldMode->FirstFreeCollisionRule = RemovedRule;
            }
            else
            {
                Rule = &(*Rule)->NextInHash;
            }
        }
    }
}

internal void
AddCollisionRule(game_mode_world *WorldMode, entity_id IDA, entity_id IDB, bool32 CanCollide)
{
    // TODO(casey): Collapse this with ShouldCollide
    if(IDA.Value > IDB.Value)
    {
        entity_id Temp = IDA;
        IDA = IDB;
        IDB = Temp;
    }

    // TODO(casey): BETTER HASH FUNCTION
    pairwise_collision_rule *Found = 0;
    uint32 HashBucket = IDA.Value & (ArrayCount(WorldMode->CollisionRuleHash) - 1);
    for(pairwise_collision_rule *Rule = WorldMode->CollisionRuleHash[HashBucket];
        Rule;
        Rule = Rule->NextInHash)
    {
        if((Rule->IDA.Value == IDA.Value) &&
           (Rule->IDB.Value == IDB.Value))
        {
            Found = Rule;
            break;
        }
    }
    
    if(!Found)
    {
        Found = WorldMode->FirstFreeCollisionRule;
        if(Found)
        {
            WorldMode->FirstFreeCollisionRule = Found->NextInHash;
        }
        else
        {
            Found = PushStruct(&WorldMode->World->Arena, pairwise_collision_rule);
        }
        
        Found->NextInHash = WorldMode->CollisionRuleHash[HashBucket];
        WorldMode->CollisionRuleHash[HashBucket] = Found;
    }

    if(Found)
    {
        Found->IDA.Value = IDA.Value;
        Found->IDB.Value = IDB.Value;
        Found->CanCollide = CanCollide;
    }
}

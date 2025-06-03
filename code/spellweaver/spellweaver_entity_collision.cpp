/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */
internal entity_collision *
MakeSimpleGroundedCollision(world_state *WorldState, real32 DimX, real32 DimY, real32 DimZ)
{
    // TODO(casey): NOT WORLD ARENA!  Change to using the fundamental types arena, etc.
    entity_collision *Collision = PushStruct(&WorldState->World->Arena, entity_collision);
    Collision->OffsetP = V3(0, 0, 0);
    Collision->CollisionRect = RectCenterDim(Collision->OffsetP, V3(DimX, DimY, DimZ));
    Collision->Height = DimZ;
    
    return(Collision);
}

internal entity_collision *
MakeNullCollision(world_state *WorldState)
{
    // TODO(casey): NOT WORLD ARENA!  Change to using the fundamental types arena, etc.
    entity_collision *Collision = PushStruct(&WorldState->World->Arena, entity_collision);
    Collision->OffsetP = V3(0, 0, 0);
    Collision->CollisionRect = InvertedInfinityRectangle3();
    Collision->Height = 0;

    return(Collision);
}

internal void
ClearCollisionRulesFor(world_state *WorldState, entity_id ID)
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
        HashBucket < ArrayCount(WorldState->CollisionRuleHash);
        ++HashBucket)
    {
        for(pairwise_collision_rule **Rule = &WorldState->CollisionRuleHash[HashBucket];
            *Rule;
            )
        {
            if(((*Rule)->IDA.Value == ID.Value) ||
               ((*Rule)->IDB.Value == ID.Value))
            {
                pairwise_collision_rule *RemovedRule = *Rule;
                *Rule = (*Rule)->NextInHash;

                RemovedRule->NextInHash = WorldState->FirstFreeCollisionRule;
                WorldState->FirstFreeCollisionRule = RemovedRule;
            }
            else
            {
                Rule = &(*Rule)->NextInHash;
            }
        }
    }
}

internal void
AddCollisionRule(world_state *WorldState, entity_id IDA, entity_id IDB, bool32 CanCollide)
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
    uint32 HashBucket = IDA.Value & (ArrayCount(WorldState->CollisionRuleHash) - 1);
    for(pairwise_collision_rule *Rule = WorldState->CollisionRuleHash[HashBucket];
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
        Found = WorldState->FirstFreeCollisionRule;
        if(Found)
        {
            WorldState->FirstFreeCollisionRule = Found->NextInHash;
        }
        else
        {
            Found = PushStruct(&WorldState->World->Arena, pairwise_collision_rule);
        }
        
        Found->NextInHash = WorldState->CollisionRuleHash[HashBucket];
        WorldState->CollisionRuleHash[HashBucket] = Found;
    }

    if(Found)
    {
        Found->IDA.Value = IDA.Value;
        Found->IDB.Value = IDB.Value;
        Found->CanCollide = CanCollide;
    }
}

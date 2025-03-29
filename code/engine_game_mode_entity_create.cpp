/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal entity *
BeginEntity(editor_mode_game *GameMode, entity_general_type GeneralType, entity_type Type, u32 CreationFlags)
{
    Assert(GameMode->CreationBufferIndex < ArrayCount(GameMode->CreationBuffers));
    entity *Entity = GameMode->CreationBuffers + GameMode->CreationBufferIndex++;

    ZeroStruct(*Entity);
    Entity->ID.Value = ++GameMode->LastUsedEntityStorageIndex;

    Entity->CreationFlags = CreationFlags;
    Entity->GeneralType = GeneralType;
    Entity->Type = Type;
    
    Entity->State = EntityState_Staying;
//    Entity->Collision = GameMode->NullCollision;

//    Entity->RenderHeight = GameMode->World->TileSideInMeters;

    return(Entity);
}

internal void
EndEntity(editor_mode_game *GameMode, entity *Entity, world_position P)
{
    --GameMode->CreationBufferIndex;
    Assert(Entity == (GameMode->CreationBuffers + GameMode->CreationBufferIndex));

    PackEntityIntoWorld(&GameMode->World->Arena, GameMode->World, Entity, P);
}

internal entity *
BeginGroundedEntity(editor_mode_game *GameMode, entity_general_type GeneralType, entity_type Type, u32 CreationFlags,
                    entity_collision *Collision)
{
    entity *Entity = BeginEntity(GameMode, GeneralType, Type, CreationFlags);
    Entity->Collision = Collision;
    return(Entity);
}

internal entity_id
AddTile(editor_mode_game *GameMode, world_position P, entity_collision *Collision, b32 Occupied)
{
    entity *Entity = BeginGroundedEntity(GameMode, GeneralType_Object, EntityType_Tile, CreationFlag_DataNeeded,
                                         Collision);
    tile_entity *Tile = (tile_entity *)Entity->Data;
//    Tile->Occupied = Occupied;

    if(Occupied)
    {
        AddFlags(Entity, EntityFlag_Collides);
    }

    entity_id Result = Entity->ID;
    EndEntity(GameMode, Entity, P);

    return(Result);
}

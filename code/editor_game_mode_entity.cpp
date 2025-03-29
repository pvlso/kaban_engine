/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#include "editor_game_mode_entity_utilities.cpp"
#include "editor_game_mode_entity_create.cpp"

inline u32
UpdateSpriteIndex(entity *Entity, r32 Time, u32 SpriteCount, u32 Speed)
{
    u32 Result = 0;
    if(Entity->Animation->AnimationTypeHaveChanged)
    {
        s32 Index = GetSpriteIndex(Time, SpriteCount, 0, Speed);
        Entity->Animation->SpriteSheetOffset = Index;
        Entity->Animation->AnimationTypeHaveChanged = false;
    }

    if(Entity->Animation->SpriteSheetOffset > SpriteCount)
    {
        s32 Index = GetSpriteIndex(Time, SpriteCount, 0, Speed);
        Entity->Animation->SpriteSheetOffset = Index;
    }
                        
    s32 SpriteIndex = GetSpriteIndex(Time, SpriteCount, Entity->Animation->SpriteSheetOffset, Speed);
    Assert(SpriteIndex >= 0);

    Result = SpriteIndex;
    return(Result);
}

internal void
RenderEntities(editor_mode_game *GameMode, sim_region *SimRegion, render_group *RenderGroup,
               object_transform *EntityTransform, entity *Entity, r32 dt, render_entity *RenderEntity)
{
    loaded_spritesheet *SpriteSheet = RenderEntity->SpriteSheet;
    u32 EntitySpriteIndex = RenderEntity->EntitySpriteIndex;

    switch(Entity->Type)
    {
        
        case EntityType_Tile:
        {
            tile_entity *Tile = (tile_entity *)Entity->Data;
            PushRect(RenderGroup, EntityTransform, V3(0, 0, 0), V2(1.0f, 1.0f), V4(1, 0, 0, 1));
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }
}

internal void
UpdateAndRenderEntities(editor_mode_game *GameMode, sim_region *SimRegion, render_group *RenderGroup, real32 dt, v2 MouseP)
{
    TIMED_FUNCTION();

    object_transform EntityTransform = DefaultUprightTransform();
    v3 LocalMouseP = Unproject(RenderGroup, &EntityTransform, MouseP);

    for(uint32 EntityIndex = 0;
        EntityIndex < SimRegion->EntityCount;
        ++EntityIndex)
    {
        entity *Entity = SimRegion->Entities + EntityIndex;
        EntityTransform.OffsetP = GetEntityGroundPoint(Entity);

        if(Entity->Updatable)
        {
            v3 LastP = Entity->P;

            render_entity RenderEntity = {};
            b32 AnimationFinished = false;
            ssa_spritesheet *SpriteSheetInfo = 0;
            u32 AnimationSpeed = 1;
            if(IsCreationFlagSet(Entity, CreationFlag_Animated))
            {
                entity_animation *Animation = Entity->Animation;

                AnimationSpeed = Animation->SpriteSheetSpeed[Animation->AnimationType][Entity->FacingDirection];
                spritesheet_id ID = Animation->SpriteSheets[Animation->AnimationType][Entity->FacingDirection];

                if(IsValid(ID))
                {
                    RenderEntity.SpriteSheet = PushSpriteSheet(RenderGroup, ID, true);

                    if(IsValid(RenderEntity.SpriteSheet->SpriteIDs[0]))
                    {
                        SpriteSheetInfo = GetSpriteSheetInfo(RenderGroup->Assets, ID);
                        RenderEntity.EntitySpriteIndex = UpdateSpriteIndex(Entity, GameMode->Time,
                                                                           SpriteSheetInfo->SpriteCount, AnimationSpeed);
                        AnimationFinished = AnimationHasComleted(GameMode->Time, Animation->SpriteSheetOffset,
                                                                 RenderEntity.EntitySpriteIndex,
                                                                 SpriteSheetInfo->SpriteCount, dt, AnimationSpeed);
                        for(u32 SpriteIndex = 0;
                            SpriteIndex < SpriteSheetInfo->SpriteCount;
                            ++SpriteIndex)
                        {
                            bitmap_id SpriteID = RenderEntity.SpriteSheet->SpriteIDs[SpriteIndex];
                            SpriteID.Value += RenderEntity.SpriteSheet->BitmapIDOffset;
                            PrefetchBitmap(RenderGroup->Assets, SpriteID, true);
                        }
                    }
                    else
                    {
                        RenderEntity.SpriteSheet = 0;
                    }
                }
            }
            
            // NOTE(paul): Render Entity
            RenderEntities(GameMode, SimRegion, RenderGroup, &EntityTransform, Entity, dt, &RenderEntity);
        }

    }
}

#if 0
if(DEBUG_UI_ENABLED)
{
    debug_id EntityDebugID_ = DEBUG_POINTER_ID(SimRegion->Entities + Entity->ID.Value);

    for(uint32 VolumeIndex = 0;
        VolumeIndex < Entity->Collision->VolumeCount;
        ++VolumeIndex)
    {
        sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;                        

        v3 LocalMouseP = Unproject(RenderGroup, EntityTransform, MouseP);

        if((LocalMouseP.x > -0.5f*Volume->Dim.x) && (LocalMouseP.x < 0.5f*Volume->Dim.x) &&
           (LocalMouseP.y > -0.5f*Volume->Dim.y) && (LocalMouseP.y < 0.5f*Volume->Dim.y))
        {
            DEBUG_HIT(EntityDebugID_, LocalMouseP.z);
        }

        v4 OutlineColor;
        if(DEBUG_HIGHLIGHTED(EntityDebugID_, &OutlineColor))
        {
            PushRectOutline(RenderGroup, EntityTransform, Volume->OffsetP - V3(0, 0, 0.5f*Volume->Dim.z), Volume->Dim.xy, OutlineColor, 0.05f);
        }
    }
                
    if(DEBUG_REQUESTED(EntityDebugID))
    {
        DEBUG_VALUE(Entity->Updatable);
        DEBUG_VALUE(Entity->Type);
        DEBUG_VALUE(Entity->P);
        DEBUG_VALUE(Entity->FacingDirection);

        DEBUG_END_DATA_BLOCK("Simulation/Entity");
    }
}
#endif

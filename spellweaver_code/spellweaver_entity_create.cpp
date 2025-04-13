/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

internal entity *
BeginEntity(game_mode_world *WorldMode, entity_type Type, b32 DataNeeded)
{
    Assert(WorldMode->CreationBufferIndex < ArrayCount(WorldMode->CreationBuffers));
    entity *Entity = WorldMode->CreationBuffers + WorldMode->CreationBufferIndex++;

    ZeroStruct(*Entity);
    Entity->ID.Value = ++WorldMode->LastUsedEntityStorageIndex;

    Entity->Type = Type;
    Entity->AnimationType = AnimationType_Idle;
    Entity->Collision = WorldMode->NullCollision;
    Entity->MovePointMaxHeap.MaxSize = 8;
    Entity->MovePointMaxHeap.Size = 0;
    Entity->MovePointMaxHeap.Nodes = PushArray(&WorldMode->World->Arena, Entity->MovePointMaxHeap.MaxSize, sort_entry);
    Entity->Data = 0;

    if(DataNeeded)
    {
        Entity->Data = PushSize(&WorldMode->World->Arena, Kilobytes(2));
    }
    
    return(Entity);
}

internal void
EndEntity(game_mode_world *WorldMode, entity *Entity, world_position P)
{
    --WorldMode->CreationBufferIndex;
    Assert(Entity == (WorldMode->CreationBuffers + WorldMode->CreationBufferIndex));

    PackEntityIntoWorld(&WorldMode->World->Arena, WorldMode->World, Entity, P);
}

internal entity *
BeginGroundedEntity(game_mode_world *WorldMode, entity_type Type, b32 DataNeeded,
                    entity_collision_volume *Collision)
{
    entity *Entity = BeginEntity(WorldMode, Type, DataNeeded);
    Entity->Collision = Collision;
    return(Entity);
}

inline void
AddEntitySpriteSheetsForType(game_assets *Assets, entity *Entity, u32 AnimationType,
                             asset_vector *MatchVector, asset_vector *WeightVector)
{
    real32 Angles[4] = {1.0f*Pi32, 1.5f*Pi32, 0.5f*Pi32, 0.0f*Pi32};

    for(u32 AngleIndex = 0;
        AngleIndex < ArrayCount(Angles);
        ++AngleIndex)
    {
        real32 Angle = Angles[AngleIndex];
        MatchVector->E[Tag_FacingDirection] = Angle;
        MatchVector->E[Tag_AnimationType] = (r32)AnimationType;
        Entity->SpriteSheets[AnimationType][AngleIndex] =
            GetBestMatchSpriteSheetFrom(Assets, Asset_SpriteSheet, MatchVector, WeightVector);
    }
}

inline void
AddEntitySpriteSheets(game_assets *Assets, entity *Entity, asset_vector *MatchVector, asset_vector *WeightVector)
{
    for(u32 SheetIndex = 0;
        SheetIndex < AnimationType_Count;
        ++SheetIndex)
    {
        AddEntitySpriteSheetsForType(Assets, Entity, SheetIndex, MatchVector, WeightVector);
    }
}

internal void
AddCollisionEntity(game_mode_world *WorldMode, game_assets *Assets, collision *Collision)
{
    entity *Entity = BeginEntity(WorldMode, EntityType_Collision, false);
    Entity->GeneralType = GeneralType_Object;
    
    v2 CollisionDim = GetDim(Collision->Rect);
    Entity->Collision = MakeSimpleGroundedCollision(WorldMode, CollisionDim.x,
                                                    CollisionDim.y, 0.0f);
    Entity->Collision->OffsetP -= V3(1.5f, 1.5f, 0.0f);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_ZSupported);
    world_position P = TilePositionFromChunkPosition(&Collision->P);
    EndEntity(WorldMode, Entity, P);
}

internal void
AddDecorationEntity(game_mode_world *WorldMode, game_assets *Assets, decoration *Decoration)
{
    entity *Entity = BeginEntity(WorldMode, EntityType_Decoration, false);
    Entity->GeneralType = GeneralType_Object;
                        
    Entity->BitmapID = Decoration->BitmapID;

    Entity->Collision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 1.0f, Decoration->Height);
    Entity->Collision->OffsetP -= V3(1.5f, 1.5f, 0.0f);
    
    world_position P = TilePositionFromChunkPosition(&Decoration->P);
    EndEntity(WorldMode, Entity, P);
}

internal void
AddAnimatedDecorationEntity(game_mode_world *WorldMode, game_assets *Assets, decoration *Decoration)
{
    entity *Entity = BeginEntity(WorldMode, EntityType_AnimatedDecoration, false);
    Entity->GeneralType = GeneralType_Object;

    Entity->SpriteSheets[AnimationType_Idle][0] = Decoration->SpriteSheetID;

    Entity->Collision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 1.0f, Decoration->Height);
    Entity->Collision->OffsetP -= V3(1.5f, 1.5f, 0.0f);
    
    world_position P = TilePositionFromChunkPosition(&Decoration->P);
    EndEntity(WorldMode, Entity, P);
}

internal entity_id
AddItem(game_mode_world *WorldMode, game_assets *Assets, world_position P, item_name ItemName)
{
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Item, true, WorldMode->ItemCollision);
    Entity->FacingDirection = 0;
    Entity->RenderHeight = 0.8f;
    Entity->GeneralType = GeneralType_Item;

    item_entity *Data = (item_entity *)Entity->Data;
    Data->Name = ItemName;

    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;
    WeightVector.E[Tag_ItemName] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Item;
    MatchVector.E[Tag_ItemName] = (r32)ItemName;

    AddEntitySpriteSheetsForType(Assets, Entity, AnimationType_Idle, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_ZSupported);

    entity_id Result = Entity->ID;
    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddObstacle(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY,
            asset_type_id AssetType, asset_vector *MatchVector, asset_vector *WeightVector,
            r32 RenderHeight, v2 CollisionDim, v3 CollisionOffset = V3(0, 0, 0))
{
    world_position P = CenteredTilePoint(AbsTileX, AbsTileY);
    entity *Entity = BeginEntity(WorldMode, EntityType_Obstacle, false);

    Entity->Collision = MakeSimpleGroundedCollision(WorldMode, CollisionDim.x, CollisionDim.y, 0.0f);
    Entity->Collision->OffsetP = CollisionOffset;
    Entity->RenderHeight = RenderHeight;
    Entity->GeneralType = GeneralType_Object;

    Entity->BitmapID = GetBestMatchBitmapFrom(Assets, AssetType, MatchVector, WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_ZSupported);

    entity_id Result = Entity->ID;
    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddObelisk(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginEntity(WorldMode, EntityType_Obelisk, false);

    Entity->Collision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 1.0f, 0.0f);
    Entity->RenderHeight = 10.0f;
    Entity->GeneralType = GeneralType_Object;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Obelisk;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_ZSupported);

    entity_id Result = Entity->ID;
    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddElderTavor(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_NPC, true, WorldMode->NPCCollision);
    talkingnpc_entity *EntityData = (talkingnpc_entity *)Entity->Data;
    
    Entity->RenderHeight = 2.2f;
    Entity->FacingDirection = 3;
    Entity->GeneralType = GeneralType_Allay;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;
    WeightVector.E[Tag_Sex] = 1.0f;
    WeightVector.E[Tag_Age] = 1.0f;
    WeightVector.E[Tag_HairColor] = 1.0f;
    WeightVector.E[Tag_Beard] = 1.0f;
    WeightVector.E[Tag_Accessories] = 1.0f;
    WeightVector.E[Tag_TopOutfit] = 1.0f;
    WeightVector.E[Tag_TopOutfitColor] = 1.0f;
    WeightVector.E[Tag_BottomOutFit] = 1.0f;
    WeightVector.E[Tag_BottomOutFitColor] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_NPCCharecter;
    MatchVector.E[Tag_Sex] = (r32)Sex_Male;
    MatchVector.E[Tag_Age] = (r32)Age_Old;
    MatchVector.E[Tag_HairColor] = (r32)Color_LightBlack;
    MatchVector.E[Tag_Beard] = (r32)Beard_Thick;
    MatchVector.E[Tag_Accessories] = (r32)Accessories_Hat;
    MatchVector.E[Tag_TopOutfit] = (r32)TopOutfit_CoatNoSleeves;
    MatchVector.E[Tag_TopOutfitColor] = (r32)Color_Brown;
    MatchVector.E[Tag_BottomOutFit] = (r32)BottomOutfit_Pans;
    MatchVector.E[Tag_BottomOutFitColor] = (r32)Color_Brown;

    AddEntitySpriteSheetsForType(Assets, Entity, AnimationType_Idle, &MatchVector, &WeightVector);

    asset_vector GeneralTextMatchVector = {};
    asset_vector GeneralTextWeightVector = {};
    GeneralTextMatchVector.E[Tag_NPCName] = (r32)NPCName_ElderTavor;
    GeneralTextMatchVector.E[Tag_ConversationType] = (r32)ConType_GeneralDialogue;

    GeneralTextWeightVector.E[Tag_NPCName] = 1.0f;
    GeneralTextWeightVector.E[Tag_ConversationType] = 1.0f;

    EntityData->TalkingState = TalkingState_QuestGiver;
    EntityData->ParagraphIndex = 0;
    EntityData->GeneralText.Value = 0;
    EntityData->GeneralText = GetBestMatchTextFrom(Assets, Asset_Text,
                                                   &GeneralTextMatchVector, &GeneralTextWeightVector);
    EntityData->QuestID = QuestName_TheLostTome;

    EntityData->NPCName = PushString(&WorldMode->World->Arena, "Tavor");

    asset_vector QuestMarkMatchVector = {};
    asset_vector QuestMarkWeightVector = {};
    QuestMarkWeightVector.E[Tag_QuestRelated] = 1.0f;

    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_Giver;
    EntityData->QuestMark[TalkingState_QuestGiver] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_Objective;
    EntityData->QuestMark[TalkingState_QuestObjective] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_ComplitionDialogue;
    EntityData->QuestMark[TalkingState_QuestComleted] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_ZSupported);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddHerbalistElara(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_NPC, true, WorldMode->NPCCollision);
    talkingnpc_entity *EntityData = (talkingnpc_entity *)Entity->Data;
    
    Entity->RenderHeight = 2.2f;
    Entity->FacingDirection = 3;
    Entity->GeneralType = GeneralType_Allay;
    
    real32 Angles[4] = {1.0f*Pi32, 1.5f*Pi32, 0.5f*Pi32, 0.0f*Pi32};
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;
    WeightVector.E[Tag_Sex] = 1.0f;
    WeightVector.E[Tag_Age] = 1.0f;
    WeightVector.E[Tag_HairColor] = 1.0f;
    WeightVector.E[Tag_Haircut] = 1.0f;
    WeightVector.E[Tag_Accessories] = 1.0f;
    WeightVector.E[Tag_TopOutfit] = 1.0f;
    WeightVector.E[Tag_TopOutfitColor] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_NPCCharecter;
    MatchVector.E[Tag_Sex] = (r32)Sex_Female;
    MatchVector.E[Tag_Age] = (r32)Age_Young;
    MatchVector.E[Tag_HairColor] = (r32)Color_LightBrown;
    MatchVector.E[Tag_Haircut] = (r32)Haircut_Tuft;
    MatchVector.E[Tag_Accessories] = (r32)Accessories_None;
    MatchVector.E[Tag_TopOutfit] = (r32)TopOutfit_Drass;
    MatchVector.E[Tag_TopOutfitColor] = (r32)Color_Green;

    AddEntitySpriteSheetsForType(Assets, Entity, AnimationType_Idle, &MatchVector, &WeightVector);

    asset_vector GeneralTextMatchVector = {};
    asset_vector GeneralTextWeightVector = {};
    GeneralTextMatchVector.E[Tag_NPCName] = (r32)NPCName_Elara;
    GeneralTextMatchVector.E[Tag_ConversationType] = (r32)ConType_GeneralDialogue;

    GeneralTextWeightVector.E[Tag_NPCName] = 1.0f;
    GeneralTextWeightVector.E[Tag_ConversationType] = 1.0f;

    EntityData->TalkingState = TalkingState_General;
    EntityData->ParagraphIndex = 0;
    EntityData->GeneralText.Value = 0;
    EntityData->GeneralText = GetBestMatchTextFrom(Assets, Asset_Text,
                                                   &GeneralTextMatchVector, &GeneralTextWeightVector);
    EntityData->QuestID = QuestName_HerbalistsPlea;

    EntityData->NPCName = PushString(&WorldMode->World->Arena, "Elara");

    asset_vector QuestMarkMatchVector = {};
    asset_vector QuestMarkWeightVector = {};
    QuestMarkWeightVector.E[Tag_QuestRelated] = 1.0f;

    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_Giver;
    EntityData->QuestMark[TalkingState_QuestGiver] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_Objective;
    EntityData->QuestMark[TalkingState_QuestObjective] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_ComplitionDialogue;
    EntityData->QuestMark[TalkingState_QuestComleted] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_ZSupported);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddJacob(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_NPC, true, WorldMode->NPCCollision);
    talkingnpc_entity *EntityData = (talkingnpc_entity *)Entity->Data;
    
    Entity->RenderHeight = 2.2f;
    Entity->FacingDirection = 3;
    Entity->GeneralType = GeneralType_Allay;
    
    real32 Angles[4] = {1.0f*Pi32, 1.5f*Pi32, 0.5f*Pi32, 0.0f*Pi32};
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;
    WeightVector.E[Tag_Sex] = 1.0f;
    WeightVector.E[Tag_Age] = 1.0f;
    WeightVector.E[Tag_HairColor] = 1.0f;
    WeightVector.E[Tag_Beard] = 1.0f;
    WeightVector.E[Tag_Accessories] = 1.0f;
    WeightVector.E[Tag_TopOutfit] = 1.0f;
    WeightVector.E[Tag_TopOutfitColor] = 1.0f;
    WeightVector.E[Tag_BottomOutFit] = 1.0f;
    WeightVector.E[Tag_BottomOutFitColor] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_NPCCharecter;
    MatchVector.E[Tag_Sex] = (r32)Sex_Male;
    MatchVector.E[Tag_Age] = (r32)Age_Old;
    MatchVector.E[Tag_HairColor] = (r32)Color_Gray;
    MatchVector.E[Tag_Beard] = (r32)Beard_Thick;
    MatchVector.E[Tag_Accessories] = (r32)Accessories_None;
    MatchVector.E[Tag_TopOutfit] = (r32)TopOutfit_CoatNoSleeves;
    MatchVector.E[Tag_TopOutfitColor] = (r32)Color_Brown;
    MatchVector.E[Tag_BottomOutFit] = (r32)BottomOutfit_Pans;
    MatchVector.E[Tag_BottomOutFitColor] = (r32)Color_Brown;

    AddEntitySpriteSheetsForType(Assets, Entity, AnimationType_Idle, &MatchVector, &WeightVector);

    asset_vector GeneralTextMatchVector = {};
    asset_vector GeneralTextWeightVector = {};
    GeneralTextMatchVector.E[Tag_NPCName] = (r32)NPCName_Jacob;
    GeneralTextMatchVector.E[Tag_ConversationType] = (r32)ConType_GeneralDialogue;

    GeneralTextWeightVector.E[Tag_NPCName] = 1.0f;
    GeneralTextWeightVector.E[Tag_ConversationType] = 1.0f;

    EntityData->TalkingState = TalkingState_General;
    EntityData->ParagraphIndex = 0;
    EntityData->GeneralText.Value = 0;
    EntityData->GeneralText = GetBestMatchTextFrom(Assets, Asset_Text,
                                                   &GeneralTextMatchVector, &GeneralTextWeightVector);
    EntityData->QuestID = QuestName_JacobTalk;

    EntityData->NPCName = PushString(&WorldMode->World->Arena, "Jacob");

    asset_vector QuestMarkMatchVector = {};
    asset_vector QuestMarkWeightVector = {};
    QuestMarkWeightVector.E[Tag_QuestRelated] = 1.0f;

    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_Giver;
    EntityData->QuestMark[TalkingState_QuestGiver] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_Objective;
    EntityData->QuestMark[TalkingState_QuestObjective] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    QuestMarkMatchVector.E[Tag_QuestRelated] = (r32)Quest_ComplitionDialogue;
    EntityData->QuestMark[TalkingState_QuestComleted] =
        GetBestMatchBitmapFrom(Assets, Asset_QuestMark, &QuestMarkMatchVector, &QuestMarkWeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_ZSupported);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddGolem(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Golem, false,
                                             WorldMode->MonsterCollision);
    Entity->RenderHeight = 4.5f;
    Entity->HealthMax_Health = (u32)((80 << 16) | 80);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->AttackSpriteFinishIndex[0] = 5;
    Entity->GeneralType = GeneralType_Enemy;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Golem;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    entity_id Result = Entity->ID;
    
    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddCultist(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Cultist, true,
                                             WorldMode->MonsterCollision);

    Entity->RenderHeight = 2.5f;
    Entity->HealthMax_Health = (u32)((60 << 16) | 60);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->GeneralType = GeneralType_Enemy;

    Entity->CastSpellSpriteFinishIndex[CastSpellType_0] = 9;
    AddTimerForCast(Entity, 3.0f, CastSpellType_0);
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Cultist;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddNecromancer(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Necromancer, true,
                                         WorldMode->MonsterCollision);

    Entity->RenderHeight = 2.5f;
    Entity->HealthMax_Health = (u32)((60 << 16) | 60);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->CastSpellSpriteFinishIndex[CastSpellType_0] = 6;
    Entity->CastSpellSpriteFinishIndex[CastSpellType_1] = 6;
    Entity->CastSpellSpriteFinishIndex[CastSpellType_2] = 10;
    Entity->GeneralType = GeneralType_Enemy;

    AddTimerForCast(Entity, 3.0f, CastSpellType_0);
    AddTimerForCast(Entity, 40.0f, CastSpellType_1);
    AddTimerForCast(Entity, 6.0f, CastSpellType_2);
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Necromancer;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddPossesed(game_mode_world *WorldMode, game_assets *Assets, world_position BaseP, v2 OffsetP)
{
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Possesed, true,
                                         WorldMode->MonsterCollision);

    Entity->RenderHeight = 2.5f;
    Entity->HealthMax_Health = (u32)((40 << 16) | 40);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->AttackSpriteFinishIndex[AttackType_0] = 10;
    Entity->GeneralType = GeneralType_Enemy;

    AddTimerForAttack(Entity, 2.0f, AttackType_0);
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Possesed;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    // NOTE(paul): Attack
    Entity->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);
    Entity->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);
    Entity->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);

    // NOTE(paul): Impact
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_0);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_1);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_2);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);

    entity_id Result = Entity->ID;

    world_position Pos = MapIntoTileSpace(WorldMode->World, BaseP, OffsetP);
    EndEntity(WorldMode, Entity, Pos);

    return(Result);
}

internal entity_id
AddGoblinBeast(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_GoblinBeast, true,
                                         WorldMode->MonsterCollision);

    Entity->RenderHeight = 2.8f;
    Entity->HealthMax_Health = (u32)((100 << 16) | 100);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->AttackSpriteFinishIndex[AttackType_0] = 6;
    Entity->AttackSpriteFinishIndex[AttackType_1] = 8;
    Entity->GeneralType = GeneralType_Enemy;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_GoblinBeast;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    // NOTE(paul): Attack
    Entity->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);
    Entity->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);
    Entity->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);

    Entity->AnimationSoundEffect[AnimationType_Attack1][0] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);
    Entity->AnimationSoundEffect[AnimationType_Attack1][1] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);
    Entity->AnimationSoundEffect[AnimationType_Attack1][2] =
        GetSoundEffectForType(Assets, SoundEffect_BeastPossesedAttack);

    // NOTE(paul): Impact
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_0);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_1);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_2);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddGoblinBerserker(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_GoblinBerserker, true,
                                         WorldMode->MonsterCollision);

    Entity->RenderHeight = 2.2f;
    Entity->HealthMax_Health = (u32)((80 << 16) | 80);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->AttackSpriteFinishIndex[AttackType_0] = 8;
    Entity->GeneralType = GeneralType_Enemy;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_GoblinBerserker;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    // NOTE(paul): Attack
    Entity->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_2);

    // NOTE(paul): Impact
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_0);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_1);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_2);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddGoblinRider(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_GoblinRider, true,
                                         WorldMode->MonsterCollision);

    Entity->RenderHeight = 3.0f;
    Entity->HealthMax_Health = (u32)((70 << 16) | 70);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->AttackSpriteFinishIndex[AttackType_0] = 4;
    Entity->AttackSpriteFinishIndex[AttackType_1] = 3;
    Entity->AttackSpriteFinishIndex[AttackType_2] = 4;
    Entity->GeneralType = GeneralType_Enemy;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_GoblinRider;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable);

    // NOTE(paul): Attack
    Entity->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_SmallMonsterAttack);
    Entity->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_SmallMonsterAttack);
    Entity->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_SmallMonsterAttack);

    Entity->AnimationSoundEffect[AnimationType_Attack1][0] =
        GetSoundEffectForType(Assets, SoundEffect_Whoosh);
    Entity->AnimationSoundEffect[AnimationType_Attack1][1] =
        GetSoundEffectForType(Assets, SoundEffect_Whoosh);
    Entity->AnimationSoundEffect[AnimationType_Attack1][2] =
        GetSoundEffectForType(Assets, SoundEffect_Whoosh);

    Entity->AnimationSoundEffect[AnimationType_Attack2][0] =
        GetSoundEffectForType(Assets, SoundEffect_BowAttack, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Attack2][1] =
        GetSoundEffectForType(Assets, SoundEffect_BowAttack, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Attack2][2] =
        GetSoundEffectForType(Assets, SoundEffect_BowAttack, VarietyType_0);

    // NOTE(paul): Impact
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_0);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_1);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_Hit, VarietyType_2);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Whoosh);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Whoosh);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Whoosh);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddSkeletonGrunt(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_SkeletonGrunt, true,
                                         WorldMode->MonsterCollision);

    Entity->RenderHeight = 2.2f;
    Entity->HealthMax_Health = (u32)((50 << 16) | 50);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->AttackSpriteFinishIndex[AttackType_0] = 7;
    Entity->GeneralType = GeneralType_Enemy;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_SkeletonWithSword;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    // NOTE(paul): Attack
    Entity->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_2);

    // NOTE(paul): Impact
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_0);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_1);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_2);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddArrowProjectile(game_mode_world *WorldMode, game_assets *Assets, casted_spell Spell)
{
    entity *Entity = BeginEntity(WorldMode, EntityType_FlyingSpell, true);
    Entity->RenderHeight = Spell.RenderHeight;
    Entity->AnimationTypeHaveChanged = true;
    Entity->Collision = WorldMode->SpellCollision;
    Entity->DistanceLimit = Spell.Distance;
    Entity->dP = Spell.dP;
    Entity->GeneralType = GeneralType_Spell;

    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable);

    flyingspell_entity *Data = (flyingspell_entity *)Entity->Data;
    Data->Type = Spell.Type;
    Data->Effect = Spell.Effect;
    Data->Damage = Spell.Damage;
    Data->Direction = Spell.Direction;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;
    WeightVector.E[Tag_SpellName] = 1.0f;
    WeightVector.E[Tag_MagicElement] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Arrow;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    entity_id Result = Entity->ID;
    
    world_position Pos = MapIntoTileSpace(WorldMode->World, Spell.BaseP, Spell.OffsetP.xy);
    EndEntity(WorldMode, Entity, Pos);

    return(Result);
}

internal entity_id
AddSkeletonHunter(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_SkeletonHunter, true,
                                         WorldMode->MonsterCollision);

    Entity->RenderHeight = 2.2f;
    Entity->HealthMax_Health = (u32)((50 << 16) | 50);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->GeneralType = GeneralType_Enemy;

    Entity->AttackSpriteFinishIndex[AttackType_0] = 9;
    AddTimerForAttack(Entity, 2.0f, AttackType_0);
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_SkeletonWithBow;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    // NOTE(paul): Attack
    Entity->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_BowAttack, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_BowAttack, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_BowAttack, VarietyType_0);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddSkeletonKing(game_mode_world *WorldMode, game_assets *Assets, uint32 AbsTileX, uint32 AbsTileY)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY);
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_SkeletonKing, true,
                                         WorldMode->MonsterCollision);

    Entity->RenderHeight = 6.0f;
    Entity->HealthMax_Health = (u32)((120 << 16) | 120);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->GeneralType = GeneralType_Enemy;

    Entity->AttackSpriteFinishIndex[AttackType_0] = 4;
    Entity->AttackSpriteFinishIndex[AttackType_1] = 5;
    Entity->CastSpellSpriteFinishIndex[CastSpellType_0] = 10;
    AddTimerForAttack(Entity, 2.0f, AttackType_0);
    AddTimerForCast(Entity, 10.0f, CastSpellType_0);
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_SkeletonKing;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    Entity->SpriteSheetSpeed[AnimationType_Attack0][0] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][1] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][2] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][3] = 10;

    Entity->SpriteSheetSpeed[AnimationType_Attack1][0] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack1][1] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack1][2] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack1][3] = 10;
    
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    // NOTE(paul): Attack
    Entity->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_2);

    // NOTE(paul): Impact
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_0);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_1);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_2);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);

    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);

    return(Result);
}

internal entity_id
AddFlyingSpell(game_mode_world *WorldMode, game_assets *Assets, casted_spell Spell)
{
    entity *Entity = BeginEntity(WorldMode, EntityType_FlyingSpell, true);
    Entity->RenderHeight = Spell.RenderHeight;
    Entity->AnimationTypeHaveChanged = true;
    Entity->Collision = WorldMode->SpellCollision;
    Entity->DistanceLimit = Spell.Distance;
    Entity->dP = Spell.dP;
    Entity->GeneralType = GeneralType_Spell;

    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable);

    flyingspell_entity *Data = (flyingspell_entity *)Entity->Data;
    Data->Type = Spell.Type;
    Data->Effect = Spell.Effect;
    Data->Damage = Spell.Damage;
    Data->Direction = Spell.Direction;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;
    WeightVector.E[Tag_SpellName] = 1.0f;
    WeightVector.E[Tag_MagicElement] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Spell;
    MatchVector.E[Tag_SpellName] = Spell.SpellName;
    MatchVector.E[Tag_MagicElement] = Spell.MagicElement;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    Entity->SpriteSheetSpeed[AnimationType_Walk][0] = Spell.ProjectileAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Walk][1] = Spell.ProjectileAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Walk][2] = Spell.ProjectileAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Walk][3] = Spell.ProjectileAnimationSpeed;

    Entity->SpriteSheetSpeed[AnimationType_Death][0] = Spell.DeathAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Death][1] = Spell.DeathAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Death][2] = Spell.DeathAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Death][3] = Spell.DeathAnimationSpeed;

    Entity->AnimationSoundEffect[AnimationType_Death][0] = Spell.ImpactEffect;
    Entity->AnimationSoundEffect[AnimationType_Death][1] = Spell.ImpactEffect;
    Entity->AnimationSoundEffect[AnimationType_Death][2] = Spell.ImpactEffect;

    entity_id Result = Entity->ID;
    
    world_position Pos = MapIntoTileSpace(WorldMode->World, Spell.BaseP, Spell.OffsetP.xy);
    EndEntity(WorldMode, Entity, Pos);

    return(Result);
}

internal entity_id
AddImmidiateSpell(game_mode_world *WorldMode, game_assets *Assets, casted_spell Spell)
{
    entity *Entity = BeginEntity(WorldMode, EntityType_ImmidiateSpell, true);
    Entity->Collision = WorldMode->SpellCollision;
    Entity->RenderHeight = Spell.RenderHeight;
    Entity->AnimationTypeHaveChanged = true;
    Entity->dP = Spell.dP;
    Entity->AttackSpriteFinishIndex[0] = Spell.ImmidiateAnimationFinishIndex; //6
    Entity->GeneralType = GeneralType_Spell;

    immidiatespell_entity *Data = (immidiatespell_entity *)Entity->Data;
    Data->Type = Spell.Type;
    Data->Damage_Heal = Spell.Damage;
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;
    WeightVector.E[Tag_SpellName] = 1.0f;
    WeightVector.E[Tag_MagicElement] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Spell;
    MatchVector.E[Tag_SpellName] = Spell.SpellName;
    MatchVector.E[Tag_MagicElement] = Spell.MagicElement;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    Entity->SpriteSheetSpeed[AnimationType_Attack0][0] = Spell.ProjectileAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][1] = Spell.ProjectileAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][2] = Spell.ProjectileAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][3] = Spell.ProjectileAnimationSpeed;

    Entity->SpriteSheetSpeed[AnimationType_Death][0] = Spell.DeathAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Death][1] = Spell.DeathAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Death][2] = Spell.DeathAnimationSpeed;
    Entity->SpriteSheetSpeed[AnimationType_Death][3] = Spell.DeathAnimationSpeed;

    entity_id Result = Entity->ID;
    
    world_position Pos = MapIntoTileSpace(WorldMode->World, Spell.BaseP, Spell.OffsetP.xy);
    EndEntity(WorldMode, Entity, Pos);

    return(Result);
}

internal entity *
AddSphere(game_mode_world *WorldMode, v3 P, world_position BasePos)
{
    world_position Pos = MapIntoTileSpace(WorldMode->World, BasePos, P.xy);
    entity *Entity = BeginEntity(WorldMode, EntityType_MagicSphere, true);
    Entity->Collision = WorldMode->SphereCollision;
    Entity->RenderHeight = 0.7f;
    Entity->GeneralType = GeneralType_Object;

    hero_sphere_entity *Data = (hero_sphere_entity *)Entity->Data;
    Data->Type = SphereType_Null;
    Data->tMove = 0.0f;
    Data->CircleCenter = V3(0.0f, 0.0f, 0.0f);
    
    EndEntity(WorldMode, Entity, Pos);
        
    return(Entity);
}

inline void
AddHeroSpell(hero_spell *Spell, spell_type Type, r32 TimerDurationSeconds, effect SpellEffect,
             u32 ManaCost, u32 Damage, r32 Distance, u32 SpellName, u32 MagicElement, u32 ImAnimationFinishIndex,
             u8 ProjectileAnimationSpeed, u8 DeathAnimationSpeed, r32 RenderHeight,
             sound_id CastEffect, sound_id ImpactEffect)
{
    Spell->Type = Type;

    Spell->Timer.Finished = true;
    Spell->Timer.DurationSeconds = TimerDurationSeconds;
    Spell->Timer.CurrentTime = TimerDurationSeconds;

    Spell->Config.Type = Type;
    Spell->Config.SpellName = (r32)SpellName;
    Spell->Config.MagicElement = (r32)MagicElement;

    Spell->Config.Effect = SpellEffect;
    Spell->Config.ManaCost = ManaCost;
    Spell->Config.Damage = Damage;

    Spell->Config.OffsetP = V3(0, 0, 0);
    Spell->Config.BaseP = NullPosition();

    Spell->Config.ImmidiateAnimationFinishIndex = ImAnimationFinishIndex;
    Spell->Config.ProjectileAnimationSpeed = ProjectileAnimationSpeed;
    Spell->Config.DeathAnimationSpeed = DeathAnimationSpeed;
    
    Spell->Config.Distance = Distance;
    Spell->Config.Direction = V3(0, 0, 0);
    Spell->Config.dP = V3(0, 0, 0);

    Spell->Config.RenderHeight = RenderHeight;
    Spell->Config.CastSpellEffect = CastEffect;
    Spell->Config.ImpactEffect = ImpactEffect;
}

internal entity_id
AddPlayer(game_mode_world *WorldMode, game_assets *Assets)
{
    world_position P = WorldMode->CameraP;
    entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Hero, true,
                                             WorldMode->PlayerCollision);
    AddFlags(Entity, EntityFlag_Collides|EntityFlag_Moveable|EntityFlag_ZSupported);

    hero_entity *Data = (hero_entity *)Entity->Data;

    Entity->RenderHeight = 2.2f;
    Entity->RefCount = 0;
    Entity->HealthMax_Health = (u32)((100 << 16) | 100);
    Entity->ManaMax_Mana = (u32)((150 << 16) | 150);
    Entity->AnimationTypeHaveChanged = true;
    Entity->SpriteSheetOffset = 0;
    Entity->AttackSpriteFinishIndex[AttackType_0] = 4;
    Entity->CastSpellSpriteFinishIndex[CastSpellType_0] = 9;
    Entity->GeneralType = GeneralType_Hero;

    asset_vector SphereMatchVector = {};
    asset_vector SphereWeightVector = {};
    SphereWeightVector.E[Tag_MagicElement] = 1.0f;
    
    u32 MagicElements[3] = {MagicElement_Water, MagicElement_Wind, MagicElement_Fire};
    real32 CircleOffset = (2.0f*Pi32) / 3.0f;
    real32 Radius = 0.5f;
    v3 OffsetP = V3(0.0f, 1.0f, 0.0f);

    for(uint32 SphereIndex = 0;
        SphereIndex < 3;
        ++SphereIndex)
    {
        SphereMatchVector.E[Tag_MagicElement] = (r32)MagicElements[SphereIndex];
        bitmap_id SphereBitmapID = GetBestMatchBitmapFrom(Assets, Asset_MagicSphere,
                                                          &SphereMatchVector, &SphereWeightVector);
        
        real32 tMove = CircleOffset * SphereIndex;
        v3 Pos = V3((Entity->P.x + Radius*Cos(tMove)),
                    (Entity->P.y + Radius*Sin(tMove)),
                    0.0f);

        entity *Sphere = AddSphere(WorldMode, Pos + OffsetP, P);
        Entity->References[Entity->RefCount].ID = Sphere->ID;
        Data->SpheresRefIndex[SphereIndex] = Entity->RefCount;
        ++Entity->RefCount;

        hero_sphere_entity *SphereData = (hero_sphere_entity *)Sphere->Data;
        SphereData->CircleCenter = Entity->P + OffsetP;
        SphereData->Type = (sphere_type)(SphereIndex + 1);
        SphereData->tMove = tMove;

        Data->SphereBitmapIDs[SphereIndex] = SphereBitmapID;
        ++Data->Combination[SphereIndex];
    }
    
    Assert(Entity->RefCount < ArrayCount(Entity->References));
    
    asset_vector WeightVector = {};
    WeightVector.E[Tag_AssetType] = 1.0f;
    WeightVector.E[Tag_FacingDirection] = 1.0f;
    WeightVector.E[Tag_AnimationType] = 1.0f;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_AssetType] = (r32)Asset_Hero;

    AddEntitySpriteSheets(Assets, Entity, &MatchVector, &WeightVector);

    Entity->SpriteSheetSpeed[AnimationType_Attack0][0] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][1] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][2] = 10;
    Entity->SpriteSheetSpeed[AnimationType_Attack0][3] = 10;

    Entity->SpriteSheetSpeed[AnimationType_CastSpell0][0] = 16;
    Entity->SpriteSheetSpeed[AnimationType_CastSpell0][1] = 16;
    Entity->SpriteSheetSpeed[AnimationType_CastSpell0][2] = 16;
    Entity->SpriteSheetSpeed[AnimationType_CastSpell0][3] = 16;

    Entity->SpriteSheetSpeed[AnimationType_Walk][0] = 16;
    Entity->SpriteSheetSpeed[AnimationType_Walk][1] = 16;
    Entity->SpriteSheetSpeed[AnimationType_Walk][2] = 16;
    Entity->SpriteSheetSpeed[AnimationType_Walk][3] = 16;

    hero_spell *Spell = Data->Spells;
    sound_id NullSoundID = {};
    sound_id CastEffectID = GetSoundEffectForType(Assets, SoundEffect_HeroCastSpellDefault);
    sound_id ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_WaterImpact);

    AddHeroSpell((Spell + 0), SpellType_MagicSword,    5.0f, SpellEffect_Dark, 20, 0,  0.0f,
                 Spell_MagicSword, MagicElement_Dark, 0, 0, 0, 0.0f, CastEffectID, NullSoundID);

    AddHeroSpell((Spell + 1), SpellType_WaterBall,     2.0f, SpellEffect_Water, 15, 10, 10.0f,
                 Spell_WaterBall, MagicElement_Water, 0, 18, 18, 2.0f, CastEffectID, ImpactEffectID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_IceBallCast);
    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_IceImpact);
    AddHeroSpell((Spell + 2), SpellType_IceBall,       2.0f, SpellEffect_Ice, 25, 20, 10.0f,
                 Spell_IceBall, MagicElement_Ice, 0, 12, 12, 2.0f, CastEffectID, ImpactEffectID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_HealCast);
    AddHeroSpell((Spell + 3), SpellType_Heal,          3.0f, SpellEffect_Heal, 0, 20,  0.0f,
                 Spell_HealCross, MagicElement_Light, 4, 5, 10, 2.0f, CastEffectID, NullSoundID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_HeroCastSpellDefault);
    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_ThunderImpact);
    AddHeroSpell((Spell + 4), SpellType_LightBall,     2.0f, SpellEffect_Light, 20, 15, 10.0f,
                 Spell_LightBall, MagicElement_Light, 0, 8, 8, 1.0f, CastEffectID, ImpactEffectID);

    AddHeroSpell((Spell + 5), SpellType_IceSword,      5.0f, SpellEffect_Ice, 20, 0,  0.0f,
                 Spell_IceSword, MagicElement_Ice, 0, 0, 0, 0.0f, CastEffectID, NullSoundID);

    AddHeroSpell((Spell + 6), SpellType_FireSword,     5.0f, SpellEffect_Fire, 25, 0,  0.0f,
                 Spell_FireSword, MagicElement_Fire, 0, 0, 0, 0.0f, CastEffectID, NullSoundID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_FireBallCast);
    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_FireBallImpact);
    AddHeroSpell((Spell + 7), SpellType_FireBall,      3.0f, SpellEffect_Fire, 30, 25, 10.0f,
                 Spell_FireBall, MagicElement_Fire, 0, 12, 12, 2.5f, CastEffectID, ImpactEffectID);

    CastEffectID = GetSoundEffectForType(Assets, SoundEffect_HeroCastSpellDefault);
    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_EnergyImpact);
    AddHeroSpell((Spell + 8), SpellType_EnergyBall,    2.0f, SpellEffect_Energy, 15, 10,  10.0f,
                 Spell_EnergyBall, MagicElement_Energy, 0, 12, 12, 2.5f, CastEffectID, ImpactEffectID);

    ImpactEffectID = GetSoundEffectForType(Assets, SoundEffect_ThunderImpact);
    AddHeroSpell((Spell + 9), SpellType_BirdStrike,    3.0f, SpellEffect_Light, 35, 40, 12.0f,
                 Spell_BirdStrike, MagicElement_Light, 0, 10, 10, 2.0f, CastEffectID, ImpactEffectID);

    Data->SwordCharmTimer = {};
    Data->SwordCharmTimer.Finished = true;
    Data->SwordCharmTimer.DurationSeconds = 25.0f;
    Data->SwordCharmTimer.CurrentTime = 25.0f;
    Data->SwordType = SwordType_Null;
    Data->CurrentQuests[Data->QuestCount++] = QuestName_FindTavor;

    // NOTE(paul): Attack
    Entity->AnimationSoundEffect[AnimationType_Attack0][0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Attack0][1] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Attack0][2] =
        GetSoundEffectForType(Assets, SoundEffect_SwordAttack, VarietyType_2);

    // NOTE(paul): Impact
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_0);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_1);
    Entity->AttackImpactSound[0] =
        GetSoundEffectForType(Assets, SoundEffect_SwordImpact, VarietyType_2);

    // NOTE(paul): Walk
    Entity->AnimationSoundEffect[AnimationType_Walk][0] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_0);
    Entity->AnimationSoundEffect[AnimationType_Walk][1] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_1);
    Entity->AnimationSoundEffect[AnimationType_Walk][2] =
        GetSoundEffectForType(Assets, SoundEffect_Walk, VarietyType_2);
    
    if(WorldMode->CameraFollowingEntityIndex.Value == 0)
    {
        WorldMode->CameraFollowingEntityIndex = Entity->ID;
    }
    
    entity_id Result = Entity->ID;

    EndEntity(WorldMode, Entity, P);
    
    return(Result);
}

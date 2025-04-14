/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

inline void
RemoveQuest(game_mode_world *WorldMode, hero_entity *HeroData, u32 QuestID)
{
    for(u32 QuestIndex = 0;
        QuestIndex < ArrayCount(HeroData->CurrentQuests);
        ++QuestIndex)
    {
        u32 HeroQuestID = HeroData->CurrentQuests[QuestIndex];
        if(HeroQuestID == QuestID)
        {
            HeroData->CurrentQuests[QuestIndex] = 0;                        
//            quest *Quest = WorldMode->Quests + HeroData->CurrentQuests[QuestIndex];
            
            --HeroData->QuestCount;
            break;
        }
    }
}

internal void
GiveReward(game_mode_world *WorldMode, sim_region *SimRegion, quest *Quest, hero_entity *HeroData)
{
    for(u32 RewardIndex = 0;
        RewardIndex < Quest->RewardCount;
        ++RewardIndex)
    {
        switch(Quest->RewardType[RewardIndex])
        {
            case RewardType_Quest:
            {
                HeroData->CurrentQuests[HeroData->QuestCount++] = Quest->Reward[RewardIndex].QuestID;
                
                Quest->FullyComleted = true;
            } break;

            case RewardType_TalkingGiver:
            {
                entity *TalkingNPC = GetEntityByID(SimRegion, Quest->Reward[RewardIndex].TalkingGiverID);
                if(TalkingNPC)
                {
                    talkingnpc_entity *NPCData = (talkingnpc_entity *)TalkingNPC->Data;
                    NPCData->TalkingState = TalkingState_QuestGiver;
                
                    Quest->FullyComleted = true;
                }
            } break;

            case RewardType_DestroyObstacle:
            {
                for(u32 ObstacleIndex = 0;
                    ObstacleIndex < ArrayCount(Quest->Reward[RewardIndex].Obstacles);
                    ++ObstacleIndex)
                {
                    entity_id ID = Quest->Reward[RewardIndex].Obstacles[ObstacleIndex];
                    if(ID.Value)
                    {
                        for(u32 EntityIndex = 0;
                            EntityIndex < ArrayCount(WorldMode->EntitiesToDestroy);
                            ++EntityIndex)
                        {
                            if(WorldMode->EntitiesToDestroy[EntityIndex].Value == 0)
                            {
                                WorldMode->EntitiesToDestroy[EntityIndex] = ID;
                                break;
                            }
                        }
                    }
                }
                
                Quest->FullyComleted = true;
            } break;

            case RewardType_GameEnd:
            {
                WorldMode->GameFinished = true;
            } break;
            
            InvalidDefaultCase;
        }
    }

    if(Quest->FullyComleted)
    {
        RemoveQuest(WorldMode, HeroData, Quest->QuestName);
    }
}

internal b32
CheckQuestForComplition(game_mode_world *WorldMode, sim_region *SimRegion, quest *Quest)
{
    entity *HeroEntity = GetEntityByID(SimRegion, WorldMode->CameraFollowingEntityIndex);
    hero_entity *HeroData = (hero_entity *)HeroEntity->Data;

    for(u32 CompIndex = 0;
        CompIndex < Quest->RequirementsCount;
        ++CompIndex)
    {
        complition_requirements *CompRequirement = Quest->CompRequirements + CompIndex; 
        switch(Quest->ComplitionType[CompIndex])
        {
            case ComplitionType_Kill:
            {
                kill_monsters *KillMonsters = &CompRequirement->KillMonsters;
                if(KillMonsters->MonsterCount)
                {
                    for(u32 MonsterIndex = 0;
                        MonsterIndex < ArrayCount(KillMonsters->MonstersToKill);
                        ++MonsterIndex)
                    {
                        entity_id MonsterID = KillMonsters->MonstersToKill[MonsterIndex];
                        if(MonsterID.Value)
                        {
                            entity *MonsterEntity = GetEntityByID(SimRegion, MonsterID);
                            if(MonsterEntity)
                            {
                                if(IsSet(MonsterEntity, EntityFlag_Deleted))
                                {
                                    --KillMonsters->MonsterCount;                    
                                }
                            }
                        }
                    }
                }
                else
                {
                    Quest->IsCompleted[CompIndex] = true;
                }
            } break;

            case ComplitionType_Find:
            {
                find_item *FindItem = &CompRequirement->FindItem;
                for(u32 ItemIndex = 0;
                    ItemIndex < HeroData->ItemCount;
                    ++ItemIndex)
                {
                    u32 ItemName = HeroData->Inventory[ItemIndex];
                    if(FindItem->Name == ItemName)
                    {
                        Quest->IsCompleted[CompIndex] = true;
                    }
                }
            } break;

            case ComplitionType_Talk:
            {
                talk_to_npc *TalkToNPC = &CompRequirement->TalkToNPC;
                entity *NPCEntity = GetEntityByID(SimRegion, TalkToNPC->NPCToTalk);
                if(NPCEntity)
                {
                    talkingnpc_entity *NPCData = (talkingnpc_entity *)NPCEntity->Data;
                    found_entity ClosestNPC = FindClosestEntityOfType(SimRegion, HeroEntity, EntityType_NPC, 12.0f);
                    if(ClosestNPC.Entity)
                    {
                        if(ClosestNPC.Entity->ID.Value == TalkToNPC->NPCToTalk.Value)
                        {
                            Quest->IsCompleted[CompIndex] = true;
                        }
                    }
                }
            } break;

            case ComplitionType_Quest:
            {
                finished_quest *FinishedQuest = &CompRequirement->FinishedQuest;

                quest *CheckQuest = WorldMode->Quests + FinishedQuest->QuestID;
                if(CheckQuest->FullyComleted)
                {
                    Quest->IsCompleted[CompIndex] = true;
                }
            } break;

            InvalidDefaultCase;
        }
    }

    b32 Result = true;
    Quest->Completed = true;
    for(u32 CompIndex = 0;
        CompIndex < Quest->RequirementsCount;
        ++CompIndex)
    {
        if(!Quest->IsCompleted[CompIndex])
        {
            Result = false;
            Quest->Completed = false;
            ZeroArray(8, Quest->IsCompleted);
            break;
        }
    }

    return(Result);
}

internal void
DrawHeroQuests(game_mode_world *WorldMode, sim_region *SimRegion, entity *HeroEntity,
               render_group *RenderGroup, text_config TextConfig)
{
    hero_entity *HeroData = (hero_entity *)HeroEntity->Data;

    TextConfig.TextTransform.OffsetP = V3(10.7f, 2.7f, 0);
    TextConfig.TextShadowTransform.OffsetP = V3(8.75f, 4.65f, 0);
    TextConfig.FontScale = 0.01;
    TextConfig.Color = V4(1, 1, 1, 1);
    
    if(HeroData->QuestCount)
    {
        for(u32 QuestIndex = 0;
            QuestIndex < ArrayCount(HeroData->CurrentQuests);
            ++QuestIndex)
        {
            u32 HeroQuestIndex = HeroData->CurrentQuests[QuestIndex];
            if(HeroQuestIndex)
            {
                quest *Quest = WorldMode->Quests + HeroQuestIndex;

                char *Buffer = (char *)QuestNames[Quest->QuestName];
                TextOutAt(RenderGroup, TextConfig, Buffer, 0);
                TextConfig.TextTransform.OffsetP.y -=
                    GetLineAdvance(TextConfig.FontInfo, TextConfig.FontScale) + 0.2f;
                TextConfig.TextShadowTransform.OffsetP.y -=
                    GetLineAdvance(TextConfig.FontInfo, TextConfig.FontScale) + 0.2f;

                if(Quest->Completed)
                {
                    TextOutAt(RenderGroup, TextConfig, Quest->CompletedText, 0);
                }
                else
                {
                    TextOutAt(RenderGroup, TextConfig, Quest->UnCompletedText, 0);
                }

                TextConfig.TextTransform.OffsetP.y -=
                    2.0f*GetLineAdvance(TextConfig.FontInfo, TextConfig.FontScale);
                TextConfig.TextShadowTransform.OffsetP.y -=
                    2.0f*GetLineAdvance(TextConfig.FontInfo, TextConfig.FontScale);
            }
        }
    }
}

internal void
AdvanceParagraphIndexForNPC(game_mode_world *WorldMode, sim_region *SimRegion, ssa_quest *QuestTextInfo,
                            talkingnpc_entity *NPCData, entity *HeroEntity)
{
    hero_entity *HeroData = (hero_entity *)HeroEntity->Data;
    u32 TextCount = 1;
    if(QuestTextInfo)
    {
        TextCount = (QuestTextInfo->GiverParCount +
                     QuestTextInfo->ObjectiveParCount +
                     QuestTextInfo->ComplitionParCount);
    }

    Assert(NPCData->ParagraphIndex < TextCount);
    switch(NPCData->TalkingState)
    {
        case TalkingState_General:
        {
            ++NPCData->ParagraphIndex;
            if(NPCData->ParagraphIndex >= TextCount)
            {
                // NOTE(paul): Advance to objective talking
                NPCData->TalkingState = TalkingState_General; 
                NPCData->ParagraphIndex = 0;
                WorldMode->UpdateMode = UpdateMode_Entities;
            }

        } break;

        case TalkingState_QuestGiver:
        {
            ++NPCData->ParagraphIndex;
            if(NPCData->ParagraphIndex >= QuestTextInfo->GiverParCount)
            {
                // NOTE(paul): Advance to objective talking
                NPCData->TalkingState = TalkingState_QuestObjective; 
                NPCData->ParagraphIndex = 0;

                HeroData->CurrentQuests[HeroData->QuestCount++] = NPCData->QuestID;
                NPCData->GiverTalkCompleted = true;
            }

        } break;

        case TalkingState_QuestObjective:
        {
            ++NPCData->ParagraphIndex;
            if(NPCData->ParagraphIndex >= QuestTextInfo->ObjectiveParCount)
            {
                if(NPCData->GiverTalkCompleted)
                {
                    NPCData->ObjectiveTalkCompleted = true;
                    WorldMode->UpdateMode = UpdateMode_Entities;
                    NPCData->TalkingState = TalkingState_QuestObjective; 
                    NPCData->ParagraphIndex = 0;
                }
            }

        } break;

        case TalkingState_QuestComleted:
        {
            ++NPCData->ParagraphIndex;
            if(NPCData->ParagraphIndex >= QuestTextInfo->ComplitionParCount)
            {
                // NOTE(paul): Mark quest as complete and clear it from hero quests
                WorldMode->UpdateMode = UpdateMode_Entities;
                NPCData->TalkingState = TalkingState_General;
                NPCData->ParagraphIndex = 0;

                quest *Quest = WorldMode->Quests + NPCData->QuestID;
                GiveReward(WorldMode, SimRegion, Quest, HeroData);
            }

        } break;

        InvalidDefaultCase;
    }
}

internal void
UpdateQuests(game_mode_world *WorldMode, sim_region *SimRegion, entity *HeroEntity)
{
    hero_entity *HeroData = (hero_entity *)HeroEntity->Data;
    for(u32 QuestIndex = 0;
        QuestIndex < ArrayCount(WorldMode->Quests);
        ++QuestIndex)
    {
        quest *Quest = WorldMode->Quests + QuestIndex;
        if((Quest->Type != QuestType_None) && (!Quest->FullyComleted))
        {
            if(CheckQuestForComplition(WorldMode, SimRegion, Quest) &&
               (Quest->RewardCondition != RewardCondition_TalkToGiver))
            {
                GiveReward(WorldMode, SimRegion, Quest, HeroData);
            }
            else if(CheckQuestForComplition(WorldMode, SimRegion, Quest) &&
                    (Quest->RewardCondition == RewardCondition_TalkToGiver))
            {
                Quest->Location = Quest->GiverLocation;
                entity *GiverNPC = GetEntityByID(SimRegion, Quest->QuestGiverNPC);
                if(GiverNPC)
                {
                    talkingnpc_entity *NPCData = (talkingnpc_entity *)GiverNPC->Data;
                    if(NPCData->GiverTalkCompleted &&
                       NPCData->ObjectiveTalkCompleted)
                    {
                        NPCData->TalkingState = TalkingState_QuestComleted;
                    }
                }
            }
        }
    }
}



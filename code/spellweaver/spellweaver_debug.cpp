/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

// TODO(casey): Stop using stdio!
#include <stdio.h>
#include <stdlib.h>

#include "spellweaver_debug.h"
#include "spellweaver_debug_ui.cpp"

struct debug_parsed_name
{
    u32 HashValue;
    u32 FileNameCount;
    u32 NameStartsAt;
    u32 LineNumber;

    u32 NameLength;
    char *Name;
};
inline debug_parsed_name
DebugParseName(char *GUID)
{
    debug_parsed_name Result = {};

    u32 PipeCount = 0;
    char *Scan = GUID;
    for(;
        *Scan;
        ++Scan)
    {
        if(*Scan == '|')
        {
            if(PipeCount == 0)
            {
                Result.FileNameCount = (u32)(Scan - GUID);
                Result.LineNumber = atoi(Scan + 1);
            }
            else if(PipeCount == 1)
            {

            }
            else
            {
                Result.NameStartsAt = (u32)(Scan - GUID + 1);
            }

            ++PipeCount;
        }

        // TODO(casey): Better hash function
        Result.HashValue = 65599*Result.HashValue + *Scan;
    }

    Result.NameLength = (u32)(Scan - GUID) - Result.NameStartsAt;
    Result.Name = GUID + Result.NameStartsAt;

    return(Result);
}

inline debug_element *
GetElementFromGUID(debug_state *DebugState, u32 Index, char *GUID)
{
    debug_element *Result = 0;

    for(debug_element *Chain = DebugState->ElementHash[Index];
        Chain;
        Chain = Chain->NextInHash)
    {
        if(StringsAreEqual(Chain->GUID, GUID))
        {
            Result = Chain;
            break;
        }
    }

    return(Result);
}

inline debug_element *
GetElementFromGUID(debug_state *DebugState, char *GUID)
{
    debug_element *Result = 0;

    if(GUID)
    {
        debug_parsed_name ParsedName = DebugParseName(GUID);
        u32 Index = (ParsedName.HashValue % ArrayCount(DebugState->ElementHash));

        Result = GetElementFromGUID(DebugState, Index, GUID);
    }

    return(Result);
}

inline debug_id
DebugIDFromLink(debug_tree *Tree, debug_variable_link *Link)
{
    debug_id Result = {};

    Result.Value[0] = Tree;
    Result.Value[1] = Link;

    return(Result);
}

inline debug_id
DebugIDFromGUID(debug_tree *Tree, char *GUID)
{
    debug_id Result = {};

    Result.Value[0] = Tree;
    Result.Value[1] = GUID;

    return(Result);
}

inline debug_state *
DEBUGGetState(game_memory *Memory)
{
    debug_state *DebugState = 0;
    if(Memory)
    {
        DebugState = (debug_state *)Memory->DebugStorage;
        if(!DebugState->Initialized)
        {
            DebugState = 0;
        }
    }

    return(DebugState);
}

inline debug_state *
DEBUGGetState(void)
{
    debug_state *Result = DEBUGGetState(DebugGlobalMemory);

    return(Result);
}

internal debug_tree *
AddTree(debug_state *DebugState, debug_variable_link *Group, v2 AtP)
{
    debug_tree *Tree = PushStruct(&DebugState->DebugArena, debug_tree);

    Tree->UIP = AtP;
    Tree->Group = Group;

    DLIST_INSERT(&DebugState->TreeSentinel, Tree);

    return(Tree);
}

inline void
BeginDebugStatistic(debug_statistic *Stat)
{
    Stat->Min = Real32Maximum;
    Stat->Max = -Real32Maximum;
    Stat->Sum = 0.0f;
    Stat->Count = 0;
}

inline void
AccumDebugStatistic(debug_statistic *Stat, r64 Value)
{
    ++Stat->Count;

    if(Stat->Min > Value)
    {
        Stat->Min = Value;
    }

    if(Stat->Max < Value)
    {
        Stat->Max = Value;
    }

    Stat->Sum += Value;
}

inline void
EndDebugStatistic(debug_statistic *Stat)
{
    if(Stat->Count)
    {
        Stat->Avg = Stat->Sum / (r64)Stat->Count;
    }
    else
    {
        Stat->Min = 0.0f;
        Stat->Max = 0.0f;
        Stat->Avg = 0.0f;
    }
}

internal memory_index
DEBUGEventToText(char *Buffer, char *End, debug_element *Element, debug_event *Event, u32 Flags)
{
    char *At = Buffer;
    char *Name = Element->GUID;

    if(Flags & DEBUGVarToText_AddDebugUI)
    {
        At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                          "#define DEBUGUI_");
    }

    if(Flags & DEBUGVarToText_AddName)
    {
        char *UseName = Name;
        if(!(Flags & DEBUGVarToText_ShowEntireGUID))
        {
            for(char *Scan = Name;
                *Scan;
                ++Scan)
            {
                if((Scan[0] == '|') &&
                   (Scan[1] != 0))
                {
                    UseName = Scan + 1;
                }
            }
        }

        At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                          "%s%s ", UseName, (Flags & DEBUGVarToText_Colon) ? ":" : "");
    }

    if(Flags & DEBUGVarToText_AddValue)
    {
        switch(Event->Type)
        {
            case DebugType_r32:                
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "%f", Event->Value_r32);
                if(Flags & DEBUGVarToText_FloatSuffix)
                {
                    *At++ = 'f';
                }
            } break;

            case DebugType_b32:                
            {
                if(Flags & DEBUGVarToText_PrettyBools)
                {
                    At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                        "%s",
                        Event->Value_b32 ? "true" : "false");
                }
                else
                {
                    At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                        "%d", Event->Value_b32);
                }
            } break;

            case DebugType_s32:
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "%d", Event->Value_s32);
            } break;

            case DebugType_u32:   
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "%u", Event->Value_u32);
            } break;

            case DebugType_v2:
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "V2(%f, %f)",
                    Event->Value_v2.x, Event->Value_v2.y);
            } break;

            case DebugType_v3:
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "V3(%f, %f, %f)",
                    Event->Value_v3.x, Event->Value_v3.y, Event->Value_v3.z);
            } break;

            case DebugType_v4:
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "V4(%f, %f, %f, %f)",
                    Event->Value_v4.x, Event->Value_v4.y,
                    Event->Value_v4.z, Event->Value_v4.w);
            } break;

            case DebugType_rectangle2:
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "Rect2(%f, %f -> %f, %f)",
                    Event->Value_rectangle2.Min.x,
                    Event->Value_rectangle2.Min.y,
                    Event->Value_rectangle2.Max.x,
                    Event->Value_rectangle2.Max.y);
            } break;

            case DebugType_rectangle3:
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "Rect2(%f, %f, %f -> %f, %f, %f)",
                    Event->Value_rectangle3.Min.x,
                    Event->Value_rectangle3.Min.y,
                    Event->Value_rectangle3.Min.z,
                    Event->Value_rectangle3.Max.x,
                    Event->Value_rectangle3.Max.y,
                    Event->Value_rectangle3.Max.z);
            } break;

            case DebugType_bitmap_id:
            {
            } break;

            default:
            {
                At += _snprintf_s(At, (size_t)(End - At), (size_t)(End - At),
                    "UNHANDLED: %s", Event->GUID);
            } break;
        }
    }

    if(Flags & DEBUGVarToText_LineFeedEnd)
    {
        *At++ = '\n';
    }

    if(Flags & DEBUGVarToText_NullTerminator)
    {
        *At++ = 0;
    }

    return(At - Buffer);
}

internal debug_view *
GetOrCreateDebugViewFor(debug_state *DebugState, debug_id ID)
{
    // TODO(casey): Better hash function
    u32 HashIndex = ((U32FromPointer(ID.Value[0]) >> 2) + (U32FromPointer(ID.Value[1]) >> 2)) % ArrayCount(DebugState->ViewHash);
    debug_view **HashSlot = DebugState->ViewHash + HashIndex;

    debug_view *Result = 0;
    for(debug_view *Search = *HashSlot;
        Search;
        Search = Search->NextInHash)
    {
        if(DebugIDsAreEqual(Search->ID, ID))
        {
            Result = Search;
            break;
        }
    }

    if(!Result)
    {
        Result = PushStruct(&DebugState->DebugArena, debug_view);
        Result->ID = ID;
        Result->Type = DebugViewType_Unknown;
        Result->NextInHash = *HashSlot;
        *HashSlot = Result;
    }

    return(Result);
}

inline debug_interaction
ElementInteraction(debug_state *DebugState, debug_id DebugID, debug_interaction_type Type, debug_element *Element)
{    
    debug_interaction ItemInteraction = {};
    ItemInteraction.ID = DebugID;    
    ItemInteraction.Type = Type;
    ItemInteraction.Element = Element;

    return(ItemInteraction);
}

inline debug_interaction
DebugIDInteraction(debug_interaction_type Type, debug_id ID)
{
    debug_interaction ItemInteraction = {};
    ItemInteraction.ID = ID;
    ItemInteraction.Type = Type;

    return(ItemInteraction);
}

inline debug_interaction
DebugLinkInteraction(debug_interaction_type Type, debug_variable_link *Link)
{
    debug_interaction ItemInteraction = {};
    ItemInteraction.Link = Link;
    ItemInteraction.Type = Type;

    return(ItemInteraction);
}

internal b32
IsSelected(debug_state *DebugState, debug_id ID)
{
    b32 Result = false;

    for(u32 Index = 0;
        Index < DebugState->SelectedIDCount;
        ++Index)
    {
        if(DebugIDsAreEqual(ID, DebugState->SelectedID[Index]))
        {
            Result = true;
            break;
        }
    }

    return(Result);
}

internal void
ClearSelection(debug_state *DebugState)
{
    DebugState->SelectedIDCount = 0;
}

internal void
AddToSelection(debug_state *DebugState, debug_id ID)
{
    if((DebugState->SelectedIDCount < ArrayCount(DebugState->SelectedID)) &&
       !IsSelected(DebugState, ID))
    {
        DebugState->SelectedID[DebugState->SelectedIDCount++] = ID;
    }
}

internal void
DEBUG_HIT(debug_id ID, r32 ZValue)
{
    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        DebugState->NextHotInteraction = DebugIDInteraction(DebugInteraction_Select, ID);
    }
}

internal b32
DEBUG_HIGHLIGHTED(debug_id ID, v4 *Color)
{
    b32 Result = false;

    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        if(IsSelected(DebugState, ID))
        {
            *Color = V4(0, 1, 1, 1);
            Result = true;
        }

        if(DebugIDsAreEqual(DebugState->HotInteraction.ID, ID))
        {
            *Color = V4(1, 1, 0, 1);
            Result = true;
        }
    }

    return(Result);
}

internal b32
DEBUG_REQUESTED(debug_id ID)
{
    b32 Result = false;

    debug_state *DebugState = DEBUGGetState();
    if(DebugState)
    {
        Result = IsSelected(DebugState, ID)
            || DebugIDsAreEqual(DebugState->HotInteraction.ID, ID);
    }

    return(Result);
}

internal u64
GetTotalClocks(debug_element_frame *Frame)
{
    u64 Result = 0;
    for(debug_stored_event *Event = Frame->OldestEvent;
        Event;
        Event = Event->Next)
    {
        Result += Event->ProfileNode.Duration;
    }
    return(Result);
}

global_variable v3 DebugColorTable[] =
{
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 1, 0},
    {0, 1, 1},
    {1, 0, 1},
    {1, 0.5f, 0},
    {1, 0, 0.5f},
    {0.5f, 1, 0},
    {0, 1, 0.5f},
    {0.5f, 0, 1},
    //    {0, 0.5f, 1},
};

internal void
DrawProfileBars(debug_state *DebugState, debug_id GraphID, rectangle2 ProfileRect, v2 MouseP,
                debug_profile_node *RootNode, r32 LaneStride, r32 LaneHeight, u32 DepthRemaining)
{
    r32 FrameSpan = (r32)(RootNode->Duration);
    r32 PixelSpan = GetDim(ProfileRect).x;

    r32 BaseZ = 100.0f - 10.0f*(r32)DepthRemaining;
    
    r32 Scale = 0.0f;
    if(FrameSpan > 0)
    {
        Scale = PixelSpan / FrameSpan;
    }

    for(debug_stored_event *StoredEvent = RootNode->FirstChild;
        StoredEvent;
        StoredEvent = StoredEvent->ProfileNode.NextSameParent)
    {
        debug_profile_node *Node = &StoredEvent->ProfileNode;
        debug_element *Element = Node->Element;
        Assert(Element);

        v3 Color = DebugColorTable[U32FromPointer(Element->GUID)%ArrayCount(DebugColorTable)];
        r32 ThisMinX = ProfileRect.Min.x + Scale*(r32)(Node->ParentRelativeClock);
        r32 ThisMaxX = ThisMinX + Scale*(r32)(Node->Duration);

        u32 LaneIndex = Node->ThreadOrdinal;
        r32 LaneY = ProfileRect.Max.y - LaneStride*LaneIndex;
        rectangle2 RegionRect = RectMinMax(V2(ThisMinX, LaneY - LaneHeight),
                                           V2(ThisMaxX, LaneY));

        PushRect(&DebugState->RenderGroup, DebugState->UITransform, RegionRect,
            BaseZ, V4(Color, 1));
        PushRectOutline(&DebugState->RenderGroup, DebugState->UITransform, RegionRect,
            BaseZ+1.0f, V4(0,0,0, 1), 2.0f);

        // TODO(casey): Pull this out so all profilers share it.
        if(IsInRectangle(RegionRect, MouseP))
        {
            char TextBuffer[256];
            _snprintf_s(TextBuffer, sizeof(TextBuffer),
                "%s: %10llucy",
                Element->GUID, Node->Duration);
            AddTooltip(DebugState, TextBuffer);
            
            // TODO(casey): It would be better to generate a graph+element debug ID here!
            debug_view *View = GetOrCreateDebugViewFor(DebugState, GraphID);
            DebugState->NextHotInteraction = 
                SetPointerInteraction(GraphID, (void **)&View->ProfileGraph.GUID, Element->GUID);
        }

        if(DepthRemaining > 0)
        {
            DrawProfileBars(DebugState, GraphID, RegionRect, MouseP, Node, 0, LaneHeight/2,
                DepthRemaining - 1);
        }
    }
}

internal void
DrawArenaOccupancy(debug_state *DebugState, debug_id GraphID, rectangle2 FrameRect, v2 MouseP,
    debug_element *RootElement)
{
    debug_element_frame *RootFrame = RootElement->Frames + DebugState->ViewingFrameOrdinal;
    debug_stored_event *Event = RootFrame->OldestEvent;
    if(Event)
    {
        memory_arena *Arena = Event->Event.Value_memory_arena_p;
        
        r32 t = (r32)(((r64)Arena->Used) / ((r64)Arena->Size));
        r32 SplitPoint = Lerp(FrameRect.Min.x, t, FrameRect.Max.x);
        rectangle2 UsedRect = RectMinMax(V2(FrameRect.Min.x, FrameRect.Min.y),
                                           V2(SplitPoint, FrameRect.Max.y));
        rectangle2 UnusedRect = RectMinMax(V2(SplitPoint, FrameRect.Min.y),
                                           V2(FrameRect.Max.x, FrameRect.Max.y));

        PushRect(&DebugState->RenderGroup, DebugState->UITransform, UsedRect,
            0.0f, V4(1,0.5f,0, 1));
        PushRectOutline(&DebugState->RenderGroup, DebugState->UITransform, UsedRect,
            1.0f, V4(0,0,0, 1), 2.0f);
        
        PushRect(&DebugState->RenderGroup, DebugState->UITransform, UnusedRect,
            0.0f, V4(0,1,0, 1));
        PushRectOutline(&DebugState->RenderGroup, DebugState->UITransform, UnusedRect,
            1.0f, V4(0,0,0, 1), 2.0f);
    }
}

internal void
DrawProfileIn(debug_state *DebugState, debug_id GraphID, rectangle2 ProfileRect, v2 MouseP,
    debug_element *RootElement)
{
    object_transform NoTransform = DefaultFlatTransform();

    u32 LaneCount = DebugState->FrameBarLaneCount;
    r32 LaneHeight = 0.0f;
    if(LaneCount > 0)
    {
        LaneHeight = GetDim(ProfileRect).y / (r32)LaneCount;
    }            

    debug_element_frame *RootFrame = RootElement->Frames + DebugState->ViewingFrameOrdinal;
    r32 NextX = ProfileRect.Min.x;
    u64 TotalClock = GetTotalClocks(RootFrame);
    u64 RelativeClock = 0;
    for(debug_stored_event *Event = RootFrame->OldestEvent;
        Event;
        Event = Event->Next)
    {
        debug_profile_node *Node = &Event->ProfileNode;
        rectangle2 EventRect = ProfileRect;

        RelativeClock += Node->Duration;
        r32 t = (r32)((r64)RelativeClock / (r64)TotalClock);
        EventRect.Min.x = NextX;
        EventRect.Max.x = (1.0f - t)*ProfileRect.Min.x + t*ProfileRect.Max.x;
        NextX = EventRect.Max.x;

        DrawProfileBars(DebugState, GraphID, EventRect, MouseP, Node, LaneHeight, LaneHeight, 1);
    }
}

internal void
DrawFrameBars(debug_state *DebugState, debug_id GraphID, rectangle2 ProfileRect, v2 MouseP,
              debug_element *RootElement)
{
    u32 FrameCount = ArrayCount(RootElement->Frames);
    if(FrameCount > 0)
    {
        object_transform NoTransform = DefaultFlatTransform();

        r32 BarWidth = (GetDim(ProfileRect).x / (r32)FrameCount);
        r32 AtX = ProfileRect.Min.x;
        for(u32 FrameIndex = 0;
            FrameIndex < FrameCount;
            ++FrameIndex)
        {
            debug_stored_event *RootEvent = RootElement->Frames[FrameIndex].MostRecentEvent;
            if(RootEvent)
            {
                debug_profile_node *RootNode = &RootEvent->ProfileNode;
                r32 FrameSpan = (r32)(RootNode->Duration);
                r32 PixelSpan = GetDim(ProfileRect).y;
                r32 Scale = 0.0f;
                if(FrameSpan > 0)
                {
                    Scale = PixelSpan / FrameSpan;
                }
                
                b32 Highlight = (FrameIndex == DebugState->ViewingFrameOrdinal);
                r32 HighDim = Highlight ? 1.0f : 0.5f;
                
                for(debug_stored_event *StoredEvent = RootNode->FirstChild;
                    StoredEvent;
                    StoredEvent = StoredEvent->ProfileNode.NextSameParent)
                {
                    debug_profile_node *Node = &StoredEvent->ProfileNode;
                    debug_element *Element = Node->Element;
                    Assert(Element);

                    v3 Color = DebugColorTable[U32FromPointer(Element->GUID)%ArrayCount(DebugColorTable)];
                    r32 ThisMinY = ProfileRect.Min.y + Scale*(r32)(Node->ParentRelativeClock);
                    r32 ThisMaxY = ThisMinY + Scale*(r32)(Node->Duration);
                    
                    rectangle2 RegionRect = RectMinMax(V2(AtX, ThisMinY), V2(AtX + BarWidth, ThisMaxY));

                    PushRect(&DebugState->RenderGroup, DebugState->UITransform, RegionRect,
                        0.0f, V4(HighDim*Color, 1));
                    PushRectOutline(&DebugState->RenderGroup, DebugState->UITransform, RegionRect,
                        1.0f, V4(0, 0, 0, 1), 2.0f);

                    if(IsInRectangle(RegionRect, MouseP))
                    {
                        char TextBuffer[256];
                        _snprintf_s(TextBuffer, sizeof(TextBuffer),
                            "%s: %10llucy",
                            Element->GUID, Node->Duration);
                        AddTooltip(DebugState, TextBuffer);
                        
                        debug_view *View = GetOrCreateDebugViewFor(DebugState, GraphID);
                        DebugState->NextHotInteraction = 
                            SetPointerInteraction(GraphID, (void **)&View->ProfileGraph.GUID, Element->GUID);
                    }
                }

                AtX += BarWidth;
            }
        }
    }
}

struct debug_clock_entry
{
    debug_element *Element;
    debug_statistic Stats;
};
internal void
DrawTopClocksList(debug_state *DebugState, debug_id GraphID, rectangle2 ProfileRect, v2 MouseP,
    debug_element *RootElement)
{
    temporary_memory Temp = BeginTemporaryMemory(&DebugState->DebugArena);

    u32 LinkCount = 0;
    for(debug_variable_link *Link = GetSentinel(DebugState->ProfileGroup)->Next;
        Link != GetSentinel(DebugState->ProfileGroup);
        Link = Link->Next)
    {
        ++LinkCount;
    }
    
    debug_clock_entry *Entries = PushArray(Temp.Arena, LinkCount, debug_clock_entry, NoClear());
    sort_entry *SortA = PushArray(Temp.Arena, LinkCount, sort_entry, NoClear());
    sort_entry *SortB = PushArray(Temp.Arena, LinkCount, sort_entry, NoClear());
        
    r64 TotalTime = 0.0f;
    u32 Index = 0;
    for(debug_variable_link *Link = GetSentinel(DebugState->ProfileGroup)->Next;
        Link != GetSentinel(DebugState->ProfileGroup);
        Link = Link->Next, ++Index)
    {
        Assert(Link->FirstChild == GetSentinel(Link));
        
        debug_clock_entry *Entry = Entries + Index;
        sort_entry *Sort = SortA + Index;

        Entry->Element = Link->Element;
        debug_element *Element = Entry->Element;

        BeginDebugStatistic(&Entry->Stats);
        for(debug_stored_event *Event = Element->Frames[DebugState->ViewingFrameOrdinal].OldestEvent;
            Event;
            Event = Event->Next)
        {
            u64 ClocksWithChildren = Event->ProfileNode.Duration;
            u64 ClocksWithoutChildren = ClocksWithChildren - Event->ProfileNode.DurationOfChildren;
            AccumDebugStatistic(&Entry->Stats, (r64)ClocksWithoutChildren);
        }
        EndDebugStatistic(&Entry->Stats);
        TotalTime += Entry->Stats.Sum;
        
        Sort->SortKey = -(r32)Entry->Stats.Sum;
        Sort->Index = Index;
    }
    
    RadixSort(LinkCount, SortA, SortB);
    
    r64 PC = 0.0f;
    if(TotalTime > 0)
    {
        PC = 100.0f / TotalTime;
    }
    
    v2 At = V2(ProfileRect.Min.x, ProfileRect.Max.y - GetBaseline(DebugState));
    for(Index = 0;
        (Index < LinkCount);
        ++Index)
    {
        debug_clock_entry *Entry = Entries + SortA[Index].Index;
        debug_statistic *Stats = &Entry->Stats;
        debug_element *Element = Entry->Element;
        
        char TextBuffer[256];
        _snprintf_s(TextBuffer, sizeof(TextBuffer),
            "%10ucy %02.02f%% %4d %s",
            (u32)Stats->Sum,
            (PC*Stats->Sum),
            Stats->Count,
            Element->GUID + Element->NameStartsAt);
        TextOutAt(DebugState, At, TextBuffer);
        
        if(At.y < ProfileRect.Min.y)
        {
            break;
        }
        else
        {
            At.y -= GetLineAdvance(DebugState);    
        }
    }
    
    EndTemporaryMemory(Temp);
}

internal void
DrawFrameSlider(debug_state *DebugState, debug_id SliderID, rectangle2 TotalRect, v2 MouseP,
                debug_element *RootElement)
{
    u32 FrameCount = ArrayCount(RootElement->Frames);
    if(FrameCount > 0)
    {
        object_transform NoTransform = DefaultFlatTransform();
        PushRect(&DebugState->RenderGroup, DebugState->BackingTransform, TotalRect, 0.0f, V4(0, 0, 0, 0.25f));

        r32 BarWidth = (GetDim(TotalRect).x / (r32)FrameCount);
        r32 AtX = TotalRect.Min.x;
        r32 ThisMinY = TotalRect.Min.y;
        r32 ThisMaxY = TotalRect.Max.y;
        for(u32 FrameIndex = 0;
            FrameIndex < FrameCount;
            ++FrameIndex)
        {
            rectangle2 RegionRect = RectMinMax(V2(AtX, ThisMinY), V2(AtX + BarWidth, ThisMaxY));

            v4 HiColor = V4(1, 1, 1, 1);
            b32 Highlight = false;
            if(FrameIndex == DebugState->ViewingFrameOrdinal)
            {
                HiColor = V4(1, 1, 0, 1);
                Highlight = true;
            }

            if(FrameIndex == DebugState->MostRecentFrameOrdinal)
            {
                HiColor = V4(0, 1, 0, 1);
                Highlight = true;
            }

            if(FrameIndex == DebugState->CollationFrameOrdinal)
            {
                HiColor = V4(1, 0, 0, 1);
                Highlight = true;
            }

            if(FrameIndex == DebugState->OldestFrameOrdinal)
            {
                HiColor = V4(0, 0.5f, 0, 1);
                Highlight = true;
            }

            if(Highlight)
            {
                PushRect(&DebugState->RenderGroup, DebugState->UITransform, RegionRect,
                    0.0f, HiColor);
            }
            PushRectOutline(&DebugState->RenderGroup, DebugState->UITransform, RegionRect,
                1.0f, V4(0.5f,0.5f,0.5f, 1), 2.0f);

            if(IsInRectangle(RegionRect, MouseP))
            {
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer), "%u", FrameIndex);
                AddTooltip(DebugState, TextBuffer);
                
                DebugState->NextHotInteraction = 
                    SetUInt32Interaction(SliderID, &DebugState->ViewingFrameOrdinal, FrameIndex);
            }

            AtX += BarWidth;
        }
    }
}

internal void
DEBUGDrawElement(layout *Layout, debug_tree *Tree, debug_element *Element, debug_id DebugID,
                 u32 FrameOrdinal)
{
    object_transform NoTransform = DefaultFlatTransform();

    debug_state *DebugState = Layout->DebugState;
    render_group *RenderGroup = &DebugState->RenderGroup;

    debug_interaction ItemInteraction =
        ElementInteraction(DebugState, DebugID, DebugInteraction_AutoModifyVariable, Element);

    b32 IsHot = InteractionIsHot(DebugState, ItemInteraction);
    v4 ItemColor = IsHot ? V4(1, 1, 0, 1) : V4(1, 1, 1, 1);

    debug_stored_event *OldestStoredEvent = 
        Element->Frames[DebugState->ViewingFrameOrdinal].OldestEvent;

    debug_view *View = GetOrCreateDebugViewFor(DebugState, DebugID);
    switch(Element->Type)
    {
        case DebugType_bitmap_id:
        {
            debug_event *Event = OldestStoredEvent ? &OldestStoredEvent->Event : 0;
            loaded_bitmap *Bitmap = 0;
            r32 BitmapScale = View->InlineBlock.Dim.y;
            if(Event)
            {
                Bitmap = GetBitmap(RenderGroup->Assets, Event->Value_bitmap_id, RenderGroup->GenerationID);
                if(Bitmap)
                {
                    used_bitmap_dim Dim = GetBitmapDim(RenderGroup, NoTransform, Bitmap, BitmapScale, V3(0.0f, 0.0f, 0.0f), 1.0f);
                    View->InlineBlock.Dim.x = Dim.Size.x;
                }
            }

            layout_element LayEl = BeginElementRectangle(Layout, &View->InlineBlock.Dim);
            MakeElementSizable(&LayEl);
            DefaultInteraction(&LayEl, ItemInteraction);
            EndElement(&LayEl);
            PushRect(&DebugState->RenderGroup, DebugState->BackingTransform, LayEl.Bounds, 0.0f, V4(0, 0, 0, 1.0f));

            if(Bitmap)
            {
                PushBitmap(&DebugState->RenderGroup, DebugState->BackingTransform, Event->Value_bitmap_id, BitmapScale,
                    V3(GetMinCorner(LayEl.Bounds), 1.0f), V4(1, 1, 1, 1), 0.0f);
            }
        } break;

        case DebugType_memory_arena_p:
        case DebugType_ArenaOccupancy:
        {
            debug_view_arena_graph *Graph = &View->ArenaGraph;
            
            BeginRow(Layout);
            Label(Layout, GetName(Element));
            BooleanButton(Layout, "Occupancy", (Element->Type == DebugType_ArenaOccupancy),
                SetUInt32Interaction(DebugID, (u32 *)&Element->Type, DebugType_ArenaOccupancy));
            EndRow(Layout);

            layout_element LayEl = BeginElementRectangle(Layout, &Graph->Block.Dim);
            if((Graph->Block.Dim.x == 0) && (Graph->Block.Dim.y == 0))
            {
                Graph->Block.Dim.x = 1400;
                Graph->Block.Dim.y = 280;
            }

            MakeElementSizable(&LayEl);
            //                DefaultInteraction(&LayEl, ItemInteraction);
            EndElement(&LayEl);

            PushRect(&DebugState->RenderGroup, DebugState->BackingTransform,
                LayEl.Bounds, 0.0f, V4(0, 0, 0, 0.75f));
            
            u32 OldClipRect = RenderGroup->CurrentClipRectIndex;
            RenderGroup->CurrentClipRectIndex = 
                PushClipRect(RenderGroup, DebugState->BackingTransform, LayEl.Bounds, 0.0f);
                
            switch(Element->Type)
            {
                case DebugType_ArenaOccupancy:
                {
                    DrawArenaOccupancy(DebugState, DebugID, LayEl.Bounds, Layout->MouseP, Element);
                } break;
            }
            
            RenderGroup->CurrentClipRectIndex = OldClipRect;
        } break;

        case DebugType_ThreadIntervalGraph:
        case DebugType_FrameBarGraph:
        case DebugType_TopClocksList:
        {
            debug_view_profile_graph *Graph = &View->ProfileGraph;

            BeginRow(Layout);
            ActionButton(Layout, "Root", SetPointerInteraction(DebugID, (void **)&Graph->GUID, 0));
            BooleanButton(Layout, "Threads", (Element->Type == DebugType_ThreadIntervalGraph),
                SetUInt32Interaction(DebugID, (u32 *)&Element->Type, DebugType_ThreadIntervalGraph));
            BooleanButton(Layout, "Frames", (Element->Type == DebugType_FrameBarGraph),
                SetUInt32Interaction(DebugID, (u32 *)&Element->Type, DebugType_FrameBarGraph));
            BooleanButton(Layout, "Clocks", (Element->Type == DebugType_TopClocksList),
                SetUInt32Interaction(DebugID, (u32 *)&Element->Type, DebugType_TopClocksList));
            EndRow(Layout);

            layout_element LayEl = BeginElementRectangle(Layout, &Graph->Block.Dim);
            if((Graph->Block.Dim.x == 0) && (Graph->Block.Dim.y == 0))
            {
                Graph->Block.Dim.x = 1400;
                Graph->Block.Dim.y = 280;
            }

            MakeElementSizable(&LayEl);
            //                DefaultInteraction(&LayEl, ItemInteraction);
            EndElement(&LayEl);

            PushRect(&DebugState->RenderGroup, DebugState->BackingTransform,
                LayEl.Bounds, 0.0f, V4(0, 0, 0, 0.75f));
            
            u32 OldClipRect = RenderGroup->CurrentClipRectIndex;
            RenderGroup->CurrentClipRectIndex = 
                PushClipRect(RenderGroup, DebugState->BackingTransform, LayEl.Bounds, 0.0f);
                
            debug_stored_event *RootNode = 0;

            u32 ViewingFrameOrdinal = DebugState->ViewingFrameOrdinal;
            debug_element *ViewingElement = GetElementFromGUID(DebugState, View->ProfileGraph.GUID);
            if(!ViewingElement)
            {
                ViewingElement = DebugState->RootProfileElement;
            }

            switch(Element->Type)
            {
                case DebugType_ThreadIntervalGraph:
                {
                    DrawProfileIn(DebugState, DebugID, LayEl.Bounds, Layout->MouseP, ViewingElement);
                } break;

                case DebugType_FrameBarGraph:
                {
                    DrawFrameBars(DebugState, DebugID, LayEl.Bounds, Layout->MouseP, ViewingElement);
                } break;
                
                case DebugType_TopClocksList:
                {
                    DrawTopClocksList(DebugState, DebugID, LayEl.Bounds, Layout->MouseP, ViewingElement);
                } break;
            }
            
            RenderGroup->CurrentClipRectIndex = OldClipRect;
        } break;

        case DebugType_FrameSlider:
        {
            v2 *Dim = &View->InlineBlock.Dim;
            if((Dim->x == 0) && (Dim->y == 0))
            {
                Dim->x = 1400;
                Dim->y = 32;
            }
            
            layout_element LayEl = BeginElementRectangle(Layout, Dim);
            MakeElementSizable(&LayEl);
            EndElement(&LayEl);

            BeginRow(Layout);
            BooleanButton(Layout, "Pause", DebugState->Paused,
                SetUInt32Interaction(DebugID, (u32 *)&DebugState->Paused, !DebugState->Paused));
            ActionButton(Layout, "Oldest", 
                SetUInt32Interaction(DebugID, &DebugState->ViewingFrameOrdinal,
                    DebugState->OldestFrameOrdinal));
            ActionButton(Layout, "Most Recent", 
                SetUInt32Interaction(DebugID, &DebugState->ViewingFrameOrdinal,
                    DebugState->MostRecentFrameOrdinal));
            EndRow(Layout);

            DrawFrameSlider(DebugState, DebugID, LayEl.Bounds, Layout->MouseP, Element);
        } break;

        case DebugType_LastFrameInfo:
        {
            char Text[256];
            
            debug_frame *MostRecentFrame = DebugState->Frames + DebugState->ViewingFrameOrdinal;
            _snprintf_s(Text, sizeof(Text),
                "Viewing frame time: %.02fms %de %dp %dd",
                MostRecentFrame->WallSecondsElapsed * 1000.0f,
                MostRecentFrame->StoredEventCount,
                MostRecentFrame->ProfileBlockCount,
                MostRecentFrame->DataBlockCount);

            BasicTextElement(Layout, Text, ItemInteraction);
        } break;

        case DebugType_DebugMemoryInfo:
        {
            char Text[256];
            _snprintf_s(Text, sizeof(Text),
                "Per-frame arena space remaining: %ukb",
                (u32)(GetArenaSizeRemaining(&DebugState->PerFrameArena, AlignNoClear(1)) / 1024));

            BasicTextElement(Layout, Text, ItemInteraction);
        } break;

        default:
        {
            debug_event NullEvent = {};
            NullEvent.GUID = Element->GUID;
            NullEvent.Type = (u8)Element->Type;

            debug_event *Event = OldestStoredEvent ? &OldestStoredEvent->Event : &NullEvent;
            char Text[256];
            DEBUGEventToText(Text, Text + sizeof(Text), Element, Event,
                DEBUGVarToText_AddName|
                    DEBUGVarToText_AddValue|
                    DEBUGVarToText_NullTerminator|
                    DEBUGVarToText_Colon|
                    DEBUGVarToText_PrettyBools);

            BasicTextElement(Layout, Text, ItemInteraction);
        } break;
    }
}

internal void
DrawTreeLink(debug_state *DebugState, layout *Layout, debug_tree *Tree, debug_variable_link *Link)
{
    u32 FrameOrdinal = DebugState->ViewingFrameOrdinal;
    
    if(HasChildren(Link))
    {
        debug_id ID = DebugIDFromLink(Tree, Link);
        debug_view *View = GetOrCreateDebugViewFor(DebugState, ID);
        debug_interaction ItemInteraction = DebugIDInteraction(DebugInteraction_ToggleExpansion, ID);
        if(DebugState->AltUI)
        {
            ItemInteraction = DebugLinkInteraction(DebugInteraction_TearValue, Link);
        }

        char *Text = Link->Name;

        rectangle2 TextBounds = GetTextSize(DebugState, Text);
        v2 Dim = {GetDim(TextBounds).x, Layout->LineAdvance};

        layout_element Element = BeginElementRectangle(Layout, &Dim);
        DefaultInteraction(&Element, ItemInteraction);
        EndElement(&Element);

        b32 IsHot = InteractionIsHot(DebugState, ItemInteraction);
        v4 ItemColor = IsHot ? V4(1, 1, 0, 1) : V4(1, 1, 1, 1);

        TextOutAt(DebugState, V2(GetMinCorner(Element.Bounds).x,
                GetMaxCorner(Element.Bounds).y - DebugState->FontScale*GetStartingBaselineY(DebugState->DebugFontInfo)),
            Text, ItemColor);

        if(View->Collapsible.ExpandedAlways)
        {
            ++Layout->Depth;
            
            for(debug_variable_link *SubLink = Link->FirstChild;
                SubLink != GetSentinel(Link);
                SubLink = SubLink->Next)
            {
                DrawTreeLink(DebugState, Layout, Tree, SubLink);
            }
            
            --Layout->Depth;
        }
    }
    else
    {
        debug_id DebugID = DebugIDFromLink(Tree, Link);
        DEBUGDrawElement(Layout, Tree, Link->Element, DebugID, FrameOrdinal);
    }
}

internal void
DrawTrees(debug_state *DebugState, v2 MouseP)
{
    object_transform NoTransform = DefaultFlatTransform();
    render_group *RenderGroup = &DebugState->RenderGroup;
    u32 FrameOrdinal = DebugState->ViewingFrameOrdinal;

    for(debug_tree *Tree = DebugState->TreeSentinel.Next;
        Tree != &DebugState->TreeSentinel;
        Tree = Tree->Next)
    {
        layout Layout = BeginLayout(DebugState, MouseP, Tree->UIP);
        debug_variable_link *Group = Tree->Group;
        if(Group)
        {
            DrawTreeLink(DebugState, &Layout, Tree, Group);
        }
        
        debug_interaction MoveInteraction = {};
        MoveInteraction.Type = DebugInteraction_Move;
        MoveInteraction.P = &Tree->UIP;
        
        rectangle2 MoveBox = RectCenterHalfDim(Tree->UIP - V2(4.0f, 4.0f), V2(4.0f, 4.0f));
        PushRect(RenderGroup, NoTransform, MoveBox, 0.0f,
            InteractionIsHot(DebugState, MoveInteraction) ? V4(1, 1, 0, 1) : V4(1, 1, 1, 1));
        
        if(IsInRectangle(MoveBox, MouseP))
        {
            DebugState->NextHotInteraction = MoveInteraction;
        }
        
        EndLayout(&Layout);
    }
}

internal void
DEBUGBeginInteract(debug_state *DebugState, game_input *Input, v2 MouseP)
{
    u32 FrameOrdinal = DebugState->MostRecentFrameOrdinal;
    if(DebugState->HotInteraction.Type)
    {
        if(DebugState->HotInteraction.Type == DebugInteraction_AutoModifyVariable)
        {
            switch(DebugState->HotInteraction.Element->Frames[FrameOrdinal].MostRecentEvent->Event.Type)
            {
                case DebugType_b32:
                {
                    DebugState->HotInteraction.Type = DebugInteraction_ToggleValue;
                } break;

                case DebugType_r32:
                {
                    DebugState->HotInteraction.Type = DebugInteraction_DragValue;
                } break;

                case DebugType_OpenDataBlock:
                {
                    DebugState->HotInteraction.Type = DebugInteraction_ToggleValue;
                } break;
            }
        }

        switch(DebugState->HotInteraction.Type)
        {
            case DebugInteraction_TearValue:
            {
                debug_variable_link *RootGroup = CloneVariableLink(DebugState, DebugState->HotInteraction.Link);
                debug_tree *Tree = AddTree(DebugState, RootGroup, MouseP);
                DebugState->HotInteraction.Type = DebugInteraction_Move;
                DebugState->HotInteraction.P = &Tree->UIP;
            } break;

            case DebugInteraction_Select:
            {
                if(!Input->ShiftDown)
                {
                    ClearSelection(DebugState);
                }
                AddToSelection(DebugState, DebugState->HotInteraction.ID);
            } break;                
        }

        DebugState->Interaction = DebugState->HotInteraction;
    }
    else
    {
        DebugState->Interaction.Type = DebugInteraction_NOP;
    }
}

internal debug_element *
GetElementFromEvent(debug_state *DebugState, debug_event *Event, 
    debug_variable_link *Parent, u32 Op);
void
DEBUGMarkEditedEvent(debug_state *DebugState, debug_event *Event)
{
    if(Event)
    {
        GlobalDebugTable->EditEvent = *Event;
        GlobalDebugTable->EditEvent.GUID = 
            GetElementFromEvent(DebugState, Event, 0, DebugElement_AddToGroup|DebugElement_CreateHierarchy)->OriginalGUID;
    }
}

internal void
DEBUGEndInteract(debug_state *DebugState, game_input *Input, v2 MouseP)
{
    u32 FrameOrdinal = DebugState->MostRecentFrameOrdinal;
    switch(DebugState->Interaction.Type)
    {
        case DebugInteraction_ToggleExpansion:
        {
            debug_view *View = GetOrCreateDebugViewFor(DebugState, DebugState->Interaction.ID);
            View->Collapsible.ExpandedAlways = !View->Collapsible.ExpandedAlways;
        } break;
        
        case DebugInteraction_SetUInt32:
        {
            *(u32 *)DebugState->Interaction.Target = DebugState->Interaction.UInt32;
        } break;
        
        case DebugInteraction_SetPointer:
        {
            *(void **)DebugState->Interaction.Target = DebugState->Interaction.Pointer;
        } break;

        case DebugInteraction_ToggleValue:
        {
            debug_event *Event = &DebugState->Interaction.Element->Frames[FrameOrdinal].
                MostRecentEvent->Event;
            Assert(Event);
            switch(Event->Type)
            {
                case DebugType_b32:
                {
                    Event->Value_b32 = !Event->Value_b32;
                } break;
            }
            DEBUGMarkEditedEvent(DebugState, Event);
        } break;
    }

    DebugState->Interaction.Type = DebugInteraction_None;
    DebugState->Interaction.Generic = 0;
}

internal void
DEBUGInteract(debug_state *DebugState, game_input *Input, v2 MouseP)
{
    v2 dMouseP = MouseP - DebugState->LastMouseP;
    if(DebugState->Interaction.Type)
    {
        u32 FrameOrdinal = DebugState->MostRecentFrameOrdinal;
        debug_tree *Tree = DebugState->Interaction.Tree;
        v2 *P = DebugState->Interaction.P;

        // NOTE(casey): Mouse move interaction
        switch(DebugState->Interaction.Type)
        {
            case DebugInteraction_DragValue:
            {
                debug_event *Event = DebugState->Interaction.Element ? 
                    &DebugState->Interaction.Element->Frames[FrameOrdinal].MostRecentEvent->Event : 0;
                switch(Event->Type)
                {
                    case DebugType_r32:
                    {
                        Event->Value_r32 += 0.1f*dMouseP.y;
                    } break;
                }
                DEBUGMarkEditedEvent(DebugState, Event);
            } break;

            case DebugInteraction_Resize:
            {
                *P += V2(dMouseP.x, -dMouseP.y);
                P->x = Maximum(P->x, 10.0f);
                P->y = Maximum(P->y, 10.0f);
            } break;

            case DebugInteraction_Move:
            {
                *P += V2(dMouseP.x, dMouseP.y);
            } break;
        }

        // NOTE(casey): Click interaction
        for(u32 TransitionIndex = Input->MouseButtons[PlatformMouseButton_Left].HalfTransitionCount;
            TransitionIndex > 1;
            --TransitionIndex)
        {
            DEBUGEndInteract(DebugState, Input, MouseP);
            DEBUGBeginInteract(DebugState, Input, MouseP);
        }

        if(!Input->MouseButtons[PlatformMouseButton_Left].EndedDown)
        {
            DEBUGEndInteract(DebugState, Input, MouseP);
        }
    }
    else
    {
        DebugState->HotInteraction = DebugState->NextHotInteraction;

        for(u32 TransitionIndex = Input->MouseButtons[PlatformMouseButton_Left].HalfTransitionCount;
            TransitionIndex > 1;
            --TransitionIndex)
        {
            DEBUGBeginInteract(DebugState, Input, MouseP);
            DEBUGEndInteract(DebugState, Input, MouseP);
        }

        if(Input->MouseButtons[PlatformMouseButton_Left].EndedDown)
        {
            DEBUGBeginInteract(DebugState, Input, MouseP);
        }
    }

    DebugState->LastMouseP = MouseP;
}

inline u32
GetLaneFromThreadIndex(debug_state *DebugState, u32 ThreadIndex)
{
    u32 Result = 0;

    // TODO(casey): Implement thread ID lookup.

    return(Result);
}

internal debug_thread *
GetDebugThread(debug_state *DebugState, u32 ThreadID)
{
    debug_thread *Result = 0;
    for(debug_thread *Thread = DebugState->FirstThread;
        Thread;
        Thread = Thread->Next)
    {
        if(Thread->ID == ThreadID)
        {
            Result = Thread;
            break;
        }
    }

    if(!Result)
    {
        FREELIST_ALLOCATE(Result, DebugState->FirstFreeThread, PushStruct(&DebugState->DebugArena, debug_thread));

        Result->ID = ThreadID;
        Result->LaneIndex = DebugState->FrameBarLaneCount++;
        Result->FirstOpenCodeBlock = 0;
        Result->FirstOpenDataBlock = 0;
        Result->Next = DebugState->FirstThread;
        DebugState->FirstThread = Result;
    }

    return(Result);
}

internal debug_variable_link *
CreateVariableLink(debug_state *DebugState, u32 NameLength, char *Name)
{
    debug_variable_link *Link = PushStruct(&DebugState->DebugArena, debug_variable_link);    
    DLIST_INIT(GetSentinel(Link));
    Link->Next = Link->Prev = 0;
    Link->Name = NameLength ? PushAndNullTerminate(&DebugState->DebugArena, NameLength, Name) : 0;
    Link->Element = 0;
    
    return(Link);
}

internal debug_variable_link *
AddElementToGroup(debug_state *DebugState, debug_variable_link *Parent, debug_element *Element)
{
    debug_variable_link *Link = CreateVariableLink(DebugState, 0, 0);

    if(Parent)
    {
        DLIST_INSERT_AS_LAST(GetSentinel(Parent), Link);
    }
    Link->Element = Element;
    
    return(Link);
}

internal debug_variable_link *
AddLinkToGroup(debug_state *DebugState, debug_variable_link *Parent, debug_variable_link *Link)
{
    DLIST_INSERT_AS_LAST(GetSentinel(Parent), Link);
    return(Link);
}

internal debug_variable_link *
CloneVariableLink(debug_state *DebugState, debug_variable_link *DestGroup, debug_variable_link *Source)
{
    debug_variable_link *Dest = AddElementToGroup(DebugState, DestGroup, Source->Element);
    Dest->Name = Source->Name;
    if(HasChildren(Source))
    {
        for(debug_variable_link *Child = Source->FirstChild;
            Child != GetSentinel(Source);
            Child = Child->Next)
        {
            CloneVariableLink(DebugState, Dest, Child);
        }
    }

    return(Dest);
}    

internal debug_variable_link *
CloneVariableLink(debug_state *DebugState, debug_variable_link *Source)
{
    debug_variable_link *Result = CloneVariableLink(DebugState, 0, Source);
    return(Result);
}

internal debug_variable_link *
GetOrCreateGroupWithName(debug_state *DebugState, debug_variable_link *Parent, u32 NameLength, char *Name)
{
    debug_variable_link *Result = 0;
    for(debug_variable_link *Link = Parent->FirstChild;
        Link != GetSentinel(Parent);
        Link = Link->Next)
    {
        if(StringsAreEqual(NameLength, Name, Link->Name))
        {
            Result = Link;
        }
    }

    if(!Result)
    {
        Result = CreateVariableLink(DebugState, NameLength, Name);
        AddLinkToGroup(DebugState, Parent, Result);
    }

    return(Result);
}

internal debug_variable_link *
GetGroupForHierarchicalName(debug_state *DebugState, debug_variable_link *Parent, char *Name, b32 CreateTerminal)
{
    debug_variable_link *Result = Parent;

    char *FirstSeparator = 0;
    char *Scan = Name;
    for(;
        *Scan;
        ++Scan)
    {
        if(*Scan == '/')
        {
            FirstSeparator = Scan;
            break;
        }
    }

    if(FirstSeparator || CreateTerminal)
    {
        u32 NameLength = 0;
        if(FirstSeparator)
        {
            NameLength = (u32)(FirstSeparator - Name);
        }
        else
        {
            NameLength = (u32)(Scan - Name);
        }

        Result = GetOrCreateGroupWithName(DebugState, Parent, NameLength, Name);
        if(FirstSeparator)
        {
            Result = GetGroupForHierarchicalName(DebugState, Result, FirstSeparator + 1, CreateTerminal);
        }
    }

    return(Result);
}

inline open_debug_block *
AllocateOpenDebugBlock(debug_state *DebugState, debug_element *Element,
                       u32 FrameIndex, debug_event *Event,
                       open_debug_block **FirstOpenBlock)
{
    open_debug_block *Result = 0;
    FREELIST_ALLOCATE(Result, DebugState->FirstFreeBlock, PushStruct(&DebugState->DebugArena, open_debug_block));

    Result->StartingFrameIndex = FrameIndex;
    Result->BeginClock = Event->Clock;
    Result->Element = Element;
    Result->NextFree = 0;

    Result->Parent = *FirstOpenBlock;
    *FirstOpenBlock = Result;

    return(Result);
}

inline void
DeallocateOpenDebugBlock(debug_state *DebugState, open_debug_block **FirstOpenBlock)
{
    open_debug_block *FreeBlock = *FirstOpenBlock;
    *FirstOpenBlock = FreeBlock->Parent;

    FreeBlock->NextFree = DebugState->FirstFreeBlock;
    DebugState->FirstFreeBlock = FreeBlock;                            
}

inline b32
EventsMatch(debug_event A, debug_event B)
{
    // TODO(casey): Have counters for blocks?
    b32 Result = (A.ThreadID == B.ThreadID);

    return(Result);
}

internal void
FreeFrame(debug_state *DebugState, u32 FrameOrdinal)
{
    Assert(FrameOrdinal < DEBUG_FRAME_COUNT);

    u32 FreedEventCount = 0;

    for(u32 ElementHashIndex = 0;
        ElementHashIndex < ArrayCount(DebugState->ElementHash);
        ++ElementHashIndex)
    {
        for(debug_element *Element = DebugState->ElementHash[ElementHashIndex];
            Element;
            Element = Element->NextInHash)
        {
            debug_element_frame *ElementFrame = Element->Frames + FrameOrdinal;
            while(ElementFrame->OldestEvent)
            {
                debug_stored_event *FreeEvent = ElementFrame->OldestEvent;
                ElementFrame->OldestEvent = FreeEvent->Next;
                FREELIST_DEALLOCATE(FreeEvent, DebugState->FirstFreeStoredEvent);
                ++FreedEventCount;
            }
            ZeroStruct(*ElementFrame);
        }
    }

    debug_frame *Frame = DebugState->Frames + FrameOrdinal;
    Assert(Frame->StoredEventCount == FreedEventCount);

    ZeroStruct(*Frame);
}

internal void
InitFrame(debug_state *DebugState, u64 BeginClock, debug_frame *Result)
{
    Result->FrameIndex = DebugState->TotalFrameCount++;
    Result->FrameBarScale = 1.0f;
    Result->BeginClock = BeginClock;
}

inline void
IncrementFrameOrdinal(u32 *Ordinal)
{
    *Ordinal = (*Ordinal+1) % DEBUG_FRAME_COUNT;
}

internal void
FreeOldestFrame(debug_state *DebugState)
{
    FreeFrame(DebugState, DebugState->OldestFrameOrdinal);

    if(DebugState->OldestFrameOrdinal == DebugState->MostRecentFrameOrdinal)
    {
        IncrementFrameOrdinal(&DebugState->MostRecentFrameOrdinal);
    }
    IncrementFrameOrdinal(&DebugState->OldestFrameOrdinal);
}

inline debug_frame *
GetCollationFrame(debug_state *DebugState)
{
    debug_frame *Result = DebugState->Frames + DebugState->CollationFrameOrdinal;

    return(Result);
}

internal debug_stored_event *
StoreEvent(debug_state *DebugState, debug_element *Element, debug_event *Event)
{
    debug_stored_event *Result = 0;
    while(!Result)
    {
        Result = DebugState->FirstFreeStoredEvent;
        if(Result)
        {
            DebugState->FirstFreeStoredEvent = Result->NextFree;
        }
        else
        {
            if(ArenaHasRoomFor(&DebugState->PerFrameArena, sizeof(debug_stored_event)))
            {
                Result = PushStruct(&DebugState->PerFrameArena, debug_stored_event);
            }
            else
            {
                FreeOldestFrame(DebugState);
            }
        }
    }

    debug_frame *CollationFrame = GetCollationFrame(DebugState);

    Result->Next = 0;
    Result->FrameIndex = CollationFrame->FrameIndex;
    Result->Event = *Event;

    ++CollationFrame->StoredEventCount;

    debug_element_frame *Frame = Element->Frames + DebugState->CollationFrameOrdinal;
    if(Frame->MostRecentEvent)
    {
        Frame->MostRecentEvent = Frame->MostRecentEvent->Next = Result;
    }
    else
    {
        Frame->OldestEvent = Frame->MostRecentEvent = Result;
    }

    return(Result);
}

internal debug_element *
GetElementFromEvent(debug_state *DebugState, debug_event *Event, debug_variable_link *Parent,
                    u32 Op)
{
    Assert(Event->GUID);

    if(!Parent)
    {
        Parent = DebugState->RootGroup;
    }

    debug_parsed_name ParsedName = DebugParseName(Event->GUID);
    u32 Index = (ParsedName.HashValue % ArrayCount(DebugState->ElementHash));

    debug_element *Result = GetElementFromGUID(DebugState, Index, Event->GUID);
    if(!Result)
    {
        Result = PushStruct(&DebugState->DebugArena, debug_element);

        Result->OriginalGUID = Event->GUID;
        Result->GUID = PushString(&DebugState->DebugArena, Event->GUID);
        Result->FileNameCount = ParsedName.FileNameCount;
        Result->LineNumber = ParsedName.LineNumber;
        Result->NameStartsAt = ParsedName.NameStartsAt;
        Result->Type = (debug_type)Event->Type;

        Result->NextInHash = DebugState->ElementHash[Index];
        DebugState->ElementHash[Index] = Result;

        debug_variable_link *ParentGroup = Parent;
        if(Op & DebugElement_CreateHierarchy)
        {
            ParentGroup = GetGroupForHierarchicalName(DebugState, Parent, GetName(Result), false);
        }
        
        if(Op & DebugElement_AddToGroup)
        {
            AddElementToGroup(DebugState, ParentGroup, Result);
        }
    }

    return(Result);
}

internal void
CollateDebugRecords(debug_state *DebugState, u32 EventCount, debug_event *EventArray)
{    
    for(u32 EventIndex = 0;
        EventIndex < EventCount;
        ++EventIndex)
    {
        debug_event *Event = EventArray + EventIndex;
        if(Event->Type == DebugType_FrameMarker)
        {
            debug_frame *CollationFrame = GetCollationFrame(DebugState);

            CollationFrame->EndClock = Event->Clock;
            if(CollationFrame->RootProfileNode)
            {
                CollationFrame->RootProfileNode->ProfileNode.Duration =
                    (CollationFrame->EndClock - CollationFrame->BeginClock);
            }

            CollationFrame->WallSecondsElapsed = Event->Value_r32;

            r32 ClockRange = (r32)(CollationFrame->EndClock - CollationFrame->BeginClock);
            ++DebugState->TotalFrameCount;

            if(DebugState->Paused)
            {
                FreeFrame(DebugState, DebugState->CollationFrameOrdinal);
            }
            else
            {
                DebugState->MostRecentFrameOrdinal = DebugState->CollationFrameOrdinal;
                IncrementFrameOrdinal(&DebugState->CollationFrameOrdinal);
                if(DebugState->CollationFrameOrdinal == DebugState->OldestFrameOrdinal)
                {
                    FreeOldestFrame(DebugState);
                }
                CollationFrame = GetCollationFrame(DebugState);
            }
            InitFrame(DebugState, Event->Clock, CollationFrame);
        }
        else 
        {
            debug_frame *CollationFrame = GetCollationFrame(DebugState);

            Assert(CollationFrame);

            u32 FrameIndex = DebugState->TotalFrameCount - 1;
            debug_thread *Thread = GetDebugThread(DebugState, Event->ThreadID);
            u64 RelativeClock = Event->Clock - CollationFrame->BeginClock;

            debug_variable_link *DefaultParentGroup = DebugState->RootGroup;
            if(Thread->FirstOpenDataBlock)
            {
                DefaultParentGroup = Thread->FirstOpenDataBlock->Group;
            }

            switch(Event->Type)
            {
                case DebugType_BeginBlock:
                {
                    ++CollationFrame->ProfileBlockCount;
                    debug_element *Element = 
                        GetElementFromEvent(DebugState, Event, DebugState->ProfileGroup, 
                            DebugElement_AddToGroup);

                    debug_stored_event *ParentEvent = CollationFrame->RootProfileNode;
                    u64 ClockBasis = CollationFrame->BeginClock;
                    if(Thread->FirstOpenCodeBlock)
                    {
                        ParentEvent = Thread->FirstOpenCodeBlock->Node;
                        ClockBasis = Thread->FirstOpenCodeBlock->BeginClock;
                    }
                    else if(!ParentEvent)
                    {
                        debug_event NullEvent = {};
                        ParentEvent = StoreEvent(DebugState, DebugState->RootProfileElement, &NullEvent);
                        debug_profile_node *Node = &ParentEvent->ProfileNode;
                        Node->Element = 0;
                        Node->FirstChild = 0;
                        Node->NextSameParent = 0;
                        Node->ParentRelativeClock = 0;
                        Node->Duration = 0;
                        Node->DurationOfChildren = 0;
                        Node->ThreadOrdinal = 0;
                        Node->CoreIndex = 0;

                        ClockBasis = CollationFrame->BeginClock;
                        CollationFrame->RootProfileNode = ParentEvent;
                    }

                    debug_stored_event *StoredEvent = StoreEvent(DebugState, Element, Event);
                    debug_profile_node *Node = &StoredEvent->ProfileNode;
                    Node->Element = Element;
                    Node->FirstChild = 0;
                    Node->ParentRelativeClock = Event->Clock - ClockBasis;
                    Node->Duration = 0;
                    Node->DurationOfChildren = 0;
                    Node->ThreadOrdinal = (u16)Thread->LaneIndex;
                    Node->CoreIndex = Event->CoreIndex;

                    Node->NextSameParent = ParentEvent->ProfileNode.FirstChild;
                    ParentEvent->ProfileNode.FirstChild = StoredEvent;

                    open_debug_block *DebugBlock = AllocateOpenDebugBlock(
                        DebugState, Element, FrameIndex, Event, 
                        &Thread->FirstOpenCodeBlock);
                    DebugBlock->Node = StoredEvent;
                } break;

                case DebugType_EndBlock:
                {
                    if(Thread->FirstOpenCodeBlock)
                    {
                        open_debug_block *MatchingBlock = Thread->FirstOpenCodeBlock;
                        Assert(Thread->ID == Event->ThreadID);

                        debug_profile_node *Node = &MatchingBlock->Node->ProfileNode;
                        Node->Duration = Event->Clock - MatchingBlock->BeginClock;
                        
                        DeallocateOpenDebugBlock(DebugState, &Thread->FirstOpenCodeBlock);
                        
                        if(Thread->FirstOpenCodeBlock)
                        {
                            debug_profile_node *ParentNode = 
                                &Thread->FirstOpenCodeBlock->Node->ProfileNode;
                            ParentNode->DurationOfChildren += Node->Duration;
                        }
                    }
                } break;

                case DebugType_OpenDataBlock:
                {
                    ++CollationFrame->DataBlockCount;
                    open_debug_block *DebugBlock = AllocateOpenDebugBlock(
                        DebugState, 0, FrameIndex, Event, &Thread->FirstOpenDataBlock);

                    debug_parsed_name ParsedName = DebugParseName(Event->GUID);
                    DebugBlock->Group =
                        GetGroupForHierarchicalName(DebugState, DefaultParentGroup, ParsedName.Name, true);
                } break;

                case DebugType_CloseDataBlock:
                {
                    if(Thread->FirstOpenDataBlock)
                    {
                        open_debug_block *MatchingBlock = Thread->FirstOpenDataBlock;
                        Assert(Thread->ID == Event->ThreadID);
                        DeallocateOpenDebugBlock(DebugState, &Thread->FirstOpenDataBlock);
                    }
                } break;

                default:
                {
                    debug_element *Element = GetElementFromEvent(DebugState, Event, DefaultParentGroup,  DebugElement_AddToGroup|DebugElement_CreateHierarchy);
                    Element->OriginalGUID = Event->GUID;
                    StoreEvent(DebugState, Element, Event);
                } break;
            }
        }
    }
}

internal void
DEBUGStart(debug_state *DebugState, game_render_commands *Commands, game_assets *Assets, u32 MainGenerationID, u32 Width, u32 Height)
{
    TIMED_FUNCTION();

    if(!DebugState->Initialized)
    {
        DebugState->FrameBarLaneCount = 0;
        DebugState->FirstThread = 0;
        DebugState->FirstFreeThread = 0;
        DebugState->FirstFreeBlock = 0;

        DebugState->TotalFrameCount = 0;    
        DebugState->MostRecentFrameOrdinal = 0;
        DebugState->CollationFrameOrdinal = 1;
        DebugState->OldestFrameOrdinal = 0;


        DebugState->TreeSentinel.Next = &DebugState->TreeSentinel;
        DebugState->TreeSentinel.Prev = &DebugState->TreeSentinel;
        DebugState->TreeSentinel.Group = 0;

        memory_index TotalMemorySize = DebugGlobalMemory->DebugStorageSize - sizeof(debug_state);
        InitializeArena(&DebugState->DebugArena, TotalMemorySize, DebugState + 1);
#if 1
        SubArena(&DebugState->PerFrameArena, &DebugState->DebugArena, (TotalMemorySize / 2));
#else
        // NOTE(casey): This is the stress-testing case to make sure the memory
        // recycling works.
        SubArena(&DebugState->PerFrameArena, &DebugState->DebugArena, 8*1024*1024);
#endif

        
        DebugState->RootGroup = CreateVariableLink(DebugState, 4, "Root");
        DebugState->RootInfoSize = 256;
        DebugState->RootGroup->Name = 
            DebugState->RootInfo = (char *)PushSize(&DebugState->DebugArena, 
                DebugState->RootInfoSize);
        
        DebugState->ProfileGroup = CreateVariableLink(DebugState, 7, "Profile");

        debug_event RootProfileEvent = {};
        RootProfileEvent.GUID = DEBUG_NAME("RootProfile");
        DebugState->RootProfileElement = GetElementFromEvent(DebugState, &RootProfileEvent, 0, 0);

        DebugState->Paused = false;
        
        DebugState->Initialized = true;

        AddTree(DebugState, DebugState->RootGroup, V2(-0.5f*Width, 0.5f*Height));
    }

    DebugState->RenderGroup = BeginRenderGroup(Assets, Commands, MainGenerationID, false);

    DebugState->DebugFont = PushFont(&DebugState->RenderGroup, DebugState->FontID);
    DebugState->DebugFontInfo = GetFontInfo(DebugState->RenderGroup.Assets, DebugState->FontID);

    DebugState->GlobalWidth = (r32)Width;
    DebugState->GlobalHeight = (r32)Height;

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    MatchVector.E[Tag_FontType] = (r32)FontType_Debug;
    WeightVector.E[Tag_FontType] = 1.0f;
    DebugState->FontID = GetBestMatchFontFrom(Assets, Asset_Font, &MatchVector, &WeightVector);

    DebugState->FontScale = 1.0f;
    Orthographic(&DebugState->RenderGroup, Width, Height, 1.0f);
    DebugState->LeftEdge = -0.5f*Width;
    DebugState->RightEdge = 0.5f*Width;

    DebugState->TextTransform = DefaultFlatTransform();
    DebugState->ShadowTransform = DefaultFlatTransform();
    DebugState->UITransform = DefaultFlatTransform();
    DebugState->BackingTransform = DefaultFlatTransform();

    DebugState->BackingTransform.SortBias = 100000.0f;
    DebugState->ShadowTransform.SortBias = 200000.0f;
    DebugState->UITransform.SortBias = 300000.0f;
    DebugState->TextTransform.SortBias = 400000.0f;

    DebugState->DefaultClipRect = DebugState->RenderGroup.CurrentClipRectIndex;
    
    if(!DebugState->Paused)
    {
        DebugState->ViewingFrameOrdinal = DebugState->MostRecentFrameOrdinal;
    }
}

internal void
DEBUGEnd(debug_state *DebugState, game_input *Input)
{
    TIMED_FUNCTION();

    render_group *RenderGroup = &DebugState->RenderGroup;

    debug_event *HotEvent = 0;

    debug_frame *MostRecentFrame = DebugState->Frames + DebugState->ViewingFrameOrdinal;
    _snprintf_s(DebugState->RootInfo, DebugState->RootInfoSize, DebugState->RootInfoSize,
        "%.02fms %de %dp %dd",
        MostRecentFrame->WallSecondsElapsed * 1000.0f,
        MostRecentFrame->StoredEventCount,
        MostRecentFrame->ProfileBlockCount,
        MostRecentFrame->DataBlockCount);

    DebugState->AltUI = Input->MouseButtons[PlatformMouseButton_Right].EndedDown;
    v2 MouseP = Unproject(RenderGroup, DefaultFlatTransform(), V2(Input->MouseX, Input->MouseY)).xy;
    DebugState->MouseTextLayout = BeginLayout(DebugState, MouseP, MouseP);
    DrawTrees(DebugState, MouseP);
    EndLayout(&DebugState->MouseTextLayout);
    DEBUGInteract(DebugState, Input, MouseP);
    
    EndRenderGroup(&DebugState->RenderGroup);

    // NOTE(casey): Clear the UI state for the next frame
    ZeroStruct(DebugState->NextHotInteraction);
}

extern "C" DEBUG_GAME_FRAME_END(DEBUGGameFrameEnd)
{   
    ZeroStruct(GlobalDebugTable->EditEvent);

    GlobalDebugTable->CurrentEventArrayIndex = !GlobalDebugTable->CurrentEventArrayIndex;    
    u64 ArrayIndex_EventIndex = AtomicExchangeU64(&GlobalDebugTable->EventArrayIndex_EventIndex,
                                                  (u64)GlobalDebugTable->CurrentEventArrayIndex << 32);

    u32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
    Assert(EventArrayIndex <= 1);
    u32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;

    debug_state *DebugState = (debug_state *)Memory->DebugStorage;
    if(DebugState)
    {
        game_assets *Assets = DEBUGGetGameAssets(Memory);

        DEBUGStart(DebugState, RenderCommands, Assets, DEBUGGetMainGenerationID(Memory), RenderCommands->Width, RenderCommands->Height);
        CollateDebugRecords(DebugState, EventCount, GlobalDebugTable->Events[EventArrayIndex]);
        DEBUGEnd(DebugState, Input);
    }
}

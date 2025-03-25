/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_debug.h"

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
                Result.LineNumber = S32FromZ(Scan + 1);
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
DEBUGGetState(engine_memory *Memory)
{
    debug_state *DebugState = 0;
    if(Memory)
    {
        DebugState = Memory->DebugState;
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

internal umm
DEBUGEventToText(char *Buffer, char *End, debug_element *Element, debug_event *Event, u32 Flags)
{
    char *At = Buffer;
    char *Name = Element->GUID;

    if(Flags & DEBUGVarToText_AddDebugUI)
    {
        At += FormatString(End - At, At, "#define DEBUGUI_");
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

        At += FormatString(End - At, At, "%s%s ", UseName, (Flags & DEBUGVarToText_Colon) ? ":" : "");
    }

    if(Flags & DEBUGVarToText_AddValue)
    {
        switch(Event->Type)
        {
            case DebugType_r32:                
            {
                At += FormatString(End - At, At, "%f", Event->Value_r32);
                if(Flags & DEBUGVarToText_FloatSuffix)
                {
                    *At++ = 'f';
                }
            } break;

            case DebugType_b32:                
            {
                if(Flags & DEBUGVarToText_PrettyBools)
                {
                    At += FormatString(End - At, At, "%s", Event->Value_b32 ? "true" : "false");
                }
                else
                {
                    At += FormatString(End - At, At, "%d", Event->Value_b32);
                }
            } break;

            case DebugType_s32:
            {
                At += FormatString(End - At, At, "%d", Event->Value_s32);
            } break;

            case DebugType_u32:   
            {
                At += FormatString(End - At, At, "%u", Event->Value_u32);
            } break;

            case DebugType_v2:
            {
                At += FormatString(End - At, At, "V2(%f, %f)", Event->Value_v2.x, Event->Value_v2.y);
            } break;

            case DebugType_v3:
            {
                At += FormatString(End - At, At, "V3(%f, %f, %f)",
                                   Event->Value_v3.x, Event->Value_v3.y, Event->Value_v3.z);
            } break;

            case DebugType_v4:
            {
                At += FormatString(End - At, At, "V4(%f, %f, %f, %f)",
                                   Event->Value_v4.x, Event->Value_v4.y,
                                   Event->Value_v4.z, Event->Value_v4.w);
            } break;

            case DebugType_rectangle2:
            {
                At += FormatString(End - At, At, "Rect2(%f, %f -> %f, %f)",
                                   Event->Value_rectangle2.Min.x,
                                   Event->Value_rectangle2.Min.y,
                                   Event->Value_rectangle2.Max.x,
                                   Event->Value_rectangle2.Max.y);
            } break;

            case DebugType_rectangle3:
            {
                At += FormatString(End - At, At, "Rect2(%f, %f, %f -> %f, %f, %f)",
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
                At += FormatString(End - At, At, "UNHANDLED: %s", Event->GUID);
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
//        DebugState->NextHotInteraction = DebugIDInteraction(DebugInteraction_Select, ID);
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

//        if(DebugIDsAreEqual(DebugState->HotInteraction.ID, ID))
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
//        Result = IsSelected(DebugState, ID)
//            || DebugIDsAreEqual(DebugState->HotInteraction.ID, ID);
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

internal void
DrawProfileBarsnk(debug_state *DebugState, debug_id GraphID, struct nk_rect ProfileRect,
                  debug_profile_node *RootNode, r32 LaneStride, r32 LaneHeight, u32 DepthRemaining)
{
    r32 FrameSpan = (r32)(RootNode->Duration);
    r32 PixelSpan = ProfileRect.w;
    
    r32 Scale = 0.0f;
    if(FrameSpan > 0)
    {
        Scale = PixelSpan / FrameSpan;
    }

    const struct nk_input *in = DebugState->nk->current->widgets_disabled ? 0 : &DebugState->nk->input;
    for(debug_stored_event *StoredEvent = RootNode->FirstChild;
        StoredEvent;
        StoredEvent = StoredEvent->ProfileNode.NextSameParent)
    {
        debug_profile_node *Node = &StoredEvent->ProfileNode;
        debug_element *Element = Node->Element;
        Assert(Element);

        v3 Color = DebugColorTable[U32FromPointer(Element->GUID)%ArrayCount(DebugColorTable)];
        r32 ThisMinX = ProfileRect.x + Scale*(r32)(Node->ParentRelativeClock);
        r32 ThisMaxX = Scale*(r32)(Node->Duration);

        u32 LaneIndex = Node->ThreadOrdinal;
        r32 LaneY = ProfileRect.y - LaneStride*LaneIndex;
        struct nk_rect RegionRect = Platform.UI.NkRect(ThisMinX, LaneY, ThisMaxX, LaneHeight);

        nk_color C = {(u8)(Color.r*255.0f), (u8)(Color.g*255.0f), (u8)(Color.b*255.0f), 255};

        nk_flags ret = 0;

        if (!(DebugState->nk->current->layout->flags & NK_WINDOW_ROM) && in &&
            NK_INBOX(in->mouse.pos.x,in->mouse.pos.y,RegionRect.x,RegionRect.y,RegionRect.w,RegionRect.h))
        {
            ret = NK_CHART_HOVERING;
            ret |= (!in->mouse.buttons[NK_BUTTON_LEFT].down &&
                    in->mouse.buttons[NK_BUTTON_LEFT].clicked) ? NK_CHART_CLICKED: 0;

            C = {255, 0, 0, 120};

            char TextBuffer[256];
            FormatString(sizeof(TextBuffer), TextBuffer, "%s: %10llucy", Element->GUID, Node->Duration);
            char *At = TextBuffer;
            char *OnePastLastBackSlash = 0;
            while(true)
            {
                if(*At == '\0')
                {
                    break;
                }

                if(*At == '\\')
                {
                    OnePastLastBackSlash = At;
                    *OnePastLastBackSlash++;
                }

                *At++;
            }

            Platform.UI.NkTooltip(DebugState->nk, OnePastLastBackSlash);
        }
                    

        Platform.UI.NkFillRect(&DebugState->nk->current->buffer,
                               RegionRect, 0.0f, C);
        Platform.UI.NkStrokeRect(&DebugState->nk->current->buffer,
                                 RegionRect, 0.0f, 0.15f, {0, 0, 0, 255});

        if(ret & NK_CHART_CLICKED)
        {
            debug_view *View = GetOrCreateDebugViewFor(DebugState, GraphID);
            View->ProfileGraph.GUID = Element->GUID;
        }

        if(DepthRemaining > 0)
        {
            DrawProfileBarsnk(DebugState, GraphID, RegionRect, Node, 0, LaneHeight/2,
                DepthRemaining - 1);
        }
    }
}

internal void
DrawProfileIn(debug_state *DebugState, debug_id GraphID, debug_element *RootElement)
{
    Platform.UI.NkLayoutRowDynamic(DebugState->nk, 300, 1);
    struct nk_rect Rect = Platform.UI.NkRect(DebugState->nk->current->layout->at_x,
                                             DebugState->nk->current->layout->at_y,
                                             DebugState->nk->current->layout->max_x - 4.0f, 300);
    Platform.UI.NkFillRect(&DebugState->nk->current->buffer,
                           Rect, 0.0f, {255, 0, 0, 100});

    u32 LaneCount = DebugState->FrameBarLaneCount;
    r32 LaneHeight = 0.0f;
    if(LaneCount > 0)
    {
        LaneHeight = Rect.h / (r32)LaneCount;
    }            

    debug_element_frame *RootFrame = RootElement->Frames + DebugState->ViewingFrameOrdinal;
    r32 NextX = Rect.x;
    u64 TotalClock = GetTotalClocks(RootFrame);
    u64 RelativeClock = 0;
    for(debug_stored_event *Event = RootFrame->OldestEvent;
        Event;
        Event = Event->Next)
    {
        debug_profile_node *Node = &Event->ProfileNode;
        struct nk_rect EventRect = Rect;

        RelativeClock += Node->Duration;
        r32 t = (r32)((r64)RelativeClock / (r64)TotalClock);
        EventRect.x = NextX;
        EventRect.w = (1.0f - t)*Rect.x + t*Rect.w;
        NextX = EventRect.w;

        DrawProfileBarsnk(DebugState, GraphID, EventRect, Node, LaneHeight, LaneHeight, 1);
    }
}

internal void
DrawFrameBars(debug_state *DebugState, debug_id GraphID, debug_element *RootElement)
{
    Platform.UI.NkLayoutRowDynamic(DebugState->nk, 300, 1);
    struct nk_rect Rect = Platform.UI.NkRect(DebugState->nk->current->layout->at_x,
                                             DebugState->nk->current->layout->at_y,
                                             DebugState->nk->current->layout->max_x - 4.0f, 300);

    u32 FrameCount = ArrayCount(RootElement->Frames);
    if(FrameCount > 0)
    {
        r32 BarWidth = (Rect.w / (r32)FrameCount);
        r32 AtX = Rect.x;

        const struct nk_input *in = DebugState->nk->current->widgets_disabled ? 0 : &DebugState->nk->input;
        for(u32 FrameIndex = 0;
            FrameIndex < FrameCount;
            ++FrameIndex)
        {
            debug_stored_event *RootEvent = RootElement->Frames[FrameIndex].MostRecentEvent;
            if(RootEvent)
            {
                debug_profile_node *RootNode = &RootEvent->ProfileNode;
                r32 FrameSpan = (r32)(RootNode->Duration);
                r32 PixelSpan = Rect.h;
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
                    r32 ThisMinY = Rect.y + Scale*(r32)(Node->ParentRelativeClock);
                    r32 ThisMaxY = Scale*(r32)(Node->Duration);

                    struct nk_rect RegionRectnk = Platform.UI.NkRect(AtX, ThisMinY, BarWidth, ThisMaxY);

                    nk_color C = {(u8)(Color.r*255.0f), (u8)(Color.g*255.0f), (u8)(Color.b*255.0f), 255};
                    nk_flags ret = 0;

                    if (!(DebugState->nk->current->layout->flags & NK_WINDOW_ROM) && in &&
                        NK_INBOX(in->mouse.pos.x,in->mouse.pos.y,RegionRectnk.x,RegionRectnk.y,RegionRectnk.w,RegionRectnk.h))
                    {
                        ret = NK_CHART_HOVERING;
                        ret |= (!in->mouse.buttons[NK_BUTTON_LEFT].down &&
                                in->mouse.buttons[NK_BUTTON_LEFT].clicked) ? NK_CHART_CLICKED: 0;

                        C = {255, 0, 0, 120};

                        char TextBuffer[256];
                        FormatString(sizeof(TextBuffer), TextBuffer, "%s: %10llucy", Element->GUID, Node->Duration);
                        char *At = TextBuffer;
                        char *OnePastLastBackSlash = 0;
                        while(true)
                        {
                            if(*At == '\0')
                            {
                                break;
                            }

                            if(*At == '\\')
                            {
                                OnePastLastBackSlash = At;
                                *OnePastLastBackSlash++;
                            }

                            *At++;
                        }

                        Platform.UI.NkTooltip(DebugState->nk, OnePastLastBackSlash);
                    }
                    
                    Platform.UI.NkFillRect(&DebugState->nk->current->buffer,
                                           RegionRectnk, 0.0f, C);
                    Platform.UI.NkStrokeRect(&DebugState->nk->current->buffer,
                                             RegionRectnk, 0.0f, 0.15f, {0, 0, 0, 255});

                    if(ret & NK_CHART_CLICKED)
                    {
                        debug_view *View = GetOrCreateDebugViewFor(DebugState, GraphID);
                        View->ProfileGraph.GUID = Element->GUID;
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
DrawTopClocksList(debug_state *DebugState, debug_id GraphID, debug_element *RootElement)
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
    
    Platform.UI.NkLayoutRowDynamic(DebugState->nk, 20, 1);
    for(Index = 0;
        (Index < LinkCount);
        ++Index)
    {
        debug_clock_entry *Entry = Entries + SortA[Index].Index;
        debug_statistic *Stats = &Entry->Stats;
        debug_element *Element = Entry->Element;
        
        char TextBuffer[256];
        FormatString(sizeof(TextBuffer), TextBuffer, "%10ucy %05.02f%% %4d %s", (u32)Stats->Sum,
                     (PC*Stats->Sum), Stats->Count, Element->GUID + Element->NameStartsAt);

        if(Stats->Sum > 0)
        {
            Platform.UI.NkLabel(DebugState->nk, TextBuffer, NK_TEXT_LEFT);
        }
    }
    
    EndTemporaryMemory(Temp);
}

internal void
DrawFrameSlider(debug_state *DebugState, debug_id SliderID, debug_element *RootElement)
{
    Platform.UI.NkLayoutRowDynamic(DebugState->nk, 50, 1);
    struct nk_rect Rect = Platform.UI.NkRect(DebugState->nk->current->layout->at_x,
                                             DebugState->nk->current->layout->at_y,
                                             DebugState->nk->current->layout->max_x, 50);
    u32 FrameCount = ArrayCount(RootElement->Frames);
    if(FrameCount > 0)
    {
        const struct nk_input *in = DebugState->nk->current->widgets_disabled ? 0 : &DebugState->nk->input;
        Platform.UI.NkFillRect(&DebugState->nk->current->buffer,
                               Rect, 0.0f, {0, 0, 0, 64});

        r32 BarWidth = (Rect.w / (r32)FrameCount);
        r32 AtX = Rect.x;
        r32 ThisMinY = Rect.y;
        r32 ThisMaxY = Rect.h;
        for(u32 FrameIndex = 0;
            FrameIndex < FrameCount;
            ++FrameIndex)
        {
            rectangle2 RegionRect = RectMinMax(V2(AtX, ThisMinY), V2(AtX + BarWidth, ThisMaxY));
            struct nk_rect RegionRectnk = Platform.UI.NkRect(AtX - BarWidth, ThisMinY, BarWidth, ThisMaxY);
            
            nk_color C = {};
            nk_color OC = {128, 128, 128, 255};
            b32 Highlight = false;
            if(FrameIndex == DebugState->ViewingFrameOrdinal)
            {
                C = {255, 255, 0, 255};
                Highlight = true;
            }

            if(FrameIndex == DebugState->MostRecentFrameOrdinal)
            {
                C = {0, 255, 0, 255};
                Highlight = true;
            }

            if(FrameIndex == DebugState->CollationFrameOrdinal)
            {
                C = {255, 0, 0, 255};
                Highlight = true;
            }

            if(FrameIndex == DebugState->OldestFrameOrdinal)
            {
                C = {0, 128, 0, 255};
                Highlight = true;
            }

            nk_flags ret = 0;

            if (!(DebugState->nk->current->layout->flags & NK_WINDOW_ROM) && in &&
                NK_INBOX(in->mouse.pos.x,in->mouse.pos.y,RegionRectnk.x,RegionRectnk.y,RegionRectnk.w,RegionRectnk.h))
            {
                ret = NK_CHART_HOVERING;
                ret |= (!in->mouse.buttons[NK_BUTTON_LEFT].down &&
                        in->mouse.buttons[NK_BUTTON_LEFT].clicked) ? NK_CHART_CLICKED: 0;

                OC = {255, 0, 0, 120};

                char TextBuffer[256];
                FormatString(sizeof(TextBuffer), TextBuffer, "%u", FrameIndex);

                Platform.UI.NkTooltip(DebugState->nk, TextBuffer);
            }

            if(Highlight)
            {
                Platform.UI.NkFillRect(&DebugState->nk->current->buffer,
                                       RegionRectnk, 0.0f, C);
            }

            Platform.UI.NkStrokeRect(&DebugState->nk->current->buffer,
                                     RegionRectnk, 0.0f, 0.15f, OC);

            if(ret & NK_CHART_CLICKED)
            {
                DebugState->ViewingFrameOrdinal = FrameIndex;
            }

            AtX += BarWidth;
        }
    }
}

internal void
DEBUGDrawElement(debug_state *DebugState, debug_tree *Tree, debug_element *Element, debug_id DebugID,
                 u32 FrameOrdinal)
{
    debug_stored_event *OldestStoredEvent = 
        Element->Frames[DebugState->ViewingFrameOrdinal].OldestEvent;

    debug_view *View = GetOrCreateDebugViewFor(DebugState, DebugID);
    switch(Element->Type)
    {
        case DebugType_bitmap_id:
        {
#if 0
            debug_event *Event = OldestStoredEvent ? &OldestStoredEvent->Event : 0;
            loaded_bitmap *Bitmap = 0;
            r32 BitmapScale = View->InlineBlock.Dim.y;
            if(Event)
            {
                Bitmap = GetBitmap(RenderGroup->Assets, Event->Value_bitmap_id, RenderGroup->GenerationID);
                if(Bitmap)
                {
                    used_bitmap_dim Dim = GetBitmapDim(RenderGroup, &NoTransform, Bitmap, BitmapScale, V3(0.0f, 0.0f, 0.0f), 1.0f);
                    View->InlineBlock.Dim.x = Dim.Size.x;
                }
            }

            layout_element LayEl = BeginElementRectangle(Layout, &View->InlineBlock.Dim);
            MakeElementSizable(&LayEl);
            DefaultInteraction(&LayEl, ItemInteraction);
            EndElement(&LayEl);
            PushRect(&DebugState->RenderGroup, &DebugState->BackingTransform, LayEl.Bounds, 0.0f, V4(0, 0, 0, 1.0f));

            if(Bitmap)
            {
                PushBitmap(&DebugState->RenderGroup, &DebugState->BackingTransform, Event->Value_bitmap_id, BitmapScale,
                    V3(GetMinCorner(LayEl.Bounds), 1.0f), V4(1, 1, 1, 1), 0.0f);
            }
#endif
        } break;

        case DebugType_memory_arena_p:
        case DebugType_ArenaOccupancy:
        {
            Platform.UI.NkLayoutRowBegin(DebugState->nk, NK_STATIC, 30, 1);
            {
                Platform.UI.NkLayoutRowPush(DebugState->nk, 80);
                Platform.UI.NkLabel(DebugState->nk, GetName(Element), NK_TEXT_LEFT);

                Platform.UI.NkLayoutRowPush(DebugState->nk, 200);
                debug_element_frame *RootFrame = Element->Frames + DebugState->ViewingFrameOrdinal;
                debug_stored_event *Event = RootFrame->OldestEvent;
                if(Event)
                {
                    memory_arena *Arena = Event->Event.Value_memory_arena_p;
                    Platform.UI.NkProg(DebugState->nk, Arena->Used, Arena->Size, nk_false);
                }
            }
            Platform.UI.NkLayoutRowEnd(DebugState->nk);

        } break;

        case DebugType_ThreadIntervalGraph:
        case DebugType_FrameBarGraph:
        case DebugType_TopClocksList:
        {
            debug_view_profile_graph *Graph = &View->ProfileGraph;

            Platform.UI.NkLayoutRowBegin(DebugState->nk, NK_STATIC, 30, 4);
            {
                Platform.UI.NkLayoutRowPush(DebugState->nk, 80);
                if(Platform.UI.NkButtonLabel(DebugState->nk, "Root"))
                {
                    Graph->GUID = 0;
                }

                if(Platform.UI.NkButtonLabel(DebugState->nk, "Threads"))
                {
                    Element->Type = DebugType_ThreadIntervalGraph;
                }

                if(Platform.UI.NkButtonLabel(DebugState->nk, "Frames"))
                {
                    Element->Type = DebugType_FrameBarGraph;
                }

                if(Platform.UI.NkButtonLabel(DebugState->nk, "Clocks"))
                {
                    Element->Type = DebugType_TopClocksList;
                }
            }
            Platform.UI.NkLayoutRowEnd(DebugState->nk);
            
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
                    DrawProfileIn(DebugState, DebugID, ViewingElement);
                } break;

                case DebugType_FrameBarGraph:
                {
                    DrawFrameBars(DebugState, DebugID, ViewingElement);
                } break;
                
                case DebugType_TopClocksList:
                {
                    DrawTopClocksList(DebugState, DebugID, ViewingElement);
                } break;
            }
        } break;

        case DebugType_FrameSlider:
        {
            Platform.UI.NkLayoutRowBegin(DebugState->nk, NK_STATIC, 30, 3);
            {
                Platform.UI.NkLayoutRowPush(DebugState->nk, 108);
                if(Platform.UI.NkButtonLabel(DebugState->nk, "Pause"))
                {
                    DebugState->Paused = !DebugState->Paused;
                }

                if(Platform.UI.NkButtonLabel(DebugState->nk, "Oldest"))
                {
                    DebugState->ViewingFrameOrdinal = DebugState->OldestFrameOrdinal;
                }

                if(Platform.UI.NkButtonLabel(DebugState->nk, "Most Recent"))
                {
                    DebugState->ViewingFrameOrdinal = DebugState->MostRecentFrameOrdinal;
                }
            }
            Platform.UI.NkLayoutRowEnd(DebugState->nk);

            DrawFrameSlider(DebugState, DebugID, Element);
        } break;

        case DebugType_LastFrameInfo:
        {
            char Text[256];
            
            debug_frame *MostRecentFrame = DebugState->Frames + DebugState->ViewingFrameOrdinal;
            FormatString(sizeof(Text), Text, "Viewing frame time: %.02fms %de %dp %dd",
                         MostRecentFrame->WallSecondsElapsed * 1000.0f, MostRecentFrame->StoredEventCount,
                         MostRecentFrame->ProfileBlockCount, MostRecentFrame->DataBlockCount);

            Platform.UI.NkLayoutRowBegin(DebugState->nk, NK_STATIC, 30, 1);
            {
                Platform.UI.NkLayoutRowPush(DebugState->nk, 400);
                Platform.UI.NkLabel(DebugState->nk, Text, NK_TEXT_LEFT);
            }
            Platform.UI.NkLayoutRowEnd(DebugState->nk);
        } break;

        case DebugType_DebugMemoryInfo:
        {
            char Text[256];
            FormatString(sizeof(Text), Text, "Per-frame arena space remaining: %ukb",
                         (u32)(GetArenaSizeRemaining(&DebugState->PerFrameArena, AlignNoClear(1)) / 1024));
            Platform.UI.NkLayoutRowBegin(DebugState->nk, NK_STATIC, 30, 1);
            {
                Platform.UI.NkLayoutRowPush(DebugState->nk, 400);
                Platform.UI.NkLabel(DebugState->nk, Text, NK_TEXT_LEFT);
            }
            Platform.UI.NkLayoutRowEnd(DebugState->nk);
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

            Platform.UI.NkLayoutRowBegin(DebugState->nk, NK_STATIC, 30, 1);
            {
                Platform.UI.NkLayoutRowPush(DebugState->nk, 400);
                Platform.UI.NkLabel(DebugState->nk, Text, NK_TEXT_LEFT);
            }
            Platform.UI.NkLayoutRowEnd(DebugState->nk);
        } break;
    }
}

internal void
DrawTreeLink(debug_state *DebugState, debug_tree *Tree, debug_variable_link *Link)
{
    u32 FrameOrdinal = DebugState->ViewingFrameOrdinal;
    
    if(HasChildren(Link))
    {
        debug_id ID = DebugIDFromLink(Tree, Link);

        char *Text = Link->Name;
        int id = *(int *)ID.Value[1];
        if(NkTreePushId(Platform.UI, DebugState->nk, NK_TREE_NODE, Text, NK_MINIMIZED, id))
        {
            for(debug_variable_link *SubLink = Link->FirstChild;
                SubLink != GetSentinel(Link);
                SubLink = SubLink->Next)
            {
                DrawTreeLink(DebugState, Tree, SubLink);
            }

            Platform.UI.NkTreePop(DebugState->nk);
        }
    }
    else
    {
        debug_id DebugID = DebugIDFromLink(Tree, Link);
        DEBUGDrawElement(DebugState, Tree, Link->Element, DebugID, FrameOrdinal);
    }
}

internal void
DrawTrees(debug_state *DebugState)
{
    u32 FrameOrdinal = DebugState->ViewingFrameOrdinal;

    for(debug_tree *Tree = DebugState->TreeSentinel.Next;
        Tree != &DebugState->TreeSentinel;
        Tree = Tree->Next)
    {
        debug_variable_link *Group = Tree->Group;
        if(Group)
        {
            DrawTreeLink(DebugState, Tree, Group);
        }
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
#if 0
            if(ArenaHasRoomFor(&DebugState->PerFrameArena, sizeof(debug_stored_event)))
            {
                Result = PushStruct(&DebugState->PerFrameArena, debug_stored_event);
            }
            else
            {
                FreeOldestFrame(DebugState);
            }
#else
            Result = PushStruct(&DebugState->PerFrameArena, debug_stored_event);
#endif
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

internal debug_state *
DEBUGInit(u32 Width, u32 Height)
{
    debug_state *DebugState = BootstrapPushStruct(debug_state, DebugArena);
    
    DebugState->CollationFrameOrdinal = 1;

    DebugState->TreeSentinel.Next = &DebugState->TreeSentinel;
    DebugState->TreeSentinel.Prev = &DebugState->TreeSentinel;

    ZeroStruct(DebugState->DebugArena);
    ZeroStruct(DebugState->PerFrameArena);
        
    DebugState->RootGroup = CreateVariableLink(DebugState, 4, "Root");
    DebugState->RootInfoSize = 256;
    DebugState->RootGroup->Name = 
        DebugState->RootInfo = (char *)PushSize(&DebugState->DebugArena, 
                                                DebugState->RootInfoSize);
        
    DebugState->ProfileGroup = CreateVariableLink(DebugState, 7, "Profile");

    debug_event RootProfileEvent = {};
    RootProfileEvent.GUID = DEBUG_NAME("RootProfile");
    DebugState->RootProfileElement = GetElementFromEvent(DebugState, &RootProfileEvent, 0, 0);
        
    DebugState->Initialized = true;

    AddTree(DebugState, DebugState->RootGroup, V2(-0.5f*Width, 0.5f*Height));

    return(DebugState);
}


internal void
DEBUGStart(debug_state *DebugState)
{
    TIMED_FUNCTION();
    
    if(!DebugState->Paused)
    {
        DebugState->ViewingFrameOrdinal = DebugState->MostRecentFrameOrdinal;
    }
}

internal void
DEBUGEnd(debug_state *DebugState)
{
    TIMED_FUNCTION();

    debug_event *HotEvent = 0;
    
    debug_frame *MostRecentFrame = DebugState->Frames + DebugState->ViewingFrameOrdinal;
    FormatString(DebugState->RootInfoSize, DebugState->RootInfo, "%.02fms %de %dp %dd",
                 MostRecentFrame->WallSecondsElapsed * 1000.0f, MostRecentFrame->StoredEventCount,
                 MostRecentFrame->ProfileBlockCount, MostRecentFrame->DataBlockCount);

    nk_ui UI = Platform.UI;
    nk_context *nk = DebugState->nk;
    if (UI.NkBegin(nk, "Profiler", UI.NkRect(0, 0, 230, 250),
                   NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                   NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE|NK_WINDOW_CLOSABLE))
    {
        DrawTrees(DebugState);
    }
    UI.NkEnd(nk);
}

extern "C" DEBUG_EDITOR_FRAME_END(DEBUGEditorFrameEnd)
{   
    ZeroStruct(GlobalDebugTable->EditEvent);

    GlobalDebugTable->CurrentEventArrayIndex = !GlobalDebugTable->CurrentEventArrayIndex;    
    u64 ArrayIndex_EventIndex = AtomicExchangeU64(&GlobalDebugTable->EventArrayIndex_EventIndex,
                                                  (u64)GlobalDebugTable->CurrentEventArrayIndex << 32);

    u32 EventArrayIndex = ArrayIndex_EventIndex >> 32;
    Assert(EventArrayIndex <= 1);
    u32 EventCount = ArrayIndex_EventIndex & 0xFFFFFFFF;

    if(!Memory->DebugState)
    {
        Memory->DebugState = DEBUGInit(RenderCommands->Width, RenderCommands->Height);
        Memory->DebugState->nk = nk;
    }

    debug_state *DebugState = Memory->DebugState;
    if(DebugState)
    {
        DEBUGStart(DebugState);
        CollateDebugRecords(DebugState, EventCount, GlobalDebugTable->Events[EventArrayIndex]);
        DEBUGEnd(DebugState);
    }
}

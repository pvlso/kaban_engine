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
    nk_context *nk = DebugState->nk;
    r32 FrameSpan = (r32)(RootNode->Duration);
    r32 PixelSpan = ProfileRect.w;
    
    r32 Scale = 0.0f;
    if(FrameSpan > 0)
    {
        Scale = PixelSpan / FrameSpan;
    }

    const struct nk_input *in = nk->current->widgets_disabled ? 0 : &nk->input;
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
        struct nk_rect RegionRect = nk_rect(ThisMinX, LaneY, ThisMaxX, LaneHeight);

        nk_color C = {(u8)(Color.r*255.0f), (u8)(Color.g*255.0f), (u8)(Color.b*255.0f), 255};

        nk_flags ret = 0;

        if (!(nk->current->layout->flags & NK_WINDOW_ROM) && in &&
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

            nk_tooltip(nk, OnePastLastBackSlash);
        }
                    

        nk_fill_rect(&nk->current->buffer,
                     RegionRect, 0.0f, C);
        nk_stroke_rect(&nk->current->buffer,
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
DrawProfileIn(debug_state *DebugState, debug_id GraphID, debug_element *RootElement, r32 Height)
{
    nk_context *nk = DebugState->nk;
    nk_layout_row_dynamic(nk, Height, 1);

    struct nk_rect Rect = {};
    nk_widget(&Rect, nk);

    nk_fill_rect(&nk->current->buffer,
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
DrawFrameBars(debug_state *DebugState, debug_id GraphID, debug_element *RootElement, r32 Height)
{
    nk_context *nk = DebugState->nk;
    nk_layout_row_dynamic(nk, Height, 1);

    struct nk_rect Rect = {};
    nk_widget(&Rect, nk);
    
    u32 FrameCount = ArrayCount(RootElement->Frames);
    if(FrameCount > 0)
    {
        r32 BarWidth = (Rect.w / (r32)FrameCount);
        r32 AtX = Rect.x;

        const struct nk_input *in = nk->current->widgets_disabled ? 0 : &nk->input;
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

                    struct nk_rect RegionRectnk = nk_rect(AtX, ThisMinY, BarWidth, ThisMaxY);

                    nk_color C = {(u8)(Color.r*255.0f), (u8)(Color.g*255.0f), (u8)(Color.b*255.0f), 255};
                    nk_flags ret = 0;

                    if (!(nk->current->layout->flags & NK_WINDOW_ROM) && in &&
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

                        nk_tooltip(nk, OnePastLastBackSlash);
                    }
                    
                    nk_fill_rect(&nk->current->buffer,
                                           RegionRectnk, 0.0f, C);
                    nk_stroke_rect(&nk->current->buffer,
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

    nk_context *nk = DebugState->nk;
    nk_layout_row_dynamic(nk, 20, 1);
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
            nk_label(nk, TextBuffer, NK_TEXT_LEFT);
        }
    }
    
    EndTemporaryMemory(Temp);
}

internal void
DrawFrameSlider(debug_state *DebugState, debug_id SliderID, debug_element *RootElement)
{
    nk_context *nk = DebugState->nk;
    nk_layout_row_dynamic(nk, 5, 1);
    nk_spacer(nk);
    nk_layout_row_dynamic(nk, 60, 1);

    struct nk_rect Rect = {};
    nk_widget(&Rect, nk);

    u32 FrameCount = ArrayCount(RootElement->Frames);
    if(FrameCount > 0)
    {
        const struct nk_input *in = nk->current->widgets_disabled ? 0 : &nk->input;
        nk_fill_rect(&nk->current->buffer,
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
            struct nk_rect RegionRectnk = nk_rect(AtX - BarWidth, ThisMinY, BarWidth, ThisMaxY);
            
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

            if (!(nk->current->layout->flags & NK_WINDOW_ROM) && in &&
                NK_INBOX(in->mouse.pos.x,in->mouse.pos.y,RegionRectnk.x,RegionRectnk.y,RegionRectnk.w,RegionRectnk.h))
            {
                ret = NK_CHART_HOVERING;
                ret |= (!in->mouse.buttons[NK_BUTTON_LEFT].down &&
                        in->mouse.buttons[NK_BUTTON_LEFT].clicked) ? NK_CHART_CLICKED: 0;

                OC = {255, 0, 0, 120};

                char TextBuffer[256];
                FormatString(sizeof(TextBuffer), TextBuffer, "%u", FrameIndex);

                nk_tooltip(nk, TextBuffer);
            }

            if(Highlight)
            {
                nk_fill_rect(&nk->current->buffer,
                                       RegionRectnk, 0.0f, C);
            }

            nk_stroke_rect(&nk->current->buffer,
                           RegionRectnk, 0.0f, 0.15f, OC);

            if(ret & NK_CHART_CLICKED)
            {
                DebugState->ViewingFrameOrdinal = FrameIndex;
            }

            AtX += BarWidth;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): DEBUG WINDOW LAYOUT
//
//   frame rate (top left)
//   +- Profile ------------------------------+ +- Project ------------------+
//   | Pause Oldest Most Recent | Root Threads | | Stop Restart                |
//   | Frames Clocks                           | | status, builds / reloads    |
//   | frame slider                            | | build log of a failed build |
//   | viewing frame info                      | |                             |
//   | graph (fills the rest)                  | |                             |
//   | debug memory                            | |                             |
//   +-----------------------------------------+ +-----------------------------+
//
// Everything is in UI units, the window is UI_BASE_RESOLUTION_X x UI_BASE_RESOLUTION_Y.
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define DEBUG_WINDOW_ROW_HEIGHT 30.0f
#define DEBUG_PROFILE_WIDTH 1140.0f
#define DEBUG_PROJECT_WIDTH 730.0f

internal void
DrawFrameRate(debug_state *DebugState)
{
    nk_context *nk = DebugState->nk;

    // NOTE(pvlso): Always the newest frame, the profiler can be looking at an older one
    debug_frame *Frame = DebugState->Frames + DebugState->MostRecentFrameOrdinal;
    r32 Seconds = Frame->WallSecondsElapsed;

    char Text[64];
    FormatString(sizeof(Text), Text, "%.02fms  %.0f fps", 1000.0f*Seconds, (Seconds > 0.0f) ? (1.0f / Seconds) : 0.0f);
    nk_layout_row_static(nk, 24, 300, 1);
    nk_label(nk, Text, NK_TEXT_LEFT);
}

internal void
DrawProfiler(debug_state *DebugState, r32 Height)
{
    nk_context *nk = DebugState->nk;
    if(nk_group_begin(nk, "Profile", NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
    {
        nk_layout_row_begin(nk, NK_STATIC, DEBUG_WINDOW_ROW_HEIGHT, 8);
        {
            nk_layout_row_push(nk, 110);
            if(nk_button_label(nk, DebugState->Paused ? "Resume" : "Pause"))
            {
                DebugState->Paused = !DebugState->Paused;
            }
            if(nk_button_label(nk, "Oldest"))
            {
                DebugState->ViewingFrameOrdinal = DebugState->OldestFrameOrdinal;
            }
            if(nk_button_label(nk, "Most Recent"))
            {
                DebugState->ViewingFrameOrdinal = DebugState->MostRecentFrameOrdinal;
            }

            nk_layout_row_push(nk, 60);
            nk_spacer(nk);

            nk_layout_row_push(nk, 110);
            debug_view *View = GetOrCreateDebugViewFor(DebugState, DebugState->ProfileGraphID);
            if(nk_button_label(nk, "Root"))
            {
                View->ProfileGraph.GUID = 0;
            }

            debug_type GraphTypes[] = {DebugType_ThreadIntervalGraph, DebugType_FrameBarGraph, DebugType_TopClocksList};
            char *GraphNames[] = {"Threads", "Frames", "Clocks"};
            for(u32 GraphIndex = 0; GraphIndex < ArrayCount(GraphTypes); ++GraphIndex)
            {
                nk_bool Selected = (DebugState->ProfileGraphType == GraphTypes[GraphIndex]);
                if(nk_selectable_label(nk, GraphNames[GraphIndex], NK_TEXT_CENTERED, &Selected))
                {
                    DebugState->ProfileGraphType = GraphTypes[GraphIndex];
                }
            }
        }
        nk_layout_row_end(nk);

        DrawFrameSlider(DebugState, DebugState->ProfileGraphID, DebugState->RootProfileElement);

        char Text[256];
        debug_frame *ViewingFrame = DebugState->Frames + DebugState->ViewingFrameOrdinal;
        FormatString(sizeof(Text), Text, "Viewing frame: %.02fms, %d events, %d profile blocks, %d data blocks",
                     ViewingFrame->WallSecondsElapsed * 1000.0f, ViewingFrame->StoredEventCount,
                     ViewingFrame->ProfileBlockCount, ViewingFrame->DataBlockCount);
        nk_layout_row_dynamic(nk, DEBUG_WINDOW_ROW_HEIGHT, 1);
        nk_label(nk, Text, NK_TEXT_LEFT);

        debug_view *View = GetOrCreateDebugViewFor(DebugState, DebugState->ProfileGraphID);
        debug_element *ViewingElement = GetElementFromGUID(DebugState, View->ProfileGraph.GUID);
        if(!ViewingElement)
        {
            ViewingElement = DebugState->RootProfileElement;
        }

        // NOTE(pvlso): What's left after the title, the rows above and the memory line below
        r32 GraphHeight = Height - 6.0f*DEBUG_WINDOW_ROW_HEIGHT - 90.0f;
        switch(DebugState->ProfileGraphType)
        {
            case DebugType_ThreadIntervalGraph:
            {
                DrawProfileIn(DebugState, DebugState->ProfileGraphID, ViewingElement, GraphHeight);
            } break;

            case DebugType_FrameBarGraph:
            {
                DrawFrameBars(DebugState, DebugState->ProfileGraphID, ViewingElement, GraphHeight);
            } break;

            default:
            {
                nk_layout_row_dynamic(nk, GraphHeight, 1);
                if(nk_group_begin(nk, "Clocks", NK_WINDOW_BORDER))
                {
                    DrawTopClocksList(DebugState, DebugState->ProfileGraphID, ViewingElement);
                    nk_group_end(nk);
                }
            } break;
        }

        FormatString(sizeof(Text), Text, "Debug per-frame arena space remaining: %ukb",
                     (u32)(GetArenaSizeRemaining(&DebugState->PerFrameArena, AlignNoClear(1)) / 1024));
        nk_layout_row_dynamic(nk, DEBUG_WINDOW_ROW_HEIGHT, 1);
        nk_label(nk, Text, NK_TEXT_LEFT);

        nk_group_end(nk);
    }
}

internal void
DrawProjectPanel(editor_state *EditorState, nk_context *nk, r32 Height)
{
    if(nk_group_begin(nk, "Project", NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
    {
        if(EditorState && (EditorState->EditorMode == EditorMode_Game))
        {
            // NOTE(pvlso): The log gets what's left under the title, the buttons, status and counts
            DrawGameControls(EditorState->Game, nk, Height - 3.0f*DEBUG_WINDOW_ROW_HEIGHT - 80.0f);
        }
        else
        {
            nk_layout_row_dynamic(nk, DEBUG_WINDOW_ROW_HEIGHT, 1);
            nk_label(nk, "No project running, start one with Run on the title screen", NK_TEXT_LEFT);
        }

        nk_group_end(nk);
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

    DebugState->ProfileGraphType = DebugType_TopClocksList;
    DebugState->ProfileGraphID.Value[0] = DebugState;
    DebugState->ProfileGraphID.Value[1] = &DebugState->ProfileGraphType;

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
DEBUGEnd(debug_state *DebugState, engine_memory *Memory, engine_input *Input)
{
    TIMED_FUNCTION();

    debug_event *HotEvent = 0;
    
    debug_frame *MostRecentFrame = DebugState->Frames + DebugState->ViewingFrameOrdinal;
    FormatString(DebugState->RootInfoSize, DebugState->RootInfo, "%.02fms %de %dp %dd",
                 MostRecentFrame->WallSecondsElapsed * 1000.0f, MostRecentFrame->StoredEventCount,
                 MostRecentFrame->ProfileBlockCount, MostRecentFrame->DataBlockCount);

    Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
           (ArrayCount(Input->Controllers[0].Buttons)));

    nk_context *nk = DebugState->nk;

    if (nk_begin(nk, "Profiler", nk_rect(0, 0, (f32)UI_BASE_RESOLUTION_X, (f32)UI_BASE_RESOLUTION_Y),
                   NK_WINDOW_REMOVE_ROM|NK_WINDOW_NO_SCROLLBAR))
    {
        nk_window_show(nk, "Profiler", NK_SHOWN);

        DrawFrameRate(DebugState);

        // NOTE(pvlso): The panels take the height left under the frame rate
        r32 PanelHeight = (r32)UI_BASE_RESOLUTION_Y - 60.0f;
        nk_layout_row_begin(nk, NK_STATIC, PanelHeight, 2);
        {
            nk_layout_row_push(nk, DEBUG_PROFILE_WIDTH);
            DrawProfiler(DebugState, PanelHeight);

            // NOTE(pvlso): The running project's dev controls live here, the main window is all game
            nk_layout_row_push(nk, DEBUG_PROJECT_WIDTH);
            DrawProjectPanel(Memory->EditorState, nk, PanelHeight);
        }
        nk_layout_row_end(nk);
    }
    nk_end(nk);
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
        DEBUGEnd(DebugState, Memory, Input);
    }
}

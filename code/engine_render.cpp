/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal void
SortEntries(editor_render_commands *Commands, memory_arena *TempArena, editor_render_prep *Prep)
{
    u32 Count = Commands->PushBufferElementCount;
    sort_entry *Entries = (sort_entry *)(Commands->PushBufferBase + Commands->SortEntryAt);

    sort_entry *Temp = PushArray(TempArena, Count, sort_entry);

    RadixSort(Count, Entries, Temp);
    
#if 0
    if(Count)
    {
        for(u32 Index = 0;
            Index < (Count - 1);
            ++Index)
        {
            sort_entry *EntryA = Entries + Index;
            sort_entry *EntryB = EntryA + 1;

            Assert(EntryA->SortKey <= EntryB->SortKey);
        }
    }
#endif
}

internal render_entry_cliprect *
LinearizeClipRects(editor_render_commands *Commands, memory_arena *TempArena)
{
    // TODO(casey): Collapse this with above!
    render_entry_cliprect *Result = PushArray(TempArena, Commands->ClipRectCount,
                                              render_entry_cliprect);

    render_entry_cliprect *Out = Result;
    for(render_entry_cliprect *Rect = Commands->FirstRect;
        Rect;
        Rect = Rect->Next)
    {
        *Out++ = *Rect;
    }

    return(Result);
}

internal editor_render_prep
PrepForRender(editor_render_commands *Commands, memory_arena *TempArena)
{
    editor_render_prep Prep;
    SortEntries(Commands, TempArena, &Prep);
    Prep.ClipRects = LinearizeClipRects(Commands, TempArena);

    return(Prep);
}

internal rectangle2i
AspectRatioFit(u32 RenderWidth, u32 RenderHeight,
               u32 WindowWidth, u32 WindowHeight)
{
    rectangle2i Result = {};

    if((RenderWidth > 0) && (RenderHeight > 0) &&
       (WindowWidth > 0) && (WindowHeight > 0))
    {
        r32 OptimalWindowWidth = (r32)WindowHeight * ((r32)RenderWidth / (r32)RenderHeight);
        r32 OptimalWindowHeight = (r32)WindowWidth * ((r32)RenderHeight / (r32)RenderWidth);

        if(OptimalWindowWidth > (r32)WindowWidth)
        {
            // NOTE(casey): Width-constrained display - top and bottom black bars
            Result.MinX = 0;
            Result.MaxX = WindowWidth;

            r32 Empty = (r32)WindowHeight - OptimalWindowHeight;
            s32 HalfEmpty = RoundReal32ToInt32(0.5f*Empty);
            s32 UseHeight = RoundReal32ToInt32(OptimalWindowHeight);

            Result.MinY = HalfEmpty;
            Result.MaxY = Result.MinY + UseHeight;
        }
        else
        {
            // NOTE(casey): Height-constrained display - left and right black bars
            Result.MinY = 0;
            Result.MaxY = WindowHeight;

            r32 Empty = (r32)WindowWidth - OptimalWindowWidth;
            s32 HalfEmpty = RoundReal32ToInt32(0.5f*Empty);
            s32 UseWidth = RoundReal32ToInt32(OptimalWindowWidth);

            Result.MinX = HalfEmpty;
            Result.MaxX = Result.MinX + UseWidth;
        }
    }
    
    return(Result);
}

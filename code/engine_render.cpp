/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline v4
Unpack4x8(uint32 Packed)
{
    v4 Result = {(real32)((Packed >> 16) & 0xFF),
                 (real32)((Packed >> 8) & 0xFF),
                 (real32)((Packed >> 0) & 0xFF),
                 (real32)((Packed >> 24) & 0xFF)};

    return(Result);
}

inline u32
Pack4x8(v4 Unpacked)
{
    u32 Result = ((RoundReal32ToUInt32(Unpacked.a) << 24) |
                  (RoundReal32ToUInt32(Unpacked.r) << 16) |
                  (RoundReal32ToUInt32(Unpacked.g) << 8) |
                  (RoundReal32ToUInt32(Unpacked.b) << 0));

    return(Result);
}

inline v4
UnscaleAndBiasNormal(v4 Normal)
{
    v4 Result;

    real32 Inv255 = 1.0f / 255.0f;

    Result.x = -1.0f + 2.0f*(Inv255*Normal.x);
    Result.y = -1.0f + 2.0f*(Inv255*Normal.y);
    Result.z = -1.0f + 2.0f*(Inv255*Normal.z);

    Result.w = Inv255*Normal.w;

    return(Result);
}

struct bilinear_sample
{
    uint32 A, B, C, D;
};
inline bilinear_sample
BilinearSample(loaded_bitmap *Texture, int32 X, int32 Y)
{
    bilinear_sample Result;
    
    uint8 *TexelPtr = ((uint8 *)Texture->Memory) + Y*Texture->Pitch + X*sizeof(uint32);
    Result.A = *(uint32 *)(TexelPtr);
    Result.B = *(uint32 *)(TexelPtr + sizeof(uint32));
    Result.C = *(uint32 *)(TexelPtr + Texture->Pitch);
    Result.D = *(uint32 *)(TexelPtr + Texture->Pitch + sizeof(uint32));

    return(Result);
}

inline v4
SRGBBilinearBlend(bilinear_sample TexelSample, real32 fX, real32 fY)
{
    v4 TexelA = Unpack4x8(TexelSample.A);
    v4 TexelB = Unpack4x8(TexelSample.B);
    v4 TexelC = Unpack4x8(TexelSample.C);
    v4 TexelD = Unpack4x8(TexelSample.D);

    // NOTE(casey): Go from sRGB to "linear" brightness space
    TexelA = SRGB255ToLinear1(TexelA);
    TexelB = SRGB255ToLinear1(TexelB);
    TexelC = SRGB255ToLinear1(TexelC);
    TexelD = SRGB255ToLinear1(TexelD);

    v4 Result = Lerp(Lerp(TexelA, fX, TexelB),
                     fY,
                     Lerp(TexelC, fX, TexelD));

    return(Result);
}

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

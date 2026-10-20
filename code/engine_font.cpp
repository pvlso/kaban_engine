/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): stb_truetype's own allocations (the packer's scratch memory) go through the platform
#define STBTT_malloc(Size, User) ((void)(User), Platform.AllocateMemory(Size))
#define STBTT_free(Memory, User) ((void)(User), Platform.DeallocateMemory(Memory))
// NOTE(pvlso): Assert is a bare if, stb_truetype uses its assert inside if/else chains
#define STBTT_assert(Expression) do {Assert(Expression);} while(0)
#define STB_TRUETYPE_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include "stb_truetype.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// NOTE(pvlso): The running game's fonts, set by UpdateAndRenderGame around the game's update.
// The engine_api font calls go through it, 0 while no game runs.
global_variable game_fonts *GlobalGameFonts;

inline u32
FontGlyphIndex(u32 CodePoint)
{
    u32 Result = '?' - FONT_ASCII_FIRST;
    if((CodePoint >= FONT_ASCII_FIRST) && (CodePoint < (FONT_ASCII_FIRST + FONT_ASCII_COUNT)))
    {
        Result = CodePoint - FONT_ASCII_FIRST;
    }
    else if((CodePoint >= FONT_LATIN1_FIRST) && (CodePoint < (FONT_LATIN1_FIRST + FONT_LATIN1_COUNT)))
    {
        Result = FONT_ASCII_COUNT + (CodePoint - FONT_LATIN1_FIRST);
    }

    return(Result);
}

// NOTE(pvlso): The code point that's really drawn for CodePoint ('?' when it isn't baked)
inline u32
FontDrawnCodePoint(u32 CodePoint)
{
    u32 Index = FontGlyphIndex(CodePoint);
    u32 Result = (Index < FONT_ASCII_COUNT) ? (FONT_ASCII_FIRST + Index) : (FONT_LATIN1_FIRST + (Index - FONT_ASCII_COUNT));
    return(Result);
}

// NOTE(pvlso): Reads one UTF-8 code point and moves At past it. A broken sequence reads as '?' and
// stops at the byte that broke it, so it never runs past the terminator.
internal u32
DecodeUTF8(char **At)
{
    u8 *Bytes = (u8 *)*At;
    u32 Result = '?';
    u32 Length = 1;
    if(Bytes[0] < 0x80)
    {
        Result = Bytes[0];
    }
    else if((Bytes[0] & 0xE0) == 0xC0) {Length = 2;}
    else if((Bytes[0] & 0xF0) == 0xE0) {Length = 3;}
    else if((Bytes[0] & 0xF8) == 0xF0) {Length = 4;}

    if(Length > 1)
    {
        u32 Value = Bytes[0] & (0x7F >> Length);
        u32 Index = 1;
        while((Index < Length) && ((Bytes[Index] & 0xC0) == 0x80))
        {
            Value = (Value << 6) | (Bytes[Index] & 0x3F);
            ++Index;
        }

        if(Index == Length)
        {
            Result = Value;
        }
        Length = Index;
    }

    *At += Length;
    return(Result);
}

inline s32
FontPixelSize(r32 Pixels)
{
    s32 Result = RoundReal32ToInt32(Pixels);
    Result = Maximum(FONT_MIN_PIXEL_SIZE, Minimum(Result, FONT_MAX_PIXEL_SIZE));
    return(Result);
}

internal void
LoadFontFile(engine_font *Font, char *FileName)
{
    FormatString(sizeof(Font->FileName), Font->FileName, "%s", FileName);

    // NOTE(pvlso): ReadEntireFile logs a missing file itself
    read_file_result File = Platform.ReadEntireFile(FileName, PlatformFileType_TTF, 0, false);
    if(File.Contents)
    {
        u8 *Data = (u8 *)File.Contents;
        s32 Offset = stbtt_GetFontOffsetForIndex(Data, 0);
        if((Offset >= 0) && stbtt_InitFont(&Font->Info, Data, Offset))
        {
            Font->FileMemory = File.Contents;
            stbtt_GetFontVMetrics(&Font->Info, &Font->Ascent, &Font->Descent, &Font->LineGap);
            LOG("Font %s loaded", FileName);
        }
        else
        {
            Platform.FreeFileMemory(File.Contents);
            LOG_WARNING("%s isn't a font stb_truetype can read", FileName);
        }
    }
}

// NOTE(pvlso): Packs the glyphs at PixelSize into the smallest atlas they fit and queues its upload.
// A size that doesn't bake keeps an empty atlas, so it isn't tried again every frame.
internal font_size *
BakeFontSize(engine_font *Font, s32 PixelSize, platform_texture_op_queue *TextureOpQueue)
{
    TIMED_FUNCTION();

    font_size *Result = (font_size *)Platform.AllocateMemory(sizeof(font_size));
    if(Result)
    {
        ZeroStruct(*Result);
        Result->PixelSize = PixelSize;

        s32 AtlasDims[][2] =
        {
            {128, 128}, {256, 128}, {256, 256}, {512, 256}, {512, 512},
            {1024, 512}, {1024, 1024}, {2048, 1024}, {2048, 2048},
        };

        u8 *Coverage = 0;
        s32 Width = 0;
        s32 Height = 0;
        for(u32 DimIndex = 0; !Coverage && (DimIndex < ArrayCount(AtlasDims)); ++DimIndex)
        {
            Width = AtlasDims[DimIndex][0];
            Height = AtlasDims[DimIndex][1];
            Coverage = (u8 *)Platform.AllocateMemory(Width*Height);
            if(Coverage)
            {
                b32 Packed = false;
                stbtt_pack_context Context;
                if(stbtt_PackBegin(&Context, Coverage, Width, Height, 0, 1, 0))
                {
                    stbtt_pack_range Ranges[2] = {};
                    Ranges[0].font_size = (r32)PixelSize;
                    Ranges[0].first_unicode_codepoint_in_range = FONT_ASCII_FIRST;
                    Ranges[0].num_chars = FONT_ASCII_COUNT;
                    Ranges[0].chardata_for_range = Result->Glyphs;
                    Ranges[1].font_size = (r32)PixelSize;
                    Ranges[1].first_unicode_codepoint_in_range = FONT_LATIN1_FIRST;
                    Ranges[1].num_chars = FONT_LATIN1_COUNT;
                    Ranges[1].chardata_for_range = Result->Glyphs + FONT_ASCII_COUNT;

                    Packed = stbtt_PackFontRanges(&Context, (u8 *)Font->FileMemory, 0, Ranges, ArrayCount(Ranges));
                    stbtt_PackEnd(&Context);
                }

                if(!Packed)
                {
                    Platform.DeallocateMemory(Coverage);
                    Coverage = 0;
                }
            }
        }

        if(Coverage)
        {
            u32 *Pixels = (u32 *)Platform.AllocateMemory(Width*Height*sizeof(u32));
            if(Pixels)
            {
                // NOTE(pvlso): Rows bottom up like the bitmaps, premultiplied white: the coverage is the alpha and,
                // through the sRGB texture, the linear colour
                for(s32 Y = 0; Y < Height; ++Y)
                {
                    u8 *Source = Coverage + (Height - 1 - Y)*Width;
                    u32 *Dest = Pixels + Y*Width;
                    for(s32 X = 0; X < Width; ++X)
                    {
                        u32 Alpha = Source[X];
                        u32 Color = (u32)(255.0f*SquareRoot((r32)Alpha / 255.0f) + 0.5f);
                        Dest[X] = (Alpha << 24) | (Color << 16) | (Color << 8) | Color;
                    }
                }

                loaded_bitmap *Atlas = &Result->Atlas;
                Atlas->Memory = Pixels;
                Atlas->Width = Width;
                Atlas->Height = Height;
                Atlas->Pitch = Width*sizeof(u32);
                Atlas->WidthOverHeight = (r32)Width / (r32)Height;

                texture_op Op = {};
                Op.IsAllocate = true;
                Op.Allocate.Width = Width;
                Op.Allocate.Height = Height;
                Op.Allocate.Data = Pixels;
                Op.Allocate.ResultHandle = &Atlas->TextureHandle;
                AddOp(TextureOpQueue, &Op);
            }
            Platform.DeallocateMemory(Coverage);
        }

        if(Result->Atlas.Memory)
        {
            LOG("Font %s: %dpx baked into a %dx%d atlas", Font->FileName, PixelSize, Width, Height);
        }
        else
        {
            LOG_WARNING("Font %s: %dpx doesn't fit a %dx%d atlas", Font->FileName, PixelSize, 2048, 2048);
        }
    }

    return(Result);
}

// NOTE(pvlso): The baked size, baking it the first time. A font with all its sizes taken gets the
// nearest baked one (drawn scaled).
internal font_size *
GetFontSize(engine_font *Font, s32 PixelSize)
{
    font_size *Result = 0;
    for(u32 SizeIndex = 0; SizeIndex < Font->SizeCount; ++SizeIndex)
    {
        if(Font->Sizes[SizeIndex]->PixelSize == PixelSize)
        {
            Result = Font->Sizes[SizeIndex];
            break;
        }
    }

    if(!Result)
    {
        if((Font->SizeCount < MAX_FONT_SIZES) && GlobalGameFonts)
        {
            Result = BakeFontSize(Font, PixelSize, GlobalGameFonts->TextureOpQueue);
            if(Result)
            {
                Font->Sizes[Font->SizeCount++] = Result;
            }
        }
        else
        {
            if(!Font->WarnedSizesFull)
            {
                LOG_WARNING("Font %s has %d sizes baked, %dpx is drawn with the nearest one", Font->FileName,
                            MAX_FONT_SIZES, PixelSize);
                Font->WarnedSizesFull = true;
            }

            for(u32 SizeIndex = 0; SizeIndex < Font->SizeCount; ++SizeIndex)
            {
                font_size *Size = Font->Sizes[SizeIndex];
                if(Size->Atlas.Memory &&
                   (!Result || (AbsoluteValue((r32)(Size->PixelSize - PixelSize)) < AbsoluteValue((r32)(Result->PixelSize - PixelSize)))))
                {
                    Result = Size;
                }
            }
        }
    }

    return(Result);
}

// NOTE(pvlso): Width in pixels of the text from At to the next '\n' or the end, Scale is pixels per font unit
internal r32
MeasureFontLine(engine_font *Font, r32 Scale, char *At)
{
    r32 Result = 0.0f;
    u32 Previous = 0;
    while(*At && (*At != '\n'))
    {
        u32 CodePoint = FontDrawnCodePoint(DecodeUTF8(&At));
        if(Previous)
        {
            Result += Scale*(r32)stbtt_GetCodepointKernAdvance(&Font->Info, Previous, CodePoint);
        }

        s32 Advance;
        s32 LeftSideBearing;
        stbtt_GetCodepointHMetrics(&Font->Info, CodePoint, &Advance, &LeftSideBearing);
        Result += Scale*(r32)Advance;
        Previous = CodePoint;
    }

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): ENGINE API side
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal engine_font *
GameGetFont(char *FileName)
{
    engine_font *Result = 0;

    game_fonts *Fonts = GlobalGameFonts;
    if(Fonts && FileName && FileName[0])
    {
        engine_font *Font = 0;
        for(u32 FontIndex = 0; FontIndex < Fonts->FontCount; ++FontIndex)
        {
            if(StringsAreEqual(Fonts->Fonts[FontIndex]->FileName, FileName))
            {
                Font = Fonts->Fonts[FontIndex];
                break;
            }
        }

        if(!Font)
        {
            if(Fonts->FontCount < MAX_GAME_FONTS)
            {
                Font = (engine_font *)Platform.AllocateMemory(sizeof(engine_font));
                if(Font)
                {
                    ZeroStruct(*Font);
                    Fonts->Fonts[Fonts->FontCount++] = Font;
                    LoadFontFile(Font, FileName);
                }
            }
            else if(!Fonts->WarnedFontsFull)
            {
                LOG_WARNING("A game can load %d fonts, %s isn't loaded", MAX_GAME_FONTS, FileName);
                Fonts->WarnedFontsFull = true;
            }
        }

        if(Font && Font->FileMemory)
        {
            Result = Font;
        }
    }

    return(Result);
}

internal void
GamePushText(render_group *Group, object_transform *ObjectTransform, engine_font *Font, char *Text, v3 P,
             r32 Height, v4 Color, text_align Align)
{
    if(!Font || !Font->FileMemory || !Text)
    {
        return;
    }

    entity_basis_p_result Basis = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P);
    if(!Basis.Valid)
    {
        return;
    }

    s32 PixelSize = FontPixelSize(Height*Basis.Scale);
    font_size *Size = GetFontSize(Font, PixelSize);
    if(!Size || !Size->Atlas.TextureHandle)
    {
        // NOTE(pvlso): Its atlas goes up at the end of this frame
        ++Group->MissingResourceCount;
        return;
    }

    // NOTE(pvlso): Fit is 1 unless the font ran out of sizes and a nearby one is scaled
    r32 Fit = (r32)PixelSize / (r32)Size->PixelSize;
    r32 Scale = stbtt_ScaleForPixelHeight(&Font->Info, (r32)PixelSize);
    r32 LineAdvance = (r32)RoundReal32ToInt32(Scale*(r32)(Font->Ascent - Font->Descent + Font->LineGap));
    r32 InvAtlasWidth = 1.0f / (r32)Size->Atlas.Width;
    r32 InvAtlasHeight = 1.0f / (r32)Size->Atlas.Height;

    // NOTE(pvlso): The atlas is premultiplied, the render group's colours aren't
    v4 TextColor = SRGB1ToLinear1(Color);
    TextColor.rgb *= TextColor.a;

    // NOTE(pvlso): Glyphs land on whole pixels, the atlas is sampled 1:1 with nearest filtering
    r32 BaselineY = (r32)RoundReal32ToInt32(Basis.P.y);
    char *At = Text;
    for(;;)
    {
        r32 PenX = Basis.P.x;
        if(Align != TextAlign_Left)
        {
            r32 LineWidth = MeasureFontLine(Font, Scale, At);
            PenX -= (Align == TextAlign_Center) ? 0.5f*LineWidth : LineWidth;
        }

        u32 Previous = 0;
        while(*At && (*At != '\n'))
        {
            u32 CodePoint = FontDrawnCodePoint(DecodeUTF8(&At));
            if(Previous)
            {
                PenX += Scale*(r32)stbtt_GetCodepointKernAdvance(&Font->Info, Previous, CodePoint);
            }

            stbtt_packedchar *Glyph = Size->Glyphs + FontGlyphIndex(CodePoint);
            if((Glyph->x1 > Glyph->x0) && (Glyph->y1 > Glyph->y0))
            {
                render_entry_bitmap *Entry = PushRenderElement(Group, render_entry_bitmap, Basis.SortKey);
                if(Entry)
                {
                    Entry->Bitmap = &Size->Atlas;
                    Entry->Color = TextColor;
                    Entry->P = V2((r32)RoundReal32ToInt32(PenX + Fit*Glyph->xoff),
                                  (r32)RoundReal32ToInt32(BaselineY - Fit*Glyph->yoff2));
                    Entry->Size = Fit*V2((r32)(Glyph->x1 - Glyph->x0), (r32)(Glyph->y1 - Glyph->y0));
                    // NOTE(pvlso): The glyph rectangle is top down in the atlas, the rows are stored bottom up
                    Entry->MinUV = V2(InvAtlasWidth*(r32)Glyph->x0, 1.0f - InvAtlasHeight*(r32)Glyph->y1);
                    Entry->MaxUV = V2(InvAtlasWidth*(r32)Glyph->x1, 1.0f - InvAtlasHeight*(r32)Glyph->y0);
                    Entry->RotateAngle = 0.0f;
                }
            }

            s32 Advance;
            s32 LeftSideBearing;
            stbtt_GetCodepointHMetrics(&Font->Info, CodePoint, &Advance, &LeftSideBearing);
            PenX += Scale*(r32)Advance;
            Previous = CodePoint;
        }

        if(*At != '\n')
        {
            break;
        }
        ++At;
        BaselineY -= LineAdvance;
    }
}

internal text_dim
GameGetTextDim(render_group *Group, engine_font *Font, char *Text, r32 Height)
{
    text_dim Result = {};

    r32 PixelsPerMeter = Group->CameraTransform.MetersToPixels;
    if(Font && Font->FileMemory && Text && (PixelsPerMeter > 0.0f))
    {
        s32 PixelSize = FontPixelSize(Height*PixelsPerMeter);
        r32 Scale = stbtt_ScaleForPixelHeight(&Font->Info, (r32)PixelSize);
        r32 MetersPerPixel = 1.0f / PixelsPerMeter;

        Result.Ascent = MetersPerPixel*Scale*(r32)Font->Ascent;
        Result.Descent = MetersPerPixel*Scale*(r32)Font->Descent;
        Result.LineAdvance = MetersPerPixel*(r32)RoundReal32ToInt32(Scale*(r32)(Font->Ascent - Font->Descent + Font->LineGap));

        r32 Widest = 0.0f;
        char *At = Text;
        for(;;)
        {
            Widest = Maximum(Widest, MeasureFontLine(Font, Scale, At));
            ++Result.LineCount;
            while(*At && (*At != '\n'))
            {
                ++At;
            }

            if(!*At)
            {
                break;
            }
            ++At;
        }
        Result.Width = MetersPerPixel*Widest;
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// NOTE(pvlso): Queues a delete for every atlas texture and frees the fonts. Their uploads have run by
// now, Stop takes two frames.
internal void
FreeGameFonts(game_fonts *Fonts, platform_texture_op_queue *TextureOpQueue)
{
    u32 TextureCount = 0;
    for(u32 FontIndex = 0; FontIndex < Fonts->FontCount; ++FontIndex)
    {
        engine_font *Font = Fonts->Fonts[FontIndex];
        for(u32 SizeIndex = 0; SizeIndex < Font->SizeCount; ++SizeIndex)
        {
            font_size *Size = Font->Sizes[SizeIndex];
            if(Size->Atlas.TextureHandle)
            {
                texture_op Op = {};
                Op.IsAllocate = false;
                Op.Deallocate.Handle = Size->Atlas.TextureHandle;
                AddOp(TextureOpQueue, &Op);
                ++TextureCount;
            }
            Platform.DeallocateMemory(Size->Atlas.Memory);
            Platform.DeallocateMemory(Size);
        }

        Platform.FreeFileMemory(Font->FileMemory);
        Platform.DeallocateMemory(Font);
    }

    if(Fonts->FontCount)
    {
        LOG("Freed %u font(s) and %u atlas texture(s)", Fonts->FontCount, TextureCount);
    }
    ZeroStruct(*Fonts);
}

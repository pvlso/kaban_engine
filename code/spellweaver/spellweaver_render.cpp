/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

#define IGNORED_TIMED_FUNCTION(...)
#define IGNORED_TIMED_BLOCK(...)


#define mmSquare(a) _mm_mul_ps(a, a)    
#define M(a, i) ((float *)&(a))[i]
#define Mi(a, i) ((uint32 *)&(a))[i]


global_variable b32 Global_Renderer_ShowLightingSamples = false;

struct tile_render_work
{
    game_render_commands *Commands;
    loaded_bitmap *OutputTarget;
    rectangle2i ClipRect;
};

inline v4
Unpack4x8(uint32 Packed)
{
    v4 Result = {(real32)((Packed >> 16) & 0xFF),
                 (real32)((Packed >> 8) & 0xFF),
                 (real32)((Packed >> 0) & 0xFF),
                 (real32)((Packed >> 24) & 0xFF)};

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

internal void
DrawFetchlessRect(loaded_bitmap *Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
                  loaded_bitmap *Texture, rectangle2i ClipRect)
{
    // NOTE(casey): Premultiply color up front
    Color.rgb *= Color.a;

    real32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
    real32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);
    
    rectangle2i FillRect = InvertedInfinityRectangle2i();

    v2 P[4] = {Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis};
    for(int PIndex = 0;
        PIndex < ArrayCount(P);
        ++PIndex)
    {
        v2 TestP = P[PIndex];
        int FloorX = FloorReal32ToInt32(TestP.x);
        int CeilX = CeilReal32ToInt32(TestP.x);
        int FloorY = FloorReal32ToInt32(TestP.y);
        int CeilY = CeilReal32ToInt32(TestP.y);

        if(FillRect.MinX > FloorX) {FillRect.MinX = FloorX;}
        if(FillRect.MinY > FloorY) {FillRect.MinY = FloorY;}
        if(FillRect.MaxX < CeilX) {FillRect.MaxX = CeilX;}
        if(FillRect.MaxY < CeilY) {FillRect.MaxY = CeilY;}
    }

    if(HasArea(FillRect))
    {
        __m128i StartClipMask = _mm_set1_epi8(-1);
        __m128i EndClipMask = _mm_set1_epi8(-1);

        __m128i StartClipMasks[] =
            {
                _mm_slli_si128(StartClipMask, 0*4),
                _mm_slli_si128(StartClipMask, 1*4),
                _mm_slli_si128(StartClipMask, 2*4),
                _mm_slli_si128(StartClipMask, 3*4),
            };

        __m128i EndClipMasks[] =
            {
                _mm_srli_si128(EndClipMask, 0*4),
                _mm_srli_si128(EndClipMask, 3*4),
                _mm_srli_si128(EndClipMask, 2*4),
                _mm_srli_si128(EndClipMask, 1*4),
            };

        if(FillRect.MinX & 3)
        {
            StartClipMask = StartClipMasks[FillRect.MinX & 3];
            FillRect.MinX = FillRect.MinX & ~3;
        }

        if(FillRect.MaxX & 3)
        {
            EndClipMask = EndClipMasks[FillRect.MaxX & 3];
            FillRect.MaxX = (FillRect.MaxX & ~3) + 4;
        }

        v2 nXAxis = InvXAxisLengthSq*(XAxis - V2(1.0f, 0.0f));
        v2 nYAxis = InvYAxisLengthSq*(YAxis - V2(0.0f, 1.0f));

        __m128 One = _mm_set1_ps(1.0f);
        __m128 Zero = _mm_set1_ps(0.0f);
        __m128 Four_4x = _mm_set1_ps(4.0f);
        __m128 One255_4x = _mm_set1_ps(255.0f);

        __m128i MaskFF = _mm_set1_epi32(0xFF);
        real32 Inv255 = 1.0f / 255.0f;
        __m128 Inv255_4x = _mm_set1_ps(Inv255);
        __m128 Colorr_4x = _mm_set1_ps(Color.r);
        __m128 Colorg_4x = _mm_set1_ps(Color.g);
        __m128 Colorb_4x = _mm_set1_ps(Color.b);
        __m128 Colora_4x = _mm_set1_ps(Color.a);
        __m128 MaxColorValue = _mm_set1_ps(255.0f*255.0f);

        __m128 nXAxisx_4x = _mm_set1_ps(nXAxis.x);
        __m128 nXAxisy_4x = _mm_set1_ps(nXAxis.y);
        __m128 nYAxisx_4x = _mm_set1_ps(nYAxis.x);
        __m128 nYAxisy_4x = _mm_set1_ps(nYAxis.y);

        __m128 Originx_4x = _mm_set1_ps(Origin.x);
        __m128 Originy_4x = _mm_set1_ps(Origin.y);

        __m128 TextureWidth_4x = _mm_set1_ps((real32)(Texture->Width));
        __m128 TextureHeight_4x = _mm_set1_ps((real32)(Texture->Height));
        __m128i TexturePitch_4x = _mm_set1_epi32(Texture->Pitch);

        uint8 *Row = ((uint8 *)Buffer->Memory +
                      FillRect.MinX*BITMAP_BYTES_PER_PIXEL +
                      FillRect.MinY*Buffer->Pitch);

        void *TextureMemory = Texture->Memory;
        int32 RowAdvance = Buffer->Pitch;
    
        int MinY = FillRect.MinY;
        int MaxY = FillRect.MaxY;
        int MinX = FillRect.MinX;
        int MaxX = FillRect.MaxX;
        for(int Yi = MinY;
            Yi < MaxY;
            ++Yi)
        {
            __m128 PixelPy = _mm_set1_ps((real32)Yi);
            __m128 PixelPx = _mm_set_ps((real32)(MinX + 3),
                                        (real32)(MinX + 2),
                                        (real32)(MinX + 1),
                                        (real32)(MinX + 0));

            PixelPy = _mm_sub_ps(PixelPy, Originy_4x); 
            PixelPx = _mm_sub_ps(PixelPx, Originx_4x); 

            __m128 PynX = _mm_mul_ps(PixelPy, nYAxisx_4x);
            __m128 PynY = _mm_mul_ps(PixelPy, nYAxisy_4x);

            __m128i ClipMask = StartClipMask;

            uint32 *Pixel = (uint32 *)Row;
            for(int Xi = MinX;
                Xi < MaxX;
                Xi += 4)
            {
                __m128i OriginalDest = _mm_load_si128((__m128i *)Pixel);

                __m128 PxnX = _mm_mul_ps(PixelPx, nXAxisx_4x);
                __m128 PxnY = _mm_mul_ps(PixelPx, nXAxisy_4x);

                // NOTE(paul): Calculate coordinates in texture in range from 0 to 1
                __m128 U = _mm_add_ps(PxnX, PynX);
                __m128 V = _mm_add_ps(PxnY, PynY);

                // NOTE(paul): Clamp U and V to range from 0 to 1
                U = _mm_min_ps(_mm_max_ps(U, Zero), One);
                V = _mm_min_ps(_mm_max_ps(V, Zero), One);

                __m128i WriteMask = _mm_castps_si128(_mm_and_ps(_mm_and_ps(_mm_cmpge_ps(U, Zero),
                                                                           _mm_cmple_ps(U, One)),
                                                                _mm_and_ps(_mm_cmpge_ps(V, Zero),
                                                                           _mm_cmple_ps(V, One)))); 
                WriteMask = _mm_and_si128(WriteMask, ClipMask);

                __m128 tX = _mm_mul_ps(U, TextureWidth_4x);
                __m128 tY = _mm_mul_ps(V, TextureHeight_4x);

                __m128i FetchX_4x = _mm_cvtps_epi32(tX);
                __m128i FetchY_4x = _mm_cvtps_epi32(tY);

                FetchX_4x = _mm_slli_epi32(FetchX_4x, 2);
                FetchY_4x = _mm_mullo_epi32(FetchY_4x, TexturePitch_4x);
                __m128i Fetch_4x = _mm_add_epi32(FetchX_4x, FetchY_4x);

                int32 Fetch0 = Mi(Fetch_4x, 0);
                int32 Fetch1 = Mi(Fetch_4x, 1);
                int32 Fetch2 = Mi(Fetch_4x, 2);
                int32 Fetch3 = Mi(Fetch_4x, 3);
                
                uint8 *TexelPtr0 = ((uint8 *)TextureMemory + Fetch0);
                uint8 *TexelPtr1 = ((uint8 *)TextureMemory + Fetch1);
                uint8 *TexelPtr2 = ((uint8 *)TextureMemory + Fetch2);
                uint8 *TexelPtr3 = ((uint8 *)TextureMemory + Fetch3);

                __m128i Texel = _mm_setr_epi32(*(uint32 *)(TexelPtr0),
                                               *(uint32 *)(TexelPtr1),
                                               *(uint32 *)(TexelPtr2),
                                               *(uint32 *)(TexelPtr3));

                __m128 Texelr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Texel, 16), MaskFF));
                __m128 Texelg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Texel, 8), MaskFF));
                __m128 Texelb = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Texel, 0), MaskFF));
                __m128 Texela = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(Texel, 24), MaskFF));

                // NOTE(casey): Load destination
                __m128 Destb = _mm_cvtepi32_ps(_mm_and_si128(OriginalDest, MaskFF));
                __m128 Destg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 8), MaskFF));
                __m128 Destr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 16), MaskFF));
                __m128 Desta = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 24), MaskFF));

                // NOTE(casey): Modulate incoming color
                Texelr = mmSquare(_mm_mul_ps(Texelr, Colorr_4x));
                Texelg = mmSquare(_mm_mul_ps(Texelg, Colorg_4x));
                Texelb = mmSquare(_mm_mul_ps(Texelb, Colorb_4x));
                Texela = _mm_mul_ps(Texela, Colora_4x);

                // NOTE(casey): Clamp color in valid range
                Texelr = _mm_min_ps(_mm_max_ps(Texelr, Zero), MaxColorValue);
                Texelg = _mm_min_ps(_mm_max_ps(Texelg, Zero), MaxColorValue);
                Texelb = _mm_min_ps(_mm_max_ps(Texelb, Zero), MaxColorValue);

                // NOTE(casey): Go from sRGB to "linear" brightness space
                Destr = mmSquare(Destr);
                Destg = mmSquare(Destg);
                Destb = mmSquare(Destb);

                // NOTE(casey): Destination bland
                __m128 InvTexelA = _mm_sub_ps(One, _mm_mul_ps(Inv255_4x, Texela));
                __m128 Blendedr = _mm_add_ps(_mm_mul_ps(InvTexelA, Destr), Texelr);
                __m128 Blendedg = _mm_add_ps(_mm_mul_ps(InvTexelA, Destg), Texelg);
                __m128 Blendedb = _mm_add_ps(_mm_mul_ps(InvTexelA, Destb), Texelb);
                __m128 Blendeda = _mm_add_ps(_mm_mul_ps(InvTexelA, Desta), Texela);
#if 1
                // NOTE(casey): Go from "linear" 0-1 brightness space to sRGB 0-255
                Blendedr = _mm_mul_ps(Blendedr, _mm_rsqrt_ps(Blendedr));
                Blendedg = _mm_mul_ps(Blendedg, _mm_rsqrt_ps(Blendedg));
                Blendedb = _mm_mul_ps(Blendedb, _mm_rsqrt_ps(Blendedb));
                Blendeda = Blendeda;
#endif
                __m128i Intr = _mm_cvtps_epi32(Blendedr);
                __m128i Intg = _mm_cvtps_epi32(Blendedg);
                __m128i Intb = _mm_cvtps_epi32(Blendedb);
                __m128i Inta = _mm_cvtps_epi32(Blendeda);

                __m128i Sr = _mm_slli_epi32(Intr, 16);
                __m128i Sg = _mm_slli_epi32(Intg, 8);
                __m128i Sb = Intb;
                __m128i Sa = _mm_slli_epi32(Inta, 24);
            
                __m128i Out = _mm_or_si128(_mm_or_si128(Sr, Sg), _mm_or_si128(Sb, Sa));
                __m128i MaskedOut = _mm_or_si128(_mm_and_si128(WriteMask, Out),
                                                 _mm_andnot_si128(WriteMask, OriginalDest));

                _mm_store_si128((__m128i *)Pixel, MaskedOut);
            
                PixelPx = _mm_add_ps(PixelPx, Four_4x);
                Pixel += 4;

                if((Xi + 8) < MaxX)
                {
                    ClipMask = _mm_set1_epi8(-1);
                }
                else
                {
                    ClipMask = EndClipMask;
                }
            }        

            Row += RowAdvance;
        }
    }
}

internal void
DrawRectangle(loaded_bitmap *Buffer, v2 vMin, v2 vMax, v4 Color, rectangle2i ClipRect)
{
    TIMED_FUNCTION();

    real32 R = Color.r;
    real32 G = Color.g;
    real32 B = Color.b;
    real32 A = Color.a;

    rectangle2i FillRect;
    FillRect.MinX = RoundReal32ToInt32(vMin.x);
    FillRect.MinY = RoundReal32ToInt32(vMin.y);
    FillRect.MaxX = RoundReal32ToInt32(vMax.x);
    FillRect.MaxY = RoundReal32ToInt32(vMax.y);

    FillRect = Intersect(ClipRect, FillRect);

    // NOTE(casey): Premultiply color up front   
    Color.rgb *= Color.a;
    Color *= 255.0f;

    if(HasArea(FillRect))
    {
        __m128i StartClipMask = _mm_set1_epi8(-1);
        __m128i EndClipMask = _mm_set1_epi8(-1);

        __m128i StartClipMasks[] =
        {
            _mm_slli_si128(StartClipMask, 0*4),
            _mm_slli_si128(StartClipMask, 1*4),
            _mm_slli_si128(StartClipMask, 2*4),
            _mm_slli_si128(StartClipMask, 3*4),            
        };

        __m128i EndClipMasks[] =
        {
            _mm_srli_si128(EndClipMask, 0*4),
            _mm_srli_si128(EndClipMask, 3*4),
            _mm_srli_si128(EndClipMask, 2*4),
            _mm_srli_si128(EndClipMask, 1*4),            
        };
        
        if(FillRect.MinX & 3)
        {
            StartClipMask = StartClipMasks[FillRect.MinX & 3];
            FillRect.MinX = FillRect.MinX & ~3;
        }

        if(FillRect.MaxX & 3)
        {
            EndClipMask = EndClipMasks[FillRect.MaxX & 3];
            FillRect.MaxX = (FillRect.MaxX & ~3) + 4;
        }
            
        real32 Inv255 = 1.0f / 255.0f;
        __m128 Inv255_4x = _mm_set1_ps(Inv255);
        real32 One255 = 255.0f;

        __m128 One = _mm_set1_ps(1.0f);
        __m128 Half = _mm_set1_ps(0.5f);
        __m128 Four_4x = _mm_set1_ps(4.0f);
        __m128 One255_4x = _mm_set1_ps(255.0f);
        __m128 Zero = _mm_set1_ps(0.0f);
        __m128i MaskFF = _mm_set1_epi32(0xFF);
        __m128i MaskFFFF = _mm_set1_epi32(0xFFFF);
        __m128i MaskFF00FF = _mm_set1_epi32(0x00FF00FF);
        __m128 Colorr_4x = _mm_set1_ps(Color.r);
        __m128 Colorg_4x = _mm_set1_ps(Color.g);
        __m128 Colorb_4x = _mm_set1_ps(Color.b);
        __m128 Colora_4x = _mm_set1_ps(Color.a);
        __m128 MaxColorValue = _mm_set1_ps(255.0f*255.0f);
        
        uint8 *Row = ((uint8 *)Buffer->Memory +
                      FillRect.MinX*BITMAP_BYTES_PER_PIXEL +
                      FillRect.MinY*Buffer->Pitch);
        int32 RowAdvance = Buffer->Pitch;
        
        int MinY = FillRect.MinY;
        int MaxY = FillRect.MaxY;
        int MinX = FillRect.MinX;
        int MaxX = FillRect.MaxX;
        
        IGNORED_TIMED_BLOCK("Pixel Fill", GetClampedRectArea(FillRect) / 2);
        for(int Y = MinY;
            Y < MaxY;
            ++Y)
        {
            __m128i ClipMask = StartClipMask;

            uint32 *Pixel = (uint32 *)Row;
            for(int XI = MinX;
                XI < MaxX;
                XI += 4)
            {            
                
                __m128i WriteMask = ClipMask;
            
                __m128i OriginalDest = _mm_load_si128((__m128i *)Pixel);
                    
                // NOTE(casey): Load destination
                __m128 Destb = _mm_cvtepi32_ps(_mm_and_si128(OriginalDest, MaskFF));
                __m128 Destg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 8), MaskFF));
                __m128 Destr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 16), MaskFF));
                __m128 Desta = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 24), MaskFF));
                    
                // NOTE(casey): Modulate by incoming color
                __m128 Texelr = mmSquare(Colorr_4x);
                __m128 Texelg = mmSquare(Colorg_4x);
                __m128 Texelb = mmSquare(Colorb_4x);
                __m128 Texela = Colora_4x;

                Texelr = _mm_min_ps(_mm_max_ps(Texelr, Zero), MaxColorValue);
                Texelg = _mm_min_ps(_mm_max_ps(Texelg, Zero), MaxColorValue);
                Texelb = _mm_min_ps(_mm_max_ps(Texelb, Zero), MaxColorValue);
                    
                // NOTE(casey): Go from sRGB to "linear" brightness space
                Destr = mmSquare(Destr);
                Destg = mmSquare(Destg);
                Destb = mmSquare(Destb);

                // NOTE(casey): Destination blend
                __m128 InvTexelA = _mm_sub_ps(One, _mm_mul_ps(Inv255_4x, Texela));
                __m128 Blendedr = _mm_add_ps(_mm_mul_ps(InvTexelA, Destr), Texelr);
                __m128 Blendedg = _mm_add_ps(_mm_mul_ps(InvTexelA, Destg), Texelg);
                __m128 Blendedb = _mm_add_ps(_mm_mul_ps(InvTexelA, Destb), Texelb);
                __m128 Blendeda = _mm_add_ps(_mm_mul_ps(InvTexelA, Desta), Texela);
        
                // NOTE(casey): Go from "linear" 0-1 brightness space to sRGB 0-255
                Blendedr = _mm_mul_ps(Blendedr, _mm_rsqrt_ps(Blendedr));
                Blendedg = _mm_mul_ps(Blendedg, _mm_rsqrt_ps(Blendedg));
                Blendedb = _mm_mul_ps(Blendedb, _mm_rsqrt_ps(Blendedb));
                Blendeda = Blendeda;
            
                __m128i Intr = _mm_cvtps_epi32(Blendedr);
                __m128i Intg = _mm_cvtps_epi32(Blendedg);
                __m128i Intb = _mm_cvtps_epi32(Blendedb);
                __m128i Inta = _mm_cvtps_epi32(Blendeda);

                __m128i Sr = _mm_slli_epi32(Intr, 16);
                __m128i Sg = _mm_slli_epi32(Intg, 8);
                __m128i Sb = Intb;
                __m128i Sa = _mm_slli_epi32(Inta, 24);

                __m128i Out = _mm_or_si128(_mm_or_si128(Sr, Sg), _mm_or_si128(Sb, Sa));

                __m128i MaskedOut = _mm_or_si128(_mm_and_si128(WriteMask, Out),
                                                 _mm_andnot_si128(WriteMask, OriginalDest));
                _mm_store_si128((__m128i *)Pixel, MaskedOut);
            
                Pixel += 4;

                if((XI + 8) < MaxX)
                {
                    ClipMask = _mm_set1_epi8(-1);
                }
                else
                {
                    ClipMask = EndClipMask;
                }
            }
        
            Row += RowAdvance;
        }
    }
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
DrawBitmap(loaded_bitmap *Buffer, loaded_bitmap *Bitmap,
           real32 RealX, real32 RealY, real32 CAlpha = 1.0f)
{
    IGNORED_TIMED_FUNCTION();

    int32 MinX = RoundReal32ToInt32(RealX);
    int32 MinY = RoundReal32ToInt32(RealY);
    int32 MaxX = MinX + Bitmap->Width;
    int32 MaxY = MinY + Bitmap->Height;

    int32 SourceOffsetX = 0;
    if(MinX < 0)
    {
        SourceOffsetX = -MinX;
        MinX = 0;
    }

    int32 SourceOffsetY = 0;
    if(MinY < 0)
    {
        SourceOffsetY = -MinY;
        MinY = 0;
    }

    if(MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }

    if(MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }

    uint8 *SourceRow = (uint8 *)Bitmap->Memory + SourceOffsetY*Bitmap->Pitch + BITMAP_BYTES_PER_PIXEL*SourceOffsetX;
    uint8 *DestRow = ((uint8 *)Buffer->Memory +
                      MinX*BITMAP_BYTES_PER_PIXEL +
                      MinY*Buffer->Pitch);
    for(int Y = MinY;
        Y < MaxY;
        ++Y)
    {
        uint32 *Dest = (uint32 *)DestRow;
        uint32 *Source = (uint32 *)SourceRow;
        for(int X = MinX;
            X < MaxX;
            ++X)
        {
            v4 Texel = {(real32)((*Source >> 16) & 0xFF),
                        (real32)((*Source >> 8) & 0xFF),
                        (real32)((*Source >> 0) & 0xFF),
                        (real32)((*Source >> 24) & 0xFF)};

            Texel = SRGB255ToLinear1(Texel);

            Texel *= CAlpha;

            v4 D = {(real32)((*Dest >> 16) & 0xFF),
                    (real32)((*Dest >> 8) & 0xFF),
                    (real32)((*Dest >> 0) & 0xFF),
                    (real32)((*Dest >> 24) & 0xFF)};

            D = SRGB255ToLinear1(D);
            
            v4 Result = (1.0f-Texel.a)*D + Texel;

            Result = Linear1ToSRGB255(Result);

            *Dest = (((uint32)(Result.a + 0.5f) << 24) |
                     ((uint32)(Result.r + 0.5f) << 16) |
                     ((uint32)(Result.g + 0.5f) << 8) |
                     ((uint32)(Result.b + 0.5f) << 0));
            
            ++Dest;
            ++Source;
        }

        DestRow += Buffer->Pitch;
        SourceRow += Bitmap->Pitch;
    }
}

void
DrawRectangleQuickly(loaded_bitmap *Buffer, v2 Origin, v2 XAxis, v2 YAxis, v4 Color,
                     loaded_bitmap *Texture, real32 PixelsToMeters,
                     rectangle2i ClipRect)
{
    IGNORED_TIMED_FUNCTION();
    
    // NOTE(casey): Premultiply color up front   
    Color.rgb *= Color.a;

    real32 XAxisLength = Length(XAxis);
    real32 YAxisLength = Length(YAxis);
    
    v2 NxAxis = (YAxisLength / XAxisLength) * XAxis;
    v2 NyAxis = (XAxisLength / YAxisLength) * YAxis;

    // NOTE(casey): NzScale could be a parameter if we want people to
    // have control over the amount of scaling in the Z direction
    // that the normals appear to have.
    real32 NzScale = 0.5f*(XAxisLength + YAxisLength);
    
    real32 InvXAxisLengthSq = 1.0f / LengthSq(XAxis);
    real32 InvYAxisLengthSq = 1.0f / LengthSq(YAxis);

    rectangle2i FillRect = InvertedInfinityRectangle2i();

    v2 P[4] = {Origin, Origin + XAxis, Origin + XAxis + YAxis, Origin + YAxis};
    for(int PIndex = 0;
        PIndex < ArrayCount(P);
        ++PIndex)
    {
        v2 TestP = P[PIndex];
        int FloorX = FloorReal32ToInt32(TestP.x);
        int CeilX = CeilReal32ToInt32(TestP.x) + 1;
        int FloorY = FloorReal32ToInt32(TestP.y);
        int CeilY = CeilReal32ToInt32(TestP.y) + 1;

        if(FillRect.MinX > FloorX) {FillRect.MinX = FloorX;}
        if(FillRect.MinY > FloorY) {FillRect.MinY = FloorY;}
        if(FillRect.MaxX < CeilX) {FillRect.MaxX = CeilX;}
        if(FillRect.MaxY < CeilY) {FillRect.MaxY = CeilY;}
    }

    FillRect = Intersect(ClipRect, FillRect);

    if(HasArea(FillRect))
    {
        __m128i StartClipMask = _mm_set1_epi8(-1);
        __m128i EndClipMask = _mm_set1_epi8(-1);

        __m128i StartClipMasks[] =
            {
                _mm_slli_si128(StartClipMask, 0*4),
                _mm_slli_si128(StartClipMask, 1*4),
                _mm_slli_si128(StartClipMask, 2*4),
                _mm_slli_si128(StartClipMask, 3*4),            
            };

        __m128i EndClipMasks[] =
            {
                _mm_srli_si128(EndClipMask, 0*4),
                _mm_srli_si128(EndClipMask, 3*4),
                _mm_srli_si128(EndClipMask, 2*4),
                _mm_srli_si128(EndClipMask, 1*4),            
            };
        
        if(FillRect.MinX & 3)
        {
            StartClipMask = StartClipMasks[FillRect.MinX & 3];
            FillRect.MinX = FillRect.MinX & ~3;
        }

        if(FillRect.MaxX & 3)
        {
            EndClipMask = EndClipMasks[FillRect.MaxX & 3];
            FillRect.MaxX = (FillRect.MaxX & ~3) + 4;
        }
            
        v2 nXAxis = InvXAxisLengthSq*XAxis;
        v2 nYAxis = InvYAxisLengthSq*YAxis;

        real32 Inv255 = 1.0f / 255.0f;
        __m128 Inv255_4x = _mm_set1_ps(Inv255);
        real32 One255 = 255.0f;

        __m128 One = _mm_set1_ps(1.0f);
        __m128 Half = _mm_set1_ps(0.5f);
        __m128 Four_4x = _mm_set1_ps(4.0f);
        __m128 One255_4x = _mm_set1_ps(255.0f);
        __m128 Zero = _mm_set1_ps(0.0f);
        __m128i MaskFF = _mm_set1_epi32(0xFF);
        __m128i MaskFFFF = _mm_set1_epi32(0xFFFF);
        __m128i MaskFF00FF = _mm_set1_epi32(0x00FF00FF);
        __m128 Colorr_4x = _mm_set1_ps(Color.r);
        __m128 Colorg_4x = _mm_set1_ps(Color.g);
        __m128 Colorb_4x = _mm_set1_ps(Color.b);
        __m128 Colora_4x = _mm_set1_ps(Color.a);
        __m128 nXAxisx_4x = _mm_set1_ps(nXAxis.x);
        __m128 nXAxisy_4x = _mm_set1_ps(nXAxis.y);
        __m128 nYAxisx_4x = _mm_set1_ps(nYAxis.x);
        __m128 nYAxisy_4x = _mm_set1_ps(nYAxis.y);
        __m128 Originx_4x = _mm_set1_ps(Origin.x);
        __m128 Originy_4x = _mm_set1_ps(Origin.y);
        __m128 MaxColorValue = _mm_set1_ps(255.0f*255.0f);
        __m128i TexturePitch_4x = _mm_set1_epi32(Texture->Pitch);

        __m128 WidthM2 = _mm_set1_ps((real32)(Texture->Width - 2));
        __m128 HeightM2 = _mm_set1_ps((real32)(Texture->Height - 2));
    
        uint8 *Row = ((uint8 *)Buffer->Memory +
                      FillRect.MinX*BITMAP_BYTES_PER_PIXEL +
                      FillRect.MinY*Buffer->Pitch);
        int32 RowAdvance = Buffer->Pitch;
    
        void *TextureMemory = Texture->Memory;
        int32 TexturePitch = Texture->Pitch;

        int MinY = FillRect.MinY;
        int MaxY = FillRect.MaxY;
        int MinX = FillRect.MinX;
        int MaxX = FillRect.MaxX;
        
        IGNORED_TIMED_BLOCK("Pixel Fill", GetClampedRectArea(FillRect) / 2);
        for(int Y = MinY;
            Y < MaxY;
            ++Y)
        {
            __m128 PixelPy = _mm_set1_ps((real32)Y);
            PixelPy = _mm_sub_ps(PixelPy, Originy_4x);
        
            __m128 PixelPx = _mm_set_ps((real32)(MinX + 3),
                                        (real32)(MinX + 2),
                                        (real32)(MinX + 1),
                                        (real32)(MinX + 0));
            PixelPx = _mm_sub_ps(PixelPx, Originx_4x);

            __m128 PynX = _mm_mul_ps(PixelPx, nXAxisy_4x);
            __m128 PynY = _mm_mul_ps(PixelPy, nYAxisy_4x);

            __m128i ClipMask = StartClipMask;

            uint32 *Pixel = (uint32 *)Row;
            int XI = MinX;
            for(;
                XI < MaxX;
                XI += 4)
            {            
                __m128 U = _mm_add_ps(_mm_mul_ps(PixelPx, nXAxisx_4x), PynX);
                __m128 V = _mm_add_ps(_mm_mul_ps(PixelPx, nYAxisx_4x), PynY);

                __m128i WriteMask = _mm_castps_si128(_mm_and_ps(_mm_and_ps(_mm_cmpge_ps(U, Zero),
                                                                           _mm_cmple_ps(U, One)),
                                                                _mm_and_ps(_mm_cmpge_ps(V, Zero),
                                                                           _mm_cmple_ps(V, One))));
                WriteMask = _mm_and_si128(WriteMask, ClipMask);
            
                __m128i OriginalDest = _mm_load_si128((__m128i *)Pixel);

                U = _mm_min_ps(_mm_max_ps(U, Zero), One);
                V = _mm_min_ps(_mm_max_ps(V, Zero), One);

                // NOTE(casey): Bias texture coordinates to start
                // on the boundary between the 0,0 and 1,1 pixels.
                __m128 tX = _mm_add_ps(_mm_mul_ps(U, WidthM2), Half);
                __m128 tY = _mm_add_ps(_mm_mul_ps(V, HeightM2), Half);
                
                __m128i FetchX_4x = _mm_cvttps_epi32(tX);
                __m128i FetchY_4x = _mm_cvttps_epi32(tY);
            
                __m128 fX = _mm_sub_ps(tX, _mm_cvtepi32_ps(FetchX_4x));
                __m128 fY = _mm_sub_ps(tY, _mm_cvtepi32_ps(FetchY_4x));

                FetchX_4x = _mm_slli_epi32(FetchX_4x, 2);
                FetchY_4x = _mm_or_si128(_mm_mullo_epi16(FetchY_4x, TexturePitch_4x),
                                         _mm_slli_epi32(_mm_mulhi_epi16(FetchY_4x, TexturePitch_4x), 16));
                __m128i Fetch_4x = _mm_add_epi32(FetchX_4x, FetchY_4x);

                int32 Fetch0 = Mi(Fetch_4x, 0);
                int32 Fetch1 = Mi(Fetch_4x, 1);
                int32 Fetch2 = Mi(Fetch_4x, 2);
                int32 Fetch3 = Mi(Fetch_4x, 3);

                uint8 *TexelPtr0 = ((uint8 *)TextureMemory) + Fetch0;
                uint8 *TexelPtr1 = ((uint8 *)TextureMemory) + Fetch1;
                uint8 *TexelPtr2 = ((uint8 *)TextureMemory) + Fetch2;
                uint8 *TexelPtr3 = ((uint8 *)TextureMemory) + Fetch3;

                __m128i SampleA = _mm_setr_epi32(*(uint32 *)(TexelPtr0),
                                                 *(uint32 *)(TexelPtr1),
                                                 *(uint32 *)(TexelPtr2),
                                                 *(uint32 *)(TexelPtr3));

                __m128i SampleB = _mm_setr_epi32(*(uint32 *)(TexelPtr0 + sizeof(uint32)),
                                                 *(uint32 *)(TexelPtr1 + sizeof(uint32)),
                                                 *(uint32 *)(TexelPtr2 + sizeof(uint32)),
                                                 *(uint32 *)(TexelPtr3 + sizeof(uint32)));

                __m128i SampleC = _mm_setr_epi32(*(uint32 *)(TexelPtr0 + TexturePitch),
                                                 *(uint32 *)(TexelPtr1 + TexturePitch),
                                                 *(uint32 *)(TexelPtr2 + TexturePitch),
                                                 *(uint32 *)(TexelPtr3 + TexturePitch));
                
                __m128i SampleD = _mm_setr_epi32(*(uint32 *)(TexelPtr0 + TexturePitch + sizeof(uint32)),
                                                 *(uint32 *)(TexelPtr1 + TexturePitch + sizeof(uint32)),
                                                 *(uint32 *)(TexelPtr2 + TexturePitch + sizeof(uint32)),
                                                 *(uint32 *)(TexelPtr3 + TexturePitch + sizeof(uint32)));
                    
                // NOTE(casey): Unpack bilinear samples
                __m128i TexelArb = _mm_and_si128(SampleA, MaskFF00FF);
                __m128i TexelAag = _mm_and_si128(_mm_srli_epi32(SampleA, 8), MaskFF00FF);
                TexelArb = _mm_mullo_epi16(TexelArb, TexelArb);
                __m128 TexelAa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelAag, 16));
                TexelAag = _mm_mullo_epi16(TexelAag, TexelAag);

                __m128i TexelBrb = _mm_and_si128(SampleB, MaskFF00FF);
                __m128i TexelBag = _mm_and_si128(_mm_srli_epi32(SampleB, 8), MaskFF00FF);
                TexelBrb = _mm_mullo_epi16(TexelBrb, TexelBrb);
                __m128 TexelBa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelBag, 16));
                TexelBag = _mm_mullo_epi16(TexelBag, TexelBag);

                __m128i TexelCrb = _mm_and_si128(SampleC, MaskFF00FF);
                __m128i TexelCag = _mm_and_si128(_mm_srli_epi32(SampleC, 8), MaskFF00FF);
                TexelCrb = _mm_mullo_epi16(TexelCrb, TexelCrb);
                __m128 TexelCa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelCag, 16));
                TexelCag = _mm_mullo_epi16(TexelCag, TexelCag);

                __m128i TexelDrb = _mm_and_si128(SampleD, MaskFF00FF);
                __m128i TexelDag = _mm_and_si128(_mm_srli_epi32(SampleD, 8), MaskFF00FF);
                TexelDrb = _mm_mullo_epi16(TexelDrb, TexelDrb);
                __m128 TexelDa = _mm_cvtepi32_ps(_mm_srli_epi32(TexelDag, 16));
                TexelDag = _mm_mullo_epi16(TexelDag, TexelDag);
            
                // NOTE(casey): Load destination
                __m128 Destb = _mm_cvtepi32_ps(_mm_and_si128(OriginalDest, MaskFF));
                __m128 Destg = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 8), MaskFF));
                __m128 Destr = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 16), MaskFF));
                __m128 Desta = _mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(OriginalDest, 24), MaskFF));
            
                // NOTE(casey): Convert texture from 0-255 sRGB to "linear" 0-1 brightness space
                __m128 TexelAr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelArb, 16));
                __m128 TexelAg = _mm_cvtepi32_ps(_mm_and_si128(TexelAag, MaskFFFF));
                __m128 TexelAb = _mm_cvtepi32_ps(_mm_and_si128(TexelArb, MaskFFFF));

                __m128 TexelBr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelBrb, 16));
                __m128 TexelBg = _mm_cvtepi32_ps(_mm_and_si128(TexelBag, MaskFFFF));
                __m128 TexelBb = _mm_cvtepi32_ps(_mm_and_si128(TexelBrb, MaskFFFF));

                __m128 TexelCr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelCrb, 16));
                __m128 TexelCg = _mm_cvtepi32_ps(_mm_and_si128(TexelCag, MaskFFFF));
                __m128 TexelCb = _mm_cvtepi32_ps(_mm_and_si128(TexelCrb, MaskFFFF));

                __m128 TexelDr = _mm_cvtepi32_ps(_mm_srli_epi32(TexelDrb, 16));
                __m128 TexelDg = _mm_cvtepi32_ps(_mm_and_si128(TexelDag, MaskFFFF));
                __m128 TexelDb = _mm_cvtepi32_ps(_mm_and_si128(TexelDrb, MaskFFFF));
                    
                // NOTE(casey): Bilinear texture blend
                __m128 ifX = _mm_sub_ps(One, fX);
                __m128 ifY = _mm_sub_ps(One, fY);
                
                __m128 l0 = _mm_mul_ps(ifY, ifX);
                __m128 l1 = _mm_mul_ps(ifY, fX);
                __m128 l2 = _mm_mul_ps(fY, ifX);
                __m128 l3 = _mm_mul_ps(fY, fX);

                __m128 Texelr = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAr), _mm_mul_ps(l1, TexelBr)),
                                           _mm_add_ps(_mm_mul_ps(l2, TexelCr), _mm_mul_ps(l3, TexelDr)));
                __m128 Texelg = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAg), _mm_mul_ps(l1, TexelBg)),
                                           _mm_add_ps(_mm_mul_ps(l2, TexelCg), _mm_mul_ps(l3, TexelDg)));
                __m128 Texelb = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAb), _mm_mul_ps(l1, TexelBb)),
                                           _mm_add_ps(_mm_mul_ps(l2, TexelCb), _mm_mul_ps(l3, TexelDb)));
                __m128 Texela = _mm_add_ps(_mm_add_ps(_mm_mul_ps(l0, TexelAa), _mm_mul_ps(l1, TexelBa)),
                                           _mm_add_ps(_mm_mul_ps(l2, TexelCa), _mm_mul_ps(l3, TexelDa)));

                // NOTE(casey): Modulate by incoming color
                Texelr = _mm_mul_ps(Texelr, Colorr_4x);
                Texelg = _mm_mul_ps(Texelg, Colorg_4x);
                Texelb = _mm_mul_ps(Texelb, Colorb_4x);
                Texela = _mm_mul_ps(Texela, Colora_4x);

                Texelr = _mm_min_ps(_mm_max_ps(Texelr, Zero), MaxColorValue);
                Texelg = _mm_min_ps(_mm_max_ps(Texelg, Zero), MaxColorValue);
                Texelb = _mm_min_ps(_mm_max_ps(Texelb, Zero), MaxColorValue);
                            
                // NOTE(casey): Go from sRGB to "linear" brightness space
                Destr = mmSquare(Destr);
                Destg = mmSquare(Destg);
                Destb = mmSquare(Destb);

                // NOTE(casey): Destination blend
                __m128 InvTexelA = _mm_sub_ps(One, _mm_mul_ps(Inv255_4x, Texela));
                __m128 Blendedr = _mm_add_ps(_mm_mul_ps(InvTexelA, Destr), Texelr);
                __m128 Blendedg = _mm_add_ps(_mm_mul_ps(InvTexelA, Destg), Texelg);
                __m128 Blendedb = _mm_add_ps(_mm_mul_ps(InvTexelA, Destb), Texelb);
                __m128 Blendeda = _mm_add_ps(_mm_mul_ps(InvTexelA, Desta), Texela);
        
                // NOTE(casey): Go from "linear" 0-1 brightness space to sRGB 0-255
                Blendedr = _mm_mul_ps(Blendedr, _mm_rsqrt_ps(Blendedr));
                Blendedg = _mm_mul_ps(Blendedg, _mm_rsqrt_ps(Blendedg));
                Blendedb = _mm_mul_ps(Blendedb, _mm_rsqrt_ps(Blendedb));
                Blendeda = Blendeda;
            
                __m128i Intr = _mm_cvtps_epi32(Blendedr);
                __m128i Intg = _mm_cvtps_epi32(Blendedg);
                __m128i Intb = _mm_cvtps_epi32(Blendedb);
                __m128i Inta = _mm_cvtps_epi32(Blendeda);

                __m128i Sr = _mm_slli_epi32(Intr, 16);
                __m128i Sg = _mm_slli_epi32(Intg, 8);
                __m128i Sb = Intb;
                __m128i Sa = _mm_slli_epi32(Inta, 24);

                __m128i Out = _mm_or_si128(_mm_or_si128(Sr, Sg), _mm_or_si128(Sb, Sa));

                __m128i MaskedOut = _mm_or_si128(_mm_and_si128(WriteMask, Out),
                                                 _mm_andnot_si128(WriteMask, OriginalDest));
                _mm_store_si128((__m128i *)Pixel, MaskedOut);
            }
            
            PixelPx = _mm_add_ps(PixelPx, Four_4x);            
            Pixel += 4;

            if((XI + 8) < MaxX)
            {
                ClipMask = _mm_set1_epi8(-1);
            }
            else
            {
                ClipMask = EndClipMask;
            }
        
            Row += RowAdvance;
        }
    }
}

internal void
SortEntries(game_render_commands *Commands, void *SortMemory)
{
    u32 Count = Commands->PushBufferElementCount;
    sort_entry *Entries = (sort_entry *)(Commands->PushBufferBase + Commands->SortEntryAt);

    RadixSort(Count, Entries, (sort_entry *)SortMemory);
    
#if SPELLWEAVER_SLOW
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

internal void
LinearizeClipRects(game_render_commands *Commands, void *ClipMemory)
{
    render_entry_cliprect *Out = (render_entry_cliprect *)ClipMemory;
    for(render_entry_cliprect *Rect = Commands->FirstRect;
        Rect;
        Rect = Rect->Next)
    {
        *Out++ = *Rect;
    }
    Commands->ClipRects = (render_entry_cliprect *)ClipMemory;
}

internal void
RenderCommandsToBitmap(game_render_commands *Commands, loaded_bitmap *OutputTarget, rectangle2i BaseClipRect)
{
    TIMED_FUNCTION();

    u32 SortEntryCount = Commands->PushBufferElementCount;
    sort_entry *SortEntries = (sort_entry *)(Commands->PushBufferBase + Commands->SortEntryAt);
    
    real32 NullPixelsToMeters = 1.0f;

    u32 ClipRectIndex = 0xFFFFFFFF;
    rectangle2i ClipRect = BaseClipRect;
    
    sort_entry *SortEntry = SortEntries;
    for(u32 SortEntryIndex = 0;
        SortEntryIndex < SortEntryCount;
        ++SortEntryIndex, ++SortEntry)
    {
        render_group_entry_header *Header = (render_group_entry_header *)
            (Commands->PushBufferBase + SortEntry->Index);
#if 1
        if(ClipRectIndex != Header->ClipRectIndex)
        {
            ClipRectIndex = Header->ClipRectIndex;
            Assert(ClipRectIndex < Commands->ClipRectCount);
    
            render_entry_cliprect *Clip = Commands->ClipRects + ClipRectIndex;
            ClipRect = Intersect(BaseClipRect, Clip->Rect);
        }
#endif
        
        void *Data = (uint8 *)Header + sizeof(*Header);
        switch(Header->Type)
        {
            case RenderGroupEntryType_render_entry_clear:
            {
                render_entry_clear *Entry = (render_entry_clear *)Data;

                DrawRectangle(OutputTarget, V2(0.0f, 0.0f),
                              V2((real32)OutputTarget->Width, (real32)OutputTarget->Height),
                              Entry->Color, ClipRect);
            } break;

            case RenderGroupEntryType_render_entry_bitmap:
            {
                render_entry_bitmap *Entry = (render_entry_bitmap *)Data;
                Assert(Entry->Bitmap);

                v2 XAxis = {1, 0};
                v2 YAxis = {0, 1};
//                DrawBitmap(OutputTarget, Entry->Bitmap, Entry->P.x, Entry->P.y);
#if 1
                DrawFetchlessRect(OutputTarget, Entry->P,
                                  Entry->Size.x*XAxis,
                                  Entry->Size.y*YAxis, Entry->Color,
                                  Entry->Bitmap, ClipRect);
#else
                DrawRectangleQuickly(OutputTarget, Entry->P,
                                     Entry->Size.x*XAxis,
                                     Entry->Size.y*YAxis, Entry->Color,
                                     Entry->Bitmap, NullPixelsToMeters, ClipRect);
#endif
            } break;

            case RenderGroupEntryType_render_entry_rectangle:
            {
                render_entry_rectangle *Entry = (render_entry_rectangle *)Data;
                DrawRectangle(OutputTarget, Entry->P, Entry->P + Entry->Dim, Entry->Color, ClipRect);
            } break;

            case RenderGroupEntryType_render_entry_coordinate_system:
            {
                render_entry_coordinate_system *Entry = (render_entry_coordinate_system *)Data;
            } break;

            InvalidDefaultCase;
        }
    }
}

internal PLATFORM_WORK_QUEUE_CALLBACK(DoTiledRenderWork)
{
    TIMED_FUNCTION();

    tile_render_work *Work = (tile_render_work *)Data;

    RenderCommandsToBitmap(Work->Commands, Work->OutputTarget, Work->ClipRect);
}

internal void
SoftwareRenderCommands(platform_work_queue *RenderQueue,
                       game_render_commands *Commands, loaded_bitmap *OutputTarget)
{
    TIMED_FUNCTION();

    /*
      TODO(casey):
      
      - Make sure that tiles are all cache-aligned
      - Can we get hyperthreads synced so they do interleaved lines?
      - How big should the tiles be for performance?
      - Actually ballpark the memory bandwidth for our DrawRectangleQuickly
      - Re-test some of our instruction choices
    */
    
    int const TileCountX = 4;
    int const TileCountY = 4;
    tile_render_work WorkArray[TileCountX*TileCountY];

    Assert(((uintptr)OutputTarget->Memory & 15) == 0);    
    int TileWidth = OutputTarget->Width / TileCountX;
    int TileHeight = OutputTarget->Height / TileCountY;

    TileWidth = ((TileWidth + 3) / 4) * 4;

    int WorkCount = 0;
    for(int TileY = 0;
        TileY < TileCountY;
        ++TileY)
    {
        for(int TileX = 0;
            TileX < TileCountX;
            ++TileX)
        {
            tile_render_work *Work = WorkArray + WorkCount++;

            rectangle2i ClipRect;
            ClipRect.MinX = TileX*TileWidth;
            ClipRect.MaxX = ClipRect.MinX + TileWidth;
            ClipRect.MinY = TileY*TileHeight;
            ClipRect.MaxY = ClipRect.MinY + TileHeight;

            if(TileX == (TileCountX - 1))
            {
                ClipRect.MaxX = OutputTarget->Width;
            }
            if(TileY == (TileCountY - 1))
            {
                ClipRect.MaxY = OutputTarget->Height;
            }

            Work->Commands = Commands;
            Work->OutputTarget = OutputTarget;
            Work->ClipRect = ClipRect;
#if 1
            // NOTE(casey): This is the multi-threaded path
            Platform.AddEntry(RenderQueue, DoTiledRenderWork, Work);
#else
            // NOTE(casey): This is the single-threaded path
            DoTiledRenderWork(RenderQueue, Work);
#endif
        }
    }

    Platform.CompleteAllWork(RenderQueue);
}

/* ========================================================================
   $File: $
   $Date: $
   $Revision: $
   $Creator: Casey Muratori $
   $Notice: (C) Copyright 2014 by Molly Rocket, Inc. All Rights Reserved. $
   ======================================================================== */

inline r32
GetLineAdvance(ssa_font *Info, r32 FontScale)
{
    r32 Result = GetLineAdvanceFor(Info)*FontScale;
    Result += 0.1;
    return(Result);
}

inline r32
GetBaseline(ssa_font *Info, r32 FontScale)
{
    r32 Result = FontScale*GetStartingBaselineY(Info);
    return(Result);
}

internal rectangle2
TextOp(render_group *RenderGroup, text_config TextConfig, char *String, u32 Length, text_op Op)
{
    rectangle2 Result = InvertedInfinityRectangle2();
    if(TextConfig.Font)
    {
        u32 PrevCodePoint = 0;
        r32 CharScale = TextConfig.FontScale;
        r32 AtY = TextConfig.P.y;
        r32 AtX = TextConfig.P.x;

        u32 Count = 0;
        for(char *At = String;
            *At;
            )
        {
            u32 CodePoint = *At;
            r32 AdvanceX =
                CharScale*GetHorizontalAdvanceForPair(TextConfig.FontInfo,
                                                      TextConfig.Font,
                                                      PrevCodePoint,
                                                      CodePoint);
            AtX += AdvanceX;

            if(!IsEndOfLine(*At))
            {
                bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets,
                                                       TextConfig.FontInfo,
                                                       TextConfig.Font,
                                                       CodePoint);
                ssa_bitmap *Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);

                r32 BitmapScale = CharScale*(r32)Info->Dim[1];
                v3 BitmapOffset = V3(AtX, AtY, TextConfig.AtZ);
                if(Op == TextOp_DrawText)
                {
                    PushBitmap(RenderGroup, &TextConfig.TextTransform, BitmapID, BitmapScale,
                               BitmapOffset, TextConfig.Color, 1.0f);
                    PushBitmap(RenderGroup, &TextConfig.TextShadowTransform, BitmapID, BitmapScale,
                               BitmapOffset + V3(2.0f, -2.0f, 0.0f), V4(0, 0, 0, 1.0f), 1.0f);
                }
                else                    
                {
                    Assert(Op == TextOp_SizeText);

                    loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, BitmapID, RenderGroup->GenerationID);
                    if(Bitmap)
                    {
                        object_transform D = DefaultFlatTransform();
                        used_bitmap_dim Dim = GetBitmapDim(RenderGroup, &D, Bitmap,
                                                           BitmapScale, BitmapOffset, 1.0f);
                        rectangle2 GlyphDim = RectMinDim(Dim.P.xy, Dim.Size);
                        Result = Union(Result, GlyphDim);
                    }
                }
            }
            else if(IsEndOfLine(*At))
            {
                AtY -= GetLineAdvance(TextConfig.FontInfo, TextConfig.FontScale);
                AtX = TextConfig.P.x;
            }

            PrevCodePoint = CodePoint;

            ++At;
            ++Count;
            if((Count >= Length) && (Length != 0))
            {
                break;
            }
        }
    }

    return(Result);
}

inline void
TextOutAt(render_group *RenderGroup, text_config TextConfig, char *String, u32 Length)
{
    TextOp(RenderGroup, TextConfig, String, Length, TextOp_DrawText);    
}

inline rectangle2
GetTextSize(render_group *RenderGroup, text_config TextConfig, char *String, u32 Length)
{
    TextConfig.P = V2(0, 0);
    rectangle2 Result = TextOp(RenderGroup, TextConfig, String, Length, TextOp_SizeText);

    return(Result);
}

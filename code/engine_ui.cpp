/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline b32
UIIsHex(char Char)
{
    b32 Result = (((Char >= '0') && (Char <= '9')) ||
                  ((Char >= 'A') && (Char <= 'F')));

    return(Result);
}

inline u32
UIGetHex(char Char)
{
    u32 Result = 0;

    if((Char >= '0') && (Char <= '9'))
    {
        Result = Char - '0';
    }
    else if((Char >= 'A') && (Char <= 'F'))
    {
        Result = 0xA + (Char - 'A');
    }

    return(Result);
}

inline r32
UIGetLineAdvance(ui_state *UIState)
{
    r32 Result = GetLineAdvanceFor(UIState->FontInfo)*UIState->FontScale;
    return(Result);
}

inline r32
GetBaseline(ui_state *UIState)
{
    r32 Result = UIState->FontScale*GetStartingBaselineY(UIState->FontInfo);
    return(Result);
}

internal rectangle2
UITextOp(ui_state *UIState, ui_text_op Op, v2 P, char *String, r32 FontScale, v4 Color = V4(1, 1, 1, 1),
         r32 AtZ = 0.0f)
{
    rectangle2 Result = InvertedInfinityRectangle2();
    if(UIState && UIState->Font)
    {
        render_group *RenderGroup = &UIState->RenderGroup;
        loaded_font *Font = UIState->Font;
        ssa_font *Info = UIState->FontInfo;

        u32 PrevCodePoint = 0;
        r32 CharScale = (FontScale == 0.0f) ? UIState->FontScale : FontScale;
        r32 AtY = P.y;
        r32 AtX = P.x;
        b32 FirstInLine = true;
        for(char *At = String;
            *At;
            )
        {
            if((At[0] == '\\') &&
               (At[1] == '#') &&
               (At[2] != 0) &&
               (At[3] != 0) &&
               (At[4] != 0))
            {
                r32 CScale = 1.0f / 9.0f;
                Color = V4(Clamp01(CScale*(r32)(At[2] - '0')),
                           Clamp01(CScale*(r32)(At[3] - '0')),
                           Clamp01(CScale*(r32)(At[4] - '0')),
                           1.0f);
                At += 5;
            }
            else if((At[0] == '\\') &&
                    (At[1] == '^') &&
                    (At[2] != 0))
            {
                r32 CScale = 1.0f / 9.0f;
                CharScale = UIState->FontScale*Clamp01(CScale*(r32)(At[2] - '0'));
                At += 3;
            }
            else
            {
                u32 CodePoint = *At;
                if((At[0] == '\\') &&
                   (UIIsHex(At[1])) &&
                   (UIIsHex(At[2])) &&
                   (UIIsHex(At[3])) &&
                   (UIIsHex(At[4])))
                {
                    CodePoint = ((UIGetHex(At[1]) << 12) |
                                 (UIGetHex(At[2]) << 8) |
                                 (UIGetHex(At[3]) << 4) |
                                 (UIGetHex(At[4]) << 0));
                    At += 4;
                }

                r32 AdvanceX = CharScale*GetHorizontalAdvanceForPair(Info, Font, PrevCodePoint, CodePoint);
                AtX += FirstInLine ? 0.0f : AdvanceX;
                FirstInLine = false;

                if(IsEndOfLine(*At))
                {
                    AtY -= UIGetLineAdvance(UIState);
                    AtX = P.x;
                    FirstInLine = true;
                }
                else if(CodePoint != ' ')
                {
                    bitmap_id BitmapID = GetBitmapForGlyph(RenderGroup->Assets, Info, Font, CodePoint);
                    ssa_bitmap *Info = GetBitmapInfo(RenderGroup->Assets, BitmapID);

                    r32 BitmapScale = CharScale*(r32)Info->Dim[1];
                    v3 BitmapOffset = V3(AtX, AtY, AtZ);
                    if(Op == UITextOp_DrawText)
                    {
                        PushBitmap(RenderGroup, &UIState->TextTransform, BitmapID, BitmapScale,
                            BitmapOffset, Color, 1.0f);
                        PushBitmap(RenderGroup, &UIState->ShadowTransform, BitmapID, BitmapScale,
                            BitmapOffset + V3(2.0f, -2.0f, 0.0f), V4(0, 0, 0, 1.0f), 1.0f);
                    }
                    else                    
                    {
                        Assert(Op == UITextOp_SizeText);

                        loaded_bitmap *Bitmap = GetBitmap(RenderGroup->Assets, BitmapID, RenderGroup->GenerationID);
                        if(Bitmap)
                        {
                            object_transform Flat = DefaultFlatTransform();
                            used_bitmap_dim Dim = GetBitmapDim(RenderGroup, &Flat,
                                                               Bitmap, BitmapScale, BitmapOffset, 1.0f);
                            rectangle2 GlyphDim = RectMinDim(Dim.P.xy, Dim.Size);
                            Result = Union(Result, GlyphDim);
                        }
                    }
                }

                PrevCodePoint = CodePoint;

                ++At;
            }
        }
    }

    return(Result);
}

internal rectangle2
UITextOpWithInEditorFont(ui_state *UIState, ui_text_op Op, v2 P, char *String, builder_loaded_font *Font,
                         r32 FontScale, v4 Color = V4(1, 1, 1, 1), r32 AtZ = 0.0f)
{
    rectangle2 Result = InvertedInfinityRectangle2();
    if(UIState)
    {
        render_group *RenderGroup = &UIState->RenderGroup;

        u32 PrevCodePoint = 0;
        r32 CharScale = FontScale;
        r32 AtY = P.y;
        r32 AtX = P.x;
        b32 FirstInLine = true;
        for(char *At = String;
            *At;
            )
        {
            u32 CodePoint = *At;

            u32 PrevGlyph = Font->UnicodeMap[PrevCodePoint];
            u32 Glyph = Font->UnicodeMap[CodePoint];

            r32 AdvanceX = CharScale*Font->HorizontalAdvance[PrevGlyph*Font->GlyphCount + Glyph];
            AtX += FirstInLine ? 0.0f : AdvanceX;
            FirstInLine = false;

            if(IsEndOfLine(*At))
            {
                AtY -= CharScale*(Font->AscenderHeight + Font->DescenderHeight + Font->ExternalLeading);
                AtX = P.x;
                FirstInLine = true;
            }
            else if(CodePoint != ' ')
            {
                loaded_bitmap *Bitmap = &Font->Glyphs[Glyph];

                r32 BitmapScale = CharScale*Bitmap->Height;
                v3 BitmapOffset = V3(AtX, AtY, 10.0f);

                if(Op == UITextOp_DrawText)
                {
                    PushBitmap(RenderGroup, &UIState->TextTransform, Bitmap, BitmapScale,
                               BitmapOffset, Color, 1.0f);
                    PushBitmap(RenderGroup, &UIState->ShadowTransform, Bitmap, BitmapScale,
                               BitmapOffset + V3(2.0f, -2.0f, 0.0f), V4(0, 0, 0, 1.0f), 1.0f);
                }
                else                    
                {
                    Assert(Op == UITextOp_SizeText);

                    if(Bitmap)
                    {
                        object_transform Flat = DefaultFlatTransform();
                        used_bitmap_dim Dim = GetBitmapDim(RenderGroup, &Flat,
                                                           Bitmap, BitmapScale, BitmapOffset, 1.0f);
                        rectangle2 GlyphDim = RectMinDim(Dim.P.xy, Dim.Size);
                        Result = Union(Result, GlyphDim);
                    }
                }
            }

            PrevCodePoint = CodePoint;
            ++At;
        }
    }

    return(Result);
}

inline void
UITextOutAt(ui_state *UIState, v2 P, char *String, r32 FontScale = 0.0f, v4 Color = V4(1, 1, 1, 1), r32 AtZ = 0.0f)
{
    render_group *RenderGroup = &UIState->RenderGroup;

    UITextOp(UIState, UITextOp_DrawText, P, String, FontScale, Color, AtZ);    
}

inline rectangle2
UIGetTextSize(ui_state *UIState, char *String, r32 FontScale = 0.0f)
{
    rectangle2 Result = UITextOp(UIState, UITextOp_SizeText, V2(0, 0), String, FontScale);

    return(Result);
}

internal void
ResetCursorArray(array_cursor *Cursor)
{
    for(u32 I = 0;
        I < Cursor->ArrayCount;
        ++I)
    {
        Cursor->Array[I] = I;
    }
}

internal void
InitializeCursor(array_cursor *Cursor, u32 CursorArrayCount)
{
    Cursor->ArrayPosition = 0;
    Cursor->ArrayCount = CursorArrayCount;
    ResetCursorArray(Cursor);
}

internal void
ReInitializeCursor(array_cursor *Cursor, u32 CursorArrayCount)
{
    Cursor->ArrayPosition = 0;
    Cursor->ArrayCount = CursorArrayCount;
    ResetCursorArray(Cursor);
}

internal void
ChangeCursorPositionForScrollWindow(array_cursor *Cursor, u32 SourceArrayCount, s16 MouseZ)
{
    if(MouseZ != 0)
    {
        u32 CursorLastIndex = Cursor->ArrayCount - 1;
        s32 Count = MouseZ > 0 ? MouseZ : MouseZ * -1;
        for(s32 MouseRotIndex = 0;
            MouseRotIndex < Count;
            ++MouseRotIndex)
        {
            if(MouseZ > 0)
            {
                s32 NewFirst = Cursor->Array[0] - 1;
                for(u32 I = CursorLastIndex; I > 0; --I)
                    Cursor->Array[I] = Cursor->Array[I - 1];

                if(NewFirst == -1)
                    NewFirst = SourceArrayCount - 1;

                Cursor->Array[0] = NewFirst;
            }
            else
            {
                u32 NewLast = Cursor->Array[CursorLastIndex] + 1;

                for(u32 I = 0; I < Cursor->ArrayCount; ++I)
                    Cursor->Array[I] = Cursor->Array[I + 1];

                if(NewLast == SourceArrayCount)
                    NewLast = 0;

                Cursor->Array[CursorLastIndex] = NewLast;
            }
        }
    }
}

internal void
ChangeCursorPositionForToolBar(array_cursor *Cursor, u32 SourceArrayCount, s16 MouseZ)
{
    if(MouseZ != 0)
    {
        u32 CursorLastIndex = Cursor->ArrayCount - 1;
        s32 Count = MouseZ > 0 ? MouseZ : MouseZ * -1;
        for(s32 MouseRotIndex = 0;
            MouseRotIndex < Count;
            ++MouseRotIndex)
        {
            if(MouseZ > 0)
            {
                if(Cursor->ArrayPosition == 0)
                {
                    s32 NewFirst = Cursor->Array[0] - 1;
                    for(u32 I = CursorLastIndex; I > 0; --I)
                        Cursor->Array[I] = Cursor->Array[I - 1];

                    if(NewFirst == -1)
                        NewFirst = SourceArrayCount - 1;

                    Cursor->Array[0] = NewFirst;
                }
                else
                    --Cursor->ArrayPosition;
            }
            else
            {
                if(Cursor->ArrayPosition == CursorLastIndex)
                {
                    u32 NewLast = Cursor->Array[CursorLastIndex] + 1;

                    for(u32 I = 0; I < Cursor->ArrayCount; ++I)
                        Cursor->Array[I] = Cursor->Array[I + 1];

                    if(NewLast == SourceArrayCount)
                        NewLast = 0;

                    Cursor->Array[CursorLastIndex] = NewLast;
                }
                else
                    ++Cursor->ArrayPosition;
            }
        }
    }
}

internal void
UIDrawWindowOutline(ui_state *UIState, v2 Center, v2 Dim, b32 StandardLable = true)
{
    render_group *RenderGroup = &UIState->RenderGroup;
    UIState->BackingTransform.OffsetP += V3(Center, 0.0f);
    v2 HalfDim = 0.5f*Dim;
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 0), Dim,
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 2.0f), Dim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 3.0f), Dim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 4.0f), Dim - V2(16.0f, 16.0f),
             UI_COLOR_RGBA1_67412CFF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 5.0f), Dim - V2(24.0f, 24.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 6.0f), Dim - V2(28.0f, 28.0f),
             UI_COLOR_RGBA1_B97A57FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 7.0f), Dim - V2(32.0f, 32.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 8.0f), Dim - V2(36.0f, 36.0f),
             UI_COLOR_RGBA1_67412CFF);
    PushRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 9.0f), Dim - V2(44.0f, 44.0f),
             UI_COLOR_RGBA1_4D3020FF);

    if(StandardLable)
    {
        r32 YOffset = HalfDim.y - 42.0f;
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 10.0f), V2(Dim.x - 44.0f, 48.0f),
                 UI_COLOR_RGBA1_67412CFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 11.0f), V2(Dim.x - 44.0f, 40.0f),
                 UI_COLOR_RGBA1_4D3020FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 12.0f), V2(Dim.x - 48.0f, 36.0f),
                 UI_COLOR_RGBA1_B97A57FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 13.0f), V2(Dim.x - 52.0f, 32.0f),
                 UI_COLOR_RGBA1_4D3020FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0, YOffset, 14.0f), V2(Dim.x - 56.0f, 28.0f),
                 UI_COLOR_RGBA1_CFEEF5FF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(0.5f, HalfDim.y - 43.0f, 15.0f), V2(Dim.x - 58.0f, 26.0f),
                 UI_COLOR_RGBA1_00A2E8FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(0.0f, YOffset, 16.0f), V2(Dim.x - 60.0f, 24.0f),
                 UI_COLOR_RGBA1_99D9EAFF);
    }
    
    r32 Ax[2] = {HalfDim.x - 12.0f, -(HalfDim.x - 12.0f)};
    r32 Ay[2] = {HalfDim.y - 3.0f, -(HalfDim.y - 3.0f)};

    r32 Bx[2] = {HalfDim.x - 3.0f, -(HalfDim.x - 3.0f)};
    r32 By[2] = {HalfDim.y - 12.0f, -(HalfDim.y - 12.0f)};

    r32 Cx[2] = {HalfDim.x - 10.0f, -(HalfDim.x - 10.0f)};
    r32 Cy[2] = {HalfDim.y - 10.0f, -(HalfDim.y - 10.0f)};

    r32 Dx[2] = {HalfDim.x - 8.0f, -(HalfDim.x - 8.0f)};
    r32 Dy[2] = {HalfDim.y - 8.0f, -(HalfDim.y - 8.0f)};

    r32 Ex[2] = {HalfDim.x - 1.0f, -(HalfDim.x - 1.0f)};
    r32 Ey[2] = {HalfDim.y - 1.0f, -(HalfDim.y - 1.0f)};

    r32 Fx[2] = {HalfDim.x - 5.0f, -(HalfDim.x - 5.0f)};
    r32 Fy[2] = {HalfDim.y - 6.0f, -(HalfDim.y - 6.0f)};

    r32 Gx[2] = {HalfDim.x - 11.0f, -(HalfDim.x - 11.0f)};
    r32 Gy[2] = {HalfDim.y - 7.0f, -(HalfDim.y - 7.0f)};

    r32 Hx[2] = {HalfDim.x - 13.0f, -(HalfDim.x - 13.0f)};

    u32 Indecies[4][2] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};

    for(u32 Itter = 0;
        Itter < 4;
        ++Itter)
    {
        u32 IndexX = Indecies[Itter][0];
        u32 IndexY = Indecies[Itter][1];

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Ax[IndexX], Ay[IndexY], 10.0f), V2(32.0f, 14.0f),
                 UI_COLOR_RGBA1_4D3020FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Bx[IndexX], By[IndexY], 10.0f), V2(14.0f, 32.0f),
                 UI_COLOR_RGBA1_4D3020FF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Ax[IndexX], Ay[IndexY], 11.0f), V2(28.0f, 10.0f),
                 UI_COLOR_RGBA1_B97A57FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Bx[IndexX], By[IndexY], 11.0f), V2(10.0f, 28.0f),
                 UI_COLOR_RGBA1_B97A57FF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Cx[IndexX], Cy[IndexY], 12.0f), V2(12.0f, 12.0f),
                 UI_COLOR_RGBA1_3F48CCFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Dx[IndexX], Dy[IndexY], 12.0f), V2(12.0f, 12.0f),
                 UI_COLOR_RGBA1_3F48CCFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Dx[IndexX], Ey[IndexY], 12.0f), V2(8.0f, 2.0f),
                 UI_COLOR_RGBA1_3F48CCFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Ex[IndexX], Dy[IndexY], 12.0f), V2(2.0f, 8.0f),
                 UI_COLOR_RGBA1_3F48CCFF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Bx[IndexX], Dy[IndexY], 13.0f), V2(2.0f, 8.0f),
                 UI_COLOR_RGBA1_99D9EAFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Fx[IndexX], Dy[IndexY], 13.0f), V2(2.0f, 12.0f),
                 UI_COLOR_RGBA1_99D9EAFF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Dx[IndexX], Fy[IndexY], 13.0f), V2(4.0f, 8.0f),
                 UI_COLOR_RGBA1_99D9EAFF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Gx[IndexX], Fy[IndexY], 14.0f), V2(2.0f, 8.0f),
                 UI_COLOR_RGBA1_00A2E8FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Hx[IndexX], Gy[IndexY], 13.0f), V2(2.0f, 6.0f),
                 UI_COLOR_RGBA1_00A2E8FF);
        PushRect(RenderGroup, &UIState->BackingTransform, V3(Cx[IndexX], By[IndexY], 13.0f), V2(8.0f, 4.0f),
                 UI_COLOR_RGBA1_00A2E8FF);

        PushRect(RenderGroup, &UIState->BackingTransform, V3(Dx[IndexX], Dy[IndexY], 14.0f), V2(4.0f, 4.0f),
                 UI_COLOR_RGBA1_FFFFFFFF);
    }
}

internal void
BeginUI(ui_state *UIState, editor_render_commands *Commands, editor_assets *Assets, u32 MainGenerationID,
        u32 Width, u32 Height, nk_ui *UI, nk_context *Nk)
{
    if(!UIState->Initialized)
    {
        UIState->UI = UI;
        UIState->Nk = Nk;
        
        UIState->Initialized = true;
    }

    UIState->RenderGroup = BeginRenderGroup(Assets, Commands, MainGenerationID, false, Width, Height);

    UIState->Font = PushFont(&UIState->RenderGroup, UIState->FontID);
    UIState->FontInfo = GetFontInfo(UIState->RenderGroup.Assets, UIState->FontID);

    UIState->GlobalWidth = (r32)Width;
    UIState->GlobalHeight = (r32)Height;

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    MatchVector.E[Tag_FontType] = FontType_Default;
    WeightVector.E[Tag_FontType] = 1;
    UIState->FontID = GetBestMatchFontFrom(Assets, Asset_Font, &MatchVector, &WeightVector);

    UIState->FontScale = 1.25f;

    f32 Ratio = (f32)Nk->BaseHeight/(f32)Nk->BaseWidth;
    Orthographic(&UIState->RenderGroup, Nk->Scale.x);
    Clear(&UIState->RenderGroup, UI_COLOR_RGBA1_4D3020FF);

    UIState->LeftEdge = -0.5f*Width;
    UIState->RightEdge = 0.5f*Width;

    UIState->TextTransform = DefaultFlatTransform();
    UIState->ShadowTransform = DefaultFlatTransform();
    UIState->UITransform = DefaultFlatTransform();
    UIState->BackingTransform = DefaultFlatTransform();

    UIState->BackingTransform.ChunkZ = 100000;
    UIState->ShadowTransform.ChunkZ = 200000;
    UIState->UITransform.ChunkZ = 300000;
    UIState->TextTransform.ChunkZ = 400000;

    UIState->DefaultClipRect = UIState->RenderGroup.CurrentClipRectIndex;
}

internal void
EndUI(editor_state *EditorState, ui_state *UIState, engine_input *Input)
{
    TIMED_FUNCTION();

    render_group *RenderGroup = &UIState->RenderGroup;

    UIState->AltUI = Input->MouseButtons[PlatformMouseButton_Right].EndedDown;
    UIState->MouseZ = Input->MouseZ;
    object_transform Flat = DefaultFlatTransform();
    v2 MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;
    
    EndRenderGroup(&UIState->RenderGroup);
}

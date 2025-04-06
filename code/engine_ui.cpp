/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline b32
UIObjectIDsAreEqual(ui_object_id A, ui_object_id B)
{
    b32 Result = ((A.High == B.High) && (A.Low == B.Low));
    return(Result);
}

internal ui_object *
GetOrCreateUIObject(ui_state *UIState, ui_object_id ID)
{
    u32 HashIndex = ((ID.High >> 2) + (ID.Low >> 2)) % ArrayCount(UIState->UIObjectsHash);
    ui_object **HashSlot = UIState->UIObjectsHash + HashIndex;

    ui_object *Result = 0;
    for(ui_object *Search = *HashSlot;
        Search;
        Search = Search->NextInHash)
    {
        if(UIObjectIDsAreEqual(Search->ID, ID))
        {
            Result = Search;
            break;
        }
    }
    

    if(!Result)
    {
        Result = PushStruct(&UIState->UIArena, ui_object);
        Result->ID = ID;
        Result->Type = UIObjectType_None;
        Result->NextInHash = *HashSlot;
        *HashSlot = Result;
    }

    return(Result);
}

inline ui_object_id
GetUIObjectID(char *Name)
{
    u64 LowMask = 0x00000000FFFFFFFF;
    u64 CRC = CRC64FromString(Name);

    ui_object_id Result = {};
    Result.High = (u32)(CRC >> 32);
    Result.Low = (u32)(CRC & LowMask);

    return(Result);
}

inline interaction_id
InteractionID(ui_state *UIState)
{
    interaction_id Result = {};
    Result.Owner = 0;
    Result.Value = UIState->NextInteractionID++;

    return(Result);
}

inline b32
IDsAreEqual(interaction_id A, interaction_id B)
{
    b32 Result = ((A.Owner == B.Owner) &&
                  (A.Value == B.Value));

    return(Result);
}

inline b32
InteractionsAreEqual(interaction A, interaction B)
{
    b32 Result = (IDsAreEqual(A.ID, B.ID) &&
                  (A.Type == B.Type) &&
                  (A.Target == B.Target) &&
                  (A.Generic == B.Generic));

    return(Result);
}

inline b32
InteractionIsHot(ui_state *UIState, interaction B)
{
    b32 Result = InteractionsAreEqual(UIState->HotInteraction, B);

    if(B.Type == Interaction_None)
    {
        Result = false;
    }

    return(Result);
}

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

#if 0
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
#endif
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

inline interaction
UISetPointerInteraction(interaction_id ID, void **Target, void *Value)
{
    interaction Result = {};
    Result.ID = ID;
    Result.Type = Interaction_SetPointer;
    Result.Target = Target;
    Result.Pointer = Value;

    return(Result);
}

inline interaction
UISetUInt32Interaction(interaction_id ID, u32 *Target, u32 Value)
{
    interaction Result = {};
    Result.ID = ID;
    Result.Type = Interaction_SetUInt32;
    Result.Target = Target;
    Result.UInt32 = Value;

    return(Result);
}

inline ui_layout 
UIBeginLayout(ui_state *UIState, v2 MouseP, v2 UpperLeftCorner)
{
    ui_layout Layout = {};
    Layout.UIState = UIState;
    Layout.MouseP = MouseP;
    Layout.BaseCorner = Layout.At = UpperLeftCorner;
    Layout.LineAdvance = UIState->FontScale*GetLineAdvanceFor(UIState->FontInfo);
    Layout.SpacingY = 4.0f;
    Layout.SpacingX = 4.0f;
    
    return(Layout);
}

inline void
UIEndLayout(ui_layout *Layout)
{
}

inline ui_layout_element
UIBeginElementRectangle(ui_layout *Layout, v2 *Dim)
{
    ui_layout_element Element = {};

    Element.Layout = Layout;
    Element.Dim = Dim;

    return(Element);
}

inline void
UIMakeElementSizable(ui_layout_element *Element)
{
    Element->Size = Element->Dim;
}

inline void
UIDefaultInteraction(ui_layout_element *Element, interaction Interaction)
{
    Element->Interaction = Interaction;
}

inline void
UIAdvanceElement(ui_layout *Layout, rectangle2 ElRect)
{
    Layout->NextYDelta = Minimum(Layout->NextYDelta, GetMinCorner(ElRect).y - Layout->At.y);

    if(Layout->NoLineFeed)
    {
        Layout->At.x = GetMaxCorner(ElRect).x + Layout->SpacingX;
    }
    else
    {
        Layout->At.y += Layout->NextYDelta - Layout->SpacingY;
        Layout->LineInitialized = false;
    }
}

inline void
UIEndElement(ui_layout_element *Element)
{
    ui_layout *Layout = Element->Layout;
    ui_state *UIState = Layout->UIState;
    object_transform NoTransform = UIState->BackingTransform;
    
    if(!Layout->LineInitialized)
    {
        Layout->At.x = Layout->BaseCorner.x + Layout->Depth*2.0f*Layout->LineAdvance;
        Layout->LineInitialized = true;
        Layout->NextYDelta = 0.0f;
    }

    r32 SizeHandlePixels = 4.0f;

    v2 Frame = {0, 0};
    if(Element->Size)
    {
        Frame.x = SizeHandlePixels;
        Frame.y = SizeHandlePixels;
    }

    v2 TotalDim = *Element->Dim + 2.0f*Frame;

    v2 TotalMinCorner = V2(Layout->At.x,
                           Layout->At.y - TotalDim.y);
    v2 TotalMaxCorner = TotalMinCorner + TotalDim;

    v2 InteriorMinCorner = TotalMinCorner + Frame;
    v2 InteriorMaxCorner = InteriorMinCorner + *Element->Dim;

    rectangle2 TotalBounds = RectMinMax(TotalMinCorner, TotalMaxCorner);
    Element->Bounds = RectMinMax(InteriorMinCorner, InteriorMaxCorner);

    if(Element->Interaction.Type && IsInRectangle(Element->Bounds, Layout->MouseP))
    {
        UIState->NextHotInteraction = Element->Interaction;
    }

    if(Element->Size)
    {
        PushRect(&UIState->RenderGroup, &NoTransform, RectMinMax(V2(TotalMinCorner.x, InteriorMinCorner.y),
                V2(InteriorMinCorner.x, InteriorMaxCorner.y)), 0.0f,
                 V4(0, 0, 0, 1));
        PushRect(&UIState->RenderGroup, &NoTransform, RectMinMax(V2(InteriorMaxCorner.x, InteriorMinCorner.y),
                                                     V2(TotalMaxCorner.x, InteriorMaxCorner.y)), 0.0f,
                 V4(0, 0, 0, 1));
        PushRect(&UIState->RenderGroup, &NoTransform, RectMinMax(V2(InteriorMinCorner.x, TotalMinCorner.y),
                                                     V2(InteriorMaxCorner.x, InteriorMinCorner.y)), 0.0f,
                 V4(0, 0, 0, 1));
        PushRect(&UIState->RenderGroup, &NoTransform, RectMinMax(V2(InteriorMinCorner.x, InteriorMaxCorner.y),
                                                     V2(InteriorMaxCorner.x, TotalMaxCorner.y)), 0.0f,
                 V4(0, 0, 0, 1));

        interaction SizeInteraction = {};
        SizeInteraction.Type = Interaction_Resize;
        SizeInteraction.P = Element->Size;

        rectangle2 SizeBox = AddRadiusTo(
            RectMinMax(V2(InteriorMaxCorner.x, TotalMinCorner.y),
                V2(TotalMaxCorner.x, InteriorMinCorner.y)), V2(4.0f, 4.0f));
        PushRect(&UIState->RenderGroup, &NoTransform, SizeBox, 0.0f,
                 (InteractionIsHot(UIState, SizeInteraction) ? V4(1, 1, 0, 1) : V4(1, 1, 1, 1)));
        if(IsInRectangle(SizeBox, Layout->MouseP))
        {
            UIState->NextHotInteraction = SizeInteraction;
        }
    }

    UIAdvanceElement(Layout, TotalBounds);
}

enum button_color
{
    BColor_None,
    BColor_Blue,
    BColor_Green,
    BColor_Red,
    BColor_Gray,
};

internal v2
UIButton(ui_layout *Layout, char *Name, interaction Interaction, r32 Width, button_color BColor = BColor_None,
         r32 Border = 20.0f, b32 Disabled = false)
{
    ui_state *UIState = Layout->UIState;

    rectangle2 TextBounds = UIGetTextSize(UIState, Name);
    v2 TextDim = GetDim(TextBounds);

    r32 OldScale = UIState->FontScale;
    while(TextDim.x > Width)
    {
        TextBounds = UIGetTextSize(UIState, Name);
        TextDim = GetDim(TextBounds);
        UIState->FontScale -= 0.01;
    }

    v2 ElementDim = {Width, Layout->LineAdvance};
    if(TextDim.x > ElementDim.x)
    {
        ElementDim = {TextDim.x + Border, Layout->LineAdvance + Border};
    }
    else
    {
        ElementDim += V2(Border, Border);
    }
    
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    if(Disabled)
    {
        interaction NullInteraction = {};
        UIDefaultInteraction(&Element, NullInteraction);
    }
    else
    {
        UIDefaultInteraction(&Element, Interaction);
    }
    UIEndElement(&Element);

    b32 IsHot = InteractionIsHot(Layout->UIState, Interaction);

    UITextOutAt(UIState, V2(GetMinCorner(Element.Bounds).x + 0.5f*ElementDim.x - 0.5f*TextDim.x,
                            GetMaxCorner(Element.Bounds).y - 0.5f*ElementDim.y - 
                            0.5f*UIState->FontScale*GetStartingBaselineY(UIState->FontInfo)),
                Name, 0.0f, V4(1.0f, 1.0f, 1.0f, 1));

    if(BColor == BColor_None)
    {
        PushRect(&UIState->RenderGroup, 
                 &UIState->BackingTransform, Element.Bounds, 0.0f, V4(0.2f, 0.5f, 0.5f, 1));
    }
    else
    {
        v2 ButtonDim = GetDim(Element.Bounds);
        v2 ButtonCenter = GetCenter(Element.Bounds);

        if(Disabled)
        {
            BColor = BColor_Gray;
        }
        
        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0),
                 ButtonDim,
                 Disabled ? UI_COLOR_RGBA1_3F3F3FFF : UI_COLOR_RGBA1_4D3020FF);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
                 ButtonDim - V2(4.0f, 4.0f),
                 Disabled ? UI_COLOR_RGBA1_7F7F7FFF : UI_COLOR_RGBA1_B97A57FF);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
                 ButtonDim - V2(8.0f, 8.0f),
                 Disabled ? UI_COLOR_RGBA1_3F3F3FFF : UI_COLOR_RGBA1_4D3020FF);

        v4 Color0 = {};
        v4 Color1 = {};
        v4 Color2 = {};

        switch(BColor)
        {
            case BColor_Green:
            {
                Color0 = UI_COLOR_RGBA1_B5E61DFF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_DDF398FF : UI_COLOR_RGBA1_22B14CFF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_22B14CFF : UI_COLOR_RGBA1_DDF398FF);
            } break;

            case BColor_Blue:
            {
                Color0 = UI_COLOR_RGBA1_99D9EAFF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_CFEEF5FF : UI_COLOR_RGBA1_00A2E8FF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_00A2E8FF : UI_COLOR_RGBA1_CFEEF5FF);
            } break;

            case BColor_Red:
            {
                Color0 = UI_COLOR_RGBA1_ED1C24FF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_F78C92FF : UI_COLOR_RGBA1_880015FF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_880015FF : UI_COLOR_RGBA1_F78C92FF);
            } break;

            case BColor_Gray:
            {
                Color0 = UI_COLOR_RGBA1_C3C3C3FF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_FFFFFFFF : UI_COLOR_RGBA1_7F7F7FFF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_7F7F7FFF : UI_COLOR_RGBA1_FFFFFFFF);
            } break;

            InvalidDefaultCase;
        }


        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
                 ButtonDim - V2(12.0f, 12.0f), Color0);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter + V2(0.5f*(ButtonDim.x - 14.0f), 0.0f), 4.0f),
                 V2(4.0f, ButtonDim.y - 12.0f), IsHot ? Color2 : Color1);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter - V2(0.5f*(ButtonDim.x - 14.0f), 0.0f), 4.0f),
                 V2(4.0f, ButtonDim.y - 12.0f), IsHot ? Color1 : Color2);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter - V2(0.0f, 0.5f*(ButtonDim.y - 16.0f)), 4.0f),
                 V2(ButtonDim.x - 16.0f, 4.0f), Color1);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter + V2(0.0f, 0.5f*(ButtonDim.y - 16.0f)), 4.0f),
                 V2(ButtonDim.x - 16.0f, 4.0f), Color2);
    }
    
    UIState->FontScale = OldScale;
    return(ElementDim);
}

internal v2
UIIncrementArrowButton(ui_layout *Layout, u32 *Value, u32 MaxValue = U32Maximum)
{
    ui_state *UIState = Layout->UIState;

    v2 ElementDim = {48.0f, 48.0f};
    
    u32 NewValue = *Value + 1;
    if(NewValue > (MaxValue - 1))
    {
        NewValue = MaxValue - 1;
    }

    interaction Interaction = UISetUInt32Interaction(InteractionID(UIState), Value, NewValue);
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, Interaction);
    UIEndElement(&Element);

    b32 IsHot = InteractionIsHot(Layout->UIState, Interaction);

    v2 ButtonDim = GetDim(Element.Bounds);
    v2 ButtonCenter = GetCenter(Element.Bounds);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0.0f), ButtonDim,
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
             ButtonDim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
             ButtonDim - V2(8.0f, 8.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
             ButtonDim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_CB9C83FF);

    v4 Color0 = IsHot ? UI_COLOR_RGBA1_B97A57FF : UI_COLOR_RGBA1_4D3020FF;
    v4 Color1 = IsHot ? UI_COLOR_RGBA1_DCBCABFF : UI_COLOR_RGBA1_B97A57FF;
    
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 4.0f),
             V2(4.0f, 28.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(8.0f, 0.0f), 4.0f),
             V2(12.0f, 12.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(3.0f, 0.0f), 4.0f),
             V2(2.0f, 24.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(5.0f, 0.0f), 4.0f),
             V2(2.0f, 20.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(7.0f, 0.0f), 4.0f),
             V2(2.0f, 16.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(9.0f, 0.0f), 4.0f),
             V2(2.0f, 12.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(11.0f, 0.0f), 4.0f),
             V2(2.0f, 8.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(13.0f, 0.0f), 4.0f),
             V2(2.0f, 4.0f),
             Color0);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(6.0f, 0.0f), 5.0f),
             V2(12.0f, 8.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(1.0f, 0.0f), 5.0f),
             V2(2.0f, 24.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(3.0f, 0.0f), 5.0f),
             V2(2.0f, 20.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(5.0f, 0.0f), 5.0f),
             V2(2.0f, 16.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(7.0f, 0.0f), 5.0f),
             V2(2.0f, 12.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(9.0f, 0.0f), 5.0f),
             V2(2.0f, 8.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(11.0f, 0.0f), 5.0f),
             V2(2.0f, 4.0f),
             Color1);

    return(ElementDim);
}

internal v2
UIDecrementArrowButton(ui_layout *Layout, u32 *Value, s32 MinValue = 0)
{
    ui_state *UIState = Layout->UIState;

    v2 ElementDim = {48.0f, 48.0f};
    
    
    s32 NewValue = *Value - 1;
    if(NewValue < MinValue)
    {
        NewValue = MinValue;
    }

    interaction Interaction = UISetUInt32Interaction(InteractionID(UIState), Value, NewValue);
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, Interaction);
    UIEndElement(&Element);

    b32 IsHot = InteractionIsHot(Layout->UIState, Interaction);

    v2 ButtonDim = GetDim(Element.Bounds);
    v2 ButtonCenter = GetCenter(Element.Bounds);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0.0f), ButtonDim,
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
             ButtonDim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
             ButtonDim - V2(8.0f, 8.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
             ButtonDim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_CB9C83FF);

    v4 Color0 = IsHot ? UI_COLOR_RGBA1_B97A57FF : UI_COLOR_RGBA1_4D3020FF;
    v4 Color1 = IsHot ? UI_COLOR_RGBA1_DCBCABFF : UI_COLOR_RGBA1_B97A57FF;
    
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 4.0f),
             V2(4.0f, 28.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(8.0f, 0.0f), 4.0f),
             V2(12.0f, 12.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(3.0f, 0.0f), 4.0f),
             V2(2.0f, 24.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(5.0f, 0.0f), 4.0f),
             V2(2.0f, 20.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(7.0f, 0.0f), 4.0f),
             V2(2.0f, 16.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(9.0f, 0.0f), 4.0f),
             V2(2.0f, 12.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(11.0f, 0.0f), 4.0f),
             V2(2.0f, 8.0f),
             Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(13.0f, 0.0f), 4.0f),
             V2(2.0f, 4.0f),
             Color0);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter + V2(6.0f, 0.0f), 5.0f),
             V2(12.0f, 8.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(1.0f, 0.0f), 5.0f),
             V2(2.0f, 24.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(3.0f, 0.0f), 5.0f),
             V2(2.0f, 20.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(5.0f, 0.0f), 5.0f),
             V2(2.0f, 16.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(7.0f, 0.0f), 5.0f),
             V2(2.0f, 12.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(9.0f, 0.0f), 5.0f),
             V2(2.0f, 8.0f),
             Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter - V2(11.0f, 0.0f), 5.0f),
             V2(2.0f, 4.0f),
             Color1);

    return(ElementDim);
}

internal v2
UIIncrementButton(ui_layout *Layout, u32 *Value, u32 MaxValue = U32Maximum)
{
    ui_state *UIState = Layout->UIState;

    v2 ElementDim = {36.0f, 36.0f};
    
    u32 NewValue = *Value + 1;
    if(NewValue > (MaxValue - 1))
    {
        NewValue = MaxValue - 1;
    }

    interaction Interaction = UISetUInt32Interaction(InteractionID(UIState), Value, NewValue);
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, Interaction);
    UIEndElement(&Element);

    b32 IsHot = InteractionIsHot(Layout->UIState, Interaction);

    v2 ButtonDim = GetDim(Element.Bounds);
    v2 ButtonCenter = GetCenter(Element.Bounds);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0.0f), ButtonDim,
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
             ButtonDim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
             ButtonDim - V2(8.0f, 8.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
             ButtonDim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_CB9C83FF);

    v4 Color0 = IsHot ? UI_COLOR_RGBA1_B5E61DFF : UI_COLOR_RGBA1_22B14CFF;
    v4 Color1 = IsHot ? UI_COLOR_RGBA1_22B14CFF : UI_COLOR_RGBA1_B5E61DFF;
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 4.0f),
             ButtonDim - V2(16.0f, 28.0f), Color0);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 4.0f),
             ButtonDim - V2(28.0f, 16.0f), Color0);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 5.0f),
             ButtonDim - V2(20.0f, 32.0f), Color1);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 5.0f),
             ButtonDim - V2(32.0f, 20.0f), Color1);

    return(ElementDim);
}


internal v2
UIScrollAdjustU32Button(ui_layout *Layout, char *Name, r32 Width, u32 *Value, button_color BColor = BColor_None,
                     u32 MinValue = 0, u32 MaxValue = U32Maximum, r32 Border = 20.0f)
{
    ui_state *UIState = Layout->UIState;

    rectangle2 TextBounds = UIGetTextSize(UIState, Name);
    v2 TextDim = GetDim(TextBounds);

    v2 ElementDim = {Width, Layout->LineAdvance};
    if(TextDim.x > ElementDim.x)
    {
        ElementDim = {TextDim.x + Border, Layout->LineAdvance + Border};
    }
    else
    {
        ElementDim += V2(Border, Border);
    }
    
    interaction Interaction = {};
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, Interaction);
    UIEndElement(&Element);

    b32 IsHot = IsInRectangle(Element.Bounds, Layout->MouseP);
    if(IsHot)
    {
        u32 NewValue = *Value + UIState->MouseZ;
        if(NewValue > MaxValue)
        {
            NewValue = MaxValue;
        }
    
        if(NewValue < MinValue)
        {
            NewValue = MinValue;
        }

        *(u32 *)Value = NewValue;
    }

    UITextOutAt(UIState, V2(GetMinCorner(Element.Bounds).x + 0.5f*ElementDim.x - 0.5f*TextDim.x,
                            GetMaxCorner(Element.Bounds).y - 0.5f*ElementDim.y - 
                            0.5f*UIState->FontScale*GetStartingBaselineY(UIState->FontInfo)),
                Name, 0.0f, V4(1.0f, 1.0f, 1.0f, 1));

    if(BColor == BColor_None)
    {
        PushRect(&UIState->RenderGroup, 
                 &UIState->BackingTransform, Element.Bounds, 0.0f, V4(0.2f, 0.5f, 0.5f, 1));
    }
    else
    {
        v2 ButtonDim = GetDim(Element.Bounds);
        v2 ButtonCenter = GetCenter(Element.Bounds);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0),
                 ButtonDim,
                 UI_COLOR_RGBA1_4D3020FF);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
                 ButtonDim - V2(4.0f, 4.0f),
                 UI_COLOR_RGBA1_B97A57FF);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
                 ButtonDim - V2(8.0f, 8.0f),
                 UI_COLOR_RGBA1_4D3020FF);

        v4 Color0 = {};
        v4 Color1 = {};
        v4 Color2 = {};

        switch(BColor)
        {
            case BColor_Green:
            {
                Color0 = UI_COLOR_RGBA1_B5E61DFF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_DDF398FF : UI_COLOR_RGBA1_22B14CFF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_22B14CFF : UI_COLOR_RGBA1_DDF398FF);
            } break;

            case BColor_Blue:
            {
                Color0 = UI_COLOR_RGBA1_99D9EAFF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_CFEEF5FF : UI_COLOR_RGBA1_00A2E8FF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_00A2E8FF : UI_COLOR_RGBA1_CFEEF5FF);
            } break;

            case BColor_Red:
            {
                Color0 = UI_COLOR_RGBA1_ED1C24FF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_F78C92FF : UI_COLOR_RGBA1_880015FF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_880015FF : UI_COLOR_RGBA1_F78C92FF);
            } break;

            InvalidDefaultCase;
        }


        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
                 ButtonDim - V2(12.0f, 12.0f), Color0);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter + V2(0.5f*(ButtonDim.x - 14.0f), 0.0f), 4.0f),
                 V2(4.0f, ButtonDim.y - 12.0f), IsHot ? Color2 : Color1);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter - V2(0.5f*(ButtonDim.x - 14.0f), 0.0f), 4.0f),
                 V2(4.0f, ButtonDim.y - 12.0f), IsHot ? Color1 : Color2);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter - V2(0.0f, 0.5f*(ButtonDim.y - 16.0f)), 4.0f),
                 V2(ButtonDim.x - 16.0f, 4.0f), Color1);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter + V2(0.0f, 0.5f*(ButtonDim.y - 16.0f)), 4.0f),
                 V2(ButtonDim.x - 16.0f, 4.0f), Color2);
    }
    
    return(ElementDim);
}

internal v2
UIScrollAdjustU8Button(ui_layout *Layout, char *Name, r32 Width, u8 *Value, button_color BColor = BColor_None,
                     u8 MinValue = 0, u8 MaxValue = 255, r32 Border = 20.0f)
{
    ui_state *UIState = Layout->UIState;

    rectangle2 TextBounds = UIGetTextSize(UIState, Name);
    v2 TextDim = GetDim(TextBounds);

    v2 ElementDim = {Width, Layout->LineAdvance};
    if(TextDim.x > ElementDim.x)
    {
        ElementDim = {TextDim.x + Border, Layout->LineAdvance + Border};
    }
    else
    {
        ElementDim += V2(Border, Border);
    }
    
    interaction Interaction = {};
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, Interaction);
    UIEndElement(&Element);

    b32 IsHot = IsInRectangle(Element.Bounds, Layout->MouseP);
    if(IsHot)
    {
        s32 NewValue = *Value + UIState->MouseZ;
        if(NewValue > MaxValue)
        {
            NewValue = MaxValue;
        }
    
        if(NewValue < MinValue)
        {
            NewValue = MinValue;
        }

        *(u8 *)Value = (u8)NewValue;
    }

    UITextOutAt(UIState, V2(GetMinCorner(Element.Bounds).x + 0.5f*ElementDim.x - 0.5f*TextDim.x,
                            GetMaxCorner(Element.Bounds).y - 0.5f*ElementDim.y - 
                            0.5f*UIState->FontScale*GetStartingBaselineY(UIState->FontInfo)),
                Name, 0.0f, V4(1.0f, 1.0f, 1.0f, 1));

    if(BColor == BColor_None)
    {
        PushRect(&UIState->RenderGroup, 
                 &UIState->BackingTransform, Element.Bounds, 0.0f, V4(0.2f, 0.5f, 0.5f, 1));
    }
    else
    {
        v2 ButtonDim = GetDim(Element.Bounds);
        v2 ButtonCenter = GetCenter(Element.Bounds);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0),
                 ButtonDim,
                 UI_COLOR_RGBA1_4D3020FF);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
                 ButtonDim - V2(4.0f, 4.0f),
                 UI_COLOR_RGBA1_B97A57FF);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
                 ButtonDim - V2(8.0f, 8.0f),
                 UI_COLOR_RGBA1_4D3020FF);

        v4 Color0 = {};
        v4 Color1 = {};
        v4 Color2 = {};

        switch(BColor)
        {
            case BColor_Green:
            {
                Color0 = UI_COLOR_RGBA1_B5E61DFF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_DDF398FF : UI_COLOR_RGBA1_22B14CFF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_22B14CFF : UI_COLOR_RGBA1_DDF398FF);
            } break;

            case BColor_Blue:
            {
                Color0 = UI_COLOR_RGBA1_99D9EAFF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_CFEEF5FF : UI_COLOR_RGBA1_00A2E8FF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_00A2E8FF : UI_COLOR_RGBA1_CFEEF5FF);
            } break;

            case BColor_Red:
            {
                Color0 = UI_COLOR_RGBA1_ED1C24FF;
                Color1 = (IsHot ? UI_COLOR_RGBA1_F78C92FF : UI_COLOR_RGBA1_880015FF);
                Color2 = (IsHot ? UI_COLOR_RGBA1_880015FF : UI_COLOR_RGBA1_F78C92FF);
            } break;

            InvalidDefaultCase;
        }


        PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
                 ButtonDim - V2(12.0f, 12.0f), Color0);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter + V2(0.5f*(ButtonDim.x - 14.0f), 0.0f), 4.0f),
                 V2(4.0f, ButtonDim.y - 12.0f), IsHot ? Color2 : Color1);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter - V2(0.5f*(ButtonDim.x - 14.0f), 0.0f), 4.0f),
                 V2(4.0f, ButtonDim.y - 12.0f), IsHot ? Color1 : Color2);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter - V2(0.0f, 0.5f*(ButtonDim.y - 16.0f)), 4.0f),
                 V2(ButtonDim.x - 16.0f, 4.0f), Color1);

        PushRect(&UIState->RenderGroup, &UIState->BackingTransform,
                 V3(ButtonCenter + V2(0.0f, 0.5f*(ButtonDim.y - 16.0f)), 4.0f),
                 V2(ButtonDim.x - 16.0f, 4.0f), Color2);
    }
    
    return(ElementDim);
}

internal v2
UIDecrementButton(ui_layout *Layout, u32 *Value, s32 MinValue = 0)
{
    ui_state *UIState = Layout->UIState;

    v2 ElementDim = {36.0f, 36.0f};
    
    s32 NewValue = *Value - 1;
    if(NewValue < MinValue)
    {
        NewValue = MinValue;
    }

    interaction Interaction = UISetUInt32Interaction(InteractionID(UIState), Value, NewValue);
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, Interaction);
    UIEndElement(&Element);

    b32 IsHot = InteractionIsHot(Layout->UIState, Interaction);

    v2 ButtonDim = GetDim(Element.Bounds);
    v2 ButtonCenter = GetCenter(Element.Bounds);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0.0f), ButtonDim,
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
             ButtonDim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
             ButtonDim - V2(8.0f, 8.0f),
             UI_COLOR_RGBA1_4D3020FF);
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
             ButtonDim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_CB9C83FF);

    v4 Color0 = IsHot ? UI_COLOR_RGBA1_FF7F27FF : UI_COLOR_RGBA1_880015FF;
    v4 Color1 = IsHot ? UI_COLOR_RGBA1_880015FF : UI_COLOR_RGBA1_FF7F27FF;
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 4.0f),
             ButtonDim - V2(16.0f, 28.0f), Color0);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 5.0f),
             ButtonDim - V2(20.0f, 32.0f), Color1);

    return(ElementDim);
}

internal v2
UIBasicTextElement(ui_layout *Layout, char *Text, interaction ItemInteraction, r32 Width, r32 Border = 20.0f)
{
    ui_state *UIState = Layout->UIState;

    rectangle2 TextBounds = UIGetTextSize(UIState, Text);
    v2 TextDim = GetDim(TextBounds);

    v2 ElementDim = {Width, TextDim.y + Border};
    if(TextDim.x > ElementDim.x - Border)
    {
        ElementDim = {TextDim.x + Border, TextDim.y + Border};
    }
    v2 Dim = ElementDim;
    
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, ItemInteraction);
    UIEndElement(&Element);

    b32 IsHot = InteractionIsHot(Layout->UIState, ItemInteraction);
    UITextOutAt(UIState, V2(GetMinCorner(Element.Bounds).x + 0.5f*ElementDim.x - 0.5f*TextDim.x,
                            GetMaxCorner(Element.Bounds).y - 0.5f*ElementDim.y + 0.5f*TextDim.y - 
                            UIState->FontScale*GetStartingBaselineY(UIState->FontInfo)),
                Text, 0.0f, V4(1.0f, 1.0f, 1.0f, 1));

    v2 ButtonDim = GetDim(Element.Bounds);
    v2 ButtonCenter = GetCenter(Element.Bounds);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0),
             ButtonDim,
             UI_COLOR_RGBA1_4D3020FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
             ButtonDim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
             ButtonDim - V2(8.0f, 8.0f),
             UI_COLOR_RGBA1_4D3020FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
             ButtonDim - V2(12.0f, 12.0f),
             IsHot ? UI_COLOR_RGBA1_EFE4B0FF : UI_COLOR_RGBA1_CB9C83FF);
    
    return(Dim);
}

internal void
UIBeginRow(ui_layout *Layout)
{
    ++Layout->NoLineFeed;
}

internal v2
UISpace(ui_layout *Layout, v2 Dim)
{
    ui_state *UIState = Layout->UIState;
    interaction NullInteraction = {};
    
    v2 ElementDim = Dim;
    
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, NullInteraction);
    UIEndElement(&Element);

    return(ElementDim);
}

internal v2
UILabel(ui_layout *Layout, char *Name, r32 Width, r32 Border = 20.0f, v4 Color = V4(1, 1, 1, 1))
{
    ui_state *UIState = Layout->UIState;
    interaction NullInteraction = {};

    rectangle2 TextBounds = UIGetTextSize(UIState, Name);
    v2 TextDim = GetDim(TextBounds);

    r32 OldScale = UIState->FontScale;
    while(TextDim.x > Width)
    {
        TextBounds = UIGetTextSize(UIState, Name);
        TextDim = GetDim(TextBounds);
        UIState->FontScale -= 0.01;
    }
    
    v2 ElementDim = {Width, TextDim.y};
    if(TextDim.x > ElementDim.x)
    {
        ElementDim = {TextDim.x + Border, TextDim.y + Border};
    }
    else
    {
        ElementDim += V2(Border, Border);
    }
    
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, NullInteraction);
    UIEndElement(&Element);

    UITextOutAt(UIState, V2(GetMinCorner(Element.Bounds).x + 0.5f*ElementDim.x - 0.5f*TextDim.x,
                            GetMaxCorner(Element.Bounds).y - 0.5f*ElementDim.y + 0.5f*TextDim.y - 
                            UIState->FontScale*GetStartingBaselineY(UIState->FontInfo)),
                Name, 0.0f, Color);

    v2 ButtonDim = GetDim(Element.Bounds);
    v2 ButtonCenter = GetCenter(Element.Bounds);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0),
             ButtonDim,
             UI_COLOR_RGBA1_4D3020FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
             ButtonDim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
             ButtonDim - V2(8.0f, 8.0f),
             UI_COLOR_RGBA1_4D3020FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
             ButtonDim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_B97A57FF);

    UIState->FontScale = OldScale;

    return(ElementDim);
}

#if 0
internal void
UILabelWithInEditorFont(ui_layout *Layout, char *Name, r32 Width, builder_loaded_font *Font, r32 FontScale = 1.0f, r32 Border = 20.0f)
{
    ui_state *UIState = Layout->UIState;
    interaction NullInteraction = {};

    rectangle2 StandardBound = UITextOpWithInEditorFont(UIState, UITextOp_SizeText, V2(0, 0), Name, Font, FontScale);
    v2 StandardDim = GetDim(StandardBound);

    rectangle2 TextBounds = {};
    if(StandardDim.x > Width)
    {
        FontScale = Width / StandardDim.x; 
        TextBounds = UITextOpWithInEditorFont(UIState, UITextOp_SizeText, V2(0, 0), Name, Font, FontScale);
    }
    else
    {
        TextBounds = StandardBound;
    }
    
    v2 TextDim = GetDim(TextBounds);
    v2 ElementDim = {TextDim.x + Border, TextDim.y + Border};
    
    ui_layout_element Element = UIBeginElementRectangle(Layout, &ElementDim);
    UIDefaultInteraction(&Element, NullInteraction);
    UIEndElement(&Element);

    v2 P = V2(GetMinCorner(Element.Bounds).x + 0.5f*ElementDim.x - 0.5f*TextDim.x,
              GetMaxCorner(Element.Bounds).y - 0.5f*ElementDim.y + 0.5f*TextDim.y - 
              FontScale*Font->AscenderHeight);

    UITextOpWithInEditorFont(UIState, UITextOp_DrawText, P, Name, Font, FontScale);

    v2 ButtonDim = GetDim(Element.Bounds);
    v2 ButtonCenter = GetCenter(Element.Bounds);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 0),
             ButtonDim,
             UI_COLOR_RGBA1_4D3020FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 1.0f),
             ButtonDim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 2.0f),
             ButtonDim - V2(8.0f, 8.0f),
             UI_COLOR_RGBA1_4D3020FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(ButtonCenter, 3.0f),
             ButtonDim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_CB9C83FF);

}
#endif

internal void
UIEndRow(ui_layout *Layout)
{
    Assert(Layout->NoLineFeed > 0);
    --Layout->NoLineFeed;

    UIAdvanceElement(Layout, RectMinMax(Layout->At, Layout->At));
}

internal void
BeginInteract(ui_state *UIState, engine_input *Input, v2 MouseP)
{
    UITextOutAt(UIState, V2(0, 0), "BeginInteract");
    if(UIState->HotInteraction.Type)
    {
        if(UIState->HotInteraction.Type == Interaction_AutoModifyVariable)
        {
        }

        switch(UIState->HotInteraction.Type)
        {
            case Interaction_TearValue:
            {
            } break;

            case Interaction_Select:
            {
            } break;                
        }

        UIState->Interaction = UIState->HotInteraction;
    }
    else
    {
        UIState->Interaction.Type = Interaction_NOP;
    }
}

internal void
EndInteract(ui_state *UIState, engine_input *Input, v2 MouseP)
{
    UITextOutAt(UIState, V2(0, 0), "EndInteract");
    switch(UIState->Interaction.Type)
    {
        case Interaction_ToggleExpansion:
        {
        } break;
        
        case Interaction_SetUInt32:
        {
            *(u32 *)UIState->Interaction.Target = UIState->Interaction.UInt32;
        } break;
        
        case Interaction_SetPointer:
        {
            *(void **)UIState->Interaction.Target = UIState->Interaction.Pointer;
        } break;

        case Interaction_ToggleValue:
        {
        } break;
    }

    UIState->Interaction.Type = Interaction_None;
    UIState->Interaction.Generic = 0;
}

internal void
Interact(ui_state *UIState, engine_input *Input, v2 MouseP)
{
    v2 dMouseP = MouseP - UIState->LastMouseP;
    if(UIState->Interaction.Type)
    {
        v2 *P = UIState->Interaction.P;

        // NOTE(casey): Mouse move interaction
        switch(UIState->Interaction.Type)
        {
            case Interaction_DragValue:
            {
            } break;

            case Interaction_Resize:
            {
                *P += V2(dMouseP.x, -dMouseP.y);
                P->x = Maximum(P->x, 10.0f);
                P->y = Maximum(P->y, 10.0f);
            } break;

            case Interaction_Move:
            {
                *P += V2(dMouseP.x, dMouseP.y);
            } break;
        }

        // NOTE(casey): Click interaction
        for(u32 TransitionIndex = Input->MouseButtons[PlatformMouseButton_Left].HalfTransitionCount;
            TransitionIndex > 1;
            --TransitionIndex)
        {
            EndInteract(UIState, Input, MouseP);
            BeginInteract(UIState, Input, MouseP);
        }

        if(!Input->MouseButtons[PlatformMouseButton_Left].EndedDown)
        {
            EndInteract(UIState, Input, MouseP);
        }
    }
    else
    {
        UIState->HotInteraction = UIState->NextHotInteraction;

        for(u32 TransitionIndex = Input->MouseButtons[PlatformMouseButton_Left].HalfTransitionCount;
            TransitionIndex > 1;
            --TransitionIndex)
        {
            BeginInteract(UIState, Input, MouseP);
            EndInteract(UIState, Input, MouseP);
        }

        if(Input->MouseButtons[PlatformMouseButton_Left].EndedDown)
        {
            BeginInteract(UIState, Input, MouseP);
        }
    }

    UIState->LastMouseP = MouseP;
}

internal void
BeginUI(ui_state *UIState, editor_render_commands *Commands, editor_assets *Assets,
        u32 MainGenerationID, u32 Width, u32 Height)
{
    if(!UIState->Initialized)
    {
//        InitializeStrings(&UIState->StringsArena);

        UIState->JsonStringsHead = ParseJson("enum_strings.json", &UIState->UIArena);
        
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
//    Orthographic(&UIState->RenderGroup, 1.0f);
    Perspective(&UIState->RenderGroup, 4.0f, 0.6f, 2.4f);
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

    UIState->NextInteractionID = 1;
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
                for(u32 I = CursorLastIndex;
                    I > 0;
                    --I)
                {
                    Cursor->Array[I] = Cursor->Array[I - 1];
                }

                if(NewFirst == -1)
                {
                    NewFirst = SourceArrayCount - 1;
                }

                Cursor->Array[0] = NewFirst;
            }
            else
            {
                u32 NewLast = Cursor->Array[CursorLastIndex] + 1;

                for(u32 I = 0;
                    I < Cursor->ArrayCount;
                    ++I)
                {
                    Cursor->Array[I] = Cursor->Array[I + 1];
                }

                if(NewLast == SourceArrayCount)
                {
                    NewLast = 0;
                }
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
                    for(u32 I = CursorLastIndex;
                        I > 0;
                        --I)
                    {
                        Cursor->Array[I] = Cursor->Array[I - 1];
                    }

                    if(NewFirst == -1)
                    {
                        NewFirst = SourceArrayCount - 1;
                    }

                    Cursor->Array[0] = NewFirst;
                }
                else
                {
                    --Cursor->ArrayPosition;
                }
            }
            else
            {
                if(Cursor->ArrayPosition == CursorLastIndex)
                {
                    u32 NewLast = Cursor->Array[CursorLastIndex] + 1;

                    for(u32 I = 0;
                        I < Cursor->ArrayCount;
                        ++I)
                    {
                        Cursor->Array[I] = Cursor->Array[I + 1];
                    }

                    if(NewLast == SourceArrayCount)
                    {
                        NewLast = 0;
                    }
                    Cursor->Array[CursorLastIndex] = NewLast;
                }
                else
                {
                    ++Cursor->ArrayPosition;
                }
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

enum scroll_data_type
{
    ScrollDataType_Strings,
    ScrollDataType_Tags,
    ScrollDataType_StoredAssets,
};

inline v2
DrawAssetScrollElement(ui_layout *Layout, u32 *ReturnIndex, r32 Width, u32 AssetIndex, stored_asset *Assets)
{
    ui_state *UIState = Layout->UIState;
    stored_asset Asset = Assets[AssetIndex];

    char *TypeIDString = JsonGetEnumString(UIState->JsonStringsHead, "AssetType", Asset.TypeID);
    char *TypeString = JsonGetEnumString(UIState->JsonStringsHead, "StoredAssetType", Asset.Type);

    char Text[512];
    FormatString(ArrayCount(Text), Text,
                 "%d. AssetID: %d\nAssetTypeID: %s\n AssetType: %s\n TagCount: %d",
                 AssetIndex, Asset.ID, TypeIDString, TypeString, Asset.TagCount);

    interaction ItemInteraction = UISetUInt32Interaction(InteractionID(UIState), ReturnIndex, AssetIndex);
    v2 Dim  = UIBasicTextElement(Layout, Text, ItemInteraction, Width);

    return(Dim);
}

inline v2
DrawStringScrollElement(ui_layout *Layout, u32 *ReturnIndex, r32 Width, u32 AssetIndex, char **Strings)
{
    ui_state *UIState = Layout->UIState;

    char Text[256];
    FormatString(ArrayCount(Text), Text, "%d. %s", AssetIndex, Strings[AssetIndex]);
    interaction ItemInteraction = UISetUInt32Interaction(InteractionID(UIState), ReturnIndex, AssetIndex);
    v2 Dim  = UIBasicTextElement(Layout, Text, ItemInteraction, Width);

    return(Dim);
}

inline v2
DrawTagScrollElement(ui_layout *Layout, u32 *ReturnIndex, r32 Width, u32 AssetIndex, ssa_tag *Tags, b32 ShowExtra)
{
    ui_state *UIState = Layout->UIState;

    ssa_tag Tag = Tags[AssetIndex];
    char *TagString = JsonGetEnumString(UIState->JsonStringsHead, "AssetTag", Tag.ID);
    char Text[256];
    if(ShowExtra)
    {
        u32 TagValue = Tag.Value;
        char *ValueKey = JsonGetTagValueEnumKey(UIState->JsonStringsHead, Tag.ID);
        if(StringsAreEqual(ValueKey, "Number"))
        {
            FormatString(ArrayCount(Text), Text, "%d. %s, %#x", AssetIndex, TagString, TagValue);
        }
        else
        {
            char *ValueString = JsonGetEnumString(UIState->JsonStringsHead, ValueKey, TagValue);
            FormatString(ArrayCount(Text), Text, "%d. %s, %s", AssetIndex, TagString, ValueString);
        }
    }
    else
    {
        FormatString(ArrayCount(Text), Text, "%d. %s, %d", AssetIndex, TagString, Tag.Value);
    }

    interaction ItemInteraction = UISetUInt32Interaction(InteractionID(UIState), ReturnIndex, AssetIndex);
    v2 Dim  = UIBasicTextElement(Layout, Text, ItemInteraction, Width);

    return(Dim);
}

internal void
UIDrawScrollWindow(ui_state *UIState, ui_layout *Layout, char *Name, v2 DimInit, u32 *ReturnIndex,
                   char *ButtonText, u32 ScrollDataType, u32 ElementsToShow, u32 ElementCount, void *Data,
                   b32 ShowExtra = false)
{
    ui_object *UIObject = GetOrCreateUIObject(UIState, GetUIObjectID(Name));
    if(!UIObject->Initialized)
    {
        UIObject->Initialized = true;

        UIObject->Type = UIObjectType_ScrollWindow;
        UIObject->ScrollWindow.Dim = DimInit;
        UIObject->ScrollWindow.Viewable = false;
        UIObject->ScrollWindow.CursorElementCount = ElementCount;
        InitializeCursor(&UIObject->ScrollWindow.Cursor, 1);
    }

    ui_scroll_window *Window = &UIObject->ScrollWindow;
    array_cursor *Cursor = &Window->Cursor;
    if(Window->CursorElementCount != ElementCount)
    {
        Window->CursorElementCount = ElementCount;
        ResetCursorArray(Cursor);
    }

    render_group *RenderGroup = &UIState->RenderGroup;
    if(Window->Viewable)
    {
        interaction NullInteraction = {};
        ui_layout_element Element = UIBeginElementRectangle(Layout, &Window->Dim);
        UIDefaultInteraction(&Element, NullInteraction);
        UIEndElement(&Element);

        object_transform InitTransform = UIState->BackingTransform;
        v2 Center = GetCenter(Element.Bounds);
        v2 Dim = GetDim(Element.Bounds);
        v2 HalfDim = 0.5f*Dim;
        r32 YOffset = HalfDim.y - 42.0f;

        UIDrawWindowOutline(UIState, Center, Dim);

        ui_layout WindowLayout = UIBeginLayout(UIState, Layout->MouseP, UIState->BackingTransform.OffsetP.xy +
                                               V2(-HalfDim.x + 22.0f, HalfDim.y - 70.0f));
        // NOTE(paul): Close button
        interaction CloseInteraction = UISetUInt32Interaction(InteractionID(UIState),
                                                              (u32 *)&Window->Viewable, false);
        v2 CloseDim = V2(20.0f, 20.0f);
        rectangle2 CloseButton = RectCenterDim(UIState->BackingTransform.OffsetP.xy +
                                               V2(HalfDim.x - 42.0f, YOffset), CloseDim);
        if(IsInRectangle(CloseButton, Layout->MouseP))
        {
            UIState->NextHotInteraction = CloseInteraction;
        }
        PushRect(RenderGroup, &UIState->BackingTransform, V3(HalfDim.x - 42.0f, YOffset, 17.0f),
                 GetDim(CloseButton), V4(0.6f, 0, 0, 1));

        // NOTE(paul): Rendering insides
        u32 OldClipRect = RenderGroup->CurrentClipRectIndex;
        RenderGroup->CurrentClipRectIndex = 
            PushClipRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 0), Dim - V2(44.0f, 44.0f), 0);

        UIState->BackingTransform = InitTransform;
        UIState->BackingTransform.ChunkZ = 145000;

        r32 Border = 20.0f;
        u32 SourceArrayIndex = Cursor->Array[Cursor->ArrayPosition];

        if(ElementsToShow > Window->CursorElementCount)
        {
            ElementsToShow = Window->CursorElementCount;
        }

        if(ElementsToShow != Cursor->ArrayCount)
        {
            ReInitializeCursor(Cursor, ElementsToShow);
        }
        
        v2 ElementDim = {};
        for(u32 Index = 0;
            Index < ElementsToShow;
            ++Index)
        {
            u32 AssetIndex = Cursor->Array[Index];

            switch(ScrollDataType)
            {
                case ScrollDataType_StoredAssets:
                {
                    stored_asset *Assets = (stored_asset *)Data;
                    ElementDim = DrawAssetScrollElement(&WindowLayout, ReturnIndex, Dim.x - 44.0f,
                                                        AssetIndex, Assets);
                } break;

                case ScrollDataType_Strings:
                {
                    char **Strings = (char **)Data;
                    ElementDim = DrawStringScrollElement(&WindowLayout, ReturnIndex, Dim.x - 44.0f,
                                                         AssetIndex, Strings);
                } break;

                case ScrollDataType_Tags:
                {
                    ssa_tag *Tags = (ssa_tag *)Data;
                    ElementDim = DrawTagScrollElement(&WindowLayout, ReturnIndex, Dim.x - 44.0f,
                                                      AssetIndex, Tags, ShowExtra);
                } break;

                InvalidDefaultCase;
            }

            if(ElementDim.x > Window->Dim.x - 44.0f)
            {
                Window->Dim.x = ElementDim.x + 64.0f + Border;
            }
        }

        r32 TotalYDim = ElementsToShow*ElementDim.y;
        Window->Dim.y = TotalYDim + 114.0f;
        
        v2 ScrollDim = Dim - V2(44.0f, 44.0f);
        rectangle2 ScrollBounds = RectCenterDim(Center, ScrollDim);
        
        if(IsInRectangle(ScrollBounds, Layout->MouseP) && UIState->MouseZ != 0)
        {
            ChangeCursorPositionForScrollWindow(Cursor, Window->CursorElementCount, UIState->MouseZ);
        }
        
        UIEndLayout(&WindowLayout);
        RenderGroup->CurrentClipRectIndex = OldClipRect;
        
        UIState->BackingTransform = InitTransform;
    }
    else
    {
        UIButton(Layout, ButtonText, UISetUInt32Interaction(InteractionID(UIState), (u32 *)&Window->Viewable, true),
                 DimInit.x, BColor_Blue);
    }
}

internal v2
UIPictureElement(ui_layout *Layout, r32 Width, loaded_bitmap *Bitmap, bitmap_id BitmapID = {}, v4 Color = V4(1, 1, 1, 1))
{
    ui_state *UIState = Layout->UIState;
    interaction NullInteraction = {};
    
    v2 Dim = V2(Width, Width);
    ui_layout_element Element = UIBeginElementRectangle(Layout, &Dim);
    UIDefaultInteraction(&Element, NullInteraction);
    UIEndElement(&Element);

    v2 PictureDim = GetDim(Element.Bounds);
    v2 PictureCenter = GetCenter(Element.Bounds);

    r32 BitmapScale = 0.0f;
    r32 BitmapHeight = 0.0f;
    if(Bitmap)
    {
        BitmapScale = CalculateBitmapScaleForSquareCanvas(PictureDim.x - 12.0f, Bitmap->Width, Bitmap->Height);
        BitmapHeight = BitmapScale*Bitmap->Height;
        PushBitmap(&UIState->RenderGroup, &UIState->BackingTransform, Bitmap, BitmapHeight, V3(PictureCenter, 4.0f), Color);
    }
    else
    {
        Bitmap = GetBitmap(UIState->RenderGroup.Assets, BitmapID, UIState->RenderGroup.GenerationID);
        if(Bitmap)
        {
            BitmapScale = CalculateBitmapScaleForSquareCanvas(PictureDim.x - 12.0f, Bitmap->Width, Bitmap->Height);
            BitmapHeight = BitmapScale*Bitmap->Height;
    
            PushBitmap(&UIState->RenderGroup, &UIState->BackingTransform, Bitmap, BitmapHeight, V3(PictureCenter, 4.0f), Color);
        }
        else
        {
            PushBitmap(&UIState->RenderGroup, &UIState->BackingTransform, BitmapID, 1.0f, V3(0, 0, 0));
        }
    }
    
    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(PictureCenter, 0),
             PictureDim,
             UI_COLOR_RGBA1_4D3020FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(PictureCenter, 1.0f),
             PictureDim - V2(4.0f, 4.0f),
             UI_COLOR_RGBA1_B97A57FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(PictureCenter, 2.0f),
             PictureDim - V2(8.0f, 8.0f),
             UI_COLOR_RGBA1_4D3020FF);

    PushRect(&UIState->RenderGroup, &UIState->BackingTransform, V3(PictureCenter, 3.0f),
             PictureDim - V2(12.0f, 12.0f),
             UI_COLOR_RGBA1_67412CFF);

    return(PictureDim);
}

internal void
UIDrawAssetAdvanceView(ui_state *UIState, ui_layout *Layout, char *Name, v2 DimInit,
                       editor_mode_assets *AssetsMode)
{
    stored_asset *StoredAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex; 

    ui_window Window = {};
    Window.Dim = DimInit;
    
    render_group *RenderGroup = &UIState->RenderGroup;

    interaction NullInteraction = {};
    ui_layout_element Element = UIBeginElementRectangle(Layout, &Window.Dim);
    UIDefaultInteraction(&Element, NullInteraction);
    UIEndElement(&Element);
        
    object_transform InitTransform = UIState->BackingTransform;
    v2 Center = GetCenter(Element.Bounds);
    v2 Dim = GetDim(Element.Bounds);
    v2 HalfDim = 0.5f*Dim;
    r32 YOffset = HalfDim.y - 42.0f;

    UIDrawWindowOutline(UIState, Center, Dim, false);

    ui_layout WindowLayout = UIBeginLayout(UIState, Layout->MouseP, UIState->BackingTransform.OffsetP.xy +
                                           V2(-HalfDim.x + 24.0f, HalfDim.y - 24.0f));

    UIState->BackingTransform = InitTransform;
    UIState->BackingTransform.ChunkZ = 140000;

    char Buffer[256];

    UIBeginRow(&WindowLayout);
    v2 ArrowButtonDim = UIDecrementArrowButton(&WindowLayout, &AssetsMode->ShowStoredAssetIndex);
    FormatString(ArrayCount(Buffer), Buffer, "Stored Asset: %d", AssetsMode->ShowStoredAssetIndex);
    UILabel(&WindowLayout, Buffer, 1400.0f, 30.0f);
    UIEndRow(&WindowLayout);

    ui_layout WindowRightLayout = UIBeginLayout(UIState, Layout->MouseP, WindowLayout.BaseCorner +
                                                V2(Dim.x - ArrowButtonDim.x - 48.0f, 0.0f));
    UIIncrementArrowButton(&WindowRightLayout, &AssetsMode->ShowStoredAssetIndex,
                           AssetsMode->StoredHeader.AssetCount);

    switch(StoredAsset->Type)
    {
        case StoredAssetType_Bitmap:
        {
            bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
            stored_asset_bitmap *StoredBitmap = &StoredAsset->Bitmap;

            UILabel(&WindowLayout, StoredBitmap->FileName, 1512.0f);
            UIPictureElement(&WindowLayout, 640.0f, &BitmapMode->Bitmap);

            ui_layout LeftLayout = UIBeginLayout(UIState, Layout->MouseP,
                                                 (WindowLayout.BaseCorner + V2(640.0f, -ArrowButtonDim.y - 58.0f)));

            UIBeginRow(&LeftLayout);
            FormatString(ArrayCount(Buffer), Buffer, "Width: %d pixels", BitmapMode->Bitmap.Width);
            UILabel(&LeftLayout, Buffer, 424.0f);
            FormatString(ArrayCount(Buffer), Buffer, "Height: %d pixels", BitmapMode->Bitmap.Height);
            UILabel(&LeftLayout, Buffer, 424.0f);
            UIEndRow(&LeftLayout);

            FormatString(ArrayCount(Buffer), Buffer, "AlignPercentage: V2(%.02f, %.02f)",
                         StoredBitmap->AlignPercentage.x, StoredBitmap->AlignPercentage.y);
            UILabel(&LeftLayout, Buffer, 872.0f);
            UIEndLayout(&LeftLayout);
        } break;

        case StoredAssetType_SpriteSheet:
        {
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
            stored_asset_spritesheet *SpriteSheet = &StoredAsset->SpriteSheet;

            UILabel(&WindowLayout, SpriteSheet->SourceFileName, 1512.0f);
            u32 SpriteIndex = (FloorReal32ToInt32(AssetsMode->Time*SpriteSheet->SpriteCount) %
                               SpriteSheet->SpriteCount);
            loaded_bitmap *SpriteBitmap = SpriteSheetMode->Sprites + SpriteIndex;
            UIPictureElement(&WindowLayout, 640.0f, SpriteBitmap);

            ui_layout LeftLayout = UIBeginLayout(UIState, Layout->MouseP,
                                                 (WindowLayout.BaseCorner + V2(640.0f, -ArrowButtonDim.y - 58.0f)));

            UIBeginRow(&LeftLayout);
            FormatString(ArrayCount(Buffer), Buffer, "Sprite Width: %d pixels", SpriteSheet->SpriteWidth);
            UILabel(&LeftLayout, Buffer, 424.0f);
            FormatString(ArrayCount(Buffer), Buffer, "Sprite Height: %d pixels", SpriteSheet->SpriteHeight);
            UILabel(&LeftLayout, Buffer, 424.0f);
            UIEndRow(&LeftLayout);

            FormatString(ArrayCount(Buffer), Buffer, "AlignPercentage: V2(%.02f, %.02f)",
                         SpriteSheet->SpriteAlignPercentage.x, SpriteSheet->SpriteAlignPercentage.y);
            UILabel(&LeftLayout, Buffer, 872.0f);
            FormatString(ArrayCount(Buffer), Buffer, "Sprite Count: %d", SpriteSheet->SpriteCount);
            UILabel(&LeftLayout, Buffer, 872.0f);
            UIEndLayout(&LeftLayout);
        } break;

        case StoredAssetType_Tileset:
        {
            tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
            stored_asset_tileset *StoredTileset = &StoredAsset->Tileset;

            u32 TileIndex = (FloorReal32ToInt32(AssetsMode->Time) %
                             StoredTileset->TileCount);
            loaded_bitmap *TileBitmap = TilesetMode->Tiles + TileIndex;
            UIPictureElement(&WindowLayout, 640.0f, TileBitmap);

            ui_layout LeftLayout = UIBeginLayout(UIState, Layout->MouseP,
                                                 (WindowLayout.BaseCorner + V2(640.0f, -ArrowButtonDim.y - 8.0f)));
            FormatString(ArrayCount(Buffer), Buffer, "Source: %s", StoredTileset->SourceFileName);
            UILabel(&LeftLayout, Buffer, 872.0f);

            UIBeginRow(&LeftLayout);
            FormatString(ArrayCount(Buffer), Buffer, "Tile Width: %d pixels", StoredTileset->TileWidth);
            UILabel(&LeftLayout, Buffer, 424.0f);
            FormatString(ArrayCount(Buffer), Buffer, "Tile Height: %d pixels", StoredTileset->TileHeight);
            UILabel(&LeftLayout, Buffer, 424.0f);
            UIEndRow(&LeftLayout);

            FormatString(ArrayCount(Buffer), Buffer, "Tile Count: %d", StoredTileset->TileCount);
            UILabel(&LeftLayout, Buffer, 872.0f);

            FormatString(ArrayCount(Buffer), Buffer, "Is Merged: %s", StoredTileset->MergedTile ? "true" : "false");
            UILabel(&LeftLayout, Buffer, 872.0f);
            FormatString(ArrayCount(Buffer), Buffer, "Merge Tile Source: %s", StoredTileset->MergeTileFileName);
            UILabel(&LeftLayout, Buffer, 872.0f);
            UIEndLayout(&LeftLayout);
        } break;

        case StoredAssetType_Font:
        {
            font_mode *FontMode = &AssetsMode->FontMode;
            stored_asset_font *StoredFont = &StoredAsset->Font;

            FormatString(ArrayCount(Buffer), Buffer, "Source: %s", StoredFont->SourceFileName);
            UILabel(&WindowLayout, Buffer, 1512.0f);
#if 0
            if(FontMode->Font.GlyphCount)
            {
                UILabelWithInEditorFont(&WindowLayout, "abcdefghijklmnopqrstuvwxyz\nABCDEFGHIJKLMNOPQRSTUVWXYZ\n123456789.:,;'\"(!?)+-*/=",
                                        1512.0f, &FontMode->Font, 3.0f);
            }
#endif
            
            UIBeginRow(&WindowLayout);
            FormatString(ArrayCount(Buffer), Buffer, "Code Point Count: %d", StoredFont->CodePointCount);
            UILabel(&WindowLayout, Buffer, 744.0f, 20.0f);
            FormatString(ArrayCount(Buffer), Buffer, "Font Size: %d pixels", StoredFont->FontSizeInPixels);
            UILabel(&WindowLayout, Buffer, 744.0f, 20.0f);
            UIEndRow(&WindowLayout);

            UIBeginRow(&WindowLayout);
            FormatString(ArrayCount(Buffer), Buffer, "First Code Point: %#x", StoredFont->FirstCodePoint);
            UILabel(&WindowLayout, Buffer, 744.0f);
            FormatString(ArrayCount(Buffer), Buffer, "Last Code Point: %#x", StoredFont->LastCodePoint);
            UILabel(&WindowLayout, Buffer, 744.0f);
            UIEndRow(&WindowLayout);

            UISpace(&WindowLayout, V2(1512.0f, 30.0f));
        } break;

        case StoredAssetType_Text:
        {
            text_mode *TextMode = &AssetsMode->TextMode;
            stored_asset_text *StoredText = &StoredAsset->Text;

            FormatString(ArrayCount(Buffer), Buffer, "Source: %s", StoredText->SourceFileName);
            UILabel(&WindowLayout, Buffer, 1512.0f);

            UILabel(&WindowLayout, TextMode->Text.String, 1512.0f);
            UISpace(&WindowLayout, V2(1512.0f, 30.0f));
        } break;

        case StoredAssetType_Sound:
        {
            sound_mode *SoundMode = &AssetsMode->SoundMode;
            stored_asset_sound *StoredSound = &StoredAsset->Sound;

            FormatString(ArrayCount(Buffer), Buffer, "Source: %s", StoredSound->SourceFileName);
            UILabel(&WindowLayout, Buffer, 1512.0f);

            UIBeginRow(&WindowLayout);
            FormatString(ArrayCount(Buffer), Buffer, "First Sample Index: %d", StoredSound->FirstSampleIndex);
            UILabel(&WindowLayout, Buffer, 744.0f);
            char *ChainString = JsonGetEnumString(UIState->JsonStringsHead, "SSASoundChain", StoredSound->Chain);
            FormatString(ArrayCount(Buffer), Buffer, "Chain: %s", ChainString);
            UILabel(&WindowLayout, Buffer, 744.0f);
            UIEndRow(&WindowLayout);

            UIBeginRow(&WindowLayout);
            FormatString(ArrayCount(Buffer), Buffer, "Sample Count: %d", SoundMode->Sound.SampleCount);
            UILabel(&WindowLayout, Buffer, 744.0f);
            FormatString(ArrayCount(Buffer), Buffer, "Channel Count: %d", SoundMode->Sound.ChannelCount);
            UILabel(&WindowLayout, Buffer, 744.0f);
            UIEndRow(&WindowLayout);

            UISpace(&WindowLayout, V2(1512.0f, 30.0f));
        } break;

        case StoredAssetType_File:
        {
            binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
            stored_asset_binary_file *StoredFile = &StoredAsset->File;

            FormatString(ArrayCount(Buffer), Buffer, "Source: %s", StoredFile->SourceFileName);
            UILabel(&WindowLayout, Buffer, 1512.0f);

            FormatString(ArrayCount(Buffer), Buffer, "File Size: %d", StoredFile->FileSize);
            UILabel(&WindowLayout, Buffer, 1512.0f);

            UISpace(&WindowLayout, V2(1512.0f, 30.0f));
        } break;
    }

    UILabel(&WindowLayout, "Stored Attributes: ", 1512.0f);
    char *TypeID = JsonGetEnumString(UIState->JsonStringsHead, "AssetType", StoredAsset->TypeID);
    char *StoredType = JsonGetEnumString(UIState->JsonStringsHead, "StoredAssetType", StoredAsset->Type);
    FormatString(ArrayCount(Buffer), Buffer, "InEditorID: %d, TypeID: %s, StoredType: %s",
                 StoredAsset->ID, TypeID, StoredType);
    UILabel(&WindowLayout, Buffer, 1512.0f);

    u32 Value = 0;
    UIDrawScrollWindow(UIState, &WindowLayout, "Stored Asset Preview Tags",
                       V2(1512.0f, 320.0f), &Value, "Stored Tags", ScrollDataType_Tags,
                       6, StoredAsset->TagCount, StoredAsset->AssetTags, true);

    ui_layout BottomLayout =
        UIBeginLayout(UIState, Layout->MouseP, (WindowLayout.BaseCorner + V2(0.0f, -1280.0f)));
    UIBeginRow(&BottomLayout);
    FormatString(ArrayCount(Buffer), Buffer, "Edit Stored Asset %d ", AssetsMode->ShowStoredAssetIndex);
    UIButton(&BottomLayout, Buffer, 
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->EditStoredAsset, true),
             744.0f, BColor_Green, 20.0f, (AssetsMode->ShowStoredAssetIndex == 0));

    FormatString(ArrayCount(Buffer), Buffer, "Remove StoredAsset %d ", AssetsMode->ShowStoredAssetIndex);
    UIButton(&BottomLayout, Buffer, 
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->RemoveStoredAsset, true),
             744.0f, BColor_Red, 20.0f, (AssetsMode->ShowStoredAssetIndex == 0));
    UIEndRow(&BottomLayout);
    
    // NOTE(paul): Rendering insides
    u32 OldClipRect = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = 
        PushClipRect(RenderGroup, &UIState->BackingTransform, V3(0, 0, 0), Dim - V2(44.0f, 44.0f), 0);
        
    UIEndLayout(&WindowRightLayout);
    UIEndLayout(&WindowLayout);
    RenderGroup->CurrentClipRectIndex = OldClipRect;
    UIState->BackingTransform = InitTransform;
}

inline void
DrawStandardEditLayout(ui_state *UIState, ui_layout *Layout, ui_layout *RightMenuLayout,
                       editor_mode_assets *AssetsMode, stored_asset *CurrentAsset)
{
    char Buffer[512];

    u32 FileCount = 0;
    char **FileStrings = 0;
    char *Text = 0;
    switch(AssetsMode->EditMode)
    {
        case EditMode_Bitmap:
        {
            FileCount = AssetsMode->BitmapFileCount; 
            FileStrings = AssetsMode->BitmapFiles;
            Text = "Choose Bitmap";
        } break;

        case EditMode_SpriteSheet:
        {
            FileCount = AssetsMode->SpriteSheetFileCount; 
            FileStrings = AssetsMode->SpriteSheetFiles;
            Text = "Choose SpriteSheet";
        } break;

        case EditMode_Tileset:
        {
            FileCount = AssetsMode->TilesetFileCount; 
            FileStrings = AssetsMode->TilesetFiles;
            Text = "Choose Tileset";
        } break;

        case EditMode_Sound:
        {
            FileCount = AssetsMode->SoundFileCount; 
            FileStrings = AssetsMode->SoundFiles;
            Text = "Choose Sound";
        } break;

        case EditMode_Text:
        {
            FileCount = AssetsMode->TextFileCount; 
            FileStrings = AssetsMode->TextFiles;
            Text = "Choose Text";
        } break;

        case EditMode_Font:
        {
            FileCount = AssetsMode->FontFileCount; 
            FileStrings = AssetsMode->FontFiles;
            Text = "Choose Font";
        } break;

        case EditMode_File:
        {
            FileCount = AssetsMode->BinaryFileCount; 
            FileStrings = AssetsMode->BinaryFiles;
            Text = "Choose File";
        } break;

        case EditMode_SSWM:
        {
            FileCount = AssetsMode->SSWMFileCount; 
            FileStrings = AssetsMode->SSWMFiles;
            Text = "Choose SSWM";
        } break;

        InvalidDefaultCase;
    }

    Assert((FileStrings != 0) && (FileCount != 0) && (Text != 0));
    
    if(AssetsMode->EditMode == EditMode_Tileset)
    {
        UIBeginRow(Layout);
        UIDrawScrollWindow(UIState, Layout, "Tileset Files Preview", V2(278.0f, 200.0f), &AssetsMode->FileIndex,
                           Text, ScrollDataType_Strings, 5, FileCount, FileStrings);
        UIDrawScrollWindow(UIState, Layout, "Merge Tile Files Preview", V2(278.0f, 200.0f), &AssetsMode->SubFileIndex,
                           "Choose Merge Tile", ScrollDataType_Strings, 2,
                           AssetsMode->SolidTileFileCount, AssetsMode->SolidTileFiles);
        UIEndRow(Layout);            
    }
    else
    {
        UIDrawScrollWindow(UIState, Layout, "Stored Asset Files Preview", V2(580.0f, 400.0f), &AssetsMode->FileIndex,
                           Text, ScrollDataType_Strings, 5, FileCount, FileStrings);
    }
#if 0            
    string_array *AssetStringArray = GetOrCreateStringArray(UIState, "AssetType", Asset_Count);
    UIDrawScrollWindow(UIState, Layout, "Asset Types Preview", V2(580.0f, 400.0f), &CurrentAsset->TypeID,
                       "Choose TypeID", ScrollDataType_Strings, 5,
                       AssetStringArray->StringCount, AssetStringArray->Strings);
            
    string_array *TagStringArray = GetOrCreateStringArray(UIState, "AssetTag", Tag_Count);
    UIDrawScrollWindow(UIState, Layout, "Asset Tags Preview", V2(580.0f, 400.0f), &AssetsMode->CurrentTagID,
                       "Choose Tag", ScrollDataType_Strings, 5,
                       TagStringArray->StringCount, TagStringArray->Strings);
#endif
    char *TagString = JsonGetEnumString(UIState->JsonStringsHead, "AssetTag", AssetsMode->CurrentTagID);
    FormatString(ArrayCount(Buffer), Buffer, "Current Tag: %s", TagString);

    UILabel(Layout, Buffer, 580.0f);

    if(AssetsMode->CurrentTagID != AssetsMode->LastTagID)
    {
        AssetsMode->LastTagID = AssetsMode->CurrentTagID;
        AssetsMode->CurrentTagValue = 0;
    }
            
    char *TagValueStringsKey = JsonGetTagValueEnumKey(UIState->JsonStringsHead, AssetsMode->CurrentTagID);
    if(StringsAreEqual(TagValueStringsKey, "Number"))
    {
        UIBeginRow(Layout);
        UIScrollAdjustU32Button(Layout, " ", 500.0f, (u32 *)&AssetsMode->CurrentTagValue, BColor_Blue);
        UIIncrementButton(Layout, &AssetsMode->CurrentTagValue);
        UIDecrementButton(Layout, &AssetsMode->CurrentTagValue);
        UIEndRow(Layout);

        FormatString(ArrayCount(Buffer), Buffer, "Current Value: %d", AssetsMode->CurrentTagValue);
    }
    else
    {
#if 0
        u32 ValueCount = TagValueCounts[AssetsMode->CurrentTagID];
        string_array *ValueStringArray = GetOrCreateStringArray(UIState, TagValueStringsKey, ValueCount);
            
        UIBeginRow(Layout);
        UIDrawScrollWindow(UIState, Layout, "Tag Value Picker", V2(500.0f, 200.0f), &AssetsMode->CurrentTagValue,
                           "Choose Value", ScrollDataType_Strings, 4,
                           ValueStringArray->StringCount, ValueStringArray->Strings);
        UIIncrementButton(Layout, &AssetsMode->CurrentTagValue, ValueCount);
        UIDecrementButton(Layout, &AssetsMode->CurrentTagValue);
        UIEndRow(Layout);

        char *ValueString = ValueStringArray->Strings[AssetsMode->CurrentTagValue];
        FormatString(ArrayCount(Buffer), Buffer, "Current Value: %s", ValueString);
#endif
    }

    UILabel(Layout, Buffer, 580.0f);

    UIButton(Layout, "Add Tag",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->AddTag, true),
             580.0f, BColor_Green);

    UILabel(RightMenuLayout, "Stored Asset Attributes: ", 575.0f);

#if 0
    char *TypeIDString = AssetStringArray->Strings[CurrentAsset->TypeID];
    FormatString(ArrayCount(Buffer), Buffer, "TypeID: %s", TypeIDString);
    UILabel(RightMenuLayout, Buffer, 575.0f);

    char *StoredTypeString = JsonGetEnumString(UIState->JsonStringsHead, "StoredAssetType", CurrentAsset->Type);
    FormatString(ArrayCount(Buffer), Buffer, "%s", StoredTypeString);
    UILabel(RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "TagCount: %d", CurrentAsset->TagCount);
    UILabel(RightMenuLayout, Buffer, 575.0f);
            
    UIDrawScrollWindow(UIState, RightMenuLayout, "Stored Asset Tags", V2(575.0f, 200.0f), &AssetsMode->CurrentTag,
                       "Tags", ScrollDataType_Tags, 5, ArrayCount(CurrentAsset->AssetTags), CurrentAsset->AssetTags);
    ssa_tag *CurrentTag = CurrentAsset->AssetTags + AssetsMode->CurrentTag;
    char *CurrentTagString = TagStringArray->Strings[CurrentTag->ID];
    FormatString(ArrayCount(Buffer), Buffer, "CurrentTag: %d. %s, %d",
                 AssetsMode->CurrentTag, CurrentTagString, CurrentTag->Value);
    UILabel(RightMenuLayout, Buffer, 575.0f);
#endif

    UIButton(RightMenuLayout, "Remove Current Tag",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->RemoveTag, true),
             575.0f, BColor_Green);

    ui_layout BottomLeftLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-1275.0f, -670.0f));
    UIButton(&BottomLeftLayout, "Exit",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->EditMode, EditMode_None),
             200.0f, BColor_Red);
    UIEndLayout(&BottomLeftLayout);

    ui_layout BottomRightLayout = UIBeginLayout(UIState, Layout->MouseP, V2(1055.0f, -670.0f));
    if(!AssetsMode->EditStoredAsset)
    {
        UIButton(&BottomRightLayout, "Add Asset",
                 UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->AddAsset, true),
                 200.0f, BColor_Green);
    }
    UIEndLayout(&BottomRightLayout);
}

internal void
DrawAssetsBitmapEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, ui_layout *Layout,
                         stored_asset *CurrentAsset)
{
    char Buffer[256];
    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
    stored_asset_bitmap *StoredBitmap = &CurrentAsset->Bitmap;
    loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
    
    ui_layout RightMenuLayout = UIBeginLayout(UIState, Layout->MouseP, V2(680.0f, 690.0f));
    DrawStandardEditLayout(UIState, Layout, &RightMenuLayout, AssetsMode, CurrentAsset);
            
    FormatString(ArrayCount(Buffer), Buffer, "%s attributes : ",
                 AssetsMode->BitmapFiles[AssetsMode->FileIndex]);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIBeginRow(&RightMenuLayout);
    FormatString(ArrayCount(Buffer), Buffer, "Width: %d pixels", Bitmap->Width);
    UILabel(&RightMenuLayout, Buffer, 275.0f);

    FormatString(ArrayCount(Buffer), Buffer, "Height: %d pixels", Bitmap->Height);
    UILabel(&RightMenuLayout, Buffer, 275.0f);
    UIEndRow(&RightMenuLayout);

    FormatString(ArrayCount(Buffer), Buffer, "Align Percentage: V2(%.02f, %.02f)",
                 Bitmap->AlignPercentage.x, Bitmap->AlignPercentage.y);

    UILabel(&RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "Stored Bitmap: %s", StoredBitmap->FileName);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "AlignPercentage: V2(%.02f, %.02f)",
                 StoredBitmap->AlignPercentage.x, StoredBitmap->AlignPercentage.y);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIEndLayout(&RightMenuLayout);
}

internal void
DrawAssetsSpriteSheetEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, ui_layout *Layout,
                              stored_asset *CurrentAsset)
{
    char Buffer[256];
    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
    stored_asset_spritesheet *StoredSpriteSheet = &CurrentAsset->SpriteSheet;
    loaded_bitmap *SpriteSheetBitmap = &SpriteSheetMode->SpriteSheetBitmap;

    ui_layout RightMenuLayout = UIBeginLayout(UIState, Layout->MouseP, V2(680.0f, 690.0f));
    DrawStandardEditLayout(UIState, Layout, &RightMenuLayout, AssetsMode, CurrentAsset);

    ui_layout MiddleLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-670.0f, -675.0f));
    UIButton(&MiddleLayout, SpriteSheetMode->ShowAnimated ? "Animated" : "Bitmap",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&SpriteSheetMode->ShowAnimated,
                                    !SpriteSheetMode->ShowAnimated),
             1320.0f, BColor_Green);
    UIEndLayout(&MiddleLayout);

    FormatString(ArrayCount(Buffer), Buffer, "%s attributes : ",
                 AssetsMode->SpriteSheetFiles[AssetsMode->FileIndex]);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIBeginRow(&RightMenuLayout);
    FormatString(ArrayCount(Buffer), Buffer, "Width: %d pixels", SpriteSheetBitmap->Width);
    UILabel(&RightMenuLayout, Buffer, 275.0f);

    FormatString(ArrayCount(Buffer), Buffer, "Height: %d pixels", SpriteSheetBitmap->Height);
    UILabel(&RightMenuLayout, Buffer, 275.0f);
    UIEndRow(&RightMenuLayout);

    FormatString(ArrayCount(Buffer), Buffer, "SpriteSheet: %s", StoredSpriteSheet->SourceFileName);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "SpriteAlignPercentage: V2(%.02f, %.02f)",
                 StoredSpriteSheet->SpriteAlignPercentage.x,
                 StoredSpriteSheet->SpriteAlignPercentage.y);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "Sprite Count: %d",
                 StoredSpriteSheet->SpriteCount);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIBeginRow(&RightMenuLayout);
    FormatString(ArrayCount(Buffer), Buffer, "SpriteWidth: %d pixels",
                 StoredSpriteSheet->SpriteWidth);
    UILabel(&RightMenuLayout, Buffer, 275.0f);
    FormatString(ArrayCount(Buffer), Buffer, "SpriteHeight: %d pixels",
                 StoredSpriteSheet->SpriteHeight);
    UILabel(&RightMenuLayout, Buffer, 275.0f);
    UIEndRow(&RightMenuLayout);

    UIScrollAdjustU32Button(&RightMenuLayout, "AdjustWidth", 575.0f, (u32 *)&StoredSpriteSheet->SpriteWidth,
                         BColor_Green, 0, (SpriteSheetBitmap->Width + 1));

    UIButton(&RightMenuLayout, "Cut SpriteSheet",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&SpriteSheetMode->CutSpriteSheet, true),
             575.0f, BColor_Green);
    UIEndLayout(&RightMenuLayout);
}

internal void
DrawAssetsTilesetEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, ui_layout *Layout,
                          stored_asset *CurrentAsset)
{
    char Buffer[256];
    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
    stored_asset_tileset *StoredTileset = &CurrentAsset->Tileset;
    loaded_bitmap *TilesetBitmap = &TilesetMode->TilesetBitmap;

    ui_layout RightMenuLayout = UIBeginLayout(UIState, Layout->MouseP, V2(680.0f, 690.0f));
    DrawStandardEditLayout(UIState, Layout, &RightMenuLayout, AssetsMode, CurrentAsset);

    ui_layout MiddleLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-670.0f, -675.0f));
    UIButton(&MiddleLayout, TilesetMode->ShowTiles ? "Tiles" : "Tileset Bitmap",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&TilesetMode->ShowTiles,
                                    !TilesetMode->ShowTiles),
             1320.0f, BColor_Green);
    UIEndLayout(&MiddleLayout);

    FormatString(ArrayCount(Buffer), Buffer, "%s attributes : ",
                 AssetsMode->TilesetFiles[AssetsMode->FileIndex]);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIBeginRow(&RightMenuLayout);
    FormatString(ArrayCount(Buffer), Buffer, "Width: %d pixels", TilesetBitmap->Width);
    UILabel(&RightMenuLayout, Buffer, 275.0f);

    FormatString(ArrayCount(Buffer), Buffer, "Height: %d pixels", TilesetBitmap->Height);
    UILabel(&RightMenuLayout, Buffer, 275.0f);
    UIEndRow(&RightMenuLayout);

    FormatString(ArrayCount(Buffer), Buffer, "Tileset: %s", StoredTileset->SourceFileName);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "Tile Count: %d", StoredTileset->TileCount);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIBeginRow(&RightMenuLayout);
    FormatString(ArrayCount(Buffer), Buffer, "TileWidth: %d pixels", StoredTileset->TileWidth);
    UILabel(&RightMenuLayout, Buffer, 275.0f);
    FormatString(ArrayCount(Buffer), Buffer, "TileHeight: %d pixels", StoredTileset->TileHeight);
    UILabel(&RightMenuLayout, Buffer, 275.0f);
    UIEndRow(&RightMenuLayout);

    UIBeginRow(&RightMenuLayout);
    UIScrollAdjustU32Button(&RightMenuLayout, "AdjustWidth", 275.0f, (u32 *)&StoredTileset->TileWidth,
                         BColor_Green, 0, (TilesetBitmap->Width + 1));
    UIScrollAdjustU32Button(&RightMenuLayout, "AdjustHeight", 275.0f, (u32 *)&StoredTileset->TileHeight,
                         BColor_Green, 0, (TilesetBitmap->Height + 1));
    UIEndRow(&RightMenuLayout);

    UIBeginRow(&RightMenuLayout);
    UIButton(&RightMenuLayout, "Cut With Merge",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&TilesetMode->CutWithMergeTileset, true),
             275.0f, BColor_Green);
    UIButton(&RightMenuLayout, "Cut",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&TilesetMode->CutTileset, true),
             275.0f, BColor_Green);
    UIEndRow(&RightMenuLayout);

    b32 IsMerged = StoredTileset->MergedTile;
    interaction NullInteraction = {};
    UIButton(&RightMenuLayout, (IsMerged ? "IsMerged" : "NotMerged"), NullInteraction,
             575.0f, (IsMerged ? BColor_Green : BColor_Red));
    UIEndLayout(&RightMenuLayout);

    if(TilesetMode->ShowTiles)
    {
        ui_layout CenterLeftLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-660.0f, 660.0f));
        UIDecrementArrowButton(&CenterLeftLayout, (u32 *)&TilesetMode->CurrentTileIndex);
        UIEndLayout(&CenterLeftLayout);

        ui_layout CenterRightLayout = UIBeginLayout(UIState, Layout->MouseP, V2(612.0f, 660.0f));
        UIIncrementArrowButton(&CenterRightLayout, (u32 *)&TilesetMode->CurrentTileIndex,
                               StoredTileset->TileCount);
        UIEndLayout(&CenterRightLayout);

        ui_layout CenterTopLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-610.0f, 660.0f));
        FormatString(ArrayCount(Buffer), Buffer, "Tile Index: %d",
                     TilesetMode->CurrentTileIndex);
        UILabel(&CenterTopLayout, Buffer, 1200.0f);
        UIEndLayout(&CenterTopLayout);
    }

    ui_layout LeftBottomLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-1275.0f, 0.0f));
    FormatString(ArrayCount(Buffer), Buffer, "Merge Tile: %s",
                 AssetsMode->SolidTileFiles[AssetsMode->SubFileIndex]);
    UILabel(&LeftBottomLayout, Buffer, 580.0f);

    UIPictureElement(&LeftBottomLayout, 600.0f, &TilesetMode->MergeTileBitmap);
    UIEndLayout(&LeftBottomLayout);
}

internal void
DrawAssetsSoundEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, ui_layout *Layout,
                        stored_asset *CurrentAsset)
{
    char Buffer[256];
    sound_mode *SoundMode = &AssetsMode->SoundMode;
    stored_asset_sound *StoredSound = &CurrentAsset->Sound;
    
    ui_layout RightMenuLayout = UIBeginLayout(UIState, Layout->MouseP, V2(680.0f, 690.0f));
    DrawStandardEditLayout(UIState, Layout, &RightMenuLayout, AssetsMode, CurrentAsset);

    ui_layout MiddleLeftLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-300.0f, 680.0f));
    UIBeginRow(&MiddleLeftLayout);
    UIButton(&MiddleLeftLayout, "Play Sound",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&SoundMode->PlaySound, true),
             200.0f, BColor_Green, 40.0f);
    UIButton(&MiddleLeftLayout, "Stop Sound",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&SoundMode->StopSound, true),
             200.0f, BColor_Red, 40.0f);
    UIEndRow(&MiddleLeftLayout);
    UIEndLayout(&MiddleLeftLayout);

    FormatString(ArrayCount(Buffer), Buffer, "%s attributes : ",
                 AssetsMode->SoundFiles[AssetsMode->FileIndex]);
    UILabel(&RightMenuLayout, Buffer, 575.0f);
#if 0
    string_array *SoundChainStringArray = GetOrCreateStringArray(UIState, "SSASoundChain", SSASoundChain_Count);
    char *ChainString = SoundChainStringArray->Strings[StoredSound->Chain];
    FormatString(ArrayCount(Buffer), Buffer, "Current Chain: %s", ChainString);

    UILabel(&RightMenuLayout, Buffer, 575.0f);
    UIDrawScrollWindow(UIState, &RightMenuLayout, "SSA Sound Chain Picker", V2(575.0f, 200.0f), &StoredSound->Chain,
                       "Choose Chain", ScrollDataType_Strings, 3,
                       SoundChainStringArray->StringCount, SoundChainStringArray->Strings);
#endif

    UIEndLayout(&RightMenuLayout);
}

internal void
DrawAssetsTextEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, ui_layout *Layout,
                       stored_asset *CurrentAsset)
{
    text_mode *TextMode = &AssetsMode->TextMode;
    
    ui_layout RightMenuLayout = UIBeginLayout(UIState, Layout->MouseP, V2(680.0f, 690.0f));
    DrawStandardEditLayout(UIState, Layout, &RightMenuLayout, AssetsMode, CurrentAsset);
    
    ui_layout MiddleLeftLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-670.0f, 675.0f));
    UILabel(&MiddleLeftLayout, TextMode->Text.String, 1305.0f, 40.0f);
    UIButton(&MiddleLeftLayout, "Edit",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&TextMode->EditTextFile, true),
             1325.0f, BColor_Green);
    UIButton(&MiddleLeftLayout, "Reload",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&TextMode->Reload, true),
             1325.0f, BColor_Green);
    UIEndLayout(&MiddleLeftLayout);
}

internal void
DrawAssetsFontEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, ui_layout *Layout,
                       stored_asset *CurrentAsset)
{
    char Buffer[256];
    font_mode *FontMode = &AssetsMode->FontMode;
    stored_asset_font *StoredFont = &CurrentAsset->Font;

    ui_layout RightMenuLayout = UIBeginLayout(UIState, Layout->MouseP, V2(680.0f, 690.0f));
    DrawStandardEditLayout(UIState, Layout, &RightMenuLayout, AssetsMode, CurrentAsset);
    
    ui_layout MiddleLeftLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-670.0f, 675.0f));
//    UILabelWithInEditorFont(&MiddleLeftLayout, "abcdefghijklmnopqrstuvwxyz\nABCDEFGHIJKLMNOPQRSTUVWXYZ\n123456789.:,;'\"(!?)+-*/=",
//                            1305.0f, &FontMode->Font, 3.0f);
    UIEndLayout(&MiddleLeftLayout);

    FormatString(ArrayCount(Buffer), Buffer, "%s attributes : ",
                 AssetsMode->FontFiles[AssetsMode->FileIndex]);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "CodePointCount: %d", StoredFont->CodePointCount);
    UILabel(&RightMenuLayout, Buffer, 575.0f);
    FormatString(ArrayCount(Buffer), Buffer, "FirstCodePoint: %#x", StoredFont->FirstCodePoint);
    UILabel(&RightMenuLayout, Buffer, 575.0f);
    FormatString(ArrayCount(Buffer), Buffer, "LastCodePoint: %#x", StoredFont->LastCodePoint);
    UILabel(&RightMenuLayout, Buffer, 575.0f);
    FormatString(ArrayCount(Buffer), Buffer, "FontSize: %d pixels", StoredFont->FontSizeInPixels);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIEndLayout(&RightMenuLayout);
}

internal void
DrawAssetsFileEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, ui_layout *Layout,
                       stored_asset *CurrentAsset)
{
    char Buffer[256];
    stored_asset_binary_file *StoredFile = &CurrentAsset->File;
    
    ui_layout RightMenuLayout = UIBeginLayout(UIState, Layout->MouseP, V2(680.0f, 690.0f));
    DrawStandardEditLayout(UIState, Layout, &RightMenuLayout, AssetsMode, CurrentAsset);

    FormatString(ArrayCount(Buffer), Buffer, "%s attributes : ",
                 AssetsMode->BinaryFiles[AssetsMode->FileIndex]);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "File Size: %d", StoredFile->FileSize);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIEndLayout(&RightMenuLayout);
}

internal void
DrawAssetsSSWMEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, ui_layout *Layout,
                       stored_asset *CurrentAsset)
{
    char Buffer[256];
    stored_asset_sswm_file *StoredFile = &CurrentAsset->SSWM;
    
    ui_layout RightMenuLayout = UIBeginLayout(UIState, Layout->MouseP, V2(680.0f, 690.0f));
    DrawStandardEditLayout(UIState, Layout, &RightMenuLayout, AssetsMode, CurrentAsset);

    FormatString(ArrayCount(Buffer), Buffer, "%s attributes : ",
                 AssetsMode->SSWMFiles[AssetsMode->FileIndex]);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    FormatString(ArrayCount(Buffer), Buffer, "File Size: %d", StoredFile->FileSize);
    UILabel(&RightMenuLayout, Buffer, 575.0f);

    UIEndLayout(&RightMenuLayout);
}

internal void
DrawTitleScreenUI(ui_state *UIState, ui_layout *Layout, editor_state *EditorState, editor_mode_title_screen *TitleScreen)
{
    char Buffer[256];
    interaction NullInteraction = {};

    Clear(&UIState->RenderGroup, UI_COLOR_RGBA1_67412CFF);
    
    UIBeginRow(Layout);
    UIButton(Layout, "Assets Mode",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&TitleScreen->ModeToPlay, EditorMode_AssetsMode),
             235.0f, BColor_Green, 80.0f);

    UIButton(Layout, "Game Mode",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&TitleScreen->ModeToPlay, EditorMode_GameMode),
             235.0f, BColor_Green, 80.0f);

    UIButton(Layout, "place holder", NullInteraction, 235.0f, BColor_Green, 80.0f);

    UIButton(Layout, "place holder", NullInteraction, 235.0f, BColor_Green, 80.0f);

    UIButton(Layout, "place holder", NullInteraction, 235.0f, BColor_Green, 80.0f);

    UIButton(Layout, "place holder", NullInteraction, 235.0f, BColor_Green, 80.0f);
    UIEndRow(Layout);

    UILabel(Layout, "Stored Assets Version: ", 300.0f);
    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Major High Version: %d", EditorState->Version.MajorHigh);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU8Button(Layout, " ", 20.0f, (u8 *)&EditorState->Version.MajorHigh, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Major Low Version: %d", EditorState->Version.MajorLow);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU8Button(Layout, " ", 20.0f, (u8 *)&EditorState->Version.MajorLow, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Minor High Version: %d", EditorState->Version.MinorHigh);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU8Button(Layout, " ", 20.0f, (u8 *)&EditorState->Version.MinorHigh, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Minor Low Version: %d", EditorState->Version.MinorLow);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU8Button(Layout, " ", 20.0f, (u8 *)&EditorState->Version.MinorLow, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UISpace(Layout, V2(100.0f, 100.0f));

    UILabel(Layout, "World Map Startup: ", 300.0f);
    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Map Major High Version: %d", EditorState->MapStartup.MapVersion.MajorHigh);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU8Button(Layout, " ", 20.0f, (u8 *)&EditorState->MapStartup.MapVersion.MajorHigh, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Map Major Low Version: %d", EditorState->MapStartup.MapVersion.MajorLow);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU8Button(Layout, " ", 20.0f, (u8 *)&EditorState->MapStartup.MapVersion.MajorLow, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Map Minor High Version: %d", EditorState->MapStartup.MapVersion.MinorHigh);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU8Button(Layout, " ", 20.0f, (u8 *)&EditorState->MapStartup.MapVersion.MinorHigh, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Map Minor Low Version: %d", EditorState->MapStartup.MapVersion.MinorLow);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU8Button(Layout, " ", 20.0f, (u8 *)&EditorState->MapStartup.MapVersion.MinorLow, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Map Width: %d", EditorState->MapStartup.MapWidth);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU32Button(Layout, " ", 20.0f, (u32 *)&EditorState->MapStartup.MapWidth, BColor_Blue, 0, 512, 16.0f);
    if(EditorState->MapStartup.MapWidth < 48)
    {
        UILabel(Layout, "Minimum width is 48 tiles!", 300.0f);
    }
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Map Height: %d", EditorState->MapStartup.MapHeight);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU32Button(Layout, " ", 20.0f, (u32 *)&EditorState->MapStartup.MapHeight, BColor_Blue, 0, 512, 16.0f);
    if(EditorState->MapStartup.MapHeight < 48)
    {
        UILabel(Layout, "Minimum height is 48 tiles!", 300.0f);
    }
    UIEndRow(Layout);

    UIBeginRow(Layout);
    FormatString(ArrayCount(Buffer), Buffer, "Map ID: %d", EditorState->MapStartup.MapID);
    UILabel(Layout, Buffer, 300.0f);
    UIScrollAdjustU32Button(Layout, " ", 20.0f, (u32 *)&EditorState->MapStartup.MapID, BColor_Blue, 0, 255, 16.0f);
    UIEndRow(Layout);

    UIButton(Layout, "NewMap",
             UISetUInt32Interaction(InteractionID(UIState), (u32 *)&EditorState->MapStartup.NewMap, !EditorState->MapStartup.NewMap),
             300.0f, EditorState->MapStartup.NewMap ? BColor_Green : BColor_Red);
}

internal void
UIDrawTileToolBar(ui_layout *Layout, array_cursor *TileCursor, loaded_tileset *Tileset, u32 TileCount)
{
    ui_state *UIState = Layout->UIState;
    if(TileCursor->ElementCount != TileCount)
    {
        ResetCursorArray(TileCursor);
    }
    
    ChangeCursorPositionForToolBar(TileCursor, TileCount, UIState->MouseZ);
    UIBeginRow(Layout);
    for(u32 ElementIndex = 0;
        ElementIndex < TileCursor->ArrayCount;
        ++ElementIndex)
    {
        u32 TileIndex = TileCursor->Array[ElementIndex];
        bitmap_id ID = Tileset->Tiles[TileIndex].BitmapID;
        ID.Value += Tileset->BitmapIDOffset;
        
        UIPictureElement(Layout, 90.0f, 0, ID, (ElementIndex == TileCursor->ArrayPosition) ? V4(0.8f, 0.8f, 0.8f, 0.1f) : V4(1, 1, 1, 1));
    }
    UIEndRow(Layout);

    TileCursor->ElementCount = TileCount;
}

internal void
DrawGameModeUI(ui_state *UIState, ui_layout *Layout, editor_mode_game *GameMode)
{
    char Buffer[256];
    interaction NullInteraction = {};

    render_group *RenderGroup = &UIState->RenderGroup;
    Clear(&UIState->RenderGroup, UI_COLOR_RGBA1_67412CFF);
    
    UIBeginRow(Layout);
    char *ModeString = JsonGetEnumString(UIState->JsonStringsHead, "EditGameMode", GameMode->GameEditMode);
    FormatString(ArrayCount(Buffer), Buffer, "Current Mode: %s", ModeString);
    UILabel(Layout, Buffer, 300.0f);
    FormatString(ArrayCount(Buffer), Buffer, "Edit Enable: %s", IsSetGameModeFlag(GameMode, GMFlag_EditEnable) ? "true" : "false");
    UILabel(Layout, Buffer, 200.0f, 20.0f, IsSetGameModeFlag(GameMode, GMFlag_EditEnable) ? V4(0, 1, 0, 1) : V4(1, 0, 0, 1));
    UIEndRow(Layout);

    switch(GameMode->GameEditMode)
    {
        case EditGameMode_None:
        {
            ui_layout BottomLeftLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-1275.0f, -665.0f));
            UIBeginRow(&BottomLeftLayout);
            UIButton(&BottomLeftLayout, "Exit",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->CurrentAction, GMAction_Exit),
                     200.0f, BColor_Red);
            UIButton(&BottomLeftLayout, "Write SSWM",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->CurrentAction, GMAction_WriteSSWM),
                     200.0f, BColor_Green);
            UIEndRow(&BottomLeftLayout);
            UIEndLayout(&BottomLeftLayout);

            UIBeginRow(Layout);
            FormatString(ArrayCount(Buffer), Buffer, "Choose Ground Layer: %d", GameMode->MapGroundLayer);
            UILabel(Layout, Buffer, 300.0f);
            UIScrollAdjustU32Button(Layout, " ", 20.0f, (u32 *)&GameMode->MapGroundLayer,
                                    BColor_Blue, 0, 15, 16.0f);
            UIEndRow(Layout);

            UIBeginRow(Layout);
            FormatString(ArrayCount(Buffer), Buffer, "Choose Layer Count: %d", GameMode->LayerCount);
            UILabel(Layout, Buffer, 300.0f);
            UIScrollAdjustU32Button(Layout, " ", 20.0f, (u32 *)&GameMode->LayerCount,
                                    BColor_Blue, 0, 15, 16.0f);
            UIEndRow(Layout);
        } break;

        case EditGameMode_Terrain:
        {
            UIBeginRow(Layout);
            FormatString(ArrayCount(Buffer), Buffer, "Current Z Layer: %d", GameMode->CurrentZLayer);
            UILabel(Layout, Buffer, 200.0f, 30.0f);
            UIButton(Layout, "Show only this layer",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->CurrentAction, GMAction_ShowCurrentLayer),
                     200.0f, IsSetGameModeFlag(GameMode, GMAction_ShowCurrentLayer) ? BColor_Green : BColor_Red);
            UIEndRow(Layout);

            UIBeginRow(Layout);
            FormatString(ArrayCount(Buffer), Buffer, "Fill Active: %s", GameMode->FillActive ? "true" : "false");
            UILabel(Layout, Buffer, 200.0f, 30.0f);
            UIButton(Layout, "Activate Fill",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->FillActive, !GameMode->FillActive),
                     200.0f, GameMode->FillActive ? BColor_Green : BColor_Red);
            UIEndRow(Layout);

            asset_type TilesetAssetType = RenderGroup->Assets->AssetTypes[Asset_Tileset];
            if(TilesetAssetType.FirstAssetIndex != TilesetAssetType.OnePastLastAssetIndex)
            {
                UIScrollAdjustU32Button(Layout, "Choose Tileset", 50.0f, &GameMode->CurrentTileset.Value, BColor_Blue,
                                     TilesetAssetType.FirstAssetIndex, TilesetAssetType.OnePastLastAssetIndex - 1);
            }

            if(GameMode->Tileset)
            {
                ui_layout MiddleTopLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-300.0f, 690.0f));
                UIDrawTileToolBar(&MiddleTopLayout, &GameMode->TileCursor, GameMode->Tileset, GameMode->TilesetInfo->TileCount);
                UIEndLayout(&MiddleTopLayout);

                u32 TileIndex = GameMode->TileCursor.Array[GameMode->TileCursor.ArrayPosition];
                GameMode->Tile.BitmapID = GameMode->Tileset->Tiles[TileIndex].BitmapID;
                GameMode->Tile.BitmapID.Value += GameMode->Tileset->BitmapIDOffset;
                GameMode->Tile.CheckSum = GameMode->Tileset->Tiles[TileIndex].CheckSum;

                UIPictureElement(&UIState->MouseTextLayout, 80.0f, 0, GameMode->Tile.BitmapID);
            }
            
        } break;

        case EditGameMode_NavMeshes:
        {
            UIBeginRow(Layout);
            FormatString(ArrayCount(Buffer), Buffer, "Polygon Count: %d", GameMode->PolygonCount);
            UILabel(Layout, Buffer, 200.0f, 30.0f);
            UIButton(Layout, "Start New",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->CurrentAction, GMAction_StartNewPolygon),
                     120.0f, BColor_Green);
            UIButton(Layout, "Reset Current",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->CurrentAction, GMAction_ResetCurrentPolygon),
                     120.0f, BColor_Green);
            UIButton(Layout, "Delete Current",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->CurrentAction, GMAction_DeleteCurrentPolygon),
                     120.0f, BColor_Red);
            UIButton(Layout, "Triangulate All",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->CurrentAction, GMAction_TriangulateAll),
                     120.0f, BColor_Green);
            UIEndRow(Layout);

            UIBeginRow(Layout);
            FormatString(ArrayCount(Buffer), Buffer, "Current Polygon: %d", GameMode->CurrentPolygonIndex);
            UILabel(Layout, Buffer, 200.0f, 30.0f);
            UIScrollAdjustU32Button(Layout, " ", 20.0f, (u32 *)&GameMode->CurrentPolygonIndex,
                                    BColor_Blue, 0, GameMode->PolygonCount - 1, 16.0f);
            UIEndRow(Layout);

            UIButton(Layout, "Write Polygons",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&GameMode->CurrentAction, GMAction_WritePolygons),
                     200.0f, BColor_Green);
        } break;
    }
}

internal void
DrawUI(editor_state *EditorState, ui_state *UIState, v2 MouseP)
{
    ui_layout Layout = UIBeginLayout(UIState, MouseP, V2(UIState->LeftEdge + 4.0f, 0.5f*UIState->GlobalHeight - 28.0f));

    switch(EditorState->EditorMode)
    {
        case EditorMode_TitleScreen:
        {
            DrawTitleScreenUI(UIState, &Layout, EditorState, EditorState->TitleScreen);
        } break;

        case EditorMode_AssetsMode:
        {
//            DrawAssetsModeUI(UIState, &Layout, EditorState->AssetsMode);
        } break;

        case EditorMode_GameMode:
        {
            DrawGameModeUI(UIState, &Layout, EditorState->GameMode);
        } break;

        InvalidDefaultCase;
    }

    UIEndLayout(&Layout);
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
    UIState->MouseTextLayout = UIBeginLayout(UIState, MouseP, MouseP);

    DrawUI(EditorState, UIState, MouseP);

    UIEndLayout(&UIState->MouseTextLayout);

//    if(EditorState->UIEnable)
//    {
//        Interact(UIState, Input, MouseP);
//    }
    
    EndRenderGroup(&UIState->RenderGroup);

    // NOTE(casey): Clear the UI state for the next frame
    ZeroStruct(UIState->NextHotInteraction);
}

/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

global_variable nk_color ColorTable[] =
{
    {0x3F, 0x3F, 0x3F, 0xFF},
    {0x4D, 0x30, 0x20, 0xFF},
    {0x67, 0x41, 0x2C, 0xFF},
    {0x78, 0x4A, 0x32, 0xFF},

    {0x7F, 0x7F, 0x7F, 0xFF},
    {0x88, 0x00, 0x15, 0xFF},
    {0xED, 0x1C, 0x24, 0xFF},
    {0xFF, 0x7F, 0x27, 0xFF},

    {0xB9, 0x7A, 0x57, 0xFF},
    {0x22, 0xB1, 0x4C, 0xFF},
    {0xB5, 0xE6, 0x1D, 0xFF},
    {0x3F, 0x48, 0xCC, 0xFF},

    {0x70, 0x92, 0xBE, 0xFF},
    {0x00, 0xA2, 0xE8, 0xFF},
    {0xCB, 0x9C, 0x83, 0xFF},
    {0xF7, 0x8C, 0x92, 0xFF},

    {0xDC, 0xBC, 0xAB, 0xFF},
    {0xDD, 0xF3, 0x98, 0xFF},
    {0xEF, 0xE4, 0xB0, 0xFF},
    {0x99, 0xD9, 0xEA, 0xFF},

    {0xC3, 0xC3, 0xC3, 0xFF},
    {0xCF, 0xEE, 0xF5, 0xFF},
    {0xE1, 0xE1, 0xE1, 0xFF},
    {0xFF, 0xFF, 0xFF, 0xFF}
};

internal string_array *
GetOrCreateStringArray(ui_state *UIState, json_object *Head, char *Key, u32 StringCount)
{
    u64 LowMask = 0x00000000FFFFFFFF;
    u64 CRC = CRC64FromString(Key);

    u32 High = (u32)(CRC >> 32);
    u32 Low = (u32)(CRC & LowMask);

    u32 HashIndex = ((High >> 2) + (Low >> 2)) % ArrayCount(UIState->EnumStringArraysHash);
    string_array **HashSlot = UIState->EnumStringArraysHash + HashIndex;

    string_array *Result = 0;
    for(string_array *Search = *HashSlot;
        Search;
        Search = Search->NextInHash)
    {
        if(StringsAreEqual(Search->Key, Key))
        {
            Result = Search;
            break;
        }
    }

    if(!Result)
    {
        Result = PushStruct(&UIState->StringsArena, string_array);
        Result->Key = PushString(&UIState->StringsArena, Key);
        Result->StringCount = StringCount;
        Result->Strings = PushArray(&UIState->StringsArena, Result->StringCount, char *);

        json_value *EnumStrings = JsonLookupObjectElement(Head, Key);
        if(EnumStrings)
        {
            for(u32 ElementIndex = 0;
                ElementIndex < Result->StringCount;
                ++ElementIndex)
            {
                Result->Strings[ElementIndex] = EnumStrings->Array.Items[ElementIndex]->String;
            }
        }
        
        Result->NextInHash = *HashSlot;
        *HashSlot = Result;
    }

    return(Result);
}

inline string_array *
GetOrCreateStringArray(ui_state *UIState, char *Key, u32 StringCount)
{
    string_array *Result = GetOrCreateStringArray(UIState, UIState->JsonStringsHead, Key, StringCount);
    return(Result);
}

inline void
DrawShowStoredAssets(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk)
{
    char Text[64];
    UI->NkLayoutRowStatic(Nk, 500, 460, 1);
    if(UI->NkGroupBegin(Nk, "Stored Assets View", NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
        UI->NkLayoutRowStatic(Nk, 110, 440, 1);
        for(u32 AssetIndex = 0;
            AssetIndex < AssetsMode->StoredHeader.AssetCount;
            ++AssetIndex)
        {
            kesa_asset Asset = AssetsMode->StoredAssets[AssetIndex];

            FormatString(ArrayCount(Text), Text, "Asset%d", AssetIndex);
            struct nk_rect Bounds = UI->NkWidgetBounds(Nk);

            nk_color C = ColorTable[2];
            if(UI->NkWidgetIsHovered(Nk))
                C = ColorTable[3];                

            if(UI->NkWidgetIsMouseClicked(Nk, NK_BUTTON_LEFT))
                AssetsMode->ShowStoredAssetIndex = AssetIndex;
            Platform.UI.NkFillRect(&Nk->current->buffer, Bounds, 0.0f, C);

            if(UI->NkGroupBegin(Nk, Text, NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
            {
                UI->NkLayoutRowStatic(Nk, 20, 440, 1);
                UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "%d. AssetID: %u",
                             AssetIndex, Asset.GUID);
                UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "TagCount: %d",
                             Asset.TagCount);

                UI->NkGroupEnd(Nk);
            }
        }
        UI->NkGroupEnd(Nk);
    }
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

#if 0
internal void
UITextOpWithInEditorFont(nk_ui *UI, nk_context *Nk, char *Text)
{
    u32 L = StringLength(Text);
    UI->Nk
    rectangle2 Result = InvertedInfinityRectangle2();
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
//                PushBitmap(RenderGroup, &UIState->TextTransform, Bitmap, BitmapScale,
//                           BitmapOffset, Color, 1.0f);
//                PushBitmap(RenderGroup, &UIState->ShadowTransform, Bitmap, BitmapScale,
//                           BitmapOffset + V3(2.0f, -2.0f, 0.0f), V4(0, 0, 0, 1.0f), 1.0f);
        }

        PrevCodePoint = CodePoint;
        ++At;
    }

    return(Result);
}
#endif

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

inline void
DrawAssetAdvanceView(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk)
{
    char Text[256];
    UI->NkLayoutSpacePush(Nk, UI->NkRect(612, -790, 1290, 1068));
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[0]);
    if(UI->NkGroupBegin(Nk, "Asset Advance View", NK_WINDOW_NO_SCROLLBAR))
    {
        kesa_asset *StoredAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex; 
        UI->NkLayoutRowDynamic(Nk, 40, 1);
        UI->NkPropertyInt(Nk, "Stored Asset: ", 0,
                          (int *)&AssetsMode->ShowStoredAssetIndex,
                          AssetsMode->StoredHeader.AssetCount, 1, 0.1f);

        UI->NkLayoutRowStatic(Nk, 770, 1280, 1);
        if(UI->NkGroupBegin(Nk, "Asset Specific View", NK_WINDOW_NO_SCROLLBAR))
        {
            switch(StoredAsset->Type)
            {
                case KESA_Bitmap:
                {
                    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
                    kesa_bitmap *StoredBitmap = &StoredAsset->Bitmap;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    UI->NkLabel(Nk, StoredAsset->SourceFileName, NK_TEXT_CENTERED);

                    UI->NkLayoutRowStatic(Nk, 520, 520, 1);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[14]);
                    UI->NkStrokeRect(&Nk->current->buffer, Rect, 10.0f, 3.0f, ColorTable[2]);

                    if(BitmapMode->Bitmap.TextureHandle)
                    {
                        struct nk_image Img = UI->NkImagePtr(BitmapMode->Bitmap.TextureHandle);
                        UI->NkImage(Nk, Img);
                    }

                    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 0, INT_MAX);
                    {
                        UI->NkLayoutSpacePush(Nk, UI->NkRect(528, -528, 530, 374));
                        if(UI->NkGroupBegin(Nk, "Bitmap Stats", NK_WINDOW_NO_SCROLLBAR))
                        {
                            UI->NkLayoutRowDynamic(Nk, 30, 2);
                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Width: %d pixels", BitmapMode->Bitmap.Width);

                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Height: %d pixels", BitmapMode->Bitmap.Height);

                            UI->NkLayoutRowDynamic(Nk, 30, 1);
                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "AlignPercentage: V2(%.02f, %.02f)",
                                         StoredBitmap->AlignPercentage.x, StoredBitmap->AlignPercentage.y);
                            
                            UI->NkGroupEnd(Nk);
                        }
                    }
                    UI->NkLayoutSpaceEnd(Nk);
                } break;

                case KESA_SpriteSheet:
                {
                    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
                    kesa_spritesheet *SpriteSheet = &StoredAsset->SpriteSheet;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    UI->NkLabel(Nk, StoredAsset->SourceFileName, NK_TEXT_CENTERED);

                    u32 SpriteIndex = (FloorReal32ToInt32(AssetsMode->Time*SpriteSheet->SpriteCount) %
                                       SpriteSheet->SpriteCount);
                    loaded_bitmap *SpriteBitmap = SpriteSheetMode->Sprites + SpriteIndex;

                    UI->NkLayoutRowStatic(Nk, 530, 530, 1);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[14]);
                    UI->NkStrokeRect(&Nk->current->buffer, Rect, 5.0f, 3.0f, ColorTable[2]);
                    if(UI->NkGroupBegin(Nk, "Image Stats", NK_WINDOW_NO_SCROLLBAR))
                    {
                        UI->NkLayoutRowStatic(Nk, 520, 520, 1);
                        if(SpriteBitmap->TextureHandle)
                        {
                            struct nk_image Img = UI->NkImagePtr(SpriteBitmap->TextureHandle);
                            UI->NkImage(Nk, Img);
                        }
                        
                        UI->NkGroupEnd(Nk);
                    }

                    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 0, INT_MAX);
                    {
                        UI->NkLayoutSpacePush(Nk, UI->NkRect(528, -528, 530, 374));
                        if(UI->NkGroupBegin(Nk, "SpriteSheet Stats", NK_WINDOW_NO_SCROLLBAR))
                        {
                            UI->NkLayoutRowDynamic(Nk, 30, 2);
                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Sprite Width: %d pixels", SpriteSheet->SpriteWidth);

                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Sprite Height: %d pixels", SpriteSheet->SpriteHeight);

                            UI->NkLayoutRowDynamic(Nk, 30, 1);
                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "AlignPercentage: V2(%.02f, %.02f)",
                                 SpriteSheet->SpriteAlignPercentage.x, SpriteSheet->SpriteAlignPercentage.y);

                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Sprite Count: %d", SpriteSheet->SpriteCount);
                            
                            UI->NkGroupEnd(Nk);
                        }
                    }
                    UI->NkLayoutSpaceEnd(Nk);
                } break;

                case KESA_Tileset:
                {
                    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
                    kesa_tileset *StoredTileset = &StoredAsset->Tileset;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    u32 TileIndex = (FloorReal32ToInt32(AssetsMode->Time) %
                                     StoredTileset->TileCount);
                    loaded_bitmap *TileBitmap = TilesetMode->Tiles + TileIndex;
                    UI->NkLayoutRowStatic(Nk, 530, 530, 1);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[14]);
                    UI->NkStrokeRect(&Nk->current->buffer, Rect, 5.0f, 3.0f, ColorTable[2]);
                    if(UI->NkGroupBegin(Nk, "Image Stats", NK_WINDOW_NO_SCROLLBAR))
                    {
                        UI->NkLayoutRowStatic(Nk, 520, 520, 1);
                        if(TileBitmap->TextureHandle)
                        {
                            struct nk_image Img = UI->NkImagePtr(TileBitmap->TextureHandle);
                            UI->NkImage(Nk, Img);
                        }
                        
                        UI->NkGroupEnd(Nk);
                    }

                    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 0, INT_MAX);
                    {
                        UI->NkLayoutSpacePush(Nk, UI->NkRect(528, -528, 530, 374));
                        if(UI->NkGroupBegin(Nk, "Tileset Stats", NK_WINDOW_NO_SCROLLBAR))
                        {
                            UI->NkLayoutRowDynamic(Nk, 30, 2);
                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Tile Width: %d pixels", StoredTileset->TileWidth);

                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Tile Height: %d pixels", StoredTileset->TileHeight);

                            UI->NkLayoutRowDynamic(Nk, 30, 1);
                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Tile Count: %d", StoredTileset->TileCount);

                            Rect = UI->NkWidgetBounds(Nk);
                            UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Merge Tile Source: %s", StoredTileset->MergeTileFileName);
                            
                            UI->NkGroupEnd(Nk);
                        }
                    }
                    UI->NkLayoutSpaceEnd(Nk);
                } break;

                case KESA_Font:
                {
                    font_mode *FontMode = &AssetsMode->FontMode;
                    kesa_font *StoredFont = &StoredAsset->Font;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Code Point Count: %d", StoredFont->CodePointCount);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Font Size: %d pixels", StoredFont->FontSizeInPixels);

                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "First Code Point: %#x", StoredFont->FirstCodePoint);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Last Code Point: %#x", StoredFont->LastCodePoint);

                    // TODO(paul): Font Handling ?? may be removed
                    if(FontMode->Font.GlyphCount)
                    {
//                        UILabelWithInEditorFont(&WindowLayout, "abcdefghijklmnopqrstuvwxyz\nABCDEFGHIJKLMNOPQRSTUVWXYZ\n123456789.:,;'\"(!?)+-*/=",
//                                                1512.0f, &FontMode->Font, 3.0f);
                    }
                } break;

                case KESA_Text:
                {
                    text_mode *TextMode = &AssetsMode->TextMode;
                    kesa_text *StoredText = &StoredAsset->Text;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    // TODO(paul): Show Text
//                    UILabel(&WindowLayout, TextMode->Text.String, 1512.0f);
//                    UISpace(&WindowLayout, V2(1512.0f, 30.0f));
                } break;

                case KESA_Sound:
                {
                    sound_mode *SoundMode = &AssetsMode->SoundMode;
                    kesa_sound *StoredSound = &StoredAsset->Sound;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "First Sample Index: %d", StoredSound->FirstSampleIndex);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    char *ChainString = JsonGetEnumString(UIState->JsonStringsHead, "SSASoundChain", StoredSound->Chain);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Chain: %s", ChainString);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Sample Count: %d", SoundMode->Sound.SampleCount);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Channel Count: %d", SoundMode->Sound.ChannelCount);
                } break;

                case KESA_File:
                {
                    // TODO(paul): Add a feature to listen to the sound saved
                    binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
                    kesa_binary_file *StoredFile = &StoredAsset->File;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "File Size: %d", StoredFile->FileSize);
                } break;
            }
            UI->NkGroupEnd(Nk);
        }

        UI->NkLayoutRowStatic(Nk, 240, 1280, 1);
        struct nk_rect Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[2]);
        if(UI->NkGroupBegin(Nk, "General View", NK_WINDOW_NO_SCROLLBAR))
        {
            UI->NkLayoutRowDynamic(Nk, 190, 2);
            if(UI->NkGroupBegin(Nk, "Stored Attributes: ", NK_WINDOW_TITLE))
            {
                char *StoredType = JsonGetEnumString(UIState->JsonStringsHead, "StoredAssetType", StoredAsset->Type);
                UI->NkLayoutRowDynamic(Nk, 30, 1);
                FormatString(ArrayCount(Text), Text, "  GUID: %d",
                             StoredAsset->GUID);
                struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                UI->NkLabel(Nk, Text, NK_TEXT_LEFT);

                FormatString(ArrayCount(Text), Text, "  StoredType: %s",
                             StoredType ? StoredType : "-");
                Rect = UI->NkWidgetBounds(Nk);
                UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                UI->NkLabel(Nk, Text, NK_TEXT_LEFT);
                
                
                UI->NkGroupEnd(Nk);
            }

            if(UI->NkGroupBegin(Nk, "Stored Asset Preview Tags", NK_WINDOW_TITLE))
            {
                UI->NkLayoutRowDynamic(Nk, 30, 1);
                for(u32 TagIndex = 0;
                    TagIndex < StoredAsset->TagCount;
                    ++TagIndex)
                {
                    kea_tag Tag = StoredAsset->AssetTags[TagIndex];
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                    UI->NkLabelf(Nk, NK_TEXT_LEFT, "  %d. %s, %s", TagIndex, Tag.Key, Tag.Value);
                }
                UI->NkGroupEnd(Nk);
            }

            // NOTE(paul): Action on Current Stored Asset 
            UI->NkLayoutRowDynamic(Nk, 30, 2);
            if(UI->NkButtonLabel(Nk, "Edit Stored Asset"))
                AssetsMode->EditStoredAsset = true;

            UI->NkLayoutSpacePush(Nk, UI->NkRect(790, 12, 470, 40));
            if(UI->NkButtonLabel(Nk, "Remove Stored Asset"))
                AssetsMode->RemoveStoredAsset = true;

            UI->NkGroupEnd(Nk);
        }

        UI->NkGroupEnd(Nk);
    }
}

inline b32
IsUNUSED(char *String)
{
    b32 Result = false;

    char *At = String;
    char TestString[] = {'U', 'N', 'U', 'S', 'E', 'D'};

    u8 Index = 0;
    char TestAt = TestString[0];
    while(*At)
    {
        if(*At == TestAt)
            ++Index;
        else
            Index = 0;

        if(Index < ArrayCount(TestString))
            TestAt = TestString[Index];
        else
        {
            Result = true;
            break;
        }

        *At++;
    }

    return(Result);
}

inline char *
AssambleStrings(memory_arena *Arena, char **Strings, u32 *Count, b32 Filter = false)
{
    u32 ResultCount = 0;
    char *Result = 0;
    u32 TotalSize = 0;
    for(u32 I = 0;
        I < *Count;
        ++I)
    {
        if(Filter && IsUNUSED(Strings[I]))
            continue;

        u32 L = StringLength(Strings[I]);
        TotalSize += L + 1;
        ++ResultCount;
    }

    Result = (char *)PushSize(Arena, TotalSize);
    char *At = Result;
    for(u32 I = 0;
        I < *Count;
        ++I)
    {
        if(Filter && IsUNUSED(Strings[I]))
            continue;

        u32 L = StringLength(Strings[I]);
        Copy(L, Strings[I], At);
        At += L;
        *At = 0;
        *At++;
    }

    *Count = ResultCount;
    
    return(Result);
}

inline char **
ExtractTagKeys(editor_mode_assets *AssetsMode, memory_arena *TempArena)
{
    char **Result = PushArray(TempArena, AssetsMode->TagMapListCount, char *);
    u32 I = 0;
    for(tag_map_list *Iter = AssetsMode->TagMapListHead;
        Iter;
        Iter = Iter->Next)
    {
        Result[I++] = Iter->Tag.Key;
    }

    return(Result);
}

inline char **
ExtractTagValues(kea_tag_map *Tag, memory_arena *TempArena)
{
    char **Result = PushArray(TempArena, Tag->ValueCount, char *);
    for(u32 I = 0; I < Tag->ValueCount; ++I)
        Result[I] = Tag->Values[I];

    return(Result);
}
    
inline void
DrawStandardEditLayout(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                       kesa_asset *CurrentAsset)
{
    temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);

    kesa_type StoredType = KESAFromEditMode(AssetsMode->EditMode);
    u32 FileCount = AssetsMode->SourceFileCounts[StoredType];
    char **FileStrings = AssetsMode->SourceFiles[StoredType];

    char *Text = 0;
    switch(AssetsMode->EditMode)
    {
        case EditMode_Bitmap:
            Text = "Pick Bitmap Source \\|/";
            break;

        case EditMode_SpriteSheet:
            Text = "Pick SpriteSheet Source \\|/";
            break;

        case EditMode_Tileset:
            Text = "Pick Tileset Source \\|/";
            break;

        case EditMode_Sound:
            Text = "Pick Sound Source \\|/";
            break;

        case EditMode_Text:
            Text = "Pick Text Source \\|/";
            break;

        case EditMode_Font:
            Text = "Pick Font Source \\|/";
            break;

        case EditMode_File:
            Text = "Pick File Source \\|/";
            break;

        case EditMode_SSWM:
            Text = "Pick SSWM Source \\|/";
            break;

        InvalidDefaultCase;
    }

    UI->NkLayoutRowStatic(Nk, 280, 450, 1);
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "File Picker", NK_WINDOW_NO_SCROLLBAR))
    {        
        char *Strings = AssambleStrings(TempMem.Arena, FileStrings, &FileCount);

        UI->NkLayoutRowStatic(Nk, 30, 440, 1);
        struct nk_rect Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, Text, NK_TEXT_CENTERED);
        UI->NkComboboxString(Nk, Strings, (int *)&AssetsMode->FileIndex,
                             FileCount, 30, {460, 460});

        if(AssetsMode->EditMode == EditMode_Tileset)
        {
            char *Strings = AssambleStrings(TempMem.Arena,
                                            AssetsMode->SolidTileFiles,
                                            &AssetsMode->SolidTileFileCount);

            UI->NkLayoutRowStatic(Nk, 30, 440, 1);
            struct nk_rect Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Choose Merge Tile", NK_TEXT_CENTERED);
            UI->NkComboboxString(Nk, Strings, (int *)&AssetsMode->SubFileIndex,
                                 AssetsMode->SolidTileFileCount, 30, {460, 460});
        }

        UI->NkLayoutRowStatic(Nk, 30, 440, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Choose Tag To Add \\|/", NK_TEXT_CENTERED);

        u32 Count = AssetsMode->TagMapListCount;
        char **TagRawStrings = ExtractTagKeys(AssetsMode, TempMem.Arena);
        char *TagStrings = AssambleStrings(TempMem.Arena, TagRawStrings, &Count, false);
        UI->NkComboboxString(Nk, TagStrings, (int *)&AssetsMode->CurrentTagID, Count, 30, {460, 460});

        tag_map_list *CurrentTag = GetTagMapByIndex(AssetsMode, AssetsMode->CurrentTagID);
        
        if(AssetsMode->CurrentTagID != AssetsMode->LastTagID)
        {
            AssetsMode->LastTagID = AssetsMode->CurrentTagID;
            AssetsMode->CurrentTagValue = 0;
        }

        UI->NkLayoutRowStatic(Nk, 30, 440, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Choose Tag Value", NK_TEXT_CENTERED);
        char *TagValues = AssambleStrings(TempMem.Arena,
                                          ExtractTagValues(&CurrentTag->Tag, TempMem.Arena),
                                          &CurrentTag->Tag.ValueCount);

        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_LEFT, "  Current Value: %s|%d",
                     CurrentTag->Tag.Values[AssetsMode->CurrentTagValue], AssetsMode->CurrentTagValue);

        UI->NkComboboxString(Nk, TagValues,
                             (int *)&AssetsMode->CurrentTagValue,
                             CurrentTag->Tag.ValueCount, 30, {460, 460});

        if(UI->NkButtonLabel(Nk, "Add Tag"))
            AssetsMode->AddTag = true;

        UI->NkGroupEnd(Nk);
    }

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -284, 450, 320});
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Stored Asset Attributes", NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
    {        
        UI->NkLayoutRowDynamic(Nk, 30, 1);
        char *StoredTypeString = JsonGetEnumString(UIState->JsonStringsHead,
                                                   "StoredAssetType", CurrentAsset->Type);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s", StoredTypeString);

        char *TagsString = 0;
        u32 TotalSize = 0;
        for(u32 I = 0;
            I < CurrentAsset->TagCount;
            ++I)
        {
            kea_tag *CurrentTag = CurrentAsset->AssetTags + I;
            u32 L = StringLength(CurrentTag->Key);
            TotalSize += L + 1;
        }
        TagsString = (char *)PushSize(TempMem.Arena, TotalSize);
        char *At = TagsString;
        for(u32 I = 0;
            I < CurrentAsset->TagCount;
            ++I)
        {
            kea_tag *CurrentTag = CurrentAsset->AssetTags + I;
            u32 L = StringLength(CurrentTag->Key);
            Copy(L, CurrentTag->Key, At);
            At += L;
            *At = 0;
            *At++;
        }
        
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[0]);
        UI->NkLabel(Nk, "Stored Asset Tags", NK_TEXT_CENTERED);

        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "TagCount: %d", CurrentAsset->TagCount);
        UI->NkComboboxString(Nk, TagsString, (int *)&AssetsMode->CurrentTag,
                             CurrentAsset->TagCount, 30, {440, 380});

        kea_tag *CurrentTag = CurrentAsset->AssetTags + AssetsMode->CurrentTag;
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Current Tag: %d. %s",
                     AssetsMode->CurrentTag, CurrentTag->Key);

        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Value: %s", CurrentTag->Value);

        if(UI->NkButtonLabel(Nk, "Remove Current Tag"))
            AssetsMode->RemoveTag = true;
        
        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {-5, 720, 450, 50});
    if(UI->NkGroupBegin(Nk, "Actions", NK_WINDOW_NO_SCROLLBAR))
    {        
        UI->NkLayoutRowDynamic(Nk, 40, 2);
        if(UI->NkButtonLabel(Nk, "Exit"))
            AssetsMode->EditMode = EditMode_None;

        if(UI->NkButtonLabel(Nk, "Add Asset"))
            AssetsMode->AddAsset = true;
        
        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);

    EndTemporaryMemory(TempMem);
}

internal void
DrawAssetsBitmapEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                         kesa_asset *CurrentAsset)
{
    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
    kesa_bitmap *StoredBitmap = &CurrentAsset->Bitmap;
    loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
    
    DrawStandardEditLayout(AssetsMode, UIState, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -60, 450, 140});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Bitmap Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(Bitmap->Memory)
        {
            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Stored Bitmap Attribs: ", NK_TEXT_CENTERED);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s",
                         AssetsMode->SourceFiles[KESA_Bitmap][AssetsMode->FileIndex]);

            UI->NkLayoutRowDynamic(Nk, 30, 2);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Width: %d pixels", Bitmap->Width);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Height: %d pixels", Bitmap->Height);

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "AlignPercentage: V2(%.02f, %.02f)",
                         StoredBitmap->AlignPercentage.x, StoredBitmap->AlignPercentage.y);

        }

        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawAssetsSpriteSheetEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                              kesa_asset *CurrentAsset)
{
    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
    kesa_spritesheet *StoredSpriteSheet = &CurrentAsset->SpriteSheet;
    loaded_bitmap *SpriteSheetBitmap = &SpriteSheetMode->SpriteSheetBitmap;

    DrawStandardEditLayout(AssetsMode, UIState, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 420});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "SpriteSheet Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(SpriteSheetBitmap->Memory)
        {
            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                         AssetsMode->SourceFiles[KESA_SpriteSheet][AssetsMode->FileIndex]);

            UI->NkLayoutRowDynamic(Nk, 30, 2);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Width: %d pixels", SpriteSheetBitmap->Width);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Height: %d pixels", SpriteSheetBitmap->Height);

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "SpriteSheet: ", NK_TEXT_CENTERED);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s", CurrentAsset->SourceFileName);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Sprite Align Percentage:", NK_TEXT_CENTERED);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "V2(%.02f, %.02f)",
                         StoredSpriteSheet->SpriteAlignPercentage.x,
                         StoredSpriteSheet->SpriteAlignPercentage.y);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Sprite Count: %d",
                         StoredSpriteSheet->SpriteCount);

            UI->NkLayoutRowDynamic(Nk, 30, 2);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Sprite Width: ", NK_TEXT_CENTERED);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Sprite Height: ", NK_TEXT_CENTERED);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%d pixels",
                         StoredSpriteSheet->SpriteWidth);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%d pixels",
                         StoredSpriteSheet->SpriteHeight);

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            UI->NkPropertyInt(Nk, "Adjust Width: ", 0,
                              (int *)&StoredSpriteSheet->SpriteWidth,
                              SpriteSheetBitmap->Width, 1, 0.1f);

            if(UI->NkButtonLabel(Nk, "Cut SpriteSheet"))
                SpriteSheetMode->CutSpriteSheet = true;

            if(UI->NkButtonLabel(Nk, SpriteSheetMode->ShowAnimated ? "Show Bitmap" : "Show Animated"))
                SpriteSheetMode->ShowAnimated = !SpriteSheetMode->ShowAnimated;
        }
        
        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawAssetsTilesetEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                          kesa_asset *CurrentAsset)
{
    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
    kesa_tileset *StoredTileset = &CurrentAsset->Tileset;
    loaded_bitmap *TilesetBitmap = &TilesetMode->TilesetBitmap;

    DrawStandardEditLayout(AssetsMode, UIState, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 420});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Tileset Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(TilesetBitmap->Memory)
        {
            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                         AssetsMode->SourceFiles[KESA_Tileset][AssetsMode->FileIndex]);
            UI->NkLayoutRowDynamic(Nk, 30, 2);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Width: %d pixels", TilesetBitmap->Width);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Height: %d pixels", TilesetBitmap->Height);

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Tileset: ", NK_TEXT_CENTERED);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s", CurrentAsset->SourceFileName);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Tile Count: %d", StoredTileset->TileCount);

            UI->NkLayoutRowDynamic(Nk, 30, 2);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Tile Width: ", NK_TEXT_CENTERED);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Tile Height: ", NK_TEXT_CENTERED);

            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%d pixels", StoredTileset->TileWidth);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%d pixels", StoredTileset->TileHeight);

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            UI->NkPropertyInt(Nk, "Adjust Width: ", 0, (int *)&StoredTileset->TileWidth,
                              TilesetBitmap->Width, 1, 0.1f);
            UI->NkPropertyInt(Nk, "Adjust Height: ", 0, (int *)&StoredTileset->TileHeight,
                              TilesetBitmap->Height, 1, 0.1f);


            UI->NkLayoutRowDynamic(Nk, 30, 2);
            if(UI->NkButtonLabel(Nk, "Cut With Merge"))
                TilesetMode->CutWithMergeTileset = true;

            if(UI->NkButtonLabel(Nk, "Cut"))
                TilesetMode->CutTileset = true;

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            b32 IsMerged = StoredTileset->MergedTile;
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, IsMerged ? "Tileset is Merged" : "Tileset is not Merged",
                        NK_TEXT_CENTERED);

            if(UI->NkButtonLabel(Nk, TilesetMode->ShowTiles ? "Show Bitmap" : "Show Tiles"))
                TilesetMode->ShowTiles = !TilesetMode->ShowTiles;
        }

        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 40, 3);
    if(TilesetMode->ShowTiles)
    {
        UI->NkLayoutSpacePush(Nk, {480, -466, 40, 40});
        if(UI->NkButtonSymbol(Nk, NK_SYMBOL_TRIANGLE_LEFT) &&
           (TilesetMode->CurrentTileIndex != 0))
        {
            TilesetMode->CurrentTileIndex -= 1;
        }

        UI->NkLayoutSpacePush(Nk, {525, -466, 860, 40});
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Tile Index: %d",
                     TilesetMode->CurrentTileIndex);

        UI->NkLayoutSpacePush(Nk, {1390, -466, 40, 40});
        if(UI->NkButtonSymbol(Nk, NK_SYMBOL_TRIANGLE_RIGHT) &&
           (TilesetMode->CurrentTileIndex < (StoredTileset->TileCount - 1)))
        {
            TilesetMode->CurrentTileIndex += 1;
        }
    }
    UI->NkLayoutSpaceEnd(Nk);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 40, 3);
    UI->NkLayoutSpacePush(Nk, {0, -90, 460, 30});
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(TilesetMode->MergeTileBitmap.Memory)
    {
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Merge Tile: %s",
                     AssetsMode->SolidTileFiles[AssetsMode->SubFileIndex]);

        UI->NkLayoutSpacePush(Nk, {0, -55, 460, 460});
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[14]);
        UI->NkStrokeRect(&Nk->current->buffer, Rect, 5.0f, 3.0f, ColorTable[2]);

        UI->NkLayoutSpacePush(Nk, {5, -50, 450, 450});
        if(TilesetMode->MergeTileBitmap.TextureHandle)
        {
            struct nk_image Img =
                UI->NkImagePtr(TilesetMode->MergeTileBitmap.TextureHandle);
            UI->NkImage(Nk, Img);
        }
    }
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawAssetsSoundEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                        kesa_asset *CurrentAsset)
{
    // TODO(paul): Make it more comfortable to use, like in a music player,
    // a button to stop and resume, a progress bar, and a bit more information
    // about sound itself: length, frequency, volume, etc.
    sound_mode *SoundMode = &AssetsMode->SoundMode;
    kesa_sound *StoredSound = &CurrentAsset->Sound;
    
    DrawStandardEditLayout(AssetsMode, UIState, UI, Nk, CurrentAsset);

    temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);
    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 140});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Bitmap Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(SoundMode->Sound.Samples[0])
        {
            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                         AssetsMode->SourceFiles[KESA_Sound][AssetsMode->FileIndex]);

            string_array *SoundChainStringArray =
                GetOrCreateStringArray(UIState, "SSASoundChain", SSASoundChain_Count);
            char *ChainString = SoundChainStringArray->Strings[StoredSound->Chain];
            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Current Chain: %s", ChainString);

            char *TagValues = AssambleStrings(TempMem.Arena,
                                              SoundChainStringArray->Strings,
                                              &SoundChainStringArray->StringCount);
            UI->NkComboboxString(Nk, TagValues, (int *)&StoredSound->Chain,
                                 SoundChainStringArray->StringCount, 30, {460, 460});
        }

        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);
    EndTemporaryMemory(TempMem);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {460, -460, 990, 140});
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Actions", NK_WINDOW_NO_SCROLLBAR))
    {
        UI->NkLayoutRowDynamic(Nk, 30, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Actions", NK_TEXT_CENTERED);

        UI->NkLayoutRowDynamic(Nk, 40, 2);
        if(UI->NkButtonLabel(Nk, "Play Sound"))
            SoundMode->PlaySound = true;
        if(UI->NkButtonLabel(Nk, "Stop Sound"))
            SoundMode->StopSound = true;

        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawAssetsTextEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                       kesa_asset *CurrentAsset)
{
    text_mode *TextMode = &AssetsMode->TextMode;

    // TODO(paul): Implement Nuklear text edditing here
    DrawStandardEditLayout(AssetsMode, UIState, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 140});

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {460, -460, 990, 140});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Actions", NK_WINDOW_NO_SCROLLBAR))
    {
        UI->NkLayoutRowDynamic(Nk, 30, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Actions", NK_TEXT_CENTERED);

        UI->NkLayoutRowDynamic(Nk, 40, 2);
        if(UI->NkButtonLabel(Nk, "Edit"))
            TextMode->EditTextFile = true;
        if(UI->NkButtonLabel(Nk, "Reload"))
            TextMode->Reload = true;

        UI->NkGroupEnd(Nk);
    }
}

internal void
DrawAssetsFontEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                       kesa_asset *CurrentAsset)
{
    // TODO(paul): Decide what to do with font assets, should I remove them completely,
    // or leave functionality, I just don't now yet if I am going to use it because,
    // nuklear provides fonts to the engine.
    font_mode *FontMode = &AssetsMode->FontMode;
    kesa_font *StoredFont = &CurrentAsset->Font;

    DrawStandardEditLayout(AssetsMode, UIState, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 140});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Font Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(FontMode->Font.Glyphs)
        {
            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ", AssetsMode->SourceFiles[KESA_Font][AssetsMode->FileIndex]);

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "CodePointCount: %d", StoredFont->CodePointCount);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "FirstCodePoint: %#x", StoredFont->FirstCodePoint);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "LastCodePoint: %#x", StoredFont->LastCodePoint);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "FontSize: %d pixels", StoredFont->FontSizeInPixels);
        }

        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawAssetsFileEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                       kesa_asset *CurrentAsset)
{
    // TODO(paul): Advance on this one, what file is loaded what data it containce,
    // visualize all the data posiable?
    kesa_binary_file *StoredFile = &CurrentAsset->File;
    
    DrawStandardEditLayout(AssetsMode, UIState, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 140});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "File Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(StoredFile->FileSize)
        {
            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                         AssetsMode->SourceFiles[KESA_File][AssetsMode->FileIndex]);

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "File Size: %d", StoredFile->FileSize);
        }

        UI->NkGroupEnd(Nk);
    }
}

internal void
DrawAssetsSSWMEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_ui *UI, nk_context *Nk,
                       kesa_asset *CurrentAsset)
{
    // TODO(paul): Advance on this one, visualize all the data posiable
    kesa_sswm_file *StoredFile = &CurrentAsset->SSWM;
    
    DrawStandardEditLayout(AssetsMode, UIState, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 140});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "SSWM Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(StoredFile->FileSize)
        {
            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                         AssetsMode->SourceFiles[KESA_SSWM][AssetsMode->FileIndex]);

            UI->NkLayoutRowDynamic(Nk, 30, 1);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabelf(Nk, NK_TEXT_CENTERED, "File Size: %d", StoredFile->FileSize);
        }
        
        UI->NkGroupEnd(Nk);
    }
}

internal void
DrawAssetsModeUI(editor_mode_assets *AssetsMode, ui_state *UIState)
{
    TIMED_FUNCTION();

    nk_ui *UI = UIState->UI;
    nk_context *Nk = UIState->Nk;
    
    kesa_asset *CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
    if(AssetsMode->EditStoredAsset)
    {
        CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
    }
    
    switch(AssetsMode->EditMode)
    {
        case EditMode_None:
        {
            UI->NkLayoutRowBegin(Nk, NK_STATIC, 40, 3);
            {
                UI->NkLayoutRowPush(Nk, 200);
                if(UI->NkButtonLabel(Nk, "Edit Bitmaps"))
                    AssetsMode->EditMode = EditMode_Bitmap;

                if(UI->NkButtonLabel(Nk, "Edit Sounds"))
                    AssetsMode->EditMode = EditMode_Sound;

                if(UI->NkButtonLabel(Nk, "Edit SpriteSheets"))
                    AssetsMode->EditMode = EditMode_SpriteSheet;

                if(UI->NkButtonLabel(Nk, "Edit Tilesets"))
                    AssetsMode->EditMode = EditMode_Tileset;

                if(UI->NkButtonLabel(Nk, "Edit Fonts"))
                    AssetsMode->EditMode = EditMode_Font;

                if(UI->NkButtonLabel(Nk, "Edit Texts"))
                    AssetsMode->EditMode = EditMode_Text;

                if(UI->NkButtonLabel(Nk, "Edit Files"))
                    AssetsMode->EditMode = EditMode_File;

                if(UI->NkButtonLabel(Nk, "Edit SSWM"))
                    AssetsMode->EditMode = EditMode_SSWM;

                if(UI->NkButtonLabel(Nk, "placeholder")) {}
                if(UI->NkButtonLabel(Nk, "placeholder")) {}
                if(UI->NkButtonLabel(Nk, "placeholder")) {}
                if(UI->NkButtonLabel(Nk, "placeholder")) {}
            }
            UI->NkLayoutRowEnd(Nk);

            UI->NkLayoutRowStatic(Nk, 8, 300, 1);
            UI->NkSpacer(Nk);
            UI->NkLayoutRowStatic(Nk, 30, 300, 1);

            struct nk_rect Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            UI->NkLabelf(Nk, NK_TEXT_LEFT, "  Asset Count: %d",
                         AssetsMode->StoredHeader.AssetCount);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            UI->NkLabelf(Nk, NK_TEXT_LEFT, "  SizeOfStoredAsset: %d",
                         AssetsMode->StoredHeader.SizeOfStoredAsset);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            UI->NkLabelf(Nk, NK_TEXT_LEFT, "  Version: %d.%d.%d.%d",
                         (AssetsMode->StoredHeader.Version >> 24) & 0xFF,
                         (AssetsMode->StoredHeader.Version >> 16) & 0xFF,
                         (AssetsMode->StoredHeader.Version >> 8) & 0xFF,
                         AssetsMode->StoredHeader.Version & 0xFF);

            DrawShowStoredAssets(AssetsMode, UIState, UI, Nk);
            
            UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 40, INT_MAX);
            UI->NkLayoutSpacePush(Nk, UI->NkRect(0, 238, 130, 40));
            if(UI->NkButtonLabel(Nk, "Exit"))
            {
                AssetsMode->Exit = true;
            }

            UI->NkLayoutSpacePush(Nk, UI->NkRect(134, 238, 130, 40));
            if(UI->NkButtonLabel(Nk, "Write Assets"))
            {
                AssetsMode->WriteAssets = true;
            }

            UI->NkLayoutSpacePush(Nk, UI->NkRect(268, 238, 130, 40));
            if(UI->NkButtonLabel(Nk, "Write SSA"))
            {
                AssetsMode->WriteSSA = true;
            }
            UI->NkLayoutSpaceEnd(Nk);

            DrawAssetAdvanceView(AssetsMode, UIState, UI, Nk);
        } break;

        case EditMode_Bitmap:
            DrawAssetsBitmapEditMode(AssetsMode, UIState, UI, Nk, CurrentAsset);
            break;

        case EditMode_SpriteSheet:
            DrawAssetsSpriteSheetEditMode(AssetsMode, UIState, UI, Nk, CurrentAsset);
            break;

        case EditMode_Tileset:
            DrawAssetsTilesetEditMode(AssetsMode, UIState, UI, Nk, CurrentAsset);
            break;

        case EditMode_Sound:
            DrawAssetsSoundEditMode(AssetsMode, UIState, UI, Nk, CurrentAsset);
            break;

        case EditMode_Text:
            DrawAssetsTextEditMode(AssetsMode, UIState, UI, Nk, CurrentAsset);
            break;

        case EditMode_Font:
            DrawAssetsFontEditMode(AssetsMode, UIState, UI, Nk, CurrentAsset);
            break;

        case EditMode_File:
            DrawAssetsFileEditMode(AssetsMode, UIState, UI, Nk, CurrentAsset);
            break;

        case EditMode_SSWM:
            DrawAssetsSSWMEditMode(AssetsMode, UIState, UI, Nk, CurrentAsset);
            break;
    }
}

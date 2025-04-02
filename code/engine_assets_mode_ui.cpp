/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline void
DrawShowStoredAssets(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk)
{
    char Text[64];
    UI->NkLayoutRowStatic(Nk, 360, 300, 1);
    if(UI->NkGroupBegin(Nk, "Stored Assets View", NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
        UI->NkLayoutRowStatic(Nk, 100, 280, 1);
        for(u32 AssetIndex = 0;
            AssetIndex < AssetsMode->StoredHeader.AssetCount;
            ++AssetIndex)
        {
            stored_asset Asset = AssetsMode->StoredAssets[AssetIndex];
            char *TypeIDString = JsonGetEnumString(AssetsMode->JsonStringsHead, "AssetType", Asset.TypeID);
            char *TypeString = JsonGetEnumString(AssetsMode->JsonStringsHead, "StoredAssetType", Asset.Type);

            FormatString(ArrayCount(Text), Text, "Asset%d", AssetIndex);
            struct nk_rect Bounds = UI->NkWidgetBounds(Nk);

            nk_color C = {255, 255, 255, 255};
            if(UI->NkWidgetIsHovered(Nk))
                C = {255, 0, 0, 255};                

            if(UI->NkWidgetIsMouseClicked(Nk, NK_BUTTON_LEFT))
                AssetsMode->ShowStoredAssetIndex = AssetIndex;
            Platform.UI.NkFillRect(&Nk->current->buffer, Bounds, 0.0f, C);

            if(UI->NkGroupBegin(Nk, Text, NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
            {
                UI->NkLayoutRowStatic(Nk, 20, 280, 1);
                UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "%d. AssetID: %d",
                             AssetIndex, Asset.ID);
                UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT,"AssetTypeID: %s",
                             TypeIDString);
                UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "AssetType: %s",
                             TypeString);
                UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "TagCount: %d",
                             Asset.TagCount);

                UI->NkGroupEnd(Nk);
            }
        }
        UI->NkGroupEnd(Nk);
    }
}

inline void
UIDrawAssetAdvanceView(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk)
{
    char Text[256];
    UI->NkLayoutSpacePush(Nk, UI->NkRect(320, -650, 940, 700));
    if(UI->NkGroupBegin(Nk, "Asset Advance View", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
    {
        stored_asset *StoredAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex; 
        UI->NkLayoutRowDynamic(Nk, 30, 1);
        UI->NkPropertyInt(Nk, "Stored Asset: ", 0,
                          (int *)&AssetsMode->ShowStoredAssetIndex,
                          AssetsMode->StoredHeader.AssetCount, 1, 0.1f);

        UI->NkLayoutRowStatic(Nk, 420, 920, 1);
        if(UI->NkGroupBegin(Nk, "Asset Specific View", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
        {
#if 1
            switch(StoredAsset->Type)
            {
                case StoredAssetType_Bitmap:
                {
                    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
                    stored_asset_bitmap *StoredBitmap = &StoredAsset->Bitmap;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, {0x4d, 0x30, 0x20, 255});
                    UI->NkLabel(Nk, StoredBitmap->FileName, NK_TEXT_CENTERED);

                    struct nk_image Img = UI->NkImagePtr(BitmapMode->Bitmap.TextureHandle);
                    UI->NkLayoutRowStatic(Nk, 374, 374, 1);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, {0x4d, 0x30, 0x20, 255});
                    UI->NkImage(Nk, Img);

                    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 0, INT_MAX);
                    {
                        UI->NkLayoutSpacePush(Nk, UI->NkRect(378, -378, 530, 374));
                        if(UI->NkGroupBegin(Nk, "Bitmap Stats", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
                        {
                            UI->NkGroupEnd(Nk);
                        }
#if 0
                        struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                        UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, {0x4d, 0x30, 0x20, 255});
                        FormatString(ArrayCount(Text), Text, "Width: %d pixels", BitmapMode->Bitmap.Width);
                        UI->NkLabel(Nk, Text, NK_TEXT_CENTERED);

                        Rect = UI->NkWidgetBounds(Nk);
                        UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, {0x4d, 0x30, 0x20, 255});
                        FormatString(ArrayCount(Text), Text, "Height: %d pixels", BitmapMode->Bitmap.Height);
                        UI->NkLabel(Nk, Text, NK_TEXT_CENTERED);

                        UI->NkLayoutSpacePush(Nk, UI->NkRect(0, 32, 364, 30));
                        Rect = UI->NkWidgetBounds(Nk);
                        UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, {0x4d, 0x30, 0x20, 255});
                        FormatString(ArrayCount(Text), Text, "AlignPercentage: V2(%.02f, %.02f)",
                                     StoredBitmap->AlignPercentage.x, StoredBitmap->AlignPercentage.y);
                        UI->NkLabel(Nk, Text, NK_TEXT_CENTERED);
#endif
                    }
//                    UI->NkLayoutSpaceEnd(Nk);
                } break;
#if 0
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
#endif
            }
#endif
            UI->NkGroupEnd(Nk);
        }

        UI->NkLayoutRowStatic(Nk, 240, 920, 1);
        if(UI->NkGroupBegin(Nk, "General View", NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
        {
            UI->NkLayoutRowDynamic(Nk, 190, 2);
            if(UI->NkGroupBegin(Nk, "Stored Attributes: ", NK_WINDOW_BORDER|NK_WINDOW_TITLE))
            {
                char *TypeID = JsonGetEnumString(AssetsMode->JsonStringsHead, "AssetType", StoredAsset->TypeID);
                char *StoredType = JsonGetEnumString(AssetsMode->JsonStringsHead, "StoredAssetType", StoredAsset->Type);
                UI->NkLayoutRowDynamic(Nk, 30, 1);
                FormatString(ArrayCount(Text), Text, "  InEditorID: %d",
                             StoredAsset->ID);
                struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, {0x4d, 0x30, 0x20, 255});
                UI->NkLabel(Nk, Text, NK_TEXT_LEFT);

                FormatString(ArrayCount(Text), Text, "  TypeID: %s",
                             TypeID);
                Rect = UI->NkWidgetBounds(Nk);
                UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, {0x4d, 0x30, 0x20, 255});
                UI->NkLabel(Nk, Text, NK_TEXT_LEFT);

                FormatString(ArrayCount(Text), Text, "  StoredType: %s",
                             StoredType);
                Rect = UI->NkWidgetBounds(Nk);
                UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, {0x4d, 0x30, 0x20, 255});
                UI->NkLabel(Nk, Text, NK_TEXT_LEFT);
                
                
                UI->NkGroupEnd(Nk);
            }

            if(UI->NkGroupBegin(Nk, "Stored Asset Preview Tags", NK_WINDOW_BORDER|NK_WINDOW_TITLE))
            {
                UI->NkLayoutRowDynamic(Nk, 30, 1);
                for(u32 TagIndex = 0;
                    TagIndex < StoredAsset->TagCount;
                    ++TagIndex)
                {
                    ssa_tag Tag = StoredAsset->AssetTags[TagIndex];
                    char *TagString = JsonGetEnumString(AssetsMode->JsonStringsHead, "AssetTag", Tag.ID);
                    u32 TagValue = Tag.Value;
                    char *ValueKey = JsonGetTagValueEnumKey(AssetsMode->JsonStringsHead, Tag.ID);
                    if(StringsAreEqual(ValueKey, "Number"))
                    {
                        FormatString(ArrayCount(Text), Text, "  %d. %s, %#x", TagIndex, TagString, TagValue);
                    }
                    else
                    {
                        char *ValueString = JsonGetEnumString(AssetsMode->JsonStringsHead, ValueKey, TagValue);
                        FormatString(ArrayCount(Text), Text, "  %d. %s, %s", TagIndex, TagString, ValueString);
                    }

                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, {0x4d, 0x30, 0x20, 255});
                    UI->NkLabel(Nk, Text, NK_TEXT_LEFT);
                }
                UI->NkGroupEnd(Nk);
            }

            // NOTE(paul): Action on Current Stored Asset 
            UI->NkLayoutRowDynamic(Nk, 30, 2);
            if(UI->NkButtonLabel(Nk, "Edit Stored Asset"))
            {
            }

            UI->NkLayoutSpacePush(Nk, UI->NkRect(790, 12, 470, 40));
            if(UI->NkButtonLabel(Nk, "Remove Stored Asset"))
            {
            }

            UI->NkGroupEnd(Nk);
        }

        UI->NkGroupEnd(Nk);
    }
}

internal void
DrawAssetsModeUI(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk)
{
    stored_asset *CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
    if(AssetsMode->EditStoredAsset)
    {
        CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
    }
    
    switch(AssetsMode->EditMode)
    {
        case EditMode_None:
        {
            UI->NkLayoutRowBegin(Nk, NK_STATIC, 32, 3);
            {
                UI->NkLayoutRowPush(Nk, 100);
                if(UI->NkButtonLabel(Nk, "Edit Bitmaps"))
                {
                }

                if(UI->NkButtonLabel(Nk, "Edit Sounds"))
                {
                }

                if(UI->NkButtonLabel(Nk, "Edit SpriteSheets"))
                {
                }

                if(UI->NkButtonLabel(Nk, "Edit Tilesets"))
                {
                }

                if(UI->NkButtonLabel(Nk, "Edit Fonts"))
                {
                }

                if(UI->NkButtonLabel(Nk, "Edit Texts"))
                {
                }

                if(UI->NkButtonLabel(Nk, "Edit Files"))
                {
                }

                if(UI->NkButtonLabel(Nk, "Edit SSWM"))
                {
                }

                if(UI->NkButtonLabel(Nk, "placeholder")) {}
                if(UI->NkButtonLabel(Nk, "placeholder")) {}
                if(UI->NkButtonLabel(Nk, "placeholder")) {}
                if(UI->NkButtonLabel(Nk, "placeholder")) {}
            }
            UI->NkLayoutRowEnd(Nk);

            UI->NkLayoutRowStatic(Nk, 8, 300, 1);
            UI->NkSpacer(Nk);
            UI->NkLayoutRowStatic(Nk, 18, 300, 1);

            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "Asset Count: %d",
                         AssetsMode->StoredHeader.AssetCount);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "SizeOfStoredAsset: %d",
                         AssetsMode->StoredHeader.SizeOfStoredAsset);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "Major High  Version: %d",
                         (AssetsMode->StoredHeader.Version >> 24) & 0xFF);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "Major Low Version: %d",
                         (AssetsMode->StoredHeader.Version >> 16) & 0xFF);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "Minor High  Version: %d",
                         (AssetsMode->StoredHeader.Version >> 8) & 0xFF);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "Minor Low Version: %d",
                         AssetsMode->StoredHeader.Version & 0xFF);

            DrawShowStoredAssets(AssetsMode, UI, Nk);
            
            UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 40, INT_MAX);
            UI->NkLayoutSpacePush(Nk, UI->NkRect(0, 12, 98, 40));
            if(UI->NkButtonLabel(Nk, "Exit"))
            {
                AssetsMode->Exit = true;
            }

            UI->NkLayoutSpacePush(Nk, UI->NkRect(104, 12, 98, 40));
            if(UI->NkButtonLabel(Nk, "Write Assets"))
            {
            }

            UI->NkLayoutSpacePush(Nk, UI->NkRect(208, 12, 98, 40));
            if(UI->NkButtonLabel(Nk, "Write SSA"))
            {
            }
            UI->NkLayoutSpaceEnd(Nk);

            UIDrawAssetAdvanceView(AssetsMode, UI, Nk);
#if 0
            ui_layout BottomLeftLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-1275.0f, -665.0f));
            UIBeginRow(&BottomLeftLayout);

            UIButton(&BottomLeftLayout, "Exit",
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->Exit, true),
                     200.0f, BColor_Red);
            UIButton(&BottomLeftLayout, "Write Assets", 
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->WriteAssets, true),
                     200.0f, BColor_Green);
            UIButton(&BottomLeftLayout, "Write SSA", 
                     UISetUInt32Interaction(InteractionID(UIState), (u32 *)&AssetsMode->WriteSSA, true),
                     200.0f, BColor_Green);

            UIEndRow(&BottomLeftLayout);
            UIEndLayout(&BottomLeftLayout);

            ui_layout RightLayout = UIBeginLayout(UIState, Layout->MouseP, V2(-315.0f, 680.0f));
            UIDrawAssetAdvanceView(UIState, &RightLayout, "Asset Advance View Window", V2(1580.0f, 1380.0f),
                                   AssetsMode);
            UIEndLayout(&RightLayout);
#endif
        } break;

        case EditMode_Bitmap:
        {
//            DrawAssetsBitmapEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_SpriteSheet:
        {
//            DrawAssetsSpriteSheetEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_Tileset:
        {
//            DrawAssetsTilesetEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_Sound:
        {
//            DrawAssetsSoundEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_Text:
        {
//            DrawAssetsTextEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_Font:
        {
//            DrawAssetsFontEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_File:
        {
//            DrawAssetsFileEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_SSWM:
        {
//            DrawAssetsSSWMEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;
    }
}

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
GetOrCreateStringArray(editor_mode_assets *AssetsMode, char *Key, u32 StringCount)
{
    u64 LowMask = 0x00000000FFFFFFFF;
    u64 CRC = CRC64FromString(Key);

    u32 High = (u32)(CRC >> 32);
    u32 Low = (u32)(CRC & LowMask);

    u32 HashIndex = ((High >> 2) + (Low >> 2)) % ArrayCount(AssetsMode->EnumStringArraysHash);
    string_array **HashSlot = AssetsMode->EnumStringArraysHash + HashIndex;

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
        Result = PushStruct(&AssetsMode->UtilityArena, string_array);
        Result->Key = PushString(&AssetsMode->UtilityArena, Key);
        Result->StringCount = StringCount;
        Result->Strings = PushArray(&AssetsMode->UtilityArena, Result->StringCount, char *);

        json_element *EnumStrings = JsonLookupElement(AssetsMode->JsonStringsHead, Key);
        if(EnumStrings)
        {
            json_element *EnumString = EnumStrings->FirstSubElement;
            for(u32 ElementIndex = 0;
                ElementIndex < Result->StringCount;
                ++ElementIndex)
            {
                Result->Strings[ElementIndex] = EnumString->Value;
                EnumString = EnumString->NextSibling;
            }
        }
        
        Result->NextInHash = *HashSlot;
        *HashSlot = Result;
    }

    return(Result);
}

inline void
DrawShowStoredAssets(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk)
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
            stored_asset Asset = AssetsMode->StoredAssets[AssetIndex];
            char *TypeIDString = JsonGetEnumString(AssetsMode->JsonStringsHead, "AssetType", Asset.TypeID);
            char *TypeString = JsonGetEnumString(AssetsMode->JsonStringsHead, "StoredAssetType", Asset.Type);

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
DrawAssetAdvanceView(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk)
{
    char Text[256];
    UI->NkLayoutSpacePush(Nk, UI->NkRect(612, -900, 1290, 1068));
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[0]);
    if(UI->NkGroupBegin(Nk, "Asset Advance View", NK_WINDOW_NO_SCROLLBAR))
    {
        stored_asset *StoredAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex; 
        UI->NkLayoutRowDynamic(Nk, 40, 1);
        UI->NkPropertyInt(Nk, "Stored Asset: ", 0,
                          (int *)&AssetsMode->ShowStoredAssetIndex,
                          AssetsMode->StoredHeader.AssetCount, 1, 0.1f);

        UI->NkLayoutRowStatic(Nk, 770, 1280, 1);
        if(UI->NkGroupBegin(Nk, "Asset Specific View", NK_WINDOW_NO_SCROLLBAR))
        {
            switch(StoredAsset->Type)
            {
                case StoredAssetType_Bitmap:
                {
                    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
                    stored_asset_bitmap *StoredBitmap = &StoredAsset->Bitmap;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    UI->NkLabel(Nk, StoredBitmap->FileName, NK_TEXT_CENTERED);

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

                case StoredAssetType_SpriteSheet:
                {
                    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
                    stored_asset_spritesheet *SpriteSheet = &StoredAsset->SpriteSheet;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    UI->NkLabel(Nk, SpriteSheet->SourceFileName, NK_TEXT_CENTERED);

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

                case StoredAssetType_Tileset:
                {
                    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
                    stored_asset_tileset *StoredTileset = &StoredAsset->Tileset;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredTileset->SourceFileName);

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

                case StoredAssetType_Font:
                {
                    font_mode *FontMode = &AssetsMode->FontMode;
                    stored_asset_font *StoredFont = &StoredAsset->Font;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredFont->SourceFileName);

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
#if 0
                    if(FontMode->Font.GlyphCount)
                    {
                        UILabelWithInEditorFont(&WindowLayout, "abcdefghijklmnopqrstuvwxyz\nABCDEFGHIJKLMNOPQRSTUVWXYZ\n123456789.:,;'\"(!?)+-*/=",
                                                1512.0f, &FontMode->Font, 3.0f);
                    }
#endif
                } break;

                case StoredAssetType_Text:
                {
                    text_mode *TextMode = &AssetsMode->TextMode;
                    stored_asset_text *StoredText = &StoredAsset->Text;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredText->SourceFileName);

                    // TODO(paul): Show Text
//                    UILabel(&WindowLayout, TextMode->Text.String, 1512.0f);
//                    UISpace(&WindowLayout, V2(1512.0f, 30.0f));
                } break;

                case StoredAssetType_Sound:
                {
                    sound_mode *SoundMode = &AssetsMode->SoundMode;
                    stored_asset_sound *StoredSound = &StoredAsset->Sound;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredSound->SourceFileName);

                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "First Sample Index: %d", StoredSound->FirstSampleIndex);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    char *ChainString = JsonGetEnumString(AssetsMode->JsonStringsHead, "SSASoundChain", StoredSound->Chain);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Chain: %s", ChainString);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Sample Count: %d", SoundMode->Sound.SampleCount);
                    Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Channel Count: %d", SoundMode->Sound.ChannelCount);
                } break;

                case StoredAssetType_File:
                {
                    // TODO(paul): Add a feature to listen to the sound saved
                    binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
                    stored_asset_binary_file *StoredFile = &StoredAsset->File;

                    UI->NkLayoutRowDynamic(Nk, 30, 1);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredFile->SourceFileName);

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
                char *TypeID = JsonGetEnumString(AssetsMode->JsonStringsHead, "AssetType", StoredAsset->TypeID);
                char *StoredType = JsonGetEnumString(AssetsMode->JsonStringsHead, "StoredAssetType", StoredAsset->Type);
                UI->NkLayoutRowDynamic(Nk, 30, 1);
                FormatString(ArrayCount(Text), Text, "  InEditorID: %d",
                             StoredAsset->ID);
                struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                UI->NkLabel(Nk, Text, NK_TEXT_LEFT);

                FormatString(ArrayCount(Text), Text, "  TypeID: %s",
                             TypeID);
                Rect = UI->NkWidgetBounds(Nk);
                UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                UI->NkLabel(Nk, Text, NK_TEXT_LEFT);

                FormatString(ArrayCount(Text), Text, "  StoredType: %s",
                             StoredType);
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
                    ssa_tag Tag = StoredAsset->AssetTags[TagIndex];
                    char *TagString = JsonGetEnumString(AssetsMode->JsonStringsHead, "AssetTag", Tag.ID);
                    u32 TagValue = Tag.Value;
                    char *ValueKey = JsonGetTagValueEnumKey(AssetsMode->JsonStringsHead, Tag.ID);
                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                    if(StringsAreEqual(ValueKey, "Number"))
                    {
                        UI->NkLabelf(Nk, NK_TEXT_LEFT, "  %d. %s, %#x", TagIndex, TagString, TagValue);
                    }
                    else
                    {
                        char *ValueString = JsonGetEnumString(AssetsMode->JsonStringsHead, ValueKey, TagValue);
                        UI->NkLabelf(Nk, NK_TEXT_LEFT, "  %d. %s, %s", TagIndex, TagString, ValueString);
                    }
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

inline void
DrawStandardEditLayout(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk,
                       stored_asset *CurrentAsset)
{
    temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);

    u32 FileCount = 0;
    char **FileStrings = 0;
    char *Text = 0;
    switch(AssetsMode->EditMode)
    {
        case EditMode_Bitmap:
            FileCount = AssetsMode->BitmapFileCount; 
            FileStrings = AssetsMode->BitmapFiles;
            Text = "Choose Bitmap";
            break;

        case EditMode_SpriteSheet:
            FileCount = AssetsMode->SpriteSheetFileCount; 
            FileStrings = AssetsMode->SpriteSheetFiles;
            Text = "Choose SpriteSheet";
            break;

        case EditMode_Tileset:
            FileCount = AssetsMode->TilesetFileCount; 
            FileStrings = AssetsMode->TilesetFiles;
            Text = "Choose Tileset";
            break;

        case EditMode_Sound:
            FileCount = AssetsMode->SoundFileCount; 
            FileStrings = AssetsMode->SoundFiles;
            Text = "Choose Sound";
            break;

        case EditMode_Text:
            FileCount = AssetsMode->TextFileCount; 
            FileStrings = AssetsMode->TextFiles;
            Text = "Choose Text";
            break;

        case EditMode_Font:
            FileCount = AssetsMode->FontFileCount; 
            FileStrings = AssetsMode->FontFiles;
            Text = "Choose Font";
            break;

        case EditMode_File:
            FileCount = AssetsMode->BinaryFileCount; 
            FileStrings = AssetsMode->BinaryFiles;
            Text = "Choose File";
            break;

        case EditMode_SSWM:
            FileCount = AssetsMode->SSWMFileCount; 
            FileStrings = AssetsMode->SSWMFiles;
            Text = "Choose SSWM";
            break;

        InvalidDefaultCase;
    }

    Assert((FileStrings != 0) && (FileCount != 0) && (Text != 0));

    string_array *AssetStringArray = GetOrCreateStringArray(AssetsMode, "AssetType", Asset_Count);
    string_array *TagStringArray = GetOrCreateStringArray(AssetsMode, "AssetTag", Tag_Count);

    char *TagValueStringsKey = JsonGetTagValueEnumKey(AssetsMode->JsonStringsHead, AssetsMode->CurrentTagID);
    u32 ValueCount = TagValueCounts[AssetsMode->CurrentTagID];
    string_array *ValueStringArray = GetOrCreateStringArray(AssetsMode, TagValueStringsKey, ValueCount);

    UI->NkLayoutRowStatic(Nk, 450, 450, 1);
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "File Picker", NK_WINDOW_NO_SCROLLBAR))
    {        
        char *Strings = AssambleStrings(TempMem.Arena, FileStrings, &FileCount);
        UI->NkLayoutRowStatic(Nk, 30, 440, 1);
        struct nk_rect Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, Text, NK_TEXT_CENTERED);
        UI->NkComboboxString(Nk, Strings, (int *)&AssetsMode->FileIndex, FileCount, 30, {460, 460});

        if(AssetsMode->EditMode == EditMode_Tileset)
        {
            char *Strings = AssambleStrings(TempMem.Arena, AssetsMode->SolidTileFiles, &AssetsMode->SolidTileFileCount);
            UI->NkLayoutRowStatic(Nk, 30, 440, 1);
            struct nk_rect Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            UI->NkLabel(Nk, "Choose Merge Tile", NK_TEXT_CENTERED);
            UI->NkComboboxString(Nk, Strings, (int *)&AssetsMode->SubFileIndex, AssetsMode->SolidTileFileCount, 30, {460, 460});
        }

        UI->NkLayoutRowStatic(Nk, 30, 440, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Choose TypeID", NK_TEXT_CENTERED);

        u32 Count = AssetStringArray->StringCount;
        char *AssetStrings = AssambleStrings(TempMem.Arena, AssetStringArray->Strings, &Count, true);
        UI->NkComboboxString(Nk, AssetStrings, (int *)&CurrentAsset->TypeID, Count, 30, {460, 460});

        UI->NkLayoutRowStatic(Nk, 30, 440, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Choose Tag", NK_TEXT_CENTERED);
        Count = TagStringArray->StringCount;
        char *TagStrings = AssambleStrings(TempMem.Arena, TagStringArray->Strings, &Count, true);
        UI->NkComboboxString(Nk, TagStrings, (int *)&AssetsMode->CurrentTagID, Count, 30, {460, 460});

        UI->NkLayoutRowStatic(Nk, 30, 440, 1);
        char *TagString = JsonGetEnumString(AssetsMode->JsonStringsHead, "AssetTag", AssetsMode->CurrentTagID);

        if(AssetsMode->CurrentTagID != AssetsMode->LastTagID)
        {
            AssetsMode->LastTagID = AssetsMode->CurrentTagID;
            AssetsMode->CurrentTagValue = 0;
        }

        UI->NkPropertyInt(Nk, "Stored Asset: ", 0, (int *)&AssetsMode->CurrentTagValue, ValueCount - 1, 1, 0.1f);

        char *ValueString = ValueStringArray->Strings[AssetsMode->CurrentTagValue];
        UI->NkLabelf(Nk, NK_TEXT_LEFT, "Current Value: %s|%d", ValueString, AssetsMode->CurrentTagValue);

        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Choose Value", NK_TEXT_CENTERED);
        char *TagValues = AssambleStrings(TempMem.Arena, ValueStringArray->Strings, &ValueStringArray->StringCount);
        UI->NkComboboxString(Nk, TagValues, (int *)&AssetsMode->CurrentTagValue, ValueStringArray->StringCount, 30, {460, 460});

        if(UI->NkButtonLabel(Nk, "Add Tag"))
            AssetsMode->AddTag = true;

        UI->NkGroupEnd(Nk);
    }

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -454, 450, 320});
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Stored Asset Attributes", NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
    {        
        UI->NkLayoutRowDynamic(Nk, 30, 1);
        char *TypeIDString = AssetStringArray->Strings[CurrentAsset->TypeID];
        struct nk_rect Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "TypeID: %s", TypeIDString);

        char *StoredTypeString = JsonGetEnumString(AssetsMode->JsonStringsHead, "StoredAssetType", CurrentAsset->Type);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s", StoredTypeString);

        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "TagCount: %d", CurrentAsset->TagCount);

        char *TagsString = 0;
        u32 TotalSize = 0;
        for(u32 I = 0;
            I < CurrentAsset->TagCount;
            ++I)
        {
            ssa_tag *CurrentTag = CurrentAsset->AssetTags + I;
            u32 L = StringLength(TagStringArray->Strings[CurrentTag->ID]);
            TotalSize += L + 1;
        }

        TagsString = (char *)PushSize(TempMem.Arena, TotalSize);
        char *At = TagsString;
        for(u32 I = 0;
            I < CurrentAsset->TagCount;
            ++I)
        {
            ssa_tag *CurrentTag = CurrentAsset->AssetTags + I;
            u32 L = StringLength(TagStringArray->Strings[CurrentTag->ID]);
            Copy(L, TagStringArray->Strings[CurrentTag->ID], At);
            At += L;
            *At = 0;
            *At++;
        }
        
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[0]);
        UI->NkLabel(Nk, "Stored Asset Tags", NK_TEXT_CENTERED);
        UI->NkComboboxString(Nk, TagsString, (int *)&AssetsMode->CurrentTag, CurrentAsset->TagCount, 30, {440, 380});

        ssa_tag *CurrentTag = CurrentAsset->AssetTags + AssetsMode->CurrentTag;
        char *CurrentTagString = TagStringArray->Strings[CurrentTag->ID];
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Current Tag: %d. %s, %d",
                     AssetsMode->CurrentTag, CurrentTagString, CurrentTag->Value);

        char *TagValueStringsKey = JsonGetTagValueEnumKey(AssetsMode->JsonStringsHead, CurrentTag->ID);
        u32 ValueCount = TagValueCounts[CurrentTag->ID];
        string_array *ValueStringArray = GetOrCreateStringArray(AssetsMode, TagValueStringsKey, ValueCount);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Value: %s|%d",
                     ValueStringArray->Strings[CurrentTag->Value], CurrentTag->Value);

        if(UI->NkButtonLabel(Nk, "Remove Current Tag"))
            AssetsMode->RemoveTag = true;
        
        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {-5, 550, 450, 50});
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
DrawAssetsBitmapEditMode(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk, stored_asset *CurrentAsset)
{
    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
    stored_asset_bitmap *StoredBitmap = &CurrentAsset->Bitmap;
    loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
    
    DrawStandardEditLayout(AssetsMode, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 180});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Bitmap Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        UI->NkLayoutRowDynamic(Nk, 30, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ", AssetsMode->BitmapFiles[AssetsMode->FileIndex]);

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
        UI->NkLabel(Nk, "Stored Bitmap: ", NK_TEXT_CENTERED);

        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s", StoredBitmap->FileName);

        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "AlignPercentage: V2(%.02f, %.02f)",
                     StoredBitmap->AlignPercentage.x, StoredBitmap->AlignPercentage.y);

        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawAssetsSpriteSheetEditMode(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk, stored_asset *CurrentAsset)
{
    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
    stored_asset_spritesheet *StoredSpriteSheet = &CurrentAsset->SpriteSheet;
    loaded_bitmap *SpriteSheetBitmap = &SpriteSheetMode->SpriteSheetBitmap;

    DrawStandardEditLayout(AssetsMode, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 420});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "SpriteSheet Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        UI->NkLayoutRowDynamic(Nk, 30, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                     AssetsMode->SpriteSheetFiles[AssetsMode->FileIndex]);

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
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s", StoredSpriteSheet->SourceFileName);

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
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Sprite Count: %d", StoredSpriteSheet->SpriteCount);

        UI->NkLayoutRowDynamic(Nk, 30, 2);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Sprite Width: ", NK_TEXT_CENTERED);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabel(Nk, "Sprite Height: ", NK_TEXT_CENTERED);

        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%d pixels", StoredSpriteSheet->SpriteWidth);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%d pixels", StoredSpriteSheet->SpriteHeight);

        UI->NkLayoutRowDynamic(Nk, 30, 1);
        UI->NkPropertyInt(Nk, "Adjust Width: ", 0, (int *)&StoredSpriteSheet->SpriteWidth,
                          SpriteSheetBitmap->Width, 1, 0.1f);

        if(UI->NkButtonLabel(Nk, "Cut SpriteSheet"))
            SpriteSheetMode->CutSpriteSheet = true;

        if(UI->NkButtonLabel(Nk, SpriteSheetMode->ShowAnimated ? "Show Bitmap" : "Show Animated"))
            SpriteSheetMode->ShowAnimated = !SpriteSheetMode->ShowAnimated;
        
        UI->NkGroupEnd(Nk);
    }
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawAssetsTilesetEditMode(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk, stored_asset *CurrentAsset)
{
    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
    stored_asset_tileset *StoredTileset = &CurrentAsset->Tileset;
    loaded_bitmap *TilesetBitmap = &TilesetMode->TilesetBitmap;

    DrawStandardEditLayout(AssetsMode, UI, Nk, CurrentAsset);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 1);
    UI->NkLayoutSpacePush(Nk, {1460, -170, 450, 420});
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(UI->NkGroupBegin(Nk, "Tileset Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        UI->NkLayoutRowDynamic(Nk, 30, 1);
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                     AssetsMode->TilesetFiles[AssetsMode->FileIndex]);
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
        UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s", StoredTileset->SourceFileName);

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
        UI->NkLabel(Nk, IsMerged ? "Tileset is Merged" : "Tileset is not Merged", NK_TEXT_CENTERED);

        if(UI->NkButtonLabel(Nk, TilesetMode->ShowTiles ? "Show Bitmap" : "Show Tiles"))
            TilesetMode->ShowTiles = !TilesetMode->ShowTiles;

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
    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Merge Tile: %s",
                 AssetsMode->SolidTileFiles[AssetsMode->SubFileIndex]);

    UI->NkLayoutSpacePush(Nk, {0, -55, 460, 460});
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[14]);
    UI->NkStrokeRect(&Nk->current->buffer, Rect, 5.0f, 3.0f, ColorTable[2]);

    UI->NkLayoutSpacePush(Nk, {5, -50, 450, 450});
    if(TilesetMode->MergeTileBitmap.TextureHandle)
    {
        struct nk_image Img = UI->NkImagePtr(TilesetMode->MergeTileBitmap.TextureHandle);
        UI->NkImage(Nk, Img);
    }

    UI->NkLayoutSpaceEnd(Nk);
}

#if 0
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
#endif

internal void
DrawAssetsModeUI(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk)
{
    TIMED_FUNCTION();
    
    stored_asset *CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
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
            UI->NkLabelf(Nk, NK_TEXT_LEFT, "  Major High  Version: %d",
                         (AssetsMode->StoredHeader.Version >> 24) & 0xFF);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            UI->NkLabelf(Nk, NK_TEXT_LEFT, "  Major Low Version: %d",
                         (AssetsMode->StoredHeader.Version >> 16) & 0xFF);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            UI->NkLabelf(Nk, NK_TEXT_LEFT, "  Minor High  Version: %d",
                         (AssetsMode->StoredHeader.Version >> 8) & 0xFF);
            Rect = UI->NkWidgetBounds(Nk);
            UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            UI->NkLabelf(Nk, NK_TEXT_LEFT, "  Minor Low Version: %d",
                         AssetsMode->StoredHeader.Version & 0xFF);

            DrawShowStoredAssets(AssetsMode, UI, Nk);
            
            UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 40, INT_MAX);
            UI->NkLayoutSpacePush(Nk, UI->NkRect(0, 128, 130, 40));
            if(UI->NkButtonLabel(Nk, "Exit"))
            {
                AssetsMode->Exit = true;
            }

            UI->NkLayoutSpacePush(Nk, UI->NkRect(134, 128, 130, 40));
            if(UI->NkButtonLabel(Nk, "Write Assets"))
            {
                AssetsMode->WriteAssets = true;
            }

            UI->NkLayoutSpacePush(Nk, UI->NkRect(268, 128, 130, 40));
            if(UI->NkButtonLabel(Nk, "Write SSA"))
            {
                AssetsMode->WriteSSA = true;
            }
            UI->NkLayoutSpaceEnd(Nk);

            DrawAssetAdvanceView(AssetsMode, UI, Nk);
        } break;

        case EditMode_Bitmap:
        {
            DrawAssetsBitmapEditMode(AssetsMode, UI, Nk, CurrentAsset);
        } break;

        case EditMode_SpriteSheet:
        {
            DrawAssetsSpriteSheetEditMode(AssetsMode, UI, Nk, CurrentAsset);
        } break;

        case EditMode_Tileset:
        {
            DrawAssetsTilesetEditMode(AssetsMode, UI, Nk, CurrentAsset);
        } break;

        case EditMode_Sound:
        {
            DrawStandardEditLayout(AssetsMode, UI, Nk, CurrentAsset);
//            DrawAssetsSoundEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_Text:
        {
            DrawStandardEditLayout(AssetsMode, UI, Nk, CurrentAsset);
//            DrawAssetsTextEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_Font:
        {
            DrawStandardEditLayout(AssetsMode, UI, Nk, CurrentAsset);
//            DrawAssetsFontEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_File:
        {
            DrawStandardEditLayout(AssetsMode, UI, Nk, CurrentAsset);
//            DrawAssetsFileEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;

        case EditMode_SSWM:
        {
            DrawStandardEditLayout(AssetsMode, UI, Nk, CurrentAsset);
//            DrawAssetsSSWMEditMode(AssetsMode, UIState, Layout, CurrentAsset);
        } break;
    }
}

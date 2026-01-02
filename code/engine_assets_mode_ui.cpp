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
DrawShowStoredAssets(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk)
{
    char Text[64];
    nk_layout_row_static(Nk, 500, 460, 1);
    if(nk_group_begin(Nk, "Stored Assets View", NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
        nk_layout_row_static(Nk, 110, 440, 1);
        for(u32 AssetIndex = 0;
            AssetIndex < AssetsMode->StoredHeader.AssetCount;
            ++AssetIndex)
        {
            kesa_asset Asset = AssetsMode->StoredAssets[AssetIndex];

            FormatString(ArrayCount(Text), Text, "Asset%d", AssetIndex);
            struct nk_rect Bounds = nk_widget_bounds(Nk);

            nk_color C = ColorTable[2];
            if(nk_widget_is_hovered(Nk))
                C = ColorTable[3];                

            if(nk_widget_is_mouse_clicked(Nk, NK_BUTTON_LEFT))
                AssetsMode->ShowStoredAssetIndex = AssetIndex;
            nk_fill_rect(&Nk->current->buffer, Bounds, 0.0f, C);

            if(nk_group_begin(Nk, Text, NK_WINDOW_BORDER|NK_WINDOW_NO_SCROLLBAR))
            {
                nk_layout_row_static(Nk, 20, 440, 1);
                nk_labelf(Nk, NK_TEXT_ALIGN_LEFT, "%d. AssetID: %u",
                             AssetIndex, Asset.GUID);
                nk_labelf(Nk, NK_TEXT_ALIGN_LEFT, "TagCount: %d",
                             Asset.TagCount);

                nk_group_end(Nk);
            }
        }
        nk_group_end(Nk);
    }
}

inline void
DrawAssetAdvanceView(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk)
{
    char Text[256];
    nk_layout_space_push(Nk, nk_rect(612, -790, 1290, 1068));
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[0]);
    if(nk_group_begin(Nk, "Asset Advance View", NK_WINDOW_NO_SCROLLBAR))
    {
        kesa_asset *StoredAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex; 
        nk_layout_row_dynamic(Nk, 40, 1);
        nk_property_int(Nk, "Stored Asset: ", 0,
                        (int *)&AssetsMode->ShowStoredAssetIndex,
                        AssetsMode->StoredHeader.AssetCount, 1, 0.1f);

        nk_layout_row_static(Nk, 770, 1280, 1);
        if(nk_group_begin(Nk, "Asset Specific View", NK_WINDOW_NO_SCROLLBAR))
        {
            switch(StoredAsset->Type)
            {
                case KESA_Bitmap:
                {
                    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
                    kesa_bitmap *StoredBitmap = &StoredAsset->Bitmap;

                    nk_layout_row_dynamic(Nk, 30, 1);
                    struct nk_rect Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    nk_label(Nk, StoredAsset->SourceFileName, NK_TEXT_CENTERED);


                    f32 WidthOverHeight = (f32)BitmapMode->Bitmap.Width/(f32)BitmapMode->Bitmap.Height;
                    nk_layout_row_static(Nk, 520, (s32)(520*WidthOverHeight), 1);
                    Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[14]);
                    nk_stroke_rect(&Nk->current->buffer, Rect, 10.0f, 3.0f, ColorTable[2]);

                    if(BitmapMode->Bitmap.TextureHandle)
                    {
                        struct nk_image Img = nk_image_ptr(BitmapMode->Bitmap.TextureHandle);
                        nk_image(Nk, Img);
                    }

                    nk_layout_space_begin(Nk, NK_STATIC, 0, INT_MAX);
                    {
                        nk_layout_space_push(Nk, nk_rect(528, -528, 530, 374));
                        if(nk_group_begin(Nk, "Bitmap Stats", NK_WINDOW_NO_SCROLLBAR))
                        {
                            nk_layout_row_dynamic(Nk, 30, 2);
                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Width: %d pixels", BitmapMode->Bitmap.Width);

                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Height: %d pixels", BitmapMode->Bitmap.Height);

                            nk_layout_row_dynamic(Nk, 30, 1);
                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "AlignPercentage: V2(%.02f, %.02f)",
                                         StoredBitmap->AlignPercentage.x, StoredBitmap->AlignPercentage.y);
                            
                            nk_group_end(Nk);
                        }
                    }
                    nk_layout_space_end(Nk);
                } break;

                case KESA_SpriteSheet:
                {
                    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
                    kesa_spritesheet *SpriteSheet = &StoredAsset->SpriteSheet;

                    nk_layout_row_dynamic(Nk, 30, 1);
                    struct nk_rect Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    nk_label(Nk, StoredAsset->SourceFileName, NK_TEXT_CENTERED);

                    u32 SpriteIndex = (FloorReal32ToInt32(AssetsMode->Time*SpriteSheet->SpriteCount) %
                                       SpriteSheet->SpriteCount);
                    loaded_bitmap *SpriteBitmap = SpriteSheetMode->Sprites + SpriteIndex;

                    nk_layout_row_static(Nk, 530, 530, 1);
                    Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 5.0f, ColorTable[14]);
                    nk_stroke_rect(&Nk->current->buffer, Rect, 5.0f, 3.0f, ColorTable[2]);
                    if(nk_group_begin(Nk, "Image Stats", NK_WINDOW_NO_SCROLLBAR))
                    {
                        nk_layout_row_static(Nk, 520, 520, 1);
                        if(SpriteBitmap->TextureHandle)
                        {
                            struct nk_image Img = nk_image_ptr(SpriteBitmap->TextureHandle);
                            nk_image(Nk, Img);
                        }
                        
                        nk_group_end(Nk);
                    }

                    nk_layout_space_begin(Nk, NK_STATIC, 0, INT_MAX);
                    {
                        nk_layout_space_push(Nk, nk_rect(528, -528, 530, 374));
                        if(nk_group_begin(Nk, "SpriteSheet Stats", NK_WINDOW_NO_SCROLLBAR))
                        {
                            nk_layout_row_dynamic(Nk, 30, 2);
                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Sprite Width: %d pixels", SpriteSheet->SpriteWidth);

                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Sprite Height: %d pixels", SpriteSheet->SpriteHeight);

                            nk_layout_row_dynamic(Nk, 30, 1);
                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "AlignPercentage: V2(%.02f, %.02f)",
                                 SpriteSheet->SpriteAlignPercentage.x, SpriteSheet->SpriteAlignPercentage.y);

                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Sprite Count: %d", SpriteSheet->SpriteCount);
                            
                            nk_group_end(Nk);
                        }
                    }
                    nk_layout_space_end(Nk);
                } break;

                case KESA_Tileset:
                {
                    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
                    kesa_tileset *StoredTileset = &StoredAsset->Tileset;

                    nk_layout_row_dynamic(Nk, 30, 1);
                    struct nk_rect Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    u32 TileIndex = (FloorReal32ToInt32(AssetsMode->Time) %
                                     StoredTileset->TileCount);
                    loaded_bitmap *TileBitmap = TilesetMode->Tiles + TileIndex;
                    nk_layout_row_static(Nk, 530, 530, 1);
                    Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 5.0f, ColorTable[14]);
                    nk_stroke_rect(&Nk->current->buffer, Rect, 5.0f, 3.0f, ColorTable[2]);
                    if(nk_group_begin(Nk, "Image Stats", NK_WINDOW_NO_SCROLLBAR))
                    {
                        nk_layout_row_static(Nk, 520, 520, 1);
                        if(TileBitmap->TextureHandle)
                        {
                            struct nk_image Img = nk_image_ptr(TileBitmap->TextureHandle);
                            nk_image(Nk, Img);
                        }
                        
                        nk_group_end(Nk);
                    }

                    nk_layout_space_begin(Nk, NK_STATIC, 0, INT_MAX);
                    {
                        nk_layout_space_push(Nk, nk_rect(528, -528, 530, 374));
                        if(nk_group_begin(Nk, "Tileset Stats", NK_WINDOW_NO_SCROLLBAR))
                        {
                            nk_layout_row_dynamic(Nk, 30, 2);
                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Tile Width: %d pixels", StoredTileset->TileWidth);

                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Tile Height: %d pixels", StoredTileset->TileHeight);

                            nk_layout_row_dynamic(Nk, 30, 1);
                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Tile Count: %d", StoredTileset->TileCount);

                            Rect = nk_widget_bounds(Nk);
                            nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                            nk_labelf(Nk, NK_TEXT_CENTERED, "Merge Tile Source: %s", StoredTileset->MergeTileFileName);
                            
                            nk_group_end(Nk);
                        }
                    }
                    nk_layout_space_end(Nk);
                } break;

                case KESA_Text:
                {
                    text_mode *TextMode = &AssetsMode->TextMode;
                    kesa_text *StoredText = &StoredAsset->Text;

                    nk_layout_row_dynamic(Nk, 30, 1);
                    struct nk_rect Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    // TODO(paul): Show Text
//                    UILabel(&WindowLayout, TextMode->Text.String, 1512.0f);
//                    UISpace(&WindowLayout, V2(1512.0f, 30.0f));
                } break;

                case KESA_Sound:
                {
                    sound_mode *SoundMode = &AssetsMode->SoundMode;
                    kesa_sound *StoredSound = &StoredAsset->Sound;

                    nk_layout_row_dynamic(Nk, 30, 1);
                    struct nk_rect Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "First Sample Index: %d", StoredSound->FirstSampleIndex);
                    Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    char *ChainString = JsonGetEnumString(UIState->JsonStringsHead, "SSASoundChain", StoredSound->Chain);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "Chain: %s", ChainString);
                    Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "Sample Count: %d", SoundMode->Sound.SampleCount);
                    Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "Channel Count: %d", SoundMode->Sound.ChannelCount);
                } break;

                case KESA_File:
                {
                    // TODO(paul): Add a feature to listen to the sound saved
                    binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
                    kesa_binary_file *StoredFile = &StoredAsset->File;

                    nk_layout_row_dynamic(Nk, 30, 1);
                    struct nk_rect Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "Source: %s", StoredAsset->SourceFileName);

                    Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 4.0f, ColorTable[2]);
                    nk_labelf(Nk, NK_TEXT_CENTERED, "File Size: %d", StoredFile->FileSize);
                } break;
            }
            nk_group_end(Nk);
        }

        nk_layout_row_static(Nk, 240, 1280, 1);
        struct nk_rect Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 5.0f, ColorTable[2]);
        if(nk_group_begin(Nk, "General View", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_dynamic(Nk, 190, 2);
            if(nk_group_begin(Nk, "Stored Attributes: ", NK_WINDOW_TITLE))
            {
                char *StoredType = JsonGetEnumString(UIState->JsonStringsHead, "StoredAssetType", StoredAsset->Type);
                nk_layout_row_dynamic(Nk, 30, 1);
                struct nk_rect Rect = nk_widget_bounds(Nk);
                nk_fill_rect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                nk_labelf(Nk, NK_TEXT_LEFT, "  GUID: %llu", StoredAsset->GUID);

                FormatString(ArrayCount(Text), Text, "  StoredType: %s",
                             StoredType ? StoredType : "-");
                Rect = nk_widget_bounds(Nk);
                nk_fill_rect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                nk_label(Nk, Text, NK_TEXT_LEFT);
                
                
                nk_group_end(Nk);
            }

            if(nk_group_begin(Nk, "Stored Asset Preview Tags", NK_WINDOW_TITLE))
            {
                nk_layout_row_dynamic(Nk, 30, 1);
                for(u32 TagIndex = 0;
                    TagIndex < StoredAsset->TagCount;
                    ++TagIndex)
                {
                    kesa_tag StoredTag = StoredAsset->AssetTags[TagIndex];
                    kea_tag_map *Tag = GetTag(AssetsMode, StoredTag.TagGUID);

                    struct nk_rect Rect = nk_widget_bounds(Nk);
                    nk_fill_rect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                    nk_labelf(Nk, NK_TEXT_LEFT, "  %d. %s, %s, kea_tag_GUID: 0x%016x",
                              TagIndex, Tag->Key, Tag->Values[StoredTag.TagValueIndex],
                              Tag->ValueGUIDs[StoredTag.TagValueIndex]);
                }
                nk_group_end(Nk);
            }

            // NOTE(paul): Action on Current Stored Asset 
            nk_layout_row_dynamic(Nk, 30, 2);
            if(nk_button_label(Nk, "Edit Stored Asset"))
                AddAction(AssetsMode, AM_EditStoredAsset);

            nk_layout_space_push(Nk, nk_rect(790, 12, 470, 40));
            if(nk_button_label(Nk, "Remove Stored Asset"))
                AddAction(AssetsMode, AM_RemoveStoredAsset);

            nk_group_end(Nk);
        }

        nk_group_end(Nk);
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
ExtractTagValues(kea_tag_map *Tag, memory_arena *TempArena)
{
    char **Result = PushArray(TempArena, Tag->ValueCount, char *);
    for(u32 I = 0; I < Tag->ValueCount; ++I)
        Result[I] = Tag->Values[I];

    return(Result);
}
    
inline void
DrawStandardEditLayout(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk,
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

        case EditMode_File:
            Text = "Pick File Source \\|/";
            break;

        case EditMode_SSWM:
            Text = "Pick SSWM Source \\|/";
            break;

        InvalidDefaultCase;
    }

    f32 UIOffsetY = (AssetsMode->EditMode == EditMode_Tileset) ? 70.0f : 0.0f;
    
    nk_layout_row_static(Nk, 310.0f + UIOffsetY, 450, 1);
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "File Picker", NK_WINDOW_NO_SCROLLBAR))
    {        
        char *Strings = AssambleStrings(TempMem.Arena, FileStrings, &FileCount);

        nk_layout_row_static(Nk, 30, 440, 1);
        struct nk_rect Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_label(Nk, Text, NK_TEXT_CENTERED);
        nk_combobox_string(Nk, Strings, (int *)&AssetsMode->FileIndex,
                             FileCount, 30, {460, 460});

        if(AssetsMode->EditMode == EditMode_Tileset)
        {
            char *Strings = AssambleStrings(TempMem.Arena,
                                            AssetsMode->SolidTileFiles,
                                            &AssetsMode->SolidTileFileCount);

            nk_layout_row_static(Nk, 30, 440, 1);
            struct nk_rect Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "Choose Merge Tile", NK_TEXT_CENTERED);
            nk_combobox_string(Nk, Strings, (int *)&AssetsMode->SubFileIndex,
                                 AssetsMode->SolidTileFileCount, 30, {460, 460});
        }

        nk_layout_row_static(Nk, 30, 440, 1);
        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_label(Nk, "Choose Tag To Add \\|/", NK_TEXT_CENTERED);

        u32 Count = AssetsMode->TagHeader.TagCount;
        char *TagStrings = AssambleStrings(TempMem.Arena, AssetsMode->TagKeys, &Count, false);
        nk_combobox_string(Nk, TagStrings, (int *)&AssetsMode->CurrentTagIndex, Count, 30, {460, 460});

        kea_tag_map *CurrentTag = AssetsMode->Tags + AssetsMode->CurrentTagIndex;
        
        if(AssetsMode->CurrentTagIndex != AssetsMode->LastTagIndex)
        {
            AssetsMode->LastTagIndex = AssetsMode->CurrentTagIndex;
            AssetsMode->CurrentTagValue = 0;
        }

        nk_layout_row_static(Nk, 30, 440, 1);
        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_label(Nk, "Choose Tag Value", NK_TEXT_CENTERED);
        char *TagValues = AssambleStrings(TempMem.Arena,
                                          ExtractTagValues(CurrentTag, TempMem.Arena),
                                          &CurrentTag->ValueCount);

        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_labelf(Nk, NK_TEXT_LEFT, "  Current Value: %s|%d",
                     CurrentTag->Values[AssetsMode->CurrentTagValue], AssetsMode->CurrentTagValue);

        nk_combobox_string(Nk, TagValues,
                             (int *)&AssetsMode->CurrentTagValue,
                             CurrentTag->ValueCount, 30, {460, 460});

        if(nk_button_label(Nk, "Add Tag"))
            AddAction(AssetsMode, AM_AddTag);

        new_tag_map *NewTag = &AssetsMode->NewTag;
        kea_tag_map *NewTagMap = &NewTag->Tag;
        if(nk_button_label(Nk, "Create New Tag"))
        {
            for(u32 I = 0; I < NewTag->Tag.ValueCount; ++ I)
            {
                new_tag_map_value *Remove = NewTag->ValuesHead.Next;
                NewTag->ValuesHead.Next = Remove->Next;

                Remove->Next = NewTag->Free->Next;
                NewTag->Free->Next = Remove;
            }

            ZeroStruct(AssetsMode->NewTag.Tag);
            AssetsMode->NewTag.Tag.ValueCount = 1;
            new_tag_map_value *NewValue = 0;
            if(NewTag->Free)
            {
                NewValue = NewTag->Free;
                NewTag->Free = NewValue->Next; 
            }
            else
            {
                NewValue = (new_tag_map_value *)Platform.AllocateMemory(sizeof(new_tag_map_value));
            }

            NewValue->Next = NewTag->ValuesHead.Next;
            NewTag->ValuesHead.Next = NewValue;

            FormatString(ArrayCount(NewTagMap->Key),
                         NewTagMap->Key,
                         "Tag_None");
            FormatString(ArrayCount(NewValue->Value),
                         NewValue->Value,
                         "None");

            AssetsMode->CreatingNewTag = true;
        }

        if(AssetsMode->CreatingNewTag)
        {
            struct nk_rect PopupBounds;
            nk_window *Win = nk_window_find(Nk, "UI Window");
            struct nk_rect WinBounds = Win->bounds;
            PopupBounds.w = 420;
            PopupBounds.h = 320;
            PopupBounds.x = (WinBounds.w - PopupBounds.w)*0.5f;
            PopupBounds.y = (WinBounds.h - PopupBounds.h)*0.5f;

            Nk->style.window.fixed_background.data.color = ColorTable[2];
            if(nk_popup_begin(Nk, NK_POPUP_STATIC,
                              "Create Tag",
                              NK_WINDOW_TITLE|NK_WINDOW_BORDER,
                              PopupBounds))
            {
                nk_layout_row_dynamic(Nk, 30, 1);

                struct nk_rect EditBounds = nk_widget_bounds(Nk);
                nk_flags Active = nk_edit_string_zero_terminated(Nk, NK_EDIT_FIELD,
                                                                 NewTagMap->Key,
                                                                 ArrayCount(NewTagMap->Key),
                                                                 nk_filter_ascii);

                if((NewTagMap->Key[0] == 0) && (Active & NK_EDIT_INACTIVE))
                {
                    nk_color placeholder = nk_rgba(150, 150, 150, 128);
                    nk_draw_text(nk_window_get_canvas(Nk),
                                 EditBounds,
                                 "Enter Tag Key...", 16,
                                 Nk->style.font, placeholder, placeholder);
                }

                nk_layout_row_dynamic(Nk, 200, 1);
                struct nk_rect Rect = nk_widget_bounds(Nk);
                nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
                if(nk_group_begin(Nk, "Tag Values", NK_WINDOW_TITLE))
                {

                    nk_layout_row_dynamic(Nk, 30, 2);
                    if(nk_button_label(Nk, "Add"))
                    {
                        new_tag_map_value *NewValue = 0;
                        if(NewTag->Free)
                        {
                            NewValue = NewTag->Free;
                            NewTag->Free = NewTag->Free->Next; 
                        }
                        else
                        {
                            NewValue = (new_tag_map_value *)Platform.AllocateMemory(sizeof(new_tag_map_value));
                        }

                        NewValue->Next = NewTag->ValuesHead.Next;
                        NewTag->ValuesHead.Next = NewValue;

                        NewTagMap->ValueCount++;
                    }

                    if(nk_button_label(Nk, "Remove") && (NewTagMap->ValueCount > 1))
                    {
                        new_tag_map_value *Remove = NewTag->ValuesHead.Next;
                        NewTag->ValuesHead.Next = Remove->Next;                        
                        ZeroStruct(*Remove);

                        Remove->Next = NewTag->Free;
                        NewTag->Free = Remove;

                        NewTagMap->ValueCount--;
                    }

                    nk_layout_row_begin(Nk, NK_STATIC, 30, 2);
                    new_tag_map_value *Value = NewTag->ValuesHead.Next;
                    for(u32 I = 0;
                        I < NewTagMap->ValueCount;
                        ++I)
                    {

                        nk_layout_row_push(Nk, 20);
                        Rect = nk_widget_bounds(Nk);
                        nk_fill_rect(&Nk->current->buffer, Rect, 5.0f, ColorTable[1]);
                        nk_labelf(Nk, NK_TEXT_CENTERED, "%d.", I);
                        nk_layout_row_push(Nk, 358);
                        nk_flags Active = nk_edit_string_zero_terminated(Nk, NK_EDIT_FIELD,
                                                                         Value->Value,
                                                                         ArrayCount(Value->Value),
                                                                         nk_filter_ascii);
                        Value = Value->Next;
                    }
                    nk_layout_row_end(Nk);

                    nk_group_end(Nk);
                }
                
                nk_layout_row_dynamic(Nk, 30, 2);
                if(nk_button_label(Nk, "Save"))
                {
                    AssetsMode->CreatingNewTag = false;
                    AddAction(AssetsMode, AM_AddNewTag);
                }

                if(nk_button_label(Nk, "Close"))
                    AssetsMode->CreatingNewTag = false;

                Nk->style.window.fixed_background.data.color.a = 0;
                nk_popup_end(Nk);
            }
        }
        
        nk_group_end(Nk);
    }

    nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
    nk_layout_space_push(Nk, {1460, -314 - UIOffsetY, 450, 275});
    Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "Stored Asset Attributes", NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
    {        
        nk_layout_row_dynamic(Nk, 30, 1);
        char *StoredTypeString = JsonGetEnumString(UIState->JsonStringsHead,
                                                   "StoredAssetType", CurrentAsset->Type);
        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_labelf(Nk, NK_TEXT_CENTERED, "%s", StoredTypeString);

        char *TagsString = 0;
        u32 TotalSize = 0;
        for(u32 I = 0;
            I < CurrentAsset->TagCount;
            ++I)
        {
            kea_tag_map *CurrentTag = GetTag(AssetsMode, CurrentAsset->AssetTags[I].TagGUID);
            u32 L = StringLength(CurrentTag->Key);
            TotalSize += L + 1;
        }
        TagsString = (char *)PushSize(TempMem.Arena, TotalSize);
        char *At = TagsString;
        for(u32 I = 0;
            I < CurrentAsset->TagCount;
            ++I)
        {
            kea_tag_map *CurrentTag = GetTag(AssetsMode, CurrentAsset->AssetTags[I].TagGUID);
            u32 L = StringLength(CurrentTag->Key);
            Copy(L, CurrentTag->Key, At);
            At += L;
            *At = 0;
            *At++;
        }

        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[0]);
        nk_label(Nk, "Stored Asset Tags", NK_TEXT_CENTERED);

        kesa_tag StoredTag = CurrentAsset->AssetTags[AssetsMode->CurrentStoredTagIndex];
        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_labelf(Nk, NK_TEXT_CENTERED, "TagCount: %d", CurrentAsset->TagCount);
        nk_combobox_string(Nk, TagsString, (int *)&AssetsMode->CurrentStoredTagIndex,
                             CurrentAsset->TagCount, 30, {440, 380});

        kea_tag_map *CurrentTag = GetTag(AssetsMode, StoredTag.TagGUID);
        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_labelf(Nk, NK_TEXT_CENTERED, "Current Tag: %d. %s",
                     AssetsMode->CurrentStoredTagIndex, CurrentTag->Key);

        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_labelf(Nk, NK_TEXT_CENTERED, "Value: %s", CurrentTag->Values[StoredTag.TagValueIndex]);

        if(nk_button_label(Nk, "Remove Current Tag"))
            AddAction(AssetsMode, AM_RemoveTag);
        
        nk_group_end(Nk);
    }
    nk_layout_space_end(Nk);

    nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
    nk_layout_space_push(Nk, {-5, 690 - UIOffsetY, 450, 50});
    if(nk_group_begin(Nk, "Actions", NK_WINDOW_NO_SCROLLBAR))
    {        
        nk_layout_row_dynamic(Nk, 40, 2);
        if(nk_button_label(Nk, "Exit"))
            AssetsMode->EditMode = EditMode_None;

        if(nk_button_label(Nk, "Add Asset"))
            AddAction(AssetsMode, AM_AddAsset);
        
        nk_group_end(Nk);
    }
    nk_layout_space_end(Nk);

    EndTemporaryMemory(TempMem);
}

internal void
DrawAssetsBitmapEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk,
                         kesa_asset *CurrentAsset)
{
    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
    kesa_bitmap *StoredBitmap = &CurrentAsset->Bitmap;
    loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
    
    DrawStandardEditLayout(AssetsMode, UIState, Nk, CurrentAsset);

    nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
    nk_layout_space_push(Nk, {1460, -60, 450, 140});
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "Bitmap Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(Bitmap->Memory)
        {
            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "Stored Bitmap Attribs: ", NK_TEXT_CENTERED);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Source: %s",
                         AssetsMode->SourceFiles[KESA_Bitmap][AssetsMode->FileIndex]);

            nk_layout_row_dynamic(Nk, 30, 2);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Width: %d pixels", Bitmap->Width);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Height: %d pixels", Bitmap->Height);

            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "AlignPercentage: V2(%.02f, %.02f)",
                         StoredBitmap->AlignPercentage.x, StoredBitmap->AlignPercentage.y);

        }

        nk_group_end(Nk);
    }
    nk_layout_space_end(Nk);
}

internal void
DrawAssetsSpriteSheetEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk,
                              kesa_asset *CurrentAsset)
{
    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
    kesa_spritesheet *StoredSpriteSheet = &CurrentAsset->SpriteSheet;
    loaded_bitmap *SpriteSheetBitmap = &SpriteSheetMode->SpriteSheetBitmap;

    DrawStandardEditLayout(AssetsMode, UIState, Nk, CurrentAsset);

    if(SpriteSheetBitmap->Memory)
    {
        nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
        nk_layout_space_push(Nk, {1460, -70, 450, 420});
        struct nk_rect Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
        if(nk_group_begin(Nk, "SpriteSheet Attributes", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                      AssetsMode->SourceFiles[KESA_SpriteSheet][AssetsMode->FileIndex]);

            nk_layout_row_dynamic(Nk, 30, 2);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Width: %d pixels", SpriteSheetBitmap->Width);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Height: %d pixels", SpriteSheetBitmap->Height);

            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "SpriteSheet: ", NK_TEXT_CENTERED);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%s", CurrentAsset->SourceFileName);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "Sprite Align Percentage:", NK_TEXT_CENTERED);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "V2(%.02f, %.02f)",
                      StoredSpriteSheet->SpriteAlignPercentage.x,
                      StoredSpriteSheet->SpriteAlignPercentage.y);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Sprite Count: %d",
                      StoredSpriteSheet->SpriteCount);

            nk_layout_row_dynamic(Nk, 30, 2);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "Sprite Width: ", NK_TEXT_CENTERED);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "Sprite Height: ", NK_TEXT_CENTERED);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%d pixels",
                      StoredSpriteSheet->SpriteWidth);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%d pixels",
                      StoredSpriteSheet->SpriteHeight);

            nk_layout_row_dynamic(Nk, 30, 1);
            nk_property_int(Nk, "Adjust Width: ", 0,
                            (int *)&StoredSpriteSheet->SpriteWidth,
                            SpriteSheetBitmap->Width, 1, 0);

            if(nk_button_label(Nk, "Cut SpriteSheet"))
                SpriteSheetMode->CutSpriteSheet = true;

            if(nk_button_label(Nk, SpriteSheetMode->ShowAnimated ? "Show Bitmap" : "Show Animated"))
                SpriteSheetMode->ShowAnimated = !SpriteSheetMode->ShowAnimated;
        
            nk_group_end(Nk);
        }
        nk_layout_space_end(Nk);
    }
}

internal void
DrawAssetsTilesetEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk,
                          kesa_asset *CurrentAsset)
{
    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
    kesa_tileset *StoredTileset = &CurrentAsset->Tileset;
    loaded_bitmap *TilesetBitmap = &TilesetMode->TilesetBitmap;

    DrawStandardEditLayout(AssetsMode, UIState, Nk, CurrentAsset);

    if(TilesetBitmap->Memory)
    {
        nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
        nk_layout_space_push(Nk, {1460, -140, 450, 420});
        struct nk_rect Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
        if(nk_group_begin(Nk, "Tileset Attributes", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                      AssetsMode->SourceFiles[KESA_Tileset][AssetsMode->FileIndex]);
            nk_layout_row_dynamic(Nk, 30, 2);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Width: %d pixels", TilesetBitmap->Width);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Height: %d pixels", TilesetBitmap->Height);

            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "Tileset: ", NK_TEXT_CENTERED);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%s", CurrentAsset->SourceFileName);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Tile Count: %d", StoredTileset->TileCount);

            nk_layout_row_dynamic(Nk, 30, 2);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "Tile Width: ", NK_TEXT_CENTERED);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, "Tile Height: ", NK_TEXT_CENTERED);

            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%d pixels", StoredTileset->TileWidth);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%d pixels", StoredTileset->TileHeight);

            nk_layout_row_dynamic(Nk, 30, 1);
            nk_property_int(Nk, "Adjust Width: ", 0, (int *)&StoredTileset->TileWidth,
                            TilesetBitmap->Width, 1, 0.1f);
            nk_property_int(Nk, "Adjust Height: ", 0, (int *)&StoredTileset->TileHeight,
                            TilesetBitmap->Height, 1, 0.1f);


            nk_layout_row_dynamic(Nk, 30, 2);
            if(nk_button_label(Nk, "Cut With Merge"))
                TilesetMode->CutWithMergeTileset = true;

            if(nk_button_label(Nk, "Cut"))
                TilesetMode->CutTileset = true;

            nk_layout_row_dynamic(Nk, 30, 1);
            b32 IsMerged = StoredTileset->MergedTile;
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_label(Nk, IsMerged ? "Tileset is Merged" : "Tileset is not Merged",
                     NK_TEXT_CENTERED);

            if(nk_button_label(Nk, TilesetMode->ShowTiles ? "Show Bitmap" : "Show Tiles"))
                TilesetMode->ShowTiles = !TilesetMode->ShowTiles;

            nk_group_end(Nk);
        }
        nk_layout_space_end(Nk);

        nk_layout_space_begin(Nk, NK_STATIC, 40, 3);
        if(TilesetMode->ShowTiles)
        {
            nk_layout_space_push(Nk, {480, -396, 40, 40});
            if(nk_button_symbol(Nk, NK_SYMBOL_TRIANGLE_LEFT) &&
               (TilesetMode->CurrentTileIndex != 0))
            {
                TilesetMode->CurrentTileIndex -= 1;
            }

            nk_layout_space_push(Nk, {525, -396, 860, 40});
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Tile Index: %d",
                      TilesetMode->CurrentTileIndex);

            nk_layout_space_push(Nk, {1390, -396, 40, 40});
            if(nk_button_symbol(Nk, NK_SYMBOL_TRIANGLE_RIGHT) &&
               (TilesetMode->CurrentTileIndex < (StoredTileset->TileCount - 1)))
            {
                TilesetMode->CurrentTileIndex += 1;
            }
        }
        nk_layout_space_end(Nk);

        if(TilesetMode->MergeTileBitmap.Memory)
        {
            nk_layout_space_begin(Nk, NK_STATIC, 40, 3);
            nk_layout_space_push(Nk, {0, 20, 460, 30});
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Merge Tile: %s",
                      AssetsMode->SolidTileFiles[AssetsMode->SubFileIndex]);

            nk_layout_space_push(Nk, {0, 55, 460, 460});
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 5.0f, ColorTable[14]);
            nk_stroke_rect(&Nk->current->buffer, Rect, 5.0f, 3.0f, ColorTable[2]);

            nk_layout_space_push(Nk, {5, 60, 450, 450});
            if(TilesetMode->MergeTileBitmap.TextureHandle)
            {
                struct nk_image Img =
                    nk_image_ptr(TilesetMode->MergeTileBitmap.TextureHandle);
                nk_image(Nk, Img);
            }
            nk_layout_space_end(Nk);
        }
    }
}

internal void
DrawAssetsSoundEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk,
                        kesa_asset *CurrentAsset)
{
    // TODO(paul): Make it more comfortable to use, like in a music player,
    // a button to stop and resume, a progress bar, and a bit more information
    // about sound itself: length, frequency, volume, etc.
    sound_mode *SoundMode = &AssetsMode->SoundMode;
    kesa_sound *StoredSound = &CurrentAsset->Sound;
    
    DrawStandardEditLayout(AssetsMode, UIState, Nk, CurrentAsset);

    if(SoundMode->Sound.Samples[0])
    {
        temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);
        nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
        nk_layout_space_push(Nk, {1460, -70, 450, 110});
        struct nk_rect Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
        if(nk_group_begin(Nk, "Bitmap Attributes", NK_WINDOW_NO_SCROLLBAR))
        {
            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                      AssetsMode->SourceFiles[KESA_Sound][AssetsMode->FileIndex]);

            char *ChainString = SoundChainStringArray[StoredSound->Chain];
            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "Current Chain: %s", ChainString);

            u32 Count = KEASoundChain_Count;
            char *TagValues = AssambleStrings(TempMem.Arena,
                                              SoundChainStringArray,
                                              &Count);
            nk_combobox_string(Nk, TagValues, (int *)&StoredSound->Chain,
                               Count, 30, {460, 460});

            nk_group_end(Nk);
        }
        nk_layout_space_end(Nk);
        EndTemporaryMemory(TempMem);
    }

    nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
    nk_layout_space_push(Nk, {460, -340, 990, 85});
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "Actions", NK_WINDOW_NO_SCROLLBAR))
    {
        nk_layout_row_dynamic(Nk, 30, 1);
        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_label(Nk, "Actions", NK_TEXT_CENTERED);

        nk_layout_row_dynamic(Nk, 40, 2);
        if(nk_button_label(Nk, "Play Sound"))
            SoundMode->PlaySound = true;
        if(nk_button_label(Nk, "Stop Sound"))
            SoundMode->StopSound = true;

        nk_group_end(Nk);
    }
    nk_layout_space_end(Nk);
}

internal void
DrawAssetsTextEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk,
                       kesa_asset *CurrentAsset)
{
    text_mode *TextMode = &AssetsMode->TextMode;

    // TODO(paul): Implement Nuklear text edditing here
    DrawStandardEditLayout(AssetsMode, UIState, Nk, CurrentAsset);

    nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
    nk_layout_space_push(Nk, {1460, -170, 450, 140});

    nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
    nk_layout_space_push(Nk, {460, -340, 990, 450});
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "Actions", NK_WINDOW_NO_SCROLLBAR))
    {
        nk_layout_row_dynamic(Nk, 400, 1);
        Rect = nk_widget_bounds(Nk);
        nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
        nk_edit_string(Nk, NK_EDIT_BOX,
                       TextMode->EditBuffer,
                       &TextMode->EditBufferLength,
                       TextMode->EditBufferMaxLength,
                       nk_filter_default);

        nk_layout_row_dynamic(Nk, 30, 2);
        if(nk_button_label(Nk, "Save"))
            TextMode->EditTextFile = true;
        if(nk_button_label(Nk, "Reload"))
            TextMode->Reload = true;

        nk_group_end(Nk);
    }
}

internal void
DrawAssetsFileEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk,
                       kesa_asset *CurrentAsset)
{
    // TODO(paul): Advance on this one, what file is loaded what data it containce,
    // visualize all the data posiable?
    kesa_binary_file *StoredFile = &CurrentAsset->File;
    
    DrawStandardEditLayout(AssetsMode, UIState, Nk, CurrentAsset);

    nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
    nk_layout_space_push(Nk, {1460, -170, 450, 140});
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "File Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(StoredFile->FileSize)
        {
            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                         AssetsMode->SourceFiles[KESA_File][AssetsMode->FileIndex]);

            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "File Size: %d", StoredFile->FileSize);
        }

        nk_group_end(Nk);
    }
}

internal void
DrawAssetsSSWMEditMode(editor_mode_assets *AssetsMode, ui_state *UIState, nk_context *Nk,
                       kesa_asset *CurrentAsset)
{
    // TODO(paul): Advance on this one, visualize all the data posiable
    kesa_sswm_file *StoredFile = &CurrentAsset->SSWM;
    
    DrawStandardEditLayout(AssetsMode, UIState, Nk, CurrentAsset);

    nk_layout_space_begin(Nk, NK_STATIC, 20, 1);
    nk_layout_space_push(Nk, {1460, -170, 450, 140});
    struct nk_rect Rect = nk_widget_bounds(Nk);
    nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
    if(nk_group_begin(Nk, "SSWM Attributes", NK_WINDOW_NO_SCROLLBAR))
    {
        if(StoredFile->FileSize)
        {
            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "%s attribs: ",
                         AssetsMode->SourceFiles[KESA_SSWM][AssetsMode->FileIndex]);

            nk_layout_row_dynamic(Nk, 30, 1);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
            nk_labelf(Nk, NK_TEXT_CENTERED, "File Size: %d", StoredFile->FileSize);
        }
        
        nk_group_end(Nk);
    }
}

internal void
DrawAssetsModeUI(editor_mode_assets *AssetsMode, ui_state *UIState)
{
    TIMED_FUNCTION();

    nk_context *Nk = UIState->Nk;
    
    kesa_asset *CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
    if(IsAction(AssetsMode, AM_EditStoredAsset))
    {
        CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
    }
    
    switch(AssetsMode->EditMode)
    {
        case EditMode_None:
        {
            nk_layout_row_begin(Nk, NK_STATIC, 40, 3);
            {
                nk_layout_row_push(Nk, 200);
                if(nk_button_label(Nk, "Edit Bitmaps"))
                    AssetsMode->EditMode = EditMode_Bitmap;

                if(nk_button_label(Nk, "Edit Sounds"))
                    AssetsMode->EditMode = EditMode_Sound;

                if(nk_button_label(Nk, "Edit SpriteSheets"))
                    AssetsMode->EditMode = EditMode_SpriteSheet;

                if(nk_button_label(Nk, "Edit Tilesets"))
                    AssetsMode->EditMode = EditMode_Tileset;

                if(nk_button_label(Nk, "Edit Texts"))
                    AssetsMode->EditMode = EditMode_Text;

                if(nk_button_label(Nk, "Edit Files"))
                    AssetsMode->EditMode = EditMode_File;

                if(nk_button_label(Nk, "Edit SSWM"))
                    AssetsMode->EditMode = EditMode_SSWM;

                if(nk_button_label(Nk, "placeholder")) {}
                if(nk_button_label(Nk, "placeholder")) {}
                if(nk_button_label(Nk, "placeholder")) {}
                if(nk_button_label(Nk, "placeholder")) {}
            }
            nk_layout_row_end(Nk);

            nk_layout_row_static(Nk, 8, 300, 1);
            nk_spacer(Nk);
            nk_layout_row_static(Nk, 30, 300, 1);

            struct nk_rect Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            nk_labelf(Nk, NK_TEXT_LEFT, "  Asset Count: %d",
                         AssetsMode->StoredHeader.AssetCount);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            nk_labelf(Nk, NK_TEXT_LEFT, "  SizeOfStoredAsset: %d",
                         AssetsMode->StoredHeader.SizeOfStoredAsset);
            Rect = nk_widget_bounds(Nk);
            nk_fill_rect(&Nk->current->buffer, Rect, 10.0f, ColorTable[2]);
            nk_labelf(Nk, NK_TEXT_LEFT, "  Version: %d.%d.%d.%d",
                         (AssetsMode->StoredHeader.Version >> 24) & 0xFF,
                         (AssetsMode->StoredHeader.Version >> 16) & 0xFF,
                         (AssetsMode->StoredHeader.Version >> 8) & 0xFF,
                         AssetsMode->StoredHeader.Version & 0xFF);

            DrawShowStoredAssets(AssetsMode, UIState, Nk);
            
            nk_layout_space_begin(Nk, NK_STATIC, 40, INT_MAX);
            nk_layout_space_push(Nk, nk_rect(0, 238, 130, 40));
            if(nk_button_label(Nk, "Exit"))
                AddAction(AssetsMode, AM_Exit);

            nk_layout_space_push(Nk, nk_rect(134, 238, 130, 40));
            if(nk_button_label(Nk, "Write Assets"))
                AddAction(AssetsMode, AM_WriteAssets);

            nk_layout_space_push(Nk, nk_rect(268, 238, 130, 40));
            if(nk_button_label(Nk, "Write SSA"))
                AddAction(AssetsMode, AM_WriteKEA);
            nk_layout_space_end(Nk);

            DrawAssetAdvanceView(AssetsMode, UIState, Nk);
        } break;

        case EditMode_Bitmap:
            DrawAssetsBitmapEditMode(AssetsMode, UIState, Nk, CurrentAsset);
            break;

        case EditMode_SpriteSheet:
            DrawAssetsSpriteSheetEditMode(AssetsMode, UIState, Nk, CurrentAsset);
            break;

        case EditMode_Tileset:
            DrawAssetsTilesetEditMode(AssetsMode, UIState, Nk, CurrentAsset);
            break;

        case EditMode_Sound:
            DrawAssetsSoundEditMode(AssetsMode, UIState, Nk, CurrentAsset);
            break;

        case EditMode_Text:
            DrawAssetsTextEditMode(AssetsMode, UIState, Nk, CurrentAsset);
            break;

        case EditMode_File:
            DrawAssetsFileEditMode(AssetsMode, UIState, Nk, CurrentAsset);
            break;

        case EditMode_SSWM:
            DrawAssetsSSWMEditMode(AssetsMode, UIState, Nk, CurrentAsset);
            break;
    }
}

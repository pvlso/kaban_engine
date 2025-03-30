/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline void
DrawWidget(editor_mode_assets *AssetsMode, u32 AssetIndex, nk_ui *UI, nk_context *Nk)
{
    char Text[256];
    stored_asset Asset = AssetsMode->StoredAssets[AssetIndex];
    char *TypeIDString = JsonGetEnumString(AssetsMode->JsonStringsHead, "AssetType", Asset.TypeID);
    char *TypeString = JsonGetEnumString(AssetsMode->JsonStringsHead, "StoredAssetType", Asset.Type);
    FormatString(ArrayCount(Text), Text,
                 "%d. AssetID: %d",
                 AssetIndex, Asset.ID);

    UI->NkLayoutRowStatic(Nk, 100, 280, 1);
    if(UI->NkGroupBegin(Nk, Text,
                        NK_WINDOW_BORDER|NK_WINDOW_TITLE|NK_WINDOW_NO_SCROLLBAR))
    {
        UI->NkLayoutRowStatic(Nk, 20, 280, 1);
        FormatString(ArrayCount(Text), Text,
                     "AssetTypeID: %s",
                     TypeIDString);
        UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);
        FormatString(ArrayCount(Text), Text,
                     "AssetType: %s",
                     TypeString);
        UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);
        FormatString(ArrayCount(Text), Text,
                     "TagCount: %d",
                     Asset.TagCount);
        UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);

        UI->NkGroupEnd(Nk);
    }
}

inline void
DrawShowStoredAssets(editor_mode_assets *AssetsMode, nk_ui *UI, nk_context *Nk)
{
    UI->NkLayoutRowStatic(Nk, 20, 300, 1);
    UI->NkSelectableLabel(Nk, "Show Stored Assets", NK_TEXT_ALIGN_CENTERED, &AssetsMode->ShowStoredAssets);
    UI->NkLayoutRowBegin(Nk, NK_STATIC, 240, 1);
    {
        UI->NkLayoutRowPush(Nk, 300);
        if(AssetsMode->ShowStoredAssets)
        {
            if(UI->NkGroupBegin(Nk, "Show Stored Assets", NK_WINDOW_BORDER))
            {
                for(u32 AssetIndex = 0;
                    AssetIndex < AssetsMode->StoredHeader.AssetCount;
                    ++AssetIndex)
                {

                    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
                    Rect.h += 4;
                    Rect.y -= 2;

                    if(UI->NkWidgetIsHovered(Nk))
                        UI->NkFillRect(&Nk->current->buffer, Rect, 0.0f, {255, 0, 255, 100});
                    else
                        UI->NkFillRect(&Nk->current->buffer, Rect, 0.0f, {255, 0, 0, 100});

                    int a = 0;

                    DrawWidget(AssetsMode, AssetIndex, UI, Nk);
                }
                UI->NkGroupEnd(Nk);
            }
        }
    }
    UI->NkLayoutRowEnd(Nk);
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

            char Text[256];
            FormatString(ArrayCount(Text), Text,
                         "Asset Count: %d",
                         AssetsMode->StoredHeader.AssetCount);
            UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);

            FormatString(ArrayCount(Text), Text,
                         "SizeOfStoredAsset: %d",
                         AssetsMode->StoredHeader.SizeOfStoredAsset);
            UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);

            FormatString(ArrayCount(Text), Text,
                         "Major High  Version: %d",
                         (AssetsMode->StoredHeader.Version >> 24) & 0xFF);
            UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);

            FormatString(ArrayCount(Text), Text,
                         "Major Low Version: %d",
                         (AssetsMode->StoredHeader.Version >> 16) & 0xFF);
            UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);

            FormatString(ArrayCount(Text), Text,
                         "Minor High  Version: %d",
                         (AssetsMode->StoredHeader.Version >> 8) & 0xFF);
            UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);

            FormatString(ArrayCount(Text), Text,
                         "Minor Low Version: %d",
                         AssetsMode->StoredHeader.Version & 0xFF);
            UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);

            DrawShowStoredAssets(AssetsMode, UI, Nk);

            UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);
            {
                UI->NkLayoutSpacePush(Nk, UI->NkRect(320, -555, 940, 700));
                if(UI->NkGroupBegin(Nk, "Asset Advance View",
                                    NK_WINDOW_BORDER|NK_WINDOW_TITLE))
                {
                    UI->NkLayoutRowDynamic(Nk, 20, 1);
                    UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);
                    UI->NkLayoutRowDynamic(Nk, 20, 1);
                    UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);
                    UI->NkLayoutRowDynamic(Nk, 20, 1);
                    UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);
                    UI->NkLayoutRowDynamic(Nk, 20, 1);
                    UI->NkLabel(Nk, Text, NK_TEXT_ALIGN_LEFT);

                    UI->NkGroupEnd(Nk);
                }
            }
            UI->NkLayoutSpaceEnd(Nk);

            UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 40, 1);
            {
                UI->NkLayoutSpacePush(Nk, UI->NkRect(0, 80, 300, 60));
                if(UI->NkGroupBegin(Nk, "Action Buttons", 0))
                {
                    UI->NkLayoutRowDynamic(Nk, 40, 3);
                    if(UI->NkButtonLabel(Nk, "Exit"))
                    {
                    }
                    if(UI->NkButtonLabel(Nk, "Write Assets"))
                    {
                    }
                    if(UI->NkButtonLabel(Nk, "Write SSA"))
                    {
                    }

                    UI->NkGroupEnd(Nk);
                }
            }
            UI->NkLayoutSpaceEnd(Nk);

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

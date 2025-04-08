/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal void
DrawGameModeUI(editor_mode_game *GameMode, nk_ui *UI, nk_context *Nk)
{
    UI->NkLayoutRowStatic(Nk, 30, 200, 2);

    char *ModeString = JsonGetEnumString(GameMode->JsonStringsHead, "EditGameMode", GameMode->GameEditMode);
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabel(Nk, "Current Mode: ", NK_TEXT_CENTERED);

    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "%s", ModeString);

    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);

    nk_color Red = {255, 0, 0, 255};
    nk_color Green = {0, 255, 0, 255};
    UI->NkLabelfColored(Nk, NK_TEXT_CENTERED,
                        (IsSetGameModeFlag(GameMode, GMFlag_EditEnable) ? Green : Red),
                        "Edit Enable: %s", IsSetGameModeFlag(GameMode, GMFlag_EditEnable) ? "true" : "false");
    switch(GameMode->GameEditMode)
    {
        case EditGameMode_None:
        {

            UI->NkLayoutRowStatic(Nk, 30, 400, 1);
            UI->NkPropertyInt(Nk, "Ground Layer: ", 0,
                              (int *)&GameMode->MapGroundLayer, 15, 1, 0.1f);
            UI->NkPropertyInt(Nk, "Layer Count: ", 0,
                              (int *)&GameMode->LayerCount, 15, 1, 0.1f);

            UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);
            UI->NkLayoutSpacePush(Nk, {0, 898, 120, 40});

            if(UI->NkButtonLabel(Nk, "Exit"))
                GameMode->CurrentAction = GMAction_Exit;

            UI->NkLayoutSpacePush(Nk, {125, 898, 120, 40});
            if(UI->NkButtonLabel(Nk, "Write SSWM"))
                GameMode->CurrentAction = GMAction_WriteSSWM;

            UI->NkLayoutSpaceEnd(Nk);
        } break;
#if 0
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

//                UIPictureElement(&UIState->MouseTextLayout, 80.0f, 0, GameMode->Tile.BitmapID);
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
#endif
    }
}

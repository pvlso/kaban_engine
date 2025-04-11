/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline void
DrawTerrainModeUI(editor_mode_game *GameMode, editor_assets *Assets, u32 GenerationID, nk_ui *UI, nk_context *Nk, s16 MouseZ)
{
    nk_color Red = {255, 0, 0, 255};
    nk_color Green = {0, 255, 0, 255};
    nk_color Highlight = {200, 200, 200, 160};
    nk_color White = {255, 255, 255, 255};

    UI->NkLayoutRowStatic(Nk, 30, 260, 1);
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Current Z Layer: %d", GameMode->CurrentZLayer);

    UI->NkLayoutRowStatic(Nk, 30, 260, 2);
    if(UI->NkButtonLabel(Nk, "Show only this layer"))
        GameMode->CurrentAction = GMAction_ShowCurrentLayer;
    if(UI->NkButtonLabel(Nk, "Toggle Fill"))
        GameMode->FillActive = !GameMode->FillActive;

    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelfColored(Nk, NK_TEXT_CENTERED,
                        (IsSetGameModeFlag(GameMode, GMFlag_ShowCurrentLayer) ? Green : Red),
                        "Active: %s", IsSetGameModeFlag(GameMode, GMFlag_ShowCurrentLayer) ? "true" : "false");
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelfColored(Nk, NK_TEXT_CENTERED,
                        (GameMode->FillActive ? Green : Red),
                        "Active: %s", GameMode->FillActive ? "true" : "false");

    asset_type TilesetAssetType = Assets->AssetTypes[Asset_Tileset];
    if(TilesetAssetType.FirstAssetIndex != TilesetAssetType.OnePastLastAssetIndex)
    {
        UI->NkPropertyInt(Nk, "Choose Tileset: ", TilesetAssetType.FirstAssetIndex,
                          (int *)&GameMode->CurrentTileset.Value,
                          TilesetAssetType.OnePastLastAssetIndex - 1, 1, 0.1f);
    }

    if(GameMode->Tileset)
    {
        u32 TileCount = GameMode->TilesetInfo->TileCount;
        array_cursor *TileCursor = &GameMode->TileCursor;
        if(TileCursor->ElementCount != TileCount)
        {
            ResetCursorArray(TileCursor);
        }
    
        ChangeCursorPositionForToolBar(TileCursor, TileCount, MouseZ);

        UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 74, 1);
        UI->NkLayoutSpacePush(Nk, {10, 5, 69*(f32)TileCursor->ArrayCount, 74});
        Rect = UI->NkWidgetBounds(Nk);
        UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, ColorTable[2]);
        Rect.w += 4;
        Rect.h += 4;
        Rect.y -= 2;
        Rect.x -= 2;
        UI->NkStrokeRect(&Nk->current->buffer, Rect, 5.0f, 6.0f, ColorTable[1]);
        if(UI->NkGroupBegin(Nk, "Tool Bar", NK_WINDOW_NO_SCROLLBAR))
        {
            UI->NkLayoutRowStatic(Nk, 64, 64, TileCursor->ArrayCount);
            for(u32 ElementIndex = 0;
                ElementIndex < TileCursor->ArrayCount;
                ++ElementIndex)
            {
                b32 Current = (ElementIndex == TileCursor->ArrayPosition);
                Rect = UI->NkWidgetBounds(Nk);
                UI->NkFillRect(&Nk->current->buffer, Rect, 5.0f, Current ? ColorTable[1] : ColorTable[3]);

                u32 TileIndex = TileCursor->Array[ElementIndex];
                bitmap_id ID = GameMode->Tileset->Tiles[TileIndex].BitmapID;
                ID.Value += GameMode->Tileset->BitmapIDOffset;
        
                PrefetchBitmap(Assets, ID, true);
                loaded_bitmap *Bitmap = GetBitmap(Assets, ID, GenerationID);
                
                if(Bitmap->TextureHandle)
                {
                    struct nk_image Img = UI->NkImagePtr(Bitmap->TextureHandle);
                    UI->NkImageColor(Nk, Img, Current ? Highlight : White);
                }
            }

            TileCursor->ElementCount = TileCount;

            UI->NkGroupEnd(Nk);
        }
        UI->NkLayoutSpaceEnd(Nk);

        u32 TileIndex = GameMode->TileCursor.Array[GameMode->TileCursor.ArrayPosition];
        GameMode->Tile.BitmapID = GameMode->Tileset->Tiles[TileIndex].BitmapID;
        GameMode->Tile.BitmapID.Value += GameMode->Tileset->BitmapIDOffset;
        GameMode->Tile.CheckSum = GameMode->Tileset->Tiles[TileIndex].CheckSum;

        UI->NkTooltipBegin(Nk, 70);

        PrefetchBitmap(Assets, GameMode->Tile.BitmapID, true);
        loaded_bitmap *Bitmap = GetBitmap(Assets, GameMode->Tile.BitmapID, GenerationID);
        UI->NkLayoutRowStatic(Nk, 64, 64, 1);
        if(Bitmap->TextureHandle)
        {
            struct nk_image Img = UI->NkImagePtr(Bitmap->TextureHandle);
            UI->NkImage(Nk, Img);
        }
        UI->NkTooltipEnd(Nk);
    }

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);
    UI->NkLayoutSpacePush(Nk, {0, 825, 160, 40});
    if(UI->NkButtonLabel(Nk, "Exit"))
        GameMode->CurrentAction = GMAction_Exit;
    UI->NkLayoutSpaceEnd(Nk);
}

inline void
DrawNavMeshModeUI(editor_mode_game *GameMode, nk_ui *UI, nk_context *Nk)
{
    UI->NkLayoutRowStatic(Nk, 30, 260, 1);
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Polygon Count: %d", GameMode->PolygonCount);
    UI->NkLayoutRowStatic(Nk, 30, 130, 4);

    if(UI->NkButtonLabel(Nk, "Start New"))
        GameMode->CurrentAction = GMAction_StartNewPolygon;
    if(UI->NkButtonLabel(Nk, "Reset Curr"))
        GameMode->CurrentAction = GMAction_ResetCurrentPolygon;
    if(UI->NkButtonLabel(Nk, "Delete Curr"))
        GameMode->CurrentAction = GMAction_DeleteCurrentPolygon;
    if(UI->NkButtonLabel(Nk, "Triangulate"))
        GameMode->CurrentAction = GMAction_TriangulateAll;

    UI->NkLayoutRowStatic(Nk, 30, 260, 1);
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Current Polygon: %d", GameMode->CurrentPolygonIndex);

    UI->NkLayoutRowStatic(Nk, 30, 260, 1);
    UI->NkPropertyInt(Nk, "Choose Poly: ", 0,
                      (int *)&GameMode->CurrentPolygonIndex,
                      GameMode->PolygonCount - 1, 1, 0.1f);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);
    UI->NkLayoutSpacePush(Nk, {0, 825, 160, 40});
    if(UI->NkButtonLabel(Nk, "Exit"))
        GameMode->CurrentAction = GMAction_Exit;

    UI->NkLayoutSpacePush(Nk, {165, 825, 160, 40});
    if(UI->NkButtonLabel(Nk, "Write Polygons"))
        GameMode->CurrentAction = GMAction_WritePolygons;
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawGameModeUI(editor_mode_game *GameMode, editor_assets *Assets, u32 GenerationID, ui_state *UIState)
{
    nk_ui *UI = UIState->UI;
    nk_context *Nk = UIState->Nk;

    UI->NkLayoutRowStatic(Nk, 30, 260, 2);

    char *ModeString = JsonGetEnumString(UIState->JsonStringsHead, "EditGameMode", GameMode->GameEditMode);
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
            UI->NkLayoutRowStatic(Nk, 30, 260, 1);
            UI->NkPropertyInt(Nk, "Ground Layer: ", 0,
                              (int *)&GameMode->MapGroundLayer, 15, 1, 0.1f);
            UI->NkPropertyInt(Nk, "Layer Count: ", 0,
                              (int *)&GameMode->LayerCount, 15, 1, 0.1f);

            UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);

            UI->NkLayoutSpacePush(Nk, {0, 890, 130, 40});
            if(UI->NkButtonLabel(Nk, "Exit"))
                GameMode->CurrentAction = GMAction_Exit;

            UI->NkLayoutSpacePush(Nk, {135, 890, 130, 40});
            if(UI->NkButtonLabel(Nk, "Write SSWM"))
                GameMode->CurrentAction = GMAction_WriteSSWM;

            UI->NkLayoutSpaceEnd(Nk);
        } break;

        case EditGameMode_Terrain:
        {
            DrawTerrainModeUI(GameMode, Assets, GenerationID, UI, Nk, UIState->MouseZ);
            
        } break;

        case EditGameMode_NavMeshes:
        {
            DrawNavMeshModeUI(GameMode, UI, Nk);
        } break;
    }
}

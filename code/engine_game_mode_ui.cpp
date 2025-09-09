/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline void
DrawTerrainModeUI(engine_map_editor *MapEditor, editor_assets *Assets, u32 GenerationID, nk_ui *UI, nk_context *Nk, s16 MouseZ)
{
    nk_color Red = {255, 0, 0, 255};
    nk_color Green = {0, 255, 0, 255};
    nk_color Highlight = {200, 200, 200, 160};
    nk_color White = {255, 255, 255, 255};

    UI->NkLayoutRowStatic(Nk, 30, 260, 1);
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Current Z Layer: %d", MapEditor->CurrentZLayer);

    UI->NkLayoutRowStatic(Nk, 30, 260, 2);
    if(UI->NkButtonLabel(Nk, "Show only this layer"))
        MapEditor->CurrentAction = MEAction_ShowCurrentLayer;
    if(UI->NkButtonLabel(Nk, "Toggle Fill"))
        MapEditor->FillActive = !MapEditor->FillActive;

    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelfColored(Nk, NK_TEXT_CENTERED,
                        (IsSetMapEditorFlag(MapEditor, MEFlag_ShowCurrentLayer) ? Green : Red),
                        "Active: %s", IsSetMapEditorFlag(MapEditor, MEFlag_ShowCurrentLayer) ? "true" : "false");
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelfColored(Nk, NK_TEXT_CENTERED,
                        (MapEditor->FillActive ? Green : Red),
                        "Active: %s", MapEditor->FillActive ? "true" : "false");

    asset_type TilesetAssetType = Assets->AssetTypes[Asset_Tileset];
    if(TilesetAssetType.FirstAssetIndex != TilesetAssetType.OnePastLastAssetIndex)
    {
        UI->NkPropertyInt(Nk, "Choose Tileset: ", TilesetAssetType.FirstAssetIndex,
                          (int *)&MapEditor->CurrentTileset.Value,
                          TilesetAssetType.OnePastLastAssetIndex - 1, 1, 0.1f);
    }

    if(MapEditor->Tileset)
    {
        u32 TileCount = MapEditor->TilesetInfo->TileCount;
        array_cursor *TileCursor = &MapEditor->TileCursor;
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
                bitmap_id ID = MapEditor->Tileset->Tiles[TileIndex].BitmapID;
                ID.Value += MapEditor->Tileset->BitmapIDOffset;
        
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

        u32 TileIndex = MapEditor->TileCursor.Array[MapEditor->TileCursor.ArrayPosition];
        MapEditor->Tile.BitmapID = MapEditor->Tileset->Tiles[TileIndex].BitmapID;
        MapEditor->Tile.BitmapID.Value += MapEditor->Tileset->BitmapIDOffset;
        MapEditor->Tile.CheckSum = MapEditor->Tileset->Tiles[TileIndex].CheckSum;

        UI->NkTooltipBegin(Nk, 70);

        PrefetchBitmap(Assets, MapEditor->Tile.BitmapID, true);
        loaded_bitmap *Bitmap = GetBitmap(Assets, MapEditor->Tile.BitmapID, GenerationID);
        UI->NkLayoutRowStatic(Nk, 64, 64, 1);
        if(Bitmap->TextureHandle)
        {
            struct nk_image Img = UI->NkImagePtr(Bitmap->TextureHandle);
            UI->NkImage(Nk, Img);
        }
        UI->NkTooltipEnd(Nk);
    }

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);
    UI->NkLayoutSpacePush(Nk, {0, 805, 160, 40});
    if(UI->NkButtonLabel(Nk, "Exit"))
        MapEditor->CurrentAction = MEAction_Exit;
    UI->NkLayoutSpaceEnd(Nk);
}

inline void
DrawNavMeshModeUI(engine_map_editor *MapEditor, nk_ui *UI, nk_context *Nk)
{
    UI->NkLayoutRowStatic(Nk, 30, 260, 1);
    struct nk_rect Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Polygon Count: %d",
                 MapEditor->NavMesh.PolygonCount);
    UI->NkLayoutRowStatic(Nk, 30, 130, 4);

    if(UI->NkButtonLabel(Nk, "Start New"))
        MapEditor->CurrentAction = MEAction_StartNewPolygon;
    if(UI->NkButtonLabel(Nk, "Reset Curr"))
        MapEditor->CurrentAction = MEAction_ResetCurrentPolygon;
    if(UI->NkButtonLabel(Nk, "Delete Curr"))
        MapEditor->CurrentAction = MEAction_DeleteCurrentPolygon;
    if(UI->NkButtonLabel(Nk, "Triangulate"))
        MapEditor->CurrentAction = MEAction_TriangulateAll;

    UI->NkLayoutRowStatic(Nk, 30, 260, 1);
    Rect = UI->NkWidgetBounds(Nk);
    UI->NkFillRect(&Nk->current->buffer, Rect, 10.0f, ColorTable[1]);
    UI->NkLabelf(Nk, NK_TEXT_CENTERED, "Current Polygon: %d", MapEditor->CurrentPolygonIndex);

    UI->NkLayoutRowStatic(Nk, 30, 260, 1);
    UI->NkPropertyInt(Nk, "Choose Poly: ", 0,
                      (int *)&MapEditor->CurrentPolygonIndex,
                      MapEditor->NavMesh.PolygonCount - 1, 1, 0.1f);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);
    UI->NkLayoutSpacePush(Nk, {0, 805, 160, 40});
    if(UI->NkButtonLabel(Nk, "Exit"))
        MapEditor->CurrentAction = MEAction_Exit;

    UI->NkLayoutSpacePush(Nk, {165, 805, 160, 40});
    if(UI->NkButtonLabel(Nk, "Write Polygons"))
        MapEditor->CurrentAction = MEAction_WritePolygons;
    UI->NkLayoutSpaceEnd(Nk);

    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 0, INT_MAX);
    {
        UI->NkLayoutSpacePush(Nk, UI->NkRect(1580, -240, 320, 200));
        struct nk_rect Bounds = UI->NkWidgetBounds(Nk);
        Platform.UI.NkFillRect(&Nk->current->buffer, Bounds, 5.0f, ColorTable[3]);

        if(UI->NkGroupBegin(Nk, "Mesh View", NK_WINDOW_BORDER))
        {
            if(NkTreePush(Platform.UI, Nk, NK_TREE_NODE, "Mesh View Options", NK_MINIMIZED))
            {
                UI->NkCheckboxLabel(Nk, "Show Native Polies", &MapEditor->ShowNativePolies);

                if(MapEditor->ShowNativePolies)
                    UI->NkCheckboxLabel(Nk, "Show Native P IDs", &MapEditor->ShowNativeIds);

                if(MapEditor->NavMesh.Partitioned)
                {
                    UI->NkCheckboxLabel(Nk, "Show Partition", &MapEditor->ShowPartition);

                    if(MapEditor->ShowPartition)
                    {
                        UI->NkCheckboxLabel(Nk, "Show Color", &MapEditor->ShowColor);
                        UI->NkCheckboxLabel(Nk, "Show Neighbours", &MapEditor->ShowNeighbours);
                    }
                }
                    
                UI->NkTreePop(Nk);
            }

            UI->NkLabel(Nk, "StartNode: ", NK_TEXT_ALIGN_LEFT);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "  TileX/Y: (%d, %d)",
                         MapEditor->StartNode.TileX, MapEditor->StartNode.TileY);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "  Offset.x/y: (%.2f, %.2f)",
                         MapEditor->StartNode.Offset.x, MapEditor->StartNode.Offset.y);

            UI->NkLabel(Nk, "EndNode: ", NK_TEXT_ALIGN_LEFT);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "  TileX/Y: (%d, %d)",
                         MapEditor->EndNode.TileX, MapEditor->EndNode.TileY);
            UI->NkLabelf(Nk, NK_TEXT_ALIGN_LEFT, "  Offset.x/y: (%.2f, %.2f)",
                         MapEditor->EndNode.Offset.x, MapEditor->EndNode.Offset.y);

            UI->NkGroupEnd(Nk);
        }
    }
    UI->NkLayoutSpaceEnd(Nk);
}

inline void
DrawEntityModeUI(engine_map_editor *MapEditor, nk_ui *UI, nk_context *Nk)
{
    UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);
    UI->NkLayoutSpacePush(Nk, {0, 935, 160, 40});
    if(UI->NkButtonLabel(Nk, "Exit"))
        MapEditor->CurrentAction = MEAction_Exit;
    UI->NkLayoutSpaceEnd(Nk);
}

internal void
DrawMapEditorUI(engine_map_editor *MapEditor, editor_assets *Assets, u32 GenerationID, ui_state *UIState)
{
    nk_ui *UI = UIState->UI;
    nk_context *Nk = UIState->Nk;

    if(!MapEditor->HideUI)
    {
        UI->NkLayoutRowStatic(Nk, 30, 260, 2);

        char *ModeString = JsonGetEnumString(UIState->JsonStringsHead, "MapEditorMode", MapEditor->MapEditorMode);
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
                            (IsSetMapEditorFlag(MapEditor, MEFlag_EditEnable) ? Green : Red),
                            "Edit Enable: %s", IsSetMapEditorFlag(MapEditor, MEFlag_EditEnable) ? "true" : "false");

        UI->NkLayoutRowStatic(Nk, 20, 120, 1);
        UI->NkCheckboxLabel(Nk, "Show Grid", &MapEditor->ShowGrid);
        switch(MapEditor->MapEditorMode)
        {
            case MapEditorMode_None:
            {
                UI->NkLayoutRowStatic(Nk, 30, 260, 1);
                UI->NkPropertyInt(Nk, "Ground Layer: ", 0,
                                  (int *)&MapEditor->MapGroundLayer, 15, 1, 0.1f);
                UI->NkPropertyInt(Nk, "Layer Count: ", 0,
                                  (int *)&MapEditor->LayerCount, 15, 1, 0.1f);

                UI->NkLayoutSpaceBegin(Nk, NK_STATIC, 20, 2);

                UI->NkLayoutSpacePush(Nk, {0, 870, 130, 40});
                if(UI->NkButtonLabel(Nk, "Exit"))
                    MapEditor->CurrentAction = MEAction_Exit;

                UI->NkLayoutSpacePush(Nk, {135, 870, 130, 40});
                if(UI->NkButtonLabel(Nk, "Write SSWM"))
                    MapEditor->CurrentAction = MEAction_WriteSSWM;

                UI->NkLayoutSpaceEnd(Nk);
            } break;

            case MapEditorMode_Terrain:
            {
                DrawTerrainModeUI(MapEditor, Assets, GenerationID, UI, Nk, UIState->MouseZ);
            
            } break;

            case MapEditorMode_NavMeshes:
            {
                DrawNavMeshModeUI(MapEditor, UI, Nk);
            } break;

            case MapEditorMode_Entity:
            {
                DrawEntityModeUI(MapEditor, UI, Nk);
            } break;
        }

    }
}

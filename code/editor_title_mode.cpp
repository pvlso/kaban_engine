/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

internal void
PlayTitleScreen(editor_state *EditorState, transient_state *TranState)
{
    SetEditorMode(EditorState, TranState, EditorMode_TitleScreen);
    
    editor_mode_title_screen *Result = PushStruct(&EditorState->ModeArena, editor_mode_title_screen);
    Result->ModeToPlay = EditorMode_None;

    EditorState->TitleScreen = Result;
}

internal b32
UpdateAndRenderTitleScreen(editor_state *EditorState, transient_state *TranState)
{
    editor_mode_title_screen *TitleScreen = EditorState->TitleScreen;
    b32 Result = false;//CheckForMetaInput(EditorState, TranState, Input);

    ui_state *UIState = &EditorState->UIState;
    nk_context *Nk = UIState->Nk;
    if(!Result)
    {
        char Buffer[256];
        nk_layout_row_begin(Nk, NK_STATIC, 40, 8);
        {
            nk_layout_row_push(Nk, 130);
            if(nk_button_label(Nk, "Assets Mode"))
            {
                PlayAssetsMode(EditorState, TranState);
                Result = true;
                return(Result);
            }

            if(nk_button_label(Nk, "Play Game"))
            {
                return(Result);
            }

            if(nk_button_label(Nk, "Simulate"))
            {
//                PlaySimulation(EditorState, TranState);
                Result = true;
                return(Result);
            }
            if(nk_button_label(Nk, "PlaceHolder")) {}
            if(nk_button_label(Nk, "PlaceHolder")) {}
            if(nk_button_label(Nk, "PlaceHolder")) {}
            if(nk_button_label(Nk, "PlaceHolder")) {}
            if(nk_button_label(Nk, "PlaceHolder")) {}
        }
        nk_layout_row_end(Nk);

        nk_layout_row_dynamic(Nk, 40, 1);
        nk_spacer(Nk);

        editor_meta EditorMeta = EditorState->EditorMeta;
        FormatString(ArrayCount(Buffer), Buffer, "Stored Assets Version: %d.%d.%d.%d",
                     EditorMeta.KESAVersion[0], EditorMeta.KESAVersion[1],
                     EditorMeta.KESAVersion[2], EditorMeta.KESAVersion[3]);
        nk_label(Nk, Buffer, NK_TEXT_ALIGN_LEFT);
        nk_spacer(Nk);

#if 0        
        if(EditorState->MapStartup.NewMap)
        {
            UI->NkCheckboxLabel(Nk, "Create New Map", &EditorState->MapStartup.NewMap);
        }
        else
        {
            UI->NkCheckboxLabel(Nk, "Load Map", &EditorState->MapStartup.NewMap);
        }

        UI->NkLayoutRowBegin(Nk, NK_STATIC, 40, 1);
        {
            UI->NkLayoutRowPush(Nk, 320);
            if(!EditorState->MapStartup.NewMap)
            {
                UI->NkPropertyU8(Nk, "#Map Major High Version: ", 0,
                                 &EditorState->MapStartup.MapVersion.MajorHigh,
                                 EditorState->Version.MajorHigh, 1, 0.1f);
                UI->NkPropertyU8(Nk, "#Map Major Low Version: ", 0,
                                 &EditorState->MapStartup.MapVersion.MajorLow,
                                 EditorState->Version.MajorLow, 1, 0.1f);
                UI->NkPropertyU8(Nk, "#Map Minor High Version: ", 0,
                                 &EditorState->MapStartup.MapVersion.MinorHigh,
                                 EditorState->Version.MinorHigh, 1, 0.1f);
                UI->NkPropertyU8(Nk, "#Map Minor Low Version: ", 0,
                                 &EditorState->MapStartup.MapVersion.MinorLow,
                                 EditorState->Version.MinorLow, 1, 0.1f);
            }

            UI->NkPropertyInt(Nk, "#Map Width: ", 16, &EditorState->MapStartup.MapWidth, 512, 1, 0.1f);
            UI->NkPropertyInt(Nk, "#Map Height: ", 16, &EditorState->MapStartup.MapHeight, 512, 1, 0.1f);
            UI->NkPropertyInt(Nk, "#Map ID: ", 0, (int *)&EditorState->MapStartup.MapID, 255, 1, 0.1f);
        }
        UI->NkLayoutRowEnd(Nk);
#endif
    }
    
    return(Result);
}

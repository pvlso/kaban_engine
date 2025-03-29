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
UpdateAndRenderTitleScreen(editor_state *EditorState, transient_state *TranState, nk_context *Nk, nk_ui *UI,
                           editor_mode_title_screen *TitleScreen)
{
    editor_assets *Assets = TranState->Assets;
    b32 Result = false;//CheckForMetaInput(EditorState, TranState, Input);

    if(!Result)
    {
        char Buffer[256];
        UI->NkLayoutRowBegin(Nk, NK_STATIC, 30, 8);
        {
            UI->NkLayoutRowPush(Nk, 80);
            if(UI->NkButtonLabel(Nk, "Assets Mode"))
            {
                PlayAssetsMode(EditorState, TranState);
                Result = true;
                return(Result);
            }

            if(UI->NkButtonLabel(Nk, "Game Mode"))
            {
                PlayGameMode(EditorState, TranState);
                Result = true;
                return(Result);
            }

            if(UI->NkButtonLabel(Nk, "PlaceHolder")) {}
            if(UI->NkButtonLabel(Nk, "PlaceHolder")) {}
            if(UI->NkButtonLabel(Nk, "PlaceHolder")) {}
            if(UI->NkButtonLabel(Nk, "PlaceHolder")) {}
            if(UI->NkButtonLabel(Nk, "PlaceHolder")) {}
            if(UI->NkButtonLabel(Nk, "PlaceHolder")) {}
        }
        UI->NkLayoutRowEnd(Nk);

        UI->NkLayoutRowDynamic(Nk, 30, 1);
        UI->NkSpacer(Nk);
        UI->NkLabel(Nk, "Stored Assets Version: ", NK_TEXT_ALIGN_LEFT);

        FormatString(ArrayCount(Buffer), Buffer, "Major High Version: %d", EditorState->Version.MajorHigh);
        UI->NkLabel(Nk, Buffer, NK_TEXT_ALIGN_LEFT);

        FormatString(ArrayCount(Buffer), Buffer, "Major Low Version: %d", EditorState->Version.MajorLow);
        UI->NkLabel(Nk, Buffer, NK_TEXT_ALIGN_LEFT);

        FormatString(ArrayCount(Buffer), Buffer, "Minor High Version: %d", EditorState->Version.MinorHigh);
        UI->NkLabel(Nk, Buffer, NK_TEXT_ALIGN_LEFT);

        FormatString(ArrayCount(Buffer), Buffer, "Minor Low Version: %d", EditorState->Version.MinorLow);
        UI->NkLabel(Nk, Buffer, NK_TEXT_ALIGN_LEFT);
        UI->NkSpacer(Nk);

        
        if(EditorState->MapStartup.NewMap)
        {
            UI->NkCheckboxLabel(Nk, "Create New Map", &EditorState->MapStartup.NewMap);
        }
        else
        {
            UI->NkCheckboxLabel(Nk, "Load Map", &EditorState->MapStartup.NewMap);
        }

        UI->NkLayoutRowBegin(Nk, NK_STATIC, 30, 1);
        {
            UI->NkLayoutRowPush(Nk, 240);
            if(!EditorState->MapStartup.NewMap)
            {
                UI->NkPropertyInt(Nk, "#Map Major High Version: ", 0,
                                  (int *)&EditorState->MapStartup.MapVersion.MajorHigh,
                                  EditorState->Version.MajorHigh, 1, 0.1f);
                UI->NkPropertyInt(Nk, "#Map Major Low Version: ", 0,
                                  (int *)&EditorState->MapStartup.MapVersion.MajorLow,
                                  EditorState->Version.MajorLow, 1, 0.1f);
                UI->NkPropertyInt(Nk, "#Map Minor High Version: ", 0,
                                  (int *)&EditorState->MapStartup.MapVersion.MinorHigh,
                                  EditorState->Version.MinorHigh, 1, 0.1f);
                UI->NkPropertyInt(Nk, "#Map Minor Low Version: ", 0,
                                  (int *)&EditorState->MapStartup.MapVersion.MinorLow,
                                  EditorState->Version.MinorLow, 1, 0.1f);
            }

            UI->NkPropertyInt(Nk, "#Map Width: ", 16, &EditorState->MapStartup.MapWidth, 512, 1, 0.1f);
            UI->NkPropertyInt(Nk, "#Map Height: ", 16, &EditorState->MapStartup.MapHeight, 512, 1, 0.1f);
            UI->NkPropertyInt(Nk, "#Map ID: ", 0, (int *)&EditorState->MapStartup.MapID, 255, 1, 0.1f);
        }
        UI->NkLayoutRowEnd(Nk);

        
        switch(TitleScreen->ModeToPlay)
        {
            case EditorMode_None:
            {
            } break;

            case EditorMode_AssetsMode:
            {
            } break;

            case EditorMode_GameMode:
            {
            } break;
        }
    }

    return(Result);
}

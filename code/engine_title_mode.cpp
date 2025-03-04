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
UpdateAndRenderTitleScreen(editor_state *EditorState, transient_state *TranState, render_group *RenderGroup,
                           editor_input *Input, u32 RenderWidth, u32 RenderHeight,
                           editor_mode_title_screen *TitleScreen)
{
    editor_assets *Assets = TranState->Assets;
    b32 Result = CheckForMetaInput(EditorState, TranState, Input);
    if(!Result)
    {
        Orthographic(RenderGroup, 1.0f);

        Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 1.0f));

        object_transform Flat = DefaultFlatTransform();
//        PushRect(RenderGroup, &Flat, V3(0, 0, 0), V2(120.0f, 120.0f), V4(1.0f, 0, 0, 1.0f));

        switch(TitleScreen->ModeToPlay)
        {
            case EditorMode_None:
            {
            } break;

            case EditorMode_AssetsMode:
            {
                PlayAssetsMode(EditorState, TranState);
                Result = true;
            } break;

            case EditorMode_GameMode:
            {
                PlayGameMode(EditorState, TranState);
                Result = true;
            } break;
        }
    }

    return(Result);
}

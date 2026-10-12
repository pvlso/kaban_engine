/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

// NOTE(pvlso): Every refresh pushes a new list into the mode arena, it's only freed when the mode changes.
// Refreshes are rare (entering the title screen, creating a project, the refresh button) so that's fine.
internal void
RefreshProjectList(editor_state *EditorState, editor_mode_title_screen *TitleScreen)
{
    TitleScreen->ProjectCount = 0;
    TitleScreen->SelectedProject = -1;

    u32 MaxCount = Platform.ListProjects(0, 0, 0);
    if(MaxCount)
    {
        TitleScreen->ProjectNames = PushArray(&EditorState->ModeArena, MaxCount, char *);
        TitleScreen->ProjectCount = Platform.ListProjects(TitleScreen->ProjectNames, MaxCount, &EditorState->ModeArena);
    }
}

// NOTE(pvlso): The name is used for the directory, the files and the include guard,
// so it has to be a valid C identifier
internal b32
IsValidProjectName(char *Name)
{
    b32 Result = (Name[0] != 0) && !IsDigit(Name[0]);
    for(char *C = Name; *C; ++C)
    {
        b32 IsLetter = (((*C >= 'a') && (*C <= 'z')) ||
                        ((*C >= 'A') && (*C <= 'Z')));
        if(!IsLetter && !IsDigit(*C) && (*C != '_'))
        {
            Result = false;
        }
    }

    return(Result);
}

// NOTE(pvlso): The template is a header with the game_state and a unity build .cpp with the
// GameUpdateAndRender export (see engine_api.h)
internal b32
CreateProject(char *Name, char *Status, umm StatusCount)
{
    b32 Result = false;

    char UpperName[PROJECT_NAME_COUNT];
    umm NameLength = StringLength(Name);
    for(umm CharIndex = 0; CharIndex <= NameLength; ++CharIndex)
    {
        char C = Name[CharIndex];
        UpperName[CharIndex] = ((C >= 'a') && (C <= 'z')) ? (C - 'a' + 'A') : C;
    }

    char FileName[256];
    char Contents[4096];

    if(!IsValidProjectName(Name))
    {
        FormatString(StatusCount, Status, "\"%s\" is not a valid name, use letters, digits and '_'", Name);
    }
    else if(!Platform.MakeDirectory(Name, PlatformFileType_Project))
    {
        FormatString(StatusCount, Status, "Could not create projects/%s, does it already exist?", Name);
    }
    else
    {
        FormatString(sizeof(FileName), FileName, "%s/%s.h", Name, Name);
        umm HeaderSize = FormatString(sizeof(Contents), Contents,
                                      "#if !defined(%s_H)\n"
                                      "\n"
                                      "#include \"engine_api.h\"\n"
                                      "\n"
                                      "// NOTE: Lives in game_memory, which the engine owns, so it survives hot reload\n"
                                      "struct game_state\n"
                                      "{\n"
                                      "    v2 P;\n"
                                      "};\n"
                                      "\n"
                                      "#define %s_H\n"
                                      "#endif\n",
                                      UpperName, UpperName);
        b32 HeaderWritten = (Platform.WriteEntireFile(FileName, PlatformFileType_Project,
                                                      (u8 *)Contents, (u32)HeaderSize) == HeaderSize);

        FormatString(sizeof(FileName), FileName, "%s/%s.cpp", Name, Name);
        umm SourceSize = FormatString(sizeof(Contents), Contents,
                                      "#include \"%s.h\"\n"
                                      "\n"
                                      "// NOTE: Unity build, include the rest of the game's .cpp files here\n"
                                      "\n"
                                      "platform_api Platform;\n"
                                      "engine_api Engine;\n"
                                      "\n"
                                      "GAME_EXPORT GAME_UPDATE_AND_RENDER(GameUpdateAndRender)\n"
                                      "{\n"
                                      "    // NOTE: The engine can be reloaded too, so the APIs are set every frame\n"
                                      "    Platform = Memory->PlatformAPI;\n"
                                      "    Engine = Memory->EngineAPI;\n"
                                      "\n"
                                      "    game_state *GameState = (game_state *)Memory->GameState;\n"
                                      "    if(!GameState)\n"
                                      "    {\n"
                                      "        GameState = PushStruct(&Memory->PermanentArena, game_state);\n"
                                      "        Memory->GameState = GameState;\n"
                                      "    }\n"
                                      "\n"
                                      "    engine_controller_input *Controller = GetController(Input, 0);\n"
                                      "    v2 dP = {};\n"
                                      "    if(Controller->MoveUp.EndedDown)    {dP.y += 1.0f;}\n"
                                      "    if(Controller->MoveDown.EndedDown)  {dP.y -= 1.0f;}\n"
                                      "    if(Controller->MoveLeft.EndedDown)  {dP.x -= 1.0f;}\n"
                                      "    if(Controller->MoveRight.EndedDown) {dP.x += 1.0f;}\n"
                                      "    GameState->P += 5.0f*Input->dtForFrame*dP;\n"
                                      "\n"
                                      "    render_group Group = BeginRenderGroup(Assets, RenderCommands);\n"
                                      "    Orthographic(&Group, 50.0f);\n"
                                      "    Clear(&Group, V4(0.1f, 0.1f, 0.15f, 1.0f));\n"
                                      "\n"
                                      "    object_transform Transform = DefaultFlatTransform();\n"
                                      "    PushRect(&Group, &Transform, V3(GameState->P, 0.0f), V2(1.0f, 1.0f), V4(1.0f, 0.5f, 0.0f, 1.0f));\n"
                                      "\n"
                                      "    EndRenderGroup(&Group);\n"
                                      "}\n",
                                      Name);
        b32 SourceWritten = (Platform.WriteEntireFile(FileName, PlatformFileType_Project,
                                                      (u8 *)Contents, (u32)SourceSize) == SourceSize);

        Result = (HeaderWritten && SourceWritten);
        if(Result)
        {
            FormatString(StatusCount, Status, "Created projects/%s", Name);
        }
        else
        {
            FormatString(StatusCount, Status, "Could not write the files of projects/%s", Name);
        }
    }

    return(Result);
}

internal void
PlayTitleScreen(editor_state *EditorState, transient_state *TranState)
{
    SetEditorMode(EditorState, TranState, EditorMode_TitleScreen);
    UseDataOf(EditorState, 0);

    editor_mode_title_screen *Result = PushStruct(&EditorState->ModeArena, editor_mode_title_screen);
    Result->ModeToPlay = EditorMode_None;
    RefreshProjectList(EditorState, Result);

    EditorState->TitleScreen = Result;
}

// NOTE(pvlso): True when the selected project started running, the title screen is gone then
internal b32
UpdateAndRenderProjects(editor_state *EditorState, transient_state *TranState,
                        editor_mode_title_screen *TitleScreen, nk_context *Nk)
{
    b32 Result = false;

    nk_layout_row_static(Nk, 400, 600, 1);
    if(nk_group_begin(Nk, "Projects", NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(Nk, 30, 1);
        if(TitleScreen->ProjectCount == 0)
        {
            nk_label(Nk, "No projects yet", NK_TEXT_ALIGN_LEFT);
        }

        for(u32 ProjectIndex = 0; ProjectIndex < TitleScreen->ProjectCount; ++ProjectIndex)
        {
            nk_bool Selected = (TitleScreen->SelectedProject == (s32)ProjectIndex);
            if(nk_selectable_label(Nk, TitleScreen->ProjectNames[ProjectIndex], NK_TEXT_ALIGN_LEFT, &Selected))
            {
                TitleScreen->SelectedProject = Selected ? (s32)ProjectIndex : -1;
            }
        }

        nk_layout_row_dynamic(Nk, 30, 2);
        if(nk_button_label(Nk, "Refresh"))
        {
            RefreshProjectList(EditorState, TitleScreen);
        }

        // NOTE(pvlso): The game mode builds the project, then runs it
        b32 CanRun = (TitleScreen->SelectedProject >= 0);
        if(!CanRun) nk_widget_disable_begin(Nk);
        b32 RunPressed = nk_button_label(Nk, "Run");
        if(!CanRun) nk_widget_disable_end(Nk);

        if(RunPressed && CanRun)
        {
            PlayGame(EditorState, TranState, TitleScreen->ProjectNames[TitleScreen->SelectedProject]);
            Result = true;
        }

        // NOTE(pvlso): Once the game runs, the mode arena that held TitleScreen is gone
        if(!Result)
        {
            nk_layout_row_dynamic(Nk, 30, 2);
            nk_edit_string_zero_terminated(Nk, NK_EDIT_FIELD, TitleScreen->NewProjectName,
                                           sizeof(TitleScreen->NewProjectName), nk_filter_ascii);
            if(nk_button_label(Nk, "New Project"))
            {
                if(CreateProject(TitleScreen->NewProjectName, TitleScreen->ProjectStatus,
                                 sizeof(TitleScreen->ProjectStatus)))
                {
                    TitleScreen->NewProjectName[0] = 0;
                    RefreshProjectList(EditorState, TitleScreen);
                }
            }

            if(TitleScreen->ProjectStatus[0])
            {
                nk_layout_row_dynamic(Nk, 30, 1);
                nk_label(Nk, TitleScreen->ProjectStatus, NK_TEXT_ALIGN_LEFT);
            }
        }

        nk_group_end(Nk);
    }

    return(Result);
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
        nk_layout_row_begin(Nk, NK_STATIC, 40, 7);
        {
            nk_layout_row_push(Nk, 130);

            // NOTE(pvlso): Edits the data of the project selected in the Projects list
            b32 HasProject = (TitleScreen->SelectedProject >= 0);
            if(!HasProject) nk_widget_disable_begin(Nk);
            b32 AssetsPressed = nk_button_label(Nk, "Assets Mode");
            if(!HasProject) nk_widget_disable_end(Nk);

            if(AssetsPressed && HasProject)
            {
                PlayAssetsMode(EditorState, TranState, TitleScreen->ProjectNames[TitleScreen->SelectedProject]);
                Result = true;
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

        if(UpdateAndRenderProjects(EditorState, TranState, TitleScreen, Nk))
        {
            Result = true;
            return(Result);
        }

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

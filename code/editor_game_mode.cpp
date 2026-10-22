/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice:  $
   ======================================================================== */

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): ENGINE API, the engine side of the table the games call through (engine_api.h)
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal render_group
GameBeginRenderGroup(engine_assets *Assets, editor_render_commands *Commands)
{
    u32 GenerationID = Assets ? BeginGeneration(Assets) : 0;
    render_group Result = BeginRenderGroup(Assets, Commands, GenerationID, false,
                                           Commands->Width, Commands->Height);
    return(Result);
}

internal void
GameEndRenderGroup(render_group *Group)
{
    EndRenderGroup(Group);
    if(Group->Assets)
    {
        EndGeneration(Group->Assets, Group->GenerationID);
    }
}

internal void
GamePushBitmap(render_group *Group, object_transform *ObjectTransform, bitmap_id ID, r32 Height, v3 Offset,
               v4 Color, r32 CAlign, r32 Angle)
{
    if(Group->Assets)
    {
        PushBitmap(Group, ObjectTransform, ID, Height, Offset, Color, CAlign, Angle);
    }
}

internal asset_best_match_result
GameGetBestMatchAssets(engine_assets *Assets, asset_tag_vector *Vector, memory_arena *Arena)
{
    asset_best_match_result Result = {};
    if(Assets)
    {
        Result = GetBestMatchAssets(Assets, Vector, Arena);
    }

    return(Result);
}

// NOTE(pvlso): The sheet only holds its sprites' GUIDs, so it's loaded right away the first time
internal bitmap_id
GameGetSprite(engine_assets *Assets, u32 SheetIndex, u32 SpriteIndex)
{
    bitmap_id Result = {};
    if(Assets && SheetIndex && (SheetIndex < Assets->AssetCount))
    {
        asset *Asset = Assets->Assets + SheetIndex;
        if((Asset->KEA.Type == KEAType_SpriteSheet) && (SpriteIndex < Asset->KEA.SpriteSheet.SpriteCount))
        {
            spritesheet_id SheetID = {SheetIndex};
            loaded_spritesheet *Sheet = GetSpriteSheet(Assets, SheetID, 0);
            if(!Sheet)
            {
                LoadSpriteSheet(Assets, SheetID, true);
                Sheet = GetSpriteSheet(Assets, SheetID, 0);
            }

            if(Sheet)
            {
                Result = Sheet->SpriteGUIDs[SpriteIndex];
            }
        }
    }

    return(Result);
}

// NOTE(pvlso): Built every frame, the function addresses change when the engine is reloaded
internal engine_api
GetEngineAPI(void)
{
    engine_api Result = {};

    Result.BeginRenderGroup = GameBeginRenderGroup;
    Result.EndRenderGroup = GameEndRenderGroup;
    Result.Perspective = Perspective;
    Result.Orthographic = Orthographic;
    Result.Clear = Clear;
    Result.BeginPixelWorld = BeginPixelWorld;
    Result.EndPixelWorld = EndPixelWorld;
    Result.SetPixelWorldScroll = SetPixelWorldScroll;

    Result.PushBitmap = GamePushBitmap;
    Result.PushRect = PushRect;
    Result.PushLine = PushLine;
    Result.PushCircleOutline = PushCircleOutline;

    Result.GetFont = GameGetFont;
    Result.PushText = GamePushText;
    Result.GetTextDim = GameGetTextDim;

    Result.GetBestMatchAssets = GameGetBestMatchAssets;
    Result.GetSprite = GameGetSprite;

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): GAME MODE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// NOTE(pvlso): "game_data_<version>.kea" -> version, false for any other name
internal b32
ParseGameDataKEAVersion(char *Name, u32 *Version)
{
    b32 Result = false;

    char *Prefix = "game_data_";
    umm PrefixLength = StringLength(Prefix);
    if(StringsAreEqual(PrefixLength, Name, Prefix) && IsDigit(Name[PrefixLength]))
    {
        u64 Value = 0;
        char *At = Name + PrefixLength;
        while(IsDigit(*At) && (Value <= U32Maximum))
        {
            Value = 10*Value + (*At++ - '0');
        }

        if((Value <= U32Maximum) && StringsAreEqual(At, ".kea"))
        {
            *Version = (u32)Value;
            Result = true;
        }
    }

    return(Result);
}

// NOTE(pvlso): The newest game_data_<version>.kea in the project's data/keas (the data root is the
// project's while it runs), the assets mode writes those. Its name goes to FileName, empty when there's none.
internal engine_assets *
LoadGameAssets(editor_state *EditorState, transient_state *TranState, char *FileName, umm FileNameCount)
{
    TIMED_FUNCTION();

    engine_assets *Result = 0;
    FileName[0] = 0;

    temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
    u32 FileCount = Platform.ListFilesInDirectory(PlatformFileType_KEA, 0, 0);
    if(FileCount)
    {
        char **FileNames = PushArray(TempMem.Arena, FileCount, char *);
        FileCount = Platform.ListFilesInDirectory(PlatformFileType_KEA, FileNames, TempMem.Arena);

        char *BestFileName = 0;
        u32 BestVersion = 0;
        for(u32 FileIndex = 0; FileIndex < FileCount; ++FileIndex)
        {
            u32 Version = 0;
            if(ParseGameDataKEAVersion(FileNames[FileIndex], &Version) &&
               (!BestFileName || (Version > BestVersion)))
            {
                BestFileName = FileNames[FileIndex];
                BestVersion = Version;
            }
        }

        if(BestFileName)
        {
            Result = AllocateAssets(BestFileName, &EditorState->ModeArena, TempMem.Arena,
                                    TranState->LowPriorityQueue, TranState->TextureOpQueue, Megabytes(128));
            FormatString(FileNameCount, FileName, "%s", BestFileName);
        }
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}

#define GAME_SOURCE_CHECK_INTERVAL 0.5f

internal void
StartGameBuild(editor_mode_game *Game)
{
    Game->BuiltSourceWriteTime = Platform.GetProjectWriteTime(Game->ProjectName);
    Game->BuildProcess = Platform.BuildProject(Game->ProjectName);
    Game->Building = true;
    ++Game->BuildCount;

    FormatString(sizeof(Game->Status), Game->Status, "Building %s...", Game->ProjectName);
    LOG("Building %s", Game->ProjectName);
}

// NOTE(pvlso): The new library is loaded before the old one goes, so a library that doesn't
// load leaves the old code running
internal b32
LoadGameCode(editor_mode_game *Game)
{
    b32 Result = false;

    char ModuleName[256];
    FormatString(sizeof(ModuleName), ModuleName, "%s/build/%s", Game->ProjectName, Game->ProjectName);

    platform_loaded_code Code = Platform.LoadCode(ModuleName, PlatformFileType_Project);
    game_update_and_render *UpdateAndRender = 0;
    if(Code.NoErrors)
    {
        UpdateAndRender = (game_update_and_render *)Platform.GetProcAddress(&Code, "GameUpdateAndRender");
    }

    if(UpdateAndRender)
    {
        if(Game->UpdateAndRender)
        {
            Platform.UnloadCode(&Game->Code);
        }

        Game->Code = Code;
        Game->UpdateAndRender = UpdateAndRender;
        ++Game->LoadCount;
        Result = true;
    }
    else
    {
        Platform.UnloadCode(&Code);
    }

    return(Result);
}

internal void
ReadGameBuildLog(editor_mode_game *Game, transient_state *TranState)
{
    Game->BuildLog[0] = 0;

    char FileName[256];
    FormatString(sizeof(FileName), FileName, "%s/build/build.log", Game->ProjectName);

    temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
    read_file_result File = Platform.ReadEntireFile(FileName, PlatformFileType_Project, TempMem.Arena, true);
    if(File.Contents)
    {
        FormatString(sizeof(Game->BuildLog), Game->BuildLog, "%s", (char *)File.Contents);
    }
    EndTemporaryMemory(TempMem);
}

internal void
UpdateGameBuild(editor_mode_game *Game, transient_state *TranState, f32 dt)
{
    if(Game->Building)
    {
        platform_process_state State = Platform.GetProcessState(&Game->BuildProcess);
        if(!State.IsRunning)
        {
            Game->Building = false;
            ReadGameBuildLog(Game, TranState);

            Game->BuildFailed = true;
            if(!State.StartedSuccessfully)
            {
                FormatString(sizeof(Game->Status), Game->Status, "Could not start the build of %s", Game->ProjectName);
                LOG_ERROR("Could not start the build of %s", Game->ProjectName);
            }
            else if(State.ReturnCode != 0)
            {
                FormatString(sizeof(Game->Status), Game->Status, "%s: build failed%s", Game->ProjectName,
                             Game->UpdateAndRender ? ", the last good build keeps running" : "");
                LOG_ERROR("%s: build failed (exit code %d)", Game->ProjectName, State.ReturnCode);
            }
            else if(!LoadGameCode(Game))
            {
                FormatString(sizeof(Game->Status), Game->Status,
                             "%s built, but it doesn't load or has no GameUpdateAndRender export", Game->ProjectName);
                LOG_ERROR("%s built, but it doesn't load or has no GameUpdateAndRender export", Game->ProjectName);
            }
            else
            {
                Game->BuildFailed = false;
                FormatString(sizeof(Game->Status), Game->Status, "Running %s", Game->ProjectName);
                LOG("%s %s", Game->ProjectName, (Game->LoadCount > 1) ? "reloaded" : "loaded");
            }
        }
    }

    // NOTE(pvlso): A save during a build is caught here once that build is done
    Game->SourceCheckTimer -= dt;
    if(!Game->Building && (Game->SourceCheckTimer <= 0.0f))
    {
        Game->SourceCheckTimer = GAME_SOURCE_CHECK_INTERVAL;
        if(Platform.GetProjectWriteTime(Game->ProjectName) != Game->BuiltSourceWriteTime)
        {
            StartGameBuild(Game);
        }
    }
}

internal void
PlayGame(editor_state *EditorState, transient_state *TranState, char *ProjectName)
{
    // NOTE(pvlso): ProjectName lives in the mode arena, which SetEditorMode frees
    char Name[PROJECT_NAME_COUNT];
    FormatString(sizeof(Name), Name, "%s", ProjectName);

    SetEditorMode(EditorState, TranState, EditorMode_Game);

    // NOTE(pvlso): The game's assets come from its own data/
    UseDataOf(EditorState, Name);

    editor_mode_game *Game = PushStruct(&EditorState->ModeArena, editor_mode_game);
    FormatString(sizeof(Game->ProjectName), Game->ProjectName, "%s", Name);
    Game->Assets = LoadGameAssets(EditorState, TranState, Game->AssetFileName, sizeof(Game->AssetFileName));
    EditorState->Game = Game;

    LOG("Run %s", Game->ProjectName);
    if(Game->Assets)
    {
        LOG("%s assets: %s", Game->ProjectName, Game->AssetFileName);
    }
    else
    {
        LOG_WARNING("%s has no game_data_<version>.kea in its data/keas", Game->ProjectName);
    }

    // NOTE(pvlso): Always builds first, so the library matches the sources and the editor's mode
    StartGameBuild(Game);
}

internal void
ClearGameMemory(editor_mode_game *Game)
{
    Clear(&Game->Memory.PermanentArena);
    Clear(&Game->Memory.TransientArena);
    Game->Memory.GameState = 0;
}

// NOTE(pvlso): Queues a delete for the texture of every loaded bitmap and closes the KEA. The
// asset memory itself is in the mode arena, which the next mode frees.
internal void
FreeGameAssets(engine_assets *Assets)
{
    TIMED_FUNCTION();

    u32 TextureCount = 0;
    if(Assets->Assets)
    {
        for(u32 AssetIndex = 0; AssetIndex < Assets->AssetCount; ++AssetIndex)
        {
            asset *Asset = Assets->Assets + AssetIndex;
            if((Asset->State == AssetState_Loaded) && Asset->Header &&
               (Asset->Header->AssetType == AssetType_Bitmap) && Asset->Header->Bitmap.TextureHandle)
            {
                texture_op Op = {};
                Op.IsAllocate = false;
                Op.Deallocate.Handle = Asset->Header->Bitmap.TextureHandle;
                AddOp(Assets->TextureOpQueue, &Op);
                ++TextureCount;
            }
        }
    }

    Platform.CloseFile(&Assets->File.Handle);
    LOG("Freed %u texture(s) of the game's assets", TextureCount);
}

// NOTE(pvlso): Stop takes two frames. The first one lets the in-flight asset loads finish
// and doesn't run the game. Their texture uploads are queued, and the platform runs that
// queue at the end of the frame, writing the handles into the asset memory. The second
// frame (FinishStopGame) can then delete every texture and free the mode arena.
internal void
StopGame(editor_state *EditorState, transient_state *TranState)
{
    editor_mode_game *Game = EditorState->Game;

    // NOTE(pvlso): A build left running would race the next Run's build for the same files.
    // Stopping mid build is rare, so just wait for it.
    b32 Waited = false;
    while(Game->Building && Platform.GetProcessState(&Game->BuildProcess).IsRunning)
    {
        if(!Waited)
        {
            LOG_WARNING("Stop: waiting for the running build of %s", Game->ProjectName);
            Waited = true;
        }
        Platform.Sleep(10);
    }

    // NOTE(pvlso): Asset loads still in flight write into the mode arena
    Platform.CompleteAllWork(TranState->LowPriorityQueue);

    ClearGameMemory(Game);
    if(Game->UpdateAndRender)
    {
        Platform.UnloadCode(&Game->Code);
        Game->UpdateAndRender = 0;
    }

    Game->Stopping = true;
    LOG("Stop %s", Game->ProjectName);
    FormatString(sizeof(Game->Status), Game->Status, "Stopping %s...", Game->ProjectName);
}

internal void
FinishStopGame(editor_state *EditorState, transient_state *TranState)
{
    editor_mode_game *Game = EditorState->Game;
    if(Game->Assets)
    {
        FreeGameAssets(Game->Assets);
    }
    FreeGameFonts(&Game->Fonts, TranState->TextureOpQueue);

    PlayTitleScreen(EditorState, TranState);
}

internal void
DrawGameBuildLog(editor_mode_game *Game, nk_context *Nk, r32 Height)
{
    nk_layout_row_dynamic(Nk, Height, 1);
    if(nk_group_begin(Nk, "Build Log", NK_WINDOW_BORDER|NK_WINDOW_TITLE))
    {
        nk_layout_row_dynamic(Nk, 18, 1);

        char *LineStart = Game->BuildLog;
        for(char *At = Game->BuildLog;; ++At)
        {
            if((*At == '\n') || (*At == 0))
            {
                int Length = (int)(At - LineStart);
                if(Length && (LineStart[Length - 1] == '\r'))
                {
                    --Length;
                }
                nk_text(Nk, LineStart, Length, NK_TEXT_ALIGN_LEFT);

                if(*At == 0)
                {
                    break;
                }
                LineStart = At + 1;
            }
        }

        nk_group_end(Nk);
    }
}

// NOTE(pvlso): The dev controls of a running project: Stop, Restart, the build status and counts,
// the log of a failed build. Internal builds draw them in the debug window (engine_debug.cpp),
// others in the main window. The buttons only set requests: the debug window is drawn after the
// frame's render commands are built, and those still point into the mode arena that Stop frees.
// Fits a narrow panel: buttons, then status and counts on their own rows, then the log.
internal void
DrawGameControls(editor_mode_game *Game, nk_context *Nk, r32 LogHeight)
{
    nk_layout_row_static(Nk, 30, 110, 2);
    if(nk_button_label(Nk, "Stop"))
    {
        Game->StopRequested = true;
    }
    if(nk_button_label(Nk, "Restart"))
    {
        Game->RestartRequested = true;
    }

    nk_layout_row_dynamic(Nk, 30, 1);
    nk_label(Nk, Game->Status, NK_TEXT_ALIGN_LEFT);

    char Assets[320];
    if(Game->AssetFileName[0])
    {
        FormatString(sizeof(Assets), Assets, "Assets: %s/data/keas/%s", Game->ProjectName, Game->AssetFileName);
    }
    else
    {
        FormatString(sizeof(Assets), Assets, "Assets: none (no game_data_<version>.kea in %s/data/keas)",
                     Game->ProjectName);
    }
    nk_label(Nk, Assets, NK_TEXT_ALIGN_LEFT);

    char Counts[128];
    FormatString(sizeof(Counts), Counts, "Builds: %u   Reloads: %u", Game->BuildCount,
                 Game->LoadCount ? (Game->LoadCount - 1) : 0);
    nk_label(Nk, Counts, NK_TEXT_ALIGN_LEFT);

    if(Game->BuildFailed && Game->BuildLog[0])
    {
        DrawGameBuildLog(Game, Nk, LogHeight);
    }
}

internal b32
UpdateAndRenderGame(editor_state *EditorState, transient_state *TranState, engine_input *Input,
                    editor_render_commands *RenderCommands)
{
    TIMED_FUNCTION();

    b32 Result = false;

    editor_mode_game *Game = EditorState->Game;
    nk_context *Nk = EditorState->UIState.Nk;

    if(Game->Stopping)
    {
        FinishStopGame(EditorState, TranState);
        Result = true;
    }
    else if(Game->StopRequested)
    {
        StopGame(EditorState, TranState);
    }
    else
    {
        UpdateGameBuild(Game, TranState, Input->dtForFrame);

#if !EDITOR_INTERNAL
        DrawGameControls(Game, Nk, 400.0f);
#endif

        // NOTE(pvlso): The game starts over with empty memory, the code stays
        if(Game->RestartRequested)
        {
            ClearGameMemory(Game);
            Game->RestartRequested = false;
            LOG("Restart %s", Game->ProjectName);
        }

        if(Game->UpdateAndRender)
        {
            Game->Memory.PlatformAPI = Platform;
            Game->Memory.EngineAPI = GetEngineAPI();
#if EDITOR_INTERNAL
            Game->Memory.DebugTable = GlobalDebugTable;
#endif

            // NOTE(pvlso): The engine_api font calls work on the running game's fonts
            Game->Fonts.TextureOpQueue = TranState->TextureOpQueue;
            GlobalGameFonts = &Game->Fonts;

            // NOTE(pvlso): The game's own TIMED_* blocks nest under this one
            BEGIN_BLOCK("GameUpdateAndRender");
            Game->UpdateAndRender(&Game->Memory, Input, RenderCommands, Game->Assets, Nk);
            END_BLOCK();

            GlobalGameFonts = 0;
        }
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

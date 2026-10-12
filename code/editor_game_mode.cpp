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
GameGetBestMatchAssets(engine_assets *Assets, asset_tag_vector *Vector)
{
    asset_best_match_result Result = {};
    if(Assets)
    {
        Result = GetBestMatchAssets(Assets, Vector);
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

    Result.PushBitmap = GamePushBitmap;
    Result.PushRect = PushRect;
    Result.PushLine = PushLine;
    Result.PushCircleOutline = PushCircleOutline;

    Result.GetBestMatchAssets = GameGetBestMatchAssets;

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

// TODO(pvlso): Every project gets its own data/ and KEA (projects plan phase 4), until then
// the games use the newest game_data_<version>.kea in data/keas, like Arkham did.
internal engine_assets *
LoadGameAssets(editor_state *EditorState, transient_state *TranState)
{
    engine_assets *Result = 0;

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
        }
    }
    EndTemporaryMemory(TempMem);

    return(Result);
}

internal b32
PlayGame(editor_state *EditorState, transient_state *TranState, char *ProjectName,
         char *Status, umm StatusCount)
{
    b32 Result = false;

    // NOTE(pvlso): ProjectName lives in the mode arena, which SetEditorMode frees
    char Name[PROJECT_NAME_COUNT];
    FormatString(sizeof(Name), Name, "%s", ProjectName);

    char ModuleName[256];
    FormatString(sizeof(ModuleName), ModuleName, "%s/build/%s", Name, Name);

    platform_loaded_code Code = Platform.LoadCode(ModuleName, PlatformFileType_Project);
    game_update_and_render *UpdateAndRender = 0;
    if(Code.NoErrors)
    {
        UpdateAndRender = (game_update_and_render *)Platform.GetProcAddress(&Code, "GameUpdateAndRender");
    }

    if(UpdateAndRender)
    {
        SetEditorMode(EditorState, TranState, EditorMode_Game);

        editor_mode_game *Game = PushStruct(&EditorState->ModeArena, editor_mode_game);
        FormatString(sizeof(Game->ProjectName), Game->ProjectName, "%s", Name);
        Game->Code = Code;
        Game->UpdateAndRender = UpdateAndRender;
        Game->Assets = LoadGameAssets(EditorState, TranState);

        EditorState->Game = Game;
        Result = true;
    }
    else
    {
        if(Code.NoErrors)
        {
            FormatString(StatusCount, Status, "projects/%s/build has no GameUpdateAndRender export", Name);
        }
        else
        {
            FormatString(StatusCount, Status, "Could not load projects/%s/build/%s, is it built?", Name, Name);
        }

        Platform.UnloadCode(&Code);
    }

    return(Result);
}

internal void
StopGame(editor_state *EditorState, transient_state *TranState)
{
    editor_mode_game *Game = EditorState->Game;

    // NOTE(pvlso): Asset loads still in flight write into the mode arena
    Platform.CompleteAllWork(TranState->LowPriorityQueue);

    Clear(&Game->Memory.PermanentArena);
    Clear(&Game->Memory.TransientArena);
    Platform.UnloadCode(&Game->Code);

    PlayTitleScreen(EditorState, TranState);
}

internal b32
UpdateAndRenderGame(editor_state *EditorState, transient_state *TranState, engine_input *Input,
                    editor_render_commands *RenderCommands)
{
    b32 Result = false;

    editor_mode_game *Game = EditorState->Game;
    nk_context *Nk = EditorState->UIState.Nk;

    char Buffer[256];
    FormatString(sizeof(Buffer), Buffer, "Running %s", Game->ProjectName);

    nk_layout_row_begin(Nk, NK_STATIC, 30, 2);
    nk_layout_row_push(Nk, 80);
    b32 StopPressed = nk_button_label(Nk, "Stop");
    nk_layout_row_push(Nk, 400);
    nk_label(Nk, Buffer, NK_TEXT_ALIGN_LEFT);
    nk_layout_row_end(Nk);

    if(StopPressed)
    {
        StopGame(EditorState, TranState);
        Result = true;
    }
    else
    {
        Game->Memory.PlatformAPI = Platform;
        Game->Memory.EngineAPI = GetEngineAPI();
        Game->UpdateAndRender(&Game->Memory, Input, RenderCommands, Game->Assets, Nk);
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

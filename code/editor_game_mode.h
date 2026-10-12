#if !defined(EDITOR_GAME_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): A project running inside the editor. Its code is projects/<name>/build/<name>.dll|so,
// rebuilt and reloaded whenever a source file changes. game_memory lives here so it outlives the reloads.
struct editor_mode_game
{
    char ProjectName[PROJECT_NAME_COUNT];

    // NOTE(pvlso): UpdateAndRender is 0 until the first build loaded
    platform_loaded_code Code;
    game_update_and_render *UpdateAndRender;
    u32 LoadCount;

    game_memory Memory;
    engine_assets *Assets;

    // NOTE(pvlso): The sources are checked every SourceCheckInterval seconds, a build starts
    // when their newest write time differs from the one the last build started with
    b32 Building;
    b32 BuildFailed;
    u32 BuildCount;
    platform_process BuildProcess;
    u64 BuiltSourceWriteTime;
    f32 SourceCheckTimer;

    char Status[256];
    char BuildLog[16384];

    // NOTE(pvlso): Set by the controls (DrawGameControls), handled at the start of the next update
    b32 StopRequested;
    b32 RestartRequested;
};

internal void PlayGame(editor_state *EditorState, transient_state *TranState, char *ProjectName);

#define EDITOR_GAME_MODE_H
#endif

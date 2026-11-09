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
    // NOTE(pvlso): Assets lives here, so a swap to a new KEA frees the old one's memory (see SwapRequested)
    memory_arena AssetArena;
    // NOTE(pvlso): The fonts the game asked for (GetFont), freed when it stops
    game_fonts Fonts;
    // NOTE(pvlso): The KEA in the project's data/keas that Assets was loaded from, empty when there's none
    char AssetFileName[256];

    // NOTE(pvlso): The sources are checked every SourceCheckInterval seconds, a build starts
    // when their newest write time differs from the one the last build started with
    b32 Building;
    b32 BuildFailed;
    u32 BuildCount;
    platform_process BuildProcess;
    u64 BuiltSourceWriteTime;
    f32 SourceCheckTimer;

    // NOTE(pvlso): The data folders are checked every GAME_DATA_CHECK_INTERVAL seconds, a KEA is built when their
    // signature (AssetDataSignature) differs from the one the last build was started for
    u64 BuiltDataSignature;
    f32 DataCheckTimer;
    char AssetStatus[256];
    // NOTE(pvlso): A new KEA was built while the game runs. A swap takes two frames like Stop: the first one lets
    // the in-flight loads finish and doesn't run the game (SwapRequested), the second frees the old assets, loads
    // the new KEA and counts the swap in Memory.AssetSwapCount (Swapping).
    b32 SwapRequested;
    b32 Swapping;
    // NOTE(pvlso): The KEA builds on a worker (EditorState->KEABuild): at the start the game waits for it
    // (LoadingAssets); while it runs, a changed folder's build goes on beside it (DataBuilding) and swaps when done
    b32 LoadingAssets;
    b32 DataBuilding;
    // NOTE(pvlso): For the log: how long from Run or Play to the game's first frame
    f64 StartSeconds;
    b32 StartLogged;

    char Status[256];
    char BuildLog[16384];

    // NOTE(pvlso): Set by the controls (DrawGameControls), handled at the start of the next update
    b32 StopRequested;
    b32 RestartRequested;

    // NOTE(pvlso): Stop takes two frames, see StopGame
    b32 Stopping;

    // NOTE(pvlso): The map editor's Play started it (EditorState->PausedMap): Stop or F5 goes back there
    b32 FromMap;
    // NOTE(pvlso): F5 goes back only once it was seen up: the press that started the game runs this mode's first
    // update on the same frame
    b32 PlayKeyArmed;
};

internal void PlayGame(editor_state *EditorState, transient_state *TranState, char *ProjectName);
internal void PlayGameFromMap(editor_state *EditorState, transient_state *TranState, struct editor_mode_map *MapMode,
                              b32 HasP, v2 P, s32 Level);

#define EDITOR_GAME_MODE_H
#endif

#if !defined(EDITOR_GAME_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): A project running inside the editor. Its code is projects/<name>/build/<name>.dll|so,
// game_memory lives here so it outlives a reload of the game code.
struct editor_mode_game
{
    char ProjectName[PROJECT_NAME_COUNT];

    platform_loaded_code Code;
    game_update_and_render *UpdateAndRender;

    game_memory Memory;
    engine_assets *Assets;
};

internal b32 PlayGame(editor_state *EditorState, transient_state *TranState, char *ProjectName,
                      char *Status, umm StatusCount);

#define EDITOR_GAME_MODE_H
#endif

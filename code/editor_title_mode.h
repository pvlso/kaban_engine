#if !defined(EDITOR_TITLE_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#define PROJECT_NAME_COUNT 64

struct editor_mode_title_screen
{
    u32 ModeToPlay;

    // NOTE(pvlso): Projects found in projects/, lives in the mode arena
    u32 ProjectCount;
    char **ProjectNames;
    s32 SelectedProject;

    char NewProjectName[PROJECT_NAME_COUNT];
    char ProjectStatus[256];
};

internal void PlayTitleScreen(editor_state *EditorState, transient_state *TranState);

#define EDITOR_TITLE_MODE_H
#endif

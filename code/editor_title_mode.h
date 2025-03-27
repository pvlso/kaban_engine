#if !defined(EDITOR_TITLE_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct editor_mode_title_screen
{
    u32 ModeToPlay;
};

internal void PlayTitleScreen(editor_state *EditorState, transient_state *TranState);

#define EDITOR_TITLE_MODE_H
#endif

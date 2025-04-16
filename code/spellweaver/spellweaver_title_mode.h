#if !defined(SPELLWEAVER_TITLE_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

struct game_mode_title_screen
{
    b32 Quit;
    r32 t;
};

internal void PlayGameTitleScreen(game_state *GameState, struct game_transient_state *TranState);

#define SPELLWEAVER_TITLE_MODE_H
#endif

#if !defined(SPELLWEAVER_RENDER_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

struct tile_render_work
{
    editor_render_commands *Commands;
    editor_render_prep *Prep;
    struct loaded_bitmap *RenderTargets;
    rectangle2i ClipRect;
};

#define SPELLWEAVER_RENDER_H
#endif

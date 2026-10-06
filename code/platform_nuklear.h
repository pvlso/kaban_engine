#if !defined(PLATFORM_NUKLEAR_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): Nuklear backend state, filled by the platform's input code and
  drawn by engine_opengl.cpp.
*/

#define NK_PLATFORM_TEXT_MAX 256

struct nk_opengl
{
    struct nk_buffer cmds;
    struct nk_draw_null_texture tex_null;
    GLuint font_tex;
};

struct nk_platform
{
    int width, height;
    int display_width, display_height;

    struct nk_opengl ogl;
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    struct nk_vec2 fb_scale;

    unsigned int text[NK_PLATFORM_TEXT_MAX];
    nk_char key_events[NK_KEY_MAX];

    int text_len;
    struct nk_vec2 scroll;
    double last_button_click;
    int is_double_click_down;
    struct nk_vec2 double_click_pos;
    float delta_time_seconds_last;
};

#define PLATFORM_NUKLEAR_H
#endif

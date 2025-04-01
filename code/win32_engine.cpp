/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_platform.h"
#include "engine_shared.h"

#include <windows.h>
#include <malloc.h>
#include <dsound.h>

#include "GL/glew.h"
#include "GL/wglew.h"

#include "win32_defines.h"

#define NK_IMPLEMENTATION
#include "nuklear.h"

#include "win32_engine.h"

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): GLOBAL VARIABLES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
platform_api Platform;

global_variable b32 GlobalRunning;
global_variable b32 GlobalPause;
global_variable b32 GlobalAppIsActive;
global_variable s64 GlobalPerfCountFrequency;

global_variable win32_window_dimension GlobalFramebufferDim;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};

global_variable b32 DEBUGGlobalShowCursor;

global_variable b32 OpenGLSupportsSRGBFramebuffer;
global_variable GLuint OpenGLDefaultInternalTextureFormat;
global_variable GLuint OpenGLReservedBlitTexture;
global_variable GLuint GlobalBlitTextureHandle;
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#include "engine_sort.cpp"
#include "engine_render.h"
#include "engine_opengl.cpp"
#include "engine_render.cpp"

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): WIN32 MEMORY
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
PLATFORM_ALLOCATE_MEMORY(Win32AllocateMemory)
{
    void *Result = VirtualAlloc(0, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    return(Result);
}

PLATFORM_DEALLOCATE_MEMORY(Win32DeallocateMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): WIN32 API
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline LARGE_INTEGER
Win32GetWallClock(void)
{    
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return(Result);
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) /
                     (real32)GlobalPerfCountFrequency);
    return(Result);
}

inline f32
Win32GetTime(void)
{
    LARGE_INTEGER WallClock = Win32GetWallClock();
    f32 Result = (f32)((f64)(WallClock.QuadPart) /
                       (f64)GlobalPerfCountFrequency);
    return(Result);
}

inline void
Win32GetCursorPos(win32_state *State, double* xpos, double* ypos)
{
    /*
       NOTE(paul): The implementation of this function is based on
       implementation in GLFW library
    */

    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    POINT pos;

    if (GetCursorPos(&pos))
    {
        ScreenToClient(State->WindowHandle, &pos);

        if (xpos)
            *xpos = pos.x;
        if (ypos)
            *ypos = pos.y;
    }
}

internal void
Win32SetClipboardString(win32_state *State, const char* string)
{
    /*
       NOTE(paul): The implementation of this function is based on
       implementation in GLFW library
    */

    int characterCount, tries = 0;
    HANDLE object;
    WCHAR* buffer;

    characterCount = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
    if (!characterCount)
        return;

    object = GlobalAlloc(GMEM_MOVEABLE, characterCount * sizeof(WCHAR));
    if (!object)
    {
        Assert(!"Win32: Failed to allocate global handle for clipboard");
        return;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer)
    {
        Assert(!"Win32: Failed to lock global handle");
        GlobalFree(object);
        return;
    }

    MultiByteToWideChar(CP_UTF8, 0, string, -1, buffer, characterCount);
    GlobalUnlock(object);

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    while (!OpenClipboard(State->WindowHandle))
    {
        Sleep(1);
        tries++;

        if (tries == 3)
        {
            Assert(!"Win32: Failed to open clipboard");
            GlobalFree(object);
            return;
        }
    }

    EmptyClipboard();
    SetClipboardData(CF_UNICODETEXT, object);
    CloseClipboard();
}

// Returns a UTF-8 string version of the specified wide string
//
internal char *
Win32CreateUTF8FromWideString(const WCHAR* source)
{
    /*
       NOTE(paul): The implementation of this function is based on
       implementation in GLFW library
    */

    char* target;
    int size;

    size = WideCharToMultiByte(CP_UTF8, 0, source, -1, NULL, 0, NULL, NULL);
    if (!size)
    {
        Assert(!"Win32: Failed to convert string to UTF-8");
        return NULL;
    }

    target = (char *)Win32AllocateMemory(size);
    ZeroSize(size, target);
    
    if (!WideCharToMultiByte(CP_UTF8, 0, source, -1, target, size, NULL, NULL))
    {
        Assert(!"Win32: Failed to convert string to UTF-8");
        Win32DeallocateMemory(target);
        return NULL;
    }

    return target;
}

internal const char*
Win32ClipboardGetString(win32_state *State)
{
    /*
       NOTE(paul): The implementation of this function is based on
       implementation in GLFW library
    */

    HANDLE object;
    WCHAR* buffer;
    int tries = 0;

    // NOTE: Retry clipboard opening a few times as some other application may have it
    //       open and also the Windows Clipboard History reads it after each update
    while (!OpenClipboard(0))
    {
        Sleep(1);
        tries++;

        if (tries == 3)
        {
            Assert(!"Win32: Failed to open clipboard");
            return NULL;
        }
    }

    object = GetClipboardData(CF_UNICODETEXT);
    if (!object)
    {
        Assert(!"Win32: Failed to convert clipboard to string");
        CloseClipboard();
        return NULL;
    }

    buffer = (WCHAR *)GlobalLock(object);
    if (!buffer)
    {
        Assert(!"Win32: Failed to lock global handle");
        CloseClipboard();
        return NULL;
    }

    Win32DeallocateMemory(State->clipboardString);
    State->clipboardString = Win32CreateUTF8FromWideString(buffer);

    GlobalUnlock(object);
    CloseClipboard();

    return State->clipboardString;
}

inline s32
Win32GetKey(win32_state *State, s32 key)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    if (key < WIN32_KEY_SPACE || key > WIN32_KEY_LAST)
    {
        Assert("Invalid key");
        return WIN32_RELEASE;
    }

    s32 Result = (s32)State->keys[key];
    return(Result);
}

inline void
Win32SetCursorPos(win32_state *State, double xpos, double ypos)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    if (xpos != xpos || xpos < -DBL_MAX || xpos > DBL_MAX ||
        ypos != ypos || ypos < -DBL_MAX || ypos > DBL_MAX)
    {
        Assert(!"Invalid cursor position %f %f");
        return;
    }

    // Update system cursor position
    POINT pos = { (int) xpos, (int) ypos };

    // Store the new position so it can be recognized later
    State->lastCursorPosX = pos.x;
    State->lastCursorPosY = pos.y;

    ClientToScreen(State->WindowHandle, &pos);
    SetCursorPos(pos.x, pos.y);
}

inline s32
Win32GetMouseButton(win32_state *State, int button)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    if (button < WIN32_MOUSE_BUTTON_1 || button > WIN32_MOUSE_BUTTON_LAST)
    {
        Assert(!"Invalid mouse button %i");
        return WIN32_RELEASE;
    }

    s32 Result = (s32)State->MouseButtons[button];
    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): NUKLEAR CALLBACKS & Setup
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal inline void
Win32NkScrollCallback(nk_win32 *NkWin32, double xoff, double yoff)
{
    NkWin32->scroll.x += (float)xoff;
    NkWin32->scroll.y += (float)yoff;
}

internal inline void
Win32NkMouseButtonCallback(nk_win32 *NkWin32, win32_state *State, int button, int action)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    double x, y;
    if(button != WIN32_MOUSE_BUTTON_LEFT)
        return;

    Win32GetCursorPos(State, &x, &y);
    if(action == WIN32_PRESS)
    {
        double dt = Win32GetTime() - NkWin32->last_button_click;
        if((dt > NK_WIN32_DOUBLE_CLICK_LO) && (dt < NK_WIN32_DOUBLE_CLICK_HI))
        {
            NkWin32->is_double_click_down = nk_true;
            NkWin32->double_click_pos = nk_vec2((float)x, (float)y);
        }

        NkWin32->last_button_click = Win32GetTime();
    }
    else
        NkWin32->is_double_click_down = nk_false;
}

internal inline void
Win32NkCharCallback(nk_win32 *NkWin32, unsigned int codepoint)
{
    if (NkWin32->text_len < NK_WIN32_TEXT_MAX)
        NkWin32->text[NkWin32->text_len++] = codepoint;
}

inline void
Win32NkKeyCallback(nk_win32 *NkWin32, int key, int scancode, int action, int mods)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    /*
     * convert WIN32_REPEAT to down (technically WIN32_RELEASE, WIN32_PRESS, WIN32_REPEAT are
     * already 0, 1, 2 but just to be clearer)
     */
    nk_char a = (nk_char)((action == WIN32_RELEASE) ? nk_false : nk_true);

    NK_UNUSED(scancode);
    NK_UNUSED(mods);

    switch (key) {
        case WIN32_KEY_DELETE:    NkWin32->key_events[NK_KEY_DEL] = a; break;
        case WIN32_KEY_TAB:       NkWin32->key_events[NK_KEY_TAB] = a; break;
        case WIN32_KEY_BACKSPACE: NkWin32->key_events[NK_KEY_BACKSPACE] = a; break;
        case WIN32_KEY_UP:        NkWin32->key_events[NK_KEY_UP] = a; break;
        case WIN32_KEY_DOWN:      NkWin32->key_events[NK_KEY_DOWN] = a; break;
        case WIN32_KEY_LEFT:      NkWin32->key_events[NK_KEY_LEFT] = a; break;
        case WIN32_KEY_RIGHT:     NkWin32->key_events[NK_KEY_RIGHT] = a; break;

        case WIN32_KEY_PAGE_UP:   NkWin32->key_events[NK_KEY_SCROLL_UP] = a; break;
        case WIN32_KEY_PAGE_DOWN: NkWin32->key_events[NK_KEY_SCROLL_DOWN] = a; break;

            /* have to add all keys used for nuklear to get correct repeat behavior
             * NOTE these are scancodes so your custom layout won't matter unfortunately
             * Also while including everything will prevent unnecessary input calls,
             * only the ones with visible effects really matter, ie paste, undo, redo
             * selecting all, copying or cutting 40 times before you release the keys
             * doesn't actually cause any visible problems */

        case WIN32_KEY_C:         NkWin32->key_events[NK_KEY_COPY] = a; break;
        case WIN32_KEY_V:         NkWin32->key_events[NK_KEY_PASTE] = a; break;
        case WIN32_KEY_X:         NkWin32->key_events[NK_KEY_CUT] = a; break;
        case WIN32_KEY_Z:         NkWin32->key_events[NK_KEY_TEXT_UNDO] = a; break;
        case WIN32_KEY_R:         NkWin32->key_events[NK_KEY_TEXT_REDO] = a; break;
        case WIN32_KEY_B:         NkWin32->key_events[NK_KEY_TEXT_LINE_START] = a; break;
        case WIN32_KEY_E:         NkWin32->key_events[NK_KEY_TEXT_LINE_END] = a; break;
        case WIN32_KEY_A:         NkWin32->key_events[NK_KEY_TEXT_SELECT_ALL] = a; break;

        case WIN32_KEY_ENTER:
        case WIN32_KEY_KP_ENTER:
            NkWin32->key_events[NK_KEY_ENTER] = a;
            break;
        default:
            ;
    }
}

internal void
Win32NkClipboardPaste(nk_handle usr, struct nk_text_edit *edit)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    win32_state *State = (win32_state *)usr.ptr;
    const char *text = Win32ClipboardGetString(State);
    if (text)
        nk_textedit_paste(edit, text, nk_strlen(text));
    (void)usr;
}

internal void
Win32NkClipboardCopy(nk_handle usr, const char *text, int len)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    win32_state *State = (win32_state *)usr.ptr;

    char *str = 0;
    (void)usr;
    if (!len) return;
    str = (char*)Win32AllocateMemory((size_t)len+1);
    if (!str) return;
    Copy(len, (void *)text, (void *)str);
    str[len] = '\0';
    Win32SetClipboardString(State, str);
    Win32DeallocateMemory(str);
}

internal struct nk_context*
Win32InitNkContext(win32_state *State, nk_win32 *NkWin32)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    nk_init_default(&NkWin32->ctx, 0);

    NkWin32->ctx.clip.userdata.ptr = (void *)State;
    NkWin32->ctx.clip.copy = Win32NkClipboardCopy;
    NkWin32->ctx.clip.paste = Win32NkClipboardPaste;
    nk_buffer_init_default(&NkWin32->ogl.cmds);

    NkWin32->is_double_click_down = nk_false;
    NkWin32->double_click_pos = nk_vec2(0, 0);

    NkWin32->delta_time_seconds_last = Win32GetTime();

    return &NkWin32->ctx;
}

internal void
Win32NkFontStashBegin(nk_win32 *NkWin32, struct nk_font_atlas **atlas)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    nk_font_atlas_init_default(&NkWin32->atlas);
    nk_font_atlas_begin(&NkWin32->atlas);
    *atlas = &NkWin32->atlas;
}

internal void
Win32NkFontStashEnd(nk_win32 *NkWin32)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    const void *image; int w, h;
    image = nk_font_atlas_bake(&NkWin32->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    NkOpenGLUploadAtlas(&NkWin32->ogl, image, w, h);
    nk_font_atlas_end(&NkWin32->atlas, nk_handle_id((int)NkWin32->ogl.font_tex), &NkWin32->ogl.tex_null);
    if (NkWin32->atlas.default_font)
        nk_style_set_font(&NkWin32->ctx, &NkWin32->atlas.default_font->handle);
}

internal void
Win32NkUpdateInputs(win32_state *State, nk_win32 *NkWin32, u32 WindowWidth, u32 WindowHeight,
                    rectangle2i DrawRegion, f32 dt)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    int i;
    double x, y;
    struct nk_context *ctx = &NkWin32->ctx;
    nk_char* k_state = NkWin32->key_events;

    /* update the timer */
    float delta_time_now = dt;
    NkWin32->delta_time_seconds_last = dt;

    NkWin32->width = WindowWidth;
    NkWin32->height = WindowHeight;
    NkWin32->display_width = GetWidth(DrawRegion);
    NkWin32->display_height = GetHeight(DrawRegion);
    NkWin32->fb_scale.x = (float)NkWin32->display_width/(float)NkWin32->width;
    NkWin32->fb_scale.y = (float)NkWin32->display_height/(float)NkWin32->height;

    nk_input_begin(ctx);
    for (i = 0; i < NkWin32->text_len; ++i)
        nk_input_unicode(ctx, NkWin32->text[i]);

    if (k_state[NK_KEY_DEL] >= 0) nk_input_key(ctx, NK_KEY_DEL, k_state[NK_KEY_DEL]);
    if (k_state[NK_KEY_ENTER] >= 0) nk_input_key(ctx, NK_KEY_ENTER, k_state[NK_KEY_ENTER]);

    if (k_state[NK_KEY_TAB] >= 0) nk_input_key(ctx, NK_KEY_TAB, k_state[NK_KEY_TAB]);
    if (k_state[NK_KEY_BACKSPACE] >= 0) nk_input_key(ctx, NK_KEY_BACKSPACE, k_state[NK_KEY_BACKSPACE]);
    if (k_state[NK_KEY_UP] >= 0) nk_input_key(ctx, NK_KEY_UP, k_state[NK_KEY_UP]);
    if (k_state[NK_KEY_DOWN] >= 0) nk_input_key(ctx, NK_KEY_DOWN, k_state[NK_KEY_DOWN]);
    if (k_state[NK_KEY_SCROLL_UP] >= 0) nk_input_key(ctx, NK_KEY_SCROLL_UP, k_state[NK_KEY_SCROLL_UP]);
    if (k_state[NK_KEY_SCROLL_DOWN] >= 0) nk_input_key(ctx, NK_KEY_SCROLL_DOWN, k_state[NK_KEY_SCROLL_DOWN]);

    nk_input_key(ctx, NK_KEY_TEXT_START, Win32GetKey(State, WIN32_KEY_HOME) == WIN32_PRESS);
    nk_input_key(ctx, NK_KEY_TEXT_END, Win32GetKey(State, WIN32_KEY_END) == WIN32_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_START, Win32GetKey(State, WIN32_KEY_HOME) == WIN32_PRESS);
    nk_input_key(ctx, NK_KEY_SCROLL_END, Win32GetKey(State, WIN32_KEY_END) == WIN32_PRESS);
    nk_input_key(ctx, NK_KEY_SHIFT, Win32GetKey(State, WIN32_KEY_LEFT_SHIFT) == WIN32_PRESS||
                 Win32GetKey(State, WIN32_KEY_RIGHT_SHIFT) == WIN32_PRESS);

    if (Win32GetKey(State, WIN32_KEY_LEFT_CONTROL) == WIN32_PRESS ||
        Win32GetKey(State, WIN32_KEY_RIGHT_CONTROL) == WIN32_PRESS) {
        /* Note these are physical keys and won't respect any layouts/key mapping */
        if (k_state[NK_KEY_COPY] >= 0) nk_input_key(ctx, NK_KEY_COPY, k_state[NK_KEY_COPY]);
        if (k_state[NK_KEY_PASTE] >= 0) nk_input_key(ctx, NK_KEY_PASTE, k_state[NK_KEY_PASTE]);
        if (k_state[NK_KEY_CUT] >= 0) nk_input_key(ctx, NK_KEY_CUT, k_state[NK_KEY_CUT]);
        if (k_state[NK_KEY_TEXT_UNDO] >= 0) nk_input_key(ctx, NK_KEY_TEXT_UNDO, k_state[NK_KEY_TEXT_UNDO]);
        if (k_state[NK_KEY_TEXT_REDO] >= 0) nk_input_key(ctx, NK_KEY_TEXT_REDO, k_state[NK_KEY_TEXT_REDO]);
        if (k_state[NK_KEY_TEXT_LINE_START] >= 0) nk_input_key(ctx, NK_KEY_TEXT_LINE_START, k_state[NK_KEY_TEXT_LINE_START]);
        if (k_state[NK_KEY_TEXT_LINE_END] >= 0) nk_input_key(ctx, NK_KEY_TEXT_LINE_END, k_state[NK_KEY_TEXT_LINE_END]);
        if (k_state[NK_KEY_TEXT_SELECT_ALL] >= 0) nk_input_key(ctx, NK_KEY_TEXT_SELECT_ALL, k_state[NK_KEY_TEXT_SELECT_ALL]);
        if (k_state[NK_KEY_LEFT] >= 0) nk_input_key(ctx, NK_KEY_TEXT_WORD_LEFT, k_state[NK_KEY_LEFT]);
        if (k_state[NK_KEY_RIGHT] >= 0) nk_input_key(ctx, NK_KEY_TEXT_WORD_RIGHT, k_state[NK_KEY_RIGHT]);
    } else {
        if (k_state[NK_KEY_LEFT] >= 0) nk_input_key(ctx, NK_KEY_LEFT, k_state[NK_KEY_LEFT]);
        if (k_state[NK_KEY_RIGHT] >= 0) nk_input_key(ctx, NK_KEY_RIGHT, k_state[NK_KEY_RIGHT]);
        nk_input_key(ctx, NK_KEY_COPY, 0);
        nk_input_key(ctx, NK_KEY_PASTE, 0);
        nk_input_key(ctx, NK_KEY_CUT, 0);
    }

    Win32GetCursorPos(State, &x, &y);

    r32 MouseU = Clamp01MapToRange((r32)DrawRegion.MinX, (f32)x, (r32)DrawRegion.MaxX);
    r32 MouseV = Clamp01MapToRange((r32)DrawRegion.MinY, (f32)y, (r32)DrawRegion.MaxY);
                            
    x = (r32)NkWin32->width*MouseU;
    y = (r32)NkWin32->height*MouseV;

    nk_input_motion(ctx, (int)x, (int)y);
    if (ctx->input.mouse.grabbed) {
        Win32SetCursorPos(State, (double)ctx->input.mouse.prev.x, (double)ctx->input.mouse.prev.y);
        ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
        ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
    }

    nk_input_button(ctx, NK_BUTTON_LEFT, (int)x, (int)y, Win32GetMouseButton(State, WIN32_MOUSE_BUTTON_LEFT) == WIN32_PRESS);
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)x, (int)y, Win32GetMouseButton(State, WIN32_MOUSE_BUTTON_MIDDLE) == WIN32_PRESS);
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)x, (int)y, Win32GetMouseButton(State, WIN32_MOUSE_BUTTON_RIGHT) == WIN32_PRESS);
    nk_input_button(ctx, NK_BUTTON_DOUBLE, (int)NkWin32->double_click_pos.x, (int)NkWin32->double_click_pos.y, NkWin32->is_double_click_down);
    nk_input_scroll(ctx, NkWin32->scroll);
    nk_input_end(&NkWin32->ctx);

    /* clear after nk_input_end (-1 since we're doing up/down boolean) */
    memset(NkWin32->key_events, -1, sizeof(NkWin32->key_events));

    NkWin32->text_len = 0;
    NkWin32->scroll = nk_vec2(0,0);
}

internal void
Win32NkShutdown(nk_win32 *NkWin32)
{
    /*
      NOTE(paul): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    struct nk_opengl *dev = &NkWin32->ogl;
    nk_font_atlas_clear(&NkWin32->atlas);
    nk_free(&NkWin32->ctx);
    glDeleteTextures(1, &dev->font_tex);
    nk_buffer_free(&dev->cmds);
    memset(&NkWin32, 0, sizeof(NkWin32));
}

inline nk_context *
Win32SetupNkContext(win32_state *State, nk_win32 *NkWin32, s32 Width, s32 Height)
{
    struct nk_context *Result = 0;
    Result = Win32InitNkContext(State, NkWin32);
    Result->BaseWidth = Width;
    Result->BaseHeight = Height;
    {
        struct nk_font_atlas *atlas;
        Win32NkFontStashBegin(NkWin32, &atlas);
        struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "fonts\\LiberationMono-Regular.ttf", 12, 0);
        /*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
        /*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
        /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0);*/
        /*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
        /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
        Win32NkFontStashEnd(NkWin32);
        nk_style_load_all_cursors(Result, atlas->cursors);
        nk_style_set_font(Result, &droid->handle);
    }

    return(Result);
}

internal inline void
Win32SetUIPointers(nk_ui *UI)
{
    UI->NkBegin = nk_begin;
    UI->NkEnd = nk_end;
    UI->NkSpacer = nk_spacer;
 
    UI->NkWindowSetFocus = nk_window_set_focus;
    UI->NkWindowCollapse = nk_window_collapse;
    UI->NkWindowShow = nk_window_show;
    UI->NkWindowIsActive = nk_window_is_active;
    UI->NkWindowIsHidden = nk_window_is_hidden;

    UI->NkGroupBegin = nk_group_begin;
    UI->NkGroupEnd = nk_group_end;

    UI->NkInputHasMouseClick = nk_input_has_mouse_click;
    UI->NkInputHasMouseClickInRect = nk_input_has_mouse_click_in_rect;
    UI->NkInputHasMouseClickInButtonRect = nk_input_has_mouse_click_in_button_rect;
    UI->NkInputHasMouseClickDownInRect = nk_input_has_mouse_click_down_in_rect;
    UI->NkInputIsMouseClickInRect = nk_input_is_mouse_click_in_rect;
    UI->NkInputIsMouseClickDownInRect = nk_input_is_mouse_click_down_in_rect;
    UI->NkInputAnyMouseClickInRect = nk_input_any_mouse_click_in_rect;
    UI->NkInputIsMousePrevHoveringRect = nk_input_is_mouse_prev_hovering_rect;
    UI->NkInputIsMouseHoveringRect = nk_input_is_mouse_hovering_rect;
    UI->NkInputMouseClicked = nk_input_mouse_clicked;
    UI->NkInputIsMouseDown = nk_input_is_mouse_down;
    UI->NkInputIsMousePressed = nk_input_is_mouse_pressed;
    UI->NkInputIsMouseReleased = nk_input_is_mouse_released;
    UI->NkInputIsKeyPressed = nk_input_is_key_pressed;
    UI->NkInputIsKeyReleased = nk_input_is_key_released;
    UI->NkInputIsKeyDown = nk_input_is_key_down;

    UI->NkLayoutRowDynamic = nk_layout_row_dynamic;
    UI->NkLayoutRowBegin = nk_layout_row_begin;
    UI->NkLayoutRowPush = nk_layout_row_push;
    UI->NkLayoutRowEnd = nk_layout_row_end;
    UI->NkLayoutRowStatic = nk_layout_row_static;

    UI->NkLayoutSpaceBegin = nk_layout_space_begin;
    UI->NkLayoutSpacePush = nk_layout_space_push;
    UI->NkLayoutSpaceEnd = nk_layout_space_end;
    UI->NkLayoutSpaceBounds = nk_layout_space_bounds;
    UI->NkLayoutSpaceRectToLocal = nk_layout_space_rect_to_local;

    UI->NkText = nk_text;
    UI->NkTextColored = nk_text_colored;
    UI->NkTextWrap = nk_text_wrap;
    UI->NkTextWrapColored = nk_text_wrap_colored;
    UI->NkLabel = nk_label;
    UI->NkLabelColored = nk_label_colored;
    UI->NkLabelWrap = nk_label_wrap;
    UI->NkLabelColoredWrap = nk_label_colored_wrap;
    UI->NkImage = nk_image;
    UI->NkImageColor = nk_image_color;

    UI->NkLabelf = nk_labelf;
    UI->NkLabelfColored = nk_labelf_colored;
    UI->NkLabelfWrap = nk_labelf_wrap;
    UI->NkLabelfColoredWrap = nk_labelf_colored_wrap;
    UI->NkLabelfv = nk_labelfv;
    UI->NkLabelfvColored = nk_labelfv_colored;
    UI->NkLabelfvWrap = nk_labelfv_wrap;
    UI->NkLabelfvColoredWrap = nk_labelfv_colored_wrap;
    UI->NkValueBool = nk_value_bool;
    UI->NkValueInt = nk_value_int;
    UI->NkValueUint = nk_value_uint;
    UI->NkValueFloat = nk_value_float;
    UI->NkValueColorByte = nk_value_color_byte;
    UI->NkValueColorFloat = nk_value_color_float;
    UI->NkValueColorHex = nk_value_color_hex;

    UI->NkButtonText = nk_button_text;
    UI->NkButtonLabel = nk_button_label;
    UI->NkButtonColor = nk_button_color;
    UI->NkButtonSymbol = nk_button_symbol;
    UI->NkButtonImage = nk_button_image;
    UI->NkButtonSymbolLabel = nk_button_symbol_label;
    UI->NkButtonSymbolText = nk_button_symbol_text;
    UI->NkButtonImageLabel = nk_button_image_label;
    UI->NkButtonImageText = nk_button_image_text;
    UI->NkButtonTextStyled = nk_button_text_styled;
    UI->NkButtonLabelStyled = nk_button_label_styled;
    UI->NkButtonSymbolStyled = nk_button_symbol_styled;
    UI->NkButtonImageStyled = nk_button_image_styled;
    UI->NkButtonSymbolTextStyled = nk_button_symbol_text_styled;
    UI->NkButtonSymbolLabelStyled = nk_button_symbol_label_styled;
    UI->NkButtonImageLabelStyled = nk_button_image_label_styled;
    UI->NkButtonImageTextStyled = nk_button_image_text_styled;
    UI->NkButtonSetBehavior = nk_button_set_behavior;
    UI->NkButtonPushBehavior = nk_button_push_behavior;
    UI->NkButtonPopBehavior = nk_button_pop_behavior;

    UI->NkCheckLabel = nk_check_label;
    UI->NkCheckText = nk_check_text;
    UI->NkCheckTextAlign = nk_check_text_align;
    UI->NkCheckFlagsLabel = nk_check_flags_label;
    UI->NkCheckFlagsText = nk_check_flags_text;
    UI->NkCheckboxLabel = nk_checkbox_label;
    UI->NkCheckboxLabelAlign = nk_checkbox_label_align;
    UI->NkCheckboxText = nk_checkbox_text;
    UI->NkCheckboxTextAlign = nk_checkbox_text_align;
    UI->NkCheckboxFlagsLabel = nk_checkbox_flags_label;
    UI->NkCheckboxFlagsText = nk_checkbox_flags_text;

    UI->NkSelectableLabel = nk_selectable_label;
    UI->NkSelectableText = nk_selectable_text;
    UI->NkSelectableImageLabel = nk_selectable_image_label;
    UI->NkSelectableImageText = nk_selectable_image_text;
    UI->NkSelectableSymbolLabel = nk_selectable_symbol_label;
    UI->NkSelectableSymbolText = nk_selectable_symbol_text;

    UI->NkEditString = nk_edit_string;
    UI->NkEditStringZeroTerminated = nk_edit_string_zero_terminated;
    UI->NkEditBuffer = nk_edit_buffer;
    UI->NkEditFocus = nk_edit_focus;
    UI->NkEditUnfocus = nk_edit_unfocus;

    UI->NkMurmurHash = nk_murmur_hash;
    UI->NkTriangleFromDirection = nk_triangle_from_direction;

    UI->NkVec2 = nk_vec2;
    UI->NkVec2i = nk_vec2i;
    UI->NkVec2v = nk_vec2v;
    UI->NkVec2iv = nk_vec2iv;

    UI->NkGetNullRect = nk_get_null_rect;
    UI->NkRect = nk_rect;
    UI->NkRecti = nk_recti;
    UI->NkRecta = nk_recta;
    UI->NkRectv = nk_rectv;
    UI->NkRectiv = nk_rectiv;
    UI->NkRectPos = nk_rect_pos;
    UI->NkRectSize = nk_rect_size;

    UI->NkStrokeLine = nk_stroke_line;
    UI->NkStrokeCurve = nk_stroke_curve;
    UI->NkStrokeRect = nk_stroke_rect;
    UI->NkStrokeCircle = nk_stroke_circle;
    UI->NkStrokeArc = nk_stroke_arc;
    UI->NkStrokeTriangle = nk_stroke_triangle;
    UI->NkStrokePolyLine = nk_stroke_polyline;
    UI->NkStrokePolygon = nk_stroke_polygon;

    UI->NkTreePushHashed = nk_tree_push_hashed;
    UI->NkTreePop = nk_tree_pop;

    UI->NkStrlen = nk_strlen;
    UI->NkStricmp = nk_stricmp;
    UI->NkStricmpn = nk_stricmpn;
    UI->NkStrtoi = nk_strtoi;
    UI->NkStrtof = nk_strtof;

    UI->NkProgress = nk_progress;
    UI->NkProg = nk_prog;

    UI->NkChartBegin = nk_chart_begin;
    UI->NkChartBeginColored = nk_chart_begin_colored;
    UI->NkChartAddSlot = nk_chart_add_slot;
    UI->NkChartAddSlotColored = nk_chart_add_slot_colored;
    UI->NkChartPush = nk_chart_push;
    UI->NkChartPushSlot = nk_chart_push_slot;
    UI->NkChartEnd = nk_chart_end;
    UI->NkPlot = nk_plot;
    UI->NkPlotFunction = nk_plot_function;

    UI->NkWindowGetCanvas = nk_window_get_canvas;

    UI->NkWidget = nk_widget;
    UI->NkWidgetFitting = nk_widget_fitting;
    UI->NkWidgetBounds = nk_widget_bounds;
    UI->NkWidgetPosition = nk_widget_position;
    UI->NkWidgetSize = nk_widget_size;
    UI->NkWidgetWidth = nk_widget_width;
    UI->NkWidgetHeight = nk_widget_height;
    UI->NkWidgetIsHovered = nk_widget_is_hovered;
    UI->NkWidgetIsMouseClicked = nk_widget_is_mouse_clicked;
    UI->NkWidgetHasMouseClickDowm = nk_widget_has_mouse_click_down;
    UI->NkSpacing = nk_spacing;
    UI->NkWidgetDisableBegin = nk_widget_disable_begin;
    UI->NkWidgetDisableEnd = nk_widget_disable_end;

    UI->NkFillRect = nk_fill_rect;
    UI->NkFillRectMultiColor = nk_fill_rect_multi_color;
    UI->NkFillCircle = nk_fill_circle;
    UI->NkFillArc = nk_fill_arc;
    UI->NkFillTriangle = nk_fill_triangle;
    UI->NkFillPolygon = nk_fill_polygon;

    UI->NkTooltip = nk_tooltip;
#ifdef NK_INCLUDE_STANDARD_VARARGS
    UI->NkTooltipf = nk_tooltipf;
    UI->NkTooltipfv = nk_tooltipfv;
#endif
    UI->NkTooltipBegin = nk_tooltip_begin;
    UI->NkTooltipEnd = nk_tooltip_end;

    UI->NkPropertyInt = nk_property_int;


    UI->NkHandlePtr = nk_handle_ptr;
    UI->NkHandleID = nk_handle_id;
    UI->NkImageHandle = nk_image_handle;
    UI->NkImagePtr = nk_image_ptr;
    UI->NkImageID = nk_image_id;
    UI->NkImageIsSubimage = nk_image_is_subimage;
    UI->NkSubimagePtr = nk_subimage_ptr;
    UI->NkSubimageID = nk_subimage_id;
    UI->NkSubimageHandle = nk_subimage_handle;
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): CODE LOADING
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline u32
StringLengthW(wchar_t *String)
{
    u32 Result = (DWORD)wcslen(String)*sizeof(wchar_t);
    return(Result);
}

internal void
CatStrings(size_t SourceACount, wchar_t *SourceA,
           size_t SourceBCount, wchar_t *SourceB,
           size_t DestCount, wchar_t *Dest)
{
    for(int Index = 0;
        Index < SourceACount;
        ++Index)
    {
        *Dest++ = *SourceA++;
    }

    for(int Index = 0;
        Index < SourceBCount;
        ++Index)
    {
        *Dest++ = *SourceB++;
    }

    *Dest++ = 0;
}

internal void
Win32GetEXEFileName(win32_state *State)
{
    // NOTE(casey): Never use MAX_PATH in code that is user-facing, because it
    // can be dangerous and lead to bad results.
    DWORD SizeOfFilename = GetModuleFileNameW(0, State->EXEFileName, sizeof(State->EXEFileName));
    State->OnePastLastEXEFileNameSlash = State->EXEFileName;
    for(wchar_t *Scan = State->EXEFileName;
        *Scan;
        ++Scan)
    {
        if(*Scan == '\\')
        {
            State->OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }
}

internal void
Win32BuildEXEPathFileName(win32_state *State, wchar_t *FileName,
                          int DestCount, wchar_t *Dest)
{
    CatStrings(State->OnePastLastEXEFileNameSlash - State->EXEFileName, State->EXEFileName,
               StringLengthW(FileName), FileName,
               DestCount, Dest);
}

inline FILETIME
Win32GetLastWriteTime(wchar_t *Filename)
{
    FILETIME LastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if(GetFileAttributesExW(Filename, GetFileExInfoStandard, &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }

    return(LastWriteTime);
}

inline b32
Win32TimeIsValid(FILETIME Time)
{
    b32 Result = (Time.dwLowDateTime != 0) || (Time.dwHighDateTime != 0);
    return(Result);
}

internal win32_engine_code
Win32LoadEngineCode(wchar_t *SourceDLLName, wchar_t *TempDLLName, wchar_t *LockFileName)
{
    win32_engine_code Result = {};

    WIN32_FILE_ATTRIBUTE_DATA Ignored;
    if(!GetFileAttributesExW(LockFileName, GetFileExInfoStandard, &Ignored))
    {
        Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);
#if EDITOR_INTERNAL
        CopyFileW(SourceDLLName, TempDLLName, FALSE);

        Result.EditorCodeDLL = LoadLibraryW(TempDLLName);
#else
        Result.EditorCodeDLL = LoadLibraryW(SourceDLLName);
#endif
        if(Result.EditorCodeDLL)
        {
            Result.UpdateAndRender = (engine_update_and_render *)
                GetProcAddress(Result.EditorCodeDLL, "EngineUpdateAndRender");

            Result.GetSoundSamples = (engine_get_sound_samples *)
                GetProcAddress(Result.EditorCodeDLL, "EngineGetSoundSamples");

            Result.DEBUGFrameEnd = (debug_editor_frame_end *)
                GetProcAddress(Result.EditorCodeDLL, "DEBUGEditorFrameEnd");


            Result.IsValid = (Result.UpdateAndRender &&
                              Result.GetSoundSamples &&
                              Result.DEBUGFrameEnd);
        }
    }
    else
    {
        // TODO: Logging
    }
    
    if(!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
        Result.DEBUGFrameEnd = 0;
    }

    return(Result);
}

internal void
Win32UnloadEngineCode(win32_engine_code *EditorCode)
{    
    if(EditorCode->EditorCodeDLL)
    {
        FreeLibrary(EditorCode->EditorCodeDLL);
        EditorCode->EditorCodeDLL = 0;
    }

    EditorCode->IsValid = false;
    EditorCode->UpdateAndRender = 0;
    EditorCode->GetSoundSamples = 0;
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): SOUND
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // NOTE(casey): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if(DSoundLibrary)
    {
        // NOTE(casey): Get a DirectSound object! - cooperative
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)
            GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels*WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec*WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if(SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // NOTE(casey): "Create" a primary buffer
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if(SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                    if(SUCCEEDED(Error))
                    {
                        // NOTE(casey): We have finally set the format!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO(casey): Diagnostic
                    }
                }
                else
                {
                    // TODO(casey): Diagnostic
                }
            }
            else
            {
                // TODO(casey): Diagnostic
            }

            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;

#if EDITOR_INTERNAL
            BufferDescription.dwFlags |= DSBCAPS_GLOBALFOCUS;
#endif
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if(SUCCEEDED(Error))
            {
                OutputDebugStringA("Secondary buffer created successfully.\n");
            }
        }
        else
        {
            // TODO(casey): Diagnostic
        }
    }
    else
    {
        // TODO(casey): Diagnostic
    }
}


internal void
Win32ClearSoundBuffer(win32_sound_output *SoundOutput)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size,
                                             0)))
    {
        // TODO(casey): assert that Region1Size/Region2Size is valid
        uint8 *DestSample = (uint8 *)Region1;
        for(DWORD ByteIndex = 0;
            ByteIndex < Region1Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;
        }

        DestSample = (uint8 *)Region2;
        for(DWORD ByteIndex = 0;
            ByteIndex < Region2Size;
            ++ByteIndex)
        {
            *DestSample++ = 0;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

internal void
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
                     engine_sound_output_buffer *SourceBuffer)
{
    // TODO(casey): More strenuous test!
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                             &Region1, &Region1Size,
                                             &Region2, &Region2Size,
                                             0)))
    {
        // TODO(casey): assert that Region1Size/Region2Size is valid

        // TODO(casey): Collapse these two loops
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *)Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region1SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        DestSample = (int16 *)Region2;
        for(DWORD SampleIndex = 0;
            SampleIndex < Region2SampleCount;
            ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++SoundOutput->RunningSampleIndex;
        }

        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): DEBUG
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#if EDITOR_INTERNAL
DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(DEBUGExecuteSystemCommand)
{
    debug_executing_process Result = {};

    STARTUPINFO StartupInfo = {};
    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION ProcessInfo = {};    
    if(CreateProcess(Command,
                     CommandLine,
                     0,
                     0,
                     FALSE,
                     0,
                     0,
                     Path,
                     &StartupInfo,
                     &ProcessInfo))
    {
        Assert(sizeof(Result.OSHandle) >= sizeof(ProcessInfo.hProcess));
        *(HANDLE *)&Result.OSHandle = ProcessInfo.hProcess;
    }
    else
    {
        DWORD ErrorCode = GetLastError();
        *(HANDLE *)&Result.OSHandle = INVALID_HANDLE_VALUE;
    }

    return(Result);
}

DEBUG_PLATFORM_GET_PROCESS_STATE(DEBUGGetProcessState)
{
    debug_process_state Result = {};

    HANDLE hProcess = *(HANDLE *)&Process.OSHandle;
    if(hProcess != INVALID_HANDLE_VALUE)
    {
        Result.StartedSuccessfully = true;

        if(WaitForSingleObject(hProcess, 0) == WAIT_OBJECT_0)
        {
            DWORD ReturnCode = 0;
            GetExitCodeProcess(hProcess, &ReturnCode);
            Result.ReturnCode = ReturnCode;
            CloseHandle(hProcess);
        }
        else
        {
            Result.IsRunning = true;
        }
    }

    return(Result);
}
#endif
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): OPENGL INIT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
Win32SetPixelFormat(HDC WindowDC)
{
    int SuggestedPixelFormatIndex = 0;
    GLuint ExtendedPick = 0;
    if(wglChoosePixelFormatARB)
    {
        int IntAttribList[] =
            {
                WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
                WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB, GL_TRUE,
                0,
            };

        if(!OpenGLSupportsSRGBFramebuffer)
        {
            IntAttribList[10] = 0;
        }

        wglChoosePixelFormatARB(WindowDC, IntAttribList, 0, 1, 
                                &SuggestedPixelFormatIndex, &ExtendedPick);
    }

    if(!ExtendedPick)
    {
        PIXELFORMATDESCRIPTOR DesiredPixelFormat = {};
        DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
        DesiredPixelFormat.nVersion = 1;
        DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
        DesiredPixelFormat.dwFlags = PFD_SUPPORT_OPENGL|PFD_DRAW_TO_WINDOW|PFD_DOUBLEBUFFER;
        DesiredPixelFormat.cColorBits = 32;
        DesiredPixelFormat.cAlphaBits = 8;
        DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

        SuggestedPixelFormatIndex = ChoosePixelFormat(WindowDC, &DesiredPixelFormat);
    }

    PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
    DescribePixelFormat(WindowDC, SuggestedPixelFormatIndex,
                        sizeof(SuggestedPixelFormat), &SuggestedPixelFormat);
    SetPixelFormat(WindowDC, SuggestedPixelFormatIndex, &SuggestedPixelFormat);
}

internal void
Win32LoadWGLExtensions(void)
{
    WNDCLASSA WindowClass = {};

    WindowClass.lpfnWndProc = DefWindowProcA;
    WindowClass.hInstance = GetModuleHandle(0);
    WindowClass.lpszClassName = "EditorWGLLoader";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "Editor",
            0,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            WindowClass.hInstance,
            0);

        HDC WindowDC = GetDC(Window);
        Win32SetPixelFormat(WindowDC);
        HGLRC OpenGLRC = wglCreateContext(WindowDC);
        if(wglMakeCurrent(WindowDC, OpenGLRC))        
        {
            Assert(glewInit() == GLEW_OK);

            if(WGLEW_EXT_framebuffer_sRGB || WGLEW_ARB_framebuffer_sRGB)
            {
                OpenGLSupportsSRGBFramebuffer = true;
            }

            wglMakeCurrent(0, 0);
        }

        wglDeleteContext(OpenGLRC);
        ReleaseDC(Window, WindowDC);
        DestroyWindow(Window);
    }
}

internal HGLRC
Win32InitOpenGL(HDC WindowDC)
{
    Win32LoadWGLExtensions();
    
    Win32SetPixelFormat(WindowDC);

    b32 ModernContext = true;
    HGLRC OpenGLRC = 0;
    if(wglCreateContextAttribsARB)
    {

        int Win32OpenGLAttribs[] =
            {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
                WGL_CONTEXT_MINOR_VERSION_ARB, 0,
                WGL_CONTEXT_FLAGS_ARB, 0 // NOTE(casey): Enable for testing WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#if EDITOR_INTERNAL
                |WGL_CONTEXT_DEBUG_BIT_ARB
#endif
                ,
                WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                0,
            };

        OpenGLRC = wglCreateContextAttribsARB(WindowDC, 0, Win32OpenGLAttribs);
    }

    if(!OpenGLRC)
    {
        ModernContext = false;
        OpenGLRC = wglCreateContext(WindowDC);
    }

    if(wglMakeCurrent(WindowDC, OpenGLRC))
    {
        opengl_info Info = OpenGLInit(ModernContext, OpenGLSupportsSRGBFramebuffer);

        if(wglSwapIntervalEXT)
        {
            wglSwapIntervalEXT(1);
        }

        glGenTextures(1, &OpenGLReservedBlitTexture);
    }

    return(OpenGLRC);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): WINDOW AND DISPLAY
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
Win32DisplayBufferInWindow(platform_work_queue *RenderQueue, editor_render_commands *Commands,
                           HDC DeviceContext, rectangle2i DrawRegion, u32 WindowWidth, u32 WindowHeight,
                           memory_arena *TempArena)
{
    temporary_memory TempMem = BeginTemporaryMemory(TempArena);

    editor_render_prep Prep = PrepForRender(Commands, TempArena);

    BEGIN_BLOCK("OpenGLRenderCommands");
    OpenGLRenderCommands(Commands, &Prep, DrawRegion, WindowWidth, WindowHeight);        
    END_BLOCK();

    BEGIN_BLOCK("SwapBuffers");
//    SwapBuffers(DeviceContext);
    END_BLOCK();

    EndTemporaryMemory(TempMem);
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return(Result);
}

internal void
ToggleFullscreen(HWND Window)
{
    // NOTE(casey): This follows Raymond Chen's prescription
    // for fullscreen toggling, see:
    // http://blogs.msdn.com/b/oldnewthing/archive/2010/04/12/9994016.aspx

    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if(Style & WS_OVERLAPPEDWINDOW)
    {
        MONITORINFO MonitorInfo = {sizeof(MonitorInfo)};
        if(GetWindowPlacement(Window, &GlobalWindowPosition) &&
           GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo))
        {
            SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                         MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                         MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                         MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else
    {
        SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): WINDOW CALLBACKS / INPUT PROCESSING
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// Retrieves and translates modifier keys
//
internal inline s32
Win32GetKeyMods(void)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    s32 mods = 0;

    if (GetKeyState(VK_SHIFT) & 0x8000)
        mods |= WIN32_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        mods |= WIN32_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)
        mods |= WIN32_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
        mods |= WIN32_MOD_SUPER;
    if (GetKeyState(VK_CAPITAL) & 1)
        mods |= WIN32_MOD_CAPS_LOCK;
    if (GetKeyState(VK_NUMLOCK) & 1)
        mods |= WIN32_MOD_NUM_LOCK;

    return mods;
}

// Create key code translation tables
//
internal void
Win32CreateKeyTables(win32_state *State)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    s16 scancode;

    memset(State->Keycodes, -1, sizeof(State->Keycodes));
    memset(State->Scancodes, -1, sizeof(State->Scancodes));

    State->Keycodes[0x00B] = WIN32_KEY_0;
    State->Keycodes[0x002] = WIN32_KEY_1;
    State->Keycodes[0x003] = WIN32_KEY_2;
    State->Keycodes[0x004] = WIN32_KEY_3;
    State->Keycodes[0x005] = WIN32_KEY_4;
    State->Keycodes[0x006] = WIN32_KEY_5;
    State->Keycodes[0x007] = WIN32_KEY_6;
    State->Keycodes[0x008] = WIN32_KEY_7;
    State->Keycodes[0x009] = WIN32_KEY_8;
    State->Keycodes[0x00A] = WIN32_KEY_9;
    State->Keycodes[0x01E] = WIN32_KEY_A;
    State->Keycodes[0x030] = WIN32_KEY_B;
    State->Keycodes[0x02E] = WIN32_KEY_C;
    State->Keycodes[0x020] = WIN32_KEY_D;
    State->Keycodes[0x012] = WIN32_KEY_E;
    State->Keycodes[0x021] = WIN32_KEY_F;
    State->Keycodes[0x022] = WIN32_KEY_G;
    State->Keycodes[0x023] = WIN32_KEY_H;
    State->Keycodes[0x017] = WIN32_KEY_I;
    State->Keycodes[0x024] = WIN32_KEY_J;
    State->Keycodes[0x025] = WIN32_KEY_K;
    State->Keycodes[0x026] = WIN32_KEY_L;
    State->Keycodes[0x032] = WIN32_KEY_M;
    State->Keycodes[0x031] = WIN32_KEY_N;
    State->Keycodes[0x018] = WIN32_KEY_O;
    State->Keycodes[0x019] = WIN32_KEY_P;
    State->Keycodes[0x010] = WIN32_KEY_Q;
    State->Keycodes[0x013] = WIN32_KEY_R;
    State->Keycodes[0x01F] = WIN32_KEY_S;
    State->Keycodes[0x014] = WIN32_KEY_T;
    State->Keycodes[0x016] = WIN32_KEY_U;
    State->Keycodes[0x02F] = WIN32_KEY_V;
    State->Keycodes[0x011] = WIN32_KEY_W;
    State->Keycodes[0x02D] = WIN32_KEY_X;
    State->Keycodes[0x015] = WIN32_KEY_Y;
    State->Keycodes[0x02C] = WIN32_KEY_Z;

    State->Keycodes[0x028] = WIN32_KEY_APOSTROPHE;
    State->Keycodes[0x02B] = WIN32_KEY_BACKSLASH;
    State->Keycodes[0x033] = WIN32_KEY_COMMA;
    State->Keycodes[0x00D] = WIN32_KEY_EQUAL;
    State->Keycodes[0x029] = WIN32_KEY_GRAVE_ACCENT;
    State->Keycodes[0x01A] = WIN32_KEY_LEFT_BRACKET;
    State->Keycodes[0x00C] = WIN32_KEY_MINUS;
    State->Keycodes[0x034] = WIN32_KEY_PERIOD;
    State->Keycodes[0x01B] = WIN32_KEY_RIGHT_BRACKET;
    State->Keycodes[0x027] = WIN32_KEY_SEMICOLON;
    State->Keycodes[0x035] = WIN32_KEY_SLASH;
    State->Keycodes[0x056] = WIN32_KEY_WORLD_2;

    State->Keycodes[0x00E] = WIN32_KEY_BACKSPACE;
    State->Keycodes[0x153] = WIN32_KEY_DELETE;
    State->Keycodes[0x14F] = WIN32_KEY_END;
    State->Keycodes[0x01C] = WIN32_KEY_ENTER;
    State->Keycodes[0x001] = WIN32_KEY_ESCAPE;
    State->Keycodes[0x147] = WIN32_KEY_HOME;
    State->Keycodes[0x152] = WIN32_KEY_INSERT;
    State->Keycodes[0x15D] = WIN32_KEY_MENU;
    State->Keycodes[0x151] = WIN32_KEY_PAGE_DOWN;
    State->Keycodes[0x149] = WIN32_KEY_PAGE_UP;
    State->Keycodes[0x045] = WIN32_KEY_PAUSE;
    State->Keycodes[0x039] = WIN32_KEY_SPACE;
    State->Keycodes[0x00F] = WIN32_KEY_TAB;
    State->Keycodes[0x03A] = WIN32_KEY_CAPS_LOCK;
    State->Keycodes[0x145] = WIN32_KEY_NUM_LOCK;
    State->Keycodes[0x046] = WIN32_KEY_SCROLL_LOCK;
    State->Keycodes[0x03B] = WIN32_KEY_F1;
    State->Keycodes[0x03C] = WIN32_KEY_F2;
    State->Keycodes[0x03D] = WIN32_KEY_F3;
    State->Keycodes[0x03E] = WIN32_KEY_F4;
    State->Keycodes[0x03F] = WIN32_KEY_F5;
    State->Keycodes[0x040] = WIN32_KEY_F6;
    State->Keycodes[0x041] = WIN32_KEY_F7;
    State->Keycodes[0x042] = WIN32_KEY_F8;
    State->Keycodes[0x043] = WIN32_KEY_F9;
    State->Keycodes[0x044] = WIN32_KEY_F10;
    State->Keycodes[0x057] = WIN32_KEY_F11;
    State->Keycodes[0x058] = WIN32_KEY_F12;
    State->Keycodes[0x064] = WIN32_KEY_F13;
    State->Keycodes[0x065] = WIN32_KEY_F14;
    State->Keycodes[0x066] = WIN32_KEY_F15;
    State->Keycodes[0x067] = WIN32_KEY_F16;
    State->Keycodes[0x068] = WIN32_KEY_F17;
    State->Keycodes[0x069] = WIN32_KEY_F18;
    State->Keycodes[0x06A] = WIN32_KEY_F19;
    State->Keycodes[0x06B] = WIN32_KEY_F20;
    State->Keycodes[0x06C] = WIN32_KEY_F21;
    State->Keycodes[0x06D] = WIN32_KEY_F22;
    State->Keycodes[0x06E] = WIN32_KEY_F23;
    State->Keycodes[0x076] = WIN32_KEY_F24;
    State->Keycodes[0x038] = WIN32_KEY_LEFT_ALT;
    State->Keycodes[0x01D] = WIN32_KEY_LEFT_CONTROL;
    State->Keycodes[0x02A] = WIN32_KEY_LEFT_SHIFT;
    State->Keycodes[0x15B] = WIN32_KEY_LEFT_SUPER;
    State->Keycodes[0x137] = WIN32_KEY_PRINT_SCREEN;
    State->Keycodes[0x138] = WIN32_KEY_RIGHT_ALT;
    State->Keycodes[0x11D] = WIN32_KEY_RIGHT_CONTROL;
    State->Keycodes[0x036] = WIN32_KEY_RIGHT_SHIFT;
    State->Keycodes[0x15C] = WIN32_KEY_RIGHT_SUPER;
    State->Keycodes[0x150] = WIN32_KEY_DOWN;
    State->Keycodes[0x14B] = WIN32_KEY_LEFT;
    State->Keycodes[0x14D] = WIN32_KEY_RIGHT;
    State->Keycodes[0x148] = WIN32_KEY_UP;

    State->Keycodes[0x052] = WIN32_KEY_KP_0;
    State->Keycodes[0x04F] = WIN32_KEY_KP_1;
    State->Keycodes[0x050] = WIN32_KEY_KP_2;
    State->Keycodes[0x051] = WIN32_KEY_KP_3;
    State->Keycodes[0x04B] = WIN32_KEY_KP_4;
    State->Keycodes[0x04C] = WIN32_KEY_KP_5;
    State->Keycodes[0x04D] = WIN32_KEY_KP_6;
    State->Keycodes[0x047] = WIN32_KEY_KP_7;
    State->Keycodes[0x048] = WIN32_KEY_KP_8;
    State->Keycodes[0x049] = WIN32_KEY_KP_9;
    State->Keycodes[0x04E] = WIN32_KEY_KP_ADD;
    State->Keycodes[0x053] = WIN32_KEY_KP_DECIMAL;
    State->Keycodes[0x135] = WIN32_KEY_KP_DIVIDE;
    State->Keycodes[0x11C] = WIN32_KEY_KP_ENTER;
    State->Keycodes[0x059] = WIN32_KEY_KP_EQUAL;
    State->Keycodes[0x037] = WIN32_KEY_KP_MULTIPLY;
    State->Keycodes[0x04A] = WIN32_KEY_KP_SUBTRACT;

    for (scancode = 0;  scancode < 512;  scancode++)
    {
        if (State->Keycodes[scancode] > 0)
            State->Scancodes[State->Keycodes[scancode]] = scancode;
    }
}

internal LRESULT CALLBACK
Win32MainWindowCallback(HWND Window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{       
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_CLOSE:
        {
            // TODO(casey): Handle this with a message to the user?
            GlobalRunning = false;
        } break;

        case WM_WINDOWPOSCHANGING:
        {
            if(GetKeyState(VK_SHIFT) & 0x8000)
            {
                WINDOWPOS *NewPos = (WINDOWPOS *)LParam;

                RECT WindowRect;
                RECT ClientRect;
                GetWindowRect(Window, &WindowRect);
                GetClientRect(Window, &ClientRect);

                s32 ClientWidth = (ClientRect.right - ClientRect.left);
                s32 ClientHeight = (ClientRect.bottom - ClientRect.top);
                s32 WidthAdd = ((WindowRect.right - WindowRect.left) - ClientWidth);
                s32 HeightAdd = ((WindowRect.bottom - WindowRect.top) - ClientHeight);

                s32 RenderWidth = GlobalFramebufferDim.Width;
                s32 RenderHeight = GlobalFramebufferDim.Height;

                s32 SugX = NewPos->cx;
                s32 SugY = NewPos->cy;

                s32 NewCx = (RenderWidth * (NewPos->cy - HeightAdd)) / RenderHeight;
                s32 NewCy = (RenderHeight * (NewPos->cx - WidthAdd)) / RenderWidth;

                if(AbsoluteValue((r32)(NewPos->cx - NewCx)) < AbsoluteValue((r32)(NewPos->cy - NewCy)))
                {
                    NewPos->cx = NewCx + WidthAdd;
                }
                else
                {
                    NewPos->cy = NewCy + HeightAdd;
                }

                Result = DefWindowProcA(Window, Message, WParam, LParam);
            }
        } break;
        
        case WM_SETCURSOR:
        {
            if(DEBUGGlobalShowCursor)
            {
                Result = DefWindowProcA(Window, Message, WParam, LParam);
            }
            else
            {
                SetCursor(0);
            }
        } break;

        case WM_ACTIVATEAPP:
        {
            GlobalAppIsActive = (b32)WParam;
        } break;

        case WM_DESTROY:
        {
            // TODO(casey): Handle this as an error - recreate window?
            GlobalRunning = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            Assert(!"Keyboard input came in through a non-dispatch message!");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            Result = DefWindowProcA(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

internal void
Win32ProcessKeyboardMessage(engine_button_state *NewState, bool32 IsDown)
{
    if(NewState->EndedDown != IsDown)
    {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}

// Notifies shared code of a scroll event
//
internal inline void
Win32InputScroll(win32_state *State, double xoffset, double yoffset)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    Assert(xoffset > -FLT_MAX);
    Assert(xoffset < FLT_MAX);
    Assert(yoffset > -FLT_MAX);
    Assert(yoffset < FLT_MAX);

    Win32NkScrollCallback(&State->Main, xoffset, yoffset);
    Win32NkScrollCallback(&State->Debug, xoffset, yoffset);
}

// Notifies shared code of a Unicode codepoint input event
// The 'plain' parameter determines whether to emit a regular character event
//

internal inline void
Win32InputChar(win32_state *State, uint32_t codepoint, int mods, b32 plain)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    Assert(mods == (mods & WIN32_MOD_MASK));
    Assert(plain == 1 || plain == 0);

    if (codepoint < 32 || (codepoint > 126 && codepoint < 160))
        return;

    if (!State->lockKeyMods)
        mods &= ~(WIN32_MOD_CAPS_LOCK | WIN32_MOD_NUM_LOCK);

    if (plain)
    {
        Win32NkCharCallback(&State->Main, codepoint);
        Win32NkCharCallback(&State->Debug, codepoint);
    }
}

// Notifies shared code of a physical key event
//
internal inline void
Win32InputKey(win32_state *State, int key, int scancode, int action, int mods)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    Assert(key >= 0 || key == WIN32_KEY_UNKNOWN);
    Assert(key <= WIN32_KEY_LAST);
    Assert(action == WIN32_PRESS || action == WIN32_RELEASE);
    Assert(mods == (mods & WIN32_MOD_MASK));

    if (key >= 0 && key <= WIN32_KEY_LAST)
    {
        b32 repeated = WIN32_FALSE;

        if (action == WIN32_RELEASE && State->keys[key] == WIN32_RELEASE)
            return;

        if (action == WIN32_PRESS && State->keys[key] == WIN32_PRESS)
            repeated = WIN32_TRUE;

        State->keys[key] = (char) action;

        if (repeated)
            action = WIN32_REPEAT;
    }

    if (!State->lockKeyMods)
        mods &= ~(WIN32_MOD_CAPS_LOCK | WIN32_MOD_NUM_LOCK);

    Win32NkKeyCallback(&State->Main, key, scancode, action, mods);
    Win32NkKeyCallback(&State->Debug, key, scancode, action, mods);
}

// Notifies shared code of a mouse button click event
//
internal inline void
Win32InputMouseClick(win32_state *State, int button, int action, int mods)
{
    /*
      NOTE(paul): The implementation of this function is based on
      implementation in GLFW library
    */

    Assert(button >= 0);
    Assert(button <= WIN32_MOUSE_BUTTON_LAST);
    Assert(action == WIN32_PRESS || action == WIN32_RELEASE);
    Assert(mods == (mods & WIN32_MOD_MASK));

    if (button < 0 || button > WIN32_MOUSE_BUTTON_LAST)
        return;

    if (!State->lockKeyMods)
        mods &= ~(WIN32_MOD_CAPS_LOCK | WIN32_MOD_NUM_LOCK);

    State->MouseButtons[button] = (char) action;

    Win32NkMouseButtonCallback(&State->Main, State, button, action);
    Win32NkMouseButtonCallback(&State->Debug, State, button, action);
}

internal void
Win32ProcessPendingMessages(win32_state *State, engine_controller_input *KeyboardController, s16 *MouseRotated)
{
    MSG Message;
    for(;;)
    {
        BOOL GotMessage = FALSE;
        
        {
            TIMED_BLOCK("PeekMessage");
            GotMessage = PeekMessage(&Message, 0, 0, 0, PM_REMOVE);
        }
        
        if(!GotMessage)
        {
            break;
        }
        
        switch(Message.message)
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;

            case WM_MOUSEWHEEL:
            {
                *MouseRotated = (s16)(Message.wParam >> 16);
                Win32InputScroll(State, 0.0, (SHORT) HIWORD(Message.wParam) / (double) WHEEL_DELTA);
            } break;

            case WM_MOUSEHWHEEL:
            {
                // This message is only sent on Windows Vista and later
                // NOTE: The X-axis is inverted for consistency with macOS and X11
                Win32InputScroll(State, -((SHORT) HIWORD(Message.wParam) / (double) WHEEL_DELTA), 0.0);
            } break;

            case WM_CHAR:
            case WM_SYSCHAR:
            {
                if (Message.wParam >= 0xd800 && Message.wParam <= 0xdbff)
                    State->highSurrogate = (WCHAR) Message.wParam;
                else
                {
                    uint32_t codepoint = 0;

                    if (Message.wParam >= 0xdc00 && Message.wParam <= 0xdfff)
                    {
                        if (State->highSurrogate)
                        {
                            codepoint += (State->highSurrogate - 0xd800) << 10;
                            codepoint += (WCHAR) Message.wParam - 0xdc00;
                            codepoint += 0x10000;
                        }
                    }
                    else
                        codepoint = (WCHAR) Message.wParam;

                    State->highSurrogate = 0;
                    Win32InputChar(State, codepoint, Win32GetKeyMods(), Message.message != WM_SYSCHAR);
                }

            } break;

            case WM_LBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_XBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_RBUTTONUP:
            case WM_MBUTTONUP:
            case WM_XBUTTONUP:
            {
                int i, button, action;

                UINT uMsg = Message.message;
                if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONUP)
                    button = WIN32_MOUSE_BUTTON_LEFT;
                else if (uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP)
                    button = WIN32_MOUSE_BUTTON_RIGHT;
                else if (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP)
                    button = WIN32_MOUSE_BUTTON_MIDDLE;
                else if (GET_XBUTTON_WPARAM(Message.wParam) == XBUTTON1)
                    button = WIN32_MOUSE_BUTTON_4;
                else
                    button = WIN32_MOUSE_BUTTON_5;

                if (uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN ||
                    uMsg == WM_MBUTTONDOWN || uMsg == WM_XBUTTONDOWN)
                {
                    action = WIN32_PRESS;
                }
                else
                    action = WIN32_RELEASE;

                for (i = 0;  i <= WIN32_MOUSE_BUTTON_LAST;  i++)
                {
                    if (State->MouseButtons[i] == WIN32_PRESS)
                        break;
                }

                if (i > WIN32_MOUSE_BUTTON_LAST)
                    SetCapture(State->WindowHandle);

                Win32InputMouseClick(State, button, action, Win32GetKeyMods());

                for (i = 0;  i <= WIN32_MOUSE_BUTTON_LAST;  i++)
                {
                    if (State->MouseButtons[i] == WIN32_PRESS)
                        break;
                }

                if (i > WIN32_MOUSE_BUTTON_LAST)
                    ReleaseCapture();
            } break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                int key, scancode;
                const int action = (HIWORD(Message.lParam) & KF_UP) ? WIN32_RELEASE : WIN32_PRESS;
                const int mods = Win32GetKeyMods();

                scancode = (HIWORD(Message.lParam) & (KF_EXTENDED | 0xff));
                if (!scancode)
                {
                    // NOTE: Some synthetic key messages have a scancode of zero
                    // HACK: Map the virtual key back to a usable scancode
                    scancode = MapVirtualKeyW((UINT) Message.wParam, MAPVK_VK_TO_VSC);
                }

                // HACK: Alt+PrtSc has a different scancode than just PrtSc
                if (scancode == 0x54)
                    scancode = 0x137;

                // HACK: Ctrl+Pause has a different scancode than just Pause
                if (scancode == 0x146)
                    scancode = 0x45;

                // HACK: CJK IME sets the extended bit for right Shift
                if (scancode == 0x136)
                    scancode = 0x36;

                key = State->Keycodes[scancode];

                // The Ctrl keys require special handling
                if (Message.wParam == VK_CONTROL)
                {
                    if (HIWORD(Message.lParam) & KF_EXTENDED)
                    {
                        // Right side keys have the extended key bit set
                        key = WIN32_KEY_RIGHT_CONTROL;
                    }
                    else
                    {
                        // NOTE: Alt Gr sends Left Ctrl followed by Right Alt
                        // HACK: We only want one event for Alt Gr, so if we detect
                        //       this sequence we discard this Left Ctrl message now
                        //       and later report Right Alt normally
                        MSG next;
                        const DWORD time = GetMessageTime();

                        if (PeekMessageW(&next, NULL, 0, 0, PM_NOREMOVE))
                        {
                            if (next.message == WM_KEYDOWN ||
                                next.message == WM_SYSKEYDOWN ||
                                next.message == WM_KEYUP ||
                                next.message == WM_SYSKEYUP)
                            {
                                if (next.wParam == VK_MENU &&
                                    (HIWORD(next.lParam) & KF_EXTENDED) &&
                                    next.time == time)
                                {
                                    // Next message is Right Alt down so discard this
                                    break;
                                }
                            }
                        }

                        // This is a regular Left Ctrl message
                        key = WIN32_KEY_LEFT_CONTROL;
                    }
                }
                else if (Message.wParam == VK_PROCESSKEY)
                {
                    // IME notifies that keys have been filtered by setting the
                    // virtual key-code to VK_PROCESSKEY
                    break;
                }

                if (action == WIN32_RELEASE && Message.wParam == VK_SHIFT)
                {
                    // HACK: Release both Shift keys on Shift up event, as when both
                    //       are pressed the first release does not emit any event
                    // NOTE: The other half of this is in _glfwPollEventsWin32
                    Win32InputKey(State, WIN32_KEY_LEFT_SHIFT, scancode, action, mods);
                    Win32InputKey(State, WIN32_KEY_RIGHT_SHIFT, scancode, action, mods);
                }
                else if (Message.wParam == VK_SNAPSHOT)
                {
                    // HACK: Key down is not reported for the Print Screen key
                    Win32InputKey(State, key, scancode, WIN32_PRESS, mods);
                    Win32InputKey(State, key, scancode, WIN32_RELEASE, mods);
                }
                else
                    Win32InputKey(State, key, scancode, action, mods);

#if 1
                uint32 VKCode = (uint32)Message.wParam;

                // NOTE(casey): Since we are comparing WasDown to IsDown,
                // we MUST use == and != to convert these bit tests to actual
                // 0 or 1 values.
                bool32 WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool32 IsDown = ((Message.lParam & (1 << 31)) == 0);
                if(WasDown != IsDown)
                {
                    if(VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if(VKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if(VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if(VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if(VKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if(VKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if(VKCode == 'F')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Fill, IsDown);
                    }
                    else if(VKCode == 'U')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Undo, IsDown);
                    }
                    else if(VKCode == '1')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->FirstMode, IsDown);
                    }
                    else if(VKCode == '2')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->SecondMode, IsDown);
                    }
                    else if(VKCode == '3')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ThirdMode, IsDown);
                    }
                    else if(VKCode == '4')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->PlayMusic, IsDown);
                    }
                    else if(VKCode == '5')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->TerminateSound, IsDown);
                    }
                    else if(VKCode == 'H')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ShowProfiler, IsDown);
                    }
                    else if(VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if(VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if(VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if(VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if(VKCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
                    else if(VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
#if EDITOR_INTERNAL
                    else if(VKCode == 'P')
                    {
                        if(IsDown)
                        {
                            GlobalPause = !GlobalPause;
                        }
                    }
                    else if(VKCode == 'L')
                    {
                    }
#endif
                    if(IsDown)
                    {
                        bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                        if((VKCode == VK_F4) && AltKeyWasDown)
                        {
                            GlobalRunning = false;
                        }
                        else if((VKCode == VK_RETURN) && AltKeyWasDown)
                        {
                            if(Message.hwnd)
                            {
                                ToggleFullscreen(Message.hwnd);
                            }
                        }
                    }
                }
#endif
                TranslateMessage(&Message);

            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessage(&Message);
            } break;
        }
    }
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): MULTITHREADING & QUEUES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
Win32AddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data)
{
    // TODO(casey): Switch to InterlockedCompareExchange eventually
    // so that any thread can add?
    uint32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
    platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
    Entry->Callback = Callback;
    Entry->Data = Data;
    ++Queue->CompletionGoal;
    _WriteBarrier();
    Queue->NextEntryToWrite = NewNextEntryToWrite;
    ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal bool32
Win32DoNextWorkQueueEntry(platform_work_queue *Queue)
{
    bool32 WeShouldSleep = false;

    uint32 OriginalNextEntryToRead = Queue->NextEntryToRead;
    uint32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
    if(OriginalNextEntryToRead != Queue->NextEntryToWrite)
    {
        uint32 Index = InterlockedCompareExchange((LONG volatile *)&Queue->NextEntryToRead,
                                                  NewNextEntryToRead,
                                                  OriginalNextEntryToRead);
        if(Index == OriginalNextEntryToRead)
        {        
            platform_work_queue_entry Entry = Queue->Entries[Index];
            Entry.Callback(Queue, Entry.Data);
            InterlockedIncrement((LONG volatile *)&Queue->CompletionCount);
        }
    }
    else
    {
        WeShouldSleep = true;
    }

    return(WeShouldSleep);
}

internal void
Win32CompleteAllWork(platform_work_queue *Queue)
{
    while(Queue->CompletionGoal != Queue->CompletionCount)
    {
        Win32DoNextWorkQueueEntry(Queue);
    }

    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;
}

DWORD WINAPI
ThreadProc(LPVOID lpParameter)
{
    win32_thread_startup *Thread = (win32_thread_startup *)lpParameter;
    platform_work_queue *Queue = Thread->Queue;

    u32 TestThreadID = GetThreadID();
    Assert(TestThreadID == GetCurrentThreadId());

    for(;;)
    {
        if(Win32DoNextWorkQueueEntry(Queue))
        {
            WaitForSingleObjectEx(Queue->SemaphoreHandle, INFINITE, FALSE);
        }
    }
}

internal PLATFORM_WORK_QUEUE_CALLBACK(DoWorkerWork)
{
    wchar_t Buffer[256];
    wsprintfW(Buffer, L"Thread %u: %s\n", GetCurrentThreadId(), (char *)Data);
    OutputDebugStringW(Buffer);
}

internal void
Win32MakeQueue(platform_work_queue *Queue, uint32 ThreadCount, win32_thread_startup *Startups)
{
    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;

    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;

    uint32 InitialCount = 0;
    Queue->SemaphoreHandle = CreateSemaphoreEx(0,
                                               InitialCount,
                                               ThreadCount,
                                               0, 0, SEMAPHORE_ALL_ACCESS);
    for(uint32 ThreadIndex = 0;
        ThreadIndex < ThreadCount;
        ++ThreadIndex)
    {
        win32_thread_startup *Startup = Startups + ThreadIndex;
        Startup->Queue = Queue;

        DWORD ThreadID;
        HANDLE ThreadHandle = CreateThread(0, 0, ThreadProc, Startup, 0, &ThreadID);
        CloseHandle(ThreadHandle);
    }
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): WIN32 FILE API
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(Win32GetAllFilesOfTypeBegin)
{
    platform_file_group Result = {};

    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)VirtualAlloc(
        0, sizeof(win32_platform_file_group),
        MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Result.Platform = Win32FileGroup;

    wchar_t *WildCard = L"*.*";
    switch(Type)
    {
        case PlatformFileType_AssetFile:
        {
            WildCard = L"*.ssa";
        } break;

        case PlatformFileType_SavedEditorFile:
        {
            WildCard = L"*.hhs";
        } break;

        InvalidDefaultCase;
    }

    Result.FileCount = 0;

    WIN32_FIND_DATAW FindData;
    HANDLE FindHandle = FindFirstFileW(WildCard, &FindData);
    while(FindHandle != INVALID_HANDLE_VALUE)
    {
        ++Result.FileCount;

        if(!FindNextFileW(FindHandle, &FindData))
        {
            break;
        }
    }
    FindClose(FindHandle);

    Win32FileGroup->FindHandle = FindFirstFileW(WildCard, &Win32FileGroup->FindData);

    return(Result);
}

internal PLATFORM_GET_ALL_FILE_OF_TYPE_END(Win32GetAllFilesOfTypeEnd)
{
    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
    if(Win32FileGroup)
    {
        FindClose(Win32FileGroup->FindHandle);

        VirtualFree(Win32FileGroup, 0, MEM_RELEASE);
    }
}

internal PLATFORM_OPEN_FILE(Win32OpenNextFile)
{
    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
    platform_file_handle Result = {};

    if(Win32FileGroup->FindHandle != INVALID_HANDLE_VALUE)
    {    
        win32_platform_file_handle *Win32Handle = (win32_platform_file_handle *)VirtualAlloc(
            0, sizeof(win32_platform_file_handle),
            MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        Result.Platform = Win32Handle;

        if(Win32Handle)
        {
            wchar_t *FileName = Win32FileGroup->FindData.cFileName;
            Win32Handle->Win32Handle = CreateFileW(FileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
            Result.NoErrors = (Win32Handle->Win32Handle != INVALID_HANDLE_VALUE);
        }

        if(!FindNextFileW(Win32FileGroup->FindHandle, &Win32FileGroup->FindData))
        {
            FindClose(Win32FileGroup->FindHandle);
            Win32FileGroup->FindHandle = INVALID_HANDLE_VALUE;
        }
    }

    return(Result);
}

internal PLATFORM_FILE_ERROR(Win32FileError)
{
#if EDITOR_INTERNAL
    OutputDebugString("WIN32 FILE ERROR: ");
    OutputDebugString(Message);
    OutputDebugString("\n");
#endif

    Handle->NoErrors = false;
}

internal PLATFORM_READ_DATA_FROM_FILE(Win32ReadDataFromFile)
{
    if(PlatformNoFileErrors(Source))
    {
        win32_platform_file_handle *Handle = (win32_platform_file_handle *)Source->Platform;
        OVERLAPPED Overlapped = {};
        Overlapped.Offset = (u32)((Offset >> 0) & 0xFFFFFFFF);
        Overlapped.OffsetHigh = (u32)((Offset >> 32) & 0xFFFFFFFF);

        uint32 FileSize32 = SafeTruncateUInt64(Size);

        DWORD BytesRead;
        if(ReadFile(Handle->Win32Handle, Dest, FileSize32, &BytesRead, &Overlapped) &&
           (FileSize32 == BytesRead))
        {
            // NOTE(casey): File read succeeded!
        }
        else
        {
            Win32FileError(Source, "Read file failed.");
        }
    }
}

internal PLATFORM_LIST_FILES_IN_DIRECTORY(Win32ListFilesInDirectory)
{
    u32 FileCount = 0;

    WIN32_FIND_DATA Data;
    HANDLE FileHandle;

    char *WildCard = "*.*";
    switch(Type)
    {
        case PlatformFileType_AssetFile:
        {
            WildCard = "*.ssa";
        } break;

        case PlatformFileType_SavedEditorFile:
        {
            WildCard = "*.hhs";
        } break;

        case PlatformFileType_PNG:
        {
            WildCard = "*.png";
        } break;

        case PlatformFileType_BMP:
        {
            WildCard = "bmps\\*.bmp";
        } break;

        case PlatformFileType_SSBMP:
        {
            WildCard = "spritesheets\\*.bmp";
        } break;

        case PlatformFileType_TSBMP:
        {
            WildCard = "tilesets\\*.bmp";
        } break;

        case PlatformFileType_STBMP:
        {
            WildCard = "solid_tiles\\*.bmp";
        } break;

        case PlatformFileType_WAV:
        {
            WildCard = "wavs\\*.wav";
        } break;

        case PlatformFileType_TXT:
        {
            WildCard = "txts\\*.txt";
        } break;

        case PlatformFileType_TTF
            :
        {
            WildCard = "fonts\\*.ttf";
        } break;

        case PlatformFileType_BIN:
        {
            WildCard = "binaryfiles\\*.bin";
        } break;

        case PlatformFileType_SSWM:
        {
            WildCard = "sswms\\*.sswm";
        } break;

        InvalidDefaultCase;
    }

    FileHandle = FindFirstFile(WildCard, &Data);

    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        do
        {
            if(Dest && Arena)
            {
                Dest[FileCount] = PushString(Arena, Data.cFileName);
            }

            ++FileCount;
            
        } while(FindNextFile(FileHandle, &Data) != 0);
    }

    FindClose(FileHandle);

    return(FileCount);
}

PLATFORM_FREE_FILE_MEMORY(Win32PlatformFreeFileMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

PLATFORM_READ_ENTIRE_FILE(Win32PlatformReadEntireFile)
{
    read_file_result Result = {};

    b32 IsTXT = false;

    char *WildCard = "*.*";
    switch(Type)
    {
        case PlatformFileType_AssetFile:
        {
            WildCard = "*.ssa";
        } break;

        case PlatformFileType_SavedEditorFile:
        {
            WildCard = "*.hhs";
        } break;

        case PlatformFileType_PNG:
        {
            WildCard = "*.png";
        } break;

        case PlatformFileType_BMP:
        {
            WildCard = "bmps\\";
        } break;

        case PlatformFileType_SSBMP:
        {
            WildCard = "spritesheets\\";
        } break;

        case PlatformFileType_TSBMP:
        {
            WildCard = "tilesets\\";
        } break;

        case PlatformFileType_STBMP:
        {
            WildCard = "solid_tiles\\";
        } break;

        case PlatformFileType_WAV:
        {
            WildCard = "wavs\\";
        } break;

        case PlatformFileType_TXT:
        {
            WildCard = "txts\\";
            IsTXT = true;
        } break;

        case PlatformFileType_JSON:
        {
            WildCard = "jsons\\";
            IsTXT = true;
        } break;

        case PlatformFileType_TTF:
        {
            WildCard = "fonts\\";
        } break;

        case PlatformFileType_BIN:
        {
            WildCard = "binaryfiles\\";
        } break;

        case PlatformFileType_SSWM:
        {
            WildCard = "sswms\\";
        } break;

        InvalidDefaultCase;
    }

    char FilePath[WIN32_STATE_FILE_NAME_COUNT];
    FormatString(ArrayCount(FilePath), FilePath, "%s%s", WildCard, FileName);

    HANDLE FileHandle = CreateFileA(FilePath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            u32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart) + (IsTXT ? 1 : 0);
            u32 ReadSize32 = IsTXT ? (FileSize32 - 1) : FileSize32;
            Result.Contents = (Arena ? PushSize(Arena, FileSize32) :
                               VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE));
            if(Result.Contents)
            {
                DWORD BytesRead;
                if(ReadFile(FileHandle, Result.Contents, ReadSize32, &BytesRead, 0) &&
                   (ReadSize32 == BytesRead))
                {
                    // NOTE(casey): File read successfully
                    Result.Size = FileSize32;
                }
                else
                {                    
                    // TODO: Logging
                    Win32PlatformFreeFileMemory(Result.Contents);
                    Result.Contents = 0;
                }
            }
            else
            {
                // TODO: Logging
            }
        }
        else
        {
            // TODO: Logging
        }

        CloseHandle(FileHandle);
    }
    else
    {
        // TODO: Logging
    }

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

internal inline void
Win32InitPlatformAPI(engine_memory *Memory, platform_work_queue *HighPQ,
                     platform_work_queue *LowPQ)
{
    Memory->HighPriorityQueue = HighPQ;
    Memory->LowPriorityQueue = LowPQ;
    Memory->PlatformAPI.AddEntry = Win32AddEntry;
    Memory->PlatformAPI.CompleteAllWork = Win32CompleteAllWork;

    Memory->PlatformAPI.GetAllFilesOfTypeBegin = Win32GetAllFilesOfTypeBegin;
    Memory->PlatformAPI.GetAllFilesOfTypeEnd = Win32GetAllFilesOfTypeEnd;
    Memory->PlatformAPI.OpenNextFile = Win32OpenNextFile;
    Memory->PlatformAPI.ReadDataFromFile = Win32ReadDataFromFile;
    Memory->PlatformAPI.FileError = Win32FileError;
    Memory->PlatformAPI.ListFilesInDirectory = Win32ListFilesInDirectory;

    Memory->PlatformAPI.FreeFileMemory = Win32PlatformFreeFileMemory;
    Memory->PlatformAPI.ReadEntireFile = Win32PlatformReadEntireFile;

    Memory->PlatformAPI.AllocateMemory = Win32AllocateMemory;
    Memory->PlatformAPI.DeallocateMemory = Win32DeallocateMemory;
            
#if EDITOR_INTERNAL
    Memory->DebugTable = GlobalDebugTable;
    Memory->PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
    Memory->PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
#endif

    Win32SetUIPointers(&Memory->PlatformAPI.UI);
}

#if EDITOR_INTERNAL
global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;
#endif

int CALLBACK
WinMain(HINSTANCE Instance,
        HINSTANCE PrevInstance,
        LPSTR CommandLine,
        int ShowCode)
{
    DEBUGSetEventRecording(true);

    win32_state Win32State = {};
    Win32CreateKeyTables(&Win32State);

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32GetEXEFileName(&Win32State);

    wchar_t SourceEditorCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"engine.dll",
                              sizeof(SourceEditorCodeDLLFullPath), SourceEditorCodeDLLFullPath);
                          
    wchar_t TempEditorCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"engine_temp.dll",
                              sizeof(TempEditorCodeDLLFullPath), TempEditorCodeDLLFullPath);

    wchar_t EditorCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"lock.tmp",
                              sizeof(EditorCodeLockFullPath), EditorCodeLockFullPath);

    // NOTE(casey): Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);
    
#if EDITOR_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif

    // NOTE(paul): Set intitial dimentions 
    GlobalFramebufferDim.Width = GetSystemMetrics(SM_CXSCREEN);
    GlobalFramebufferDim.Height = GetSystemMetrics(SM_CYSCREEN);

    // NOTE(paul): Init window class 
    WNDCLASSA WindowClass = {};
    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
    WindowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    WindowClass.lpszClassName = "EditorWindowClass";

    if(RegisterClassA(&WindowClass))
    {
        HWND Window =
            CreateWindowExA(
                0, // WS_EX_TOPMOST|WS_EX_LAYERED,
                WindowClass.lpszClassName,
                "Editor",
                WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                0,
                0,
                Instance,
                0);
        if(Window)
        {
            Win32State.WindowHandle = Window;
            ToggleFullscreen(Window);

            // NOTE(paul): Init OpenGLRC
            HDC OpenGLDC = GetDC(Window);
            HGLRC OpenGLRC = 0;
            OpenGLRC = Win32InitOpenGL(OpenGLDC);

            // NOTE(paul): Init multithreading queues
            win32_thread_startup HighPriStartups[3] = {};
            platform_work_queue HighPriorityQueue = {};
            Win32MakeQueue(&HighPriorityQueue, ArrayCount(HighPriStartups), HighPriStartups);

            win32_thread_startup LowPriStartups[3] = {};
            platform_work_queue LowPriorityQueue = {};
            Win32MakeQueue(&LowPriorityQueue, ArrayCount(LowPriStartups), LowPriStartups);

            // NOTE(paul): Set fixed refresh rate
            f32 EditorUpdateHz = 60.0f;
            f32 TargetSecondsPerFrame = 1.0f / EditorUpdateHz;

            win32_sound_output SoundOutput = {};
            // TODO(casey): Make this like sixty seconds?
            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            // TODO(casey): Actually compute this variance and see
            // what the lowest reasonable value is.
            SoundOutput.SafetyBytes = (int)(((real32)SoundOutput.SamplesPerSecond*(real32)SoundOutput.BytesPerSample / EditorUpdateHz)/3.0f);
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearSoundBuffer(&SoundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            u32 MaxPossibleOverrun = 2*8*sizeof(u16);
            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize + MaxPossibleOverrun,
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if EDITOR_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif

            // NOTE(paul): Initialize Engine Memory and Platform API
            engine_memory EditorMemory = {};
            Win32InitPlatformAPI(&EditorMemory, &HighPriorityQueue, &LowPriorityQueue);
            Platform = EditorMemory.PlatformAPI;

            // NOTE(paul): Init render memory
            // TODO(casey): Decide what our pushbuffer size is!
            u32 PushBufferSize = Megabytes(64);
            void *PushBuffer = Win32AllocateMemory(PushBufferSize);

            u32 TextureOpCount = 1024;
            platform_texture_op_queue *TextureOpQueue = &EditorMemory.TextureOpQueue;
            TextureOpQueue->FirstFree = (texture_op *)Win32AllocateMemory(sizeof(texture_op)*TextureOpCount);

            for(u32 TextureOpIndex = 0;
                TextureOpIndex < (TextureOpCount - 1);
                ++TextureOpIndex)
            {
                texture_op *Op = TextureOpQueue->FirstFree + TextureOpIndex;
                Op->Next = TextureOpQueue->FirstFree + TextureOpIndex + 1;
            }

            // NOTE(paul): Init Input
            engine_input Input[2] = {};
            engine_input *NewInput = &Input[0];
            engine_input *OldInput = &Input[1];

            LARGE_INTEGER LastCounter = Win32GetWallClock();
            LARGE_INTEGER FlipWallClock = Win32GetWallClock();

            int DebugTimeMarkerIndex = 0;
            win32_debug_time_marker DebugTimeMarkers[30] = {0};

            DWORD AudioLatencyBytes = 0;
            real32 AudioLatencySeconds = 0;
            bool32 SoundIsValid = false;

            win32_engine_code Engine = Win32LoadEngineCode(SourceEditorCodeDLLFullPath,
                                                           TempEditorCodeDLLFullPath,
                                                           EditorCodeLockFullPath);
            DEBUGSetEventRecording(Engine.IsValid);

            ShowWindow(Window, SW_SHOW);

            memory_arena FrameTempArena = {};

            s32 UIBaseWidth = 1280;
            s32 UIBaseHeight = 720;
            nk_context *nk = Win32SetupNkContext(&Win32State, &Win32State.Main,
                                                 UIBaseWidth, UIBaseHeight);
            nk_context *debug_nk = Win32SetupNkContext(&Win32State, &Win32State.Debug,
                                                       UIBaseWidth, UIBaseHeight);
            nk_colorf bg = {};
            
            GlobalRunning = true;
            while(GlobalRunning)
            {
                // NOTE(paul): Init Render Commands and Handle Aspect Ratio
                editor_render_commands RenderCommands = RenderCommandStruct(
                    PushBufferSize, PushBuffer,
                    (u32)GlobalFramebufferDim.Width, (u32)GlobalFramebufferDim.Height);
//                    1280, 720);

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                rectangle2i DrawRegion = AspectRatioFit(RenderCommands.Width, RenderCommands.Height,
                                                        Dimension.Width, Dimension.Height);
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): Input Processing
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
                BEGIN_BLOCK("Input Processing");

                // NOTE(paul): Set Delta Time
                NewInput->dtForFrame = TargetSecondsPerFrame;
                        
                // TODO(casey): Zeroing macro
                // TODO(casey): We can't zero everything because the up/down state will
                // be wrong!!!
                engine_controller_input *OldKeyboardController = GetController(OldInput, 0);
                engine_controller_input *NewKeyboardController = GetController(NewInput, 0);
                *NewKeyboardController = {};
                NewKeyboardController->IsConnected = true;

                s16 MouseZ = 0;
                for(int ButtonIndex = 0;
                    ButtonIndex < ArrayCount(NewKeyboardController->Buttons);
                    ++ButtonIndex)
                {
                    NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                        OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                }

                {
                    TIMED_BLOCK("Win32 Message Processing");
                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController, &MouseZ);
                }

//                if(!GlobalPause && GlobalAppIsActive)
                {
                    {
                        TIMED_BLOCK("Mouse Position");

                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP);
                        r32 MouseX = (r32)MouseP.x;
                        r32 MouseY = (r32)((Dimension.Height - 1) - MouseP.y);
                        NewInput->MouseZ = MouseZ / 120;

                        r32 MouseU = Clamp01MapToRange((r32)DrawRegion.MinX, MouseX, (r32)DrawRegion.MaxX);
                        r32 MouseV = Clamp01MapToRange((r32)DrawRegion.MinY, MouseY, (r32)DrawRegion.MaxY);
                            
                        NewInput->MouseX = (r32)RenderCommands.Width*MouseU;
                        NewInput->MouseY = (r32)RenderCommands.Height*MouseV;

                        NewInput->ShiftDown = (GetKeyState(VK_SHIFT) & (1 << 15));
                        NewInput->AltDown = (GetKeyState(VK_MENU) & (1 << 15));
                        NewInput->ControlDown = (GetKeyState(VK_CONTROL) & (1 << 15));
                    }

                    {
                        TIMED_BLOCK("Keyboard Processing");

                        DWORD WinButtonID[PlatformMouseButton_Count] =
                            {
                                VK_LBUTTON,
                                VK_MBUTTON,
                                VK_RBUTTON,
                                VK_XBUTTON1,
                                VK_XBUTTON2,
                            };

                        for(u32 ButtonIndex = 0;
                            ButtonIndex < PlatformMouseButton_Count;
                            ++ButtonIndex)
                        {
                            NewInput->MouseButtons[ButtonIndex] = OldInput->MouseButtons[ButtonIndex];
                            NewInput->MouseButtons[ButtonIndex].HalfTransitionCount = 0;
                            Win32ProcessKeyboardMessage(&NewInput->MouseButtons[ButtonIndex],
                                                        GetKeyState(WinButtonID[ButtonIndex]) & (1 << 15));
                        }
                    }
                }

                END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): Engine Update
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

                Win32NkUpdateInputs(&Win32State, &Win32State.Main,
                                    UIBaseWidth, UIBaseHeight,
                                    DrawRegion,
                                    TargetSecondsPerFrame);
#if EDITOR_INTERNAL
                Win32NkUpdateInputs(&Win32State, &Win32State.Debug,
                                    UIBaseWidth, UIBaseHeight,
                                    DrawRegion,
                                    TargetSecondsPerFrame);
#endif
                BEGIN_BLOCK("Engine Update");
                if(!GlobalPause)
                {
                    if(Engine.UpdateAndRender)
                    {
                        Engine.UpdateAndRender(nk, &EditorMemory, NewInput, &RenderCommands);
                        if(NewInput->QuitRequested)
                        {
                            GlobalRunning = false;
                        }
                    }
                    else
                    {
                        // TODO: Logging
                    }
                }
                
                END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): Audio Update
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
                BEGIN_BLOCK("Audio Update");

                LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);

                DWORD PlayCursor;
                DWORD WriteCursor;
                if(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                {
                    /* NOTE(casey):

                       Here is how sound output computation works.

                       We define a safety value that is the number
                       of samples we think our editor update loop
                       may vary by (let's say up to 2ms)

                       When we wake up to write audio, we will look
                       and see what the play cursor position is and we
                       will forecast ahead where we think the play
                       cursor will be on the next frame boundary.

                       We will then look to see if the write cursor is
                       before that by at least our safety value.  If
                       it is, the target fill position is that frame
                       boundary plus one frame.  This gives us perfect
                       audio sync in the case of a card that has low
                       enough latency.

                       If the write cursor is _after_ that safety
                       margin, then we assume we can never sync the
                       audio perfectly, so we will write one frame's
                       worth of audio plus the safety margin's worth
                       of guard samples.
                    */
                    if(!SoundIsValid)
                    {
                        SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                        SoundIsValid = true;
                    }

                    DWORD ByteToLock = ((SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) %
                                        SoundOutput.SecondaryBufferSize);

                    DWORD ExpectedSoundBytesPerFrame =
                        (int)((real32)(SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample) /
                              EditorUpdateHz);
                    real32 SecondsLeftUntilFlip = (TargetSecondsPerFrame - FromBeginToAudioSeconds);
                    DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip/TargetSecondsPerFrame)*(real32)ExpectedSoundBytesPerFrame);

                    DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

                    DWORD SafeWriteCursor = WriteCursor;
                    if(SafeWriteCursor < PlayCursor)
                    {
                        SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                    }
                    Assert(SafeWriteCursor >= PlayCursor);
                    SafeWriteCursor += SoundOutput.SafetyBytes;

                    bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);                        

                    DWORD TargetCursor = 0;
                    if(AudioCardIsLowLatency)
                    {
                        TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                    }
                    else
                    {
                        TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame +
                                        SoundOutput.SafetyBytes);
                    }
                    TargetCursor = (TargetCursor % SoundOutput.SecondaryBufferSize);

                    DWORD BytesToWrite = 0;
                    if(ByteToLock > TargetCursor)
                    {
                        BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                        BytesToWrite += TargetCursor;
                    }
                    else
                    {
                        BytesToWrite = TargetCursor - ByteToLock;
                    }

                    engine_sound_output_buffer SoundBuffer = {};
                    SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                    SoundBuffer.SampleCount = Align8(BytesToWrite / SoundOutput.BytesPerSample);
                    BytesToWrite = SoundBuffer.SampleCount*SoundOutput.BytesPerSample;
                    SoundBuffer.Samples = Samples;
                    if(Engine.GetSoundSamples)
                    {
                        Engine.GetSoundSamples(&EditorMemory, &SoundBuffer);
                    }

                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                }
                else
                {
                    SoundIsValid = false;
                }

                END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
                
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): Debug Collation
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#if EDITOR_INTERNAL
                BEGIN_BLOCK("Debug Collation");
                    
                FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceEditorCodeDLLFullPath);
                b32 ExecutableNeedsToBeReloaded = 
                    (CompareFileTime(&NewDLLWriteTime, &Engine.DLLLastWriteTime) != 0);

                EditorMemory.ExecutableReloaded = false;
                if(ExecutableNeedsToBeReloaded)
                {
                    Win32CompleteAllWork(&HighPriorityQueue);
                    Win32CompleteAllWork(&LowPriorityQueue);
                    DEBUGSetEventRecording(false);
                }
                    
                if(Engine.DEBUGFrameEnd)
                {
                    Engine.DEBUGFrameEnd(debug_nk, &EditorMemory, NewInput, &RenderCommands);
                }
                    
                if(ExecutableNeedsToBeReloaded)
                {
                    Win32UnloadEngineCode(&Engine);
                    for(u32 LoadTryIndex = 0;
                        !Engine.IsValid && (LoadTryIndex < 100);
                        ++LoadTryIndex)
                    {
                        Engine = Win32LoadEngineCode(SourceEditorCodeDLLFullPath,
                                                     TempEditorCodeDLLFullPath,
                                                     EditorCodeLockFullPath);
                        Sleep(100);
                    }
                        
                    EditorMemory.ExecutableReloaded = true;
                    DEBUGSetEventRecording(Engine.IsValid);
                }

                    
                END_BLOCK();
#endif
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): Frame Display
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
                
                BEGIN_BLOCK("Frame Display");

                BeginTicketMutex(&TextureOpQueue->Mutex);
                texture_op *FirstTextureOp = TextureOpQueue->First;
                texture_op *LastTextureOp = TextureOpQueue->Last;
                TextureOpQueue->First = 0;
                TextureOpQueue->Last = 0;
                EndTicketMutex(&TextureOpQueue->Mutex);

                if(FirstTextureOp)
                {
                    Assert(LastTextureOp);
                    OpenGLManageTextures(FirstTextureOp);

                    BeginTicketMutex(&TextureOpQueue->Mutex);
                    LastTextureOp->Next = TextureOpQueue->FirstFree;
                    TextureOpQueue->FirstFree = FirstTextureOp;
                    EndTicketMutex(&TextureOpQueue->Mutex);
                }

                HDC DeviceContext = GetDC(Window);
                Win32DisplayBufferInWindow(&HighPriorityQueue, &RenderCommands, DeviceContext,
                                           DrawRegion, Dimension.Width, Dimension.Height, &FrameTempArena);
                NKOpenGLRenderCommands(&Win32State.Main, DrawRegion, NK_ANTI_ALIASING_ON);
#if EDITOR_INTERNAL
                NKOpenGLRenderCommands(&Win32State.Debug, DrawRegion, NK_ANTI_ALIASING_ON);
#endif
                SwapBuffers(DeviceContext);
                ReleaseDC(Window, DeviceContext);


                END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

                // NOTE(paul): Swap Inputs
                FlipWallClock = Win32GetWallClock();

                engine_input *Temp = NewInput;
                NewInput = OldInput;
                OldInput = Temp;

#if 1
                BEGIN_BLOCK("FramerateWait");

                if(!GlobalPause)
                {
                    LARGE_INTEGER WorkCounter = Win32GetWallClock();
                    real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);

                    // TODO(casey): NOT TESTED YET!  PROBABLY BUGGY!!!!!
                    real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                    if(SecondsElapsedForFrame < TargetSecondsPerFrame)
                    {                        
                        if(SleepIsGranular)
                        {
                            DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame -
                                                               SecondsElapsedForFrame));
                            if(SleepMS > 0)
                            {
                                Sleep(SleepMS);
                            }
                        }

                        real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                                   Win32GetWallClock());
                        if(TestSecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            // TODO: Logging
                        }

                        while(SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {                            
                            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter,
                                                                            Win32GetWallClock());
                        }
                    }
                    else
                    {
                    }
                }

                END_BLOCK();
#endif
                // NOTE(paul): Record frame time
                LARGE_INTEGER EndCounter = Win32GetWallClock();                    
                FRAME_MARKER(Win32GetSecondsElapsed(LastCounter, EndCounter));
                LastCounter = EndCounter;
            }
        }
        else
        {
//            PlatformWriteLogFile(L"Error: Window is undefined", __FILE__, __LINE__);
        }
    }
    else
    {
//        PlatformWriteLogFile(L"Error: Unable to create WindowClass", __FILE__, __LINE__);
    }

    return(0);
}

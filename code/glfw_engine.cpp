/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice:  $
   ======================================================================== */

/*
  NOTE(pvlso): Cross platform layer on top of GLFW (window, input, clipboard,
  OpenGL context). Everything else the engine needs from the OS goes through
  platform_services.cpp, which only talks to the OS through platform_os.h.

  Unity build:
    platform_os.h         - what every OS has to provide
    linux_os.cpp          - Linux implementation
    win32_os.cpp          - Windows implementation
    platform_services.cpp - the platform_api handed to the engine (files, work queues, code loading)
    platform_sound.cpp    - sound output through miniaudio
    glfw_engine.cpp       - window, input, nuklear, main loop
*/

#include "engine_platform.h"
#include "engine_shared.h"

#include <stdio.h>

#if defined(_WIN32)
#include "win32_os.h"
#else
#include "linux_os.h"
#endif
#include "platform_os.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#include "miniaudio/miniaudio.h"

#include "glew/glew.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "platform_nuklear.h"
#include "platform_services.h"
#include "platform_sound.h"
#include "glfw_engine.h"

#if defined(_WIN32)
#include "win32_os.cpp"
#else
#include "linux_os.cpp"
#endif
#include "platform_services.cpp"
#include "platform_sound.cpp"

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): GLOBAL VARIABLES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
platform_api Platform;

global_variable b32 GlobalRunning;
global_variable b32 GlobalPause;
global_variable b32 GlobalAppIsActive;
global_variable u64 GlobalPerfCountFrequency;

global_variable s32 GlobalFramebufferWidth;
global_variable s32 GlobalFramebufferHeight;
global_variable platform_sound_output GlobalSoundOutput;

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
// NOTE(pvlso): TIME
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline u64
GLFWGetWallClock(void)
{
    u64 Result = glfwGetTimerValue();
    return(Result);
}

inline r32
GLFWGetSecondsElapsed(u64 Start, u64 End)
{
    r32 Result = (r32)((r64)(End - Start) / (r64)GlobalPerfCountFrequency);
    return(Result);
}

inline f32
GLFWGetTime(void)
{
    f32 Result = (f32)glfwGetTime();
    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): NUKLEAR CALLBACKS & Setup
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// NOTE(pvlso): GLFW reports the cursor in screen coordinates, which may differ from
// framebuffer pixels on HiDPI displays. Everything else here works in pixels.
internal void
GLFWGetCursorPosInPixels(glfw_window *Window, r64 *X, r64 *Y)
{
    r64 CursorX, CursorY;
    glfwGetCursorPos(Window->Handle, &CursorX, &CursorY);

    s32 WindowWidth, WindowHeight, FramebufferWidth, FramebufferHeight;
    glfwGetWindowSize(Window->Handle, &WindowWidth, &WindowHeight);
    glfwGetFramebufferSize(Window->Handle, &FramebufferWidth, &FramebufferHeight);

    *X = CursorX;
    *Y = CursorY;
    if((WindowWidth > 0) && (WindowHeight > 0))
    {
        *X = CursorX*(r64)FramebufferWidth/(r64)WindowWidth;
        *Y = CursorY*(r64)FramebufferHeight/(r64)WindowHeight;
    }
}

internal void
GLFWSetCursorPosInPixels(glfw_window *Window, r64 X, r64 Y)
{
    s32 WindowWidth, WindowHeight, FramebufferWidth, FramebufferHeight;
    glfwGetWindowSize(Window->Handle, &WindowWidth, &WindowHeight);
    glfwGetFramebufferSize(Window->Handle, &FramebufferWidth, &FramebufferHeight);

    if((FramebufferWidth > 0) && (FramebufferHeight > 0))
    {
        X = X*(r64)WindowWidth/(r64)FramebufferWidth;
        Y = Y*(r64)WindowHeight/(r64)FramebufferHeight;
    }

    glfwSetCursorPos(Window->Handle, X, Y);
}

internal inline void
GLFWNkScrollCallback(nk_platform *NkGLFW, double xoff, double yoff)
{
    NkGLFW->scroll.x += (float)xoff;
    NkGLFW->scroll.y += (float)yoff;
}

internal inline void
GLFWNkMouseButtonCallback(glfw_window *Window, int button, int action)
{
    /*
      NOTE(pvlso): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    nk_platform *NkGLFW = &Window->Nk;

    double x, y;
    if(button != GLFW_MOUSE_BUTTON_LEFT)
        return;

    GLFWGetCursorPosInPixels(Window, &x, &y);
    if(action == GLFW_PRESS)
    {
        double dt = GLFWGetTime() - NkGLFW->last_button_click;
        if((dt > NK_GLFW_DOUBLE_CLICK_LO) && (dt < NK_GLFW_DOUBLE_CLICK_HI))
        {
            NkGLFW->is_double_click_down = nk_true;
            NkGLFW->double_click_pos = nk_vec2((float)x, (float)y);
        }

        NkGLFW->last_button_click = GLFWGetTime();
    }
    else
        NkGLFW->is_double_click_down = nk_false;
}

internal inline void
GLFWNkCharCallback(nk_platform *NkGLFW, unsigned int codepoint)
{
    if (NkGLFW->text_len < NK_PLATFORM_TEXT_MAX)
        NkGLFW->text[NkGLFW->text_len++] = codepoint;
}

internal inline void
GLFWNkKeyCallback(nk_platform *NkGLFW, int key, int action)
{
    /*
      NOTE(pvlso): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    /*
     * convert GLFW_REPEAT to down (technically GLFW_RELEASE, GLFW_PRESS, GLFW_REPEAT are
     * already 0, 1, 2 but just to be clearer)
     */
    nk_char a = (nk_char)((action == GLFW_RELEASE) ? nk_false : nk_true);

    switch (key) {
        case GLFW_KEY_DELETE:    NkGLFW->key_events[NK_KEY_DEL] = a; break;
        case GLFW_KEY_TAB:       NkGLFW->key_events[NK_KEY_TAB] = a; break;
        case GLFW_KEY_BACKSPACE: NkGLFW->key_events[NK_KEY_BACKSPACE] = a; break;
        case GLFW_KEY_UP:        NkGLFW->key_events[NK_KEY_UP] = a; break;
        case GLFW_KEY_DOWN:      NkGLFW->key_events[NK_KEY_DOWN] = a; break;
        case GLFW_KEY_LEFT:      NkGLFW->key_events[NK_KEY_LEFT] = a; break;
        case GLFW_KEY_RIGHT:     NkGLFW->key_events[NK_KEY_RIGHT] = a; break;

        case GLFW_KEY_PAGE_UP:   NkGLFW->key_events[NK_KEY_SCROLL_UP] = a; break;
        case GLFW_KEY_PAGE_DOWN: NkGLFW->key_events[NK_KEY_SCROLL_DOWN] = a; break;

            /* have to add all keys used for nuklear to get correct repeat behavior
             * NOTE these are scancodes so your custom layout won't matter unfortunately
             * Also while including everything will prevent unnecessary input calls,
             * only the ones with visible effects really matter, ie paste, undo, redo
             * selecting all, copying or cutting 40 times before you release the keys
             * doesn't actually cause any visible problems */

        case GLFW_KEY_C:         NkGLFW->key_events[NK_KEY_COPY] = a; break;
        case GLFW_KEY_V:         NkGLFW->key_events[NK_KEY_PASTE] = a; break;
        case GLFW_KEY_X:         NkGLFW->key_events[NK_KEY_CUT] = a; break;
        case GLFW_KEY_Z:         NkGLFW->key_events[NK_KEY_TEXT_UNDO] = a; break;
        case GLFW_KEY_R:         NkGLFW->key_events[NK_KEY_TEXT_REDO] = a; break;
        case GLFW_KEY_B:         NkGLFW->key_events[NK_KEY_TEXT_LINE_START] = a; break;
        case GLFW_KEY_E:         NkGLFW->key_events[NK_KEY_TEXT_LINE_END] = a; break;
        case GLFW_KEY_A:         NkGLFW->key_events[NK_KEY_TEXT_SELECT_ALL] = a; break;

        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            NkGLFW->key_events[NK_KEY_ENTER] = a;
            break;
        default:
            ;
    }
}

internal void
GLFWNkClipboardPaste(nk_handle usr, struct nk_text_edit *edit)
{
    glfw_window *Window = (glfw_window *)usr.ptr;
    const char *text = glfwGetClipboardString(Window->Handle);
    if (text)
        nk_textedit_paste(edit, text, nk_strlen(text));
}

internal void
GLFWNkClipboardCopy(nk_handle usr, const char *text, int len)
{
    glfw_window *Window = (glfw_window *)usr.ptr;

    if (!len) return;
    char *str = (char*)OSAllocateMemory((size_t)len+1);
    if (!str) return;
    Copy(len, (void *)text, (void *)str);
    str[len] = '\0';
    glfwSetClipboardString(Window->Handle, str);
    OSDeallocateMemory(str);
}

internal struct nk_context*
GLFWInitNkContext(glfw_window *Window)
{
    nk_platform *NkGLFW = &Window->Nk;
    nk_init_default(&NkGLFW->ctx, 0);

    NkGLFW->ctx.clip.userdata.ptr = (void *)Window;
    NkGLFW->ctx.clip.copy = GLFWNkClipboardCopy;
    NkGLFW->ctx.clip.paste = GLFWNkClipboardPaste;
    nk_buffer_init_default(&NkGLFW->ogl.cmds);

    NkGLFW->is_double_click_down = nk_false;
    NkGLFW->double_click_pos = nk_vec2(0, 0);

    NkGLFW->delta_time_seconds_last = GLFWGetTime();

    memset(NkGLFW->key_events, -1, sizeof(NkGLFW->key_events));

    return &NkGLFW->ctx;
}

internal void
GLFWNkFontStashBegin(nk_platform *NkGLFW, struct nk_font_atlas **atlas)
{
    nk_font_atlas_init_default(&NkGLFW->atlas);
    nk_font_atlas_begin(&NkGLFW->atlas);
    *atlas = &NkGLFW->atlas;
}

internal void
GLFWNkFontStashEnd(nk_platform *NkGLFW)
{
    const void *image; int w, h;
    image = nk_font_atlas_bake(&NkGLFW->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    NkOpenGLUploadAtlas(&NkGLFW->ogl, image, w, h);
    nk_font_atlas_end(&NkGLFW->atlas, nk_handle_id((int)NkGLFW->ogl.font_tex), &NkGLFW->ogl.tex_null);
    if (NkGLFW->atlas.default_font)
        nk_style_set_font(&NkGLFW->ctx, &NkGLFW->atlas.default_font->handle);
}

inline b32
GLFWKeyIsDown(glfw_window *Window, s32 Key)
{
    b32 Result = (glfwGetKey(Window->Handle, Key) == GLFW_PRESS);
    return(Result);
}

internal void
GLFWNkUpdateInputs(glfw_window *Window, u32 WindowWidth, u32 WindowHeight,
                   rectangle2i DrawRegion, f32 dt)
{
    /*
      NOTE(pvlso): The implementation of this function is based on
      nuklear implementation for GLFW library provided with nuklear
      repo
    */

    nk_platform *NkGLFW = &Window->Nk;

    int i;
    double x, y;
    struct nk_context *ctx = &NkGLFW->ctx;
    nk_char* k_state = NkGLFW->key_events;

    NkGLFW->delta_time_seconds_last = dt;

    NkGLFW->width = WindowWidth;
    NkGLFW->height = WindowHeight;
    NkGLFW->display_width = GetWidth(DrawRegion);
    NkGLFW->display_height = GetHeight(DrawRegion);
    NkGLFW->fb_scale.x = (float)NkGLFW->display_width/(float)NkGLFW->width;
    NkGLFW->fb_scale.y = (float)NkGLFW->display_height/(float)NkGLFW->height;

    nk_input_begin(ctx);
    for (i = 0; i < NkGLFW->text_len; ++i)
        nk_input_unicode(ctx, NkGLFW->text[i]);

    if (k_state[NK_KEY_DEL] >= 0) nk_input_key(ctx, NK_KEY_DEL, k_state[NK_KEY_DEL]);
    if (k_state[NK_KEY_ENTER] >= 0) nk_input_key(ctx, NK_KEY_ENTER, k_state[NK_KEY_ENTER]);

    if (k_state[NK_KEY_TAB] >= 0) nk_input_key(ctx, NK_KEY_TAB, k_state[NK_KEY_TAB]);
    if (k_state[NK_KEY_BACKSPACE] >= 0) nk_input_key(ctx, NK_KEY_BACKSPACE, k_state[NK_KEY_BACKSPACE]);
    if (k_state[NK_KEY_UP] >= 0) nk_input_key(ctx, NK_KEY_UP, k_state[NK_KEY_UP]);
    if (k_state[NK_KEY_DOWN] >= 0) nk_input_key(ctx, NK_KEY_DOWN, k_state[NK_KEY_DOWN]);
    if (k_state[NK_KEY_SCROLL_UP] >= 0) nk_input_key(ctx, NK_KEY_SCROLL_UP, k_state[NK_KEY_SCROLL_UP]);
    if (k_state[NK_KEY_SCROLL_DOWN] >= 0) nk_input_key(ctx, NK_KEY_SCROLL_DOWN, k_state[NK_KEY_SCROLL_DOWN]);

    nk_input_key(ctx, NK_KEY_TEXT_START, GLFWKeyIsDown(Window, GLFW_KEY_HOME));
    nk_input_key(ctx, NK_KEY_TEXT_END, GLFWKeyIsDown(Window, GLFW_KEY_END));
    nk_input_key(ctx, NK_KEY_SCROLL_START, GLFWKeyIsDown(Window, GLFW_KEY_HOME));
    nk_input_key(ctx, NK_KEY_SCROLL_END, GLFWKeyIsDown(Window, GLFW_KEY_END));
    nk_input_key(ctx, NK_KEY_SHIFT, GLFWKeyIsDown(Window, GLFW_KEY_LEFT_SHIFT) ||
                 GLFWKeyIsDown(Window, GLFW_KEY_RIGHT_SHIFT));

    if (GLFWKeyIsDown(Window, GLFW_KEY_LEFT_CONTROL) ||
        GLFWKeyIsDown(Window, GLFW_KEY_RIGHT_CONTROL)) {
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

    GLFWGetCursorPosInPixels(Window, &x, &y);

    r32 MouseU = Clamp01MapToRange((r32)DrawRegion.Min.x, (f32)x, (r32)DrawRegion.Max.x);
    r32 MouseV = Clamp01MapToRange((r32)DrawRegion.Min.y, (f32)y, (r32)DrawRegion.Max.y);

    x = (r32)NkGLFW->width*MouseU;
    y = (r32)NkGLFW->height*MouseV;

    nk_input_motion(ctx, (int)x, (int)y);
    if (ctx->input.mouse.grabbed) {
        // NOTE(pvlso): prev is in UI coordinates, map it back into the draw region
        r64 GrabX = DrawRegion.Min.x + ((r64)ctx->input.mouse.prev.x/(r64)NkGLFW->width)*GetWidth(DrawRegion);
        r64 GrabY = DrawRegion.Min.y + ((r64)ctx->input.mouse.prev.y/(r64)NkGLFW->height)*GetHeight(DrawRegion);
        GLFWSetCursorPosInPixels(Window, GrabX, GrabY);
        ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
        ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
    }

    nk_input_button(ctx, NK_BUTTON_LEFT, (int)x, (int)y, glfwGetMouseButton(Window->Handle, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)x, (int)y, glfwGetMouseButton(Window->Handle, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)x, (int)y, glfwGetMouseButton(Window->Handle, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
    nk_input_button(ctx, NK_BUTTON_DOUBLE, (int)NkGLFW->double_click_pos.x, (int)NkGLFW->double_click_pos.y, NkGLFW->is_double_click_down);
    nk_input_scroll(ctx, NkGLFW->scroll);
    nk_input_end(&NkGLFW->ctx);

    /* clear after nk_input_end (-1 since we're doing up/down boolean) */
    memset(NkGLFW->key_events, -1, sizeof(NkGLFW->key_events));

    NkGLFW->text_len = 0;
    NkGLFW->scroll = nk_vec2(0,0);
}

internal void
GLFWNkShutdown(nk_platform *NkGLFW)
{
    struct nk_opengl *dev = &NkGLFW->ogl;
    nk_font_atlas_clear(&NkGLFW->atlas);
    nk_free(&NkGLFW->ctx);
    glDeleteTextures(1, &dev->font_tex);
    nk_buffer_free(&dev->cmds);
    memset(NkGLFW, 0, sizeof(*NkGLFW));
}

inline nk_context *
GLFWSetupNkContext(glfw_window *Window)
{
    nk_platform *NkGLFW = &Window->Nk;
    struct nk_context *Result = GLFWInitNkContext(Window);
    {
        struct nk_font_atlas *atlas;
        GLFWNkFontStashBegin(NkGLFW, &atlas);
        struct nk_font *deffont = nk_font_atlas_add_default(atlas, 14, 0);

        // NOTE(pvlso): nk_font_atlas_add_from_file asserts on a missing file, so check first
        struct nk_font *droid = 0;
        char *FontPath = "fonts/LiberationMono-Regular.ttf";
        if(OSFileExists(FontPath))
        {
            droid = nk_font_atlas_add_from_file(atlas, FontPath, 20, 0);
        }
        GLFWNkFontStashEnd(NkGLFW);
        nk_style_load_all_cursors(Result, atlas->cursors);

        if(droid)
            nk_style_set_font(Result, &droid->handle);
        else
            nk_style_set_font(Result, &deffont->handle);
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------



// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): WINDOW AND DISPLAY
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
GLFWDisplayBufferInWindow(editor_render_commands *Commands, rectangle2i DrawRegion,
                          u32 WindowWidth, u32 WindowHeight, memory_arena *TempArena)
{
    temporary_memory TempMem = BeginTemporaryMemory(TempArena);

    editor_render_prep Prep = PrepForRender(Commands, TempArena);

    BEGIN_BLOCK("OpenGLRenderCommands");
    OpenGLRenderCommands(Commands, &Prep, DrawRegion, WindowWidth, WindowHeight);
    END_BLOCK();

    EndTemporaryMemory(TempMem);
}

internal void
GLFWToggleFullscreen(glfw_state *State)
{
    if(glfwGetWindowMonitor(State->MainWindow.Handle))
    {
        glfwSetWindowMonitor(State->MainWindow.Handle, 0,
                             State->WindowedX, State->WindowedY,
                             State->WindowedWidth, State->WindowedHeight, 0);
    }
    else
    {
        glfwGetWindowPos(State->MainWindow.Handle, &State->WindowedX, &State->WindowedY);
        glfwGetWindowSize(State->MainWindow.Handle, &State->WindowedWidth, &State->WindowedHeight);

        const GLFWvidmode *Mode = glfwGetVideoMode(State->Monitor);
        glfwSetWindowMonitor(State->MainWindow.Handle, State->Monitor, 0, 0,
                             Mode->width, Mode->height, Mode->refreshRate);
    }

    glfwSwapInterval(1);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): WINDOW CALLBACKS / INPUT PROCESSING
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
GLFWProcessKeyboardMessage(engine_button_state *NewState, b32 IsDown)
{
    if(NewState->EndedDown != IsDown)
    {
        NewState->EndedDown = IsDown;
        ++NewState->HalfTransitionCount;
    }
}

internal void
GLFWErrorCallback(int Error, const char *Description)
{
    fprintf(stderr, "GLFW ERROR %d: %s\n", Error, Description);
}

internal void
GLFWWindowCloseCallback(GLFWwindow *Window)
{
    GlobalRunning = false;
}

// NOTE(pvlso): Maps a GLFW window back to ours, both windows point at the same glfw_state
internal glfw_window *
GLFWGetWindow(glfw_state *State, GLFWwindow *Handle)
{
    glfw_window *Result = &State->MainWindow;
#if EDITOR_INTERNAL
    if(Handle == State->DebugWindow.Handle)
    {
        Result = &State->DebugWindow;
    }
#endif

    return(Result);
}

// NOTE(pvlso): The app is active while any of its windows has focus. When focus moves
// between our windows the lost event comes first, so the other window isn't focused yet.
internal void
GLFWWindowFocusCallback(GLFWwindow *Window, int Focused)
{
    glfw_state *State = (glfw_state *)glfwGetWindowUserPointer(Window);

    b32 AnyFocused = (Focused == GLFW_TRUE);
    AnyFocused = AnyFocused || glfwGetWindowAttrib(State->MainWindow.Handle, GLFW_FOCUSED);
#if EDITOR_INTERNAL
    AnyFocused = AnyFocused || glfwGetWindowAttrib(State->DebugWindow.Handle, GLFW_FOCUSED);
#endif

    GlobalAppIsActive = AnyFocused;
}

internal void
GLFWScrollCallback(GLFWwindow *Window, double XOffset, double YOffset)
{
    glfw_state *State = (glfw_state *)glfwGetWindowUserPointer(Window);
    glfw_window *EventWindow = GLFWGetWindow(State, Window);

    // NOTE(pvlso): Only the main window drives the engine
    if(EventWindow == &State->MainWindow)
    {
        State->ScrollY += YOffset;
    }

    GLFWNkScrollCallback(&EventWindow->Nk, XOffset, YOffset);
}

internal void
GLFWCharCallback(GLFWwindow *Window, unsigned int CodePoint)
{
    glfw_state *State = (glfw_state *)glfwGetWindowUserPointer(Window);

    if((CodePoint < 32) || ((CodePoint > 126) && (CodePoint < 160)))
        return;

    GLFWNkCharCallback(&GLFWGetWindow(State, Window)->Nk, CodePoint);
}

internal void
GLFWMouseButtonCallback(GLFWwindow *Window, int Button, int Action, int Mods)
{
    glfw_state *State = (glfw_state *)glfwGetWindowUserPointer(Window);

    GLFWNkMouseButtonCallback(GLFWGetWindow(State, Window), Button, Action);
}

internal void
GLFWKeyCallback(GLFWwindow *Window, int Key, int Scancode, int Action, int Mods)
{
    glfw_state *State = (glfw_state *)glfwGetWindowUserPointer(Window);
    glfw_window *EventWindow = GLFWGetWindow(State, Window);

    GLFWNkKeyCallback(&EventWindow->Nk, Key, Action);

    // NOTE(pvlso): Only the main window drives the engine, typing into the debug window
    // shouldn't move things around in the editor
    engine_controller_input *KeyboardController = State->KeyboardController;
    if((Action != GLFW_REPEAT) && KeyboardController && (EventWindow == &State->MainWindow))
    {
        b32 IsDown = (Action == GLFW_PRESS);
        switch(Key)
        {
            case GLFW_KEY_W:      GLFWProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown); break;
            case GLFW_KEY_A:      GLFWProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown); break;
            case GLFW_KEY_S:      GLFWProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown); break;
            case GLFW_KEY_D:      GLFWProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown); break;
            case GLFW_KEY_Q:      GLFWProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown); break;
            case GLFW_KEY_E:      GLFWProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown); break;
            case GLFW_KEY_F:      GLFWProcessKeyboardMessage(&KeyboardController->Fill, IsDown); break;
            case GLFW_KEY_U:      GLFWProcessKeyboardMessage(&KeyboardController->Undo, IsDown); break;
            case GLFW_KEY_1:      GLFWProcessKeyboardMessage(&KeyboardController->FirstMode, IsDown); break;
            case GLFW_KEY_2:      GLFWProcessKeyboardMessage(&KeyboardController->SecondMode, IsDown); break;
            case GLFW_KEY_3:      GLFWProcessKeyboardMessage(&KeyboardController->ThirdMode, IsDown); break;
            case GLFW_KEY_4:      GLFWProcessKeyboardMessage(&KeyboardController->ForthMode, IsDown); break;
            case GLFW_KEY_5:      GLFWProcessKeyboardMessage(&KeyboardController->PlayMusic, IsDown); break;
            case GLFW_KEY_6:      GLFWProcessKeyboardMessage(&KeyboardController->TerminateSound, IsDown); break;
            case GLFW_KEY_H:      GLFWProcessKeyboardMessage(&KeyboardController->ShowProfiler, IsDown); break;
            case GLFW_KEY_P:      GLFWProcessKeyboardMessage(&KeyboardController->ShowUI, IsDown); break;
            case GLFW_KEY_UP:     GLFWProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown); break;
            case GLFW_KEY_LEFT:   GLFWProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown); break;
            case GLFW_KEY_DOWN:   GLFWProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown); break;
            case GLFW_KEY_RIGHT:  GLFWProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown); break;
            case GLFW_KEY_ESCAPE: GLFWProcessKeyboardMessage(&KeyboardController->Back, IsDown); break;
            case GLFW_KEY_SPACE:  GLFWProcessKeyboardMessage(&KeyboardController->Start, IsDown); break;
        }

        if(IsDown)
        {
            b32 AltKeyWasDown = (Mods & GLFW_MOD_ALT);
            if((Key == GLFW_KEY_F4) && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
            else if((Key == GLFW_KEY_ENTER) && AltKeyWasDown)
            {
                GLFWToggleFullscreen(State);
            }
        }
    }
}

internal void
GLFWProcessPendingMessages(glfw_state *State, engine_controller_input *KeyboardController, s16 *MouseZ)
{
    State->KeyboardController = KeyboardController;
    State->ScrollY = 0;

    {
        TIMED_BLOCK("glfwPollEvents");
        glfwPollEvents();
    }

    *MouseZ = (s16)RoundReal32ToInt32((r32)State->ScrollY);
    State->KeyboardController = 0;
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#if EDITOR_INTERNAL
global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;
#endif

internal void
GLFWSetCallbacks(GLFWwindow *Window)
{
    glfwSetWindowCloseCallback(Window, GLFWWindowCloseCallback);
    glfwSetWindowFocusCallback(Window, GLFWWindowFocusCallback);
    glfwSetKeyCallback(Window, GLFWKeyCallback);
    glfwSetCharCallback(Window, GLFWCharCallback);
    glfwSetMouseButtonCallback(Window, GLFWMouseButtonCallback);
    glfwSetScrollCallback(Window, GLFWScrollCallback);
}

internal b32
GLFWInitOpenGL(void)
{
    b32 Result = false;

    glewExperimental = GL_TRUE;
    if(glewInit() == GLEW_OK)
    {
        OpenGLSupportsSRGBFramebuffer = (GLEW_ARB_framebuffer_sRGB || GLEW_EXT_framebuffer_sRGB);

        opengl_info Info = OpenGLInit(true, OpenGLSupportsSRGBFramebuffer);

        glfwSwapInterval(1);

        glGenTextures(1, &OpenGLReservedBlitTexture);

        Result = true;
    }

    return(Result);
}

// NOTE(pvlso): Plain main on every OS, on Windows the exe is linked with -ENTRY:mainCRTStartup
int
main(int ArgCount, char **Args)
{
    DEBUGSetEventRecording(true);

    os_info OSInfo = OSInit();
    PlatformInitPaths();

    glfw_state *State = (glfw_state *)OSAllocateMemory(sizeof(glfw_state));

#if EDITOR_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif

    glfwSetErrorCallback(GLFWErrorCallback);
    if(!glfwInit())
    {
        fprintf(stderr, "GLFW PLATFORM: glfwInit failed.\n");
        return(1);
    }

    GlobalPerfCountFrequency = glfwGetTimerFrequency();

    State->Monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode *Mode = glfwGetVideoMode(State->Monitor);

    // NOTE(pvlso): Render at the monitor resolution, like the win32 layer
    GlobalFramebufferWidth = Mode->width;
    GlobalFramebufferHeight = Mode->height;

#if EDITOR_INTERNAL
    s32 MonitorCount = 0;
    GLFWmonitor **Monitors = glfwGetMonitors(&MonitorCount);
    for(s32 MonitorIndex = 0; MonitorIndex < MonitorCount; ++MonitorIndex)
    {
        if(Monitors[MonitorIndex] != State->Monitor)
        {
            State->DebugMonitor = Monitors[MonitorIndex];
            break;
        }
    }

    if(State->DebugMonitor)
    {
        // NOTE(pvlso): GLFW minimizes fullscreen windows when they lose focus, with both windows
        // fullscreen clicking one would minimize the other
        glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    }
#endif

    // NOTE(pvlso): The renderer and nuklear backend use fixed function GL, so ask for
    // a 3.0 context without a profile, which gives a compatibility context.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
#if EDITOR_INTERNAL
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    // NOTE(pvlso): Lets X11 window managers match our windows (e.g. i3 for_window rules),
    // ignored on other platforms
    glfwWindowHintString(GLFW_X11_CLASS_NAME, "kaban");
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "editor");

    State->WindowedWidth = (2*Mode->width)/3;
    State->WindowedHeight = (2*Mode->height)/3;
    State->WindowedX = (Mode->width - State->WindowedWidth)/2;
    State->WindowedY = (Mode->height - State->WindowedHeight)/2;

    GLFWwindow *Window = glfwCreateWindow(State->WindowedWidth, State->WindowedHeight, "Editor", 0, 0);
    if(!Window)
    {
        fprintf(stderr, "GLFW PLATFORM: Could not create the window / OpenGL context.\n");
        glfwTerminate();
        return(1);
    }

    State->MainWindow.Handle = Window;
    glfwSetWindowUserPointer(Window, State);
    glfwSetWindowPos(Window, State->WindowedX, State->WindowedY);
    glfwSetWindowAspectRatio(Window, GlobalFramebufferWidth, GlobalFramebufferHeight);

    GLFWSetCallbacks(Window);

    if(!DEBUGGlobalShowCursor)
    {
        glfwSetInputMode(Window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }

    glfwMakeContextCurrent(Window);
    if(!GLFWInitOpenGL())
    {
        fprintf(stderr, "GLFW PLATFORM: glewInit failed.\n");
        glfwTerminate();
        return(1);
    }

#if EDITOR_INTERNAL
    // NOTE(pvlso): The debug window gets its own context (GLFW has one per window) that shares
    // textures with the main one, so the nuklear font atlas works in both.
    glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "debug");
    GLFWwindow *DebugWindow = glfwCreateWindow(GLFW_DEBUG_WINDOW_WIDTH, GLFW_DEBUG_WINDOW_HEIGHT, "Debug", 0, Window);
    if(DebugWindow)
    {
        State->DebugWindow.Handle = DebugWindow;
        glfwSetWindowUserPointer(DebugWindow, State);
        GLFWSetCallbacks(DebugWindow);

        glfwMakeContextCurrent(DebugWindow);
        OpenGLInit(true, OpenGLSupportsSRGBFramebuffer);
        // NOTE(pvlso): Only the main window waits for vsync, otherwise every frame waits twice
        glfwSwapInterval(0);
        glfwMakeContextCurrent(Window);
    }
    else
    {
        fprintf(stderr, "GLFW PLATFORM: Could not create the debug window.\n");
    }
#endif

    // NOTE(pvlso): Init multithreading queues
    platform_work_queue HighPriorityQueue = {};
    PlatformMakeQueue(&HighPriorityQueue, 3);

    platform_work_queue LowPriorityQueue = {};
    PlatformMakeQueue(&LowPriorityQueue, 3);

    // NOTE(pvlso): Set fixed refresh rate
    f32 EditorUpdateHz = 60.0f;
    f32 TargetSecondsPerFrame = 1.0f / EditorUpdateHz;

    // NOTE(pvlso): Three frames of audio queued ahead of the device
    s32 SamplesPerSecond = 48000;
    u32 LatencySampleCount = Align8((u32)(3.0f*(f32)SamplesPerSecond / EditorUpdateHz));
    PlatformInitSound(&GlobalSoundOutput, SamplesPerSecond, LatencySampleCount);

    u32 MaxPossibleOverrun = 2*8*sizeof(u16);
    s16 *Samples = (s16 *)OSAllocateMemory(GlobalSoundOutput.RingSampleCount*GlobalSoundOutput.BytesPerSample +
                                           MaxPossibleOverrun);

    // NOTE(pvlso): Initialize Engine Memory and Platform API
    engine_memory EngineMemory = {};
    PlatformInitAPI(&EngineMemory, &HighPriorityQueue, &LowPriorityQueue);
    Platform = EngineMemory.PlatformAPI;

    // NOTE(pvlso): Init render memory
    // TODO(casey): Decide what our pushbuffer size is!
    u32 PushBufferSize = Megabytes(64);
    void *PushBuffer = OSAllocateMemory(PushBufferSize);

    u32 TextureOpCount = 1024;
    platform_texture_op_queue *TextureOpQueue = &EngineMemory.TextureOpQueue;
    TextureOpQueue->FirstFree = (texture_op *)OSAllocateMemory(sizeof(texture_op)*TextureOpCount);

    for(u32 TextureOpIndex = 0;
        TextureOpIndex < (TextureOpCount - 1);
        ++TextureOpIndex)
    {
        texture_op *Op = TextureOpQueue->FirstFree + TextureOpIndex;
        Op->Next = TextureOpQueue->FirstFree + TextureOpIndex + 1;
    }

    // NOTE(pvlso): Init Input
    engine_input Input[2] = {};
    engine_input *NewInput = &Input[0];
    engine_input *OldInput = &Input[1];

    u64 LastCounter = GLFWGetWallClock();

    platform_engine_code Engine = PlatformLoadEngineCode("engine");
    if(!Engine.IsValid)
    {
        fprintf(stderr, "GLFW PLATFORM: Could not load %s\n", Engine.Code.SourceLibraryName);
    }
    DEBUGSetEventRecording(Engine.IsValid);

    glfwShowWindow(Window);
    GLFWToggleFullscreen(State);
#if EDITOR_INTERNAL
    if(State->DebugWindow.Handle)
    {
        // NOTE(pvlso): Positioned after showing, X11 window managers ignore it on unmapped windows
        glfwShowWindow(State->DebugWindow.Handle);
        if(State->DebugMonitor)
        {
            const GLFWvidmode *DebugMode = glfwGetVideoMode(State->DebugMonitor);
            glfwSetWindowMonitor(State->DebugWindow.Handle, State->DebugMonitor, 0, 0,
                                 DebugMode->width, DebugMode->height, DebugMode->refreshRate);
        }
        else
        {
            glfwSetWindowPos(State->DebugWindow.Handle, GLFW_DEBUG_WINDOW_X, GLFW_DEBUG_WINDOW_Y);
        }
    }
#endif
    GlobalAppIsActive = true;

    memory_arena FrameTempArena = {};

    nk_context *nk = GLFWSetupNkContext(&State->MainWindow);
#if EDITOR_INTERNAL
    nk_context *debug_nk = GLFWSetupNkContext(&State->DebugWindow);
#endif

    GlobalRunning = true;
    while(GlobalRunning)
    {
        // NOTE(pvlso): Init Render Commands and Handle Aspect Ratio
        editor_render_commands RenderCommands = RenderCommandStruct(
            PushBufferSize, PushBuffer,
            (u32)GlobalFramebufferWidth, (u32)GlobalFramebufferHeight);

        s32 DimensionWidth, DimensionHeight;
        glfwGetFramebufferSize(Window, &DimensionWidth, &DimensionHeight);
        rectangle2i DrawRegion = AspectRatioFit(RenderCommands.Width, RenderCommands.Height,
                                                DimensionWidth, DimensionHeight);

#if EDITOR_INTERNAL
        s32 DebugDimensionWidth = 0, DebugDimensionHeight = 0;
        if(State->DebugWindow.Handle)
        {
            glfwGetFramebufferSize(State->DebugWindow.Handle, &DebugDimensionWidth, &DebugDimensionHeight);
        }
        rectangle2i DebugDrawRegion = AspectRatioFit(RenderCommands.Width, RenderCommands.Height,
                                                     DebugDimensionWidth, DebugDimensionHeight);
#endif
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Input Processing
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
        BEGIN_BLOCK("Input Processing");

        // NOTE(pvlso): Set Delta Time
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
            TIMED_BLOCK("GLFW Message Processing");
            GLFWProcessPendingMessages(State, NewKeyboardController, &MouseZ);
        }

        {
            TIMED_BLOCK("Mouse Position");

            r64 CursorX, CursorY;
            GLFWGetCursorPosInPixels(&State->MainWindow, &CursorX, &CursorY);
            r32 MouseX = (r32)CursorX;
            r32 MouseY = (r32)((DimensionHeight - 1) - CursorY);
            NewInput->MouseZ = MouseZ;

            r32 MouseU = Clamp01MapToRange((r32)DrawRegion.Min.x, MouseX, (r32)DrawRegion.Max.x);
            r32 MouseV = Clamp01MapToRange((r32)DrawRegion.Min.y, MouseY, (r32)DrawRegion.Max.y);

            NewInput->MouseX = (r32)RenderCommands.Width*MouseU;
            NewInput->MouseY = (r32)RenderCommands.Height*MouseV;

            NewInput->ShiftDown = (GLFWKeyIsDown(&State->MainWindow, GLFW_KEY_LEFT_SHIFT) || GLFWKeyIsDown(&State->MainWindow, GLFW_KEY_RIGHT_SHIFT));
            NewInput->AltDown = (GLFWKeyIsDown(&State->MainWindow, GLFW_KEY_LEFT_ALT) || GLFWKeyIsDown(&State->MainWindow, GLFW_KEY_RIGHT_ALT));
            NewInput->ControlDown = (GLFWKeyIsDown(&State->MainWindow, GLFW_KEY_LEFT_CONTROL) || GLFWKeyIsDown(&State->MainWindow, GLFW_KEY_RIGHT_CONTROL));
        }

        {
            TIMED_BLOCK("Mouse Buttons");

            s32 GLFWButtonID[PlatformMouseButton_Count] =
                {
                    GLFW_MOUSE_BUTTON_LEFT,
                    GLFW_MOUSE_BUTTON_MIDDLE,
                    GLFW_MOUSE_BUTTON_RIGHT,
                    GLFW_MOUSE_BUTTON_4,
                    GLFW_MOUSE_BUTTON_5,
                };

            for(u32 ButtonIndex = 0;
                ButtonIndex < PlatformMouseButton_Count;
                ++ButtonIndex)
            {
                NewInput->MouseButtons[ButtonIndex] = OldInput->MouseButtons[ButtonIndex];
                NewInput->MouseButtons[ButtonIndex].HalfTransitionCount = 0;
                GLFWProcessKeyboardMessage(&NewInput->MouseButtons[ButtonIndex],
                                           glfwGetMouseButton(Window, GLFWButtonID[ButtonIndex]) == GLFW_PRESS);
            }
        }

        END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Engine Update
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
        GLFWNkUpdateInputs(&State->MainWindow,
                           UI_BASE_RESOLUTION_X, UI_BASE_RESOLUTION_Y,
                           DrawRegion, TargetSecondsPerFrame);
#if EDITOR_INTERNAL
        if(State->DebugWindow.Handle)
        {
            GLFWNkUpdateInputs(&State->DebugWindow,
                               UI_BASE_RESOLUTION_X, UI_BASE_RESOLUTION_Y,
                               DebugDrawRegion, TargetSecondsPerFrame);
        }
#endif
        BEGIN_BLOCK("Engine Update");
        if(!GlobalPause)
        {
            if(Engine.UpdateAndRender)
            {
                v2 UIScale = V2(State->MainWindow.Nk.fb_scale.x, State->MainWindow.Nk.fb_scale.y);
                Engine.UpdateAndRender(nk, UIScale, &EngineMemory, NewInput, &RenderCommands);
                if(NewInput->QuitRequested)
                {
                    GlobalRunning = false;
                }
            }
        }
        END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Audio Update
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
        BEGIN_BLOCK("Audio Update");
        PlatformUpdateSound(&GlobalSoundOutput, &Engine, &EngineMemory, Samples);
        END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Debug Collation
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#if EDITOR_INTERNAL
        BEGIN_BLOCK("Debug Collation");

        b32 ExecutableNeedsToBeReloaded = PlatformCodeNeedsReload(&Engine.Code);

        EngineMemory.ExecutableReloaded = false;
        if(ExecutableNeedsToBeReloaded)
        {
            PlatformCompleteAllWork(&HighPriorityQueue);
            PlatformCompleteAllWork(&LowPriorityQueue);
            DEBUGSetEventRecording(false);
        }

        if(Engine.DEBUGFrameEnd)
        {
            Engine.DEBUGFrameEnd(debug_nk, &EngineMemory, NewInput, &RenderCommands);
        }

        if(ExecutableNeedsToBeReloaded)
        {
            PlatformUnloadEngineCode(&Engine);
            for(u32 LoadTryIndex = 0;
                !Engine.IsValid && (LoadTryIndex < 100);
                ++LoadTryIndex)
            {
                Engine = PlatformLoadEngineCode("engine");
                if(!Engine.IsValid)
                {
                    OSSleep(100);
                }
            }

            EngineMemory.ExecutableReloaded = true;
            DEBUGSetEventRecording(Engine.IsValid);
        }

        END_BLOCK();
#endif
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Frame Display
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

        GLFWDisplayBufferInWindow(&RenderCommands, DrawRegion, DimensionWidth, DimensionHeight, &FrameTempArena);
        NKOpenGLRenderCommands(&State->MainWindow.Nk, DrawRegion, NK_ANTI_ALIASING_ON);

#if EDITOR_INTERNAL
        if(State->DebugWindow.Handle)
        {
            glfwMakeContextCurrent(State->DebugWindow.Handle);

            glViewport(0, 0, DebugDimensionWidth, DebugDimensionHeight);
            glClearColor(1, 1, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            NKOpenGLRenderCommands(&State->DebugWindow.Nk, DebugDrawRegion, NK_ANTI_ALIASING_ON);

            glfwSwapBuffers(State->DebugWindow.Handle);
            glfwMakeContextCurrent(Window);
        }
#endif

        BEGIN_BLOCK("SwapBuffers");
        glfwSwapBuffers(Window);
        END_BLOCK();

        END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

        // NOTE(pvlso): Swap Inputs
        engine_input *Temp = NewInput;
        NewInput = OldInput;
        OldInput = Temp;

        BEGIN_BLOCK("FramerateWait");
        if(!GlobalPause)
        {
            r32 SecondsElapsedForFrame = GLFWGetSecondsElapsed(LastCounter, GLFWGetWallClock());
            if(SecondsElapsedForFrame < TargetSecondsPerFrame)
            {
                if(OSInfo.SleepIsGranular)
                {
                    // NOTE(pvlso): Leave a millisecond for the spin below to absorb oversleep
                    s32 SleepMS = (s32)(1000.0f*(TargetSecondsPerFrame - SecondsElapsedForFrame)) - 1;
                    if(SleepMS > 0)
                    {
                        OSSleep(SleepMS);
                    }
                }

                while(SecondsElapsedForFrame < TargetSecondsPerFrame)
                {
                    SecondsElapsedForFrame = GLFWGetSecondsElapsed(LastCounter, GLFWGetWallClock());
                }
            }
        }
        END_BLOCK();

        // NOTE(pvlso): Record frame time
        u64 EndCounter = GLFWGetWallClock();
        FRAME_MARKER(GLFWGetSecondsElapsed(LastCounter, EndCounter));
        LastCounter = EndCounter;
    }

    PlatformShutdownSound(&GlobalSoundOutput);
    PlatformCompleteAllWork(&HighPriorityQueue);
    PlatformCompleteAllWork(&LowPriorityQueue);

    GLFWNkShutdown(&State->MainWindow.Nk);
#if EDITOR_INTERNAL
    GLFWNkShutdown(&State->DebugWindow.Nk);
#endif

    PlatformUnloadEngineCode(&Engine);

#if EDITOR_INTERNAL
    if(State->DebugWindow.Handle)
    {
        glfwDestroyWindow(State->DebugWindow.Handle);
    }
#endif
    glfwDestroyWindow(Window);
    glfwTerminate();

    return(0);
}

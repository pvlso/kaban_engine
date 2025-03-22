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

#include "GL/glew.h"
#include "GL/wglew.h"

#include "win32_defines.h"

#define NK_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_glfw_gl2.h"

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
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    if (State->cursorMode == WIN32_CURSOR_DISABLED)
    {
        if (xpos)
            *xpos = State->virtualCursorPosX;
        if (ypos)
            *ypos = State->virtualCursorPosY;
    }
    else
    {
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
}

const char *
glfwGetClipboardString(void)
{
    return(0);
}

void
glfwSetClipboardString(const char *str)
{
}

inline s32
Win32GetKey(win32_state *State, s32 key)
{
    if (key < WIN32_KEY_SPACE || key > WIN32_KEY_LAST)
    {
        Assert("Invalid key");
        return WIN32_RELEASE;
    }

    if (State->keys[key] == _WIN32_STICK)
    {
        // Sticky mode: release key now
        State->keys[key] = WIN32_RELEASE;
        return WIN32_PRESS;
    }

    s32 Result = (s32)State->keys[key];
    return(Result);
}

inline void
Win32SetCursorPos(win32_state *State, double xpos, double ypos)
{
    if (xpos != xpos || xpos < -DBL_MAX || xpos > DBL_MAX ||
        ypos != ypos || ypos < -DBL_MAX || ypos > DBL_MAX)
    {
//        _glfwInputError(WIN32_INVALID_VALUE,
//                        "Invalid cursor position %f %f",
//                        xpos, ypos);
        return;
    }

    if (State->cursorMode == WIN32_CURSOR_DISABLED)
    {
        // Only update the accumulated position if the cursor is disabled
        State->virtualCursorPosX = xpos;
        State->virtualCursorPosY = ypos;
    }
    else
    {
        // Update system cursor position
        POINT pos = { (int) xpos, (int) ypos };

        // Store the new position so it can be recognized later
        State->lastCursorPosX = pos.x;
        State->lastCursorPosY = pos.y;

        ClientToScreen(State->WindowHandle, &pos);
        SetCursorPos(pos.x, pos.y);
    }
}

inline s32
Win32GetMouseButton(win32_state *State, int button)
{
    if (button < WIN32_MOUSE_BUTTON_1 || button > WIN32_MOUSE_BUTTON_LAST)
    {
//        _glfwInputError(WIN32_INVALID_ENUM, "Invalid mouse button %i", button);
        return WIN32_RELEASE;
    }

    if (State->MouseButtons[button] == _WIN32_STICK)
    {
        // Sticky mode: release mouse button now
        State->MouseButtons[button] = WIN32_RELEASE;
        return WIN32_PRESS;
    }

    s32 Result = (s32)State->MouseButtons[button];
    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): NUKLEAR CALLBACKS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal inline void
Win32NkScrollCallback(nk_win32 *NkWin32, double xoff, double yoff)
{
    NkWin32->scroll.x += (float)xoff;
    NkWin32->scroll.y += (float)yoff;
}

internal inline void
Win32NkMouseButtonCallback(nk_win32 *glfw, win32_state *State, int button, int action)
{
    double x, y;
    if (button != WIN32_MOUSE_BUTTON_LEFT) return;
    Win32GetCursorPos(State, &x, &y);
    if (action == WIN32_PRESS)  {
        double dt = Win32GetTime() - glfw->last_button_click;
        if (dt > NK_WIN32_DOUBLE_CLICK_LO && dt < NK_WIN32_DOUBLE_CLICK_HI) {
            glfw->is_double_click_down = nk_true;
            glfw->double_click_pos = nk_vec2((float)x, (float)y);
        }

        glfw->last_button_click = Win32GetTime();
    } else glfw->is_double_click_down = nk_false;
}

internal inline void
Win32NkCharCallback(nk_win32 *glfw, unsigned int codepoint)
{
    if (glfw->text_len < NK_WIN32_TEXT_MAX)
        glfw->text[glfw->text_len++] = codepoint;
}

inline void
Win32NkKeyCallback(nk_win32 *glfw, int key, int scancode, int action, int mods)
{
    /*
     * convert WIN32_REPEAT to down (technically WIN32_RELEASE, WIN32_PRESS, WIN32_REPEAT are
     * already 0, 1, 2 but just to be clearer)
     */
    nk_char a = (nk_char)((action == WIN32_RELEASE) ? nk_false : nk_true);

    NK_UNUSED(scancode);
    NK_UNUSED(mods);

    switch (key) {
        case WIN32_KEY_DELETE:    glfw->key_events[NK_KEY_DEL] = a; break;
        case WIN32_KEY_TAB:       glfw->key_events[NK_KEY_TAB] = a; break;
        case WIN32_KEY_BACKSPACE: glfw->key_events[NK_KEY_BACKSPACE] = a; break;
        case WIN32_KEY_UP:        glfw->key_events[NK_KEY_UP] = a; break;
        case WIN32_KEY_DOWN:      glfw->key_events[NK_KEY_DOWN] = a; break;
        case WIN32_KEY_LEFT:      glfw->key_events[NK_KEY_LEFT] = a; break;
        case WIN32_KEY_RIGHT:     glfw->key_events[NK_KEY_RIGHT] = a; break;

        case WIN32_KEY_PAGE_UP:   glfw->key_events[NK_KEY_SCROLL_UP] = a; break;
        case WIN32_KEY_PAGE_DOWN: glfw->key_events[NK_KEY_SCROLL_DOWN] = a; break;

            /* have to add all keys used for nuklear to get correct repeat behavior
             * NOTE these are scancodes so your custom layout won't matter unfortunately
             * Also while including everything will prevent unnecessary input calls,
             * only the ones with visible effects really matter, ie paste, undo, redo
             * selecting all, copying or cutting 40 times before you release the keys
             * doesn't actually cause any visible problems */

        case WIN32_KEY_C:         glfw->key_events[NK_KEY_COPY] = a; break;
        case WIN32_KEY_V:         glfw->key_events[NK_KEY_PASTE] = a; break;
        case WIN32_KEY_X:         glfw->key_events[NK_KEY_CUT] = a; break;
        case WIN32_KEY_Z:         glfw->key_events[NK_KEY_TEXT_UNDO] = a; break;
        case WIN32_KEY_R:         glfw->key_events[NK_KEY_TEXT_REDO] = a; break;
        case WIN32_KEY_B:         glfw->key_events[NK_KEY_TEXT_LINE_START] = a; break;
        case WIN32_KEY_E:         glfw->key_events[NK_KEY_TEXT_LINE_END] = a; break;
        case WIN32_KEY_A:         glfw->key_events[NK_KEY_TEXT_SELECT_ALL] = a; break;

        case WIN32_KEY_ENTER:
        case WIN32_KEY_KP_ENTER:
            glfw->key_events[NK_KEY_ENTER] = a;
            break;
        default:
            ;
    }
}

internal void
nk_win323_clipboard_paste(nk_handle usr, struct nk_text_edit *edit)
{
    const char *text = glfwGetClipboardString();
    if (text) nk_textedit_paste(edit, text, nk_strlen(text));
    (void)usr;
}

internal void
nk_win323_clipboard_copy(nk_handle usr, const char *text, int len)
{
    char *str = 0;
    (void)usr;
    if (!len) return;
    str = (char*)malloc((size_t)len+1);
    if (!str) return;
    memcpy(str, text, (size_t)len);
    str[len] = '\0';
    glfwSetClipboardString(str);
    free(str);
}

internal struct nk_context*
Win32InitNkContext(nk_win32 *glfw)
{
    nk_init_default(&glfw->ctx, 0);
    glfw->ctx.clip.copy = nk_win323_clipboard_copy;
    glfw->ctx.clip.paste = nk_win323_clipboard_paste;
    glfw->ctx.clip.userdata = nk_handle_ptr(0);
    nk_buffer_init_default(&glfw->ogl.cmds);

    glfw->is_double_click_down = nk_false;
    glfw->double_click_pos = nk_vec2(0, 0);

    glfw->delta_time_seconds_last = Win32GetTime();

    return &glfw->ctx;
}

internal void
Win32NkFontStashBegin(nk_win32 *glfw, struct nk_font_atlas **atlas)
{
    nk_font_atlas_init_default(&glfw->atlas);
    nk_font_atlas_begin(&glfw->atlas);
    *atlas = &glfw->atlas;
}

internal void
Win32NkFontStashEnd(nk_win32 *glfw)
{
    const void *image; int w, h;
    image = nk_font_atlas_bake(&glfw->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
    NkOpenGLUploadAtlas(&glfw->ogl, image, w, h);
    nk_font_atlas_end(&glfw->atlas, nk_handle_id((int)glfw->ogl.font_tex), &glfw->ogl.tex_null);
    if (glfw->atlas.default_font)
        nk_style_set_font(&glfw->ctx, &glfw->atlas.default_font->handle);
}

internal void
Win32NkUpdateInputs(win32_state *State, nk_win32 *glfw, u32 WindowWidth, u32 WindowHeight,
                    u32 DrawWidth, u32 DrawHeight, f32 dt)
{
    int i;
    double x, y;
    struct nk_context *ctx = &glfw->ctx;
    nk_char* k_state = glfw->key_events;

    /* update the timer */
    float delta_time_now = dt;
    glfw->delta_time_seconds_last = dt;

    glfw->width = WindowWidth;
    glfw->height = WindowHeight;
    glfw->display_width = DrawWidth;
    glfw->display_height = DrawHeight;
    glfw->fb_scale.x = (float)glfw->display_width/(float)glfw->width;
    glfw->fb_scale.y = (float)glfw->display_height/(float)glfw->height;

    nk_input_begin(ctx);
    for (i = 0; i < glfw->text_len; ++i)
        nk_input_unicode(ctx, glfw->text[i]);

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
    nk_input_motion(ctx, (int)x, (int)y);
    if (ctx->input.mouse.grabbed) {
        Win32SetCursorPos(State, (double)ctx->input.mouse.prev.x, (double)ctx->input.mouse.prev.y);
        ctx->input.mouse.pos.x = ctx->input.mouse.prev.x;
        ctx->input.mouse.pos.y = ctx->input.mouse.prev.y;
    }

    nk_input_button(ctx, NK_BUTTON_LEFT, (int)x, (int)y, Win32GetMouseButton(State, WIN32_MOUSE_BUTTON_LEFT) == WIN32_PRESS);
    nk_input_button(ctx, NK_BUTTON_MIDDLE, (int)x, (int)y, Win32GetMouseButton(State, WIN32_MOUSE_BUTTON_MIDDLE) == WIN32_PRESS);
    nk_input_button(ctx, NK_BUTTON_RIGHT, (int)x, (int)y, Win32GetMouseButton(State, WIN32_MOUSE_BUTTON_RIGHT) == WIN32_PRESS);
    nk_input_button(ctx, NK_BUTTON_DOUBLE, (int)glfw->double_click_pos.x, (int)glfw->double_click_pos.y, glfw->is_double_click_down);
    nk_input_scroll(ctx, glfw->scroll);
    nk_input_end(&glfw->ctx);

    /* clear after nk_input_end (-1 since we're doing up/down boolean) */
    memset(glfw->key_events, -1, sizeof(glfw->key_events));

    glfw->text_len = 0;
    glfw->scroll = nk_vec2(0,0);
}

internal void
Win32NkShutdown(nk_win32 *glfw)
{
    struct nk_opengl *dev = &glfw->ogl;
    nk_font_atlas_clear(&glfw->atlas);
    nk_free(&glfw->ctx);
    glDeleteTextures(1, &dev->font_tex);
    nk_buffer_free(&dev->cmds);
    memset(&glfw, 0, sizeof(glfw));
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

internal win32_editor_code
Win32LoadEditorCode(wchar_t *SourceDLLName, wchar_t *TempDLLName, wchar_t *LockFileName)
{
    win32_editor_code Result = {};

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

            Result.DEBUGFrameEnd = (debug_editor_frame_end *)
                GetProcAddress(Result.EditorCodeDLL, "DEBUGEditorFrameEnd");

            Result.IsValid = (Result.UpdateAndRender &&
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
        Result.DEBUGFrameEnd = 0;
    }

    return(Result);
}

internal void
Win32UnloadEditorCode(win32_editor_code *EditorCode)
{    
    if(EditorCode->EditorCodeDLL)
    {
        FreeLibrary(EditorCode->EditorCodeDLL);
        EditorCode->EditorCodeDLL = 0;
    }

    EditorCode->IsValid = false;
    EditorCode->UpdateAndRender = 0;
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
Win32ProcessKeyboardMessage(editor_button_state *NewState, bool32 IsDown)
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
    assert(xoffset > -FLT_MAX);
    assert(xoffset < FLT_MAX);
    assert(yoffset > -FLT_MAX);
    assert(yoffset < FLT_MAX);

    Win32NkScrollCallback(&State->Main, xoffset, yoffset);
    Win32NkScrollCallback(&State->Debug, xoffset, yoffset);
}

// Notifies shared code of a Unicode codepoint input event
// The 'plain' parameter determines whether to emit a regular character event
//

internal inline void
Win32InputChar(win32_state *State, uint32_t codepoint, int mods, b32 plain)
{
    assert(mods == (mods & WIN32_MOD_MASK));
    assert(plain == 1 || plain == 0);

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
    assert(key >= 0 || key == WIN32_KEY_UNKNOWN);
    assert(key <= WIN32_KEY_LAST);
    assert(action == WIN32_PRESS || action == WIN32_RELEASE);
    assert(mods == (mods & WIN32_MOD_MASK));

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
    assert(button >= 0);
    assert(button <= WIN32_MOUSE_BUTTON_LAST);
    assert(action == WIN32_PRESS || action == WIN32_RELEASE);
    assert(mods == (mods & WIN32_MOD_MASK));

    if (button < 0 || button > WIN32_MOUSE_BUTTON_LAST)
        return;

    if (!State->lockKeyMods)
        mods &= ~(WIN32_MOD_CAPS_LOCK | WIN32_MOD_NUM_LOCK);

    State->MouseButtons[button] = (char) action;

    Win32NkMouseButtonCallback(&State->Main, State, button, action);
    Win32NkMouseButtonCallback(&State->Debug, State, button, action);
}

internal void
Win32ProcessPendingMessages(win32_state *State, editor_controller_input *KeyboardController, s16 *MouseRotated)
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

                if (Message.message == WM_SYSCHAR && State->keymenu)
                    break;

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

#if EDITOR_INTERNAL
global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;
#endif

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024

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

#if EDITOR_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif

            // NOTE(paul): Initialize Engine Memory and Platform API
            engine_memory EditorMemory = {};

#if EDITOR_INTERNAL
            EditorMemory.DebugTable = GlobalDebugTable;
#endif
            EditorMemory.HighPriorityQueue = &HighPriorityQueue;
            EditorMemory.LowPriorityQueue = &LowPriorityQueue;
            EditorMemory.PlatformAPI.AddEntry = Win32AddEntry;
            EditorMemory.PlatformAPI.CompleteAllWork = Win32CompleteAllWork;

            EditorMemory.PlatformAPI.GetAllFilesOfTypeBegin = Win32GetAllFilesOfTypeBegin;
            EditorMemory.PlatformAPI.GetAllFilesOfTypeEnd = Win32GetAllFilesOfTypeEnd;
            EditorMemory.PlatformAPI.OpenNextFile = Win32OpenNextFile;
            EditorMemory.PlatformAPI.ReadDataFromFile = Win32ReadDataFromFile;
            EditorMemory.PlatformAPI.FileError = Win32FileError;
            EditorMemory.PlatformAPI.ListFilesInDirectory = Win32ListFilesInDirectory;

            EditorMemory.PlatformAPI.FreeFileMemory = Win32PlatformFreeFileMemory;
            EditorMemory.PlatformAPI.ReadEntireFile = Win32PlatformReadEntireFile;

            EditorMemory.PlatformAPI.AllocateMemory = Win32AllocateMemory;
            EditorMemory.PlatformAPI.DeallocateMemory = Win32DeallocateMemory;

            EditorMemory.PlatformAPI.UI.NkBegin = nk_begin;
            EditorMemory.PlatformAPI.UI.NkEnd = nk_end;

            EditorMemory.PlatformAPI.UI.NkLayoutRowDynamic = nk_layout_row_dynamic;
            EditorMemory.PlatformAPI.UI.NkLayoutRowBegin = nk_layout_row_begin;
            EditorMemory.PlatformAPI.UI.NkLayoutRowPush = nk_layout_row_push;
            EditorMemory.PlatformAPI.UI.NkLayoutRowEnd = nk_layout_row_end;

            EditorMemory.PlatformAPI.UI.NkText = nk_text;
            EditorMemory.PlatformAPI.UI.NkTextColored = nk_text_colored;
            EditorMemory.PlatformAPI.UI.NkTextWrap = nk_text_wrap;
            EditorMemory.PlatformAPI.UI.NkTextWrapColored = nk_text_wrap_colored;
            EditorMemory.PlatformAPI.UI.NkLabel = nk_label;
            EditorMemory.PlatformAPI.UI.NkLabelColored = nk_label_colored;
            EditorMemory.PlatformAPI.UI.NkLabelWrap = nk_label_wrap;
            EditorMemory.PlatformAPI.UI.NkLabelColoredWrap = nk_label_colored_wrap;
            EditorMemory.PlatformAPI.UI.NkImage = nk_image;
            EditorMemory.PlatformAPI.UI.NkImageColor = nk_image_color;

            EditorMemory.PlatformAPI.UI.NkLabelf = nk_labelf;
            EditorMemory.PlatformAPI.UI.NkLabelfColored = nk_labelf_colored;
            EditorMemory.PlatformAPI.UI.NkLabelfWrap = nk_labelf_wrap;
            EditorMemory.PlatformAPI.UI.NkLabelfColoredWrap = nk_labelf_colored_wrap;
            EditorMemory.PlatformAPI.UI.NkLabelfv = nk_labelfv;
            EditorMemory.PlatformAPI.UI.NkLabelfvColored = nk_labelfv_colored;
            EditorMemory.PlatformAPI.UI.NkLabelfvWrap = nk_labelfv_wrap;
            EditorMemory.PlatformAPI.UI.NkLabelfvColoredWrap = nk_labelfv_colored_wrap;
            EditorMemory.PlatformAPI.UI.NkValueBool = nk_value_bool;
            EditorMemory.PlatformAPI.UI.NkValueInt = nk_value_int;
            EditorMemory.PlatformAPI.UI.NkValueUint = nk_value_uint;
            EditorMemory.PlatformAPI.UI.NkValueFloat = nk_value_float;
            EditorMemory.PlatformAPI.UI.NkValueColorByte = nk_value_color_byte;
            EditorMemory.PlatformAPI.UI.NkValueColorFloat = nk_value_color_float;
            EditorMemory.PlatformAPI.UI.NkValueColorHex = nk_value_color_hex;

            EditorMemory.PlatformAPI.UI.NkButtonText = nk_button_text;
            EditorMemory.PlatformAPI.UI.NkButtonLabel = nk_button_label;
            EditorMemory.PlatformAPI.UI.NkButtonColor = nk_button_color;
            EditorMemory.PlatformAPI.UI.NkButtonSymbol = nk_button_symbol;
            EditorMemory.PlatformAPI.UI.NkButtonImage = nk_button_image;
            EditorMemory.PlatformAPI.UI.NkButtonSymbolLabel = nk_button_symbol_label;
            EditorMemory.PlatformAPI.UI.NkButtonSymbolText = nk_button_symbol_text;
            EditorMemory.PlatformAPI.UI.NkButtonImageLabel = nk_button_image_label;
            EditorMemory.PlatformAPI.UI.NkButtonImageText = nk_button_image_text;
            EditorMemory.PlatformAPI.UI.NkButtonTextStyled = nk_button_text_styled;
            EditorMemory.PlatformAPI.UI.NkButtonLabelStyled = nk_button_label_styled;
            EditorMemory.PlatformAPI.UI.NkButtonSymbolStyled = nk_button_symbol_styled;
            EditorMemory.PlatformAPI.UI.NkButtonImageStyled = nk_button_image_styled;
            EditorMemory.PlatformAPI.UI.NkButtonSymbolTextStyled = nk_button_symbol_text_styled;
            EditorMemory.PlatformAPI.UI.NkButtonSymbolLabelStyled = nk_button_symbol_label_styled;
            EditorMemory.PlatformAPI.UI.NkButtonImageLabelStyled = nk_button_image_label_styled;
            EditorMemory.PlatformAPI.UI.NkButtonImageTextStyled = nk_button_image_text_styled;
            EditorMemory.PlatformAPI.UI.NkButtonSetBehavior = nk_button_set_behavior;
            EditorMemory.PlatformAPI.UI.NkButtonPushBehavior = nk_button_push_behavior;
            EditorMemory.PlatformAPI.UI.NkButtonPopBehavior = nk_button_pop_behavior;

            EditorMemory.PlatformAPI.UI.NkCheckLabel = nk_check_label;
            EditorMemory.PlatformAPI.UI.NkCheckText = nk_check_text;
            EditorMemory.PlatformAPI.UI.NkCheckTextAlign = nk_check_text_align;
            EditorMemory.PlatformAPI.UI.NkCheckFlagsLabel = nk_check_flags_label;
            EditorMemory.PlatformAPI.UI.NkCheckFlagsText = nk_check_flags_text;
            EditorMemory.PlatformAPI.UI.NkCheckboxLabel = nk_checkbox_label;
            EditorMemory.PlatformAPI.UI.NkCheckboxLabelAlign = nk_checkbox_label_align;
            EditorMemory.PlatformAPI.UI.NkCheckboxText = nk_checkbox_text;
            EditorMemory.PlatformAPI.UI.NkCheckboxTextAlign = nk_checkbox_text_align;
            EditorMemory.PlatformAPI.UI.NkCheckboxFlagsLabel = nk_checkbox_flags_label;
            EditorMemory.PlatformAPI.UI.NkCheckboxFlagsText = nk_checkbox_flags_text;

            EditorMemory.PlatformAPI.UI.NkEditString = nk_edit_string;
            EditorMemory.PlatformAPI.UI.NkEditStringZeroTerminated = nk_edit_string_zero_terminated;
            EditorMemory.PlatformAPI.UI.NkEditBuffer = nk_edit_buffer;
            EditorMemory.PlatformAPI.UI.NkEditFocus = nk_edit_focus;
            EditorMemory.PlatformAPI.UI.NkEditUnfocus = nk_edit_unfocus;

            EditorMemory.PlatformAPI.UI.NkMurmurHash = nk_murmur_hash;
            EditorMemory.PlatformAPI.UI.NkTriangleFromDirection = nk_triangle_from_direction;

            EditorMemory.PlatformAPI.UI.NkVec2 = nk_vec2;
            EditorMemory.PlatformAPI.UI.NkVec2i = nk_vec2i;
            EditorMemory.PlatformAPI.UI.NkVec2v = nk_vec2v;
            EditorMemory.PlatformAPI.UI.NkVec2iv = nk_vec2iv;

            EditorMemory.PlatformAPI.UI.NkGetNullRect = nk_get_null_rect;
            EditorMemory.PlatformAPI.UI.NkRect = nk_rect;
            EditorMemory.PlatformAPI.UI.NkRecti = nk_recti;
            EditorMemory.PlatformAPI.UI.NkRecta = nk_recta;
            EditorMemory.PlatformAPI.UI.NkRectv = nk_rectv;
            EditorMemory.PlatformAPI.UI.NkRectiv = nk_rectiv;
            EditorMemory.PlatformAPI.UI.NkRectPos = nk_rect_pos;
            EditorMemory.PlatformAPI.UI.NkRectSize = nk_rect_size;
            
#if EDITOR_INTERNAL
            EditorMemory.PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
            EditorMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
#endif

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
            editor_input Input[2] = {};
            editor_input *NewInput = &Input[0];
            editor_input *OldInput = &Input[1];

            LARGE_INTEGER LastCounter = Win32GetWallClock();
            LARGE_INTEGER FlipWallClock = Win32GetWallClock();

            win32_editor_code Editor = Win32LoadEditorCode(SourceEditorCodeDLLFullPath,
                                                           TempEditorCodeDLLFullPath,
                                                           EditorCodeLockFullPath);
            DEBUGSetEventRecording(Editor.IsValid);

            ShowWindow(Window, SW_SHOW);

            memory_arena FrameTempArena = {};

            struct nk_context *nk;
            struct nk_colorf bg;
            nk = Win32InitNkContext(&Win32State.Main);
            bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
            char window_title[64] = "Title";

            {
                struct nk_font_atlas *atlas;
                Win32NkFontStashBegin(&Win32State.Main, &atlas);
                struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "fonts\\LiberationMono-Regular.ttf", 14, 0);
                /*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
                /*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
                /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0);*/
                /*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
                /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
                Win32NkFontStashEnd(&Win32State.Main);
                nk_style_load_all_cursors(nk, atlas->cursors);
                nk_style_set_font(nk, &droid->handle);
            }

            struct nk_context *debug_nk;
            debug_nk = Win32InitNkContext(&Win32State.Debug);

            {
                struct nk_font_atlas *atlas;
                Win32NkFontStashBegin(&Win32State.Debug, &atlas);
                struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "fonts\\LiberationMono-Regular.ttf", 14, 0);
                /*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
                /*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
                /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0);*/
                /*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
                /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
                Win32NkFontStashEnd(&Win32State.Debug);
                nk_style_load_all_cursors(debug_nk, atlas->cursors);
                nk_style_set_font(debug_nk, &droid->handle);
            }
            
            GlobalRunning = true;
            while(GlobalRunning)
            {
                {DEBUG_DATA_BLOCK("Platform/Controls");
                    DEBUG_B32(GlobalPause);
                }

                // NOTE(paul): Init Render Commands and Handle Aspect Ratio
                editor_render_commands RenderCommands = RenderCommandStruct(
                    PushBufferSize, PushBuffer,
                    (u32)GlobalFramebufferDim.Width,
                    (u32)GlobalFramebufferDim.Height);

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
                editor_controller_input *OldKeyboardController = GetController(OldInput, 0);
                editor_controller_input *NewKeyboardController = GetController(NewInput, 0);
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

                if(!GlobalPause && GlobalAppIsActive)
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
// NOTE(paul): Editor Update
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

                Win32NkUpdateInputs(&Win32State, &Win32State.Main, Dimension.Width, Dimension.Height,
                                    RenderCommands.Width, RenderCommands.Height,
                                    TargetSecondsPerFrame);

                Win32NkUpdateInputs(&Win32State, &Win32State.Debug, Dimension.Width, Dimension.Height,
                                    RenderCommands.Width, RenderCommands.Height,
                                    TargetSecondsPerFrame);

                BEGIN_BLOCK("Editor Update");
                if(!GlobalPause)
                {
                    if(Editor.UpdateAndRender)
                    {
                        Editor.UpdateAndRender(nk, &EditorMemory, NewInput, &RenderCommands);
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
#if 0

                /* GUI */
                if (nk_begin(debug_nk, "Demo", nk_rect(200, 200, 230, 250),
                             NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                             NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
                {
                    enum {EASY, HARD};
                    static int op = EASY;
                    static int property = 20;
                    nk_layout_row_static(debug_nk, 30, 80, 1);
                    if (nk_button_label(debug_nk, "button"))
                    {
//                        fprintf(stdout, "button pressed\n");
                    }

                    nk_layout_row_dynamic(debug_nk, 30, 2);
                    if (nk_option_label(debug_nk, "easy", op == EASY)) op = EASY;
                    if (nk_option_label(debug_nk, "hard", op == HARD)) op = HARD;

                    nk_layout_row_dynamic(debug_nk, 25, 1);
                    nk_property_int(debug_nk, "Compression:", 0, &property, 100, 10, 1);

                    nk_layout_row_dynamic(debug_nk, 20, 1);
                    nk_label(debug_nk, "background:", NK_TEXT_LEFT);
                    nk_layout_row_dynamic(debug_nk, 25, 1);
                    if (nk_combo_begin_color(debug_nk, nk_rgb_cf(bg), nk_vec2(nk_widget_width(debug_nk),400))) {
                        nk_layout_row_dynamic(debug_nk, 120, 1);
                        bg = nk_color_picker(debug_nk, bg, NK_RGBA);
                        nk_layout_row_dynamic(debug_nk, 25, 1);
                        bg.r = nk_propertyf(debug_nk, "#R:", 0, bg.r, 1.0f, 0.01f,0.005f);
                        bg.g = nk_propertyf(debug_nk, "#G:", 0, bg.g, 1.0f, 0.01f,0.005f);
                        bg.b = nk_propertyf(debug_nk, "#B:", 0, bg.b, 1.0f, 0.01f,0.005f);
                        bg.a = nk_propertyf(debug_nk, "#A:", 0, bg.a, 1.0f, 0.01f,0.005f);
                        nk_combo_end(debug_nk);
                    }
                }
                nk_end(debug_nk);
#endif
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
                    (CompareFileTime(&NewDLLWriteTime, &Editor.DLLLastWriteTime) != 0);

                EditorMemory.ExecutableReloaded = false;
                if(ExecutableNeedsToBeReloaded)
                {
                    Win32CompleteAllWork(&HighPriorityQueue);
                    Win32CompleteAllWork(&LowPriorityQueue);
                    DEBUGSetEventRecording(false);
                }
                    
                if(Editor.DEBUGFrameEnd)
                {
                    Editor.DEBUGFrameEnd(&EditorMemory, NewInput, &RenderCommands);
                }
                    
                if(ExecutableNeedsToBeReloaded)
                {
                    Win32UnloadEditorCode(&Editor);
                    for(u32 LoadTryIndex = 0;
                        !Editor.IsValid && (LoadTryIndex < 100);
                        ++LoadTryIndex)
                    {
                        Editor = Win32LoadEditorCode(SourceEditorCodeDLLFullPath,
                                                     TempEditorCodeDLLFullPath,
                                                     EditorCodeLockFullPath);
                        Sleep(100);
                    }
                        
                    EditorMemory.ExecutableReloaded = true;
                    DEBUGSetEventRecording(Editor.IsValid);
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
                NKOpenGLRenderCommands(&Win32State.Main, NK_ANTI_ALIASING_ON);
                NKOpenGLRenderCommands(&Win32State.Debug, NK_ANTI_ALIASING_ON);
                SwapBuffers(DeviceContext);
                ReleaseDC(Window, DeviceContext);


                END_BLOCK();
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

                // NOTE(paul): Swap Inputs
                FlipWallClock = Win32GetWallClock();

                editor_input *Temp = NewInput;
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

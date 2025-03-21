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
#include "win32_engine.h"

#define NK_INCLUDE_FIXED_TYPES
//#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_GLFW_GL2_IMPLEMENTATION
#include "nuklear.h"
#include "nuklear_glfw_gl2.h"

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

void _glfwGetCursorPosWin32(win32_state *State, double* xpos, double* ypos)
{
    POINT pos;

    if (GetCursorPos(&pos))
    {
        ScreenToClient(State->Handle, &pos);

        if (xpos)
            *xpos = pos.x;
        if (ypos)
            *ypos = pos.y;
    }
}

void
glfwGetCursorPos(win32_state *State, double *xpos, double *ypos)
{
    if (xpos)
        *xpos = 0;
    if (ypos)
        *ypos = 0;

    if (State->cursorMode == GLFW_CURSOR_DISABLED)
    {
        if (xpos)
            *xpos = State->virtualCursorPosX;
        if (ypos)
            *ypos = State->virtualCursorPosY;
    }
    else
        _glfwGetCursorPosWin32(State, xpos, ypos);
}

double
glfwGetTime(void)
{
//    return (double) (_glfwPlatformGetTimerValue() - _glfw.timer.offset) /
//        _glfwPlatformGetTimerFrequency();
    return(0.0);
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

#define _GLFW_STICK 3

int
glfwGetKey(win32_state *State, int key)
{
    if (key < GLFW_KEY_SPACE || key > GLFW_KEY_LAST)
    {
//        _glfwInputError(GLFW_INVALID_ENUM, "Invalid key %i", key);
        return GLFW_RELEASE;
    }

    if (State->keys[key] == _GLFW_STICK)
    {
        // Sticky mode: release key now
        State->keys[key] = GLFW_RELEASE;
        return GLFW_PRESS;
    }

    return (int) State->keys[key];
}

void _glfwSetCursorPosWin32(win32_state *State, double xpos, double ypos)
{
    POINT pos = { (int) xpos, (int) ypos };

    // Store the new position so it can be recognized later
    State->lastCursorPosX = pos.x;
    State->lastCursorPosY = pos.y;

    ClientToScreen(State->Handle, &pos);
    SetCursorPos(pos.x, pos.y);
}

void
glfwSetCursorPos(win32_state *State, double xpos, double ypos)
{
    if (xpos != xpos || xpos < -DBL_MAX || xpos > DBL_MAX ||
        ypos != ypos || ypos < -DBL_MAX || ypos > DBL_MAX)
    {
//        _glfwInputError(GLFW_INVALID_VALUE,
//                        "Invalid cursor position %f %f",
//                        xpos, ypos);
        return;
    }

//    if (!_glfw.platform.windowFocused(window))
//        return;

    if (State->cursorMode == GLFW_CURSOR_DISABLED)
    {
        // Only update the accumulated position if the cursor is disabled
        State->virtualCursorPosX = xpos;
        State->virtualCursorPosY = ypos;
    }
    else
    {
        // Update system cursor position
        _glfwSetCursorPosWin32(State, xpos, ypos);
    }
}

int
glfwGetMouseButton(win32_state *State, int button)
{
    if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST)
    {
//        _glfwInputError(GLFW_INVALID_ENUM, "Invalid mouse button %i", button);
        return GLFW_RELEASE;
    }

    if (State->mouseButtons[button] == _GLFW_STICK)
    {
        // Sticky mode: release mouse button now
        State->mouseButtons[button] = GLFW_RELEASE;
        return GLFW_PRESS;
    }

    return (int) State->mouseButtons[button];
}


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
void _glfwInputScroll(double xoffset, double yoffset)
{
    assert(xoffset > -FLT_MAX);
    assert(xoffset < FLT_MAX);
    assert(yoffset > -FLT_MAX);
    assert(yoffset < FLT_MAX);

    nk_gflw3_scroll_callback(xoffset, yoffset);
}

// Notifies shared code of a Unicode codepoint input event
// The 'plain' parameter determines whether to emit a regular character event
//
void _glfwInputChar(win32_state *State, uint32_t codepoint, int mods, b32 plain)
{
    assert(mods == (mods & GLFW_MOD_MASK));
    assert(plain == 1 || plain == 0);

    if (codepoint < 32 || (codepoint > 126 && codepoint < 160))
        return;

    if (!State->lockKeyMods)
        mods &= ~(GLFW_MOD_CAPS_LOCK | GLFW_MOD_NUM_LOCK);

//    if (window->callbacks.charmods)
//        window->callbacks.charmods((GLFWwindow*) window, codepoint, mods);

    if (plain)
    {
        nk_glfw3_char_callback(codepoint);
    }
}

// Retrieves and translates modifier keys
//
static int getKeyMods(void)
{
    int mods = 0;

    if (GetKeyState(VK_SHIFT) & 0x8000)
        mods |= GLFW_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        mods |= GLFW_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)
        mods |= GLFW_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
        mods |= GLFW_MOD_SUPER;
    if (GetKeyState(VK_CAPITAL) & 1)
        mods |= GLFW_MOD_CAPS_LOCK;
    if (GetKeyState(VK_NUMLOCK) & 1)
        mods |= GLFW_MOD_NUM_LOCK;

    return mods;
}

// Create key code translation tables
//
static void createKeyTables(win32_state *State)
{
    s16 scancode;

    memset(State->keycodes, -1, sizeof(State->keycodes));
    memset(State->scancodes, -1, sizeof(State->scancodes));

    State->keycodes[0x00B] = GLFW_KEY_0;
    State->keycodes[0x002] = GLFW_KEY_1;
    State->keycodes[0x003] = GLFW_KEY_2;
    State->keycodes[0x004] = GLFW_KEY_3;
    State->keycodes[0x005] = GLFW_KEY_4;
    State->keycodes[0x006] = GLFW_KEY_5;
    State->keycodes[0x007] = GLFW_KEY_6;
    State->keycodes[0x008] = GLFW_KEY_7;
    State->keycodes[0x009] = GLFW_KEY_8;
    State->keycodes[0x00A] = GLFW_KEY_9;
    State->keycodes[0x01E] = GLFW_KEY_A;
    State->keycodes[0x030] = GLFW_KEY_B;
    State->keycodes[0x02E] = GLFW_KEY_C;
    State->keycodes[0x020] = GLFW_KEY_D;
    State->keycodes[0x012] = GLFW_KEY_E;
    State->keycodes[0x021] = GLFW_KEY_F;
    State->keycodes[0x022] = GLFW_KEY_G;
    State->keycodes[0x023] = GLFW_KEY_H;
    State->keycodes[0x017] = GLFW_KEY_I;
    State->keycodes[0x024] = GLFW_KEY_J;
    State->keycodes[0x025] = GLFW_KEY_K;
    State->keycodes[0x026] = GLFW_KEY_L;
    State->keycodes[0x032] = GLFW_KEY_M;
    State->keycodes[0x031] = GLFW_KEY_N;
    State->keycodes[0x018] = GLFW_KEY_O;
    State->keycodes[0x019] = GLFW_KEY_P;
    State->keycodes[0x010] = GLFW_KEY_Q;
    State->keycodes[0x013] = GLFW_KEY_R;
    State->keycodes[0x01F] = GLFW_KEY_S;
    State->keycodes[0x014] = GLFW_KEY_T;
    State->keycodes[0x016] = GLFW_KEY_U;
    State->keycodes[0x02F] = GLFW_KEY_V;
    State->keycodes[0x011] = GLFW_KEY_W;
    State->keycodes[0x02D] = GLFW_KEY_X;
    State->keycodes[0x015] = GLFW_KEY_Y;
    State->keycodes[0x02C] = GLFW_KEY_Z;

    State->keycodes[0x028] = GLFW_KEY_APOSTROPHE;
    State->keycodes[0x02B] = GLFW_KEY_BACKSLASH;
    State->keycodes[0x033] = GLFW_KEY_COMMA;
    State->keycodes[0x00D] = GLFW_KEY_EQUAL;
    State->keycodes[0x029] = GLFW_KEY_GRAVE_ACCENT;
    State->keycodes[0x01A] = GLFW_KEY_LEFT_BRACKET;
    State->keycodes[0x00C] = GLFW_KEY_MINUS;
    State->keycodes[0x034] = GLFW_KEY_PERIOD;
    State->keycodes[0x01B] = GLFW_KEY_RIGHT_BRACKET;
    State->keycodes[0x027] = GLFW_KEY_SEMICOLON;
    State->keycodes[0x035] = GLFW_KEY_SLASH;
    State->keycodes[0x056] = GLFW_KEY_WORLD_2;

    State->keycodes[0x00E] = GLFW_KEY_BACKSPACE;
    State->keycodes[0x153] = GLFW_KEY_DELETE;
    State->keycodes[0x14F] = GLFW_KEY_END;
    State->keycodes[0x01C] = GLFW_KEY_ENTER;
    State->keycodes[0x001] = GLFW_KEY_ESCAPE;
    State->keycodes[0x147] = GLFW_KEY_HOME;
    State->keycodes[0x152] = GLFW_KEY_INSERT;
    State->keycodes[0x15D] = GLFW_KEY_MENU;
    State->keycodes[0x151] = GLFW_KEY_PAGE_DOWN;
    State->keycodes[0x149] = GLFW_KEY_PAGE_UP;
    State->keycodes[0x045] = GLFW_KEY_PAUSE;
    State->keycodes[0x039] = GLFW_KEY_SPACE;
    State->keycodes[0x00F] = GLFW_KEY_TAB;
    State->keycodes[0x03A] = GLFW_KEY_CAPS_LOCK;
    State->keycodes[0x145] = GLFW_KEY_NUM_LOCK;
    State->keycodes[0x046] = GLFW_KEY_SCROLL_LOCK;
    State->keycodes[0x03B] = GLFW_KEY_F1;
    State->keycodes[0x03C] = GLFW_KEY_F2;
    State->keycodes[0x03D] = GLFW_KEY_F3;
    State->keycodes[0x03E] = GLFW_KEY_F4;
    State->keycodes[0x03F] = GLFW_KEY_F5;
    State->keycodes[0x040] = GLFW_KEY_F6;
    State->keycodes[0x041] = GLFW_KEY_F7;
    State->keycodes[0x042] = GLFW_KEY_F8;
    State->keycodes[0x043] = GLFW_KEY_F9;
    State->keycodes[0x044] = GLFW_KEY_F10;
    State->keycodes[0x057] = GLFW_KEY_F11;
    State->keycodes[0x058] = GLFW_KEY_F12;
    State->keycodes[0x064] = GLFW_KEY_F13;
    State->keycodes[0x065] = GLFW_KEY_F14;
    State->keycodes[0x066] = GLFW_KEY_F15;
    State->keycodes[0x067] = GLFW_KEY_F16;
    State->keycodes[0x068] = GLFW_KEY_F17;
    State->keycodes[0x069] = GLFW_KEY_F18;
    State->keycodes[0x06A] = GLFW_KEY_F19;
    State->keycodes[0x06B] = GLFW_KEY_F20;
    State->keycodes[0x06C] = GLFW_KEY_F21;
    State->keycodes[0x06D] = GLFW_KEY_F22;
    State->keycodes[0x06E] = GLFW_KEY_F23;
    State->keycodes[0x076] = GLFW_KEY_F24;
    State->keycodes[0x038] = GLFW_KEY_LEFT_ALT;
    State->keycodes[0x01D] = GLFW_KEY_LEFT_CONTROL;
    State->keycodes[0x02A] = GLFW_KEY_LEFT_SHIFT;
    State->keycodes[0x15B] = GLFW_KEY_LEFT_SUPER;
    State->keycodes[0x137] = GLFW_KEY_PRINT_SCREEN;
    State->keycodes[0x138] = GLFW_KEY_RIGHT_ALT;
    State->keycodes[0x11D] = GLFW_KEY_RIGHT_CONTROL;
    State->keycodes[0x036] = GLFW_KEY_RIGHT_SHIFT;
    State->keycodes[0x15C] = GLFW_KEY_RIGHT_SUPER;
    State->keycodes[0x150] = GLFW_KEY_DOWN;
    State->keycodes[0x14B] = GLFW_KEY_LEFT;
    State->keycodes[0x14D] = GLFW_KEY_RIGHT;
    State->keycodes[0x148] = GLFW_KEY_UP;

    State->keycodes[0x052] = GLFW_KEY_KP_0;
    State->keycodes[0x04F] = GLFW_KEY_KP_1;
    State->keycodes[0x050] = GLFW_KEY_KP_2;
    State->keycodes[0x051] = GLFW_KEY_KP_3;
    State->keycodes[0x04B] = GLFW_KEY_KP_4;
    State->keycodes[0x04C] = GLFW_KEY_KP_5;
    State->keycodes[0x04D] = GLFW_KEY_KP_6;
    State->keycodes[0x047] = GLFW_KEY_KP_7;
    State->keycodes[0x048] = GLFW_KEY_KP_8;
    State->keycodes[0x049] = GLFW_KEY_KP_9;
    State->keycodes[0x04E] = GLFW_KEY_KP_ADD;
    State->keycodes[0x053] = GLFW_KEY_KP_DECIMAL;
    State->keycodes[0x135] = GLFW_KEY_KP_DIVIDE;
    State->keycodes[0x11C] = GLFW_KEY_KP_ENTER;
    State->keycodes[0x059] = GLFW_KEY_KP_EQUAL;
    State->keycodes[0x037] = GLFW_KEY_KP_MULTIPLY;
    State->keycodes[0x04A] = GLFW_KEY_KP_SUBTRACT;

    for (scancode = 0;  scancode < 512;  scancode++)
    {
        if (State->keycodes[scancode] > 0)
            State->scancodes[State->keycodes[scancode]] = scancode;
    }
}

// Notifies shared code of a physical key event
//
void _glfwInputKey(win32_state *State, int key, int scancode, int action, int mods)
{
    assert(key >= 0 || key == GLFW_KEY_UNKNOWN);
    assert(key <= GLFW_KEY_LAST);
    assert(action == GLFW_PRESS || action == GLFW_RELEASE);
    assert(mods == (mods & GLFW_MOD_MASK));

    if (key >= 0 && key <= GLFW_KEY_LAST)
    {
        b32 repeated = GLFW_FALSE;

        if (action == GLFW_RELEASE && State->keys[key] == GLFW_RELEASE)
            return;

        if (action == GLFW_PRESS && State->keys[key] == GLFW_PRESS)
            repeated = GLFW_TRUE;

//        if (action == GLFW_RELEASE && State->stickyKeys)
//            State->keys[key] = _GLFW_STICK;
//        else
        State->keys[key] = (char) action;

        if (repeated)
            action = GLFW_REPEAT;
    }

    if (!State->lockKeyMods)
        mods &= ~(GLFW_MOD_CAPS_LOCK | GLFW_MOD_NUM_LOCK);

    nk_glfw3_key_callback(key, scancode, action, mods);
//    if (window->callbacks.key)
//        window->callbacks.key((GLFWwindow*) window, key, scancode, action, mods);
}

// Notifies shared code of a mouse button click event
//
void _glfwInputMouseClick(win32_state *State, int button, int action, int mods)
{
    assert(button >= 0);
    assert(button <= GLFW_MOUSE_BUTTON_LAST);
    assert(action == GLFW_PRESS || action == GLFW_RELEASE);
    assert(mods == (mods & GLFW_MOD_MASK));

    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST)
        return;

    if (!State->lockKeyMods)
        mods &= ~(GLFW_MOD_CAPS_LOCK | GLFW_MOD_NUM_LOCK);

//    if (action == GLFW_RELEASE && State->stickyMouseButtons)
//        State->mouseButtons[button] = _GLFW_STICK;
//    else
    State->mouseButtons[button] = (char) action;

    nk_glfw3_mouse_button_callback(State, button, action);
//    if (window->callbacks.mouseButton)
//        window->callbacks.mouseButton((GLFWwindow*) window, button, action, mods);
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
                _glfwInputScroll(0.0, (SHORT) HIWORD(Message.wParam) / (double) WHEEL_DELTA);
            } break;

            case WM_MOUSEHWHEEL:
            {
                // This message is only sent on Windows Vista and later
                // NOTE: The X-axis is inverted for consistency with macOS and X11
                _glfwInputScroll(-((SHORT) HIWORD(Message.wParam) / (double) WHEEL_DELTA), 0.0);
            } break;


            case WM_INPUTLANGCHANGE:
            {
//                _glfwUpdateKeyNamesWin32();
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
                    _glfwInputChar(State, codepoint, getKeyMods(), Message.message != WM_SYSCHAR);
                }

                if (Message.message == WM_SYSCHAR && State->keymenu)
                    break;

            } break;

            case WM_UNICHAR:
            {
                if (Message.wParam == UNICODE_NOCHAR)
                {
                    // WM_UNICHAR is not sent by Windows, but is sent by some
                    // third-party input method engine
                    // Returning TRUE here announces support for this message
                    break;
                }

                _glfwInputChar(State, (uint32_t) Message.wParam, getKeyMods(), GLFW_TRUE);
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
                    button = GLFW_MOUSE_BUTTON_LEFT;
                else if (uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONUP)
                    button = GLFW_MOUSE_BUTTON_RIGHT;
                else if (uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONUP)
                    button = GLFW_MOUSE_BUTTON_MIDDLE;
                else if (GET_XBUTTON_WPARAM(Message.wParam) == XBUTTON1)
                    button = GLFW_MOUSE_BUTTON_4;
                else
                    button = GLFW_MOUSE_BUTTON_5;

                if (uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN ||
                    uMsg == WM_MBUTTONDOWN || uMsg == WM_XBUTTONDOWN)
                {
                    action = GLFW_PRESS;
                }
                else
                    action = GLFW_RELEASE;

                for (i = 0;  i <= GLFW_MOUSE_BUTTON_LAST;  i++)
                {
                    if (State->mouseButtons[i] == GLFW_PRESS)
                        break;
                }

//                if (i > GLFW_MOUSE_BUTTON_LAST)
//                    SetCapture(hWnd);

                _glfwInputMouseClick(State, button, action, getKeyMods());

                for (i = 0;  i <= GLFW_MOUSE_BUTTON_LAST;  i++)
                {
                    if (State->mouseButtons[i] == GLFW_PRESS)
                        break;
                }

                if (i > GLFW_MOUSE_BUTTON_LAST)
                    ReleaseCapture();

//                if (uMsg == WM_XBUTTONDOWN || uMsg == WM_XBUTTONUP)
//                    return TRUE;

            } break;
            
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                int key, scancode;
                const int action = (HIWORD(Message.lParam) & KF_UP) ? GLFW_RELEASE : GLFW_PRESS;
                const int mods = getKeyMods();

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

                key = State->keycodes[scancode];

                // The Ctrl keys require special handling
                if (Message.wParam == VK_CONTROL)
                {
                    if (HIWORD(Message.lParam) & KF_EXTENDED)
                    {
                        // Right side keys have the extended key bit set
                        key = GLFW_KEY_RIGHT_CONTROL;
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
                        key = GLFW_KEY_LEFT_CONTROL;
                    }
                }
                else if (Message.wParam == VK_PROCESSKEY)
                {
                    // IME notifies that keys have been filtered by setting the
                    // virtual key-code to VK_PROCESSKEY
                    break;
                }

                if (action == GLFW_RELEASE && Message.wParam == VK_SHIFT)
                {
                    // HACK: Release both Shift keys on Shift up event, as when both
                    //       are pressed the first release does not emit any event
                    // NOTE: The other half of this is in _glfwPollEventsWin32
                    _glfwInputKey(State, GLFW_KEY_LEFT_SHIFT, scancode, action, mods);
                    _glfwInputKey(State, GLFW_KEY_RIGHT_SHIFT, scancode, action, mods);
                }
                else if (Message.wParam == VK_SNAPSHOT)
                {
                    // HACK: Key down is not reported for the Print Screen key
                    _glfwInputKey(State, key, scancode, GLFW_PRESS, mods);
                    _glfwInputKey(State, key, scancode, GLFW_RELEASE, mods);
                }
                else
                    _glfwInputKey(State, key, scancode, action, mods);

#if 0
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
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
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
    createKeyTables(&Win32State);

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
            Win32State.Handle = Window;
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
            nk = nk_glfw3_init(NK_GLFW3_INSTALL_CALLBACKS);
            bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;
            char window_title[64] = "Title";

            {struct nk_font_atlas *atlas;
                nk_glfw3_font_stash_begin(&atlas);
                /*struct nk_font *droid = nk_font_atlas_add_from_file(atlas, "../../../extra_font/DroidSans.ttf", 14, 0);*/
                /*struct nk_font *roboto = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Roboto-Regular.ttf", 14, 0);*/
                /*struct nk_font *future = nk_font_atlas_add_from_file(atlas, "../../../extra_font/kenvector_future_thin.ttf", 13, 0);*/
                /*struct nk_font *clean = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyClean.ttf", 12, 0);*/
                /*struct nk_font *tiny = nk_font_atlas_add_from_file(atlas, "../../../extra_font/ProggyTiny.ttf", 10, 0);*/
                /*struct nk_font *cousine = nk_font_atlas_add_from_file(atlas, "../../../extra_font/Cousine-Regular.ttf", 13, 0);*/
                nk_glfw3_font_stash_end();
                /*nk_style_load_all_cursors(ctx, atlas->cursors);*/
                /*nk_style_set_font(ctx, &droid->handle);*/}
                
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
                BEGIN_BLOCK("Editor Update");
                if(!GlobalPause)
                {
                    if(Editor.UpdateAndRender)
                    {
                        Editor.UpdateAndRender(&EditorMemory, NewInput, &RenderCommands);
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

                struct nk_rect area = nk_rect(0.f, 0.f, (float)Dimension.Width, (float)Dimension.Height);
                nk_window_set_bounds(nk, "main", area);

                nk_glfw3_new_frame(&Win32State, Dimension.Width, Dimension.Height,
                                   RenderCommands.Width, RenderCommands.Height,
                                   TargetSecondsPerFrame);

                if (nk_begin(nk, "main", area, 0))
                {
                    nk_layout_row_dynamic(nk, 30, 4);

                    if (nk_button_label(nk, "Make Windowed"))
                    {
                    }

                    if (nk_button_label(nk, "Maximize"))
                    {
                    }
                    if (nk_button_label(nk, "Iconify"))
                    {
                    }
                    if (nk_button_label(nk, "Restore"))
                    {
                    }

                    nk_layout_row_dynamic(nk, 30, 2);

                    if (nk_button_label(nk, "Hide (for 3s)"))
                    {
                    }

                    if (nk_button_label(nk, "Request Attention (after 3s)"))
                    {
                    }

                    nk_layout_row_dynamic(nk, 30, 1);

                    nk_label(nk, "Press Enter in a text field to set value", NK_TEXT_CENTERED);

                    nk_flags events;
                    const nk_flags flags = NK_EDIT_FIELD |
                        NK_EDIT_SIG_ENTER |
                        NK_EDIT_GOTO_END_ON_ACTIVATE;

                    nk_layout_row_begin(nk, NK_DYNAMIC, 30, 2);
                    nk_layout_row_push(nk, 1.f / 3.f);
                    nk_label(nk, "Title", NK_TEXT_LEFT);
                    nk_layout_row_push(nk, 2.f / 3.f);
                    events = nk_edit_string_zero_terminated(nk, flags, window_title,
                                                            sizeof(window_title), NULL);
                    if (events & NK_EDIT_COMMITED)
                    {
                    }

                    nk_layout_row_end(nk);
                    nk_label(nk, "Platform does not support window position", NK_TEXT_LEFT);

                    nk_layout_row_dynamic(nk, 30, 3);
                    nk_label(nk, "Size", NK_TEXT_LEFT);

                    nk_label(nk, "Framebuffer Size", NK_TEXT_LEFT);
                    nk_labelf(nk, NK_TEXT_LEFT, "%i", RenderCommands.Width);
                    nk_labelf(nk, NK_TEXT_LEFT, "%i", RenderCommands.Height);
                }
                nk_end(nk);
#if 0
                /* GUI */
                if (nk_begin(ctx, "Demo", nk_rect(50, 50, 230, 250),
                             NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|
                             NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE))
                {
                    enum {EASY, HARD};
                    static int op = EASY;
                    static int property = 20;
                    nk_layout_row_static(ctx, 30, 80, 1);
                    if (nk_button_label(ctx, "button"))
                    {
//                        fprintf(stdout, "button pressed\n");
                    }

                    nk_layout_row_dynamic(ctx, 30, 2);
                    if (nk_option_label(ctx, "easy", op == EASY)) op = EASY;
                    if (nk_option_label(ctx, "hard", op == HARD)) op = HARD;

                    nk_layout_row_dynamic(ctx, 25, 1);
                    nk_property_int(ctx, "Compression:", 0, &property, 100, 10, 1);

                    nk_layout_row_dynamic(ctx, 20, 1);
                    nk_label(ctx, "background:", NK_TEXT_LEFT);
                    nk_layout_row_dynamic(ctx, 25, 1);
                    if (nk_combo_begin_color(ctx, nk_rgb_cf(bg), nk_vec2(nk_widget_width(ctx),400))) {
                        nk_layout_row_dynamic(ctx, 120, 1);
                        bg = nk_color_picker(ctx, bg, NK_RGBA);
                        nk_layout_row_dynamic(ctx, 25, 1);
                        bg.r = nk_propertyf(ctx, "#R:", 0, bg.r, 1.0f, 0.01f,0.005f);
                        bg.g = nk_propertyf(ctx, "#G:", 0, bg.g, 1.0f, 0.01f,0.005f);
                        bg.b = nk_propertyf(ctx, "#B:", 0, bg.b, 1.0f, 0.01f,0.005f);
                        bg.a = nk_propertyf(ctx, "#A:", 0, bg.a, 1.0f, 0.01f,0.005f);
                        nk_combo_end(ctx);
                    }
                }
                nk_end(ctx);
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
                nk_glfw3_render(NK_ANTI_ALIASING_ON);
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

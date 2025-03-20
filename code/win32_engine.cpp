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
#define GLFW_HAT_CENTERED           0
#define GLFW_HAT_UP                 1
#define GLFW_HAT_RIGHT              2
#define GLFW_HAT_DOWN               4
#define GLFW_HAT_LEFT               8
#define GLFW_HAT_RIGHT_UP           (GLFW_HAT_RIGHT | GLFW_HAT_UP)
#define GLFW_HAT_RIGHT_DOWN         (GLFW_HAT_RIGHT | GLFW_HAT_DOWN)
#define GLFW_HAT_LEFT_UP            (GLFW_HAT_LEFT  | GLFW_HAT_UP)
#define GLFW_HAT_LEFT_DOWN          (GLFW_HAT_LEFT  | GLFW_HAT_DOWN)

/*! @ingroup input
 */
#define GLFW_KEY_UNKNOWN            -1

/*! @} */

/*! @defgroup keys Keyboard key tokens
 *  @brief Keyboard key tokens.
 *
 *  See [key input](@ref input_key) for how these are used.
 *
 *  These key codes are inspired by the _USB HID Usage Tables v1.12_ (p. 53-60),
 *  but re-arranged to map to 7-bit ASCII for printable keys (function keys are
 *  put in the 256+ range).
 *
 *  The naming of the key codes follow these rules:
 *   - The US keyboard layout is used
 *   - Names of printable alphanumeric characters are used (e.g. "A", "R",
 *     "3", etc.)
 *   - For non-alphanumeric characters, Unicode:ish names are used (e.g.
 *     "COMMA", "LEFT_SQUARE_BRACKET", etc.). Note that some names do not
 *     correspond to the Unicode standard (usually for brevity)
 *   - Keys that lack a clear US mapping are named "WORLD_x"
 *   - For non-printable keys, custom names are used (e.g. "F4",
 *     "BACKSPACE", etc.)
 *
 *  @ingroup input
 *  @{
 */

/* Printable keys */
#define GLFW_KEY_SPACE              32
#define GLFW_KEY_APOSTROPHE         39  /* ' */
#define GLFW_KEY_COMMA              44  /* , */
#define GLFW_KEY_MINUS              45  /* - */
#define GLFW_KEY_PERIOD             46  /* . */
#define GLFW_KEY_SLASH              47  /* / */
#define GLFW_KEY_0                  48
#define GLFW_KEY_1                  49
#define GLFW_KEY_2                  50
#define GLFW_KEY_3                  51
#define GLFW_KEY_4                  52
#define GLFW_KEY_5                  53
#define GLFW_KEY_6                  54
#define GLFW_KEY_7                  55
#define GLFW_KEY_8                  56
#define GLFW_KEY_9                  57
#define GLFW_KEY_SEMICOLON          59  /* ; */
#define GLFW_KEY_EQUAL              61  /* = */
#define GLFW_KEY_A                  65
#define GLFW_KEY_B                  66
#define GLFW_KEY_C                  67
#define GLFW_KEY_D                  68
#define GLFW_KEY_E                  69
#define GLFW_KEY_F                  70
#define GLFW_KEY_G                  71
#define GLFW_KEY_H                  72
#define GLFW_KEY_I                  73
#define GLFW_KEY_J                  74
#define GLFW_KEY_K                  75
#define GLFW_KEY_L                  76
#define GLFW_KEY_M                  77
#define GLFW_KEY_N                  78
#define GLFW_KEY_O                  79
#define GLFW_KEY_P                  80
#define GLFW_KEY_Q                  81
#define GLFW_KEY_R                  82
#define GLFW_KEY_S                  83
#define GLFW_KEY_T                  84
#define GLFW_KEY_U                  85
#define GLFW_KEY_V                  86
#define GLFW_KEY_W                  87
#define GLFW_KEY_X                  88
#define GLFW_KEY_Y                  89
#define GLFW_KEY_Z                  90
#define GLFW_KEY_LEFT_BRACKET       91  /* [ */
#define GLFW_KEY_BACKSLASH          92  /* \ */
#define GLFW_KEY_RIGHT_BRACKET      93  /* ] */
#define GLFW_KEY_GRAVE_ACCENT       96  /* ` */
#define GLFW_KEY_WORLD_1            161 /* non-US #1 */
#define GLFW_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define GLFW_KEY_ESCAPE             256
#define GLFW_KEY_ENTER              257
#define GLFW_KEY_TAB                258
#define GLFW_KEY_BACKSPACE          259
#define GLFW_KEY_INSERT             260
#define GLFW_KEY_DELETE             261
#define GLFW_KEY_RIGHT              262
#define GLFW_KEY_LEFT               263
#define GLFW_KEY_DOWN               264
#define GLFW_KEY_UP                 265
#define GLFW_KEY_PAGE_UP            266
#define GLFW_KEY_PAGE_DOWN          267
#define GLFW_KEY_HOME               268
#define GLFW_KEY_END                269
#define GLFW_KEY_CAPS_LOCK          280
#define GLFW_KEY_SCROLL_LOCK        281
#define GLFW_KEY_NUM_LOCK           282
#define GLFW_KEY_PRINT_SCREEN       283
#define GLFW_KEY_PAUSE              284
#define GLFW_KEY_F1                 290
#define GLFW_KEY_F2                 291
#define GLFW_KEY_F3                 292
#define GLFW_KEY_F4                 293
#define GLFW_KEY_F5                 294
#define GLFW_KEY_F6                 295
#define GLFW_KEY_F7                 296
#define GLFW_KEY_F8                 297
#define GLFW_KEY_F9                 298
#define GLFW_KEY_F10                299
#define GLFW_KEY_F11                300
#define GLFW_KEY_F12                301
#define GLFW_KEY_F13                302
#define GLFW_KEY_F14                303
#define GLFW_KEY_F15                304
#define GLFW_KEY_F16                305
#define GLFW_KEY_F17                306
#define GLFW_KEY_F18                307
#define GLFW_KEY_F19                308
#define GLFW_KEY_F20                309
#define GLFW_KEY_F21                310
#define GLFW_KEY_F22                311
#define GLFW_KEY_F23                312
#define GLFW_KEY_F24                313
#define GLFW_KEY_F25                314
#define GLFW_KEY_KP_0               320
#define GLFW_KEY_KP_1               321
#define GLFW_KEY_KP_2               322
#define GLFW_KEY_KP_3               323
#define GLFW_KEY_KP_4               324
#define GLFW_KEY_KP_5               325
#define GLFW_KEY_KP_6               326
#define GLFW_KEY_KP_7               327
#define GLFW_KEY_KP_8               328
#define GLFW_KEY_KP_9               329
#define GLFW_KEY_KP_DECIMAL         330
#define GLFW_KEY_KP_DIVIDE          331
#define GLFW_KEY_KP_MULTIPLY        332
#define GLFW_KEY_KP_SUBTRACT        333
#define GLFW_KEY_KP_ADD             334
#define GLFW_KEY_KP_ENTER           335
#define GLFW_KEY_KP_EQUAL           336
#define GLFW_KEY_LEFT_SHIFT         340
#define GLFW_KEY_LEFT_CONTROL       341
#define GLFW_KEY_LEFT_ALT           342
#define GLFW_KEY_LEFT_SUPER         343
#define GLFW_KEY_RIGHT_SHIFT        344
#define GLFW_KEY_RIGHT_CONTROL      345
#define GLFW_KEY_RIGHT_ALT          346
#define GLFW_KEY_RIGHT_SUPER        347
#define GLFW_KEY_MENU               348

#define GLFW_KEY_LAST               GLFW_KEY_MENU

#define GLFW_RELEASE                0
#define GLFW_PRESS                  1

#define GLFW_MOD_SHIFT           0x0001
/*! @brief If this bit is set one or more Control keys were held down.
 *
 *  If this bit is set one or more Control keys were held down.
 */
#define GLFW_MOD_CONTROL         0x0002
/*! @brief If this bit is set one or more Alt keys were held down.
 *
 *  If this bit is set one or more Alt keys were held down.
 */
#define GLFW_MOD_ALT             0x0004
/*! @brief If this bit is set one or more Super keys were held down.
 *
 *  If this bit is set one or more Super keys were held down.
 */
#define GLFW_MOD_SUPER           0x0008
/*! @brief If this bit is set the Caps Lock key is enabled.
 *
 *  If this bit is set the Caps Lock key is enabled and the @ref
 *  GLFW_LOCK_KEY_MODS input mode is set.
 */
#define GLFW_MOD_CAPS_LOCK       0x0010
/*! @brief If this bit is set the Num Lock key is enabled.
 *
 *  If this bit is set the Num Lock key is enabled and the @ref
 *  GLFW_LOCK_KEY_MODS input mode is set.
 */
#define GLFW_MOD_NUM_LOCK        0x0020

/*! @defgroup buttons Mouse buttons
 *  @brief Mouse button IDs.
 *
 *  See [mouse button input](@ref input_mouse_button) for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define GLFW_MOUSE_BUTTON_1         0
#define GLFW_MOUSE_BUTTON_2         1
#define GLFW_MOUSE_BUTTON_3         2
#define GLFW_MOUSE_BUTTON_4         3
#define GLFW_MOUSE_BUTTON_5         4
#define GLFW_MOUSE_BUTTON_6         5
#define GLFW_MOUSE_BUTTON_7         6
#define GLFW_MOUSE_BUTTON_8         7
#define GLFW_MOUSE_BUTTON_LAST      GLFW_MOUSE_BUTTON_8
#define GLFW_MOUSE_BUTTON_LEFT      GLFW_MOUSE_BUTTON_1
#define GLFW_MOUSE_BUTTON_RIGHT     GLFW_MOUSE_BUTTON_2
#define GLFW_MOUSE_BUTTON_MIDDLE    GLFW_MOUSE_BUTTON_3
/*! @} */

/*! @defgroup joysticks Joysticks
 *  @brief Joystick IDs.
 *
 *  See [joystick input](@ref joystick) for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define GLFW_JOYSTICK_1             0
#define GLFW_JOYSTICK_2             1
#define GLFW_JOYSTICK_3             2
#define GLFW_JOYSTICK_4             3
#define GLFW_JOYSTICK_5             4
#define GLFW_JOYSTICK_6             5
#define GLFW_JOYSTICK_7             6
#define GLFW_JOYSTICK_8             7
#define GLFW_JOYSTICK_9             8
#define GLFW_JOYSTICK_10            9
#define GLFW_JOYSTICK_11            10
#define GLFW_JOYSTICK_12            11
#define GLFW_JOYSTICK_13            12
#define GLFW_JOYSTICK_14            13
#define GLFW_JOYSTICK_15            14
#define GLFW_JOYSTICK_16            15
#define GLFW_JOYSTICK_LAST          GLFW_JOYSTICK_16
/*! @} */

/*! @defgroup gamepad_buttons Gamepad buttons
 *  @brief Gamepad buttons.
 *
 *  See @ref gamepad for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define GLFW_GAMEPAD_BUTTON_A               0
#define GLFW_GAMEPAD_BUTTON_B               1
#define GLFW_GAMEPAD_BUTTON_X               2
#define GLFW_GAMEPAD_BUTTON_Y               3
#define GLFW_GAMEPAD_BUTTON_LEFT_BUMPER     4
#define GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER    5
#define GLFW_GAMEPAD_BUTTON_BACK            6
#define GLFW_GAMEPAD_BUTTON_START           7
#define GLFW_GAMEPAD_BUTTON_GUIDE           8
#define GLFW_GAMEPAD_BUTTON_LEFT_THUMB      9
#define GLFW_GAMEPAD_BUTTON_RIGHT_THUMB     10
#define GLFW_GAMEPAD_BUTTON_DPAD_UP         11
#define GLFW_GAMEPAD_BUTTON_DPAD_RIGHT      12
#define GLFW_GAMEPAD_BUTTON_DPAD_DOWN       13
#define GLFW_GAMEPAD_BUTTON_DPAD_LEFT       14
#define GLFW_GAMEPAD_BUTTON_LAST            GLFW_GAMEPAD_BUTTON_DPAD_LEFT

#define GLFW_GAMEPAD_BUTTON_CROSS       GLFW_GAMEPAD_BUTTON_A
#define GLFW_GAMEPAD_BUTTON_CIRCLE      GLFW_GAMEPAD_BUTTON_B
#define GLFW_GAMEPAD_BUTTON_SQUARE      GLFW_GAMEPAD_BUTTON_X
#define GLFW_GAMEPAD_BUTTON_TRIANGLE    GLFW_GAMEPAD_BUTTON_Y
/*! @} */

/*! @defgroup gamepad_axes Gamepad axes
 *  @brief Gamepad axes.
 *
 *  See @ref gamepad for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define GLFW_GAMEPAD_AXIS_LEFT_X        0
#define GLFW_GAMEPAD_AXIS_LEFT_Y        1
#define GLFW_GAMEPAD_AXIS_RIGHT_X       2
#define GLFW_GAMEPAD_AXIS_RIGHT_Y       3
#define GLFW_GAMEPAD_AXIS_LEFT_TRIGGER  4
#define GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER 5
#define GLFW_GAMEPAD_AXIS_LAST          GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER
/*! @} */

/*! @defgroup errors Error codes
 *  @brief Error codes.
 *
 *  See [error handling](@ref error_handling) for how these are used.
 *
 *  @ingroup init
 *  @{ */
/*! @brief No error has occurred.
 *
 *  No error has occurred.
 *
 *  @analysis Yay.
 */
#define GLFW_NO_ERROR               0
/*! @brief GLFW has not been initialized.
 *
 *  This occurs if a GLFW function was called that must not be called unless the
 *  library is [initialized](@ref intro_init).
 *
 *  @analysis Application programmer error.  Initialize GLFW before calling any
 *  function that requires initialization.
 */
#define GLFW_NOT_INITIALIZED        0x00010001
/*! @brief No context is current for this thread.
 *
 *  This occurs if a GLFW function was called that needs and operates on the
 *  current OpenGL or OpenGL ES context but no context is current on the calling
 *  thread.  One such function is @ref glfwSwapInterval.
 *
 *  @analysis Application programmer error.  Ensure a context is current before
 *  calling functions that require a current context.
 */
#define GLFW_NO_CURRENT_CONTEXT     0x00010002
/*! @brief One of the arguments to the function was an invalid enum value.
 *
 *  One of the arguments to the function was an invalid enum value, for example
 *  requesting @ref GLFW_RED_BITS with @ref glfwGetWindowAttrib.
 *
 *  @analysis Application programmer error.  Fix the offending call.
 */
#define GLFW_INVALID_ENUM           0x00010003
/*! @brief One of the arguments to the function was an invalid value.
 *
 *  One of the arguments to the function was an invalid value, for example
 *  requesting a non-existent OpenGL or OpenGL ES version like 2.7.
 *
 *  Requesting a valid but unavailable OpenGL or OpenGL ES version will instead
 *  result in a @ref GLFW_VERSION_UNAVAILABLE error.
 *
 *  @analysis Application programmer error.  Fix the offending call.
 */
#define GLFW_INVALID_VALUE          0x00010004
/*! @brief A memory allocation failed.
 *
 *  A memory allocation failed.
 *
 *  @analysis A bug in GLFW or the underlying operating system.  Report the bug
 *  to our [issue tracker](https://github.com/glfw/glfw/issues).
 */
#define GLFW_OUT_OF_MEMORY          0x00010005
/*! @brief GLFW could not find support for the requested API on the system.
 *
 *  GLFW could not find support for the requested API on the system.
 *
 *  @analysis The installed graphics driver does not support the requested
 *  API, or does not support it via the chosen context creation API.
 *  Below are a few examples.
 *
 *  @par
 *  Some pre-installed Windows graphics drivers do not support OpenGL.  AMD only
 *  supports OpenGL ES via EGL, while Nvidia and Intel only support it via
 *  a WGL or GLX extension.  macOS does not provide OpenGL ES at all.  The Mesa
 *  EGL, OpenGL and OpenGL ES libraries do not interface with the Nvidia binary
 *  driver.  Older graphics drivers do not support Vulkan.
 */
#define GLFW_API_UNAVAILABLE        0x00010006
/*! @brief The requested OpenGL or OpenGL ES version is not available.
 *
 *  The requested OpenGL or OpenGL ES version (including any requested context
 *  or framebuffer hints) is not available on this machine.
 *
 *  @analysis The machine does not support your requirements.  If your
 *  application is sufficiently flexible, downgrade your requirements and try
 *  again.  Otherwise, inform the user that their machine does not match your
 *  requirements.
 *
 *  @par
 *  Future invalid OpenGL and OpenGL ES versions, for example OpenGL 4.8 if 5.0
 *  comes out before the 4.x series gets that far, also fail with this error and
 *  not @ref GLFW_INVALID_VALUE, because GLFW cannot know what future versions
 *  will exist.
 */
#define GLFW_VERSION_UNAVAILABLE    0x00010007
/*! @brief A platform-specific error occurred that does not match any of the
 *  more specific categories.
 *
 *  A platform-specific error occurred that does not match any of the more
 *  specific categories.
 *
 *  @analysis A bug or configuration error in GLFW, the underlying operating
 *  system or its drivers, or a lack of required resources.  Report the issue to
 *  our [issue tracker](https://github.com/glfw/glfw/issues).
 */
#define GLFW_PLATFORM_ERROR         0x00010008
/*! @brief The requested format is not supported or available.
 *
 *  If emitted during window creation, the requested pixel format is not
 *  supported.
 *
 *  If emitted when querying the clipboard, the contents of the clipboard could
 *  not be converted to the requested format.
 *
 *  @analysis If emitted during window creation, one or more
 *  [hard constraints](@ref window_hints_hard) did not match any of the
 *  available pixel formats.  If your application is sufficiently flexible,
 *  downgrade your requirements and try again.  Otherwise, inform the user that
 *  their machine does not match your requirements.
 *
 *  @par
 *  If emitted when querying the clipboard, ignore the error or report it to
 *  the user, as appropriate.
 */
#define GLFW_FORMAT_UNAVAILABLE     0x00010009
/*! @brief The specified window does not have an OpenGL or OpenGL ES context.
 *
 *  A window that does not have an OpenGL or OpenGL ES context was passed to
 *  a function that requires it to have one.
 *
 *  @analysis Application programmer error.  Fix the offending call.
 */
#define GLFW_NO_WINDOW_CONTEXT      0x0001000A
/*! @brief The specified cursor shape is not available.
 *
 *  The specified standard cursor shape is not available, either because the
 *  current platform cursor theme does not provide it or because it is not
 *  available on the platform.
 *
 *  @analysis Platform or system settings limitation.  Pick another
 *  [standard cursor shape](@ref shapes) or create a
 *  [custom cursor](@ref cursor_custom).
 */
#define GLFW_CURSOR_UNAVAILABLE     0x0001000B
/*! @brief The requested feature is not provided by the platform.
 *
 *  The requested feature is not provided by the platform, so GLFW is unable to
 *  implement it.  The documentation for each function notes if it could emit
 *  this error.
 *
 *  @analysis Platform or platform version limitation.  The error can be ignored
 *  unless the feature is critical to the application.
 *
 *  @par
 *  A function call that emits this error has no effect other than the error and
 *  updating any existing out parameters.
 */
#define GLFW_FEATURE_UNAVAILABLE    0x0001000C
/*! @brief The requested feature is not implemented for the platform.
 *
 *  The requested feature has not yet been implemented in GLFW for this platform.
 *
 *  @analysis An incomplete implementation of GLFW for this platform, hopefully
 *  fixed in a future release.  The error can be ignored unless the feature is
 *  critical to the application.
 *
 *  @par
 *  A function call that emits this error has no effect other than the error and
 *  updating any existing out parameters.
 */
#define GLFW_FEATURE_UNIMPLEMENTED  0x0001000D
/*! @brief Platform unavailable or no matching platform was found.
 *
 *  If emitted during initialization, no matching platform was found.  If the @ref
 *  GLFW_PLATFORM init hint was set to `GLFW_ANY_PLATFORM`, GLFW could not detect any of
 *  the platforms supported by this library binary, except for the Null platform.  If the
 *  init hint was set to a specific platform, it is either not supported by this library
 *  binary or GLFW was not able to detect it.
 *
 *  If emitted by a native access function, GLFW was initialized for a different platform
 *  than the function is for.
 *
 *  @analysis Failure to detect any platform usually only happens on non-macOS Unix
 *  systems, either when no window system is running or the program was run from
 *  a terminal that does not have the necessary environment variables.  Fall back to
 *  a different platform if possible or notify the user that no usable platform was
 *  detected.
 *
 *  Failure to detect a specific platform may have the same cause as above or be because
 *  support for that platform was not compiled in.  Call @ref glfwPlatformSupported to
 *  check whether a specific platform is supported by a library binary.
 */
#define GLFW_PLATFORM_UNAVAILABLE   0x0001000E
/*! @} */

/*! @addtogroup window
 *  @{ */
/*! @brief Input focus window hint and attribute
 *
 *  Input focus [window hint](@ref GLFW_FOCUSED_hint) or
 *  [window attribute](@ref GLFW_FOCUSED_attrib).
 */
#define GLFW_FOCUSED                0x00020001
/*! @brief Window iconification window attribute
 *
 *  Window iconification [window attribute](@ref GLFW_ICONIFIED_attrib).
 */
#define GLFW_ICONIFIED              0x00020002
/*! @brief Window resize-ability window hint and attribute
 *
 *  Window resize-ability [window hint](@ref GLFW_RESIZABLE_hint) and
 *  [window attribute](@ref GLFW_RESIZABLE_attrib).
 */
#define GLFW_RESIZABLE              0x00020003
/*! @brief Window visibility window hint and attribute
 *
 *  Window visibility [window hint](@ref GLFW_VISIBLE_hint) and
 *  [window attribute](@ref GLFW_VISIBLE_attrib).
 */
#define GLFW_VISIBLE                0x00020004
/*! @brief Window decoration window hint and attribute
 *
 *  Window decoration [window hint](@ref GLFW_DECORATED_hint) and
 *  [window attribute](@ref GLFW_DECORATED_attrib).
 */
#define GLFW_DECORATED              0x00020005
/*! @brief Window auto-iconification window hint and attribute
 *
 *  Window auto-iconification [window hint](@ref GLFW_AUTO_ICONIFY_hint) and
 *  [window attribute](@ref GLFW_AUTO_ICONIFY_attrib).
 */
#define GLFW_AUTO_ICONIFY           0x00020006
/*! @brief Window decoration window hint and attribute
 *
 *  Window decoration [window hint](@ref GLFW_FLOATING_hint) and
 *  [window attribute](@ref GLFW_FLOATING_attrib).
 */
#define GLFW_FLOATING               0x00020007
/*! @brief Window maximization window hint and attribute
 *
 *  Window maximization [window hint](@ref GLFW_MAXIMIZED_hint) and
 *  [window attribute](@ref GLFW_MAXIMIZED_attrib).
 */
#define GLFW_MAXIMIZED              0x00020008
/*! @brief Cursor centering window hint
 *
 *  Cursor centering [window hint](@ref GLFW_CENTER_CURSOR_hint).
 */
#define GLFW_CENTER_CURSOR          0x00020009
/*! @brief Window framebuffer transparency hint and attribute
 *
 *  Window framebuffer transparency
 *  [window hint](@ref GLFW_TRANSPARENT_FRAMEBUFFER_hint) and
 *  [window attribute](@ref GLFW_TRANSPARENT_FRAMEBUFFER_attrib).
 */
#define GLFW_TRANSPARENT_FRAMEBUFFER 0x0002000A
/*! @brief Mouse cursor hover window attribute.
 *
 *  Mouse cursor hover [window attribute](@ref GLFW_HOVERED_attrib).
 */
#define GLFW_HOVERED                0x0002000B
/*! @brief Input focus on calling show window hint and attribute
 *
 *  Input focus [window hint](@ref GLFW_FOCUS_ON_SHOW_hint) or
 *  [window attribute](@ref GLFW_FOCUS_ON_SHOW_attrib).
 */
#define GLFW_FOCUS_ON_SHOW          0x0002000C

/*! @brief Mouse input transparency window hint and attribute
 *
 *  Mouse input transparency [window hint](@ref GLFW_MOUSE_PASSTHROUGH_hint) or
 *  [window attribute](@ref GLFW_MOUSE_PASSTHROUGH_attrib).
 */
#define GLFW_MOUSE_PASSTHROUGH      0x0002000D

/*! @brief Initial position x-coordinate window hint.
 *
 *  Initial position x-coordinate [window hint](@ref GLFW_POSITION_X).
 */
#define GLFW_POSITION_X             0x0002000E

/*! @brief Initial position y-coordinate window hint.
 *
 *  Initial position y-coordinate [window hint](@ref GLFW_POSITION_Y).
 */
#define GLFW_POSITION_Y             0x0002000F

/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_RED_BITS).
 */
#define GLFW_RED_BITS               0x00021001
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_GREEN_BITS).
 */
#define GLFW_GREEN_BITS             0x00021002
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_BLUE_BITS).
 */
#define GLFW_BLUE_BITS              0x00021003
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_ALPHA_BITS).
 */
#define GLFW_ALPHA_BITS             0x00021004
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_DEPTH_BITS).
 */
#define GLFW_DEPTH_BITS             0x00021005
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_STENCIL_BITS).
 */
#define GLFW_STENCIL_BITS           0x00021006
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_ACCUM_RED_BITS).
 */
#define GLFW_ACCUM_RED_BITS         0x00021007
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_ACCUM_GREEN_BITS).
 */
#define GLFW_ACCUM_GREEN_BITS       0x00021008
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_ACCUM_BLUE_BITS).
 */
#define GLFW_ACCUM_BLUE_BITS        0x00021009
/*! @brief Framebuffer bit depth hint.
 *
 *  Framebuffer bit depth [hint](@ref GLFW_ACCUM_ALPHA_BITS).
 */
#define GLFW_ACCUM_ALPHA_BITS       0x0002100A
/*! @brief Framebuffer auxiliary buffer hint.
 *
 *  Framebuffer auxiliary buffer [hint](@ref GLFW_AUX_BUFFERS).
 */
#define GLFW_AUX_BUFFERS            0x0002100B
/*! @brief OpenGL stereoscopic rendering hint.
 *
 *  OpenGL stereoscopic rendering [hint](@ref GLFW_STEREO).
 */
#define GLFW_STEREO                 0x0002100C
/*! @brief Framebuffer MSAA samples hint.
 *
 *  Framebuffer MSAA samples [hint](@ref GLFW_SAMPLES).
 */
#define GLFW_SAMPLES                0x0002100D
/*! @brief Framebuffer sRGB hint.
 *
 *  Framebuffer sRGB [hint](@ref GLFW_SRGB_CAPABLE).
 */
#define GLFW_SRGB_CAPABLE           0x0002100E
/*! @brief Monitor refresh rate hint.
 *
 *  Monitor refresh rate [hint](@ref GLFW_REFRESH_RATE).
 */
#define GLFW_REFRESH_RATE           0x0002100F
/*! @brief Framebuffer double buffering hint and attribute.
 *
 *  Framebuffer double buffering [hint](@ref GLFW_DOUBLEBUFFER_hint) and
 *  [attribute](@ref GLFW_DOUBLEBUFFER_attrib).
 */
#define GLFW_DOUBLEBUFFER           0x00021010

/*! @brief Context client API hint and attribute.
 *
 *  Context client API [hint](@ref GLFW_CLIENT_API_hint) and
 *  [attribute](@ref GLFW_CLIENT_API_attrib).
 */
#define GLFW_CLIENT_API             0x00022001
/*! @brief Context client API major version hint and attribute.
 *
 *  Context client API major version [hint](@ref GLFW_CONTEXT_VERSION_MAJOR_hint)
 *  and [attribute](@ref GLFW_CONTEXT_VERSION_MAJOR_attrib).
 */
#define GLFW_CONTEXT_VERSION_MAJOR  0x00022002
/*! @brief Context client API minor version hint and attribute.
 *
 *  Context client API minor version [hint](@ref GLFW_CONTEXT_VERSION_MINOR_hint)
 *  and [attribute](@ref GLFW_CONTEXT_VERSION_MINOR_attrib).
 */
#define GLFW_CONTEXT_VERSION_MINOR  0x00022003
/*! @brief Context client API revision number attribute.
 *
 *  Context client API revision number
 *  [attribute](@ref GLFW_CONTEXT_REVISION_attrib).
 */
#define GLFW_CONTEXT_REVISION       0x00022004
/*! @brief Context robustness hint and attribute.
 *
 *  Context client API revision number [hint](@ref GLFW_CONTEXT_ROBUSTNESS_hint)
 *  and [attribute](@ref GLFW_CONTEXT_ROBUSTNESS_attrib).
 */
#define GLFW_CONTEXT_ROBUSTNESS     0x00022005
/*! @brief OpenGL forward-compatibility hint and attribute.
 *
 *  OpenGL forward-compatibility [hint](@ref GLFW_OPENGL_FORWARD_COMPAT_hint)
 *  and [attribute](@ref GLFW_OPENGL_FORWARD_COMPAT_attrib).
 */
#define GLFW_OPENGL_FORWARD_COMPAT  0x00022006
/*! @brief Debug mode context hint and attribute.
 *
 *  Debug mode context [hint](@ref GLFW_CONTEXT_DEBUG_hint) and
 *  [attribute](@ref GLFW_CONTEXT_DEBUG_attrib).
 */
#define GLFW_CONTEXT_DEBUG          0x00022007
/*! @brief Legacy name for compatibility.
 *
 *  This is an alias for compatibility with earlier versions.
 */
#define GLFW_OPENGL_DEBUG_CONTEXT   GLFW_CONTEXT_DEBUG
/*! @brief OpenGL profile hint and attribute.
 *
 *  OpenGL profile [hint](@ref GLFW_OPENGL_PROFILE_hint) and
 *  [attribute](@ref GLFW_OPENGL_PROFILE_attrib).
 */
#define GLFW_OPENGL_PROFILE         0x00022008
/*! @brief Context flush-on-release hint and attribute.
 *
 *  Context flush-on-release [hint](@ref GLFW_CONTEXT_RELEASE_BEHAVIOR_hint) and
 *  [attribute](@ref GLFW_CONTEXT_RELEASE_BEHAVIOR_attrib).
 */
#define GLFW_CONTEXT_RELEASE_BEHAVIOR 0x00022009
/*! @brief Context error suppression hint and attribute.
 *
 *  Context error suppression [hint](@ref GLFW_CONTEXT_NO_ERROR_hint) and
 *  [attribute](@ref GLFW_CONTEXT_NO_ERROR_attrib).
 */
#define GLFW_CONTEXT_NO_ERROR       0x0002200A
/*! @brief Context creation API hint and attribute.
 *
 *  Context creation API [hint](@ref GLFW_CONTEXT_CREATION_API_hint) and
 *  [attribute](@ref GLFW_CONTEXT_CREATION_API_attrib).
 */
#define GLFW_CONTEXT_CREATION_API   0x0002200B
/*! @brief Window content area scaling window
 *  [window hint](@ref GLFW_SCALE_TO_MONITOR).
 */
#define GLFW_SCALE_TO_MONITOR       0x0002200C
/*! @brief Window framebuffer scaling
 *  [window hint](@ref GLFW_SCALE_FRAMEBUFFER_hint).
 */
#define GLFW_SCALE_FRAMEBUFFER      0x0002200D
/*! @brief Legacy name for compatibility.
 *
 *  This is an alias for the
 *  [GLFW_SCALE_FRAMEBUFFER](@ref GLFW_SCALE_FRAMEBUFFER_hint) window hint for
 *  compatibility with earlier versions.
 */
#define GLFW_COCOA_RETINA_FRAMEBUFFER 0x00023001
/*! @brief macOS specific
 *  [window hint](@ref GLFW_COCOA_FRAME_NAME_hint).
 */
#define GLFW_COCOA_FRAME_NAME         0x00023002
/*! @brief macOS specific
 *  [window hint](@ref GLFW_COCOA_GRAPHICS_SWITCHING_hint).
 */
#define GLFW_COCOA_GRAPHICS_SWITCHING 0x00023003
/*! @brief X11 specific
 *  [window hint](@ref GLFW_X11_CLASS_NAME_hint).
 */
#define GLFW_X11_CLASS_NAME         0x00024001
/*! @brief X11 specific
 *  [window hint](@ref GLFW_X11_CLASS_NAME_hint).
 */
#define GLFW_X11_INSTANCE_NAME      0x00024002
#define GLFW_WIN32_KEYBOARD_MENU    0x00025001
/*! @brief Win32 specific [window hint](@ref GLFW_WIN32_SHOWDEFAULT_hint).
 */
#define GLFW_WIN32_SHOWDEFAULT      0x00025002
/*! @brief Wayland specific
 *  [window hint](@ref GLFW_WAYLAND_APP_ID_hint).
 *  
 *  Allows specification of the Wayland app_id.
 */
#define GLFW_WAYLAND_APP_ID         0x00026001
/*! @} */
#define GLFW_REPEAT                 2

static s16           keycodes[512];
static s16           scancodes[GLFW_KEY_LAST + 1];
static char          keynames[GLFW_KEY_LAST + 1][5];

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

static int                 cursorMode;
static char                mouseButtons[GLFW_MOUSE_BUTTON_LAST + 1];
static char                keys[GLFW_KEY_LAST + 1];
// Virtual cursor position when cursor is disabled
static double              virtualCursorPosX, virtualCursorPosY;

// Create key code translation tables
//
static void createKeyTables(void)
{
    s16 scancode;

    memset(keycodes, -1, sizeof(keycodes));
    memset(scancodes, -1, sizeof(scancodes));

    keycodes[0x00B] = GLFW_KEY_0;
    keycodes[0x002] = GLFW_KEY_1;
    keycodes[0x003] = GLFW_KEY_2;
    keycodes[0x004] = GLFW_KEY_3;
    keycodes[0x005] = GLFW_KEY_4;
    keycodes[0x006] = GLFW_KEY_5;
    keycodes[0x007] = GLFW_KEY_6;
    keycodes[0x008] = GLFW_KEY_7;
    keycodes[0x009] = GLFW_KEY_8;
    keycodes[0x00A] = GLFW_KEY_9;
    keycodes[0x01E] = GLFW_KEY_A;
    keycodes[0x030] = GLFW_KEY_B;
    keycodes[0x02E] = GLFW_KEY_C;
    keycodes[0x020] = GLFW_KEY_D;
    keycodes[0x012] = GLFW_KEY_E;
    keycodes[0x021] = GLFW_KEY_F;
    keycodes[0x022] = GLFW_KEY_G;
    keycodes[0x023] = GLFW_KEY_H;
    keycodes[0x017] = GLFW_KEY_I;
    keycodes[0x024] = GLFW_KEY_J;
    keycodes[0x025] = GLFW_KEY_K;
    keycodes[0x026] = GLFW_KEY_L;
    keycodes[0x032] = GLFW_KEY_M;
    keycodes[0x031] = GLFW_KEY_N;
    keycodes[0x018] = GLFW_KEY_O;
    keycodes[0x019] = GLFW_KEY_P;
    keycodes[0x010] = GLFW_KEY_Q;
    keycodes[0x013] = GLFW_KEY_R;
    keycodes[0x01F] = GLFW_KEY_S;
    keycodes[0x014] = GLFW_KEY_T;
    keycodes[0x016] = GLFW_KEY_U;
    keycodes[0x02F] = GLFW_KEY_V;
    keycodes[0x011] = GLFW_KEY_W;
    keycodes[0x02D] = GLFW_KEY_X;
    keycodes[0x015] = GLFW_KEY_Y;
    keycodes[0x02C] = GLFW_KEY_Z;

    keycodes[0x028] = GLFW_KEY_APOSTROPHE;
    keycodes[0x02B] = GLFW_KEY_BACKSLASH;
    keycodes[0x033] = GLFW_KEY_COMMA;
    keycodes[0x00D] = GLFW_KEY_EQUAL;
    keycodes[0x029] = GLFW_KEY_GRAVE_ACCENT;
    keycodes[0x01A] = GLFW_KEY_LEFT_BRACKET;
    keycodes[0x00C] = GLFW_KEY_MINUS;
    keycodes[0x034] = GLFW_KEY_PERIOD;
    keycodes[0x01B] = GLFW_KEY_RIGHT_BRACKET;
    keycodes[0x027] = GLFW_KEY_SEMICOLON;
    keycodes[0x035] = GLFW_KEY_SLASH;
    keycodes[0x056] = GLFW_KEY_WORLD_2;

    keycodes[0x00E] = GLFW_KEY_BACKSPACE;
    keycodes[0x153] = GLFW_KEY_DELETE;
    keycodes[0x14F] = GLFW_KEY_END;
    keycodes[0x01C] = GLFW_KEY_ENTER;
    keycodes[0x001] = GLFW_KEY_ESCAPE;
    keycodes[0x147] = GLFW_KEY_HOME;
    keycodes[0x152] = GLFW_KEY_INSERT;
    keycodes[0x15D] = GLFW_KEY_MENU;
    keycodes[0x151] = GLFW_KEY_PAGE_DOWN;
    keycodes[0x149] = GLFW_KEY_PAGE_UP;
    keycodes[0x045] = GLFW_KEY_PAUSE;
    keycodes[0x039] = GLFW_KEY_SPACE;
    keycodes[0x00F] = GLFW_KEY_TAB;
    keycodes[0x03A] = GLFW_KEY_CAPS_LOCK;
    keycodes[0x145] = GLFW_KEY_NUM_LOCK;
    keycodes[0x046] = GLFW_KEY_SCROLL_LOCK;
    keycodes[0x03B] = GLFW_KEY_F1;
    keycodes[0x03C] = GLFW_KEY_F2;
    keycodes[0x03D] = GLFW_KEY_F3;
    keycodes[0x03E] = GLFW_KEY_F4;
    keycodes[0x03F] = GLFW_KEY_F5;
    keycodes[0x040] = GLFW_KEY_F6;
    keycodes[0x041] = GLFW_KEY_F7;
    keycodes[0x042] = GLFW_KEY_F8;
    keycodes[0x043] = GLFW_KEY_F9;
    keycodes[0x044] = GLFW_KEY_F10;
    keycodes[0x057] = GLFW_KEY_F11;
    keycodes[0x058] = GLFW_KEY_F12;
    keycodes[0x064] = GLFW_KEY_F13;
    keycodes[0x065] = GLFW_KEY_F14;
    keycodes[0x066] = GLFW_KEY_F15;
    keycodes[0x067] = GLFW_KEY_F16;
    keycodes[0x068] = GLFW_KEY_F17;
    keycodes[0x069] = GLFW_KEY_F18;
    keycodes[0x06A] = GLFW_KEY_F19;
    keycodes[0x06B] = GLFW_KEY_F20;
    keycodes[0x06C] = GLFW_KEY_F21;
    keycodes[0x06D] = GLFW_KEY_F22;
    keycodes[0x06E] = GLFW_KEY_F23;
    keycodes[0x076] = GLFW_KEY_F24;
    keycodes[0x038] = GLFW_KEY_LEFT_ALT;
    keycodes[0x01D] = GLFW_KEY_LEFT_CONTROL;
    keycodes[0x02A] = GLFW_KEY_LEFT_SHIFT;
    keycodes[0x15B] = GLFW_KEY_LEFT_SUPER;
    keycodes[0x137] = GLFW_KEY_PRINT_SCREEN;
    keycodes[0x138] = GLFW_KEY_RIGHT_ALT;
    keycodes[0x11D] = GLFW_KEY_RIGHT_CONTROL;
    keycodes[0x036] = GLFW_KEY_RIGHT_SHIFT;
    keycodes[0x15C] = GLFW_KEY_RIGHT_SUPER;
    keycodes[0x150] = GLFW_KEY_DOWN;
    keycodes[0x14B] = GLFW_KEY_LEFT;
    keycodes[0x14D] = GLFW_KEY_RIGHT;
    keycodes[0x148] = GLFW_KEY_UP;

    keycodes[0x052] = GLFW_KEY_KP_0;
    keycodes[0x04F] = GLFW_KEY_KP_1;
    keycodes[0x050] = GLFW_KEY_KP_2;
    keycodes[0x051] = GLFW_KEY_KP_3;
    keycodes[0x04B] = GLFW_KEY_KP_4;
    keycodes[0x04C] = GLFW_KEY_KP_5;
    keycodes[0x04D] = GLFW_KEY_KP_6;
    keycodes[0x047] = GLFW_KEY_KP_7;
    keycodes[0x048] = GLFW_KEY_KP_8;
    keycodes[0x049] = GLFW_KEY_KP_9;
    keycodes[0x04E] = GLFW_KEY_KP_ADD;
    keycodes[0x053] = GLFW_KEY_KP_DECIMAL;
    keycodes[0x135] = GLFW_KEY_KP_DIVIDE;
    keycodes[0x11C] = GLFW_KEY_KP_ENTER;
    keycodes[0x059] = GLFW_KEY_KP_EQUAL;
    keycodes[0x037] = GLFW_KEY_KP_MULTIPLY;
    keycodes[0x04A] = GLFW_KEY_KP_SUBTRACT;

    for (scancode = 0;  scancode < 512;  scancode++)
    {
        if (keycodes[scancode] > 0)
            scancodes[keycodes[scancode]] = scancode;
    }
}

// Notifies shared code of a physical key event
//
void
_glfwInputKey(int key, int scancode, int action, int mods)
{
    assert(key >= 0 || key == GLFW_KEY_UNKNOWN);
    assert(key <= GLFW_KEY_LAST);
    assert(action == GLFW_PRESS || action == GLFW_RELEASE);
//    assert(mods == (mods & GLFW_MOD_MASK));

    if (key >= 0 && key <= GLFW_KEY_LAST)
    {
        b32 repeated = false;

        if (action == GLFW_RELEASE && keys[key] == GLFW_RELEASE)
            return;

        if (action == GLFW_PRESS && keys[key] == GLFW_PRESS)
            repeated = true;

        keys[key] = (char) action;

        if (repeated)
            action = GLFW_REPEAT;
    }

//    if (!lockKeyMods)
//        mods &= ~(GLFW_MOD_CAPS_LOCK | GLFW_MOD_NUM_LOCK);

    OutputDebugStringW(L"Pressed\n");
}

static WCHAR               highSurrogate;

// Notifies shared code of a Unicode codepoint input event
// The 'plain' parameter determines whether to emit a regular character event
//
void _glfwInputChar(uint32_t codepoint, int mods, b32 plain)
{
//    assert(mods == (mods & GLFW_MOD_MASK));
    assert(plain == 1 || plain == 0);

    if (codepoint < 32 || (codepoint > 126 && codepoint < 160))
        return;

//    if (window->callbacks.charmods)
//        window->callbacks.charmods((GLFWwindow*) window, codepoint, mods);

    if (plain)
    {
//        if (window->callbacks.character)
//            window->callbacks.character((GLFWwindow*) window, codepoint);
    }
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
            } break;


            case WM_CHAR:
            case WM_SYSCHAR:
            {
#if 1
                if (Message.wParam >= 0xd800 && Message.wParam <= 0xdbff)
                    highSurrogate = (WCHAR) Message.wParam;
                else
                {
                    uint32_t codepoint = 0;

                    if (Message.wParam >= 0xdc00 && Message.wParam <= 0xdfff)
                    {
                        if (highSurrogate)
                        {
                            codepoint += (highSurrogate - 0xd800) << 10;
                            codepoint += (WCHAR) Message.wParam - 0xdc00;
                            codepoint += 0x10000;
                        }
                    }
                    else
                        codepoint = (WCHAR) Message.wParam;

                    highSurrogate = 0;
                    _glfwInputChar(codepoint, getKeyMods(), Message.message != WM_SYSCHAR);
                }

//                if (Message.message == WM_SYSCHAR && window->win32.keymenu)
//                    break;
#endif
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

                key = keycodes[scancode];

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
                    _glfwInputKey(GLFW_KEY_LEFT_SHIFT, scancode, action, mods);
                    _glfwInputKey(GLFW_KEY_RIGHT_SHIFT, scancode, action, mods);
                }
                else if (Message.wParam == VK_SNAPSHOT)
                {
                    // HACK: Key down is not reported for the Print Screen key
                    _glfwInputKey(key, scancode, GLFW_PRESS, mods);
                    _glfwInputKey(key, scancode, GLFW_RELEASE, mods);
                }
                else
                    _glfwInputKey(key, scancode, action, mods);
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

            struct nk_context *ctx;
            struct nk_colorf bg;
            ctx = nk_glfw3_init(NK_GLFW3_INSTALL_CALLBACKS);
            bg.r = 0.10f, bg.g = 0.18f, bg.b = 0.24f, bg.a = 1.0f;

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

                nk_glfw3_new_frame(Dimension.Width, Dimension.Height,
                                   RenderCommands.Width, RenderCommands.Height,
                                   TargetSecondsPerFrame);

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

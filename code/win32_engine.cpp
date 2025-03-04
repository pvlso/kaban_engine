/* ========================================================================
   $File: $
   $Date: 2025 $
   $Revision: $
   $Creator: Pavlo Solodrai $
   $Notice:  $
   ======================================================================== */

#include "engine_platform.h"
#include "engine_shared.h"

#include <windows.h>
#include <malloc.h>
#include <xinput.h>
#include <dsound.h>
#include <gl/gl.h>

#include "win32_engine.h"

// ================================================================================
// NOTE(paul): GLOBALS
// ================================================================================

platform_api Platform;

global_variable b32 GlobalRunning;
global_variable b32 GlobalPause;
global_variable b32 GlobalAppIsActive;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable s64 GlobalPerfCountFrequency;
global_variable b32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = {sizeof(GlobalWindowPosition)};
global_variable GLuint GlobalBlitTextureHandle;
global_variable VOID *GlobalFontBits;
// ================================================================================
// --------------------------------------------------------------------------------
// ================================================================================

// TODO(paul): Elimineta font loading through windows, use stb lib instead
global_variable HDC GlobalFontDeviceContext;

// ================================================================================
// NOTE(paul): OpenGL win32 defines
// ================================================================================

#define WGL_DRAW_TO_WINDOW_ARB                  0x2001
#define WGL_ACCELERATION_ARB                    0x2003
#define WGL_SUPPORT_OPENGL_ARB                  0x2010
#define WGL_DOUBLE_BUFFER_ARB                   0x2011
#define WGL_PIXEL_TYPE_ARB                      0x2013

#define WGL_TYPE_RGBA_ARB                       0x202B
#define WGL_FULL_ACCELERATION_ARB               0x2027

#define WGL_FRAMEBUFFER_SRGB_CAPABLE_ARB        0x20A9

typedef HGLRC WINAPI wgl_create_context_attribs_arb(HDC hDC, HGLRC hShareContext,
    const int *attribList);

typedef BOOL WINAPI wgl_get_pixel_format_attrib_iv_arb(HDC hdc,
    int iPixelFormat,
    int iLayerPlane,
    UINT nAttributes,
    const int *piAttributes,
    int *piValues);

typedef BOOL WINAPI wgl_get_pixel_format_attrib_fv_arb(HDC hdc,
    int iPixelFormat,
    int iLayerPlane,
    UINT nAttributes,
    const int *piAttributes,
    FLOAT *pfValues);

typedef BOOL WINAPI wgl_choose_pixel_format_arb(HDC hdc,
    const int *piAttribIList,
    const FLOAT *pfAttribFList,
    UINT nMaxFormats,
    int *piFormats,
    UINT *nNumFormats);

typedef GLenum WINAPI gl_check_framebuffer_status(GLenum target);
typedef void WINAPI gl_bind_framebuffer(GLenum target, GLuint framebuffer);
typedef void WINAPI gl_gen_framebuffers(GLsizei n, GLuint *framebuffers);
typedef void WINAPI gl_framebuffer_texture_2d(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);

global_variable gl_check_framebuffer_status *glCheckFramebufferStatus;
global_variable gl_bind_framebuffer *glBindFramebuffer;
global_variable gl_gen_framebuffers *glGenFramebuffers;
global_variable gl_framebuffer_texture_2d *glFramebufferTexture2D;

typedef int WINAPI wgl_get_swap_interval_ext(void);
typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
typedef const char * WINAPI wgl_get_extensions_string_ext(void);

global_variable wgl_create_context_attribs_arb *wglCreateContextAttribsARB;
global_variable wgl_choose_pixel_format_arb *wglChoosePixelFormatARB;
global_variable wgl_swap_interval_ext *wglSwapIntervalEXT;
global_variable wgl_get_swap_interval_ext *wglGetSwapIntervalEXT;
global_variable wgl_get_extensions_string_ext *wglGetExtensionsStringEXT;
global_variable b32 OpenGLSupportsSRGBFramebuffer;
global_variable GLuint OpenGLDefaultInternalTextureFormat;
global_variable GLuint OpenGLReservedBlitTexture;

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

#include "engine_sort.cpp"
#include "engine_render.h"
#include "engine_opengl.cpp"
#include "engine_render.cpp"

//=================================================================================
// NOTE(paul): OPENGL INIT & Win32 Part
//=================================================================================

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
            wglChoosePixelFormatARB = 
                (wgl_choose_pixel_format_arb *)wglGetProcAddress("wglChoosePixelFormatARB");
            wglCreateContextAttribsARB =
               (wgl_create_context_attribs_arb *)wglGetProcAddress("wglCreateContextAttribsARB");
            wglSwapIntervalEXT = (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
            wglGetSwapIntervalEXT = (wgl_get_swap_interval_ext *)wglGetProcAddress("wglGetSwapIntervalEXT");
            wglGetExtensionsStringEXT = (wgl_get_extensions_string_ext *)wglGetProcAddress("wglGetExtensionsStringEXT");

            if(wglGetExtensionsStringEXT)
            {
                char *Extensions = (char *)wglGetExtensionsStringEXT();
                char *At = Extensions;
                while(*At)
                {
                    while(IsWhitespace(*At)) {++At;}
                    char *End = At;
                    while(*End && !IsWhitespace(*End)) {++End;}

                    umm Count = End - At;        

                    if(0) {}
                    else if(StringsAreEqual(Count, At, "WGL_EXT_framebuffer_sRGB"))
                    {
                        OpenGLSupportsSRGBFramebuffer = true;
                    }
                    else if(StringsAreEqual(Count, At, "WGL_ARB_framebuffer_sRGB"))
                    {
                        OpenGLSupportsSRGBFramebuffer = true;
                    }

                    At = End;
                }
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

        if(Info.GL_ARB_framebuffer_object)
        {
            glCheckFramebufferStatus = (gl_check_framebuffer_status *)wglGetProcAddress("glCheckFramebufferStatus");
            glBindFramebuffer = (gl_bind_framebuffer *)wglGetProcAddress("glBindFramebuffer");
            glGenFramebuffers = (gl_gen_framebuffers *)wglGetProcAddress("glGenFramebuffers");
            glFramebufferTexture2D = (gl_framebuffer_texture_2d *)wglGetProcAddress("glFramebufferTexture2D");
        }

        if(wglSwapIntervalEXT)
        {
            wglSwapIntervalEXT(1);
        }

        glGenTextures(1, &OpenGLReservedBlitTexture);
    }

    return(OpenGLRC);
}

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

//=================================================================================
// NOTE(paul): Win32 Display Buffer and Window Utils
//=================================================================================

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
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;

    // NOTE(casey): When the biHeight field is negative, this is the clue to
    // Windows to treat this bitmap as top-down, not bottom-up, meaning that
    // the first three bytes of the image are the color for the top left pixel
    // in the bitmap, not the bottom left!
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    // NOTE(casey): Thank you to Chris Hecker of Spy Party fame
    // for clarifying the deal with StretchDIBits and BitBlt!
    // No more DC for us.
    Buffer->Pitch = Width*BytesPerPixel;
    int BitmapMemorySize = (Buffer->Pitch*Buffer->Height);
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    // NOTE(casey): VirtualAlloc should _only_ have given us back
    // zero'd memory which is all black, so we don't need to clear it.
}

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
    SwapBuffers(DeviceContext);
    END_BLOCK();

    EndTemporaryMemory(TempMem);
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

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

//=================================================================================
// NOTE(paul): DIRECT SOUND
//=================================================================================

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
                     editor_sound_output_buffer *SourceBuffer)
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

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

//=================================================================================
// NOTE(paul): Win32 DEBUG
//=================================================================================
#if EDITOR_INTERNAL

global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if(GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if(Result.Contents)
            {
                DWORD BytesRead;
                if(ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) &&
                   (FileSize32 == BytesRead))
                {
                    // NOTE(casey): File read successfully
                    Result.ContentsSize = FileSize32;
                }
                else
                {                    
                    // TODO: Logging
                    DEBUGPlatformFreeFileMemory(Result.Contents);
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

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if(WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // NOTE(casey): File read successfully
            Result = (BytesWritten == MemorySize);
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
//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

//=================================================================================
// NOTE(paul): Win32 Code Loading
//=================================================================================

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

            Result.GetSoundSamples = (editor_get_sound_samples *)
                GetProcAddress(Result.EditorCodeDLL, "EditorGetSoundSamples");

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
Win32UnloadEditorCode(win32_editor_code *EditorCode)
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

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

//=================================================================================
// NOTE(paul): Win32 Callbacks and Input Processing
//=================================================================================

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

                s32 RenderWidth = GlobalBackbuffer.Width;
                s32 RenderHeight = GlobalBackbuffer.Height;

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

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
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

            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

//=================================================================================
// NOTE(paul): Win32 Multiy Threading
//=================================================================================

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

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = ((real32)(End.QuadPart - Start.QuadPart) /
                     (real32)GlobalPerfCountFrequency);
    return(Result);
}

//=================================================================================
// NOTE(paul): Win32 Memory
//=================================================================================

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

PLATFORM_FREE_FILE_MEMORY(Win32PlatformFreeFileMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================

//=================================================================================
// NOTE(paul): Win32 File API
//=================================================================================

struct win32_platform_file_handle
{
    HANDLE Win32Handle;
};

struct win32_platform_file_group
{
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
};

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

//=================================================================================
//---------------------------------------------------------------------------------
//=================================================================================


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

    wchar_t Win32EXEFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"win32_editor.exe",
                              sizeof(Win32EXEFullPath), Win32EXEFullPath);

    wchar_t TempWin32EXEFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"win32_editor_temp.exe",
                              sizeof(TempWin32EXEFullPath), TempWin32EXEFullPath);

    wchar_t DeleteWin32EXEFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"win32_editor_old.exe",
                              sizeof(DeleteWin32EXEFullPath), DeleteWin32EXEFullPath);

    wchar_t SourceEditorCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"editor.dll",
                              sizeof(SourceEditorCodeDLLFullPath), SourceEditorCodeDLLFullPath);
                          
    wchar_t TempEditorCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"editor_temp.dll",
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
    WNDCLASSA WindowClass = {};

    int ScreenDimX = GetSystemMetrics(SM_CXSCREEN);
    int ScreenDimY = GetSystemMetrics(SM_CYSCREEN);
    Win32ResizeDIBSection(&GlobalBackbuffer, ScreenDimX, ScreenDimY);

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
            HDC OpenGLDC = GetDC(Window);
            HGLRC OpenGLRC = 0;

            OpenGLRC = Win32InitOpenGL(OpenGLDC);

            win32_thread_startup HighPriStartups[3] = {};
            platform_work_queue HighPriorityQueue = {};
            Win32MakeQueue(&HighPriorityQueue, ArrayCount(HighPriStartups), HighPriStartups);

            win32_thread_startup LowPriStartups[3] = {};
            platform_work_queue LowPriorityQueue = {};
            Win32MakeQueue(&LowPriorityQueue, ArrayCount(LowPriStartups), LowPriStartups);

            win32_sound_output SoundOutput = {};

            // TODO(casey): How do we reliably query on this on Windows?
            int MonitorRefreshHz = 60;
            HDC RefreshDC = GetDC(Window);
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            if((Win32RefreshRate > 1) && (Win32RefreshRate <= 60))
            {
                MonitorRefreshHz = Win32RefreshRate;
            }
            
            real32 EditorUpdateHz = (real32)(MonitorRefreshHz);
            real32 TargetSecondsPerFrame = 1.0f / (real32)EditorUpdateHz;

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

            GlobalRunning = true;

            memory_arena FrameTempArena = {};
            
            // TODO(casey): Decide what our pushbuffer size is!
            u32 PushBufferSize = Megabytes(64);
            void *PushBuffer = Win32AllocateMemory(PushBufferSize);

            u32 MaxPossibleOverrun = 2*8*sizeof(u16);
            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize + MaxPossibleOverrun,
                                                   MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);


#if EDITOR_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
            LPVOID BaseAddress = 0;
#endif

            GenerateCRC64Table();

            editor_memory EditorMemory = {};

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
            EditorMemory.PlatformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
            EditorMemory.PlatformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
            EditorMemory.PlatformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;
            EditorMemory.PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
            EditorMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
#endif

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

            Platform = EditorMemory.PlatformAPI;
            
            // TODO(casey): Handle various memory footprints (USING
            // SYSTEM METRICS)

            // TODO(casey): Use MEM_LARGE_PAGES and
            // call adjust token privileges when not on Windows XP?

            // TODO(casey): TransientStorage needs to be broken up
            // into editor transient and cache transient, and only the
            // former need be saved for state playback.

            if(Samples)
            {
                editor_input Input[2] = {};
                editor_input *NewInput = &Input[0];
                editor_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();

                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[30] = {0};

                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds = 0;
                bool32 SoundIsValid = false;

                win32_editor_code Editor = Win32LoadEditorCode(SourceEditorCodeDLLFullPath,
                                                               TempEditorCodeDLLFullPath,
                                                               EditorCodeLockFullPath);
                DEBUGSetEventRecording(Editor.IsValid);

                ShowWindow(Window, SW_SHOW);
                while(GlobalRunning)
                {
                    {DEBUG_DATA_BLOCK("Platform/Controls");
                        DEBUG_B32(GlobalPause);
                    }
                    //
                    //
                    //

                    NewInput->dtForFrame = TargetSecondsPerFrame;
                        
                    //
                    //
                    //

                    BEGIN_BLOCK("Input Processing");

                    editor_render_commands RenderCommands = RenderCommandStruct(
                        PushBufferSize, PushBuffer,
                        (u32)GlobalBackbuffer.Width,
                        (u32)GlobalBackbuffer.Height);

                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                    rectangle2i DrawRegion = AspectRatioFit(RenderCommands.Width, RenderCommands.Height,
                                                            Dimension.Width, Dimension.Height);


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

                    //
                    //
                    //

                    BEGIN_BLOCK("Editor Update");

                    editor_offscreen_buffer Buffer = {};
                    Buffer.Memory = GlobalBackbuffer.Memory;
                    Buffer.Width = GlobalBackbuffer.Width; 
                    Buffer.Height = GlobalBackbuffer.Height;
                    Buffer.Pitch = GlobalBackbuffer.Pitch;
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

                    //
                    //
                    //

                    BEGIN_BLOCK("Audio Update");

                    if(!GlobalPause)
                    {
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

                            editor_sound_output_buffer SoundBuffer = {};
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount = Align8(BytesToWrite / SoundOutput.BytesPerSample);
                            BytesToWrite = SoundBuffer.SampleCount*SoundOutput.BytesPerSample;
                            SoundBuffer.Samples = Samples;
                            if(Editor.GetSoundSamples)
                            {
                                Editor.GetSoundSamples(&EditorMemory, &SoundBuffer);
                            }

                            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                        }
                        else
                        {
                            SoundIsValid = false;
                        }
                    }

                    END_BLOCK();

                    //
                    //
                    //

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
                    ReleaseDC(Window, DeviceContext);

                    FlipWallClock = Win32GetWallClock();

                    editor_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;

                    END_BLOCK();

                    //
                    //
                    //

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

                    LARGE_INTEGER EndCounter = Win32GetWallClock();                    
                    FRAME_MARKER(Win32GetSecondsElapsed(LastCounter, EndCounter));
                    LastCounter = EndCounter;
                }
            }
            else
            {
//                PlatformWriteLogFile(L"Error: Editor Memory Unititialized", __FILE__, __LINE__);
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

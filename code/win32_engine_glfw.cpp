/* ========================================================================
   $File: $
   $Date: 2025 $
   $Revision: $
   $Creator: Pavlo Solodrai  $
   $Notice: $
   ======================================================================== */

#include "engine_platform.h"
#include "engine_shared.h"

#include <windows.h>
#include "GL/glew.h"
#include "GLFW/glfw3.h"

#include <malloc.h>
#include <xinput.h>
#include <dsound.h>

#include "win32_engine_glfw.h"

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
global_variable GLuint GlobalBlitTextureHandle;
global_variable VOID *GlobalBlitTextureHandlelFontBits;

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
// NOTE(paul): Win32 Display Buffer and Window Utils
//=================================================================================

internal void
Win32DisplayBufferInWindow(GLFWwindow *Window, platform_work_queue *RenderQueue,
                           editor_render_commands *Commands, rectangle2i DrawRegion,
                           int WindowWidth, int WindowHeight, memory_arena *TempArena)
{
    temporary_memory TempMem = BeginTemporaryMemory(TempArena);

    editor_render_prep Prep = PrepForRender(Commands, TempArena);
    
    BEGIN_BLOCK("OpenGLRenderCommands");
    OpenGLRenderCommands(Commands, &Prep, DrawRegion, WindowWidth, WindowHeight);        
    END_BLOCK();

    BEGIN_BLOCK("SwapBuffers");
    glfwSwapBuffers(Window);
    END_BLOCK();

    EndTemporaryMemory(TempMem);
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

void
Win32FramebufferSizeCallback(GLFWwindow *Window, int Width, int Height)
{
    glViewport(0, 0, Width, Height);
}

int
main(int argc, char *argv[])
{
    DEBUGSetEventRecording(true);

    // NOTE(paul): Gatting neccessary paths ===========================================
    win32_state Win32State = {};

    LARGE_INTEGER PerfCountFrequencyResult;
    QueryPerformanceFrequency(&PerfCountFrequencyResult);
    GlobalPerfCountFrequency = PerfCountFrequencyResult.QuadPart;

    Win32GetEXEFileName(&Win32State);

    wchar_t Win32EXEFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"win32_engine.exe",
                              sizeof(Win32EXEFullPath), Win32EXEFullPath);

    wchar_t SourceEditorCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"engine.dll",
                              sizeof(SourceEditorCodeDLLFullPath), SourceEditorCodeDLLFullPath);
                          
    wchar_t TempEditorCodeDLLFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"engine_temp.dll",
                              sizeof(TempEditorCodeDLLFullPath), TempEditorCodeDLLFullPath);

    wchar_t EditorCodeLockFullPath[WIN32_STATE_FILE_NAME_COUNT];
    Win32BuildEXEPathFileName(&Win32State, L"lock.tmp",
                              sizeof(EditorCodeLockFullPath), EditorCodeLockFullPath);
   // =================================================================================


    // NOTE(casey): Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

    // NOTE(paul): GLFW Init Hints  ===========================================
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);
    glfwInitHint(GLFW_JOYSTICK_HAT_BUTTONS, GLFW_FALSE);
    glfwInitHint(GLFW_ANGLE_PLATFORM_TYPE, GLFW_ANGLE_PLATFORM_TYPE_OPENGL);

    if(glfwInit())
    {
        // NOTE(paul): GLFW Window Creating Hints
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

        GLFWmonitor *PrimaryMonitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *VideoMode = glfwGetVideoMode(PrimaryMonitor);

        glfwWindowHint(GLFW_RED_BITS, VideoMode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, VideoMode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, VideoMode->blueBits);

        glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
        
        GLFWwindow *MainWindow = glfwCreateWindow(1920, 1080, "Window", 0, 0);
        
//        glfwSetWindowMonitor(MainWindow, PrimaryMonitor, 0, 0,
//                             VideoMode->width, VideoMode->height, VideoMode->refreshRate);

        if(MainWindow)
        {
            glfwMakeContextCurrent(MainWindow);

            glewExperimental = GL_TRUE;
            if(glewInit() == GLEW_OK)
            {
                OpenGLDefaultInternalTextureFormat = GL_SRGB8_ALPHA8;
                glEnable(GL_FRAMEBUFFER_SRGB);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
                glfwSwapInterval(1);
                glGenTextures(1, &OpenGLReservedBlitTexture);
                
                int FramebufferWidth = 0;
                int FramebufferHeight = 0;
                glfwGetFramebufferSize(MainWindow, &FramebufferWidth, &FramebufferHeight);
                glViewport(0, 0, FramebufferWidth, FramebufferHeight);
                glfwSetFramebufferSizeCallback(MainWindow, Win32FramebufferSizeCallback);

                // NOTE(paul): Threads initialization  ===========================================
                win32_thread_startup HighPriStartups[3] = {};
                platform_work_queue HighPriorityQueue = {};
                Win32MakeQueue(&HighPriorityQueue, ArrayCount(HighPriStartups), HighPriStartups);

                win32_thread_startup LowPriStartups[3] = {};
                platform_work_queue LowPriorityQueue = {};
                Win32MakeQueue(&LowPriorityQueue, ArrayCount(LowPriStartups), LowPriStartups);

                // NOTE(paul): Setting refresh rate  ===========================================
                // TODO(casey): How do we reliably query on this on Windows?
                int MonitorRefreshHz = VideoMode->refreshRate;
                real32 EditorUpdateHz = (real32)(MonitorRefreshHz);
                real32 TargetSecondsPerFrame = 1.0f / (real32)EditorUpdateHz;

                GlobalRunning = true;

                memory_arena FrameTempArena = {};
            
                // TODO(casey): Decide what our pushbuffer size is!
                u32 PushBufferSize = Megabytes(64);
                void *PushBuffer = Win32AllocateMemory(PushBufferSize);

#if EDITOR_INTERNAL
                LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else
                LPVOID BaseAddress = 0;
#endif

                // NOTE(paul): API Initialization  ===========================================
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
                EditorMemory.PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
                EditorMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
#endif
                // =================================================================================

                // NOTE(paul): Allocating Texture Array ============================================
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
                
                
                editor_input Input[2] = {};
                editor_input *NewInput = &Input[0];
                editor_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();

                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[30] = {0};

                win32_editor_code Editor = Win32LoadEditorCode(SourceEditorCodeDLLFullPath,
                                                               TempEditorCodeDLLFullPath,
                                                               EditorCodeLockFullPath);
                DEBUGSetEventRecording(Editor.IsValid);

                // NOTE(paul): Main Loop ============================================
                while(GlobalRunning && !glfwWindowShouldClose(MainWindow))
                {
                    {DEBUG_DATA_BLOCK("Platform/Controls");
                        DEBUG_B32(GlobalPause);
                    }
                    //
                    //
                    //

                    NewInput->dtForFrame = TargetSecondsPerFrame;

                    int width, height;
                    glfwGetFramebufferSize(MainWindow, &width, &height);

                    editor_render_commands RenderCommands = RenderCommandStruct(
                        PushBufferSize, PushBuffer,
                        (u32)width,
                        (u32)height);

                    glfwSetWindowAspectRatio(MainWindow, 16, 9);

                    int WindowWidth = 0;
                    int WindowHeight = 0;
                    glfwGetWindowSize(MainWindow, &WindowWidth, &WindowHeight);

                    rectangle2i DrawRegion = AspectRatioFit(RenderCommands.Width, RenderCommands.Height,
                                                            WindowWidth, WindowHeight);

#if 0
                    BEGIN_BLOCK("Input Processing");

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
#endif

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
                    
                    
                    glClearColor(0.2f, 0.0f, 0.2f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                    Win32DisplayBufferInWindow(MainWindow, &HighPriorityQueue, &RenderCommands,
                                               DrawRegion, WindowWidth, WindowHeight, &FrameTempArena);
                    glfwPollEvents();

                    END_BLOCK();

                    FlipWallClock = Win32GetWallClock();

                    editor_input *Temp = NewInput;
                    NewInput = OldInput;
                    OldInput = Temp;
#if 0
                    if(glfwGetKey(MainWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                    {
                        GlobalRunning = false;
//                        glfwSetWindowMonitor(MainWindow, 0, 100, 100, 1920, 1080, 0);
                    }

#endif

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

                    LARGE_INTEGER EndCounter = Win32GetWallClock();                    
                    FRAME_MARKER(Win32GetSecondsElapsed(LastCounter, EndCounter));
                    LastCounter = EndCounter;
                }
                
            }
            else
            {
                glfwDestroyWindow(MainWindow);
                // TODO(paul): Logging
            }
        }
        else
        {
            // TODO(paul): Logging
        }
    }
    else
    {
        const char *Description;
        int ErrorCode = glfwGetError(&Description);
        if(ErrorCode != GLFW_NO_ERROR)
        {
            // TODO(paul): Handle Error
            /*
              glfwSetErrorCallback(error_callback);
              void error_callback(int code, const char* description)
              {
                  display_error_message(code, description);
              }
              
             */
        }
    }

    glfwTerminate();
    
    return(0);
}

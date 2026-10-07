/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice:  $
   ======================================================================== */

/*
  NOTE(pvlso): Windows implementation of platform_os.h

  Paths come in as UTF-8 with '/' separators, they are converted to UTF-16
  with '\\' separators before they reach the OS (LoadLibrary doesn't accept '/').
*/

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): STRINGS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal s32
Win32UTF8ToWidePath(char *Source, wchar_t *Dest, s32 DestCount)
{
    s32 Result = MultiByteToWideChar(CP_UTF8, 0, Source, -1, Dest, DestCount);
    if(Result == 0)
    {
        Dest[0] = 0;
    }

    for(wchar_t *At = Dest; *At; ++At)
    {
        if(*At == L'/')
        {
            *At = L'\\';
        }
    }

    return(Result);
}

internal s32
Win32WideToUTF8(wchar_t *Source, char *Dest, s32 DestSize)
{
    s32 Result = WideCharToMultiByte(CP_UTF8, 0, Source, -1, Dest, DestSize, 0, 0);
    if(Result == 0)
    {
        Dest[0] = 0;
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

internal os_info
OSInit(void)
{
    os_info Result = {};

    // NOTE(casey): Set the Windows scheduler granularity to 1ms
    // so that our Sleep() can be more granular.
    Result.SleepIsGranular = (timeBeginPeriod(1) == TIMERR_NOERROR);

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): MEMORY
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void *
OSAllocateMemory(umm Size)
{
    void *Result = VirtualAlloc(0, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    return(Result);
}

internal void
OSDeallocateMemory(void *Memory)
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
// NOTE(pvlso): TIME
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
OSSleep(s32 Milliseconds)
{
    if(Milliseconds > 0)
    {
        Sleep(Milliseconds);
    }
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): PATHS & FILE SYSTEM
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
OSGetEXEFileName(char *Dest, umm DestCount)
{
    wchar_t EXEFileNameW[WIN32_OS_PATH_COUNT];
    DWORD Length = GetModuleFileNameW(0, EXEFileNameW, ArrayCount(EXEFileNameW));
    EXEFileNameW[(Length < ArrayCount(EXEFileNameW)) ? Length : 0] = 0;
    Win32WideToUTF8(EXEFileNameW, Dest, (s32)DestCount);
}

internal b32
OSSetWorkingDirectory(char *Path)
{
    wchar_t PathW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(Path, PathW, ArrayCount(PathW));
    b32 Result = SetCurrentDirectoryW(PathW);
    return(Result);
}

internal b32
OSFileExists(char *Path)
{
    wchar_t PathW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(Path, PathW, ArrayCount(PathW));
    b32 Result = (GetFileAttributesW(PathW) != INVALID_FILE_ATTRIBUTES);
    return(Result);
}

internal void
OSCreateDirectory(char *Path)
{
    wchar_t PathW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(Path, PathW, ArrayCount(PathW));
    CreateDirectoryW(PathW, 0);
}

internal void
OSDeleteFile(char *Path)
{
    wchar_t PathW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(Path, PathW, ArrayCount(PathW));
    DeleteFileW(PathW);
}

internal b32
OSCopyFile(char *SourcePath, char *DestPath)
{
    wchar_t SourceW[WIN32_OS_PATH_COUNT];
    wchar_t DestW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(SourcePath, SourceW, ArrayCount(SourceW));
    Win32UTF8ToWidePath(DestPath, DestW, ArrayCount(DestW));
    b32 Result = CopyFileW(SourceW, DestW, FALSE);
    return(Result);
}

internal u64
OSGetLastWriteTime(char *Path)
{
    u64 Result = 0;

    wchar_t PathW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(Path, PathW, ArrayCount(PathW));

    WIN32_FILE_ATTRIBUTE_DATA Data;
    if(GetFileAttributesExW(PathW, GetFileExInfoStandard, &Data))
    {
        Result = (((u64)Data.ftLastWriteTime.dwHighDateTime << 32) |
                  ((u64)Data.ftLastWriteTime.dwLowDateTime));
    }

    return(Result);
}

internal b32
OSOpenDirectory(os_directory *Directory, char *Path)
{
    char Pattern[WIN32_OS_PATH_COUNT];
    snprintf(Pattern, sizeof(Pattern), "%s/*", Path);

    wchar_t PatternW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(Pattern, PatternW, ArrayCount(PatternW));

    Directory->FindHandle = FindFirstFileW(PatternW, &Directory->FindData);
    Directory->HasPendingEntry = (Directory->FindHandle != INVALID_HANDLE_VALUE);

    b32 Result = Directory->HasPendingEntry;
    return(Result);
}

internal char *
OSNextFileInDirectory(os_directory *Directory)
{
    char *Result = 0;

    while(!Result && Directory->HasPendingEntry)
    {
        if(!(Directory->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            Win32WideToUTF8(Directory->FindData.cFileName, Directory->FileName, sizeof(Directory->FileName));
            Result = Directory->FileName;
        }

        Directory->HasPendingEntry = (FindNextFileW(Directory->FindHandle, &Directory->FindData) != 0);
    }

    return(Result);
}

internal char *
OSNextDirectoryInDirectory(os_directory *Directory)
{
    char *Result = 0;

    while(!Result && Directory->HasPendingEntry)
    {
        if((Directory->FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
           (Directory->FindData.cFileName[0] != L'.'))
        {
            Win32WideToUTF8(Directory->FindData.cFileName, Directory->FileName, sizeof(Directory->FileName));
            Result = Directory->FileName;
        }

        Directory->HasPendingEntry = (FindNextFileW(Directory->FindHandle, &Directory->FindData) != 0);
    }

    return(Result);
}

internal void
OSCloseDirectory(os_directory *Directory)
{
    if(Directory->FindHandle != INVALID_HANDLE_VALUE)
    {
        FindClose(Directory->FindHandle);
        Directory->FindHandle = INVALID_HANDLE_VALUE;
    }
    Directory->HasPendingEntry = false;
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): FILES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal b32
OSOpenFile(os_file *File, char *Path, platform_file_op Op)
{
    wchar_t PathW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(Path, PathW, ArrayCount(PathW));

    switch(Op)
    {
        case PlatformFileOp_Read:
        {
            File->Handle = CreateFileW(PathW, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
        } break;

        case PlatformFileOp_Write:
        {
            File->Handle = CreateFileW(PathW, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, 0, 0);
        } break;

        case PlatformFileOp_WriteExisting:
        {
            File->Handle = CreateFileW(PathW, GENERIC_WRITE, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        } break;

        InvalidDefaultCase;
    }

    b32 Result = (File->Handle != INVALID_HANDLE_VALUE);
    return(Result);
}

internal void
OSCloseFile(os_file *File)
{
    if(File->Handle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(File->Handle);
        File->Handle = INVALID_HANDLE_VALUE;
    }
}

internal b32
OSReadFile(os_file *File, u64 Offset, u64 Size, void *Dest)
{
    OVERLAPPED Overlapped = {};
    Overlapped.Offset = (u32)((Offset >> 0) & 0xFFFFFFFF);
    Overlapped.OffsetHigh = (u32)((Offset >> 32) & 0xFFFFFFFF);

    u32 Size32 = SafeTruncateUInt64(Size);

    DWORD BytesRead;
    b32 Result = (ReadFile(File->Handle, Dest, Size32, &BytesRead, &Overlapped) &&
                  (Size32 == BytesRead));
    return(Result);
}

internal b32
OSWriteFile(os_file *File, u64 Offset, u64 Size, void *Data)
{
    OVERLAPPED Overlapped = {};
    Overlapped.Offset = (u32)((Offset >> 0) & 0xFFFFFFFF);
    Overlapped.OffsetHigh = (u32)((Offset >> 32) & 0xFFFFFFFF);

    u32 Size32 = SafeTruncateUInt64(Size);

    DWORD BytesWritten;
    b32 Result = (WriteFile(File->Handle, Data, Size32, &BytesWritten, &Overlapped) &&
                  (Size32 == BytesWritten));
    return(Result);
}

internal u64
OSGetFileSize(os_file *File)
{
    u64 Result = (u64)-1;

    LARGE_INTEGER FileSize;
    if(GetFileSizeEx(File->Handle, &FileSize))
    {
        Result = FileSize.QuadPart;
    }

    return(Result);
}

internal void
OSSeekFile(os_file *File, s64 Offset, platform_file_seek_op Op)
{
    LARGE_INTEGER Distance;
    Distance.QuadPart = Offset;
    switch(Op)
    {
        case PlatformFileSeek_Set:     SetFilePointerEx(File->Handle, Distance, 0, FILE_BEGIN); break;
        case PlatformFileSeek_Current: SetFilePointerEx(File->Handle, Distance, 0, FILE_CURRENT); break;
        case PlatformFileSeek_End:     SetFilePointerEx(File->Handle, Distance, 0, FILE_END); break;

        InvalidDefaultCase;
    }
}

internal u64
OSTellFile(os_file *File)
{
    u64 Result = 0;

    LARGE_INTEGER Zero = {};
    LARGE_INTEGER Current;
    if(SetFilePointerEx(File->Handle, Zero, &Current, FILE_CURRENT))
    {
        Result = Current.QuadPart;
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): CODE LOADING
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal os_library
OSLoadLibrary(char *Path)
{
    wchar_t PathW[WIN32_OS_PATH_COUNT];
    Win32UTF8ToWidePath(Path, PathW, ArrayCount(PathW));
    os_library Result = (os_library)LoadLibraryW(PathW);
    return(Result);
}

internal void
OSUnloadLibrary(os_library Library)
{
    FreeLibrary((HMODULE)Library);
}

internal void *
OSGetLibrarySymbol(os_library Library, char *Name)
{
    void *Result = (void *)GetProcAddress((HMODULE)Library, Name);
    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): THREADS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
struct win32_thread_startup
{
    os_thread_proc *Proc;
    void *Parameter;
};

DWORD WINAPI
Win32ThreadProc(LPVOID Parameter)
{
    win32_thread_startup Startup = *(win32_thread_startup *)Parameter;
    OSDeallocateMemory(Parameter);

    Startup.Proc(Startup.Parameter);

    return(0);
}

internal void
OSCreateThread(os_thread_proc *Proc, void *Parameter)
{
    win32_thread_startup *Startup = (win32_thread_startup *)OSAllocateMemory(sizeof(win32_thread_startup));
    if(Startup)
    {
        Startup->Proc = Proc;
        Startup->Parameter = Parameter;

        DWORD ThreadID;
        HANDLE ThreadHandle = CreateThread(0, 0, Win32ThreadProc, Startup, 0, &ThreadID);
        if(ThreadHandle)
        {
            CloseHandle(ThreadHandle);
        }
        else
        {
            OSDeallocateMemory(Startup);
        }
    }
}

internal void
OSInitSemaphore(os_semaphore *Semaphore, u32 MaxCount)
{
    *Semaphore = CreateSemaphoreExW(0, 0, MaxCount, 0, 0, SEMAPHORE_ALL_ACCESS);
}

internal void
OSSignalSemaphore(os_semaphore *Semaphore)
{
    ReleaseSemaphore(*Semaphore, 1, 0);
}

internal void
OSWaitSemaphore(os_semaphore *Semaphore)
{
    WaitForSingleObjectEx(*Semaphore, INFINITE, FALSE);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): PROCESSES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#if EDITOR_INTERNAL
#define WIN32_INVALID_PROCESS_HANDLE ((u64)-1)

internal
DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(OSDEBUGExecuteSystemCommand)
{
    debug_executing_process Result = {};
    Result.OSHandle = WIN32_INVALID_PROCESS_HANDLE;

    STARTUPINFOA StartupInfo = {};
    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION ProcessInfo = {};
    if(CreateProcessA(Command, CommandLine, 0, 0, FALSE, 0, 0, Path, &StartupInfo, &ProcessInfo))
    {
        Assert(sizeof(Result.OSHandle) >= sizeof(ProcessInfo.hProcess));
        *(HANDLE *)&Result.OSHandle = ProcessInfo.hProcess;
        CloseHandle(ProcessInfo.hThread);
    }

    return(Result);
}

internal
DEBUG_PLATFORM_GET_PROCESS_STATE(OSDEBUGGetProcessState)
{
    debug_process_state Result = {};

    if(Process.OSHandle != WIN32_INVALID_PROCESS_HANDLE)
    {
        Result.StartedSuccessfully = true;

        HANDLE ProcessHandle = *(HANDLE *)&Process.OSHandle;
        if(WaitForSingleObject(ProcessHandle, 0) == WAIT_OBJECT_0)
        {
            DWORD ReturnCode = 0;
            GetExitCodeProcess(ProcessHandle, &ReturnCode);
            Result.ReturnCode = ReturnCode;
            CloseHandle(ProcessHandle);
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

#if !defined(PLATFORM_OS_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The things GLFW and miniaudio don't cover. Every OS file
  (linux_os.cpp, win32_os.cpp) implements all of these, nothing above this
  layer touches the OS directly.

  Each OS header also defines:
    os_file          - an open file
    os_directory     - a directory being iterated
    os_semaphore     - a counting semaphore
    OS_LIBRARY_EXTENSION - ".so", ".dll"

  Paths are UTF-8 and use '/' as the separator, the OS file converts if it has to.
*/

typedef void *os_library;
typedef void os_thread_proc(void *Parameter);

struct os_info
{
    // NOTE(pvlso): OSSleep wakes up within about a millisecond of the requested time
    b32 SleepIsGranular;
};

internal os_info OSInit(void);

//
// NOTE(pvlso): Memory
//
internal void *OSAllocateMemory(umm Size);
internal void OSDeallocateMemory(void *Memory);

//
// NOTE(pvlso): Time
//
internal void OSSleep(s32 Milliseconds);

//
// NOTE(pvlso): Paths and file system
//
internal void OSGetEXEFileName(char *Dest, umm DestCount);
internal b32 OSSetWorkingDirectory(char *Path);
internal b32 OSFileExists(char *Path);
internal void OSCreateDirectory(char *Path);
internal void OSDeleteFile(char *Path);
internal b32 OSCopyFile(char *SourcePath, char *DestPath);
// NOTE(pvlso): 0 when the file doesn't exist
internal u64 OSGetLastWriteTime(char *Path);

// NOTE(pvlso): Lists regular files only. The returned name lives until the next call.
internal b32 OSOpenDirectory(os_directory *Directory, char *Path);
internal char *OSNextFileInDirectory(os_directory *Directory);
internal void OSCloseDirectory(os_directory *Directory);

//
// NOTE(pvlso): Files
//
internal b32 OSOpenFile(os_file *File, char *Path, platform_file_op Op);
internal void OSCloseFile(os_file *File);
// NOTE(pvlso): Read/Write go to an explicit offset and leave the file position right
// after the data, so OSSeekFile/OSTellFile keep working when mixed with them.
internal b32 OSReadFile(os_file *File, u64 Offset, u64 Size, void *Dest);
internal b32 OSWriteFile(os_file *File, u64 Offset, u64 Size, void *Data);
// NOTE(pvlso): (u64)-1 on failure
internal u64 OSGetFileSize(os_file *File);
internal void OSSeekFile(os_file *File, s64 Offset, platform_file_seek_op Op);
internal u64 OSTellFile(os_file *File);

//
// NOTE(pvlso): Code loading
//
internal os_library OSLoadLibrary(char *Path);
internal void OSUnloadLibrary(os_library Library);
internal void *OSGetLibrarySymbol(os_library Library, char *Name);

//
// NOTE(pvlso): Threads
//
internal void OSCreateThread(os_thread_proc *Proc, void *Parameter);
internal void OSInitSemaphore(os_semaphore *Semaphore, u32 MaxCount);
internal void OSSignalSemaphore(os_semaphore *Semaphore);
internal void OSWaitSemaphore(os_semaphore *Semaphore);

//
// NOTE(pvlso): Processes
//
#if EDITOR_INTERNAL
internal DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(OSDEBUGExecuteSystemCommand);
internal DEBUG_PLATFORM_GET_PROCESS_STATE(OSDEBUGGetProcessState);
#endif

#define PLATFORM_OS_H
#endif

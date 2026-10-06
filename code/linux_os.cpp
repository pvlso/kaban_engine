/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice:  $
   ======================================================================== */

/*
  NOTE(pvlso): Linux implementation of platform_os.h
*/

internal os_info
OSInit(void)
{
    os_info Result = {};

    // NOTE(pvlso): A crashed DEBUGExecuteSystemCommand child or a closed pipe shouldn't kill us
    signal(SIGPIPE, SIG_IGN);

    Result.SleepIsGranular = true;

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): MEMORY
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): munmap needs the size back, so it's stored in front of the block.
// 64 bytes keeps the returned memory cache line (and AVX) aligned.
#define LINUX_ALLOCATION_HEADER_SIZE 64

internal void *
OSAllocateMemory(umm Size)
{
    void *Result = 0;

    umm TotalSize = Size + LINUX_ALLOCATION_HEADER_SIZE;
    void *Block = mmap(0, TotalSize, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if(Block != MAP_FAILED)
    {
        *(umm *)Block = TotalSize;
        Result = (u8 *)Block + LINUX_ALLOCATION_HEADER_SIZE;
    }

    return(Result);
}

internal void
OSDeallocateMemory(void *Memory)
{
    if(Memory)
    {
        void *Block = (u8 *)Memory - LINUX_ALLOCATION_HEADER_SIZE;
        munmap(Block, *(umm *)Block);
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
        struct timespec Request;
        Request.tv_sec = Milliseconds / 1000;
        Request.tv_nsec = (long)(Milliseconds % 1000)*1000000;
        while((nanosleep(&Request, &Request) == -1) && (errno == EINTR)) {}
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
    ssize_t Size = readlink("/proc/self/exe", Dest, DestCount - 1);
    if(Size < 0)
    {
        Size = 0;
    }
    Dest[Size] = 0;
}

internal b32
OSSetWorkingDirectory(char *Path)
{
    b32 Result = (chdir(Path) == 0);
    return(Result);
}

internal b32
OSFileExists(char *Path)
{
    struct stat Stat;
    b32 Result = (stat(Path, &Stat) == 0);
    return(Result);
}

internal void
OSCreateDirectory(char *Path)
{
    mkdir(Path, 0755);
}

internal void
OSDeleteFile(char *Path)
{
    unlink(Path);
}

internal b32
OSCopyFile(char *SourcePath, char *DestPath)
{
    b32 Result = false;

    int Source = open(SourcePath, O_RDONLY);
    if(Source != -1)
    {
        int Dest = open(DestPath, O_WRONLY|O_CREAT|O_TRUNC, 0755);
        if(Dest != -1)
        {
            Result = true;

            u8 Buffer[65536];
            for(;;)
            {
                ssize_t BytesRead = read(Source, Buffer, sizeof(Buffer));
                if(BytesRead == 0)
                {
                    break;
                }
                else if(BytesRead < 0)
                {
                    if(errno == EINTR) continue;
                    Result = false;
                    break;
                }

                ssize_t BytesWritten = 0;
                while(BytesWritten < BytesRead)
                {
                    ssize_t Written = write(Dest, Buffer + BytesWritten, BytesRead - BytesWritten);
                    if(Written < 0)
                    {
                        if(errno == EINTR) continue;
                        Result = false;
                        break;
                    }
                    BytesWritten += Written;
                }

                if(!Result)
                {
                    break;
                }
            }

            close(Dest);
        }

        close(Source);
    }

    return(Result);
}

internal u64
OSGetLastWriteTime(char *Path)
{
    u64 Result = 0;

    struct stat Stat;
    if(stat(Path, &Stat) == 0)
    {
        Result = ((u64)Stat.st_mtim.tv_sec*1000000000ull) + (u64)Stat.st_mtim.tv_nsec;
    }

    return(Result);
}

internal b32
OSOpenDirectory(os_directory *Directory, char *Path)
{
    snprintf(Directory->Path, sizeof(Directory->Path), "%s", Path);
    Directory->Handle = opendir(Path);

    b32 Result = (Directory->Handle != 0);
    return(Result);
}

internal char *
OSNextFileInDirectory(os_directory *Directory)
{
    char *Result = 0;

    if(Directory->Handle)
    {
        while(struct dirent *Entry = readdir(Directory->Handle))
        {
            if(Entry->d_name[0] == '.')
            {
                continue;
            }

            char FilePath[4096];
            snprintf(FilePath, sizeof(FilePath), "%s/%s", Directory->Path, Entry->d_name);
            struct stat Stat;
            if((stat(FilePath, &Stat) == 0) && S_ISREG(Stat.st_mode))
            {
                Result = Entry->d_name;
                break;
            }
        }
    }

    return(Result);
}

internal void
OSCloseDirectory(os_directory *Directory)
{
    if(Directory->Handle)
    {
        closedir(Directory->Handle);
        Directory->Handle = 0;
    }
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
    switch(Op)
    {
        case PlatformFileOp_Read:
        {
            File->FileDescriptor = open(Path, O_RDONLY);
        } break;

        case PlatformFileOp_Write:
        {
            File->FileDescriptor = open(Path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
        } break;

        case PlatformFileOp_WriteExisting:
        {
            File->FileDescriptor = open(Path, O_WRONLY);
        } break;

        InvalidDefaultCase;
    }

    b32 Result = (File->FileDescriptor != -1);
    return(Result);
}

internal void
OSCloseFile(os_file *File)
{
    if(File->FileDescriptor != -1)
    {
        close(File->FileDescriptor);
        File->FileDescriptor = -1;
    }
}

internal b32
OSReadFile(os_file *File, u64 Offset, u64 Size, void *Dest)
{
    u64 TotalRead = 0;
    while(TotalRead < Size)
    {
        ssize_t BytesRead = pread(File->FileDescriptor, (u8 *)Dest + TotalRead,
                                  Size - TotalRead, Offset + TotalRead);
        if(BytesRead < 0)
        {
            if(errno == EINTR) continue;
            break;
        }
        else if(BytesRead == 0)
        {
            break;
        }

        TotalRead += BytesRead;
    }

    lseek(File->FileDescriptor, Offset + TotalRead, SEEK_SET);

    b32 Result = (TotalRead == Size);
    return(Result);
}

internal b32
OSWriteFile(os_file *File, u64 Offset, u64 Size, void *Data)
{
    u64 TotalWritten = 0;
    while(TotalWritten < Size)
    {
        ssize_t BytesWritten = pwrite(File->FileDescriptor, (u8 *)Data + TotalWritten,
                                      Size - TotalWritten, Offset + TotalWritten);
        if(BytesWritten < 0)
        {
            if(errno == EINTR) continue;
            break;
        }

        TotalWritten += BytesWritten;
    }

    lseek(File->FileDescriptor, Offset + TotalWritten, SEEK_SET);

    b32 Result = (TotalWritten == Size);
    return(Result);
}

internal u64
OSGetFileSize(os_file *File)
{
    u64 Result = (u64)-1;

    struct stat Stat;
    if(fstat(File->FileDescriptor, &Stat) == 0)
    {
        Result = Stat.st_size;
    }

    return(Result);
}

internal void
OSSeekFile(os_file *File, s64 Offset, platform_file_seek_op Op)
{
    switch(Op)
    {
        case PlatformFileSeek_Set:     lseek(File->FileDescriptor, (off_t)Offset, SEEK_SET); break;
        case PlatformFileSeek_Current: lseek(File->FileDescriptor, (off_t)Offset, SEEK_CUR); break;
        case PlatformFileSeek_End:     lseek(File->FileDescriptor, (off_t)Offset, SEEK_END); break;

        InvalidDefaultCase;
    }
}

internal u64
OSTellFile(os_file *File)
{
    u64 Result = 0;

    off_t Current = lseek(File->FileDescriptor, 0, SEEK_CUR);
    if(Current >= 0)
    {
        Result = (u64)Current;
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
    os_library Result = dlopen(Path, RTLD_NOW|RTLD_LOCAL);
#if EDITOR_INTERNAL
    if(!Result)
    {
        fprintf(stderr, "LINUX OS: %s\n", dlerror());
    }
#endif

    return(Result);
}

internal void
OSUnloadLibrary(os_library Library)
{
    dlclose(Library);
}

internal void *
OSGetLibrarySymbol(os_library Library, char *Name)
{
    void *Result = dlsym(Library, Name);
    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): THREADS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
struct linux_thread_startup
{
    os_thread_proc *Proc;
    void *Parameter;
};

internal void *
LinuxThreadProc(void *Parameter)
{
    linux_thread_startup Startup = *(linux_thread_startup *)Parameter;
    OSDeallocateMemory(Parameter);

    Startup.Proc(Startup.Parameter);

    return(0);
}

internal void
OSCreateThread(os_thread_proc *Proc, void *Parameter)
{
    linux_thread_startup *Startup = (linux_thread_startup *)OSAllocateMemory(sizeof(linux_thread_startup));
    if(Startup)
    {
        Startup->Proc = Proc;
        Startup->Parameter = Parameter;

        pthread_t ThreadHandle;
        if(pthread_create(&ThreadHandle, 0, LinuxThreadProc, Startup) == 0)
        {
            pthread_detach(ThreadHandle);
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
    sem_init(Semaphore, 0, 0);
}

internal void
OSSignalSemaphore(os_semaphore *Semaphore)
{
    sem_post(Semaphore);
}

internal void
OSWaitSemaphore(os_semaphore *Semaphore)
{
    while((sem_wait(Semaphore) == -1) && (errno == EINTR)) {}
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): PROCESSES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#if EDITOR_INTERNAL
#define LINUX_INVALID_PROCESS_HANDLE ((u64)-1)

internal
DEBUG_PLATFORM_EXECUTE_SYSTEM_COMMAND(OSDEBUGExecuteSystemCommand)
{
    debug_executing_process Result = {};
    Result.OSHandle = LINUX_INVALID_PROCESS_HANDLE;

    // NOTE(pvlso): CommandLine goes through the shell, like CreateProcess does with a 0 Command
    pid_t ProcessID = fork();
    if(ProcessID == 0)
    {
        if(Path && (chdir(Path) != 0))
        {
            _exit(127);
        }

        if(CommandLine)
        {
            execl("/bin/sh", "sh", "-c", CommandLine, (char *)0);
        }
        else if(Command)
        {
            execl(Command, Command, (char *)0);
        }

        _exit(127);
    }
    else if(ProcessID > 0)
    {
        Result.OSHandle = (u64)ProcessID;
    }

    return(Result);
}

internal
DEBUG_PLATFORM_GET_PROCESS_STATE(OSDEBUGGetProcessState)
{
    debug_process_state Result = {};

    if(Process.OSHandle != LINUX_INVALID_PROCESS_HANDLE)
    {
        Result.StartedSuccessfully = true;

        int Status = 0;
        pid_t WaitResult = waitpid((pid_t)Process.OSHandle, &Status, WNOHANG);
        if(WaitResult == 0)
        {
            Result.IsRunning = true;
        }
        else if((WaitResult > 0) && WIFEXITED(Status))
        {
            Result.ReturnCode = WEXITSTATUS(Status);
        }
    }

    return(Result);
}
#endif
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

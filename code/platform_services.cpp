/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice:  $
   ======================================================================== */

/*
  NOTE(pvlso): Implementation of the platform_api (engine_platform.h). Only uses
  platform_os.h, so it's the same code on every OS.
*/

global_variable platform_paths GlobalPaths;
global_variable u32 GlobalTempLibraryCounter;

global_variable char *GlobalDataDirs[PlatformFileType_Count] =
{
    "",             "keas",         "kesas",      "kewms",      "kets",
    "bmps",         "spritesheets", "tilesets",   "solidtiles", "wavs",
    "txts",         "jsons",        "ttfs",       "bins",       ""
};

global_variable char *GlobalFileExtentionsForType[PlatformFileType_Count] =
{
    ".*",    ".kea",  ".kesa", ".kewm", ".ket",
    ".bmp",  ".bmp",  ".bmp",  ".bmp",  ".wav",
    ".txt",  ".json", ".ttf",  ".bin",  ".*"
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): PATHS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline b32
IsPathSeparator(char C)
{
    b32 Result = ((C == '/') || (C == '\\'));
    return(Result);
}

internal b32
StringEndsWithNoCase(char *String, char *Ending)
{
    b32 Result = false;

    umm StringLen = StringLength(String);
    umm EndingLen = StringLength(Ending);
    if(StringLen >= EndingLen)
    {
        Result = true;
        char *A = String + (StringLen - EndingLen);
        for(char *B = Ending; *B; ++A, ++B)
        {
            char CA = ((*A >= 'A') && (*A <= 'Z')) ? (*A - 'A' + 'a') : *A;
            char CB = ((*B >= 'A') && (*B <= 'Z')) ? (*B - 'A' + 'a') : *B;
            if(CA != CB)
            {
                Result = false;
                break;
            }
        }
    }

    return(Result);
}

internal void
PlatformBuildEXEPathFileName(char *FileName, char *Dest, umm DestCount)
{
    int DirLength = (int)(GlobalPaths.OnePastLastEXEFileNameSlash - GlobalPaths.EXEFileName);
    snprintf(Dest, DestCount, "%.*s%s", DirLength, GlobalPaths.EXEFileName, FileName);
}

// NOTE(pvlso): The directory files of a type live in, without a trailing separator
internal void
PlatformBuildTypeDirectory(platform_file_type Type, char *Dest, umm DestCount)
{
    if(Type == PlatformFileType_Project)
    {
        snprintf(Dest, DestCount, "%sprojects", GlobalPaths.ROOTPath);
    }
    else
    {
        snprintf(Dest, DestCount, "%s%s", GlobalPaths.DATAPath, GlobalDataDirs[Type]);
    }
}

// NOTE(pvlso): PlatformFileType_None means the name is relative to the exe directory
internal void
PlatformBuildDataPath(char *FileName, platform_file_type Type, char *Dest, umm DestCount)
{
    if(Type != PlatformFileType_None)
    {
        char Directory[PLATFORM_PATH_COUNT];
        PlatformBuildTypeDirectory(Type, Directory, sizeof(Directory));
        snprintf(Dest, DestCount, "%s/%s", Directory, FileName);
    }
    else
    {
        PlatformBuildEXEPathFileName(FileName, Dest, DestCount);
    }
}

// NOTE(pvlso): The data directory sits next to the build directory the exe lives in
internal void
PlatformInitPaths(void)
{
    OSGetEXEFileName(GlobalPaths.EXEFileName, sizeof(GlobalPaths.EXEFileName));

    GlobalPaths.OnePastLastEXEFileNameSlash = GlobalPaths.EXEFileName;
    for(char *Scan = GlobalPaths.EXEFileName; *Scan; ++Scan)
    {
        if(IsPathSeparator(*Scan))
        {
            GlobalPaths.OnePastLastEXEFileNameSlash = Scan + 1;
        }
    }

    char *SlashBeforeBuild = GlobalPaths.OnePastLastEXEFileNameSlash;
    char *BuildString = "build";
    for(char *Scan = GlobalPaths.EXEFileName; *Scan; ++Scan)
    {
        if(IsPathSeparator(*Scan))
        {
            char *Test = Scan + 1;
            char *C = BuildString;
            while((*Test == *C) && *C)
            {
                Test++;
                C++;
            }

            if(IsPathSeparator(*Test) && (*C == 0))
            {
                SlashBeforeBuild = Scan + 1;
                break;
            }
        }
    }

    int RootLength = (int)(SlashBeforeBuild - GlobalPaths.EXEFileName);
    snprintf(GlobalPaths.ROOTPath, sizeof(GlobalPaths.ROOTPath), "%.*s",
             RootLength, GlobalPaths.EXEFileName);
    snprintf(GlobalPaths.DATAPath, sizeof(GlobalPaths.DATAPath), "%sdata/", GlobalPaths.ROOTPath);
    OSCreateDirectory(GlobalPaths.DATAPath);

    // NOTE(pvlso): Creates data/<type dir> for every type and projects/
    char DirPath[PLATFORM_PATH_COUNT];
    for(u32 Type = 1;
        Type < PlatformFileType_Count;
        ++Type)
    {
        PlatformBuildTypeDirectory((platform_file_type)Type, DirPath, sizeof(DirPath));
        OSCreateDirectory(DirPath);
    }

    // NOTE(pvlso): Third party code (nuklear fonts) opens files relative to the working directory
    char EXEDirectory[PLATFORM_PATH_COUNT];
    PlatformBuildEXEPathFileName(".", EXEDirectory, sizeof(EXEDirectory));
    if(!OSSetWorkingDirectory(EXEDirectory))
    {
        fprintf(stderr, "PLATFORM: Could not change directory to %s\n", EXEDirectory);
    }
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): FILE API
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Collects the names (not paths) of all files of a type in its data directory.
// Dest and Arena can be 0 to only count.
internal u32
PlatformCollectFilesOfType(platform_file_type Type, char **Dest, memory_arena *Arena, u32 MaxCount)
{
    u32 FileCount = 0;

    char Path[PLATFORM_PATH_COUNT];
    PlatformBuildTypeDirectory(Type, Path, sizeof(Path));

    os_directory Directory;
    if(OSOpenDirectory(&Directory, Path))
    {
        while(char *FileName = OSNextFileInDirectory(&Directory))
        {
            if((Type != PlatformFileType_None) &&
               !StringEndsWithNoCase(FileName, GlobalFileExtentionsForType[Type]))
            {
                continue;
            }

            if(FileCount >= MaxCount)
            {
                break;
            }

            if(Dest && Arena)
            {
                Dest[FileCount] = PushString(Arena, FileName);
            }

            ++FileCount;
        }
    }
    OSCloseDirectory(&Directory);

    return(FileCount);
}

internal os_file *
PlatformOpenOSFile(char *Path, platform_file_op Op, b32 *NoErrors)
{
    *NoErrors = false;

    os_file *File = (os_file *)OSAllocateMemory(sizeof(os_file));
    if(File)
    {
        *NoErrors = OSOpenFile(File, Path, Op);
    }

    return(File);
}

internal void
PlatformCloseOSFile(os_file *File)
{
    if(File)
    {
        OSCloseFile(File);
        OSDeallocateMemory(File);
    }
}

internal PLATFORM_GET_ALL_FILE_OF_TYPE_BEGIN(PlatformGetAllFilesOfTypeBegin)
{
    platform_file_group Result = {};

    platform_file_group_data *FileGroup = (platform_file_group_data *)OSAllocateMemory(sizeof(platform_file_group_data));
    if(FileGroup)
    {
        FileGroup->Type = Type;

        u32 MaxCount = PlatformCollectFilesOfType(Type, 0, 0, U32Maximum);
        if(MaxCount)
        {
            FileGroup->FileNames = (char **)PushArray(&FileGroup->Arena, MaxCount, char *);
            FileGroup->FileCount = PlatformCollectFilesOfType(Type, FileGroup->FileNames, &FileGroup->Arena, MaxCount);
        }

        Result.FileCount = FileGroup->FileCount;
        Result.Platform = FileGroup;
    }

    return(Result);
}

internal PLATFORM_GET_ALL_FILE_OF_TYPE_END(PlatformGetAllFilesOfTypeEnd)
{
    platform_file_group_data *FileGroupData = (platform_file_group_data *)FileGroup->Platform;
    if(FileGroupData)
    {
        Clear(&FileGroupData->Arena);
        OSDeallocateMemory(FileGroupData);
        FileGroup->Platform = 0;
    }
}

internal PLATFORM_OPEN_NEXT_FILE(PlatformOpenNextFile)
{
    platform_file_group_data *FileGroupData = (platform_file_group_data *)FileGroup->Platform;
    platform_file_handle Result = {};

    if(FileGroupData && (FileGroupData->NextFileIndex < FileGroupData->FileCount))
    {
        char *FileName = FileGroupData->FileNames[FileGroupData->NextFileIndex++];

        char Path[PLATFORM_PATH_COUNT];
        PlatformBuildDataPath(FileName, FileGroupData->Type, Path, sizeof(Path));
        Result.Platform = PlatformOpenOSFile(Path, PlatformFileOp_Read, &Result.NoErrors);
    }

    return(Result);
}

internal PLATFORM_OPEN_FILE(PlatformOpenFile)
{
    platform_file_handle Result = {};

    char Path[PLATFORM_PATH_COUNT];
    PlatformBuildDataPath(FileName, Type, Path, sizeof(Path));
    Result.Platform = PlatformOpenOSFile(Path, Op, &Result.NoErrors);

    return(Result);
}

internal PLATFORM_CLOSE_FILE(PlatformCloseFile)
{
    PlatformCloseOSFile((os_file *)Handle->Platform);
    Handle->Platform = 0;
}

internal PLATFORM_FILE_ERROR(PlatformFileError)
{
#if EDITOR_INTERNAL
    fprintf(stderr, "PLATFORM FILE ERROR: %s\n", Message);
#endif

    Handle->NoErrors = false;
}

internal PLATFORM_READ_DATA_FROM_FILE(PlatformReadDataFromFile)
{
    if(PlatformNoFileErrors(Source))
    {
        if(!OSReadFile((os_file *)Source->Platform, Offset, Size, Dest))
        {
            PlatformFileError(Source, "Read file failed.");
        }
    }
}

internal PLATFORM_WRITE_DATA_TO_FILE(PlatformWriteDataToFile)
{
    if(PlatformNoFileErrors(Source))
    {
        if(!OSWriteFile((os_file *)Source->Platform, Offset, Size, Data))
        {
            PlatformFileError(Source, "Write file failed.");
        }
    }
}

internal PLATFORM_SEEK_FILE(PlatformSeekFile)
{
    if(PlatformNoFileErrors(Handle))
    {
        // NOTE(pvlso): For PlatformFileSeek_End the offset counts back from the end
        s64 Distance = (Op == PlatformFileSeek_End) ? -(s64)Offset : (s64)Offset;
        OSSeekFile((os_file *)Handle->Platform, Distance, Op);
    }
}

internal PLATFORM_FILE_TELL(PlatformFileTell)
{
    u64 Result = 0;
    if(PlatformNoFileErrors(Handle))
    {
        Result = OSTellFile((os_file *)Handle->Platform);
    }

    return(Result);
}

internal PLATFORM_LIST_FILES_IN_DIRECTORY(PlatformListFilesInDirectory)
{
    u32 FileCount = PlatformCollectFilesOfType(Type, Dest, Arena, U32Maximum);
    return(FileCount);
}

internal PLATFORM_LIST_PROJECTS(PlatformListProjects)
{
    u32 ProjectCount = 0;

    char ProjectsPath[PLATFORM_PATH_COUNT];
    PlatformBuildTypeDirectory(PlatformFileType_Project, ProjectsPath, sizeof(ProjectsPath));

    os_directory Directory;
    if(OSOpenDirectory(&Directory, ProjectsPath))
    {
        while(char *Name = OSNextDirectoryInDirectory(&Directory))
        {
            char MainFile[PLATFORM_PATH_COUNT];
            snprintf(MainFile, sizeof(MainFile), "%s/%s/%s.cpp", ProjectsPath, Name, Name);
            if(OSFileExists(MainFile))
            {
                if(Dest && Arena)
                {
                    if(ProjectCount >= DestCount)
                    {
                        break;
                    }

                    Dest[ProjectCount] = PushString(Arena, Name);
                }

                ++ProjectCount;
            }
        }
    }
    OSCloseDirectory(&Directory);

    return(ProjectCount);
}

internal PLATFORM_MAKE_DIRECTORY(PlatformMakeDirectory)
{
    b32 Result = false;

    char FullPath[PLATFORM_PATH_COUNT];
    PlatformBuildDataPath(Path, Type, FullPath, sizeof(FullPath));
    if(!OSFileExists(FullPath))
    {
        OSCreateDirectory(FullPath);
        Result = OSFileExists(FullPath);
    }

    return(Result);
}

internal PLATFORM_FREE_FILE_MEMORY(PlatformFreeFileMemory)
{
    OSDeallocateMemory(Memory);
}

internal PLATFORM_READ_ENTIRE_FILE(PlatformReadEntireFile)
{
    read_file_result Result = {};

    char Path[PLATFORM_PATH_COUNT];
    PlatformBuildDataPath(FileName, Type, Path, sizeof(Path));

    b32 NoErrors = false;
    os_file *File = PlatformOpenOSFile(Path, PlatformFileOp_Read, &NoErrors);
    if(NoErrors)
    {
        u64 FileSize = OSGetFileSize(File);
        if(FileSize != (u64)-1)
        {
            u32 FileSize32 = SafeTruncateUInt64(FileSize) + (IsTXT ? 1 : 0);
            u32 ReadSize32 = IsTXT ? (FileSize32 - 1) : FileSize32;
            Result.Contents = (Arena ? PushSize(Arena, FileSize32) : OSAllocateMemory(FileSize32));
            if(Result.Contents)
            {
                if(OSReadFile(File, 0, ReadSize32, Result.Contents))
                {
                    // NOTE(casey): File read successfully
                    Result.Size = FileSize32;
                    if(IsTXT)
                    {
                        ((u8 *)Result.Contents)[ReadSize32] = 0;
                    }
                }
                else
                {
                    // TODO: Logging
                    if(!Arena)
                    {
                        PlatformFreeFileMemory(Result.Contents);
                    }
                    Result.Contents = 0;
                }
            }
        }
    }

    PlatformCloseOSFile(File);

    return(Result);
}

internal PLATFORM_WRITE_ENTIRE_FILE(PlatformWriteEntireFile)
{
    u32 Result = 0;

    char Path[PLATFORM_PATH_COUNT];
    PlatformBuildDataPath(FileName, Type, Path, sizeof(Path));

    b32 NoErrors = false;
    os_file *File = PlatformOpenOSFile(Path, PlatformFileOp_Write, &NoErrors);
    if(NoErrors)
    {
        if(OSWriteFile(File, 0, Size, Data))
        {
            Result = Size;
        }
    }

    PlatformCloseOSFile(File);

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): MULTITHREADING & QUEUES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
PlatformAddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data)
{
    // TODO(casey): Switch to InterlockedCompareExchange eventually
    // so that any thread can add?
    uint32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
    platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
    Entry->Callback = Callback;
    Entry->Data = Data;
    ++Queue->CompletionGoal;
    CompletePreviousWritesBeforeFutureWrites;
    Queue->NextEntryToWrite = NewNextEntryToWrite;
    OSSignalSemaphore(&Queue->SemaphoreHandle);
}

internal b32
PlatformDoNextWorkQueueEntry(platform_work_queue *Queue)
{
    b32 WeShouldSleep = false;

    uint32 OriginalNextEntryToRead = Queue->NextEntryToRead;
    uint32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);
    if(OriginalNextEntryToRead != Queue->NextEntryToWrite)
    {
        uint32 Index = AtomicCompareExchangeUInt32(&Queue->NextEntryToRead,
                                                   NewNextEntryToRead,
                                                   OriginalNextEntryToRead);
        if(Index == OriginalNextEntryToRead)
        {
            platform_work_queue_entry Entry = Queue->Entries[Index];
            Entry.Callback(Queue, Entry.Data);
            AtomicIncrementU32(&Queue->CompletionCount);
        }
    }
    else
    {
        WeShouldSleep = true;
    }

    return(WeShouldSleep);
}

internal void
PlatformCompleteAllWork(platform_work_queue *Queue)
{
    while(Queue->CompletionGoal != Queue->CompletionCount)
    {
        PlatformDoNextWorkQueueEntry(Queue);
    }

    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;
}

internal void
PlatformWorkerThread(void *Parameter)
{
    platform_work_queue *Queue = (platform_work_queue *)Parameter;

    for(;;)
    {
        if(PlatformDoNextWorkQueueEntry(Queue))
        {
            OSWaitSemaphore(&Queue->SemaphoreHandle);
        }
    }
}

internal void
PlatformMakeQueue(platform_work_queue *Queue, uint32 ThreadCount)
{
    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;

    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;

    OSInitSemaphore(&Queue->SemaphoreHandle, ThreadCount);

    for(uint32 ThreadIndex = 0;
        ThreadIndex < ThreadCount;
        ++ThreadIndex)
    {
        OSCreateThread(PlatformWorkerThread, Queue);
    }
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): CODE LOADING
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
internal void
PlatformInitCode(platform_code *Code, char *ModuleName)
{
    *Code = {};

    char FileName[PLATFORM_PATH_COUNT];
    snprintf(FileName, sizeof(FileName), "%s%s", ModuleName, OS_LIBRARY_EXTENSION);
    PlatformBuildEXEPathFileName(FileName, Code->SourceLibraryName, sizeof(Code->SourceLibraryName));

    // NOTE(pvlso): Every load gets its own temp file ("engine_temp_3.so"). dlopen hands
    // back the already loaded library for a path it has seen if the old one couldn't be
    // fully unloaded, which would silently skip the reload.
    snprintf(FileName, sizeof(FileName), "%s_temp_%u%s", ModuleName, GlobalTempLibraryCounter++, OS_LIBRARY_EXTENSION);
    PlatformBuildEXEPathFileName(FileName, Code->TempLibraryName, sizeof(Code->TempLibraryName));

    // NOTE(pvlso): The build writes lock.tmp while a library is being written
    PlatformBuildEXEPathFileName("lock.tmp", Code->LockFileName, sizeof(Code->LockFileName));
}

internal b32
PlatformLoadCodeFile(platform_code *Code, char *ModuleName)
{
    PlatformInitCode(Code, ModuleName);

    if(!OSFileExists(Code->LockFileName))
    {
        Code->LibraryLastWriteTime = OSGetLastWriteTime(Code->SourceLibraryName);
#if EDITOR_INTERNAL
        if(OSCopyFile(Code->SourceLibraryName, Code->TempLibraryName))
        {
            Code->Library = OSLoadLibrary(Code->TempLibraryName);
        }
#else
        Code->TempLibraryName[0] = 0;
        Code->Library = OSLoadLibrary(Code->SourceLibraryName);
#endif
        Code->IsValid = (Code->Library != 0);
    }

    return(Code->IsValid);
}

internal void
PlatformUnloadCodeFile(platform_code *Code)
{
    if(Code->Library)
    {
        OSUnloadLibrary(Code->Library);
        Code->Library = 0;
    }

    if(Code->TempLibraryName[0])
    {
        OSDeleteFile(Code->TempLibraryName);
        Code->TempLibraryName[0] = 0;
    }

    Code->IsValid = false;
}

internal b32
PlatformCodeNeedsReload(platform_code *Code)
{
    u64 NewLibraryWriteTime = OSGetLastWriteTime(Code->SourceLibraryName);
    b32 Result = ((NewLibraryWriteTime != 0) &&
                  (NewLibraryWriteTime != Code->LibraryLastWriteTime) &&
                  !OSFileExists(Code->LockFileName));
    return(Result);
}

internal
PLATFORM_LOAD_CODE(PlatformLoadCode)
{
    platform_loaded_code Result = {};

    platform_code *Code = (platform_code *)OSAllocateMemory(sizeof(platform_code));
    if(Code)
    {
        Result.NoErrors = PlatformLoadCodeFile(Code, ModuleName);
        Result.Platform = Code;
    }

    return(Result);
}

internal
PLATFORM_UNLOAD_CODE(PlatformUnloadCode)
{
    platform_code *PlatformCode = (platform_code *)Code->Platform;
    if(PlatformCode)
    {
        PlatformUnloadCodeFile(PlatformCode);
        OSDeallocateMemory(PlatformCode);
        Code->Platform = 0;
    }

    Code->NoErrors = false;
}

internal
PLATFORM_GET_PROC_ADDRESS(PlatformGetProcAddress)
{
    void *Result = 0;

    platform_code *PlatformCode = (platform_code *)Code->Platform;
    if(PlatformCode && PlatformCode->Library)
    {
        Result = OSGetLibrarySymbol(PlatformCode->Library, FunctionName);
    }
    else
    {
        Code->NoErrors = false;
    }

    return(Result);
}

internal platform_engine_code
PlatformLoadEngineCode(char *ModuleName)
{
    platform_engine_code Result = {};

    if(PlatformLoadCodeFile(&Result.Code, ModuleName))
    {
        Result.UpdateAndRender = (engine_update_and_render *)
            OSGetLibrarySymbol(Result.Code.Library, "EngineUpdateAndRender");

        Result.GetSoundSamples = (engine_get_sound_samples *)
            OSGetLibrarySymbol(Result.Code.Library, "EngineGetSoundSamples");

        Result.DEBUGFrameEnd = (debug_editor_frame_end *)
            OSGetLibrarySymbol(Result.Code.Library, "DEBUGEditorFrameEnd");

        Result.IsValid = (Result.UpdateAndRender &&
                          Result.GetSoundSamples &&
                          Result.DEBUGFrameEnd);
    }

    if(!Result.IsValid)
    {
        PlatformUnloadCodeFile(&Result.Code);
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
        Result.DEBUGFrameEnd = 0;
    }

    return(Result);
}

internal void
PlatformUnloadEngineCode(platform_engine_code *EngineCode)
{
    PlatformUnloadCodeFile(&EngineCode->Code);

    EngineCode->IsValid = false;
    EngineCode->UpdateAndRender = 0;
    EngineCode->GetSoundSamples = 0;
    EngineCode->DEBUGFrameEnd = 0;
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

internal void
PlatformInitAPI(engine_memory *Memory, platform_work_queue *HighPQ, platform_work_queue *LowPQ)
{
    Memory->HighPriorityQueue = HighPQ;
    Memory->LowPriorityQueue = LowPQ;
    Memory->PlatformAPI.AddEntry = PlatformAddEntry;
    Memory->PlatformAPI.CompleteAllWork = PlatformCompleteAllWork;

    Memory->PlatformAPI.GetAllFilesOfTypeBegin = PlatformGetAllFilesOfTypeBegin;
    Memory->PlatformAPI.GetAllFilesOfTypeEnd = PlatformGetAllFilesOfTypeEnd;
    Memory->PlatformAPI.OpenNextFile = PlatformOpenNextFile;
    Memory->PlatformAPI.OpenFile = PlatformOpenFile;
    Memory->PlatformAPI.CloseFile = PlatformCloseFile;
    Memory->PlatformAPI.ReadDataFromFile = PlatformReadDataFromFile;
    Memory->PlatformAPI.WriteDataToFile = PlatformWriteDataToFile;
    Memory->PlatformAPI.FileError = PlatformFileError;
    Memory->PlatformAPI.ListFilesInDirectory = PlatformListFilesInDirectory;
    Memory->PlatformAPI.ListProjects = PlatformListProjects;
    Memory->PlatformAPI.MakeDirectory = PlatformMakeDirectory;

    Memory->PlatformAPI.ReadEntireFile = PlatformReadEntireFile;
    Memory->PlatformAPI.WriteEntireFile = PlatformWriteEntireFile;
    Memory->PlatformAPI.FreeFileMemory = PlatformFreeFileMemory;
    Memory->PlatformAPI.FileSeek = PlatformSeekFile;
    Memory->PlatformAPI.FileTell = PlatformFileTell;

    Memory->PlatformAPI.AllocateMemory = OSAllocateMemory;
    Memory->PlatformAPI.DeallocateMemory = OSDeallocateMemory;

    Memory->PlatformAPI.LoadCode = PlatformLoadCode;
    Memory->PlatformAPI.UnloadCode = PlatformUnloadCode;
    Memory->PlatformAPI.GetProcAddress = PlatformGetProcAddress;
    Memory->PlatformAPI.Sleep = OSSleep;

#if EDITOR_INTERNAL
    Memory->DebugTable = GlobalDebugTable;
    Memory->PlatformAPI.DEBUGExecuteSystemCommand = OSDEBUGExecuteSystemCommand;
    Memory->PlatformAPI.DEBUGGetProcessState = OSDEBUGGetProcessState;
#endif
}

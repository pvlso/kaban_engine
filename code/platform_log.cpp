/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The platform side of the log (engine_debug_interface.h). Internal builds only.
  Every entry goes into GlobalDebugTable->Log and is appended to kaban.log next to the exe,
  which is truncated at startup. Times come from glfwGetTime, so PlatformInitLog has to run
  after glfwInit.
*/

#if EDITOR_INTERNAL

global_variable os_file GlobalLogFile;
global_variable b32 GlobalLogFileIsOpen;
global_variable u64 GlobalLogFileSize;

internal void
PlatformLogCopyString(char *Dest, umm DestCount, char const *Source)
{
    umm At = 0;
    if(Source)
    {
        while(Source[At] && ((At + 1) < DestCount))
        {
            Dest[At] = Source[At];
            ++At;
        }
    }
    Dest[At] = 0;
}

internal
DEBUG_LOG_WRITE(PlatformLogWrite)
{
    debug_log *Log = &GlobalDebugTable->Log;

    r64 Seconds = glfwGetTime();

    char Message[ArrayCount(Log->Entries[0].Message)];
    va_list ArgList;
    va_start(ArgList, Format);
    vsnprintf(Message, sizeof(Message), Format, ArgList);
    va_end(ArgList);

    // NOTE(pvlso): __FILE__ can be a full path, only the file name is kept
    char const *FileName = File ? File : "";
    for(char const *At = FileName; *At; ++At)
    {
        if(IsPathSeparator(*At))
        {
            FileName = At + 1;
        }
    }

    BeginTicketMutex(&Log->Mutex);

    debug_log_entry *Entry = Log->Entries + (Log->EntryCount % DEBUG_LOG_ENTRY_COUNT);
    Entry->Seconds = Seconds;
    Entry->Level = Level;
    PlatformLogCopyString(Entry->FileName, sizeof(Entry->FileName), FileName);
    PlatformLogCopyString(Entry->Function, sizeof(Entry->Function), Function);
    PlatformLogCopyString(Entry->Message, sizeof(Entry->Message), Message);
    ++Log->EntryCount;

    if(GlobalLogFileIsOpen)
    {
        char const *LevelName = ((Level == DebugLog_Error) ? "ERROR" :
                                 (Level == DebugLog_Warning) ? "WARNING" : "INFO");

        char Line[512];
        int LineLength = snprintf(Line, sizeof(Line), "%10.3fs  %-7s  %s  %s  %s\n",
                                  Seconds, LevelName, Entry->FileName, Entry->Function, Entry->Message);
        if(LineLength > 0)
        {
            // NOTE(pvlso): snprintf returns the length it wanted, a cut line is sizeof(Line) - 1
            u64 Size = ((umm)LineLength < sizeof(Line)) ? (u64)LineLength : (u64)(sizeof(Line) - 1);
            if(OSWriteFile(&GlobalLogFile, GlobalLogFileSize, Size, Line))
            {
                GlobalLogFileSize += Size;
            }
        }
    }

    EndTicketMutex(&Log->Mutex);
}

internal void
PlatformInitLog(void)
{
    char Path[PLATFORM_PATH_COUNT];
    PlatformBuildDataPath("kaban.log", PlatformFileType_None, Path, sizeof(Path));
    GlobalLogFileIsOpen = OSOpenFile(&GlobalLogFile, Path, PlatformFileOp_Write);
    GlobalLogFileSize = 0;

    GlobalDebugTable->Log.Write = PlatformLogWrite;
}

internal void
PlatformShutdownLog(void)
{
    BeginTicketMutex(&GlobalDebugTable->Log.Mutex);
    GlobalDebugTable->Log.Write = 0;
    if(GlobalLogFileIsOpen)
    {
        OSCloseFile(&GlobalLogFile);
        GlobalLogFileIsOpen = false;
    }
    EndTicketMutex(&GlobalDebugTable->Log.Mutex);
}

#else

internal void PlatformInitLog(void) {}
internal void PlatformShutdownLog(void) {}

#endif

#if !defined(PLATFORM_SERVICES_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

#define PLATFORM_PATH_COUNT 4096

struct platform_paths
{
    char EXEFileName[PLATFORM_PATH_COUNT];
    char *OnePastLastEXEFileNameSlash;

    // NOTE(pvlso): Ends with a separator
    char DATAPath[PLATFORM_PATH_COUNT];
};

struct platform_work_queue_entry
{
    platform_work_queue_callback *Callback;
    void *Data;
};

struct platform_work_queue
{
    uint32 volatile CompletionGoal;
    uint32 volatile CompletionCount;

    uint32 volatile NextEntryToWrite;
    uint32 volatile NextEntryToRead;
    os_semaphore SemaphoreHandle;

    platform_work_queue_entry Entries[256];
};

struct platform_file_group_data
{
    memory_arena Arena;
    char **FileNames;
    u32 FileCount;
    u32 NextFileIndex;
    platform_file_type Type;
};

struct platform_code
{
    os_library Library;
    u64 LibraryLastWriteTime;
    char SourceLibraryName[PLATFORM_PATH_COUNT];
    char TempLibraryName[PLATFORM_PATH_COUNT];
    char LockFileName[PLATFORM_PATH_COUNT];
    b32 IsValid;
};

struct platform_engine_code
{
    platform_code Code;

    // IMPORTANT(casey): Either of the callbacks can be 0!  You must
    // check before calling.
    engine_update_and_render *UpdateAndRender;
    engine_get_sound_samples *GetSoundSamples;
    debug_editor_frame_end *DEBUGFrameEnd;

    b32 IsValid;
};

#define PLATFORM_SERVICES_H
#endif

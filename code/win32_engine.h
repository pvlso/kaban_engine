#if !defined(WIN32_EDITOR_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct win32_window_dimension
{
    s32 Width;
    s32 Height;
};

struct win32_loaded_code
{
    HMODULE DLL;
    FILETIME DLLLastWriteTime;
    b32 IsValid;
};

struct win32_engine_code
{
    HMODULE EditorCodeDLL;
    FILETIME DLLLastWriteTime;

    // IMPORTANT(casey): Either of the callbacks can be 0!  You must
    // check before calling.
    engine_update_and_render *UpdateAndRender;
    engine_get_sound_samples *GetSoundSamples;
    debug_editor_frame_end *DEBUGFrameEnd;

    bool32 IsValid;
};

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int BytesPerSample;
    DWORD SecondaryBufferSize;
    DWORD SafetyBytes;

    // TODO(casey): Should running sample index be in bytes as well
    // TODO(casey): Math gets simpler if we add a "bytes per second" field?
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;

    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};

// NOTE(pvlso): The nuklear backend state is shared with the renderer and the GLFW layer
typedef nk_platform nk_win32;

struct win32_window
{
    HWND Handle;
    nk_win32 Nk;

    WCHAR highSurrogate;
    
    char MouseButtons[WIN32_MOUSE_BUTTON_LAST + 1];
    char keys[WIN32_KEY_LAST + 1];

    s32 cursorMode;
    s32 lastCursorPosX, lastCursorPosY;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_state
{
    wchar_t EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
    wchar_t *OnePastLastEXEFileNameSlash;

    win32_window *CurrentWindow;

    b32 lockKeyMods;

    s16 Keycodes[512];
    s16 Scancodes[WIN32_KEY_LAST + 1];

    win32_window MainWindow;
    char *clipboardString;

#if EDITOR_INTERNAL
    win32_window DebugWindow;
#endif
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
    HANDLE SemaphoreHandle;

    platform_work_queue_entry Entries[256];
};

struct win32_thread_startup
{
    platform_work_queue *Queue;
};

struct win32_platform_file_handle
{
    HANDLE Win32Handle;
};

struct win32_platform_file_group
{
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
};

#define WIN32_EDITOR_H
#endif

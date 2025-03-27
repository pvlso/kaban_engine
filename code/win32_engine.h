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

struct nk_opengl
{
    struct nk_buffer cmds;
    struct nk_draw_null_texture tex_null;
    GLuint font_tex;
};

struct nk_gl_vertex
{
    float position[2];
    float uv[2];
    nk_byte col[4];
};

struct nk_win32
{
    int width, height;
    int display_width, display_height;

    struct nk_opengl ogl;
    struct nk_context ctx;
    struct nk_font_atlas atlas;
    struct nk_vec2 fb_scale;

    unsigned int text[NK_WIN32_TEXT_MAX];
    nk_char key_events[NK_KEY_MAX];

    int text_len;
    struct nk_vec2 scroll;
    double last_button_click;
    int is_double_click_down;
    struct nk_vec2 double_click_pos;
    float delta_time_seconds_last;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_state
{
    wchar_t EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
    wchar_t *OnePastLastEXEFileNameSlash;
    
    HWND WindowHandle;

    WCHAR highSurrogate;
    b32 lockKeyMods;

    s16 Keycodes[512];
    s16 Scancodes[WIN32_KEY_LAST + 1];

    char MouseButtons[WIN32_MOUSE_BUTTON_LAST + 1];
    char keys[WIN32_KEY_LAST + 1];

    s32 cursorMode;
    s32 lastCursorPosX, lastCursorPosY;
    char *clipboardString;

    nk_win32 Main;
#if EDITOR_INTERNAL
    nk_win32 Debug;
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

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

// NOTE(paul): FONT
#define ONE_PAST_MAX_FONT_CODEPOINT (0x10FFFF + 1)
#define MAX_FONT_WIDTH 1024
#define MAX_FONT_HEIGHT 1024

#pragma pack(push, 1)
struct ttf_offset_subtable
{
    u32 ScalerType;
    u16 NumTables;
    u16 SearchRange;
    u16 EntrySelector;
    u16 RangeShift;
};

struct ttf_table_directory
{
    u32 Tag;
    u32 CheckSum;
    u32 Offset;
    u32 Length;
};

struct ttf_name_record
{
    u16 PlatformID;
    u16 EncodingID;
    u16 LanguageID;
    u16 NameID;
    u16 Length;
    u16 Offset;
};
#pragma pack(pop)

inline u16
ReadU16(u8 *Data, u32 Offset)
{
    u16 Result = (Data[Offset] << 8) | (Data[Offset + 1]);
    return(Result);
}

inline u32
ReadU32(u8 *Data, u32 Offset)
{
    u32 Result = ((Data[Offset] << 24) | (Data[Offset + 1] << 16) |
                  (Data[Offset + 2] << 8) | (Data[Offset + 3]));
    return(Result);
}

struct win32_loaded_font
{
    HFONT Win32Handle;
    TEXTMETRIC TextMetric;
    r32 LineAdvance;

    u32 *Glyphs;
    r32 *HorizontalAdvance;

    u32 MinCodePoint;
    u32 MaxCodePoint;

    u32 MaxGlyphCount;
    u32 GlyphCount;

    u32 *GlyphIndexFromCodePoint;
    u32 OnePastHighestCodePoint;
};

#define WIN32_EDITOR_H
#endif

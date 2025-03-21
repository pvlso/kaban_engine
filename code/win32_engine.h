#if !defined(WIN32_EDITOR_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#define GLFW_MOD_MASK (GLFW_MOD_SHIFT | \
                       GLFW_MOD_CONTROL | \
                       GLFW_MOD_ALT | \
                       GLFW_MOD_SUPER | \
                       GLFW_MOD_CAPS_LOCK | \
                       GLFW_MOD_NUM_LOCK)

struct win32_window_dimension
{
    s32 Width;
    s32 Height;
};

struct win32_editor_code
{
    HMODULE EditorCodeDLL;
    FILETIME DLLLastWriteTime;

    // IMPORTANT(casey): Either of the callbacks can be 0!  You must
    // check before calling.
    engine_update_and_render *UpdateAndRender;
    debug_editor_frame_end *DEBUGFrameEnd;

    bool32 IsValid;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_state
{
    wchar_t EXEFileName[WIN32_STATE_FILE_NAME_COUNT];
    wchar_t *OnePastLastEXEFileNameSlash;

    HWND Handle;
    
    WCHAR highSurrogate;
    b32 lockKeyMods;
    b32 stickyKeys;
    b32 stickyMouseButtons;
    b32 keymenu;

    short int           keycodes[512];
    short int           scancodes[GLFW_KEY_LAST + 1];
    char                keynames[GLFW_KEY_LAST + 1][5];

    char                mouseButtons[GLFW_MOUSE_BUTTON_LAST + 1];
    char                keys[GLFW_KEY_LAST + 1];

    int                 cursorMode;
    double              virtualCursorPosX, virtualCursorPosY;
    int                 lastCursorPosX, lastCursorPosY;
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

void glfwGetCursorPos(win32_state *State, double *x, double *y);
double glfwGetTime(void);
const char * glfwGetClipboardString(void);
void glfwSetClipboardString(const char *str);
int glfwGetKey(win32_state *State, int keycode);
void glfwSetCursorPos(win32_state *State, double x, double y);
int glfwGetMouseButton(win32_state *State, int buttoncode);

#define WIN32_EDITOR_H
#endif

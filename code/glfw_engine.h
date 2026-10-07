#if !defined(GLFW_ENGINE_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

#define NK_GLFW_DOUBLE_CLICK_LO 0.02
#define NK_GLFW_DOUBLE_CLICK_HI 0.2

#define GLFW_DEBUG_WINDOW_X 200
#define GLFW_DEBUG_WINDOW_Y 200
#define GLFW_DEBUG_WINDOW_WIDTH 960
#define GLFW_DEBUG_WINDOW_HEIGHT 540

// NOTE(pvlso): Every OS window has its own nuklear input state, events go to the
// window they came from.
struct glfw_window
{
    GLFWwindow *Handle;
    nk_platform Nk;
};

struct glfw_state
{
    GLFWmonitor *Monitor;

    // NOTE(pvlso): Windowed position and size of the main window, restored when leaving fullscreen
    s32 WindowedX, WindowedY;
    s32 WindowedWidth, WindowedHeight;

    // NOTE(pvlso): Set for the duration of glfwPollEvents so the key callback
    // can update the keyboard controller
    engine_controller_input *KeyboardController;
    r64 ScrollY;

    glfw_window MainWindow;
#if EDITOR_INTERNAL
    // NOTE(pvlso): Separate OS window for the profiler, shares the main window's GL objects
    glfw_window DebugWindow;
    // NOTE(pvlso): First non primary monitor, the debug window goes fullscreen there. 0 with one monitor.
    GLFWmonitor *DebugMonitor;
#endif
};

#define GLFW_ENGINE_H
#endif

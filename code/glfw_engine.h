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

struct glfw_state
{
    GLFWwindow *Window;
    GLFWmonitor *Monitor;

    // NOTE(pvlso): Windowed position and size, restored when leaving fullscreen
    s32 WindowedX, WindowedY;
    s32 WindowedWidth, WindowedHeight;

    // NOTE(pvlso): Set for the duration of glfwPollEvents so the key callback
    // can update the keyboard controller
    engine_controller_input *KeyboardController;
    r64 ScrollY;

    nk_platform NkMain;
#if EDITOR_INTERNAL
    nk_platform NkDebug;
#endif
};

#define GLFW_ENGINE_H
#endif

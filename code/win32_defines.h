#if !defined(WIN32_DEFINES_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

/*************************************************************************
 * GLFW API tokens
 *************************************************************************/

/*! @name GLFW version macros
 *  @{ */
/*! @brief The major version number of the GLFW header.
 *
 *  The major version number of the GLFW header.  This is incremented when the
 *  API is changed in non-compatible ways.
 *  @ingroup init
 */
#define WIN32_VERSION_MAJOR          3
/*! @brief The minor version number of the GLFW header.
 *
 *  The minor version number of the GLFW header.  This is incremented when
 *  features are added to the API but it remains backward-compatible.
 *  @ingroup init
 */
#define WIN32_VERSION_MINOR          4
/*! @brief The revision number of the GLFW header.
 *
 *  The revision number of the GLFW header.  This is incremented when a bug fix
 *  release is made that does not contain any API changes.
 *  @ingroup init
 */
#define WIN32_VERSION_REVISION       0
/*! @} */

/*! @brief One.
 *
 *  This is only semantic sugar for the number 1.  You can instead use `1` or
 *  `true` or `_True` or `GL_TRUE` or `VK_TRUE` or anything else that is equal
 *  to one.
 *
 *  @ingroup init
 */
#define WIN32_TRUE                   1
/*! @brief Zero.
 *
 *  This is only semantic sugar for the number 0.  You can instead use `0` or
 *  `false` or `_False` or `GL_FALSE` or `VK_FALSE` or anything else that is
 *  equal to zero.
 *
 *  @ingroup init
 */
#define WIN32_FALSE                  0

/*! @name Key and button actions
 *  @{ */
/*! @brief The key or mouse button was released.
 *
 *  The key or mouse button was released.
 *
 *  @ingroup input
 */
#define WIN32_RELEASE                0
/*! @brief The key or mouse button was pressed.
 *
 *  The key or mouse button was pressed.
 *
 *  @ingroup input
 */
#define WIN32_PRESS                  1
/*! @brief The key was held down until it repeated.
 *
 *  The key was held down until it repeated.
 *
 *  @ingroup input
 */
#define WIN32_REPEAT                 2
/*! @} */

/*! @defgroup hat_state Joystick hat states
 *  @brief Joystick hat states.
 *
 *  See [joystick hat input](@ref joystick_hat) for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define WIN32_HAT_CENTERED           0
#define WIN32_HAT_UP                 1
#define WIN32_HAT_RIGHT              2
#define WIN32_HAT_DOWN               4
#define WIN32_HAT_LEFT               8
#define WIN32_HAT_RIGHT_UP           (WIN32_HAT_RIGHT | WIN32_HAT_UP)
#define WIN32_HAT_RIGHT_DOWN         (WIN32_HAT_RIGHT | WIN32_HAT_DOWN)
#define WIN32_HAT_LEFT_UP            (WIN32_HAT_LEFT  | WIN32_HAT_UP)
#define WIN32_HAT_LEFT_DOWN          (WIN32_HAT_LEFT  | WIN32_HAT_DOWN)

/*! @ingroup input
 */
#define WIN32_KEY_UNKNOWN            -1

/*! @} */

/*! @defgroup keys Keyboard key tokens
 *  @brief Keyboard key tokens.
 *
 *  See [key input](@ref input_key) for how these are used.
 *
 *  These key codes are inspired by the _USB HID Usage Tables v1.12_ (p. 53-60),
 *  but re-arranged to map to 7-bit ASCII for printable keys (function keys are
 *  put in the 256+ range).
 *
 *  The naming of the key codes follow these rules:
 *   - The US keyboard layout is used
 *   - Names of printable alphanumeric characters are used (e.g. "A", "R",
 *     "3", etc.)
 *   - For non-alphanumeric characters, Unicode:ish names are used (e.g.
 *     "COMMA", "LEFT_SQUARE_BRACKET", etc.). Note that some names do not
 *     correspond to the Unicode standard (usually for brevity)
 *   - Keys that lack a clear US mapping are named "WORLD_x"
 *   - For non-printable keys, custom names are used (e.g. "F4",
 *     "BACKSPACE", etc.)
 *
 *  @ingroup input
 *  @{
 */

/* Printable keys */
#define WIN32_KEY_SPACE              32
#define WIN32_KEY_APOSTROPHE         39  /* ' */
#define WIN32_KEY_COMMA              44  /* , */
#define WIN32_KEY_MINUS              45  /* - */
#define WIN32_KEY_PERIOD             46  /* . */
#define WIN32_KEY_SLASH              47  /* / */
#define WIN32_KEY_0                  48
#define WIN32_KEY_1                  49
#define WIN32_KEY_2                  50
#define WIN32_KEY_3                  51
#define WIN32_KEY_4                  52
#define WIN32_KEY_5                  53
#define WIN32_KEY_6                  54
#define WIN32_KEY_7                  55
#define WIN32_KEY_8                  56
#define WIN32_KEY_9                  57
#define WIN32_KEY_SEMICOLON          59  /* ; */
#define WIN32_KEY_EQUAL              61  /* = */
#define WIN32_KEY_A                  65
#define WIN32_KEY_B                  66
#define WIN32_KEY_C                  67
#define WIN32_KEY_D                  68
#define WIN32_KEY_E                  69
#define WIN32_KEY_F                  70
#define WIN32_KEY_G                  71
#define WIN32_KEY_H                  72
#define WIN32_KEY_I                  73
#define WIN32_KEY_J                  74
#define WIN32_KEY_K                  75
#define WIN32_KEY_L                  76
#define WIN32_KEY_M                  77
#define WIN32_KEY_N                  78
#define WIN32_KEY_O                  79
#define WIN32_KEY_P                  80
#define WIN32_KEY_Q                  81
#define WIN32_KEY_R                  82
#define WIN32_KEY_S                  83
#define WIN32_KEY_T                  84
#define WIN32_KEY_U                  85
#define WIN32_KEY_V                  86
#define WIN32_KEY_W                  87
#define WIN32_KEY_X                  88
#define WIN32_KEY_Y                  89
#define WIN32_KEY_Z                  90
#define WIN32_KEY_LEFT_BRACKET       91  /* [ */
#define WIN32_KEY_BACKSLASH          92  /* \ */
#define WIN32_KEY_RIGHT_BRACKET      93  /* ] */
#define WIN32_KEY_GRAVE_ACCENT       96  /* ` */
#define WIN32_KEY_WORLD_1            161 /* non-US #1 */
#define WIN32_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define WIN32_KEY_ESCAPE             256
#define WIN32_KEY_ENTER              257
#define WIN32_KEY_TAB                258
#define WIN32_KEY_BACKSPACE          259
#define WIN32_KEY_INSERT             260
#define WIN32_KEY_DELETE             261
#define WIN32_KEY_RIGHT              262
#define WIN32_KEY_LEFT               263
#define WIN32_KEY_DOWN               264
#define WIN32_KEY_UP                 265
#define WIN32_KEY_PAGE_UP            266
#define WIN32_KEY_PAGE_DOWN          267
#define WIN32_KEY_HOME               268
#define WIN32_KEY_END                269
#define WIN32_KEY_CAPS_LOCK          280
#define WIN32_KEY_SCROLL_LOCK        281
#define WIN32_KEY_NUM_LOCK           282
#define WIN32_KEY_PRINT_SCREEN       283
#define WIN32_KEY_PAUSE              284
#define WIN32_KEY_F1                 290
#define WIN32_KEY_F2                 291
#define WIN32_KEY_F3                 292
#define WIN32_KEY_F4                 293
#define WIN32_KEY_F5                 294
#define WIN32_KEY_F6                 295
#define WIN32_KEY_F7                 296
#define WIN32_KEY_F8                 297
#define WIN32_KEY_F9                 298
#define WIN32_KEY_F10                299
#define WIN32_KEY_F11                300
#define WIN32_KEY_F12                301
#define WIN32_KEY_F13                302
#define WIN32_KEY_F14                303
#define WIN32_KEY_F15                304
#define WIN32_KEY_F16                305
#define WIN32_KEY_F17                306
#define WIN32_KEY_F18                307
#define WIN32_KEY_F19                308
#define WIN32_KEY_F20                309
#define WIN32_KEY_F21                310
#define WIN32_KEY_F22                311
#define WIN32_KEY_F23                312
#define WIN32_KEY_F24                313
#define WIN32_KEY_F25                314
#define WIN32_KEY_KP_0               320
#define WIN32_KEY_KP_1               321
#define WIN32_KEY_KP_2               322
#define WIN32_KEY_KP_3               323
#define WIN32_KEY_KP_4               324
#define WIN32_KEY_KP_5               325
#define WIN32_KEY_KP_6               326
#define WIN32_KEY_KP_7               327
#define WIN32_KEY_KP_8               328
#define WIN32_KEY_KP_9               329
#define WIN32_KEY_KP_DECIMAL         330
#define WIN32_KEY_KP_DIVIDE          331
#define WIN32_KEY_KP_MULTIPLY        332
#define WIN32_KEY_KP_SUBTRACT        333
#define WIN32_KEY_KP_ADD             334
#define WIN32_KEY_KP_ENTER           335
#define WIN32_KEY_KP_EQUAL           336
#define WIN32_KEY_LEFT_SHIFT         340
#define WIN32_KEY_LEFT_CONTROL       341
#define WIN32_KEY_LEFT_ALT           342
#define WIN32_KEY_LEFT_SUPER         343
#define WIN32_KEY_RIGHT_SHIFT        344
#define WIN32_KEY_RIGHT_CONTROL      345
#define WIN32_KEY_RIGHT_ALT          346
#define WIN32_KEY_RIGHT_SUPER        347
#define WIN32_KEY_MENU               348

#define WIN32_KEY_LAST               WIN32_KEY_MENU

/*! @} */

/*! @defgroup mods Modifier key flags
 *  @brief Modifier key flags.
 *
 *  See [key input](@ref input_key) for how these are used.
 *
 *  @ingroup input
 *  @{ */

/*! @brief If this bit is set one or more Shift keys were held down.
 *
 *  If this bit is set one or more Shift keys were held down.
 */
#define WIN32_MOD_SHIFT           0x0001
/*! @brief If this bit is set one or more Control keys were held down.
 *
 *  If this bit is set one or more Control keys were held down.
 */
#define WIN32_MOD_CONTROL         0x0002
/*! @brief If this bit is set one or more Alt keys were held down.
 *
 *  If this bit is set one or more Alt keys were held down.
 */
#define WIN32_MOD_ALT             0x0004
/*! @brief If this bit is set one or more Super keys were held down.
 *
 *  If this bit is set one or more Super keys were held down.
 */
#define WIN32_MOD_SUPER           0x0008
/*! @brief If this bit is set the Caps Lock key is enabled.
 *
 *  If this bit is set the Caps Lock key is enabled and the @ref
 *  WIN32_LOCK_KEY_MODS input mode is set.
 */
#define WIN32_MOD_CAPS_LOCK       0x0010
/*! @brief If this bit is set the Num Lock key is enabled.
 *
 *  If this bit is set the Num Lock key is enabled and the @ref
 *  WIN32_LOCK_KEY_MODS input mode is set.
 */
#define WIN32_MOD_NUM_LOCK        0x0020

/*! @} */

/*! @defgroup buttons Mouse buttons
 *  @brief Mouse button IDs.
 *
 *  See [mouse button input](@ref input_mouse_button) for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define WIN32_MOUSE_BUTTON_1         0
#define WIN32_MOUSE_BUTTON_2         1
#define WIN32_MOUSE_BUTTON_3         2
#define WIN32_MOUSE_BUTTON_4         3
#define WIN32_MOUSE_BUTTON_5         4
#define WIN32_MOUSE_BUTTON_6         5
#define WIN32_MOUSE_BUTTON_7         6
#define WIN32_MOUSE_BUTTON_8         7
#define WIN32_MOUSE_BUTTON_LAST      WIN32_MOUSE_BUTTON_8
#define WIN32_MOUSE_BUTTON_LEFT      WIN32_MOUSE_BUTTON_1
#define WIN32_MOUSE_BUTTON_RIGHT     WIN32_MOUSE_BUTTON_2
#define WIN32_MOUSE_BUTTON_MIDDLE    WIN32_MOUSE_BUTTON_3
/*! @} */

/*! @defgroup joysticks Joysticks
 *  @brief Joystick IDs.
 *
 *  See [joystick input](@ref joystick) for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define WIN32_JOYSTICK_1             0
#define WIN32_JOYSTICK_2             1
#define WIN32_JOYSTICK_3             2
#define WIN32_JOYSTICK_4             3
#define WIN32_JOYSTICK_5             4
#define WIN32_JOYSTICK_6             5
#define WIN32_JOYSTICK_7             6
#define WIN32_JOYSTICK_8             7
#define WIN32_JOYSTICK_9             8
#define WIN32_JOYSTICK_10            9
#define WIN32_JOYSTICK_11            10
#define WIN32_JOYSTICK_12            11
#define WIN32_JOYSTICK_13            12
#define WIN32_JOYSTICK_14            13
#define WIN32_JOYSTICK_15            14
#define WIN32_JOYSTICK_16            15
#define WIN32_JOYSTICK_LAST          WIN32_JOYSTICK_16
/*! @} */

/*! @defgroup gamepad_buttons Gamepad buttons
 *  @brief Gamepad buttons.
 *
 *  See @ref gamepad for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define WIN32_GAMEPAD_BUTTON_A               0
#define WIN32_GAMEPAD_BUTTON_B               1
#define WIN32_GAMEPAD_BUTTON_X               2
#define WIN32_GAMEPAD_BUTTON_Y               3
#define WIN32_GAMEPAD_BUTTON_LEFT_BUMPER     4
#define WIN32_GAMEPAD_BUTTON_RIGHT_BUMPER    5
#define WIN32_GAMEPAD_BUTTON_BACK            6
#define WIN32_GAMEPAD_BUTTON_START           7
#define WIN32_GAMEPAD_BUTTON_GUIDE           8
#define WIN32_GAMEPAD_BUTTON_LEFT_THUMB      9
#define WIN32_GAMEPAD_BUTTON_RIGHT_THUMB     10
#define WIN32_GAMEPAD_BUTTON_DPAD_UP         11
#define WIN32_GAMEPAD_BUTTON_DPAD_RIGHT      12
#define WIN32_GAMEPAD_BUTTON_DPAD_DOWN       13
#define WIN32_GAMEPAD_BUTTON_DPAD_LEFT       14
#define WIN32_GAMEPAD_BUTTON_LAST            WIN32_GAMEPAD_BUTTON_DPAD_LEFT

#define WIN32_GAMEPAD_BUTTON_CROSS       WIN32_GAMEPAD_BUTTON_A
#define WIN32_GAMEPAD_BUTTON_CIRCLE      WIN32_GAMEPAD_BUTTON_B
#define WIN32_GAMEPAD_BUTTON_SQUARE      WIN32_GAMEPAD_BUTTON_X
#define WIN32_GAMEPAD_BUTTON_TRIANGLE    WIN32_GAMEPAD_BUTTON_Y
/*! @} */

/*! @defgroup gamepad_axes Gamepad axes
 *  @brief Gamepad axes.
 *
 *  See @ref gamepad for how these are used.
 *
 *  @ingroup input
 *  @{ */
#define WIN32_GAMEPAD_AXIS_LEFT_X        0
#define WIN32_GAMEPAD_AXIS_LEFT_Y        1
#define WIN32_GAMEPAD_AXIS_RIGHT_X       2
#define WIN32_GAMEPAD_AXIS_RIGHT_Y       3
#define WIN32_GAMEPAD_AXIS_LEFT_TRIGGER  4
#define WIN32_GAMEPAD_AXIS_RIGHT_TRIGGER 5
#define WIN32_GAMEPAD_AXIS_LAST          WIN32_GAMEPAD_AXIS_RIGHT_TRIGGER
/*! @} */

#define WIN32_CURSOR                 0x00033001
#define WIN32_STICKY_KEYS            0x00033002
#define WIN32_STICKY_MOUSE_BUTTONS   0x00033003
#define WIN32_LOCK_KEY_MODS          0x00033004
#define WIN32_RAW_MOUSE_MOTION       0x00033005

#define WIN32_CURSOR_NORMAL          0x00034001
#define WIN32_CURSOR_HIDDEN          0x00034002
#define WIN32_CURSOR_DISABLED        0x00034003
#define WIN32_CURSOR_CAPTURED        0x00034004

#define _WIN32_STICK 3

#define WIN32_DEFINES_H
#endif

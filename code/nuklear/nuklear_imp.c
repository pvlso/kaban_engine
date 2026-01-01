/* ========================================================================
   $File: $
   $Date: 2025 $
   $Revision: $
   $Creator: pvlso $
   $Notice:  $
   ======================================================================== */

#if defined(NUKLEAR_DLL_BUILD)
#  define NK_API __declspec(dllexport)
#elif defined(NUKLEAR_DLL_USE)
#  define NK_API __declspec(dllimport)
#else
#  define NK_API
#endif

#define NK_IMPLEMENTATION
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT

#include "nuklear.h"

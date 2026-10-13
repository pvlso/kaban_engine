#if !defined(ENGINE_NUKLEAR_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

// NOTE(pvlso): Nuklear lives in its own shared library (nuklear_imp.c), the engine, the platform
// layer and the games all link against it. The flags change the layout of nuklear's structs,
// so they have to match nuklear_imp.c everywhere.
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_STANDARD_IO
#include "nuklear/nuklear.h"

#define ENGINE_NUKLEAR_H
#endif

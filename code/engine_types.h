#if !defined(ENGINE_TYPES_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */
    
#if !defined(COMPILER_MSVC)
#define COMPILER_MSVC 0
#endif
    
#if !defined(COMPILER_LLVM)
#define COMPILER_LLVM 0
#endif

#if !COMPILER_MSVC && !COMPILER_LLVM
#if _MSC_VER
#undef COMPILER_MSVC
#define COMPILER_MSVC 1
#else
#undef COMPILER_LLVM
#define COMPILER_LLVM 1
#endif
#endif

#if COMPILER_MSVC
#include <intrin.h>
#elif COMPILER_LLVM
#include <x86intrin.h>
#else
#error SEE/NEON optimizations are not available for this compiler yet!!!!
#endif

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Types
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>
    
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef intptr_t intptr;
typedef uintptr_t uintptr;
    
typedef float real32;
typedef double real64;
    
typedef int8 s8;
typedef int8 s08;
typedef int16 s16;
typedef int32 s32;
typedef int64 s64;
typedef bool32 b32;

typedef uint8 u8;
typedef uint8 u08;
typedef uint16 u16;
typedef uint32 u32;
typedef uint64 u64;

typedef real32 r32;
typedef real64 r64;
typedef real32 f32;
typedef real64 f64;

typedef uintptr_t umm;
typedef intptr_t  smm;

typedef int32 fp22_10;
    
struct memory_arena
{
    umm Size;
    u8 *Base;
    umm Used;

    umm MinimumBlockSize;
    
    u32 BlockCount;
    s32 TempCount;
};

// NOTE(pvlso): V2 ============================================================================================================================================
union v2
{
    struct
    {
        f32 x, y;
    };
    struct
    {
        f32 u, v;
    };

    f32 E[2];
};
    
union v2d
{
    __m128d V;

    struct
    {
        f64 x, y;
    };

    struct
    {
        f32 u, v;
    };

    f64 E[2];
};

union v2i
{
    struct
    {
        s32 x, y;
    };

    s32 E[2];
};

union v2di
{
    __m128i V;

    struct
    {
        s64 x, y;
    };

    s64 E[2];
};

// ===========================================================================================================================================================

// NOTE(pvlso): V3 ============================================================================================================================================
union v3
{
    struct
    {
        f32 x, y, z;
    };
    struct
    {
        f32 u, v, w;
    };
    struct
    {
        f32 r, g, b;
    };
    struct
    {
        v2 xy;
        f32 Ignored0_;
    };
    struct
    {
        f32 Ignored1_;
        v2 yz;
    };
    struct
    {
        v2 uv;
        f32 Ignored2_;
    };
    struct
    {
        f32 Ignored3_;
        v2 vw;
    };

    f32 E[3];
};

union v3d
{
    struct
    {
        f64 x, y, z;
    };
    struct
    {
        f64 u, v, w;
    };
    struct
    {
        f64 r, g, b;
    };
    struct
    {
        v2d xy;
        f64 Ignored0_;
    };
    struct
    {
        f64 Ignored1_;
        v2d yz;
    };
    struct
    {
        v2d uv;
        f64 Ignored2_;
    };
    struct
    {
        f64 Ignored3_;
        v2d vw;
    };

    f64 E[3];
};

union v3i
{
    struct
    {
        s32 x, y, z;
    };
    struct
    {
        s32 u, v, w;
    };
    struct
    {
        s32 r, g, b;
    };
    struct
    {
        v2i xy;
        s32 Ignored0_;
    };
    struct
    {
        s32 Ignored1_;
        v2i yz;
    };
    struct
    {
        v2i uv;
        s32 Ignored2_;
    };
    struct
    {
        s32 Ignored3_;
        v2i vw;
    };

    s32 E[3];
};

union v3di
{
    struct
    {
        s64 x, y, z;
    };
    struct
    {
        s64 u, v, w;
    };
    struct
    {
        s64 r, g, b;
    };
    struct
    {
        v2di xy;
        s64 Ignored0_;
    };
    struct
    {
        s64 Ignored1_;
        v2di yz;
    };
    struct
    {
        v2di uv;
        s64 Ignored2_;
    };
    struct
    {
        s64 Ignored3_;
        v2di vw;
    };

    s64 E[3];
};

// ===========================================================================================================================================================

// NOTE(pvlso): V4 ============================================================================================================================================
union v4
{
    struct
    {
        union
        {
            v3 xyz;
            struct
            {
                f32 x, y, z;
            };
        };
        
        f32 w;        
    };

    struct
    {
        union
        {
            v3 rgb;
            struct
            {
                f32 r, g, b;
            };
        };
        
        f32 a;        
    };
    struct
    {
        v2 xy;
        f32 Ignored0_;
        f32 Ignored1_;
    };
    struct
    {
        f32 Ignored2_;
        v2 yz;
        f32 Ignored3_;
    };
    struct
    {
        f32 Ignored4_;
        f32 Ignored5_;
        v2 zw;
    };

    __m128 V;
    f32 E[4];
};

union v4d
{
    __m256d V;
    struct
    {
        union
        {
            v3d xyz;
            struct
            {
                f64 x, y, z;
            };
        };
        
        f64 w;        
    };

    struct
    {
        union
        {
            v3d rgb;
            struct
            {
                f64 r, g, b;
            };
        };
        
        f64 a;        
    };

    struct
    {
        v2d xy;
        f64 Ignored0_;
        f64 Ignored1_;
    };

    struct
    {
        f64 Ignored2_;
        v2d yz;
        f64 Ignored3_;
    };

    struct
    {
        f64 Ignored4_;
        f64 Ignored5_;
        v2d zw;
    };

    f64 E[4];
};

union v4i
{
    struct
    {
        union
        {
            v3i xyz;
            struct
            {
                s32 x, y, z;
            };
        };
        
        s32 w;        
    };

    struct
    {
        union
        {
            v3i rgb;
            struct
            {
                s32 r, g, b;
            };
        };
        
        s32 a;        
    };
    struct
    {
        v2i xy;
        s32 Ignored0_;
        s32 Ignored1_;
    };
    struct
    {
        s32 Ignored2_;
        v2i yz;
        s32 Ignored3_;
    };
    struct
    {
        s32 Ignored4_;
        s32 Ignored5_;
        v2i zw;
    };

    __m128i V;
    s32 E[4];
};

union v4di
{
    __m256i V;
    struct
    {
        union
        {
            v3di xyz;
            struct
            {
                s64 x, y, z;
            };
        };
        
        s64 w;        
    };

    struct
    {
        union
        {
            v3di rgb;
            struct
            {
                s64 r, g, b;
            };
        };
        
        s64 a;        
    };

    struct
    {
        v2di xy;
        s64 Ignored0_;
        s64 Ignored1_;
    };

    struct
    {
        s64 Ignored2_;
        v2di yz;
        s64 Ignored3_;
    };

    struct
    {
        s64 Ignored4_;
        s64 Ignored5_;
        v2di zw;
    };

    s64 E[4];
};
// ===========================================================================================================================================================

// NOTE(pvlso): RECT2 =========================================================================================================================================

struct rectangle2
{
    v2 Min;
    v2 Max;
};

struct rectangle2i
{
    v2i Min;
    v2i Max;
};

struct rectangle2d
{
    v2d Min;
    v2d Max;
};

struct rectangle2di
{
    v2di Min;
    v2di Max;
};

// ===========================================================================================================================================================

// NOTE(pvlso): RECT3 =========================================================================================================================================

struct rectangle3
{
    v3 Min;
    v3 Max;
};

struct rectangle3i
{
    v3i Min;
    v3i Max;
};

struct rectangle3d
{
    v3d Min;
    v3d Max;
};

struct rectangle3di
{
    v3di Min;
    v3di Max;
};

// ===========================================================================================================================================================

// NOTE(pvlso): TRIANGLE ======================================================================================================================================
// ===========================================================================================================================================================
struct triangle
{
    v2 Vertices[3];
    rectangle2 Bounds;
};

struct triangled
{
    v2d Vertices[3];
    rectangle2d Bounds;
};

// NOTE(pvlso): POLYGON =======================================================================================================================================

#define COMMON_EPSILON_F32 1.19e-07
#define COMMON_EPSILON_F64 0.0000000000001f

#define MAX_HOLE_COUNT 128

struct polygon2
{
    s32 VertexCount;
    v2 *Vertices;

    b32 HasHoles;
    s32 HoleCount;
    s32 *HoleVertexCounts;
    v2 *HolesVertices;
};

struct polygon2d
{
    s32 VertexCount;
    v2d *Vertices;

    b32 HasHoles;
    s32 HoleCount;
    s32 *HoleVertexCounts;
    v2d *HolesVertices;
};

#define MAX_POLYGON_COUNT 32
struct polygon2_set
{
    s32 PolygonCount;
    polygon2 *Polygons;
};

struct polygon2d_set
{
    s32 PolygonCount;
    polygon2d *Polygons;
};
    
// ===========================================================================================================================================================

// NOTE(pvlso): LINE ==========================================================================================================================================
struct lined
{
    v2d a;
    v2d b;
};

struct line
{
    v2 a;
    v2 b;
};
// ===========================================================================================================================================================

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

struct bitmap_id
{
    u64 Value;
};

struct sound_id
{
    u64 Value;
};

struct tileset_id
{
    u64 Value;
};

struct spritesheet_id
{
    u64 Value;
};

struct text_id
{
    u64 Value;
};

struct file_id
{
    u64 Value;
};

#define ENGINE_TYPES_H
#endif

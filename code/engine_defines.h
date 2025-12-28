#if !defined(ENGINE_DEFINES_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): COMMON DEFINES
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define U32FromPointer(Pointer) ((u32)(umm)(Pointer))
#define PointerFromU32(type, Value) (type *)((umm)Value)

#define OffsetOf(type, Member) (umm)&(((type *)0)->Member)
    
#define U16Maximum 0xFFFF
#define U32Maximum 0xFFFFFFFF
#define U64Maximum 0xFFFFFFFFFFFFFFFF

#define Real32Maximum FLT_MAX
#define Real32Minimum -FLT_MAX

#define Real64Maximum DBL_MAX
#define Real64Minimum -DBL_MAX

#if !defined(internal)
#define internal static
#endif
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f
#define Tau32 6.28318530717958647692f
    
#if EDITOR_SLOW
// TODO(casey): Complete assertion macro - don't worry everyone!
#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}
#else
#define Assert(Expression)
#endif

#define InvalidCodePath Assert(!"InvalidCodePath")
#define InvalidDefaultCase default: {InvalidCodePath;} break

#define Kilobytes(Value) ((Value)*1024LL)
#define Megabytes(Value) (Kilobytes(Value)*1024LL)
#define Gigabytes(Value) (Megabytes(Value)*1024LL)
#define Terabytes(Value) (Gigabytes(Value)*1024LL)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))
// TODO(casey): swap, min, max ... macros???

#define AlignPow2(Value, Alignment) ((Value + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#define ENGINE_DEFINES_H
#endif

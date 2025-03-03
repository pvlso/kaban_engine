#if !defined(ENGINE_INTRINSICS_H)
/* ========================================================================
   $File: $
   $Date: 2025 $
   $Revision: $
   $Creator: Pavlo Solodrai $
   $Notice: $
   ======================================================================== */

//
// TODO(casey): Convert all of these to platform-efficient versions
// and remove math.h
//

#include "math.h"

inline s32
SignOf(s32 Value)
{
    s32 Result = (Value >= 0) ? 1 : -1;
    return(Result);
}

inline f32
SignOf(f32 Value)
{
    f32 Result = (Value >= 0) ? 1.0f : -1.0f;
    return(Result);
}

inline f32
SquareRoot(f32 F32)
{
    f32 Result = sqrtf(F32);
    return(Result);
}

inline f64
SquareRoot(f64 Float64)
{
    f64 Result = sqrt(Float64);
    return(Result);
}

inline f32
AbsoluteValue(f32 F32)
{
    f32 Result = (f32)fabs(F32);
    return(Result);
}

inline f64
AbsoluteValue(f64 Float64)
{
    f64 Result = fabs(Float64);
    return(Result);
}

inline s32
AbsoluteValue(s32 S32)
{
    s32 Result = abs(S32);
    return(Result);
}

inline u32
RotateLeft(u32 Value, s32 Amount)
{
#if COMPILER_MSVC
    u32 Result = _rotl(Value, Amount);
#else
    // TODO(casey): Actually port this to other compiler platforms!
    Amount &= 31;
    u32 Result = ((Value << Amount) | (Value >> (32 - Amount)));
#endif

    return(Result);
}

inline u32
RotateRight(u32 Value, s32 Amount)
{
#if COMPILER_MSVC
    u32 Result = _rotr(Value, Amount);
#else
    // TODO(casey): Actually port this to other compiler platforms!
    Amount &= 31;
    us32 Result = ((Value >> Amount) | (Value << (32 - Amount)));
#endif

    return(Result);
}

inline s32
RoundF32ToS32(f32 F32)
{
    s32 Result = (s32)roundf(F32);
    return(Result);
}

inline u32
RoundF32ToUS32(f32 F32)
{
    u32 Result = (u32)roundf(F32);
    return(Result);
}

inline s32 
FloorF32ToS32(f32 F32)
{
    s32 Result = (s32)floorf(F32);
    return(Result);
}

inline s32 
CeilF32ToS32(f32 F32)
{
    s32 Result = (s32)ceilf(F32);
    return(Result);
}

inline s32
TruncateF32ToS32(f32 F32)
{
    s32 Result = (s32)F32;
    return(Result);
}

inline f32
Sin(f32 Angle)
{
    f32 Result = sinf(Angle);
    return(Result);
}

inline f32
Cos(f32 Angle)
{
    f32 Result = cosf(Angle);
    return(Result);
}

inline f32
ATan2(f32 Y, f32 X)
{
    f32 Result = atan2f(Y, X);
    return(Result);
}

inline f64
AToF(char *A)
{
    f64 Result = atof(A);

    return(Result);
}

struct bit_scan_result
{
    b32 Found;
    u32 Index;
};

inline bit_scan_result
FindLeastSignificantSetBit(u32 Value)
{
    bit_scan_result Result = {};

#if COMPILER_MSVC
    Result.Found = _BitScanForward((unsigned long *)&Result.Index, Value);
#else
    for(u32 Test = 0;
        Test < 32;
        ++Test)
    {
        if(Value & (1 << Test))
        {
            Result.Index = Test;
            Result.Found = true;
            break;
        }
    }
#endif
    
    return(Result);
}

#define ENGINE_INTRINSICS_H
#endif

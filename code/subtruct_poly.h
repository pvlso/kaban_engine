#if !defined(SUBTRUCT_POLY_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct dv2
{
    f64 x;
    f64 y;
};

inline dv2
dV2(v2 V)
{
    dv2 Result = {V.x, V.y};
    return(Result);
}

inline f64
Square(f64 A)
{
    f64 Result = A*A;

    return(Result);
}

inline f64
Lerp(f64 A, f64 t, f64 B)
{
    f64 Result = (1.0f - t)*A + t*B;

    return(Result);
}

inline f64
Clamp(f64 Min, f64 Value, f64 Max)
{
    f64 Result = Value;

    if(Result < Min)
    {
        Result = Min;
    }
    else if(Result > Max)
    {
        Result = Max;
    }

    return(Result);
}

inline f64
Clamp01(f64 Value)
{
    f64 Result = Clamp(0.0, Value, 1.0);

    return(Result);
}

inline f64
Clamp01MapToRange(f64 Min, f64 t, f64 Max)
{
    f64 Result = 0.0f;
    
    f64 Range = Max - Min;
    if(Range != 0.0f)
    {
        Result = Clamp01((t - Min) / Range);
    }

    return(Result);
}

inline f64
SafeRatioN(f64 Numerator, f64 Divisor, f64 N)
{
    f64 Result = N;

    if(Divisor != 0.0)
    {
        Result = Numerator / Divisor;
    }

    return(Result);
}

inline f64
SafeRatio0(f64 Numerator, f64 Divisor)
{
    f64 Result = SafeRatioN(Numerator, Divisor, 0.0);

    return(Result);
}

inline f64
SafeRatio1(f64 Numerator, f64 Divisor)
{
    f64 Result = SafeRatioN(Numerator, Divisor, 1.0);

    return(Result);
}

inline dv2
Perp(dv2 A)
{
    dv2 Result = {-A.y, A.x};
    return(Result);
}

inline dv2
operator*(f64 A, dv2 B)
{
    dv2 Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    
    return(Result);
}

inline dv2
operator*(dv2 B, f64 A)
{
    dv2 Result = A*B;

    return(Result);
}

inline dv2 &
operator*=(dv2 &B, f64 A)
{
    B = A * B;

    return(B);
}

inline dv2
operator-(dv2 A)
{
    dv2 Result;

    Result.x = -A.x;
    Result.y = -A.y;

    return(Result);
}

inline dv2
operator+(dv2 A, dv2 B)
{
    dv2 Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;

    return(Result);
}

inline dv2 &
operator+=(dv2 &A, dv2 B)
{
    A = A + B;

    return(A);
}

inline dv2
operator-(dv2 A, dv2 B)
{
    dv2 Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;

    return(Result);
}

inline dv2 &
operator-=(dv2 &A, dv2 B)
{
    A = A - B;

    return(A);
}

inline dv2
Lerp(dv2 A, f64 t, dv2 B)
{
    dv2 Result = (1.0f - t)*A + t*B;

    return(Result);
}

inline dv2
Hadamard(dv2 A, dv2 B)
{
    dv2 Result = {A.x*B.x, A.y*B.y};

    return(Result);
}

inline f64
Inner(dv2 A, dv2 B)
{
    f64 Result = A.x*B.x + A.y*B.y;

    return(Result);
}

inline f64
Cross(dv2 A, dv2 B)
{
    f64 Result = A.x*B.y - A.y*B.x;

    return(Result);
}

inline f64
LengthSq(dv2 A)
{
    f64 Result = Inner(A, A);

    return(Result);
}

inline f64
Length(dv2 A)
{
    f64 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline dv2
Clamp01(dv2 Value)
{
    dv2 Result;

    Result.x = Clamp01(Value.x);
    Result.y = Clamp01(Value.y);

    return(Result);
}

inline dv2
Normalize(dv2 A)
{
    dv2 Result = {};

    f64 L = Length(A);
    if(L)
    {
        Result = A * (1.0f / L);
    }

    return(Result);
}

struct dpolygon2
{
    s32 VertexCount;
    dv2 *Vertices;

    b32 HasHole;
    s32 HoleVertexCount;
    dv2 *HoleVertices;
};

#define SUBTRUCT_POLY_H
#endif

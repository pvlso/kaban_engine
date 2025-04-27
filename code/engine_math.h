#if !defined(ENGINE_MATH_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR CONVERT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// NOTE(paul): V2 ============================================================================================================================================
inline v2
V2(f32 X, f32 Y)
{
    v2 Result;

    Result.x = X;
    Result.y = Y;

    return(Result);
}

inline v2
V2(s32 A, s32 B)
{
    v2 Result = {(f32)A, (f32)B};

    return(Result);
}

inline v2i
V2i(v2 A)
{
    v2i Result = {(s32)A.x, (s32)A.y};

    return(Result);
}

inline v2i
V2i(f32 X, f32 Y)
{
    v2i Result = {(s32)X, (s32)Y};

    return(Result);
}
    
inline v2
V2(v2i A)
{
    v2 Result = {(f32)A.x, (f32)A.y};

    return(Result);
}

inline v2
V2i(u32 X, u32 Y)
{
    v2 Result = {(f32)X, (f32)Y};

    return(Result);
}

inline v2d
V2d(f32 X, f32 Y)
{
    v2d Result;

    Result.x = X;
    Result.y = Y;

    return(Result);
}

inline v2d
V2d(f64 X, f64 Y)
{
    v2d Result = {X, Y};
    return(Result);
}

inline v2d
V2d(v2 A)
{
    v2d Result = {A.x, A.y};
    return(Result);
}

inline v2d
V2d(__m128d A)
{
    v2d Result = {A};
    return(Result);
}

inline v2
V2(v2d A)
{
    v2 Result;

    Result.x = (f32)round(A.x*10000.f) * 0.0001f;
    Result.y = (f32)round(A.y*10000.f) * 0.0001f;

    return(Result);
}

inline v2
V2(f64 X, f64 Y)
{
    v2 Result;

    Result.x = (f32)round(X*10000.f) * 0.0001f;
    Result.y = (f32)round(Y*10000.f) * 0.0001f;

    return(Result);
}

// ===========================================================================================================================================================

// NOTE(paul): V3 ============================================================================================================================================
inline v3
V3(f32 X, f32 Y, f32 Z)
{
    v3 Result;

    Result.x = X;
    Result.y = Y;
    Result.z = Z;

    return(Result);
}

inline v3
V3(v2 XY, real32 Z)
{
    v3 Result;

    Result.x = XY.x;
    Result.y = XY.y;
    Result.z = Z;

    return(Result);
}

inline v3
V3i(s32 X, s32 Y, s32 Z)
{
    v3 Result;

    Result.x = (f32)X;
    Result.y = (f32)Y;
    Result.z = (f32)Z;

    return(Result);
}

inline v3
V3i(u32 X, u32 Y, u32 Z)
{
    v3 Result;

    Result.x = (f32)X;
    Result.y = (f32)Y;
    Result.z = (f32)Z;

    return(Result);
}
// ===========================================================================================================================================================

// NOTE(paul): V4 ============================================================================================================================================
inline v4
V4(f32 X, f32 Y, f32 Z, f32 W)
{
    v4 Result;

    Result.x = X;
    Result.y = Y;
    Result.z = Z;
    Result.w = W;

    return(Result);
}

inline v4
V4(v3 XYZ, f32 W)
{
    v4 Result;

    Result.xyz = XYZ;
    Result.w = W;

    return(Result);
}

inline v4
V4i(s32 X, s32 Y, s32 Z, s32 W)
{
    v4 Result {(f32)X, (f32)Y, (f32)Z, (f32)W};

    return(Result);
}

inline v4
V4i(u32 X, u32 Y, u32 Z, u32 W)
{
    v4 Result {(f32)X, (f32)Y, (f32)Z, (f32)W};

    return(Result);
}
// ===========================================================================================================================================================

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): SCALAR OPERATIONS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// NOTE(paul): FLOAT =========================================================================================================================================
inline f32
Square(f32 A)
{
    f32 Result = A*A;

    return(Result);
}

inline f32
Lerp(f32 A, f32 t, f32 B)
{
    f32 Result = (1.0f - t)*A + t*B;

    return(Result);
}

inline f32
Clamp(f32 Min, f32 Value, f32 Max)
{
    f32 Result = Value;

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

inline f32
Clamp01(f32 Value)
{
    f32 Result = Clamp(0.0f, Value, 1.0f);

    return(Result);
}

inline f32
Clamp01MapToRange(f32 Min, f32 t, f32 Max)
{
    f32 Result = 0.0f;
    
    f32 Range = Max - Min;
    if(Range != 0.0f)
    {
        Result = Clamp01((t - Min) / Range);
    }

    return(Result);
}

inline f32
SafeRatioN(f32 Numerator, f32 Divisor, f32 N)
{
    f32 Result = N;

    if(Divisor != 0.0f)
    {
        Result = Numerator / Divisor;
    }

    return(Result);
}

inline f32
SafeRatio0(f32 Numerator, f32 Divisor)
{
    f32 Result = SafeRatioN(Numerator, Divisor, 0.0f);

    return(Result);
}

inline f32
SafeRatio1(f32 Numerator, f32 Divisor)
{
    f32 Result = SafeRatioN(Numerator, Divisor, 1.0f);

    return(Result);
}
// ===========================================================================================================================================================

// NOTE(paul): DOUBLE ========================================================================================================================================
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

    if(Divisor != 0.0f)
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
// ===========================================================================================================================================================

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 2 FLOAT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline b32
PointsAreEqual(v2 a, v2 b, f32 Epsilon)
{
    b32 Result = (AbsoluteValue(a.x - b.x) <= Epsilon) && (AbsoluteValue(a.y - b.y) <= Epsilon);
    return(Result);
}

inline v2
Perp(v2 A)
{
    v2 Result = {-A.y, A.x};
    return(Result);
}

inline v2
operator*(f32 A, v2 B)
{
    v2 Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    
    return(Result);
}

inline v2
operator*(v2 B, f32 A)
{
    v2 Result = A*B;

    return(Result);
}

inline v2 &
operator*=(v2 &B, f32 A)
{
    B = A * B;

    return(B);
}

inline v2
operator-(v2 A)
{
    v2 Result;

    Result.x = -A.x;
    Result.y = -A.y;

    return(Result);
}

inline v2
operator+(v2 A, v2 B)
{
    v2 Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;

    return(Result);
}

inline v2 &
operator+=(v2 &A, v2 B)
{
    A = A + B;

    return(A);
}

inline v2
operator-(v2 A, v2 B)
{
    v2 Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;

    return(Result);
}

inline v2 &
operator-=(v2 &A, v2 B)
{
    A = A - B;

    return(A);
}

inline v2
Lerp(v2 A, f32 t, v2 B)
{
    v2 Result = (1.0f - t)*A + t*B;

    return(Result);
}

inline v2
Hadamard(v2 A, v2 B)
{
    v2 Result = {A.x*B.x, A.y*B.y};

    return(Result);
}

inline real32
Inner(v2 A, v2 B)
{
    f32 Result = A.x*B.x + A.y*B.y;

    return(Result);
}

inline r32
Cross(v2 A, v2 B)
{
    f32 Result = A.x*B.y - A.y*B.x;

    return(Result);
}

inline real32
LengthSq(v2 A)
{
    f32 Result = Inner(A, A);

    return(Result);
}

inline real32
Length(v2 A)
{
    f32 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline v2
Clamp01(v2 Value)
{
    v2 Result;

    Result.x = Clamp01(Value.x);
    Result.y = Clamp01(Value.y);

    return(Result);
}

inline v2
Arm2(r32 Angle)
{
    v2 Result = {Cos(Angle), Sin(Angle)};

    return(Result);
}

inline v2
Normalize(v2 A)
{
    v2 Result = {};

    r32 L = Length(A);
    if(L)
    {
        Result = A * (1.0f / L);
    }

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 2 DOUBLE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline b32
PointsAreEqual(v2d a, v2d b, f64 Epsilon)
{
    b32 Result = (AbsoluteValue(a.x - b.x) <= Epsilon) && (AbsoluteValue(a.y - b.y) <= Epsilon);
    return(Result);
}

inline v2d
Perp(v2d A)
{
    v2d Result = {-A.y, A.x};
    return(Result);
}

inline v2d
operator*(v2d A, v2d B)
{
    v2d Result;

    Result = {_mm_mul_pd(A.V, B.V)};
    
    return(Result);
}

inline v2d &
operator*=(v2d &B, v2d A)
{
    B = A * B;

    return(B);
}


inline v2d
operator*(f64 A, v2d B)
{
    v2d Result;

    __m128d C = _mm_set1_pd(A);
    Result = {_mm_mul_pd(C, B.V)};
    
    return(Result);
}

inline v2d
operator*(v2d B, f64 A)
{
    v2d Result = A*B;

    return(Result);
}

inline v2d &
operator*=(v2d &B, f64 A)
{
    B = A * B;

    return(B);
}

inline v2d
operator-(v2d A)
{
    v2d Result;

    Result.x = -A.x;
    Result.y = -A.y;

    return(Result);
}


inline v2d
operator+(v2d A, v2d B)
{
    v2d Result;

    Result = {_mm_add_pd(A.V, B.V)};

    return(Result);
}

inline v2d &
operator+=(v2d &A, v2d B)
{
    A = A + B;

    return(A);
}


inline v2d
operator-(v2d A, v2d B)
{
    v2d Result;

    Result = {_mm_sub_pd(A.V, B.V)};

    return(Result);
}

inline v2d &
operator-=(v2d &A, v2d B)
{
    A = A - B;

    return(A);
}

inline v2d
Lerp(v2d A, f64 t, v2d B)
{
    v2d Result = (1.0f - t)*A + t*B;

    return(Result);
}

inline v2d
Hadamard(v2d A, v2d B)
{
    v2d Result = {A.x*B.x, A.y*B.y};

    return(Result);
}

inline f64
Inner(v2d A, v2d B)
{
    v2d Mul = A*B;
    f64 Result = Mul.x + Mul.y;

    return(Result);
}

inline f64
Cross(v2d A, v2d B)
{
    f64 Result = A.x*B.y - A.y*B.x;

    return(Result);
}

inline f64
LengthSq(v2d A)
{
    f64 Result = Inner(A, A);

    return(Result);
}

inline f64
Length(v2d A)
{
    f64 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline v2d
Clamp01(v2d Value)
{
    v2d Result;

    Result.x = Clamp01(Value.x);
    Result.y = Clamp01(Value.y);

    return(Result);
}

inline v2d
Arm2(f64 Angle)
{
    v2d Result = {Cos(Angle), Sin(Angle)};

    return(Result);
}

inline v2d
Normalize(v2d A)
{
    v2d Result = {};

    f64 L = Length(A);
    if(L)
    {
        Result = A * (1.0f / L);
    }

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 2 INTEGER
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v2i
Perp(v2i A)
{
    v2i Result = {-A.y, A.x};
    return(Result);
}

inline v2i
operator-(v2i A)
{
    v2i Result;

    Result.x = -A.x;
    Result.y = -A.y;

    return(Result);
}

inline v2i
operator+(v2i A, v2i B)
{
    v2i Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;

    return(Result);
}

inline v2i &
operator+=(v2i &A, v2i B)
{
    A = A + B;

    return(A);
}

inline v2i
operator-(v2i A, v2i B)
{
    v2i Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;

    return(Result);
}

inline v2i &
operator-=(v2i &A, v2i B)
{
    A = A - B;

    return(A);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 2 LONG INTEGER
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v2di
Perp(v2di A)
{
    v2di Result = {};
    Result.x = -A.y;
    Result.y = A.x;
    return(Result);
}

inline v2di
operator-(v2di A)
{
    v2di Result;

    Result.x = -A.x;
    Result.y = -A.y;

    return(Result);
}

inline v2di
operator+(v2di A, v2di B)
{
    v2di Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;

    return(Result);
}

inline v2di &
operator+=(v2di &A, v2di B)
{
    A = A + B;

    return(A);
}

inline v2di
operator-(v2di A, v2di B)
{
    v2di Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;

    return(Result);
}

inline v2di &
operator-=(v2di &A, v2di B)
{
    A = A - B;

    return(A);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 3 FLOAT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v3
operator*(f32 A, v3 B)
{
    v3 Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    Result.z = A*B.z;
    
    return(Result);
}

inline v3
operator*(v3 B, f32 A)
{
    v3 Result = A*B;

    return(Result);
}

inline v3 &
operator*=(v3 &B, f32 A)
{
    B = A * B;

    return(B);
}

inline v3
operator-(v3 A)
{
    v3 Result;

    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;

    return(Result);
}

inline v3
operator+(v3 A, v3 B)
{
    v3 Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;

    return(Result);
}

inline v3 &
operator+=(v3 &A, v3 B)
{
    A = A + B;

    return(A);
}

inline v3
operator-(v3 A, v3 B)
{
    v3 Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;

    return(Result);
}

inline v3 &
operator-=(v3 &A, v3 B)
{
    A = A - B;

    return(A);
}

inline v3
Hadamard(v3 A, v3 B)
{
    v3 Result = {A.x*B.x, A.y*B.y, A.z*B.z};

    return(Result);
}

inline f32
Inner(v3 A, v3 B)
{
    f32 Result = A.x*B.x + A.y*B.y + A.z*B.z;

    return(Result);
}

inline f32
LengthSq(v3 A)
{
    f32 Result = Inner(A, A);

    return(Result);
}

inline f32
Length(v3 A)
{
    f32 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline v3
Normalize(v3 A)
{
    v3 Result = {};
    r32 L = Length(A);
    if(L)
    {
        Result = A * (1.0f / L);
    }

    return(Result);
}

inline v3
Clamp01(v3 Value)
{
    v3 Result;

    Result.x = Clamp01(Value.x);
    Result.y = Clamp01(Value.y);
    Result.z = Clamp01(Value.z);

    return(Result);
}

inline v3
Lerp(v3 A, f32 t, v3 B)
{
    v3 Result = (1.0f - t)*A + t*B;

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 3 DOUBLE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v3d
operator*(f64 A, v3d B)
{
    v3d Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    Result.z = A*B.z;
    
    return(Result);
}

inline v3d
operator*(v3d B, f64 A)
{
    v3d Result = A*B;

    return(Result);
}

inline v3d &
operator*=(v3d &B, f64 A)
{
    B = A * B;

    return(B);
}

inline v3d
operator-(v3d A)
{
    v3d Result;

    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;

    return(Result);
}

inline v3d
operator+(v3d A, v3d B)
{
    v3d Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;

    return(Result);
}

inline v3d &
operator+=(v3d &A, v3d B)
{
    A = A + B;

    return(A);
}

inline v3d
operator-(v3d A, v3d B)
{
    v3d Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;

    return(Result);
}

inline v3d &
operator-=(v3d &A, v3d B)
{
    A = A - B;

    return(A);
}

inline v3d
Hadamard(v3d A, v3d B)
{
    v3d Result = {A.x*B.x, A.y*B.y, A.z*B.z};

    return(Result);
}

inline f64
Inner(v3d A, v3d B)
{
    f64 Result = A.x*B.x + A.y*B.y + A.z*B.z;

    return(Result);
}

inline f64
LengthSq(v3d A)
{
    f64 Result = Inner(A, A);

    return(Result);
}

inline f64
Length(v3d A)
{
    f64 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline v3d
Normalize(v3d A)
{
    v3d Result = {};
    f64 L = Length(A);
    if(L)
    {
        Result = A * (1.0 / L);
    }

    return(Result);
}

inline v3d
Clamp01(v3d Value)
{
    v3d Result;

    Result.x = Clamp01(Value.x);
    Result.y = Clamp01(Value.y);
    Result.z = Clamp01(Value.z);

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 3 INTEGER
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v3i
operator-(v3i A)
{
    v3i Result;

    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;

    return(Result);
}

inline v3i
operator+(v3i A, v3i B)
{
    v3i Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;

    return(Result);
}

inline v3i &
operator+=(v3i &A, v3i B)
{
    A = A + B;

    return(A);
}

inline v3i
operator-(v3i A, v3i B)
{
    v3i Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;

    return(Result);
}

inline v3i &
operator-=(v3i &A, v3i B)
{
    A = A - B;

    return(A);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 3 LONG INTEGER
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v3di
operator-(v3di A)
{
    v3di Result;

    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;

    return(Result);
}

inline v3di
operator+(v3di A, v3di B)
{
    v3di Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;

    return(Result);
}

inline v3di &
operator+=(v3di &A, v3di B)
{
    A = A + B;

    return(A);
}

inline v3di
operator-(v3di A, v3di B)
{
    v3di Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;

    return(Result);
}

inline v3di &
operator-=(v3di &A, v3di B)
{
    A = A - B;

    return(A);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 4 FLOAT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v4
operator*(f32 A, v4 B)
{
    v4 Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    Result.z = A*B.z;
    Result.w = A*B.w;
    
    return(Result);
}

inline v4
operator*(v4 B, f32 A)
{
    v4 Result = A*B;

    return(Result);
}

inline v4 &
operator*=(v4 &B, f32 A)
{
    B = A * B;

    return(B);
}

inline v4
operator-(v4 A)
{
    v4 Result;

    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;
    Result.w = -A.w;

    return(Result);
}

inline v4
operator+(v4 A, v4 B)
{
    v4 Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;
    Result.w = A.w + B.w;

    return(Result);
}

inline v4 &
operator+=(v4 &A, v4 B)
{
    A = A + B;

    return(A);
}

inline v4
operator-(v4 A, v4 B)
{
    v4 Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;
    Result.w = A.w - B.w;

    return(Result);
}

inline v4 &
operator-=(v4 &A, v4 B)
{
    A = A - B;

    return(A);
}

inline v4
Hadamard(v4 A, v4 B)
{
    v4 Result = {A.x*B.x, A.y*B.y, A.z*B.z, A.w*B.w};

    return(Result);
}

inline real32
Inner(v4 A, v4 B)
{
    real32 Result = A.x*B.x + A.y*B.y + A.z*B.z + A.w*B.w;

    return(Result);
}

inline real32
LengthSq(v4 A)
{
    real32 Result = Inner(A, A);

    return(Result);
}

inline real32
Length(v4 A)
{
    real32 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline v4
Clamp01(v4 Value)
{
    v4 Result;

    Result.x = Clamp01(Value.x);
    Result.y = Clamp01(Value.y);
    Result.z = Clamp01(Value.z);
    Result.w = Clamp01(Value.w);

    return(Result);
}

inline v4
Lerp(v4 A, real32 t, v4 B)
{
    v4 Result = (1.0f - t)*A + t*B;

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 4 DOUBLE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v4d
operator*(f64 A, v4d B)
{
    v4d Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    Result.z = A*B.z;
    Result.w = A*B.w;
    
    return(Result);
}

inline v4d
operator*(v4d B, f64 A)
{
    v4d Result = A*B;

    return(Result);
}

inline v4d &
operator*=(v4d &B, f64 A)
{
    B = A * B;

    return(B);
}

inline v4d
operator-(v4d A)
{
    v4d Result;

    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;
    Result.w = -A.w;

    return(Result);
}

inline v4d
operator+(v4d A, v4d B)
{
    v4d Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;
    Result.w = A.w + B.w;

    return(Result);
}

inline v4d &
operator+=(v4d &A, v4d B)
{
    A = A + B;

    return(A);
}

inline v4d
operator-(v4d A, v4d B)
{
    v4d Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;
    Result.w = A.w - B.w;

    return(Result);
}

inline v4d &
operator-=(v4d &A, v4d B)
{
    A = A - B;

    return(A);
}

inline v4d
Hadamard(v4d A, v4d B)
{
    v4d Result = {A.x*B.x, A.y*B.y, A.z*B.z, A.w*B.w};

    return(Result);
}

inline f64
Inner(v4d A, v4d B)
{
    f64 Result = A.x*B.x + A.y*B.y + A.z*B.z + A.w*B.w;

    return(Result);
}

inline f64
LengthSq(v4d A)
{
    f64 Result = Inner(A, A);

    return(Result);
}

inline f64
Length(v4d A)
{
    f64 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline v4d
Clamp01(v4d Value)
{
    v4d Result;

    Result.x = Clamp01(Value.x);
    Result.y = Clamp01(Value.y);
    Result.z = Clamp01(Value.z);
    Result.w = Clamp01(Value.w);

    return(Result);
}

inline v4d
Lerp(v4d A, real32 t, v4d B)
{
    v4d Result = (1.0f - t)*A + t*B;

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 4 INTEGER
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v4i
operator-(v4i A)
{
    v4i Result;

    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;
    Result.w = -A.w;

    return(Result);
}

inline v4i
operator+(v4i A, v4i B)
{
    v4i Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;
    Result.w = A.w + B.w;

    return(Result);
}

inline v4i &
operator+=(v4i &A, v4i B)
{
    A = A + B;

    return(A);
}

inline v4i
operator-(v4i A, v4i B)
{
    v4i Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;
    Result.w = A.w - B.w;

    return(Result);
}

inline v4i &
operator-=(v4i &A, v4i B)
{
    A = A - B;

    return(A);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): VECTOR 4 LONG INTEGER
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline v4di
operator-(v4di A)
{
    v4di Result;

    Result.x = -A.x;
    Result.y = -A.y;
    Result.z = -A.z;
    Result.w = -A.w;

    return(Result);
}

inline v4di
operator+(v4di A, v4di B)
{
    v4di Result;

    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
    Result.z = A.z + B.z;
    Result.w = A.w + B.w;

    return(Result);
}

inline v4di &
operator+=(v4di &A, v4di B)
{
    A = A + B;

    return(A);
}

inline v4di
operator-(v4di A, v4di B)
{
    v4di Result;

    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
    Result.z = A.z - B.z;
    Result.w = A.w - B.w;

    return(Result);
}

inline v4di &
operator-=(v4di &A, v4di B)
{
    A = A - B;

    return(A);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): RECT 2 FLOAT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline b32
HasArea(rectangle2 A)
{
    bool32 Result = ((A.Min.x < A.Max.x) && (A.Min.y < A.Max.y));

    return(Result);
}

inline rectangle2
InvertedInfinityRectangle2(void)
{
    rectangle2 Result;

    Result.Min.x = Result.Min.y = Real32Maximum;
    Result.Max.x = Result.Max.y = -Real32Maximum;

    return(Result);
}

inline rectangle2
Union(rectangle2 A, rectangle2 B)
{
    rectangle2 Result;
    
    Result.Min.x = (A.Min.x < B.Min.x) ? A.Min.x : B.Min.x;
    Result.Min.y = (A.Min.y < B.Min.y) ? A.Min.y : B.Min.y;
    Result.Max.x = (A.Max.x > B.Max.x) ? A.Max.x : B.Max.x;
    Result.Max.y = (A.Max.y > B.Max.y) ? A.Max.y : B.Max.y;

    return(Result);
}

inline v2
GetMinCorner(rectangle2 Rect)
{
    v2 Result = Rect.Min;
    return(Result);
}

inline v2
GetMaxCorner(rectangle2 Rect)
{
    v2 Result = Rect.Max;
    return(Result);
}

inline v2
GetDim(rectangle2 Rect)
{
    v2 Result = Rect.Max - Rect.Min;
    return(Result);
}

inline v2
GetCenter(rectangle2 Rect)
{
    v2 Result = 0.5f*(Rect.Min + Rect.Max);
    return(Result);
}

inline r32
GetArea(rectangle2 A)
{
    v2 Dim = GetDim(A);
    r32 Result = Dim.x*Dim.y;

    return(Result);
}

inline rectangle2
RectMinMax(v2 Min, v2 Max)
{
    rectangle2 Result;

    Result.Min = Min;
    Result.Max = Max;

    return(Result);
}

inline rectangle2
RectMinDim(v2 Min, v2 Dim)
{
    rectangle2 Result;

    Result.Min = Min;
    Result.Max = Min + Dim;

    return(Result);
}

inline rectangle2
RectCenterHalfDim(v2 Center, v2 HalfDim)
{
    rectangle2 Result;

    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;

    return(Result);
}

inline rectangle2
AddRadiusTo(rectangle2 A, v2 Radius)
{
    rectangle2 Result;
    Result.Min = A.Min - Radius;
    Result.Max = A.Max + Radius;

    return(Result);
}

inline rectangle2
Offset(rectangle2 A, v2 Offset)
{
    rectangle2 Result;

    Result.Min = A.Min + Offset;
    Result.Max = A.Max + Offset;

    return(Result);
}

inline rectangle2
RectCenterDim(v2 Center, v2 Dim)
{
    rectangle2 Result = RectCenterHalfDim(Center, 0.5f*Dim);

    return(Result);
}

inline bool32
IsInRectangle(rectangle2 Rectangle, v2 Test)
{
    bool32 Result = ((Test.x >= Rectangle.Min.x) &&
                     (Test.y >= Rectangle.Min.y) &&
                     (Test.x < Rectangle.Max.x) &&
                     (Test.y < Rectangle.Max.y));

    return(Result);
}

inline v2
GetBarycentric(rectangle2 A, v2 P)
{
    v2 Result;

    Result.x = SafeRatio0(P.x - A.Min.x, A.Max.x - A.Min.x);
    Result.y = SafeRatio0(P.y - A.Min.y, A.Max.y - A.Min.y);

    return(Result);
}

inline bool32
RectanglesIntersect(rectangle2 A, rectangle2 B)
{
    bool32 Result = !((B.Max.x <= A.Min.x) ||
                      (B.Min.x >= A.Max.x) ||
                      (B.Max.y <= A.Min.y) ||
                      (B.Min.y >= A.Max.y));
    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): RECT 2 DOUBLE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline rectangle2d
InvertedInfinityRectangle2d(void)
{
    rectangle2d Result;

    Result.Min.V = _mm_set1_pd(Real64Maximum);
    Result.Max.V = _mm_set1_pd(Real64Minimum);

    return(Result);
}

inline v2d
GetDim(rectangle2d Rect)
{
    v2d Result = Rect.Max - Rect.Min;
    return(Result);
}

inline b32
RectanglesIntersect(rectangle2d A, rectangle2d B)
{
    b32 Result = !((B.Max.x <= A.Min.x) ||
                   (B.Min.x >= A.Max.x) ||
                   (B.Max.y <= A.Min.y) ||
                   (B.Min.y >= A.Max.y));
    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): RECT 2 INTEGER
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline b32
HasArea(rectangle2i A)
{
    bool32 Result = ((A.Min.x < A.Max.x) && (A.Min.y < A.Max.y));

    return(Result);
}

inline rectangle2i
InvertedInfinityRectangle2i(void)
{
    rectangle2i Result;

    Result.Min.x = Result.Min.y = INT_MAX;
    Result.Max.x = Result.Max.y = -INT_MAX;

    return(Result);
}

inline rectangle2i
Union(rectangle2i A, rectangle2i B)
{
    rectangle2i Result;
    
    Result.Min.x = (A.Min.x < B.Min.x) ? A.Min.x : B.Min.x;
    Result.Min.y = (A.Min.y < B.Min.y) ? A.Min.y : B.Min.y;
    Result.Max.x = (A.Max.x > B.Max.x) ? A.Max.x : B.Max.x;
    Result.Max.y = (A.Max.y > B.Max.y) ? A.Max.y : B.Max.y;

    return(Result);
}

inline v2i
GetMinCorner(rectangle2i Rect)
{
    v2i Result = Rect.Min;
    return(Result);
}

inline v2i
GetMaxCorner(rectangle2i Rect)
{
    v2i Result = Rect.Max;
    return(Result);
}

inline v2i
GetDim(rectangle2i Rect)
{
    v2i Result = Rect.Max - Rect.Min;
    return(Result);
}

inline s32
GetWidth(rectangle2i A)
{
    s32 Result = A.Max.x - A.Min.x;
    return(Result);
}

inline s32
GetHeight(rectangle2i A)
{
    s32 Result = A.Max.y - A.Min.y;
    return(Result);
}

inline v2
GetCenter(rectangle2i Rect)
{
    v2 Result = 0.5f*V2(Rect.Min + Rect.Max);
    return(Result);
}

inline f32
GetArea(rectangle2i A)
{
    v2i Dim = GetDim(A);
    f32 Result = (f32)Dim.x*(f32)Dim.y;

    return(Result);
}

inline rectangle2i
RectMinMax(v2i Min, v2i Max)
{
    rectangle2i Result;

    Result.Min = Min;
    Result.Max = Max;

    return(Result);
}

inline rectangle2i
RectMinDim(v2i Min, v2i Dim)
{
    rectangle2i Result;

    Result.Min = Min;
    Result.Max = Min + Dim;

    return(Result);
}

inline rectangle2i
RectCenterHalfDim(v2i Center, v2i HalfDim)
{
    rectangle2i Result;

    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;

    return(Result);
}

inline rectangle2i
AddRadiusTo(rectangle2i A, v2i Radius)
{
    rectangle2i Result;
    Result.Min = A.Min - Radius;
    Result.Max = A.Max + Radius;

    return(Result);
}

inline rectangle2i
Offset(rectangle2i A, v2i Offset)
{
    rectangle2i Result;

    Result.Min = A.Min + Offset;
    Result.Max = A.Max + Offset;

    return(Result);
}

inline rectangle2i
Offset(rectangle2i A, s32 X, s32 Y)
{
    rectangle2i Result = A;

    Result.Min.x += X;
    Result.Max.x += X;
    Result.Min.y += Y;
    Result.Max.y += Y;

    return(Result);
}

inline rectangle2i
RectCenterDim(v2i Center, v2i Dim)
{
    rectangle2i Result =
        RectCenterHalfDim(Center, V2i(0.5f*Dim.x, 0.5f*Dim.y));

    return(Result);
}

inline bool32
IsInRectangle(rectangle2i Rectangle, v2i Test)
{
    bool32 Result = ((Test.x >= Rectangle.Min.x) &&
                     (Test.y >= Rectangle.Min.y) &&
                     (Test.x < Rectangle.Max.x) &&
                     (Test.y < Rectangle.Max.y));

    return(Result);
}

inline rectangle2i
Intersect(rectangle2i A, rectangle2i B)
{
    rectangle2i Result;
    
    Result.Min.x = (A.Min.x < B.Min.x) ? B.Min.x : A.Min.x;
    Result.Min.y = (A.Min.y < B.Min.y) ? B.Min.y : A.Min.y;
    Result.Max.x = (A.Max.x > B.Max.x) ? B.Max.x : A.Max.x;
    Result.Max.y = (A.Max.y > B.Max.y) ? B.Max.y : A.Max.y;    

    return(Result);
}

inline int32
GetClampedRectArea(rectangle2i A)
{
    int32 Width = (A.Max.x - A.Min.x);
    int32 Height = (A.Max.y - A.Min.y);
    int32 Result = 0;
    if((Width > 0) && (Height > 0))
    {
        Result = Width*Height;
    }

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): RECT 3 FLOAT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline rectangle3
InvertedInfinityRectangle3(void)
{
    rectangle3 Result;

    Result.Min.x = Result.Min.y = Real32Maximum;
    Result.Max.x = Result.Max.y = -Real32Maximum;
    Result.Max.z = Result.Max.z = 0;

    return(Result);
}

inline v3
GetMinCorner(rectangle3 Rect)
{
    v3 Result = Rect.Min;
    return(Result);
}

inline v3
GetMaxCorner(rectangle3 Rect)
{
    v3 Result = Rect.Max;
    return(Result);
}

inline v3
GetDim(rectangle3 Rect)
{
    v3 Result = Rect.Max - Rect.Min;
    return(Result);
}

inline v3
GetCenter(rectangle3 Rect)
{
    v3 Result = 0.5f*(Rect.Min + Rect.Max);
    return(Result);
}

inline rectangle3
RectMinMax(v3 Min, v3 Max)
{
    rectangle3 Result;

    Result.Min = Min;
    Result.Max = Max;

    return(Result);
}

inline rectangle3
RectMinDim(v3 Min, v3 Dim)
{
    rectangle3 Result;

    Result.Min = Min;
    Result.Max = Min + Dim;

    return(Result);
}

inline rectangle3
RectCenterHalfDim(v3 Center, v3 HalfDim)
{
    rectangle3 Result;

    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;

    return(Result);
}

inline rectangle3
AddRadiusTo(rectangle3 A, v3 Radius)
{
    rectangle3 Result;

    Result.Min = A.Min - Radius;
    Result.Max = A.Max + Radius;

    return(Result);
}

inline rectangle3
Offset(rectangle3 A, v3 Offset)
{
    rectangle3 Result;

    Result.Min = A.Min + Offset;
    Result.Max = A.Max + Offset;

    return(Result);
}

inline rectangle3
RectCenterDim(v3 Center, v3 Dim)
{
    rectangle3 Result = RectCenterHalfDim(Center, 0.5f*Dim);

    return(Result);
}

inline bool32
IsInRectangle(rectangle3 Rectangle, v3 Test)
{
    bool32 Result = ((Test.x >= Rectangle.Min.x) &&
                     (Test.y >= Rectangle.Min.y) &&
                     (Test.z >= Rectangle.Min.z) &&
                     (Test.x < Rectangle.Max.x) &&
                     (Test.y < Rectangle.Max.y) &&
                     (Test.z < Rectangle.Max.z));

    return(Result);
}

inline bool32
RectanglesIntersect(rectangle3 A, rectangle3 B)
{
    bool32 Result = !((B.Max.x <= A.Min.x) ||
                      (B.Min.x >= A.Max.x) ||
                      (B.Max.y <= A.Min.y) ||
                      (B.Min.y >= A.Max.y) ||
                      (B.Max.z <= A.Min.z) ||
                      (B.Min.z >= A.Max.z));
    return(Result);
}

inline v3
GetBarycentric(rectangle3 A, v3 P)
{
    v3 Result;

    Result.x = SafeRatio0(P.x - A.Min.x, A.Max.x - A.Min.x);
    Result.y = SafeRatio0(P.y - A.Min.y, A.Max.y - A.Min.y);
    Result.z = SafeRatio0(P.z - A.Min.z, A.Max.z - A.Min.z);

    return(Result);
}

inline rectangle2
ToRectangleXY(rectangle3 A)
{
    rectangle2 Result;

    Result.Min = A.Min.xy;
    Result.Max = A.Max.xy;

    return(Result);
}

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): COLORS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline v4
SRGB255ToLinear1(v4 C)
{
    v4 Result;

    real32 Inv255 = 1.0f / 255.0f;
    
    Result.r = Square(Inv255*C.r);
    Result.g = Square(Inv255*C.g);
    Result.b = Square(Inv255*C.b);
    Result.a = Inv255*C.a;

    return(Result);
}

inline v4
SRGB1ToLinear1(v4 C)
{
    v4 Result;
    
    Result.r = Square(C.r);
    Result.g = Square(C.g);
    Result.b = Square(C.b);
    Result.a = C.a;

    return(Result);
}

inline v4
Linear1ToSRGB255(v4 C)
{
    v4 Result;

    real32 One255 = 255.0f;

    Result.r = One255*SquareRoot(C.r);
    Result.g = One255*SquareRoot(C.g);
    Result.b = One255*SquareRoot(C.b);
    Result.a = One255*C.a;

    return(Result);
}

inline v4
Linear1ToSRGB1(v4 C)
{
    v4 Result;

    Result.r = SquareRoot(C.r);
    Result.g = SquareRoot(C.g);
    Result.b = SquareRoot(C.b);
    Result.a = C.a;

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): GEOMETRY
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline s32
LineIntersect(v2 x0, v2 x1, v2 y0, v2 y1, v2 *sect)
{
    s32 Result = 1;

    v2 dx = x1 - x0;
    v2 dy = y1 - y0;
    r32 d = Cross(dy, dx);

    if(!d)
    {
        Result = 0;
    }
    else
    {
        r32 a = (Cross(x0, dx) - Cross(y0, dx)) / d;
        if(sect)
        {
            *sect = y0 + a*dy;
        }

        if((a < 0.0f) || (a > 1.0f))
        {
            Result = -1;
        }
        else
        {
            a = (Cross(x0, dy) - Cross(y0, dy)) / d;
            if((a < 0) || (a > 1))
            {
                Result = -1;
            }
        }
    }
    
    return(Result);
}

inline r32
Distance(v2 x, v2 y0, v2 y1)
{
    r32 Result = Real32Maximum;
    
    v2 dy = y1 - y0;
    v2 x1 = V2(x.x + dy.y, x.y - dy.x);

    v2 s;
    s32 Intersect = LineIntersect(x, x1, y0, y1, &s);
    if(Intersect != -1)
    {
        s = s - x;
        Result = Length(s);
    }

    return(Result);
}

inline f32
DistanceToSegment(v2 p, v2 a, v2 b)
{
    f32 Result = 0.0f;
    
    f32 l2 = LengthSq(a - b);
    if(l2 == 0.0f)
    {
        Result = Length(p - a);
    }
    else
    {
        v2 pa = p - a;
        v2 ba = b - a;
        f32 t = Inner(pa, ba) / l2;
        t = Clamp(0, t, 1);

        v2 Closest = Lerp(a, t, b);

        Result = Length(p - Closest);
    }

    return(Result);
}


inline s32
LineIntersect(v2d x0, v2d x1, v2d y0, v2d y1, v2d *sect)
{
    s32 Result = 1;

    v2d dx = x1 - x0;
    v2d dy = y1 - y0;
    f64 d = Cross(dy, dx);

    if(!d)
    {
        Result = 0;
    }
    else
    {
        f64 a = (Cross(x0, dx) - Cross(y0, dx)) / d;
        if(sect)
        {
            *sect = y0 + a*dy;
        }

        if((a < 0.0f) || (a > 1.0f))
        {
            Result = -1;
        }
        else
        {
            a = (Cross(x0, dy) - Cross(y0, dy)) / d;
            if((a < 0) || (a > 1))
            {
                Result = -1;
            }
        }
    }
    
    return(Result);
}

inline f64
DistanceToSegment(v2d p, v2d a, v2d b)
{
    f64 Result = 0.0f;
    
    f64 l2 = LengthSq(a - b);
    if(l2 == 0.0f)
    {
        Result = Length(p - a);
    }
    else
    {
        v2d pa = p - a;
        v2d ba = b - a;
        f64 t = Inner(pa, ba) / l2;
        t = Clamp(0, t, 1);

        v2d Closest = Lerp(a, t, b);

        Result = Length(p - Closest);
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): TRIANGLE 2 FLOAT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline f32
TriangleSignedArea(v2 a, v2 b, v2 c)
{
    f32 Result = 0.5f*(a.x*(b.y - c.y) + b.x*(c.y - a.y) + c.x*(a.y - b.y));
    return(Result);
}

inline f32
TriangleSignedArea(triangle *T)
{
    f32 Result = 0.5f*(T->Vertices[0].x*(T->Vertices[1].y - T->Vertices[2].y) +
                       T->Vertices[1].x*(T->Vertices[2].y - T->Vertices[0].y) +
                       T->Vertices[2].x*(T->Vertices[0].y - T->Vertices[1].y));
    return(Result);
}

inline f32
TriangleArea2(v2 a, v2 b, v2 c)
{
    f32 Result = Cross(a, b) + Cross(c, a) + Cross(b, c);
    return(Result);
}

inline f32
TriangleArea2(triangle *T)
{
    f32 Result = (Cross(T->Vertices[0], T->Vertices[1]) +
                  Cross(T->Vertices[2], T->Vertices[0]) +
                  Cross(T->Vertices[1], T->Vertices[2]));
    return(Result);
}

inline b32
IsTriangleCollinear(triangle *A, f32 Epsilon)
{
    f32 d0 = DistanceToSegment(A->Vertices[0], A->Vertices[1], A->Vertices[2]);
    f32 d1 = DistanceToSegment(A->Vertices[1], A->Vertices[0], A->Vertices[2]);
    f32 d2 = DistanceToSegment(A->Vertices[2], A->Vertices[0], A->Vertices[1]);

    b32 Result = ((d0 < Epsilon) || (d1 < Epsilon) || (d2 < Epsilon));
    return(Result);
}

inline b32
IsInTriangle(v2 p, v2 a, v2 b, v2 c)
{
    b32 Result = true;
    
    v2 ab = b - a;
    v2 bc = c - b;
    v2 ca = a - c;

    v2 ap = p - a;
    v2 bp = p - b;
    v2 cp = p - c;

    r32 Cross0 = Cross(ab, ap);
    r32 Cross1 = Cross(bc, bp);
    r32 Cross2 = Cross(ca, cp);

    Result = !((Cross0 > 0.0f) || (Cross1 > 0.0f) || (Cross2 > 0.0f));

    return(Result);
}

inline void
CalculateTriangleBoundingBox(triangle *T)
{
    T->Bounds.Min = T->Bounds.Max = T->Vertices[0];

    for(s32 I = 1;
        I < ArrayCount(T->Vertices);
        ++I)
    {
        if(T->Vertices[I].x < T->Bounds.Min.x) T->Bounds.Min.x = T->Vertices[I].x;
        if(T->Vertices[I].x > T->Bounds.Max.x) T->Bounds.Max.x = T->Vertices[I].x;
        if(T->Vertices[I].y < T->Bounds.Min.y) T->Bounds.Min.y = T->Vertices[I].y;
        if(T->Vertices[I].y > T->Bounds.Max.y) T->Bounds.Max.y = T->Vertices[I].y;
    }
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): TRIANGLE 2 DOUBLE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

inline triangle
TriangleDtoTriangle(triangled T)
{
    triangle Result = {};
    Result.Vertices[0] = V2(T.Vertices[0]);
    Result.Vertices[1] = V2(T.Vertices[1]);
    Result.Vertices[2] = V2(T.Vertices[2]);

    return(Result);
}

inline f64
TriangleSignedArea(v2d a, v2d b, v2d c)
{
    f64 Result = 0.5f*(a.x*(b.y - c.y) + b.x*(c.y - a.y) + c.x*(a.y - b.y));
    return(Result);
}

inline f64
TriangleSignedArea(triangled *T)
{
    f64 Result = 0.5f*(T->Vertices[0].x*(T->Vertices[1].y - T->Vertices[2].y) +
                       T->Vertices[1].x*(T->Vertices[2].y - T->Vertices[0].y) +
                       T->Vertices[2].x*(T->Vertices[0].y - T->Vertices[1].y));
    return(Result);
}

inline b32
IsTriangleCollinear(triangled *A, f64 Epsilon)
{
    f64 d0 = DistanceToSegment(A->Vertices[0], A->Vertices[1], A->Vertices[2]);
    f64 d1 = DistanceToSegment(A->Vertices[1], A->Vertices[0], A->Vertices[2]);
    f64 d2 = DistanceToSegment(A->Vertices[2], A->Vertices[0], A->Vertices[1]);

    b32 Result = ((d0 < Epsilon) || (d1 < Epsilon) || (d2 < Epsilon));
    return(Result);
}

inline b32
IsInTriangle(v2d p, v2d a, v2d b, v2d c)
{
    b32 Result = true;
    
    v2d ab = b - a;
    v2d bc = c - b;
    v2d ca = a - c;

    v2d ap = p - a;
    v2d bp = p - b;
    v2d cp = p - c;

    f64 Cross0 = Cross(ab, ap);
    f64 Cross1 = Cross(bc, bp);
    f64 Cross2 = Cross(ca, cp);

    Result = !((Cross0 > 0.0f) || (Cross1 > 0.0f) || (Cross2 > 0.0f));

    return(Result);
}

inline void
CalculateTriangleBoundingBox(triangled *T)
{
    T->Bounds.Min = T->Bounds.Max = T->Vertices[0];

    for(s32 I = 1;
        I < ArrayCount(T->Vertices);
        ++I)
    {
        if(T->Vertices[I].x < T->Bounds.Min.x) T->Bounds.Min.x = T->Vertices[I].x;
        if(T->Vertices[I].x > T->Bounds.Max.x) T->Bounds.Max.x = T->Vertices[I].x;
        if(T->Vertices[I].y < T->Bounds.Min.y) T->Bounds.Min.y = T->Vertices[I].y;
        if(T->Vertices[I].y > T->Bounds.Max.y) T->Bounds.Max.y = T->Vertices[I].y;
    }
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): POLYGON 2 FLOAT
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline b32
IsPolygonCollinearF32(polygon2 *A, r32 Epsilon)
{
    b32 Result = false;

    for(s32 I = 0;
        I < A->VertexCount;
        ++I)
    {
        s32 First = I;
        s32 Second = (I + 1) % A->VertexCount;
        s32 Third = (I + 2) % A->VertexCount;
        r32 d = DistanceToSegment(A->Vertices[First], A->Vertices[Second], A->Vertices[Third]);
        if(d < Epsilon)
        {
            Result = true;
            break;
        }
    }
    return(Result);
}

inline rectangle2
CalculatePolygonBoundingBox(polygon2 *A)
{
    rectangle2 Result = InvertedInfinityRectangle2();

    for(s32 I = 0;
        I < A->VertexCount;
        ++I)
    {
        if(A->Vertices[I].x < Result.Min.x) Result.Min.x = A->Vertices[I].x;
        if(A->Vertices[I].x > Result.Max.x) Result.Max.x = A->Vertices[I].x;
        if(A->Vertices[I].y < Result.Min.y) Result.Min.y = A->Vertices[I].y;
        if(A->Vertices[I].y > Result.Max.y) Result.Max.y = A->Vertices[I].y;
    }

    return(Result);
}

inline b32
IsConvex(polygon2 *Poly)
{
    b32 Result = true;
    if(Poly->VertexCount > 3)
    {
        s32 Sign = 0;
        for(s32 I = 0;
            I < Poly->VertexCount;
            ++I)
        {
            v2 a = Poly->Vertices[I];
            v2 b = Poly->Vertices[(I + 1) % Poly->VertexCount];
            v2 c = Poly->Vertices[(I + 2) % Poly->VertexCount];

            v2 d1 = b - a;
            v2 d2 = c - b;

            f32 CrossProduct = Cross(d1, d2);

            if(CrossProduct != 0.0f)
            {
                if(Sign == 0)
                {
                    Sign = (CrossProduct > 0.0f) ? 1 : -1;
                }
                else if(((CrossProduct > 0.0f) && (Sign < 0)) || ((CrossProduct < 0.0f) && (Sign > 0)))
                {
                    Result = false;
                    break;
                }
            }
        }
    }

    return(Result);
}

inline f32
PolygonSignedArea2(polygon2 *P)
{
    f32 Result = 0.0f;
    for(s32 I = 1;
        I < (P->VertexCount - 1);
        ++I)
    {
        Result += TriangleArea2(P->Vertices[0], P->Vertices[I], P->Vertices[I + 1]);
    }
    
    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(paul): POLYGON 2 DOUBLE
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
inline f64
PolygonSignedArea(polygon2d *Polygon)
{
    f64 Result = 0.0f;
    for(s32 Index = 0;
        Index < Polygon->VertexCount;
        ++Index)
    {
        s32 Next = (Index + 1) % Polygon->VertexCount;
        v2d FirstVertex = Polygon->Vertices[Index];
        v2d SecondVertex = Polygon->Vertices[Next];

        Result += Cross(FirstVertex, SecondVertex);
    }

    Result = 0.5f*Result;
    
    return(Result);
}

inline b32
IsConvex(polygon2d *Poly)
{
    b32 Result = true;
    if(Poly->VertexCount > 3)
    {
        s32 Sign = 0;
        for(s32 I = 0;
            I < Poly->VertexCount;
            ++I)
        {
            v2d a = Poly->Vertices[I];
            v2d b = Poly->Vertices[(I + 1) % Poly->VertexCount];
            v2d c = Poly->Vertices[(I + 2) % Poly->VertexCount];

            v2d d1 = b - a;
            v2d d2 = c - b;

            f64 CrossProduct = Cross(d1, d2);

            if(CrossProduct != 0.0f)
            {
                if(Sign == 0)
                {
                    Sign = (CrossProduct > 0.0f) ? 1 : -1;
                }
                else if(((CrossProduct > 0.0f) && (Sign < 0)) || ((CrossProduct < 0.0f) && (Sign > 0)))
                {
                    Result = false;
                    break;
                }
            }
        }
    }

    return(Result);
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#define ENGINE_MATH_H
#endif

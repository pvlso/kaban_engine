#if !defined(ENGINE_MATH_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */
    
inline v2
V2i(int32 X, int32 Y)
{
    v2 Result = {(real32)X, (real32)Y};

    return(Result);
}

inline v2
V2i(uint32 X, uint32 Y)
{
    v2 Result = {(real32)X, (real32)Y};

    return(Result);
}

inline v2
V2(real32 X, real32 Y)
{
    v2 Result;

    Result.x = X;
    Result.y = Y;

    return(Result);
}

inline v3
V3i(s32 X, s32 Y, s32 Z)
{
    v3 Result;

    Result.x = (r32)X;
    Result.y = (r32)Y;
    Result.z = (r32)Z;

    return(Result);
}

inline v3
V3i(u32 X, u32 Y, u32 Z)
{
    v3 Result;

    Result.x = (r32)X;
    Result.y = (r32)Y;
    Result.z = (r32)Z;

    return(Result);
}

inline v3
V3(real32 X, real32 Y, real32 Z)
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

inline v4
V4i(s32 X, s32 Y, s32 Z, s32 W)
{
    v4 Result {(r32)X, (r32)Y, (r32)Z, (r32)W};

    return(Result);
}

inline v4
V4i(u32 X, u32 Y, u32 Z, u32 W)
{
    v4 Result {(r32)X, (r32)Y, (r32)Z, (r32)W};

    return(Result);
}

inline v4
V4(real32 X, real32 Y, real32 Z, real32 W)
{
    v4 Result;

    Result.x = X;
    Result.y = Y;
    Result.z = Z;
    Result.w = W;

    return(Result);
}

inline v4
V4(v3 XYZ, real32 W)
{
    v4 Result;

    Result.xyz = XYZ;
    Result.w = W;

    return(Result);
}

//
// NOTE(casey): Scalar operations
//

inline real32
Square(real32 A)
{
    real32 Result = A*A;

    return(Result);
}

inline real32
Lerp(real32 A, real32 t, real32 B)
{
    real32 Result = (1.0f - t)*A + t*B;

    return(Result);
}

inline real32
Clamp(real32 Min, real32 Value, real32 Max)
{
    real32 Result = Value;

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

inline real32
Clamp01(real32 Value)
{
    real32 Result = Clamp(0.0f, Value, 1.0f);

    return(Result);
}

inline real32
Clamp01MapToRange(real32 Min, real32 t, real32 Max)
{
    real32 Result = 0.0f;
    
    real32 Range = Max - Min;
    if(Range != 0.0f)
    {
        Result = Clamp01((t - Min) / Range);
    }

    return(Result);
}

inline real32
SafeRatioN(real32 Numerator, real32 Divisor, real32 N)
{
    real32 Result = N;

    if(Divisor != 0.0f)
    {
        Result = Numerator / Divisor;
    }

    return(Result);
}

inline real32
SafeRatio0(real32 Numerator, real32 Divisor)
{
    real32 Result = SafeRatioN(Numerator, Divisor, 0.0f);

    return(Result);
}

inline real32
SafeRatio1(real32 Numerator, real32 Divisor)
{
    real32 Result = SafeRatioN(Numerator, Divisor, 1.0f);

    return(Result);
}
    
//
// NOTE(casey): v2 operations
//

inline v2
Perp(v2 A)
{
    v2 Result = {-A.y, A.x};
    return(Result);
}

inline v2
operator*(real32 A, v2 B)
{
    v2 Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    
    return(Result);
}

inline v2
operator*(v2 B, real32 A)
{
    v2 Result = A*B;

    return(Result);
}

inline v2 &
operator*=(v2 &B, real32 A)
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
Lerp(v2 A, real32 t, v2 B)
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
    real32 Result = A.x*B.x + A.y*B.y;

    return(Result);
}

inline r32
Cross(v2 A, v2 B)
{
    real32 Result = A.x*B.y - A.y*B.x;

    return(Result);
}

inline real32
LengthSq(v2 A)
{
    real32 Result = Inner(A, A);

    return(Result);
}

inline real32
Length(v2 A)
{
    real32 Result = SquareRoot(LengthSq(A));
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

//
// NOTE(casey): v3 operations
//

inline v3
operator*(real32 A, v3 B)
{
    v3 Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    Result.z = A*B.z;
    
    return(Result);
}

inline v3
operator*(v3 B, real32 A)
{
    v3 Result = A*B;

    return(Result);
}

inline v3 &
operator*=(v3 &B, real32 A)
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

inline real32
Inner(v3 A, v3 B)
{
    real32 Result = A.x*B.x + A.y*B.y + A.z*B.z;

    return(Result);
}

inline real32
LengthSq(v3 A)
{
    real32 Result = Inner(A, A);

    return(Result);
}

inline real32
Length(v3 A)
{
    real32 Result = SquareRoot(LengthSq(A));
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
Lerp(v3 A, real32 t, v3 B)
{
    v3 Result = (1.0f - t)*A + t*B;

    return(Result);
}

//
// NOTE(casey): v4 operations
//

inline v4
operator*(real32 A, v4 B)
{
    v4 Result;

    Result.x = A*B.x;
    Result.y = A*B.y;
    Result.z = A*B.z;
    Result.w = A*B.w;
    
    return(Result);
}

inline v4
operator*(v4 B, real32 A)
{
    v4 Result = A*B;

    return(Result);
}

inline v4 &
operator*=(v4 &B, real32 A)
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

//
// NOTE(casey): Rectangle2
//

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

//
// NOTE(casey): Rectangle3
//

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

//
//
//

inline bool32
RectanglesIntersect(rectangle2 A, rectangle2 B)
{
    bool32 Result = !((B.Max.x <= A.Min.x) ||
                      (B.Min.x >= A.Max.x) ||
                      (B.Max.y <= A.Min.y) ||
                      (B.Min.y >= A.Max.y));
    return(Result);
}

struct rectangle2i
{
    s32 MinX, MinY;
    s32 MaxX, MaxY;
};

inline s32
GetWidth(rectangle2i A)
{
    s32 Result = A.MaxX - A.MinX;
    return(Result);
}

inline s32
GetHeight(rectangle2i A)
{
    s32 Result = A.MaxY - A.MinY;
    return(Result);
}

inline rectangle2i
Offset(rectangle2i A, s32 X, s32 Y)
{
    rectangle2i Result = A;

    Result.MinX += X;
    Result.MaxX += X;
    Result.MinY += Y;
    Result.MaxY += Y;

    return(Result);
}

inline rectangle2i
Intersect(rectangle2i A, rectangle2i B)
{
    rectangle2i Result;
    
    Result.MinX = (A.MinX < B.MinX) ? B.MinX : A.MinX;
    Result.MinY = (A.MinY < B.MinY) ? B.MinY : A.MinY;
    Result.MaxX = (A.MaxX > B.MaxX) ? B.MaxX : A.MaxX;
    Result.MaxY = (A.MaxY > B.MaxY) ? B.MaxY : A.MaxY;    

    return(Result);
}

inline rectangle2i
Union(rectangle2i A, rectangle2i B)
{
    rectangle2i Result;
    
    Result.MinX = (A.MinX < B.MinX) ? A.MinX : B.MinX;
    Result.MinY = (A.MinY < B.MinY) ? A.MinY : B.MinY;
    Result.MaxX = (A.MaxX > B.MaxX) ? A.MaxX : B.MaxX;
    Result.MaxY = (A.MaxY > B.MaxY) ? A.MaxY : B.MaxY;

    return(Result);
}

inline int32
GetClampedRectArea(rectangle2i A)
{
    int32 Width = (A.MaxX - A.MinX);
    int32 Height = (A.MaxY - A.MinY);
    int32 Result = 0;
    if((Width > 0) && (Height > 0))
    {
        Result = Width*Height;
    }

    return(Result);
}

inline bool32
HasArea(rectangle2i A)
{
    bool32 Result = ((A.MinX < A.MaxX) && (A.MinY < A.MaxY));

    return(Result);
}

inline rectangle2i
InvertedInfinityRectangle2i(void)
{
    rectangle2i Result;

    Result.MinX = Result.MinY = INT_MAX;
    Result.MaxX = Result.MaxY = -INT_MAX;

    return(Result);
}

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

#define MAX_POLYGON_COUNT 32
struct polygon2_set
{
    s32 PolygonCount;
    polygon2 *Polygons;
};

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

struct triangle
{
    v2 Vertices[3];
    rectangle2 Bounds;
};

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

inline f32
PolygonSignedArea(polygon2 *Polygon)
{
    f32 Result = 0.0f;
    for(s32 Index = 0;
        Index < Polygon->VertexCount;
        ++Index)
    {
        s32 Next = (Index + 1) % Polygon->VertexCount;
        v2 FirstVertex = Polygon->Vertices[Index];
        v2 SecondVertex = Polygon->Vertices[Next];

        Result += Cross(FirstVertex, SecondVertex);
    }

    Result = 0.5f*Result;
    
    return(Result);
}

#define ENGINE_MATH_H
#endif

#if !defined(ENGINE_CLIPPER_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The user's Clipper-2D (data/Clipper-2d-main/code/clipper, a rewrite of part of Angus Johnson's
  Clipper2: boolean operations on polygons), brought into the unity build: its types that the engine also has are
  renamed (rect_f64, rect_s64, clipper_heap, clipper_sort_entry, clipper_active, clipper_vertex), its memory comes
  from ClipperArena, its copies of the engine's Copy and ZeroSize are gone. ClipPolygons (engine_clipper.cpp) is
  what the engine uses.
*/

// NOTE(pvlso): Clipper's SIMD paths follow the compiler's target (build.bat builds for AVX2 with clang and cl).
// Its "SSE" paths use SSE3 and SSE4.1 (hadd, round), so they go on with AVX; its AVX-512 ones need DQ and VL.
#if defined(__AVX512F__) && defined(__AVX512DQ__) && defined(__AVX512VL__)
#define CLIPPER_AVX512 1
#else
#define CLIPPER_AVX512 0
#endif
#if defined(__AVX2__)
#define CLIPPER_AVX2 1
#else
#define CLIPPER_AVX2 0
#endif
#if defined(__AVX__)
#define CLIPPER_AVX 1
#define CLIPPER_SSE 1
#else
#define CLIPPER_AVX 0
#define CLIPPER_SSE 0
#endif
// NOTE(pvlso): The points' SIMD lanes (v2_f64.W, v2_s64.W) for those paths
#define CLIPPER_SIMD CLIPPER_SSE

#if !defined(CLIPPER_MATH_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#define CLIPPER_HI_PRECISION 0

union v2_s64
{
    struct
    {
        s64 x, y;
    };

#if CLIPPER_SIMD
    __m128i W;
#endif
    s64 E[2];
};

struct path_s64
{
    s32 Count;
    u32 AllocateCount;

    v2_s64 *Points;
};

struct paths_s64
{
    s32 PathCount;
    u32 AllocateCount;

    path_s64 *Paths;
};

union v2_f64
{
    struct
    {
        f64 x, y;
    };

#if CLIPPER_SIMD
    __m128d W;
#endif
    f64 E[2];
};

struct path_f64
{
    s32 Count;
    u32 AllocateCount;

    v2_f64 *Points;
};

struct paths_f64
{
    s32 PathCount;
    u32 AllocateCount;

    path_f64 *Paths;
};

struct rect_f64
{
    v2_f64 Min;
    v2_f64 Max;
};

struct rect_s64
{
    v2_s64 Min;
    v2_s64 Max;
};



inline s64
RoundF64ToInt64(f64 Value)
{
    s64 Result = (s64)round(Value);
    return(Result);
}

inline s64
RoundF32ToInt64(f32 Value)
{
    s64 Result = (s64)roundf(Value);
    return(Result);
}

inline s32
TriSign(s64 x) // returns 0, 1 or -1
{
    s32 Result = ((x > 0) - (x < 0));
    return(Result); 
}

struct mul_u64_res
{
    u64 Value;
    u64 Carry;
};

inline b32
AreMulsEqual(mul_u64_res A, mul_u64_res B)
{
    b32 Result = ((A.Value == B.Value) && (A.Carry == B.Carry));
    return(Result);
}

inline u64
Low(u64 Value)
{
    u64 Result = (Value & 0xFFFFFFFF); 
    return(Result);
}

inline u64
High(u64 Value)
{
    u64 Result = (Value >> 32); 
    return(Result);
}

inline mul_u64_res
MultiplyU64(u64 a, u64 b) // #834, #835
{
    mul_u64_res Result = {};
    
    u64 x1 = Low(a) * Low(b);
    u64 x2 = (High(a) * Low(b)) + High(x1);
    u64 x3 = (Low(a) * High(b)) + Low(x2);
    Result.Value = (Low(x3) << 32) | Low(x1);
    Result.Carry = (High(a) * High(b)) + High(x2) + High(x3);

    return(Result);
}

// returns true if (and only if) a * b == c * d
inline b32
ProductsAreEqual(s64 a, s64 b, s64 c, s64 d)
{
    // nb: unsigned values needed for calculating overflow carry
    u64 abs_a = (u64)abs(a);
    u64 abs_b = (u64)abs(b);
    u64 abs_c = (u64)abs(c);
    u64 abs_d = (u64)abs(d);

    mul_u64_res abs_ab = MultiplyU64(abs_a, abs_b);
    mul_u64_res abs_cd = MultiplyU64(abs_c, abs_d);

    // nb: it's important to differentiate 0 values here from other values
    s32 sign_ab = TriSign(a) * TriSign(b);
    s32 sign_cd = TriSign(c) * TriSign(d);

    b32 Result = (AreMulsEqual(abs_ab, abs_cd) && (sign_ab == sign_cd));
    
    return(Result);
}

#define CLIPPER_MATH_H
#endif

#if !defined(CLIPPER_MATH_F64_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

inline b32
IsValid(rect_f64 Rect)
{
    b32 Result = (Rect.Min.x != DBL_MAX);
    return(Result);
}








inline v2_f64
V2F64(f64 X, f64 Y)
{
    v2_f64 Result = {X, Y}; 

    return(Result);
}

// NOTE(babykaban): Double V2
inline v2_f64
V2F64(s64 X, s64 Y)
{
    v2_f64 Result = {(f64)X, (f64)Y}; 
    return(Result);
}

inline v2_f64
V2F64(v2_s64 A)
{
    v2_f64 Result = V2F64(A.x, A.y);
    return(Result);
}

inline v2_f64
V2F64(v2 A)
{
    v2_f64 Result = {A.x, A.y};
    return(Result);
}
 
inline v2_f64
operator*(f64 A, v2_f64 B)
{
    v2_f64 Result;

#if CLIPPER_SSE
    Result.W = _mm_mul_pd(B.W, _mm_set1_pd(A));
#else
    Result.x = A*B.x;
    Result.y = A*B.y;
#endif
    
    return(Result);
}
 
inline v2_f64
operator*(v2_f64 A, v2_f64 B)
{
    v2_f64 Result;

#if CLIPPER_SSE
    Result.W = _mm_mul_pd(B.W, A.W);
#else
    Result.x = A.x*B.x;
    Result.y = A.y*B.y;
#endif
    return(Result);
}

inline v2_f64
operator*(v2_f64 B, f64 A)
{
    v2_f64 Result = A*B;
    return(Result);
}

inline v2_f64 &
operator*=(v2_f64 &B, f64 A)
{
    B = A*B;

    return(B);
}

inline v2_f64
operator-(v2_f64 A)
{
    v2_f64 Result;

#if CLIPPER_SSE
    Result.W = _mm_xor_pd(A.W, _mm_set1_pd(-0.0));
#else
    Result.x = -A.x;
    Result.y = -A.y;
#endif

    return(Result);
}

inline v2_f64
operator+(v2_f64 A, v2_f64 B)
{
    v2_f64 Result;

#if CLIPPER_SSE
    Result.W = _mm_add_pd(A.W, B.W);
#else
    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
#endif
    return(Result);
}

inline v2_f64 &
operator+=(v2_f64 &A, v2_f64 B)
{
    A = A + B;

    return(A);
}

inline v2_f64
operator-(v2_f64 A, v2_f64 B)
{
    v2_f64 Result;

#if CLIPPER_SSE
    Result.W = _mm_sub_pd(A.W, B.W);
#else
    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
#endif
    return(Result);
}

inline v2_f64 &
operator-=(v2_f64 &A, v2_f64 B)
{
    A = A - B;

    return(A);
}

inline f64
Inner(v2_f64 A, v2_f64 B)
{
#if CLIPPER_SSE
    v2_f64 Mul = A*B;
    f64 Result = _mm_cvtsd_f64(_mm_hadd_pd(Mul.W, Mul.W));
#else
    f64 Result = A.x*B.x + A.y*B.y;
#endif

    return(Result);
}

inline f64
Inner(v2_f64 A, v2_f64 B, v2_f64 C)
{
    v2_f64 AB = B - A;
    v2_f64 BC = C - A;
    f64 Result = Inner(AB, BC);

    return(Result);
}

inline f64
Cross(v2_f64 A, v2_f64 B)
{
    f64 Result = A.x*B.y - A.y*B.x;

    return(Result);
}

inline f64
Cross(v2_f64 A, v2_f64 B, v2_f64 C)
{
    v2_f64 AB = B - A;
    v2_f64 BC = C - A;
    f64 Result = Cross(AB, BC);

    return(Result);
}

inline f64
LengthSq(v2_f64 A)
{
    f64 Result = Inner(A, A);

    return(Result);
}

inline f64
Length(v2_f64 A)
{
    f64 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline b32
PointsAreEqual(v2_f64 A, v2_f64 B)
{
    b32 Result = ((A.x == B.x) && (A.y == B.y));
    return(Result);
}

inline v2_f64
MidPointF64(v2_f64 p1, v2_f64 p2)
{
    v2_f64 Result;
    Result = 0.5*(p1 + p2);
    return(Result);
}

inline rect_f64
InvertedInfinityRectF64(void)
{
    rect_f64 Result;

    Result.Min.x = Result.Min.y = DBL_MAX;
    Result.Max.x = Result.Max.y = -DBL_MAX;

    return(Result);
}

inline v2_f64
GetMinCorner(rect_f64 Rect)
{
    v2_f64 Result = Rect.Min;
    return(Result);
}

inline v2_f64
GetMaxCorner(rect_f64 Rect)
{
    v2_f64 Result = Rect.Max;
    return(Result);
}

inline v2_f64
GetDim(rect_f64 Rect)
{
    v2_f64 Result = Rect.Max - Rect.Min;
    return(Result);
}

inline v2_f64
GetCenter(rect_f64 Rect)
{
    v2_f64 Result = 0.5f*(Rect.Min + Rect.Max);
    return(Result);
}

inline f64
GetArea(rect_f64 A)
{
    v2_f64 Dim = GetDim(A);
    f64 Result = Dim.x*Dim.y;

    return(Result);
}

inline rect_f64
RectMinMax(v2_f64 Min, v2_f64 Max)
{
    rect_f64 Result;

    Result.Min = Min;
    Result.Max = Max;

    return(Result);
}

inline rect_f64
RectMinDim(v2_f64 Min, v2_f64 Dim)
{
    rect_f64 Result;

    Result.Min = Min;
    Result.Max = Min + Dim;

    return(Result);
}

inline rect_f64
RectCenterHalfDim(v2_f64 Center, v2_f64 HalfDim)
{
    rect_f64 Result;

    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;

    return(Result);
}

inline rect_f64
AddRadiusTo(rect_f64 A, v2_f64 Radius)
{
    rect_f64 Result;
    Result.Min = A.Min - Radius;
    Result.Max = A.Max + Radius;

    return(Result);
}

inline rect_f64
ScaleRect(rect_f64 Rect, f64 Scale)
{
    rect_f64 Result = {};
    Result.Min = Scale*Rect.Min;
    Result.Max = Scale*Rect.Max;

    return(Result);
}

inline rect_f64
Offset(rect_f64 A, v2_f64 Offset)
{
    rect_f64 Result;

    Result.Min = A.Min + Offset;
    Result.Max = A.Max + Offset;

    return(Result);
}

inline rect_f64
RectCenterDim(v2_f64 Center, v2_f64 Dim)
{
    rect_f64 Result = RectCenterHalfDim(Center, 0.5f*Dim);

    return(Result);
}

inline b32
IsInRectangle(rect_f64 Rectangle, v2_f64 Test)
{
    bool32 Result = ((Test.x > Rectangle.Min.x) &&
                     (Test.y > Rectangle.Min.y) &&
                     (Test.x < Rectangle.Max.x) &&
                     (Test.y < Rectangle.Max.y));

    return(Result);
}

inline b32
IsInRectangle(rect_f64 Rect, rect_f64 Test)
{
    bool32 Result = ((Test.Min.x >= Rect.Min.x) &&
                     (Test.Min.y >= Rect.Min.y) &&
                     (Test.Max.x <= Rect.Max.x) &&
                     (Test.Max.y <= Rect.Max.y));

    return(Result);
}

inline b32
Intersects(rect_f64 A, rect_f64 B)
{
    b32 Result = ((Maximum(A.Min.x, B.Min.x) <= Minimum(A.Max.x, B.Max.x)) &&
                  (Maximum(A.Max.y, B.Max.y) <= Minimum(A.Min.y, B.Min.y)));

    return(Result);
}

inline b32
IsRectEmpty(rect_f64 Rect)
{
    b32 Result = ((Rect.Min.y <= Rect.Max.y) ||
                  (Rect.Max.x <= Rect.Min.x));

    return(Result);
}

inline v2_f64
GetBarycentric(rect_f64 A, v2_f64 P)
{
    v2_f64 Result;

    Result.x = SafeRatio0(P.x - A.Min.x, A.Max.x - A.Min.x);
    Result.y = SafeRatio0(P.y - A.Min.y, A.Max.y - A.Min.y);

    return(Result);
}

inline b32
RectsAreEqual(rect_f64 A, rect_f64 B)
{
    b32 Result = (PointsAreEqual(A.Min, B.Min) &&
                  PointsAreEqual(A.Max, B.Max));
    return(Result);
}

inline rect_f64
Union(rect_f64 A, rect_f64 B)
{
    rect_f64 Result;
    
    Result.Min.x = Minimum(A.Min.x, B.Min.x);
    Result.Min.y = Minimum(A.Min.y, B.Min.y);
    Result.Max.x = Maximum(A.Max.x, B.Max.x);
    Result.Max.y = Maximum(A.Max.y, B.Max.y);

    return(Result);
}

inline rect_f64
GetBounds(path_f64 *Path)
{
    rect_f64 Result = InvertedInfinityRectF64();

    for(s32 I = 0;
        I < Path->Count;
        ++I)
    {
        v2_f64 p = Path->Points[I];
        if (p.x < Result.Min.x) Result.Min.x = p.x;
        if (p.x > Result.Max.x) Result.Max.x = p.x;
        if (p.y < Result.Min.y) Result.Min.y = p.y;
        if (p.y > Result.Max.y) Result.Max.y = p.y;
    }

    return(Result);
}

inline rect_f64
GetBounds(paths_f64 *Paths)
{
    rect_f64 Result = InvertedInfinityRectF64();

    Result = GetBounds(Paths->Paths + 0);
    for(s32 I = 1;
        I < Paths->PathCount;
        ++I)
    {
        path_f64 *Path = Paths->Paths + I;
        rect_f64 Bounds = GetBounds(Path);
        Result = Union(Result, Bounds);
    }

    return(Result);
}

inline b32
NearEqual(v2_f64 p1, v2_f64 p2, f64 MaxDistSq)
{
    v2_f64 p21 = p1 - p2;

    b32 Result = (Square(p21.x) + Square(p21.y) < MaxDistSq);
    return(Result);
}


inline f64
PerpDistFromLineSq(v2_f64 p, v2_f64 a, v2_f64 b)
{
    f64 Result = 0.0;

    v2_f64 ap = p - a;
    v2_f64 ab = b - a;
    if(!((ab.x == 0) && (ab.y == 0)))
    {
        Result = Square(Cross(ap, ab)) / Inner(ab, ab);
    }

    return(Result);
}

inline f64
Area(path_f64 *Path)
{
    f64 Result = 0.0;

    s32 Count = Path->Count;
    if(Count >= 3)
    {
        v2_f64 *Iter1 = Path->Points + (Count - 2);
        v2_f64 *Iter2 = Iter1;
        v2_f64 *Stop = Iter2;

        if(!(Count & 1))
        {
            ++Stop;
        }

        for(Iter1 = Path->Points;
            !PointsAreEqual(*Iter1, *Stop);)
        {
            Result += (Iter2->y + Iter1->y) * (Iter2->x - Iter1->x);
            Iter2 = Iter1 + 1;
            Result += (Iter1->y + Iter2->y) * (Iter1->x - Iter2->x);
            Iter1 += 2;
        }

        if(Count & 1)
        {
            Result += (Iter2->y + Iter1->y) * (Iter2->x - Iter1->x);
        }

        Result *= 0.5;
    }

    return(Result);
}

inline f64
Area(paths_f64 *Paths)
{
    f64 Result = 0.0;
    for(s32 I = 0;
        I < Paths->PathCount;
        ++I)
    {
        Result += Area(Paths->Paths + I);
    }

    return(Result);
}

inline b32
IsPositive(path_f64 *Poly)
{
    // A curve has positive orientation [and area] if a region 'R'
    // is on the left when traveling around the outside of 'R'.
    //https://mathworld.wolfram.com/CurveOrientation.html
    //nb: This statement is premised on using Cartesian coordinates
    b32 Result = (Area(Poly) >= 0);
    return(Result);
}

inline b32
IsCollinear(v2_f64 p1, v2_f64 SharedP, v2_f64 p2) // #777
{
    s64 a = RoundF64ToInt64(SharedP.x - p1.x);
    s64 b = RoundF64ToInt64(p2.y - SharedP.y);
    s64 c = RoundF64ToInt64(SharedP.y - p1.y);
    s64 d = RoundF64ToInt64(p2.x - SharedP.x);
    // When checking for collinearity with very large coordinate values
    // then ProductsAreEqual is more accurate than using CrossProduct.

    b32 Result = ProductsAreEqual(a, b, c, d);
    return(Result);
}

#if CLIPPER_HI_PRECISION
// caution: this will compromise performance
#define CC_MIN(x,y) ((x)>(y)?(y):(x))
#define CC_MAX(x,y) ((x)<(y)?(y):(x))

inline b32
GetSegmentIntersectPt(v2_f64 a, v2_f64 b, v2_f64 c, v2_f64 d, v2_f64 *p)
{
    b32 Result = false;

    f64 ln1dy = (b.y - a.y);
    f64 ln1dx = (a.x - b.x);
    f64 ln2dy = (d.y - c.y);
    f64 ln2dx = (c.x - d.x);
    f64 det = (ln2dy * ln1dx) - (ln1dy * ln2dx);

    if(det != 0.0)
    {
        f64 bb0minx = CC_MIN(a.x, b.x);
        f64 bb0miny = CC_MIN(a.y, b.y);
        f64 bb0maxx = CC_MAX(a.x, b.x);
        f64 bb0maxy = CC_MAX(a.y, b.y);
        f64 bb1minx = CC_MIN(c.x, d.x);
        f64 bb1miny = CC_MIN(c.y, d.y);
        f64 bb1maxx = CC_MAX(c.x, d.x);
        f64 bb1maxy = CC_MAX(c.y, d.y);

        f64 originx = (CC_MIN(bb0maxx, bb1maxx) + CC_MAX(bb0minx, bb1minx)) / 2.0;
        f64 originy = (CC_MIN(bb0maxy, bb1maxy) + CC_MAX(bb0miny, bb1miny)) / 2.0;
        f64 ln0c = (ln1dy * (a.x - originx)) + (ln1dx * (a.y - originy));
        f64 ln1c = (ln2dy * (c.x - originx)) + (ln2dx * (c.y - originy));
        f64 hitx = ((ln1dx * ln1c) - (ln2dx * ln0c)) / det;
        f64 hity = ((ln2dy * ln0c) - (ln1dy * ln1c)) / det;

        p->x = originx + hitx;
        p->y = originy + hity;
        Result = true;
    }

    return(Result);
}

#else

inline b32
GetSegmentIntersectPt(v2_f64 a, v2_f64 b, v2_f64 c, v2_f64 d, v2_f64 *p)
{
    // https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection

    b32 Result = false;
    
    v2_f64 ab = b - a;
    v2_f64 cd = d - c;

    f64 det = Cross(cd, ab);
    if(det != 0.0)
    {
        f64 t = ((a.x - c.x) * cd.y - (a.y - c.y) * cd.x) / det;
        if(t <= 0.0) *p = a;
        else if(t >= 1.0) *p = b;
        else
        {
            *p = a + t*ab;
        }
    }

    return true;
}
#endif

inline v2_f64
ReflectPoint(v2_f64 p, v2_f64 pivot)
{
    v2_f64 Result = pivot + (pivot - p);
    return(Result);
}

inline s32
GetSign(f64 Val) 
{ 
    s32 Result = 0;
    if(Val)
    {
        Result =  (Val > 0) ? 1 : -1;
    }

    return(Result);
}

inline v2_f64
GetClosestPointOnSegment(v2_f64 offPt, v2_f64 seg1, v2_f64 seg2)
{
    v2_f64 Result = {};
    
    if(PointsAreEqual(seg1, seg2))
    {
        Result = seg1;
    }
    else
    {
        v2_f64 d = seg2 - seg1;

        f64 q = Inner(offPt - seg1, d) / Inner(d, d);
        Clamp01(q);

        Result = seg1 + q*d;
    }

    return(Result);
}

enum point_in_poly_res
{
    PointInPolyRes_On,
    PointInPolyRes_Inside,
    PointInPolyRes_Outside,
};

inline point_in_poly_res
PointInPolygon(v2_f64 p, path_f64 *Poly)
{
    point_in_poly_res Result = PointInPolyRes_Outside;

    s32 val = 0;
    if(Poly->Count >= 3)
    {
        v2_f64 *Start = Poly->Points + 0;
        v2_f64 *End = Poly->Points + (Poly->Count - 1);
        v2_f64 *First = Start;
        v2_f64 *Cur;
        v2_f64 *Prev;

        while (PointsAreEqual(*First, *End) && (First->y == p.y))
            ++First;

        if(!PointsAreEqual(*First, *End))
        {
            b32 IsAbove = (First->y < p.y);
            b32 StartingAbove = IsAbove;

            Cur = First + 1;
            while(true)
            {
                if(PointsAreEqual(*Cur, *End))
                {
                    if(PointsAreEqual(*End, *First) ||
                       PointsAreEqual(*First, *Start))
                    {
                        break;
                    }

                    End = First;
                    Cur = Start;
                }

                if(IsAbove)
                {
                    while(!PointsAreEqual(*Cur, *End) && (Cur->y < p.y))
                        ++Cur;
                }
                else
                {
                    while(!PointsAreEqual(*Cur, *End) && (Cur->y > p.y))
                        ++Cur;
                }

                if(!PointsAreEqual(*Cur, *End))
                {
                    if(PointsAreEqual(*Cur, *Start))
                        Prev = Poly->Points + (Poly->Count - 2); //nb: NOT cend (since might equal first)
                    else
                        Prev = Cur - 1;

                    if(Cur->y == p.y)
                    {
                        if((Cur->x == p.x) ||
                           ((Cur->y == Prev->y) &&
                            ((p.x < Prev->x) != (p.x < Cur->x))))
                        {
                            Result = PointInPolyRes_On;
                            break;
                        }
                        else
                        {
                            ++Cur;
                            if(PointsAreEqual(*Cur, *First))
                                break;
                        }
                    }
                    else
                    {
                        if((p.x < Cur->x) && (p.x < Prev->x))
                        {
                            // we're only interested in edges crossing on the left
                        }
                        else if((p.x > Prev->x) && (p.x > Cur->x))
                            val = 1 - val; // toggle val
                        else
                        {
                            f64 d = Cross(*Prev, *Cur, p);
                            if(d == 0)
                            {
                                Result = PointInPolyRes_On;
                                break;
                            }
                            else if ((d < 0) == IsAbove)
                                val = 1 - val;
                        }

                        IsAbove = !IsAbove;
                        ++Cur;
                    }
                }
            }

            if((IsAbove != StartingAbove) && (Result != PointInPolyRes_On))
            {
                End = Poly->Points + (Poly->Count - 1);
                if(PointsAreEqual(*Cur, *End))
                    Cur = Start;
                if(PointsAreEqual(*Cur, *Start))
                    Prev = End - 1;
                else
                    Prev = Cur - 1;

                f64 d = Cross(*Prev, *Cur, p);
                if(d == 0)
                    Result = PointInPolyRes_On;
                else if ((d < 0) == IsAbove)
                    val = 1 - val;
            }
        }
    }

    if(Result != PointInPolyRes_On)
        Result = (val == 0) ? PointInPolyRes_Outside : PointInPolyRes_Inside;

    return(Result);
}

#define CLIPPER_MATH_F64_H
#endif

#if !defined(CLIPPER_MATH_S64_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

inline b32
IsValid(rect_s64 Rect)
{
    b32 Result = (Rect.Min.x != DBL_MAX);
    return(Result);
}

inline f64
Square(s64 A)
{
    f64 Result = (f64)A*(f64)A;

    return(Result);
}

inline v2_s64
V2S64(s64 X, s64 Y)
{
    v2_s64 Result = {X, Y}; 
    return(Result);
}

inline v2_s64
CastV2S64(v2_f64 A)
{
    v2_s64 Result = {(s64)A.x, (s64)A.y}; 

    return(Result);
}

inline v2_s64
V2S64(f64 X, f64 Y)
{
    v2_s64 Result = {};
#if CLIPPER_AVX512
    Result.W = _mm_cvtpd_epi64(_mm_round_pd(_mm_set_pd(Y, X),
                                            _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
#elif CLIPPER_SSE
    v2_f64 T = {};
    T.W = _mm_round_pd(_mm_set_pd(Y, X), _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC);
    Result = CastV2S64(T);
#else
    Result.x = (s64)round(X);
    Result.y = (s64)round(Y);
#endif
    
    return(Result);
}

inline v2_s64
V2S64(v2_f64 A)
{
    v2_s64 Result = V2S64(A.x, A.y);
    return(Result);
}

inline v2_s64
operator*(f64 A, v2_s64 B)
{
    v2_s64 Result;
    Result = V2S64(V2F64(B)*A);
    return(Result);
}

inline v2_s64
operator*(v2_s64 B, f64 A)
{
    v2_s64 Result = A*B;
    return(Result);
}

inline v2_s64 &
operator*=(v2_s64 &B, f64 A)
{
    B = A*B;

    return(B);
}

inline v2_s64
operator-(v2_s64 A)
{
    v2_s64 Result;

    Result.x = -A.x;
    Result.y = -A.y;

    return(Result);
}

inline v2_s64
operator+(v2_s64 A, v2_s64 B)
{
    v2_s64 Result;

#if CLIPPER_SSE
    Result.W = _mm_add_epi64(A.W, B.W);
#else
    Result.x = A.x + B.x;
    Result.y = A.y + B.y;
#endif
    return(Result);
}

inline v2_s64 &
operator+=(v2_s64 &A, v2_s64 B)
{
    A = A + B;

    return(A);
}

inline v2_s64
operator-(v2_s64 A, v2_s64 B)
{
    v2_s64 Result;

#if CLIPPER_SSE
    Result.W = _mm_sub_epi64(A.W, B.W);
#else
    Result.x = A.x - B.x;
    Result.y = A.y - B.y;
#endif
    return(Result);
}

inline v2_s64 &
operator-=(v2_s64 &A, v2_s64 B)
{
    A = A - B;

    return(A);
}

inline f64
Inner(v2_s64 A, v2_s64 B)
{
    f64 Result = Inner(V2F64(A), V2F64(B));

    return(Result);
}

inline f64
Inner(v2_s64 A, v2_s64 B, v2_s64 C)
{
    v2_f64 AB = V2F64(B - A);
    v2_f64 BC = V2F64(C - A);
    f64 Result = Inner(AB, BC);

    return(Result);
}

inline f64
Cross(v2_s64 A, v2_s64 B)
{
    f64 Result = Cross(V2F64(A), V2F64(B));

    return(Result);
}

inline f64
Cross(v2_s64 A, v2_s64 B, v2_s64 C)
{
    v2_f64 AB = V2F64(B - A);
    v2_f64 BC = V2F64(C - A);
    f64 Result = Cross(AB, BC);

    return(Result);
}

inline f64
LengthSq(v2_s64 A)
{
    f64 Result = Inner(A, A);

    return(Result);
}

inline f64
Length(v2_s64 A)
{
    f64 Result = SquareRoot(LengthSq(A));
    return(Result);
}

inline b32
PointsAreEqual(v2_s64 A, v2_s64 B)
{
    b32 Result = ((A.x == B.x) && (A.y == B.y));
    return(Result);
}

inline v2_s64
MidPointS64(v2_s64 p1, v2_s64 p2)
{
    v2_s64 Result;
    Result.x = (p1.x + p2.x) / 2;
    Result.y = (p1.y + p2.y) / 2;
    return(Result);
}

inline rect_s64
InvertedInfinityRectS64(void)
{
    rect_s64 Result;

    Result.Min.x = Result.Min.y = INT64_MAX;
    Result.Max.x = Result.Max.y = -INT64_MAX;

    return(Result);
}

inline v2_s64
GetMinCorner(rect_s64 Rect)
{
    v2_s64 Result = Rect.Min;
    return(Result);
}

inline v2_s64
GetMaxCorner(rect_s64 Rect)
{
    v2_s64 Result = Rect.Max;
    return(Result);
}

inline v2_s64
GetDim(rect_s64 Rect)
{
    v2_s64 Result = Rect.Max - Rect.Min;
    return(Result);
}

inline v2_s64
GetCenter(rect_s64 Rect)
{
    v2_s64 Result = {};
    Result.x = (Rect.Min.x + Rect.Max.x) / 2;
    Result.y = (Rect.Min.y + Rect.Max.y) / 2;
    return(Result);
}

inline rect_s64
RectMinMax(v2_s64 Min, v2_s64 Max)
{
    rect_s64 Result;

    Result.Min = Min;
    Result.Max = Max;

    return(Result);
}

inline rect_s64
RectMinDim(v2_s64 Min, v2_s64 Dim)
{
    rect_s64 Result;

    Result.Min = Min;
    Result.Max = Min + Dim;

    return(Result);
}

inline rect_s64
RectCenterHalfDim(v2_s64 Center, v2_s64 HalfDim)
{
    rect_s64 Result;

    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;

    return(Result);
}

inline rect_s64
AddRadiusTo(rect_s64 A, v2_s64 Radius)
{
    rect_s64 Result;
    Result.Min = A.Min - Radius;
    Result.Max = A.Max + Radius;

    return(Result);
}

inline rect_s64
ScaleRect(rect_s64 Rect, f64 Scale)
{
    rect_s64 Result = {};
    Result.Min = Scale*Rect.Min;
    Result.Max = Scale*Rect.Max;

    return(Result);
}

inline rect_s64
Offset(rect_s64 A, v2_s64 Offset)
{
    rect_s64 Result;

    Result.Min = A.Min + Offset;
    Result.Max = A.Max + Offset;

    return(Result);
}

inline rect_s64
RectCenterDim(v2_s64 Center, v2_s64 Dim)
{
    rect_s64 Result = RectCenterHalfDim(Center, 0.5f*Dim);

    return(Result);
}

inline b32
IsInRectangle(rect_s64 Rectangle, v2_s64 Test)
{
    bool32 Result = ((Test.x > Rectangle.Min.x) &&
                     (Test.y > Rectangle.Min.y) &&
                     (Test.x < Rectangle.Max.x) &&
                     (Test.y < Rectangle.Max.y));

    return(Result);
}

inline b32
IsInRectangle(rect_s64 Rect, rect_s64 Test)
{
    bool32 Result = ((Test.Min.x >= Rect.Min.x) &&
                     (Test.Min.y >= Rect.Min.y) &&
                     (Test.Max.x <= Rect.Max.x) &&
                     (Test.Max.y <= Rect.Max.y));

    return(Result);
}

inline b32
Intersects(rect_s64 A, rect_s64 B)
{
    b32 Result = ((Maximum(A.Min.x, B.Min.x) <= Minimum(A.Max.x, B.Max.x)) &&
                  (Maximum(A.Max.y, B.Max.y) <= Minimum(A.Min.y, B.Min.y)));

    return(Result);
}

inline b32
IsRectEmpty(rect_s64 Rect)
{
    b32 Result = ((Rect.Min.y <= Rect.Max.y) ||
                  (Rect.Max.x <= Rect.Min.x));

    return(Result);
}

inline b32
RectsAreEqual(rect_s64 A, rect_s64 B)
{
    b32 Result = (PointsAreEqual(A.Min, B.Min) &&
                  PointsAreEqual(A.Max, B.Max));
    return(Result);
}

inline rect_s64
Union(rect_s64 A, rect_s64 B)
{
    rect_s64 Result;
    
    Result.Min.x = Minimum(A.Min.x, B.Min.x);
    Result.Min.y = Minimum(A.Min.y, B.Min.y);
    Result.Max.x = Maximum(A.Max.x, B.Max.x);
    Result.Max.y = Maximum(A.Max.y, B.Max.y);

    return(Result);
}

inline rect_s64
GetBounds(path_s64 *Path)
{
    rect_s64 Result = InvertedInfinityRectS64();

    for(s32 I = 0;
        I < Path->Count;
        ++I)
    {
        v2_s64 p = Path->Points[I];
        if (p.x < Result.Min.x) Result.Min.x = p.x;
        if (p.x > Result.Max.x) Result.Max.x = p.x;
        if (p.y < Result.Min.y) Result.Min.y = p.y;
        if (p.y > Result.Max.y) Result.Max.y = p.y;
    }

    return(Result);
}

inline rect_s64
GetBounds(paths_s64 *Paths)
{
    rect_s64 Result = InvertedInfinityRectS64();

    Result = GetBounds(Paths->Paths + 0);
    for(s32 I = 1;
        I < Paths->PathCount;
        ++I)
    {
        path_s64 *Path = Paths->Paths + I;
        rect_s64 Bounds = GetBounds(Path);
        Result = Union(Result, Bounds);
    }

    return(Result);
}


inline b32
NearEqual(v2_s64 p1, v2_s64 p2, f64 MaxDistSq)
{
    v2_s64 p21 = p1 - p2;

    b32 Result = (Square(p21.x) + Square(p21.y) < MaxDistSq);
    return(Result);
}


inline f64
PerpDistFromLineSq(v2_s64 p, v2_s64 a, v2_s64 b)
{
//    TimeFunction;

    // a = p.x - a.x : ap.x
    // b = b.x - a.x : ab.x
    // c = p.y - a.y : ap.y
    // d = b.y - a.y : ab.y

    // c = a*d - c*b
    // r = c^2 / b*b + d*d
    // r = (a*d - c*b)^2 / b*b + d*d
    // r = (a*d - c*b)^2 / b^2 + d^2

    //      (a*d)^2 - 2*(a*d)*(c*b) + (c*b)^2
    // r =  _________________________________
    //                 b^2 + d^2

    // sub(a, b, c, d) = _mm256_sub_pd(set(p.x, b.x, p.y, b.y), set(a.x, a.x, a.y, a.y))

    // s[4] = mul(ad(x), bb(z), bc(y), dd(w)) = _mm256_mul_pd(set(a, b, c, d), set(d, b, b, d))
    
    //          (x - y)^2
    // r =     _____________
    //           z + w
    
    // r = (s[0] - s[2])^2 / (s[1] + s[3]);
    f64 Result = 0.0;

#if CLIPPER_AVX
    f64 buf[4] = {};
    __m256d Sub = _mm256_sub_pd(_mm256_set_pd((f64)p.x, (f64)b.x, (f64)p.y, (f64)b.y),
                                _mm256_set_pd((f64)a.x, (f64)a.x, (f64)a.y, (f64)a.y));

    _mm256_storeu_pd(buf, Sub);

    if(!((buf[0] == 0) && (buf[2] == 0)))    
    {
        __m256d Sub1 = _mm256_set_pd(buf[0], buf[2], buf[2], buf[0]);
        __m256d Mul = _mm256_mul_pd(Sub, Sub1);
        _mm256_storeu_pd(buf, Mul);

        Result = Square(buf[3] - buf[1]) / (buf[2] + buf[0]);
    }
#else
    v2_f64 ap = V2F64(p - a);
    v2_f64 ab = V2F64(b - a);
    
    if(!((ab.x == 0) && (ab.y == 0)))
    {
        Result = Square(Cross(ap, ab)) / Inner(ab, ab);
    }
#endif    

    return(Result);
}

inline f64
Area(path_s64 *Path)
{
    f64 Result = 0.0;

    s32 Count = Path->Count;
    if(Count >= 3)
    {
        v2_s64 *Iter1 = Path->Points + (Count - 2);
        v2_s64 *Iter2 = Iter1;
        v2_s64 *Stop = Iter2;

        if(!(Count & 1))
        {
            ++Stop;
        }

        for(Iter1 = Path->Points;
            !PointsAreEqual(*Iter1, *Stop);)
        {
            Result += (f64)((Iter2->y + Iter1->y) * (Iter2->x - Iter1->x));
            Iter2 = Iter1 + 1;
            Result += (f64)((Iter1->y + Iter2->y) * (Iter1->x - Iter2->x));
            Iter1 += 2;
        }

        if(Count & 1)
        {
            Result += (Iter2->y + Iter1->y) * (Iter2->x - Iter1->x);
        }

        Result *= 0.5;
    }

    return(Result);
}

inline f64
Area(paths_s64 *Paths)
{
    f64 Result = 0.0;
    for(s32 I = 0;
        I < Paths->PathCount;
        ++I)
    {
        Result += Area(Paths->Paths + I);
    }

    return(Result);
}

inline b32
IsPositive(path_s64 *Poly)
{
    // A curve has positive orientation [and area] if a region 'R'
    // is on the left when traveling around the outside of 'R'.
    //https://mathworld.wolfram.com/CurveOrientation.html
    //nb: This statement is premised on using Cartesian coordinates
    b32 Result = (Area(Poly) >= 0);
    return(Result);
}

inline b32
IsCollinear(v2_s64 p1, v2_s64 SharedP, v2_s64 p2) // #777
{
    s64 a = SharedP.x - p1.x;
    s64 b = p2.y - SharedP.y;
    s64 c = SharedP.y - p1.y;
    s64 d = p2.x - SharedP.x;
    // When checking for collinearity with very large coordinate values
    // then ProductsAreEqual is more accurate than using CrossProduct.
    b32 Result = ProductsAreEqual(a, b, c, d);
    return(Result);
}

#if CLIPPER2_HI_PRECISION
// caution: this will compromise performance
// https://github.com/AngusJohnson/Clipper2/issues/317#issuecomment-1314023253
// See also CPP/BenchMark/GetIntersectPtBenchmark.cpp
#define CC_MIN(x,y) ((x)>(y)?(y):(x))
#define CC_MAX(x,y) ((x)<(y)?(y):(x))

inline b32
GetSegmentIntersectPt(v2_s64 a, v2_s64 b, v2_s64 c, v2_s64 d, v2_s64 *p)
{
    b32 Result = false;

    f64 ln1dy = (b.y - a.y);
    f64 ln1dx = (a.x - b.x);
    f64 ln2dy = (d.y - c.y);
    f64 ln2dx = (c.x - d.x);
    f64 det = (ln2dy * ln1dx) - (ln1dy * ln2dx);

    if(det != 0.0)
    {
        f64 bb0minx = CC_MIN(a.x, b.x);
        f64 bb0miny = CC_MIN(a.y, b.y);

        f64 bb0maxx = CC_MAX(a.x, b.x);
        f64 bb0maxy = CC_MAX(a.y, b.y);

        f64 bb1minx = CC_MIN(c.x, d.x);
        f64 bb1miny = CC_MIN(c.y, d.y);

        f64 bb1maxx = CC_MAX(c.x, d.x);
        f64 bb1maxy = CC_MAX(c.y, d.y);

        f64 originx = (CC_MIN(bb0maxx, bb1maxx) + CC_MAX(bb0minx, bb1minx)) / 2.0;
        f64 originy = (CC_MIN(bb0maxy, bb1maxy) + CC_MAX(bb0miny, bb1miny)) / 2.0;

        f64 ln0c = (ln1dy * (a.x - originx)) + (ln1dx * (a.y - originy));
        f64 ln1c = (ln2dy * (c.x - originx)) + (ln2dx * (c.y - originy));

        f64 hitx = ((ln1dx * ln1c) - (ln2dx * ln0c)) / det;
        f64 hity = ((ln2dy * ln0c) - (ln1dy * ln1c)) / det;

        // TODO(babykaban): The problem might be here
        p->x = originx + hitx;
        p->y = originy + hity;

        Result = true;
    }

    return(Result);
}

#else

inline b32
GetSegmentIntersectPt(v2_s64 a, v2_s64 b, v2_s64 c, v2_s64 d, v2_s64 *p)
{
    // https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection

    b32 Result = false;
    
    v2_f64 ab = V2F64(b - a);
    v2_f64 cd = V2F64(d - c);

//    f64 det = Cross(cd, ab);
    f64 det = cd.x*ab.y - cd.y*ab.x;
    if(det != 0.0)
    {
        f64 t = ((a.x - c.x) * cd.y - (a.y - c.y) * cd.x) / det;

        if(t <= 0.0)
            *p = a;
        else if(t >= 1.0)
            *p = b;
        else
        {
            *p = {(s64)(a.x + t*ab.x), (s64)(a.y + t*ab.y)};
        }
    }

    return true;
}
#endif

inline v2_s64
TranslatePoint(v2_s64 p, v2_s64 Offset)
{
    // TODO(babykaban): Eliminate
    v2_s64 Result = p + Offset;
    return(Result);
}

inline v2_s64
ReflectPoint(v2_s64 p, v2_s64 pivot)
{
    v2_s64 Result = pivot + (pivot - p);
    return(Result);
}

inline s32
GetSign(s64 Val) 
{ 
    s32 Result = 0;
    if(Val)
    {
        Result =  (Val > 0) ? 1 : -1;
    }

    return(Result);
}

inline bool
SegmentsIntersect(v2_s64 seg1a, v2_s64 seg1b, v2_s64 seg2a, v2_s64 seg2b, b32 inclusive = false)
{
    b32 Result = false;

    if(inclusive)
    {
        f64 res1 = Cross(seg1a, seg2a, seg2b);
        f64 res2 = Cross(seg1b, seg2a, seg2b);
        if((res1 * res2) <= 0)
        {
            f64 res3 = Cross(seg2a, seg1a, seg1b);
            f64 res4 = Cross(seg2b, seg1a, seg1b);
            if ((res3 * res4) <= 0)
            {
                Result = (res1 || res2 || res3 || res4); // ensures not collinear
            }
        }
    }
    else
    {
        Result = ((GetSign(Cross(seg1a, seg2a, seg2b)) * GetSign(Cross(seg1b, seg2a, seg2b)) < 0) &&
                  (GetSign(Cross(seg2a, seg1a, seg1b)) * GetSign(Cross(seg2b, seg1a, seg1b)) < 0));
    }

    return(Result);
}

inline v2_s64
GetClosestPointOnSegment(v2_s64 offPt, v2_s64 seg1, v2_s64 seg2)
{
    v2_s64 Result = {};
    
    if(PointsAreEqual(seg1, seg2))
    {
        Result = seg1;
    }
    else
    {
        v2_f64 d = V2F64(seg2 - seg1);

        f64 q = Inner(V2F64(offPt - seg1), d) / Inner(d, d);
        Clamp01(q);

        Result = seg1 + V2S64(q*d);
    }

    return(Result);
}


inline point_in_poly_res
PointInPolygon(v2_s64 p, path_s64 *Poly)
{
    point_in_poly_res Result = PointInPolyRes_Outside;

    s32 val = 0;
    if(Poly->Count >= 3)
    {
        v2_s64 *Start = Poly->Points + 0;
        v2_s64 *End = Poly->Points + (Poly->Count - 1);
        v2_s64 *First = Start;
        v2_s64 *Cur;
        v2_s64 *Prev;

        while (PointsAreEqual(*First, *End) && (First->y == p.y))
            ++First;

        if(!PointsAreEqual(*First, *End))
        {
            b32 IsAbove = (First->y < p.y);
            b32 StartingAbove = IsAbove;

            Cur = First + 1;
            while(true)
            {
                if(PointsAreEqual(*Cur, *End))
                {
                    if(PointsAreEqual(*End, *First) ||
                       PointsAreEqual(*First, *Start))
                    {
                        break;
                    }

                    End = First;
                    Cur = Start;
                }

                if(IsAbove)
                {
                    while(!PointsAreEqual(*Cur, *End) && (Cur->y < p.y))
                        ++Cur;
                }
                else
                {
                    while(!PointsAreEqual(*Cur, *End) && (Cur->y > p.y))
                        ++Cur;
                }

                if(!PointsAreEqual(*Cur, *End))
                {
                    if(PointsAreEqual(*Cur, *Start))
                        Prev = Poly->Points + (Poly->Count - 2); //nb: NOT cend (since might equal first)
                    else
                        Prev = Cur - 1;

                    if(Cur->y == p.y)
                    {
                        if((Cur->x == p.x) ||
                           ((Cur->y == Prev->y) &&
                            ((p.x < Prev->x) != (p.x < Cur->x))))
                        {
                            Result = PointInPolyRes_On;
                            break;
                        }
                        else
                        {
                            ++Cur;
                            if(PointsAreEqual(*Cur, *First))
                                break;
                        }
                    }
                    else
                    {
                        if((p.x < Cur->x) && (p.x < Prev->x))
                        {
                            // we're only interested in edges crossing on the left
                        }
                        else if((p.x > Prev->x) && (p.x > Cur->x))
                            val = 1 - val; // toggle val
                        else
                        {
                            f64 d = Cross(*Prev, *Cur, p);
                            if(d == 0)
                            {
                                Result = PointInPolyRes_On;
                                break;
                            }
                            else if ((d < 0) == IsAbove)
                                val = 1 - val;
                        }

                        IsAbove = !IsAbove;
                        ++Cur;
                    }
                }
            }

            if((IsAbove != StartingAbove) && (Result != PointInPolyRes_On))
            {
                End = Poly->Points + (Poly->Count - 1);
                if(PointsAreEqual(*Cur, *End))
                    Cur = Start;
                if(PointsAreEqual(*Cur, *Start))
                    Prev = End - 1;
                else
                    Prev = Cur - 1;

                f64 d = Cross(*Prev, *Cur, p);
                if(d == 0)
                    Result = PointInPolyRes_On;
                else if ((d < 0) == IsAbove)
                    val = 1 - val;
            }
        }
    }

    if(Result != PointInPolyRes_On)
        Result = (val == 0) ? PointInPolyRes_Outside : PointInPolyRes_Inside;

    return(Result);
}

#define CLIPPER_MATH_S64_H
#endif

#if !defined(CLIPPER_CORE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */
#ifndef PI
  global_variable f64 PI = 3.141592653589793238;
#endif

#ifdef CLIPPER_MAX_DECIMAL_PRECISION
  global_variable s32 CLIPPER_MAX_DEC_PRECISION = CLIPPER_MAX_DECIMAL_PRECISION;
#else
  global_variable s32 CLIPPER_MAX_DEC_PRECISION = 8; // see Discussions #564
#endif

#define MAX_COORD (INT64_MAX >> 2)
#define MIN_COORD -MAX_COORD
#define MAX_COORD_F64 (double)(MAX_COORD)
#define MIN_COORD_F64 (double)(MIN_COORD)

#define F64Max MAX_DBL
#define F64Min MIN_DBL

#define InvalidPointS64 PointS64(INT64_MAX, INT64_MAX)
#define InvalidPointF64 PointF64(DBL_MAX, DBL_MAX)

#define InvalidPointRectF64 InvertedInfinityRectF64()

#define RECORD_MEMORY_USEAGE 0

#if RECORD_MEMORY_USEAGE
global_variable u64 MemoryAllocated = 0;
global_variable u64 PathMemoryUsed = 0;

enum array_type
{
    ArrayType_OutRec,
    ArrayType_MinimaList,
    ArrayType_HorzSegList,
    ArrayType_HorzJoinList,
    ArrayType_IntersectNode,
    ArrayType_VertexLists,
    ArrayType_DeallocateActivesList,
    ArrayType_DeallocateOutPtsList,

    ArrayType_Count,
};

global_variable u32 ArrayMaxSizes[ArrayType_Count] = {};
#endif

#define BASIC_ALLOCATE_COUNT 16

// NOTE(pvlso): Clipper's memory comes from ClipperArena, set around each operation (ClipPolygons); nothing is freed one
// by one, the caller's temporary memory takes it all back
global_variable memory_arena *ClipperArena;

#define ClipperZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
#define ClipperZeroArray(Count, Pointer) ZeroSize((Count)*sizeof((Pointer)[0]), Pointer)
#define ClipperPushStruct(type) (type *)ClipperMalloc(sizeof(type))
#define ClipperPushArray(Count, type) (type *)ClipperMalloc((Count)*sizeof(type))
#define ClipperPushSize(Size) ClipperMalloc(Size)

inline void *
ClipperMalloc(umm Size)
{
    Assert(ClipperArena);
    void *Result = PushSize(ClipperArena, Size, AlignNoClear(32)); // NOTE(pvlso): for its AVX loads
    ZeroSize(Size, Result);
    return(Result);
}

inline void
ClipperFreeBlock(void *Ptr)
{
}

inline void *
Realloc(void *Base, umm Size, umm NewSize)
{
    void *Result = ClipperMalloc(NewSize);
    if(Base)
    {
        Copy(Minimum(Size, NewSize), Base, Result);
    }
    return(Result);
}

#define ReallocArray(Base, OldCount, NewCount, type) (type *)Realloc(Base, (OldCount)*sizeof(type), (NewCount)*sizeof(type))
#define MallocArray(Count, type) (type *)ClipperMalloc(sizeof(type)*(Count));
#define MallocStruct(type) (type *)ClipperMalloc(sizeof(type))

inline void
Free(void *Ptr, umm Size)
{
}

inline b32
NeedIncrease(u32 Count)
{
    b32 Result = ((Count % BASIC_ALLOCATE_COUNT) == 0) && (Count != 0);
    return(Result);
}
// ===========================================================================================================================================================

inline path_f64
GetPathF64(s32 Count)
{
    path_f64 Result = {};
    Result.AllocateCount = Count;
    Result.Points = MallocArray(Count, v2_f64);

#if RECORD_MEMORY_USEAGE
    PathMemoryUsed += Count*sizeof(v2_f64);
#endif
    
    return(Result);
}

inline paths_f64
GetPathsF64(s32 Count)
{
    paths_f64 Result = {};
    Result.AllocateCount = Count;
    Result.Paths = MallocArray(Count, path_f64);
    
#if RECORD_MEMORY_USEAGE
    PathMemoryUsed += Count*sizeof(path_f64);
#endif

    return(Result);
}

inline paths_f64
GetAndCopyPathsF64(paths_f64 *Old, s32 Count)
{
    paths_f64 Result = {};
    Result.AllocateCount = Count;
    Result.Paths = MallocArray(Count, path_f64);

    Result.PathCount = Old->PathCount;
    Copy(Old->PathCount*sizeof(path_f64), Old->Paths, Result.Paths);
    
#if RECORD_MEMORY_USEAGE
    PathMemoryUsed += Count*sizeof(path_f64);
#endif

    return(Result);
}

inline path_s64
GetPathS64(s32 Count)
{
    path_s64 Result = {};
    Result.AllocateCount = Count;
    Result.Points = MallocArray(Count, v2_s64);

#if RECORD_MEMORY_USEAGE
    PathMemoryUsed += Count*sizeof(v2_s64);
#endif
    
    return(Result);
}

inline paths_s64
GetPathsS64(s32 Count)
{
    paths_s64 Result = {};
    Result.AllocateCount = Count;
    Result.Paths = MallocArray(Count, path_s64);

#if RECORD_MEMORY_USEAGE
    PathMemoryUsed += Count*sizeof(path_s64);
#endif
    
    return(Result);
}

inline void
IncreasePathF64(path_f64 *Path)
{
    Path->Points =
        ReallocArray(Path->Points, Path->Count,
                     Path->Count + BASIC_ALLOCATE_COUNT, v2_f64);
    Path->AllocateCount = Path->Count + BASIC_ALLOCATE_COUNT;

#if RECORD_MEMORY_USEAGE
    PathMemoryUsed += BASIC_ALLOCATE_COUNT*sizeof(v2_f64);
#endif
}

inline void
IncreasePathS64(path_s64 *Path)
{
    Path->Points =
        ReallocArray(Path->Points, Path->Count,
                     Path->Count + BASIC_ALLOCATE_COUNT, v2_s64);
    Path->AllocateCount = Path->Count + BASIC_ALLOCATE_COUNT;

#if RECORD_MEMORY_USEAGE
    PathMemoryUsed += BASIC_ALLOCATE_COUNT*sizeof(v2_f64);
#endif
}

inline void
CheckPrecisionRange(s32 *Precision)
{
    if(!((*Precision >= -CLIPPER_MAX_DEC_PRECISION) &&
         (*Precision <= CLIPPER_MAX_DEC_PRECISION)))
    {
        *Precision = (*Precision > 0) ? CLIPPER_MAX_DEC_PRECISION : -CLIPPER_MAX_DEC_PRECISION;
    }
}

inline path_f64
RectAsPath(rect_f64 Rect)
{
    path_f64 Result = GetPathF64(4);
    Result.Points[0] = V2F64(Rect.Min.x, Rect.Max.y);
    Result.Points[1] = Rect.Max;
    Result.Points[2] = V2F64(Rect.Max.x, Rect.Min.y);
    Result.Points[3] = Rect.Min;

    return(Result);
}

inline path_f64
ScalePath(path_f64 *Source, v2_f64 Scale)
{
    path_f64 Result = GetPathF64(Source->Count);
    Result.Count = Source->Count;
    if((Scale.x == 0) || (Scale.y == 0))
    {
        if(Scale.x == 0)
            Scale.x = 1.0;
        if(Scale.y == 0)
            Scale.y = 1.0;
    }
    
    for(s32 I = 0;
        I < Source->Count;
        ++I)
    {
        Result.Points[I] = Scale*Source->Points[I];
    }

    return(Result);
}

inline void
ScaleCurrentPath(path_f64 *Source, v2_f64 Scale)
{
    if((Scale.x == 0) || (Scale.y == 0))
    {
        if(Scale.x == 0)
            Scale.x = 1.0;
        if(Scale.y == 0)
            Scale.y = 1.0;
    }
    
    for(s32 I = 0;
        I < Source->Count;
        ++I)
    {
        Source->Points[I] = Scale*Source->Points[I];
    }
}

inline path_f64
ScaleConvertPathToF64(path_s64 *Source, v2_f64 Scale)
{
    path_f64 Result = GetPathF64(Source->Count);
    Result.Count = Source->Count;
    if((Scale.x == 0) || (Scale.y == 0))
    {
        if(Scale.x == 0)
            Scale.x = 1.0;
        if(Scale.y == 0)
            Scale.y = 1.0;
    }

#if 0
    // TODO: NOTE(babykaban): Not used right now, has to  be tested
    // avaliable only with AVX+ instruction sets
    v2_s64 *SourcePtr = Source->Points;
    v2_f64 *DestPtr = Result.Points;
    b32 OneLeft = Source->Count % 2;
    u32 LoopCount = OneLeft ? (Source->Count - 1) : Source->Count;
    for(u32 I = 0;
        I < LoopCount;
        I += 2)
    {
        __m256i TwoP = _mm256_load_si256((__m256i *)SourcePtr);
        __m256d Scale_2x = _mm256_set_m128d(Scale.W, Scale.W);
        __m256d TwoP_f64 = _mm256_mul_pd(_mm256_castsi256_pd(TwoP), Scale_2x);

        _mm256_store_pd((f64 *)DestPtr, TwoP_f64);

        SourcePtr += 2;
        DestPtr += 2;
    }

    if(OneLeft)
    {
        v2_s64 Point = *SourcePtr;
        __m128d Point_f64 = _mm_mul_pd(_mm_castsi128_pd(Point.W), Scale.W);
        _mm_store_pd((f64 *)DestPtr, Point_f64);
    }
#else    
    for(s32 I = 0;
        I < Source->Count;
        ++I)
    {
        Result.Points[I] = Scale*V2F64(Source->Points[I]);
    }
#endif
    return(Result);
}

inline path_s64
ScaleConvertPathToS64(path_f64 *Source, v2_f64 Scale)
{
    path_s64 Result = GetPathS64(Source->Count);
    Result.Count = Source->Count;
    if((Scale.x == 0) || (Scale.y == 0))
    {
        if(Scale.x == 0)
            Scale.x = 1.0;
        if(Scale.y == 0)
            Scale.y = 1.0;
    }

#if CLIPPER_AVX512
    // TODO: NOTE(babykaban): Only avaliable with AVX+ set of instructions
    // 272.57
    v2_f64 *SourcePtr = Source->Points;
    v2_s64 *DestPtr = Result.Points;
    b32 OneLeft = Source->Count % 2;
    u32 LoopCount = OneLeft ? (Source->Count - 1) : Source->Count;
    for(u32 I = 0;
        I < LoopCount;
        I += 2)
    {
        __m256d TwoP = _mm256_load_pd((f64 *)SourcePtr);
        __m256d Scale_2x = _mm256_set_m128d(Scale.W, Scale.W);
        TwoP = _mm256_mul_pd(TwoP, Scale_2x);

        __m128i FirstP_s64 = _mm_cvtpd_epi64(_mm_round_pd(_mm256_extractf128_pd(TwoP, 0),
                                                          _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
        __m128i SecondP_s64 = _mm_cvtpd_epi64(_mm_round_pd(_mm256_extractf128_pd(TwoP, 1),
                                                           _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));

        _mm_store_si128((__m128i *)DestPtr, FirstP_s64);
        _mm_store_si128((__m128i *)((s64 *)DestPtr + 2), SecondP_s64);

        SourcePtr += 2;
        DestPtr += 2;
    }

    if(OneLeft)
    {
        v2_f64 Point = *SourcePtr;
        Point.W = _mm_mul_pd(Point.W, Scale.W);
        __m128i Point_s64 = _mm_cvtpd_epi64(
            _mm_round_pd(Point.W, _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
        _mm_store_si128((__m128i *)DestPtr, Point_s64);
    }
#else
    for(s32 I = 0;
        I < Source->Count;
        ++I)
    {
        Result.Points[I] = V2S64(Scale*Source->Points[I]);
    }
#endif    

    return(Result);
}

inline path_f64
ScalePath(path_f64 *Source, f64 Scale)
{
    path_f64 Result = ScalePath(Source, V2F64(Scale, Scale));
    return(Result);
}

inline void
ScaleCurrentPath(path_f64 *Source, f64 Scale)
{
    ScaleCurrentPath(Source, V2F64(Scale, Scale));
}

inline path_s64
ScaleConvertPathToS64(path_f64 *Source, f64 Scale)
{
    path_s64 Result = ScaleConvertPathToS64(Source, V2F64(Scale, Scale));
    return(Result);
}

inline path_f64
ScaleConvertPathToF64(path_s64 *Source, f64 Scale)
{
    path_f64 Result = ScaleConvertPathToF64(Source, V2F64(Scale, Scale));
    return(Result);
}

inline paths_f64
ScalePaths(paths_f64 *Source, f64 ScaleX, f64 ScaleY)
{
    paths_f64 Result = GetPathsF64(Source->PathCount);
    Result.PathCount = Source->PathCount;
    
    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        Result.Paths[I] = ScalePath(Source->Paths + I, V2F64(ScaleX, ScaleY));
    }

    return(Result);
}

inline void
ScaleCurrentPaths(paths_f64 *Source, f64 ScaleX, f64 ScaleY)
{
    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        ScaleCurrentPath(Source->Paths + I, V2F64(ScaleX, ScaleY));
    }
}

inline paths_f64
ScaleConvertPathsToF64(paths_s64 *Source, f64 ScaleX, f64 ScaleY)
{
    paths_f64 Result = GetPathsF64(Source->PathCount);
    Result.PathCount = Source->PathCount;
    
    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        Result.Paths[I] = ScaleConvertPathToF64(Source->Paths + I, V2F64(ScaleX, ScaleY));
    }

    return(Result);
}

inline paths_s64
ScaleConvertPathsToS64(paths_f64 *Source, f64 ScaleX, f64 ScaleY)
{
    paths_s64 Result = GetPathsS64(Source->PathCount);
    Result.PathCount = Source->PathCount;
    
    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        Result.Paths[I] = ScaleConvertPathToS64(Source->Paths + I, V2F64(ScaleX, ScaleY));
    }

    return(Result);
}

inline paths_f64
ScalePaths(paths_f64 *Source, f64 Scale)
{
    paths_f64 Result = ScalePaths(Source, Scale, Scale);
    return(Result);
}

inline void
ScaleCurrentPaths(paths_f64 *Source, f64 Scale)
{
    ScaleCurrentPaths(Source, Scale, Scale);
}

inline paths_s64
ScaleConvertPathsToS64(paths_f64 *Source, f64 Scale)
{
    paths_s64 Result = ScaleConvertPathsToS64(Source, Scale, Scale);
    return(Result);
}

inline paths_f64
ScaleConvertPathsToS64(paths_s64 *Source, f64 Scale)
{
    paths_f64 Result = ScaleConvertPathsToF64(Source, Scale, Scale);
    return(Result);
}

inline path_s64
ConvertF64PathToS64(path_f64 *Source)
{
    path_s64 Result = GetPathS64(Source->Count);
    Result.Count = Source->Count;
    for(s32 I = 0;
        I < Source->Count;
        ++I)
    {
        Result.Points[I] = V2S64(Source->Points[I]);
    }

    return(Result);
}

inline paths_s64
ConvertF64PathsToS64(paths_f64 *Source)
{
    paths_s64 Result = GetPathsS64(Source->PathCount);
    Result.PathCount = Source->PathCount;

    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        Result.Paths[I] = ConvertF64PathToS64(Source->Paths + I);
    }

    return(Result);
}

inline path_f64
StripNearEqual(path_f64 *Path, f64 MaxDistSq, b32 IsClosedPath)
{
    path_f64 Result = {};
    if(Path->Count > 0)
    {
        Result = GetPathF64(Path->Count);

        v2_f64 *PathIter = Path->Points;
        v2_f64 *PathEnd = Path->Points + (Path->Count - 1);

        v2_f64 FirstP = *PathIter++;
        v2_f64 LastP = FirstP;
        Result.Points[Result.Count++] = *PathIter;

        for(; !PointsAreEqual(*PathIter, *PathEnd); ++PathIter)
        {
            if(!NearEqual(*PathIter, LastP, MaxDistSq))
            {
                LastP = *PathIter;
                Result.Points[Result.Count++] = LastP;
            }
        }

        if(IsClosedPath)
        {
            while((Result.Count > 1) &&
                  NearEqual(Result.Points[Result.Count - 1], FirstP, MaxDistSq))
            {
                Result.Points[--Result.Count] = {};
            }

            
        }
    }

    return(Result);
}

inline paths_f64
StripNearEqual(paths_f64 *Source, f64 MaxDistSq, b32 IsClosedPath)
{
    paths_f64 Result = GetPathsF64(Source->PathCount);

    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        Result.Paths[I] = StripNearEqual(Source->Paths + I, MaxDistSq, IsClosedPath);
    }

    return(Result);
}

inline void
StripDuplicates(path_f64 *Path, b32 IsClosedPath)
{
    for(s32 I = 0;
        I < (Path->Count - 1);
        ++I)
    {
        if(PointsAreEqual(Path->Points[I], Path->Points[I + 1]))
        {
            for(s32 J = I;
                J < (Path->Count - 1);
                ++J)
            {
                Path->Points[J] = Path->Points[J + 1];
            }

            --Path->Count;
            --I;
        }
    }

    if(IsClosedPath)
    {
        while((Path->Count > 1) &&
              PointsAreEqual(Path->Points[Path->Count - 1], Path->Points[0]))
        {
            Path->Points[--Path->Count] = {};
        }
    }

    
}

inline void
StripDuplicates(paths_f64 *Source, b32 IsClosedPath)
{
    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        StripDuplicates(Source->Paths + I, IsClosedPath);
    }
}

inline path_s64
RectAsPath(rect_s64 Rect)
{
    path_s64 Result = GetPathS64(4);
    Result.Points[0] = V2S64(Rect.Min.x, Rect.Max.y);
    Result.Points[1] = Rect.Max;
    Result.Points[2] = V2S64(Rect.Max.x, Rect.Min.y);
    Result.Points[3] = Rect.Min;

    return(Result);
}

inline path_f64
ConvertS64PathToF64(path_s64 *Source)
{
    path_f64 Result = GetPathF64(Source->Count);
    Result.Count = Source->Count;
    for(s32 I = 0;
        I < Source->Count;
        ++I)
    {
        Result.Points[I] = V2F64(Source->Points[I]);
    }

    return(Result);
}

inline paths_f64
ConvertS64PathsToF64(paths_s64 *Source)
{
    paths_f64 Result = GetPathsF64(Source->PathCount);
    Result.PathCount = Source->PathCount;

    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        Result.Paths[I] = ConvertS64PathToF64(Source->Paths + I);
    }

    return(Result);
}

inline path_s64
StripNearEqual(path_s64 *Path, f64 MaxDistSq, b32 IsClosedPath)
{
    path_s64 Result = {};
    if(Path->Count > 0)
    {
        Result = GetPathS64(Path->Count);

        v2_s64 *PathIter = Path->Points;
        v2_s64 *PathEnd = Path->Points + (Path->Count - 1);

        v2_s64 FirstP = *PathIter++;
        v2_s64 LastP = FirstP;
        Result.Points[Result.Count++] = *PathIter;

        for(; !PointsAreEqual(*PathIter, *PathEnd); ++PathIter)
        {
            if(!NearEqual(*PathIter, LastP, MaxDistSq))
            {
                LastP = *PathIter;
                Result.Points[Result.Count++] = LastP;
            }
        }

        if(IsClosedPath)
        {
            while((Result.Count > 1) &&
                  NearEqual(Result.Points[Result.Count - 1], FirstP, MaxDistSq))
            {
                Result.Points[--Result.Count] = {};
            }

            
        }
    }

    return(Result);
}

inline paths_s64
StripNearEqual(paths_s64 *Source, f64 MaxDistSq, b32 IsClosedPath)
{
    paths_s64 Result = GetPathsS64(Source->PathCount);

    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        Result.Paths[I] = StripNearEqual(Source->Paths + I, MaxDistSq, IsClosedPath);
    }

    return(Result);
}

inline void
StripDuplicates(path_s64 *Path, b32 IsClosedPath)
{
    for(s32 I = 0;
        I < (Path->Count - 1);
        ++I)
    {
        if(PointsAreEqual(Path->Points[I], Path->Points[I + 1]))
        {
            for(s32 J = I;
                J < (Path->Count - 1);
                ++J)
            {
                Path->Points[J] = Path->Points[J + 1];
            }

            --Path->Count;
            --I;
        }
    }

    if(IsClosedPath)
    {
        while((Path->Count > 1) &&
              PointsAreEqual(Path->Points[Path->Count - 1], Path->Points[0]))
        {
            Path->Points[--Path->Count] = {};
        }
    }

    
}

inline void
StripDuplicates(paths_s64 *Source, b32 IsClosedPath)
{
    for(s32 I = 0;
        I < Source->PathCount;
        ++I)
    {
        StripDuplicates(Source->Paths + I, IsClosedPath);
    }
}

enum heap_type
{
    HeapType_S32,
    HeapType_S64,
    HeapType_U32,
    HeapType_U64,
};

struct clipper_sort_entry
{
    union
    {
        s32 Value_S32;
        s64 Value_S64;
        u32 Value_U32;
        u64 Value_U64;
    };
};

struct clipper_heap
{
    heap_type Type;

    clipper_sort_entry *Nodes;
    u32 MaxSize;
    u32 Size;

    void *Free;
};

inline void
InitHeap(clipper_heap *Heap, u32 MaxSize, heap_type Type)
{
    Heap->Type = Type;
    Heap->MaxSize = MaxSize;
    Heap->Size = 0;
    Heap->Nodes = MallocArray(Heap->MaxSize, clipper_sort_entry);
    Heap->Free = Heap->Nodes;
}


inline void
Swap(clipper_sort_entry *A, clipper_sort_entry *B)
{
    clipper_sort_entry Temp = *B;
    *B = *A;
    *A = Temp;
}

inline b32
HeapifyDownComp(heap_type HeapType, clipper_sort_entry A, clipper_sort_entry B, b32 IsMaxHeap)
{
    b32 Result = false;
    switch(HeapType)
    {
        case HeapType_S32: {Result = IsMaxHeap ? (A.Value_S32 > B.Value_S32) : (A.Value_S32 < B.Value_S32);} break;
        case HeapType_S64: {Result = IsMaxHeap ? (A.Value_S64 > B.Value_S64) : (A.Value_S64 < B.Value_S64);} break;
        case HeapType_U32: {Result = IsMaxHeap ? (A.Value_U32 > B.Value_U32) : (A.Value_U32 < B.Value_U32);} break;
        case HeapType_U64: {Result = IsMaxHeap ? (A.Value_U64 > B.Value_U64) : (A.Value_U64 < B.Value_U64);} break;
            InvalidDefaultCase;
    }

    return(Result);
}

inline b32
HeapifyUpComp(heap_type HeapType, clipper_sort_entry A, clipper_sort_entry B, b32 IsMaxHeap)
{
    b32 Result = false;
    switch(HeapType)
    {
        case HeapType_S32: {Result = IsMaxHeap ? (A.Value_S32 < B.Value_S32) : (A.Value_S32 > B.Value_S32);} break;
        case HeapType_S64: {Result = IsMaxHeap ? (A.Value_S64 < B.Value_S64) : (A.Value_S64 > B.Value_S64);} break;
        case HeapType_U32: {Result = IsMaxHeap ? (A.Value_U32 < B.Value_U32) : (A.Value_U32 > B.Value_U32);} break;
        case HeapType_U64: {Result = IsMaxHeap ? (A.Value_U64 < B.Value_U64) : (A.Value_U64 > B.Value_U64);} break;
            InvalidDefaultCase;
    }

    return(Result);
}

inline void
MinHeapifyDown(clipper_heap *Heap, u32 Index)
{
    u32 Smallest = Index;
    u32 Left = 2*Index + 1;
    u32 Right = 2*Index + 2;

    if((Left < Heap->Size) &&
       HeapifyDownComp(Heap->Type, Heap->Nodes[Left], Heap->Nodes[Smallest], false))
    {
        Smallest = Left;
    }

    if((Right < Heap->Size) &&
       HeapifyDownComp(Heap->Type, Heap->Nodes[Right], Heap->Nodes[Smallest], false))
    {
        Smallest = Right;
    }

    if(Smallest != Index)
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Smallest);
        MinHeapifyDown(Heap, Smallest);
    }
}

inline void
MaxHeapifyDown(clipper_heap *Heap, u32 Index)
{
    u32 Largest = Index;
    u32 Left = 2*Index + 1;
    u32 Right = 2*Index + 2;

    if((Left < Heap->Size) &&
       HeapifyDownComp(Heap->Type, Heap->Nodes[Left], Heap->Nodes[Largest], true))
    {
        Largest = Left;
    }

    if((Right < Heap->Size) &&
       HeapifyDownComp(Heap->Type, Heap->Nodes[Right], Heap->Nodes[Largest], true))
    {
        Largest = Right;
    }

    if(Largest != Index)
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Largest);
        MaxHeapifyDown(Heap, Largest);
    }
}

inline void
MinHeapifyUp(clipper_heap *Heap, u32 Index)
{
    u32 Parent = (Index - 1) / 2;

    if(Index &&
       HeapifyUpComp(Heap->Type, Heap->Nodes[Parent], Heap->Nodes[Index], false))
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Parent);
        MinHeapifyUp(Heap, Parent);
    }
}

inline void
MaxHeapifyUp(clipper_heap *Heap, u32 Index)
{
    u32 Parent = (Index - 1) / 2;

    if(Index &&
       HeapifyUpComp(Heap->Type, Heap->Nodes[Parent], Heap->Nodes[Index], true))
    {
        Swap(Heap->Nodes + Index, Heap->Nodes + Parent);
        MaxHeapifyUp(Heap, Parent);
    }
}

inline void
IncreaseHeapSize(clipper_heap *Heap)
{
    Heap->Nodes = ReallocArray(Heap->Nodes, Heap->MaxSize,
                               Heap->MaxSize + BASIC_ALLOCATE_COUNT, clipper_sort_entry);
    Heap->MaxSize = Heap->MaxSize + BASIC_ALLOCATE_COUNT;
}
 
inline void
MinHeapInsertNode(clipper_heap *Heap, clipper_sort_entry Key)
{
    if(Heap->Size == Heap->MaxSize)
    {
        IncreaseHeapSize(Heap);
    }

    Assert(Heap->Size != Heap->MaxSize);

    Heap->Nodes[Heap->Size] = Key;
    ++Heap->Size;
    MinHeapifyUp(Heap, Heap->Size - 1);
}

inline void
MaxHeapInsertNode(clipper_heap *Heap, clipper_sort_entry Key)
{
    if(Heap->Size == Heap->MaxSize)
    {
        IncreaseHeapSize(Heap);
    }

    Assert(Heap->Size != Heap->MaxSize);

    Heap->Nodes[Heap->Size] = Key;
    ++Heap->Size;
    MaxHeapifyUp(Heap, Heap->Size - 1);
}

inline clipper_sort_entry
MinHeapExtractNode(clipper_heap *Heap)
{
    Assert(Heap->Size > 0);

    clipper_sort_entry Result = {};
    Result = Heap->Nodes[0];
    Heap->Nodes[0] = Heap->Nodes[Heap->Size - 1];
    --Heap->Size;

    MinHeapifyDown(Heap, 0);

    return(Result);
}

inline clipper_sort_entry
MaxHeapExtractNode(clipper_heap *Heap)
{
    Assert(Heap->Size > 0);

    clipper_sort_entry Result = {};
    Result = Heap->Nodes[0];
    Heap->Nodes[0] = Heap->Nodes[Heap->Size - 1];
    --Heap->Size;

    MaxHeapifyDown(Heap, 0);

    return(Result);
}

#define CLIPPER_CORE_H
#endif

#if !defined(CLIPPER_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */


// NOTE(pvlso): Clipper2's fmin/fmax/nearbyint (https://github.com/AngusJohnson/Clipper2/discussions/334), as functions
// so they don't take over the CRT's names in the unity build
inline f64 ClipperFMin(f64 A, f64 B) {return(_mm_cvtsd_f64(_mm_min_sd(_mm_set_sd(A), _mm_set_sd(B))));}
inline f64 ClipperFMax(f64 A, f64 B) {return(_mm_cvtsd_f64(_mm_max_sd(_mm_set_sd(A), _mm_set_sd(B))));}
inline s64 ClipperNearbyInt(f64 A) {return(_mm_cvtsd_si64(_mm_set_sd(A)));}
enum clip_type
{
    ClipType_NoClip,

    ClipType_Intersection,
    ClipType_Union,
    ClipType_Difference,
    ClipType_Xor,

    ClipType_Count,
};

enum fill_rule
{
    FillRule_EvenOdd,
    FillRule_NonZero,
    FillRule_Positive,
    FillRule_Negative,

    FillRule_Count,
};

enum path_type
{
    PathType_Subject,
    PathType_Clip,
};

enum join_with
{
    JoinWith_NoJoin,
    JoinWith_Left,
    JoinWith_Right,
};

enum vertex_flag
{
    VertexFlag_Empty = 0,
    VertexFlag_OpenStart = (1 << 0),
    VertexFlag_OpenEnd = (1 << 1),
    VertexFlag_LocalMax = (1 << 2),
    VertexFlag_LocalMin = (1 << 3),
};

inline void
AddVertexFlag(u32 *Flags, u32 Flag)
{
    *Flags = (*Flags) | Flag;
}

inline void
ClearVertexFlag(u32 *Flags, u32 Flag)
{
    *Flags = (*Flags) & ~Flag;
}

inline b32
IsVertexFlagSet(u32 Flags, u32 Flag)
{
    b32 Result = Flags & Flag;

    return(Result);
}

struct clipper_vertex
{
    v2_s64 P;
    clipper_vertex *Next;
    clipper_vertex *Prev;

    u32 Flags;
};

struct output_rectangle;
struct horz_segment;
struct output_point
{
    v2_s64 P;
    output_point *Next;
    output_point *Prev;

    u32 OutRectIndex;

    horz_segment *Horz;
};

struct horz_segment
{
    output_point *left_op;
    output_point *right_op;
    b32 left_to_right;
};

struct horz_join
{
    output_point *op1;
    output_point *op2;
};

struct local_minima
{
    clipper_vertex *Vertex;
    path_type PolyType;
    b32 IsOpen;
};

struct clipper_active
{
    v2_s64 bot;
    v2_s64 top;
    s64 curr_x;     //current (updated at every new scanline)
    f64 dx;
    s32 wind_dx;            //1 or -1 depending on winding direction
    s32 wind_cnt;
    s32 wind_cnt2;      //winding count of the opposite polytype

    u32 outrecIndex;
    //AEL: 'clipper_active edge list' (Vatti's AET - clipper_active edge table)
    //     a linked list of all edges (from left to right) that are present
    //     (or 'clipper_active') within the current scanbeam (a horizontal 'beam' that
    //     sweeps from bottom to top over the paths in the clipping operation).
    clipper_active *prev_in_ael;
    clipper_active *next_in_ael;
    //SEL: 'sorted edge list' (Vatti's ST - sorted table)
    //     linked list used when sorting edges into their new positions at the
    //     top of scanbeams, but also (re)used to process horizontals.
    clipper_active *prev_in_sel;
    clipper_active *next_in_sel;
    clipper_active *jump;
    clipper_vertex *vertex_top;
    local_minima *local_min;  // the bottom of an edge 'bound' (also Vatti)
    b32 IsLeftBound;
    join_with JoinWith;
};

struct output_rectangle
{
    u32 Index;
    u32 OwnerIndex;

    clipper_active *FrontEdge;
    clipper_active *BackEdge;
    output_point *Points;
    u32 SplitCount;
    output_rectangle *Splits;
    output_rectangle *RecursiveSplit;

    rect_s64 Bounds;
    path_s64 Path;
    b32 IsOpen;
};

struct intersect_node
{
    v2_s64 pt;
    clipper_active *edge1;
    clipper_active *edge2;
};

struct vertex_list
{
    u32 VertexCount;
    clipper_vertex *Vertices;
};

struct clipper
{
    f64 Scale;
    f64 InvScale;

    u32 VertexListCount;
    vertex_list *VertexLists;

    u32 MinimaListCount;
    local_minima *MinimaList;       //pointers in case of memory reallocs
    b32 MinimaListSorted;

    clip_type ClipType;
    fill_rule FillRule;

    local_minima *CurrentLocalMinima;
    clipper_heap ScanLineMaxHeap;
    clipper_active *ActiveEdgeList;
    clipper_active *StoredEdgeList;
    s64 BottomY;

    u32 OutputRectCount;
    output_rectangle *OutRecList; //pointers in case list memory reallocated

    u32 HorzCount;
    horz_segment *HorzSegList;

    u32 JointCount;
    horz_join *HorzJoinList;
    
    u32 IntersectNodeCount;
    intersect_node *IntersectNodes;

    b32 PreserveCollinear;
    b32 ReverseSolution;
    b32 HasOpenPaths;
    b32 Succeeded;

    clipper_active *ActivesFreeList;
    u32 DeallocActivesCount;
    void **DeallocActivesList;

    output_point *OutPtFreeList;
    u32 DeallocOutPtsCount;
    void **DeallocOutPtsList;

};

inline void
IncreaseOutRecList(clipper *Clipper)
{
    Clipper->OutRecList =
        ReallocArray(Clipper->OutRecList, Clipper->OutputRectCount,
                     Clipper->OutputRectCount + BASIC_ALLOCATE_COUNT, output_rectangle);

#if RECORD_MEMORY_USEAGE
    ArrayMaxSizes[ArrayType_OutRec] = Clipper->OutputRectCount + BASIC_ALLOCATE_COUNT;
#endif
}

inline void
IncreaseHorzSegList(clipper *Clipper)
{
    Clipper->HorzSegList =
        ReallocArray(Clipper->HorzSegList, Clipper->HorzCount,
                     Clipper->HorzCount + BASIC_ALLOCATE_COUNT, horz_segment);

#if RECORD_MEMORY_USEAGE
    ArrayMaxSizes[ArrayType_HorzSegList] = Clipper->HorzCount + BASIC_ALLOCATE_COUNT;
#endif
}

inline void
IncreaseHorzJoinList(clipper *Clipper)
{
    Clipper->HorzJoinList =
        ReallocArray(Clipper->HorzJoinList, Clipper->JointCount,
                     Clipper->JointCount + BASIC_ALLOCATE_COUNT, horz_join);

#if RECORD_MEMORY_USEAGE
    ArrayMaxSizes[ArrayType_HorzJoinList] = Clipper->JointCount + BASIC_ALLOCATE_COUNT;
#endif
}

inline void
IncreaseIntersectNodes(clipper *Clipper)
{
    Clipper->IntersectNodes =
        ReallocArray(Clipper->IntersectNodes, Clipper->IntersectNodeCount,
                     Clipper->IntersectNodeCount + BASIC_ALLOCATE_COUNT, intersect_node);

#if RECORD_MEMORY_USEAGE
    ArrayMaxSizes[ArrayType_IntersectNode] = Clipper->IntersectNodeCount + BASIC_ALLOCATE_COUNT;
#endif
}

inline void
IncreaseMinimaList(clipper *Clipper)
{
    Clipper->MinimaList =
        ReallocArray(Clipper->MinimaList, Clipper->MinimaListCount,
                     Clipper->MinimaListCount + BASIC_ALLOCATE_COUNT, local_minima);

#if RECORD_MEMORY_USEAGE
    ArrayMaxSizes[ArrayType_MinimaList] = Clipper->MinimaListCount + BASIC_ALLOCATE_COUNT;
#endif
}

inline void
IncreaseVertexLists(clipper *Clipper)
{
    Clipper->VertexLists =
        ReallocArray(Clipper->VertexLists, Clipper->VertexListCount,
                     Clipper->VertexListCount + BASIC_ALLOCATE_COUNT, vertex_list);

#if RECORD_MEMORY_USEAGE
    ArrayMaxSizes[ArrayType_VertexLists] = Clipper->VertexListCount + BASIC_ALLOCATE_COUNT;
#endif
}

inline void
IncreaseDeallocateActivesList(clipper *Clipper)
{
    Clipper->DeallocActivesList =
        ReallocArray(Clipper->DeallocActivesList, Clipper->DeallocActivesCount,
                     Clipper->DeallocActivesCount + BASIC_ALLOCATE_COUNT, void *);

#if RECORD_MEMORY_USEAGE
    ArrayMaxSizes[ArrayType_DeallocateActivesList] =
        Clipper->DeallocActivesCount + BASIC_ALLOCATE_COUNT;
#endif
}

inline void
IncreaseDeallocateOutPtsList(clipper *Clipper)
{
    Clipper->DeallocOutPtsList =
        ReallocArray(Clipper->DeallocOutPtsList, Clipper->DeallocOutPtsCount,
                     Clipper->DeallocOutPtsCount + BASIC_ALLOCATE_COUNT, void *);

#if RECORD_MEMORY_USEAGE
    ArrayMaxSizes[ArrayType_DeallocateOutPtsList] =
        Clipper->DeallocOutPtsCount + BASIC_ALLOCATE_COUNT;
#endif
}

inline horz_join
HorzJoin(output_point *op1, output_point *op2)
{
    horz_join Result = {};
    Result.op1 = op1;
    Result.op2 = op2;

    return(Result);
}

inline horz_segment *
GetHorzSegment(output_point *Point)
{
    horz_segment *Result = MallocStruct(horz_segment);
    Result->left_to_right = true;
    Result->left_op = Point;

    return(Result);
}

inline void
InitLocalMinima(local_minima *Minima, clipper_vertex *Vertex, path_type Type, b32 Open)
{
    Minima->Vertex = Vertex;
    Minima->PolyType = Type;
    Minima->IsOpen = Open;
}

inline void
InitActive(clipper_active *Active)
{
    ClipperZeroStruct(*Active);
    Active->wind_dx = 1;
}

inline void
FreeActive(clipper *Clipper, clipper_active *e)
{
    ClipperZeroStruct(*e);

    clipper_active *Free = Clipper->ActivesFreeList;
    Clipper->ActivesFreeList = e;
    e->next_in_ael = Free;
}

inline clipper_active *
GetNewActive(clipper *Clipper)
{
    clipper_active *Result = 0;

    if(Clipper->ActivesFreeList)
    {
        Result = Clipper->ActivesFreeList;
        Clipper->ActivesFreeList = Result->next_in_ael;
        Result->next_in_ael = 0;
    }
    else
    {
        clipper_active *Mem = (clipper_active *)ClipperMalloc(sizeof(clipper_active)*BASIC_ALLOCATE_COUNT);
        // NOTE(pvlso): The list of blocks grows like the other lists (it stopped at 16 blocks before)
        if(NeedIncrease(Clipper->DeallocActivesCount)) {IncreaseDeallocateActivesList(Clipper);}
        Clipper->DeallocActivesList[Clipper->DeallocActivesCount++] = Mem;
        
        for(s32 I = 0;
            I < BASIC_ALLOCATE_COUNT;
            ++I)
        {
            clipper_active *New = Mem + I;
            clipper_active *Free = Clipper->ActivesFreeList;
            Clipper->ActivesFreeList = New;
            New->next_in_ael = Free;
        }

        Result = Clipper->ActivesFreeList;
        Clipper->ActivesFreeList = Result->next_in_ael;
        Result->next_in_ael = 0;
    }

    Result->wind_dx = 1;
    
    return(Result);
}

inline void
FreeOutPt(clipper *Clipper, output_point *p)
{
    ClipperZeroStruct(*p);

    output_point *Free = Clipper->OutPtFreeList;
    Clipper->OutPtFreeList = p;
    p->Next = Free;
}

inline output_point *
GetOutPt(clipper *Clipper, v2_s64 P, u32 OutRectIndex)
{

    output_point *Result = 0;
    if(Clipper->OutPtFreeList)
    {
        Result = Clipper->OutPtFreeList;
        Clipper->OutPtFreeList = Result->Next;
        Result->Next = 0;
    }
    else
    {
        output_point *Mem = MallocArray(BASIC_ALLOCATE_COUNT, output_point);
        // NOTE(pvlso): The list of blocks grows like the other lists (it stopped at 16 blocks: 240 points)
        if(NeedIncrease(Clipper->DeallocOutPtsCount)) {IncreaseDeallocateOutPtsList(Clipper);}
        Clipper->DeallocOutPtsList[Clipper->DeallocOutPtsCount++] = Mem;
        
        for(s32 I = 0;
            I < BASIC_ALLOCATE_COUNT;
            ++I)
        {
            output_point *New = Mem + I;
            output_point *Free = Clipper->OutPtFreeList;
            Clipper->OutPtFreeList = New;
            New->Next = Free;
        }

        Result = Clipper->OutPtFreeList;
        Clipper->OutPtFreeList = Result->Next;
        Result->Next = 0;
    }

    Result->P = P;
    Result->OutRectIndex = OutRectIndex;
    Result->Next = Result;
    Result->Prev = Result;

    return(Result);
}

inline intersect_node
IntersectNode(clipper_active *edge1, clipper_active *edge2, v2_s64 pt)
{
    intersect_node Result = {};
    Result.pt = pt;
    Result.edge1 = edge1;
    Result.edge2 = edge2;

    return(Result);
}

inline u32
NewOutRec(clipper *Clipper)
{
    if(NeedIncrease(Clipper->OutputRectCount))
    {
        IncreaseOutRecList(Clipper);
    }
    
    u32 Result = Clipper->OutputRectCount;
    output_rectangle *Outrec = Clipper->OutRecList + Clipper->OutputRectCount;
    Outrec->Index = Clipper->OutputRectCount++;

    return(Result);
}

inline output_rectangle *
GetOutRec(clipper *Clipper, u32 Index)
{
    output_rectangle *Result = Clipper->OutRecList + Index;
    return(Result);
}

inline void
FreePaths(paths_f64 *Paths)
{
    for(s32 I = 0;
        I < Paths->PathCount;
        ++I)
    {
        path_f64 *Path = Paths->Paths + I;
#if RECORD_MEMORY_USEAGE
        Free(Path->Points, sizeof(v2_f64)*Path->AllocateCount);
        PathMemoryUsed -= sizeof(v2_f64)*Path->AllocateCount;
#else
        Free(Path->Points, sizeof(v2_f64)*Path->AllocateCount);
#endif
    }

#if RECORD_MEMORY_USEAGE
    Free(Paths->Paths, Paths->AllocateCount*sizeof(path_f64));
    PathMemoryUsed -= Paths->AllocateCount*sizeof(path_f64);
#else
    Free(Paths->Paths, Paths->AllocateCount*sizeof(path_s64));
#endif
}

inline void
FreePaths(paths_s64 *Paths)
{
    for(s32 I = 0;
        I < Paths->PathCount;
        ++I)
    {
        path_s64 *Path = Paths->Paths + I;
#if RECORD_MEMORY_USEAGE
        Free(Path->Points, sizeof(v2_s64)*Path->AllocateCount);
        PathMemoryUsed -= sizeof(v2_s64)*Path->AllocateCount;
#else
        Free(Path->Points, sizeof(v2_s64)*Path->AllocateCount);
#endif
    }

#if RECORD_MEMORY_USEAGE
    Free(Paths->Paths, Paths->AllocateCount*sizeof(path_s64));
    PathMemoryUsed -= Paths->AllocateCount*sizeof(path_s64);
#else
    Free(Paths->Paths, Paths->AllocateCount*sizeof(path_s64));
#endif
}

// NOTE(babykaban): SORTS ====================================================================================================================================
inline void
Swap(local_minima *A, local_minima *B)
{
    local_minima Temp = *B;
    *B = *A;
    *A = Temp;
}

inline void
Swap(horz_segment *A, horz_segment *B)
{
    horz_segment Temp = *B;
    *B = *A;
    *A = Temp;
}

inline void
Swap(intersect_node *A, intersect_node *B)
{
    intersect_node Temp = *B;
    *B = *A;
    *A = Temp;
}

inline b32
LocMinSorter(local_minima *A, local_minima *B)
{
    b32 Result = false;

    if(B->Vertex->P.y != A->Vertex->P.y)
    {
        Result = (B->Vertex->P.y < A->Vertex->P.y);
    }
    else
    {
        Result = (B->Vertex->P.x > A->Vertex->P.x);
    }

    return(!Result);
}

inline b32
HorzSegSorter(horz_segment *hs1, horz_segment *hs2)
{
    if(!hs1->right_op || !hs2->right_op)
        return !(hs1->right_op ? 1 : 0);

    return !(hs2->left_op->P.x > hs1->left_op->P.x);
}

inline b32
IntersectListSortI(intersect_node a, intersect_node b)
{
    if(a.pt.y == b.pt.y)
    {
        return a.pt.x < b.pt.x;
    }
    else
    {
        return a.pt.y > b.pt.y; 
    }
}

inline b32
HorzSegSorterI(horz_segment hs1, horz_segment hs2)
{
    if(!hs1.right_op || !hs2.right_op)
        return !(hs1.right_op ? 1 : 0);

    return !(hs2.left_op->P.x > hs1.left_op->P.x);
}

inline b32
LocMinSorterI(clipper_vertex *A, clipper_vertex *B)
{
    b32 Result = false;

    if(B->P.y != A->P.y)
    {
        Result = (B->P.y < A->P.y);
    }
    else
    {
        Result = (B->P.x > A->P.x);
    }

    return(!Result);
}

inline b32
IntersectListSort(intersect_node *a, intersect_node *b)
{
    //note different inequality tests ...
    return (a->pt.y == b->pt.y) ? (a->pt.x < b->pt.x) : (a->pt.y > b->pt.y);
}

inline void
InsertionSort(s32 Count, intersect_node *Nodes)
{
    for(s32 I = 1; I < Count; ++I)
    {
        intersect_node Node = Nodes[I];
        s32 J = I - 1;

        while((J >= 0) && IntersectListSortI(Nodes[J], Node))
        {
            Nodes[J + 1] = Nodes[J];
            J = J - 1;
        }

        Nodes[J + 1] = Node;
    }
}

inline void
InsertionSort(s32 Count, horz_segment *Nodes)
{
    for(s32 I = 1; I < Count; ++I)
    {
        horz_segment Node = Nodes[I];
        s32 J = I - 1;

        while((J >= 0) && HorzSegSorterI(Nodes[J], Node))
        {
            Nodes[J + 1] = Nodes[J];
            J = J - 1;
        }

        Nodes[J + 1] = Node;
    }
}

inline void
InsertionSort(s32 Count, local_minima *Nodes)
{
    for(s32 I = 1; I < Count; ++I)
    {
        local_minima Node = Nodes[I];
        s32 J = I - 1;

        while((J >= 0) && LocMinSorterI(Nodes[J].Vertex, Node.Vertex))
        {
            Nodes[J + 1] = Nodes[J];
            J = J - 1;
        }

        Nodes[J + 1] = Node;
    }
}

internal void
MergeSort(u32 Count, intersect_node *First)
{
    if(Count == 1)
    {
        // NOTE(casey): No work to do.
    }
    else if(Count == 2)
    {
        intersect_node *EntryA = First;
        intersect_node *EntryB = First + 1;
        if(IntersectListSort(EntryA, EntryB))
        {
            Swap(EntryA, EntryB);
        }
    }
    else
    {
        intersect_node *Temp = (intersect_node *)ClipperMalloc(sizeof(intersect_node)*Count);

        u32 Half0 = Count / 2;
        u32 Half1 = Count - Half0;

        Assert(Half0 >= 1);
        Assert(Half1 >= 1);

        intersect_node *InHalf0 = First;
        intersect_node *InHalf1 = First + Half0;
        intersect_node *End = First + Count;

        MergeSort(Half0, InHalf0);
        MergeSort(Half1, InHalf1);

        intersect_node *ReadHalf0 = InHalf0;
        intersect_node *ReadHalf1 = InHalf1;

        intersect_node *Out = Temp;
        for(u32 Index = 0;
            Index < Count;
            ++Index)
        {
            if(ReadHalf0 == InHalf1)
            {
                *Out++ = *ReadHalf1++;
            }
            else if(ReadHalf1 == End)
            {
                *Out++ = *ReadHalf0++;
            }
            else if(!IntersectListSort(ReadHalf0, ReadHalf1))
            {
                *Out++ = *ReadHalf0++;
            }
            else
            {
                *Out++ = *ReadHalf1++;
            }            
        }
        Assert(Out == (Temp + Count));
        Assert(ReadHalf0 == InHalf1);
        Assert(ReadHalf1 == End);
            
        Copy(sizeof(intersect_node)*Count, Temp, First);

        ClipperFreeBlock(Temp);
    }
}

internal void
MergeSort(u32 Count, local_minima *First)
{
    if(Count == 1)
    {
        // NOTE(casey): No work to do.
    }
    else if(Count == 2)
    {
        local_minima *EntryA = First;
        local_minima *EntryB = First + 1;
        if(LocMinSorter(EntryA, EntryB))
        {
            Swap(EntryA, EntryB);
        }
    }
    else
    {
        local_minima *Temp = (local_minima *)ClipperMalloc(sizeof(local_minima)*Count);

        u32 Half0 = Count / 2;
        u32 Half1 = Count - Half0;

        Assert(Half0 >= 1);
        Assert(Half1 >= 1);

        local_minima *InHalf0 = First;
        local_minima *InHalf1 = First + Half0;
        local_minima *End = First + Count;

        MergeSort(Half0, InHalf0);
        MergeSort(Half1, InHalf1);

        local_minima *ReadHalf0 = InHalf0;
        local_minima *ReadHalf1 = InHalf1;

        local_minima *Out = Temp;
        for(u32 Index = 0;
            Index < Count;
            ++Index)
        {
            if(ReadHalf0 == InHalf1)
            {
                *Out++ = *ReadHalf1++;
            }
            else if(ReadHalf1 == End)
            {
                *Out++ = *ReadHalf0++;
            }
            else if(!LocMinSorter(ReadHalf0, ReadHalf1))
            {
                *Out++ = *ReadHalf0++;
            }
            else
            {
                *Out++ = *ReadHalf1++;
            }            
        }
        Assert(Out == (Temp + Count));
        Assert(ReadHalf0 == InHalf1);
        Assert(ReadHalf1 == End);
            
        Copy(sizeof(local_minima)*Count, Temp, First);

        ClipperFreeBlock(Temp);
    }
}

internal void
MergeSort(u32 Count, horz_segment *First)
{
    if(Count == 1)
    {
        // NOTE(casey): No work to do.
    }
    else if(Count == 2)
    {
        horz_segment *EntryA = First;
        horz_segment *EntryB = First + 1;
        if(HorzSegSorter(EntryA, EntryB))
        {
            Swap(EntryA, EntryB);
        }
    }
    else
    {
        horz_segment *Temp = (horz_segment *)ClipperMalloc(sizeof(horz_segment)*Count);

        u32 Half0 = Count / 2;
        u32 Half1 = Count - Half0;

        Assert(Half0 >= 1);
        Assert(Half1 >= 1);

        horz_segment *InHalf0 = First;
        horz_segment *InHalf1 = First + Half0;
        horz_segment *End = First + Count;

        MergeSort(Half0, InHalf0);
        MergeSort(Half1, InHalf1);

        horz_segment *ReadHalf0 = InHalf0;
        horz_segment *ReadHalf1 = InHalf1;

        horz_segment *Out = Temp;
        for(u32 Index = 0;
            Index < Count;
            ++Index)
        {
            if(ReadHalf0 == InHalf1)
            {
                *Out++ = *ReadHalf1++;
            }
            else if(ReadHalf1 == End)
            {
                *Out++ = *ReadHalf0++;
            }
            else if(!HorzSegSorter(ReadHalf0, ReadHalf1))
            {
                *Out++ = *ReadHalf0++;
            }
            else
            {
                *Out++ = *ReadHalf1++;
            }            
        }
        Assert(Out == (Temp + Count));
        Assert(ReadHalf0 == InHalf1);
        Assert(ReadHalf1 == End);
            
        Copy(sizeof(horz_segment)*Count, Temp, First);

        ClipperFreeBlock(Temp);
    }
}
// ===========================================================================================================================================================

#define CLIPPER_H
#endif

// NOTE(pvlso): Polygons in the engine's coordinates for ClipPolygons; an outline winds one way, a hole the other
struct clip_polygon
{
    u32 Count;
    v2 *Points;
};

struct clip_polygon_set
{
    u32 Count;
    clip_polygon *Polygons;
};

#define ENGINE_CLIPPER_H
#endif

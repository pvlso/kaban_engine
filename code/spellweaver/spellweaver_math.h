#if !defined(SPELLWEAVER_MATH_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

//
// NOTE(casey): Scalar operations
//

inline s32
LineIntersect(v2 x0, v2 x1, v2 y0, v2 y1, r32 Tolerance, v2 *sect)
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

inline b32
LineIntersectsRectangle(v2 p0, v2 p1, rectangle2 Rect)
{
    b32 Result = false;
 
    v2 RectCorners[4] =
        {
            Rect.Min,
            {Rect.Max.x, Rect.Min.y},
            Rect.Max,
            {Rect.Min.x, Rect.Max.y}
        };

    v2 RectEdges[4][2] =
        {
            {RectCorners[0], RectCorners[1]},
            {RectCorners[1], RectCorners[2]},
            {RectCorners[2], RectCorners[3]},
            {RectCorners[3], RectCorners[0]}
        };

    for(u32 I = 0;
        I < 4;
        ++I)
    {
        if(LineIntersect(p0, p1, RectEdges[I][0], RectEdges[I][1], 0.0f, 0) == 1)
        {
            Result = true;
        }
    }

    return(Result);
}

inline r32
Distance(v2 x, v2 y0, v2 y1, r32 Tolerance)
{
    r32 Result = Real32Maximum;
    
    v2 dy = y1 - y0;
    v2 x1 = V2(x.x + dy.y, x.y - dy.x);

    v2 s;
    s32 Intersect = LineIntersect(x, x1, y0, y1, Tolerance, &s);
    if(Intersect != -1)
    {
        s = s - x;
        Result = Length(s);
    }

    return(Result);
}

#define SPELLWEAVER_MATH_H
#endif

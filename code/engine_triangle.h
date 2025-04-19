#if !defined(EDITOR_TRIANGLE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

// NOTE(babykaban): Triangulations ===========================================================================================================================

/*
  NOTE(babykaban): Points and adjacencies are structured in counterclockwise order shown below: 

         V3
        /  \
       /    \
      /      \
 AdjV3V1   AdjV2V3
    /          \
   /            \
  V1------------V2
      AdjV1V2
*/

struct triangulate_triangle
{
    union
    {
        struct
        {
            s32 V1, V2, V3;
        };

        s32 Vertices[3];
    };

    union
    {
        struct
        {
            s32 AdjV1V2, AdjV2V3, AdjV3V1;
        };

        s32 Adjacencies[3];
    };
};

union triangle_adjs
{
    struct
    {
        s32 AdjV1V2, AdjV2V3, AdjV3V1;
    };

    s32 Adjacencies[3];
};

struct triangulate_result
{
    s32 TriangleCount;
    triangle *Triangles;
    triangle_adjs *Adjacencies;
};

struct triangulate_resultd
{
    s32 TriangleCount;
    triangled *Triangles;
    triangle_adjs *Adjacencies;
};

inline void
AdjacenciesFindWhichEqualAndSetTo(triangulate_triangle *Test, s32 Compare, s32 Set)
{
    /*
      NOTE(babykaban):
      The function checks all three adjacency entries (AdjV1V2, AdjV2V3, AdjV3V1) of the triangle it's called on.
      It searches for the adjacency value that equals the old neighbor's index (Compare).
      It replaces that old neighbor index with the new one (Set), reflecting the new connection after the flip.
    */

    for(s32 m = 0; m < 3; m++)
    {
        if(Test->Adjacencies[m] == Compare)
        {
            Test->Adjacencies[m] = Set;
            break;
        }
    }
}
// ===========================================================================================================================================================

// NOTE(babykaban): Triangle Subtraction =====================================================================================================================
#define TRISUB_EPSILON_F32 0.00001f
#define TRISUB_MINIMAL_POINT_DISTANCE 0.00008f

#define TRISUB_MAX_POINTS_PER_POLYGON 16 // NOTE(babykaban): Actually 10 but to make sure that there is enough spece 16 should be good
#define TRISUB_MAX_POLYGON_COUNT 4 // NOTE(babykaban): Actually 3 

struct between_indecies
{
    s32 Index0;
    s32 Index1;
};

struct points_between
{
    s32 Count;
    s32 I0;
    s32 I1;
};

struct vertex_info
{
    s32 VertexIndex;
    v2 Point;

    b32 Outside;
    s32 Cross;
    b32 Processed;
};

struct subtract_result
{
    polygon2_set Set;
    b32 Success;
    b32 FullyRemoved;
};

struct vertexd_info
{
    s32 VertexIndex;
    v2d Point;

    b32 Outside;
    s32 Cross;
    b32 Processed;
};

inline b32
TRISUBPointsAreEqual(v2 a, v2 b)
{
    b32 Result = ((AbsoluteValue(a.x - b.x) < TRISUB_EPSILON_F32) && (AbsoluteValue(a.y - b.y) < TRISUB_EPSILON_F32));
    return(Result);
}

inline b32
TRISUBIsNewPoint(polygon2 *A, v2 p)
{
    b32 Result = true;
    for(s32 I = 0;
        I < A->VertexCount;
        ++I)
    {
        if(TRISUBPointsAreEqual(A->Vertices[I], p))
        {
            Result = false;
            break;
        }
    }

    return(Result);
}

inline b32
TRISUBIsNewPoint(v2 *A, s32 Count, v2 p)
{
    b32 Result = true;
    for(s32 I = 0;
        I < Count;
        ++I)
    {
        if(TRISUBPointsAreEqual(A[I], p))
        {
            Result = false;
            break;
        }
    }

    return(Result);
}

inline void
TRISUBInsertPointBetween(polygon2 *A, v2 p, between_indecies Indecies)
{
    for(s32 I = A->VertexCount;
        I > Indecies.Index1;
        --I)
    {
        A->Vertices[I] = A->Vertices[I - 1];
    }

    ++A->VertexCount;
    A->Vertices[Indecies.Index1] = p;
}

// ===========================================================================================================================================================

inline b32
SPLITPointsAreEqual(v2 a, v2 b)
{
    b32 Result = ((AbsoluteValue(a.x - b.x) < 0.0001f) && (AbsoluteValue(a.y - b.y) < 0.0001f));
    return(Result);
}

inline void
InsertPointBetween(polygon2 *A, v2 p, s32 Index)
{
    for(s32 I = A->VertexCount;
        I > Index;
        --I)
    {
        A->Vertices[I] = A->Vertices[I - 1];
    }

    ++A->VertexCount;
    A->Vertices[Index] = p;
}

inline void
InsertPointBetween(polygon2d *A, v2d p, s32 Index)
{
    for(s32 I = A->VertexCount;
        I > Index;
        --I)
    {
        A->Vertices[I] = A->Vertices[I - 1];
    }

    ++A->VertexCount;
    A->Vertices[Index] = p;
}

inline void
RemoveAt(polygon2 *Poly, s32 Index)
{
    Poly->Vertices[Index] = {};
    for(s32 I = Index;
        I < (Poly->VertexCount - 1);
        ++I)
    {
        Poly->Vertices[I] = Poly->Vertices[I + 1];
    }

    Poly->Vertices[Poly->VertexCount] = {};
    --Poly->VertexCount;
}

inline void
RemoveAt(polygon2d *Poly, s32 Index)
{
    Poly->Vertices[Index] = {};
    for(s32 I = Index;
        I < (Poly->VertexCount - 1);
        ++I)
    {
        Poly->Vertices[I] = Poly->Vertices[I + 1];
    }

    Poly->Vertices[Poly->VertexCount] = {};
    --Poly->VertexCount;
}

#define EDITOR_TRIANGLE_H
#endif

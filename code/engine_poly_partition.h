#if !defined(ENGINE_POLY_PARTITION_H)

/*************************************************************************/
/* Copyright (c) 2011-2021 Ivan Fratric and contributors.                */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

enum epp_orientation
{
  EPP_ORIENTATION_CW = -1,
  EPP_ORIENTATION_NONE = 0,
  EPP_ORIENTATION_CCW = 1,
};

enum epp_vertex_type
{
  EPP_VERTEXTYPE_REGULAR = 0,
  EPP_VERTEXTYPE_START = 1,
  EPP_VERTEXTYPE_END = 2,
  EPP_VERTEXTYPE_SPLIT = 3,
  EPP_VERTEXTYPE_MERGE = 4,
};

// 2D point structure.
struct epp_point
{
    f64 x, y;
    // User-specified vertex identifier. Note that this isn't used internally
    // by the library, but will be faithfully copied around.
    int id;
};

inline epp_point
operator-(epp_point A, epp_point B)
{
    epp_point r;
    r.x = A.x - B.x;
    r.y = A.y - B.y;
    return r;
}

inline epp_point
operator/(epp_point P, f64 A)
{
    epp_point r = {};
    r.x = P.x / A;
    r.y = P.y / A;

    return r;
}

// Polygon implemented as an array of points with a "hole" flag.
struct epp_poly
{
    epp_point *points;
    s32 numpoints;
    b32 hole;
};

inline b32
Valid(epp_poly P)
{
    return(P.numpoints >= 3);
}

struct partition_vertex
{
    b32 isActive;
    b32 isConvex;
    b32 isEar;

    epp_point p;
    f64 angle;
    partition_vertex *previous;
    partition_vertex *next;
};

struct monotone_vertex
{
    epp_point p;
    s32 previous;
    s32 next;
};

struct diagonal
{
    s32 index1;
    s32 index2;
};

// Dynamic programming state for minimum-weight triangulation.
struct dp_state
{
    b32 visible;
    f64 weight;
    s32 bestvertex;
};

// Dynamic programming state for convex partitioning.
struct dp_state2
{
    b32 visible;
    s32 weight;
    diagonal *pairs;
};

// Edge that intersects the scanline.
struct scan_line_edge
{
    s32 index;
    epp_point p1;
    epp_point p2;
};

// Simple heuristic procedure for removing holes from a list of polygons.
// It works by creating a diagonal from the right-most hole vertex
// to some other visible vertex.
// Time complexity: O(h*(n^2)), h is the # of holes, n is the # of vertices.
// Space complexity: O(n)
// params:
//    inpolys:
//       A list of polygons that can contain holes.
//       Vertices of all non-hole polys have to be in counter-clockwise order.
//       Vertices of all hole polys have to be in clockwise order.
//    outpolys:
//       A list of polygons without holes.
// Returns 1 on success, 0 on failure.
internal int EPPRemoveHoles(TPPLPolyList *inpolys, TPPLPolyList *outpolys);

// Triangulates a polygon by ear clipping.
// Time complexity: O(n^2), n is the number of vertices.
// Space complexity: O(n)
// params:
//    poly:
//       An input polygon to be triangulated.
//       Vertices have to be in counter-clockwise order.
//    triangles:
//       A list of triangles (result).
// Returns 1 on success, 0 on failure.
internal int EPPTriangulate_EC(TPPLPoly *poly, TPPLPolyList *triangles);

// Triangulates a list of polygons that may contain holes by ear clipping
// algorithm. It first calls RemoveHoles to get rid of the holes, and then
// calls Triangulate_EC for each resulting polygon.
// Time complexity: O(h*(n^2)), h is the # of holes, n is the # of vertices.
// Space complexity: O(n)
// params:
//    inpolys:
//       A list of polygons to be triangulated (can contain holes).
//       Vertices of all non-hole polys have to be in counter-clockwise order.
//       Vertices of all hole polys have to be in clockwise order.
//    triangles:
//       A list of triangles (result).
// Returns 1 on success, 0 on failure.
internal int EPPTriangulate_EC(TPPLPolyList *inpolys, TPPLPolyList *triangles);

// Creates an optimal polygon triangulation in terms of minimal edge length.
// Time complexity: O(n^3), n is the number of vertices
// Space complexity: O(n^2)
// params:
//    poly:
//       An input polygon to be triangulated.
//       Vertices have to be in counter-clockwise order.
//    triangles:
//       A list of triangles (result).
// Returns 1 on success, 0 on failure.
internal int EPPTriangulate_OPT(TPPLPoly *poly, TPPLPolyList *triangles);

// Triangulates a polygon by first partitioning it into monotone polygons.
// Time complexity: O(n*log(n)), n is the number of vertices.
// Space complexity: O(n)
// params:
//    poly:
//       An input polygon to be triangulated.
//       Vertices have to be in counter-clockwise order.
//    triangles:
//       A list of triangles (result).
// Returns 1 on success, 0 on failure.
internal int EPPTriangulate_MONO(TPPLPoly *poly, TPPLPolyList *triangles);

// Triangulates a list of polygons by first
// partitioning them into monotone polygons.
// Time complexity: O(n*log(n)), n is the number of vertices.
// Space complexity: O(n)
// params:
//    inpolys:
//       A list of polygons to be triangulated (can contain holes).
//       Vertices of all non-hole polys have to be in counter-clockwise order.
//       Vertices of all hole polys have to be in clockwise order.
//    triangles:
//       A list of triangles (result).
// Returns 1 on success, 0 on failure.
internal int EPPTriangulate_MONO(TPPLPolyList *inpolys, TPPLPolyList *triangles);

// Creates a monotone partition of a list of polygons that
// can contain holes. Triangulates a set of polygons by
// first partitioning them into monotone polygons.
// Time complexity: O(n*log(n)), n is the number of vertices.
// Space complexity: O(n)
// params:
//    inpolys:
//       A list of polygons to be triangulated (can contain holes).
//       Vertices of all non-hole polys have to be in counter-clockwise order.
//       Vertices of all hole polys have to be in clockwise order.
//    monotonePolys:
//       A list of monotone polygons (result).
// Returns 1 on success, 0 on failure.
internal int EPPMonotonePartition(TPPLPolyList *inpolys, TPPLPolyList *monotonePolys);

// Partitions a polygon into convex polygons by using the
// Hertel-Mehlhorn algorithm. The algorithm gives at most four times
// the number of parts as the optimal algorithm, however, in practice
// it works much better than that and often gives optimal partition.
// It uses triangulation obtained by ear clipping as intermediate result.
// Time complexity O(n^2), n is the number of vertices.
// Space complexity: O(n)
// params:
//    poly:
//       An input polygon to be partitioned.
//       Vertices have to be in counter-clockwise order.
//    parts:
//       Resulting list of convex polygons.
// Returns 1 on success, 0 on failure.
internal int EPPConvexPartition_HM(TPPLPoly *poly, TPPLPolyList *parts);

// Partitions a list of polygons into convex parts by using the
// Hertel-Mehlhorn algorithm. The algorithm gives at most four times
// the number of parts as the optimal algorithm, however, in practice
// it works much better than that and often gives optimal partition.
// It uses triangulation obtained by ear clipping as intermediate result.
// Time complexity O(n^2), n is the number of vertices.
// Space complexity: O(n)
// params:
//    inpolys:
//       An input list of polygons to be partitioned. Vertices of
//       all non-hole polys have to be in counter-clockwise order.
//       Vertices of all hole polys have to be in clockwise order.
//    parts:
//       Resulting list of convex polygons.
// Returns 1 on success, 0 on failure.
internal int EPPConvexPartition_HM(TPPLPolyList *inpolys, TPPLPolyList *parts);

// Optimal convex partitioning (in terms of number of resulting
// convex polygons) using the Keil-Snoeyink algorithm.
// For reference, see M. Keil, J. Snoeyink, "On the time bound for
// convex decomposition of simple polygons", 1998.
// Time complexity O(n^3), n is the number of vertices.
// Space complexity: O(n^3)
// params:
//    poly:
//       An input polygon to be partitioned.
//       Vertices have to be in counter-clockwise order.
//    parts:
//       Resulting list of convex polygons.
// Returns 1 on success, 0 on failure.
internal int EPPConvexPartition_OPT(TPPLPoly *poly, TPPLPolyList *parts);

#define ENGINE_POLY_PARTITION_H
#endif

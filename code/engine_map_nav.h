#if !defined(ENGINE_MAP_NAV_H)
/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): A map's navigation mesh (map editor plan, 6d). The map editor's areas (engine_map_file.h) are baked
  level by level: the walkable areas joined, the holes cut out of them, the result split into convex polygons. Two
  polygons of a level are linked over the edge they share. Polygons of different levels are linked only where both
  stop: an outer edge of one lies on an outer edge of the other with their insides on opposite sides (stairs, a
  ramp). So a bridge over a path, which only overlaps it, stays apart. An edge may touch only part of the other's,
  the link is the part they share.

  Every polygon winds the same way (a positive area, MapNavSignedArea), and a link's A to B follows the winding of
  the polygon it's listed under.

  Write Assets stores a baked mesh in the map's asset (engine_file_formats.h); the map editor bakes one to test it.
*/

#define MAP_NAV_ROUNDING 1000.0f // NOTE(pvlso): baked points are rounded to this part of a cell
#define MAP_NAV_EPSILON 0.01f    // NOTE(pvlso): in cells: closer than this is touching

struct map_nav_mesh
{
    u32 PolyCount;
    u32 LinkCount;
    u32 PointCount;
    kea_nav_poly *Polys;
    kea_nav_link *Links;
    v2 *Points;
};

struct map_nav_search_node
{
    r32 Cost;   // NOTE(pvlso): from the start to Entry
    r32 Total;  // NOTE(pvlso): Cost and the straight way from Entry to the goal
    v2 Entry;   // NOTE(pvlso): the middle of the link it was reached over
    s32 Parent; // NOTE(pvlso): the polygon it was reached from, -1 for none
    u32 Link;   // NOTE(pvlso): the parent's link it was reached over
    b32 Open;
    b32 Closed;
};

// NOTE(pvlso): What a path search works in, MapNavScratchSize bytes for a mesh's polygons
struct map_nav_scratch
{
    map_nav_search_node *Nodes;
    v2 *Portals;        // NOTE(pvlso): left, right per portal
    s32 *Levels;        // NOTE(pvlso): per portal, the level past it
    v2 *Corners;
    s32 *CornerPortals;
};

inline umm
MapNavScratchSize(u32 PolyCount)
{
    umm PortalCount = (umm)PolyCount + 2;
    umm Result = ((umm)PolyCount*sizeof(map_nav_search_node) + PortalCount*2*sizeof(v2) + PortalCount*sizeof(v2) +
                  PortalCount*sizeof(s32) + PortalCount*sizeof(s32));

    return(Result);
}

inline map_nav_scratch
MapNavScratchOf(void *Memory, u32 PolyCount)
{
    umm PortalCount = (umm)PolyCount + 2;
    map_nav_scratch Result = {};
    Result.Nodes = (map_nav_search_node *)Memory;
    Result.Portals = (v2 *)(Result.Nodes + PolyCount);
    Result.Corners = Result.Portals + PortalCount*2;
    Result.Levels = (s32 *)(Result.Corners + PortalCount);
    Result.CornerPortals = Result.Levels + PortalCount;

    return(Result);
}

#define ENGINE_MAP_NAV_H
#endif

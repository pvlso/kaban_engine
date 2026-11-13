#if !defined(ENGINE_API_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The game <-> engine boundary. This is the only header a game includes
  (with code/ on the include path), the engine includes it too.

  Everything in here is compiled into the game (types, math, memory arenas, strings,
  random, input, nuklear), except the engine_api table: the engine fills it in and
  passes it to the game every frame, together with the platform_api.

  The game is a library that exports GameUpdateAndRender (see GAME_UPDATE_AND_RENDER).
  It's a unity build, so exactly one .cpp of the game defines the API globals:

      platform_api Platform;
      engine_api Engine;
      debug_table *GlobalDebugTable;

  and sets them from game_memory at the start of GameUpdateAndRender. GlobalDebugTable
  makes the profiler macros (TIMED_FUNCTION, TIMED_BLOCK, BEGIN_BLOCK/END_BLOCK) work in the
  game, its blocks show up in the debug window's Profile panel under "GameUpdateAndRender".
  It also makes LOG / LOG_WARNING / LOG_ERROR (printf style) work, they go to the debug
  window's Log panel and build/kaban.log.
  It's 0 in release builds, where the macros compile to nothing. A TIMED_* in
  GameUpdateAndRender itself has to come after GlobalDebugTable is set.
*/

#include "engine_platform.h"
#include "engine_nuklear.h"

#include "engine_math.h"
#include "engine_random.h"
#include "engine_memory.h"
#include "engine_string.h"

#include "engine_render_group.h"

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): ASSETS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
struct engine_assets;

struct asset_tag_pair
{
    char *Key;
    char *Value;
};

struct asset_tag_vector
{
    u32 PairCount;
    asset_tag_pair Pairs[24];
};

struct asset_best_match_result
{
    u32 AssetCount;

    union
    {
        u32 Index;
        u32 *Indecies;
    };
};

// NOTE(pvlso): A curated tile set (made in the assets mode from any sheets): its tiles laid out in a grid of its own
struct tileset_info
{
    u32 TileCount;
    u32 GridWidth;
    u32 GridHeight;
    u32 TileSize; // NOTE(pvlso): in pixels
};

// NOTE(pvlso): A map made in the map editor (an island, in Spellweaver): Width x Height cells in layers. A map is drawn
// level by level from the lowest (0 the ground, 1 a bridge or plateau above it, -1 below): a level's layers by Z and,
// within a Z, in their order, then the entities on that level, then the next level. A cell's Set is 0 when empty, else the map's set + 1 (GetMapTileset gives that
// set's tile set, for GetTileInfo); Tile is the tile's index in its set.
struct map_info
{
    u32 Width;
    u32 Height;
    u32 LayerCount;
    u32 SetCount;
    u32 EntityCount; // NOTE(pvlso): the entities placed on it, GetMapEntity
    u32 NavPolyCount; // NOTE(pvlso): the convex polygons of its navigation mesh, 0 when it has none
};

// NOTE(pvlso): A point of a path over a map's navigation mesh (FindNavPath), in cells from the map's top left (y
// down); Level is the level from this point on
struct nav_path_point
{
    v2 P;
    s32 Level;
};

struct map_tile_cell
{
    u16 Set;
    u16 Tile;
};

struct map_layer_info
{
    char Name[32];
    s32 Z;     // NOTE(pvlso): its order within its level
    s32 Level;
    b32 Hidden;
    map_tile_cell *Cells; // NOTE(pvlso): Width*Height, row by row from the top; valid while the game's assets are
};

// NOTE(pvlso): An entity definition made in the entity editor, packed from the project's data/entities; a game finds
// one by its name (FindEntity) or by its GUID (FindEntityByGUID: maps and saves name definitions by it). It's only
// what any game needs to show and place a thing: a display name, its type (one of the project's
// data/txts/entity_types.txt), clips, a footprint (blocks movement) and a hit area (what spells and interaction hit).
// What it does is the game's.
enum entity_collision_shape
{
    EntityCollision_None,
    EntityCollision_Circle,
    EntityCollision_Box,
};

struct entity_shape_info
{
    entity_collision_shape Shape;
    v2 Center; // NOTE(pvlso): from the foot, in pixels, y up
    v2 Size;   // NOTE(pvlso): a box's width and height, a circle's radius in x
};

struct entity_info
{
    u64 GUID;
    char Name[32]; // NOTE(pvlso): its file's, without .txt
    char DisplayName[64];
    char Type[32];  // NOTE(pvlso): empty when it has none
    u32 ClipCount;  // NOTE(pvlso): the clips it has a look for; the first is its default
    entity_shape_info Footprint;
    entity_shape_info HitArea;
};

struct entity_clip_info
{
    char Name[32];
    bitmap_id Bitmap; // NOTE(pvlso): the frame showing at the time asked for, standing on its foot (the asset's
                      // alignment); 0 when it can't be found
    u32 SpriteSheet;  // NOTE(pvlso): the sprite sheet's asset index for GetSprite, 0 when the look is a bitmap
    u32 FrameCount;
    r32 FramesPerSecond;
    b32 Loop;
    b32 Finished;     // NOTE(pvlso): a clip played once is past its last frame (which stays showing)
    v2 Dim;           // NOTE(pvlso): the frame's size in pixels
    v2 Align;         // NOTE(pvlso): where its foot is on the frame, 0 to 1 from the bottom left (the entity editor's,
                      // else the asset's alignment): draw it with no alignment of its own at P - Align*Dim
};

// NOTE(pvlso): An entity placed on a map in the map editor: its own GUID (kept across the map's saves, for a save to
// name it by), its definition, where its foot stands and whether it's mirrored
struct map_entity_info
{
    u64 GUID;
    u64 Definition;    // NOTE(pvlso): the definition's GUID
    u32 Entity;        // NOTE(pvlso): the definition's asset index (FindEntityByGUID), 0 when it isn't in the KEA
    v2 P;              // NOTE(pvlso): the foot, in cells from the map's top left, y down (like the rows)
    b32 Flip;          // NOTE(pvlso): mirrored left to right
    s32 Level;         // NOTE(pvlso): the level it stands on
};

// NOTE(pvlso): Its picture is a part of its set's atlas, drawn with PushTile
struct tile_info
{
    bitmap_id Atlas;  // NOTE(pvlso): its set's pictures; 0 when the set has none
    v2 MinUV;         // NOTE(pvlso): the frame showing at the time asked for, when the tile is animated
    v2 MaxUV;
    u32 Flags;        // NOTE(pvlso): bit n is the n-th name in the project's data/txts/tile_flags.txt
    u32 Kind;         // NOTE(pvlso): 0 for none, else the n-th name in the project's data/txts/tile_kinds.txt, from 1
    u32 GridX;        // NOTE(pvlso): its place in the set's grid, from the top left
    u32 GridY;
    b32 Animated;
    char Name[32];
};

inline void
TagVectorAdd(asset_tag_vector *Vector, char *Key, char *Value)
{
    asset_tag_pair *Pair = Vector->Pairs + Vector->PairCount++;
    Pair->Key = Key;
    Pair->Value = Value;
}

inline void
ClearTagVector(asset_tag_vector *Vector)
{
    Vector->PairCount = 0;
}
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): FONTS
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): A .ttf from the project's data/ttfs. The engine owns it, the pointer stays good until the game stops
// (Restart and hot reloads keep it).
struct engine_font;

enum text_align
{
    TextAlign_Left,
    TextAlign_Center,
    TextAlign_Right,
};

// NOTE(pvlso): In meters, for the text drawn at that Height with that render group's camera
struct text_dim
{
    r32 Width;       // NOTE(pvlso): the widest line
    r32 Ascent;      // NOTE(pvlso): above the baseline
    r32 Descent;     // NOTE(pvlso): below the baseline, negative
    r32 LineAdvance; // NOTE(pvlso): baseline to baseline
    u32 LineCount;
};
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): ENGINE API
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): Assets can be 0 (no asset file loaded), then bitmaps are skipped and
// GetBestMatchAssets finds nothing.
struct engine_api
{
    // NOTE(pvlso): Covers the whole render target (Commands->Width x Commands->Height)
    render_group (*BeginRenderGroup)(engine_assets *Assets, editor_render_commands *Commands);
    void (*EndRenderGroup)(render_group *Group);
    void (*Perspective)(render_group *Group, r32 MetersToPixels, r32 FocalLength, r32 DistanceAboveTarget);
    void (*Orthographic)(render_group *Group, r32 MetersToPixels);
    void (*Clear)(render_group *Group, v4 Color);

    // NOTE(pvlso): Pixel art: what is pushed between these goes into an image of whole pixels (at least MinWidth x
    // MinHeight) scaled up on screen by a whole number, under everything pushed outside them. Set the camera
    // after BeginPixelWorld, in the image's pixels; EndPixelWorld puts the screen's camera back.
    pixel_world (*BeginPixelWorld)(render_group *Group, s32 MinWidth, s32 MinHeight);
    void (*EndPixelWorld)(render_group *Group);
    // NOTE(pvlso): For smooth scrolling: the camera's position past whole image pixels (0 to 1), when the world
    // is drawn with the camera rounded down to them
    void (*SetPixelWorldScroll)(render_group *Group, v2 Scroll);
    // NOTE(pvlso): A game shader draws the world on screen: the world image with its upright things over it, as they
    // would be on screen, so it covers everything pushed in the pixel world. The text asset at ShaderIndex (found
    // with GetBestMatchAssets) is a GLSL 330 fragment shader without its declarations, the engine puts these before:
    //     uniform sampler2D Texture;  // the world on screen, ScreenSize screen pixels, read at FragUV
    //     uniform vec2 ScreenSize;
    //     uniform vec2 ImageSize;     // the world image, in its pixels
    //     uniform float PixelScale;   // screen pixels per image pixel
    //     uniform vec2 PixelOrigin;   // where image pixel (0, 0) starts, in screen pixels
    //     uniform float Time;         // Params.Time
    //     uniform vec4 Params[4];     // Params.Params
    //     in vec2 FragUV;  in vec4 FragColor;  out vec4 OutColor;
    // It runs for every screen pixel. Something that changes across the screen (a vignette) should be worked out per
    // image pixel ((FragUV*ScreenSize - PixelOrigin) / PixelScale), so the pixels stay square. A shader that doesn't
    // compile is logged (once for each version of it) and the world is drawn without it. 0 for no shader.
    void (*SetPixelWorldShader)(render_group *Group, u32 ShaderIndex, render_shader_params Params);
    // NOTE(pvlso): Lighting for the pixel world. Each world pixel's colour is multiplied by Ambient plus the lights
    // reaching it (falling off over Radius meters, more where its normal map faces the light). Called every frame it
    // should be lit, before the game's shader runs on the lit world. Without it the world isn't lit at all.
    void (*SetPixelWorldLighting)(render_group *Group, v3 Ambient);
    // NOTE(pvlso): A light at P, P.z meters above the ground (in the pixel world, up to 64 a frame)
    void (*PushLight)(render_group *Group, object_transform *ObjectTransform, v3 P, r32 Radius, v3 Color);

    // NOTE(pvlso): NormalMap (GetNormalMap, or 0) makes it catch the pixel world's lights by its shape
    void (*PushBitmap)(render_group *Group, object_transform *ObjectTransform, bitmap_id ID, r32 Height, v3 Offset,
                       v4 Color, r32 CAlign, r32 Angle, bitmap_id NormalMap);
    void (*PushRect)(render_group *Group, object_transform *ObjectTransform, v3 Offset, v2 Dim, v4 Color);
    void (*PushLine)(render_group *Group, object_transform *ObjectTransform, v3 P0, v3 P1, v4 Color);
    void (*PushCircleOutline)(render_group *Group, object_transform *ObjectTransform, v3 Center, f32 Radius,
                              s32 Segments, v4 Color);
    // NOTE(pvlso): A filled triangle (the corners in the transform's space; Color's alpha blends it)
    void (*PushTriangle)(render_group *Group, object_transform *ObjectTransform, v3 A, v3 B, v3 C, v4 Color);
    // NOTE(pvlso): A tile (GetTileInfo, at the frame asked for there) as a Size square, its bottom left corner at
    // Corner. Its set's atlas loads the first time; nothing is drawn until it has.
    void (*PushTile)(render_group *Group, object_transform *ObjectTransform, tile_info *Tile, v3 Corner, r32 Size,
                     v4 Color);

    // NOTE(pvlso): Loads data/ttfs/<FileName> the first time, 0 when it's missing or not a font (logged once).
    engine_font *(*GetFont)(char *FileName);
    // NOTE(pvlso): P is where the first line's baseline starts (Align moves the line around it), Height is the
    // font's height from ascent to descent in meters, rounded to whole screen pixels. '\n' starts a new line
    // below. Text is UTF-8, anything outside ASCII and Latin-1 draws as '?'. A Font of 0 draws nothing.
    void (*PushText)(render_group *Group, object_transform *ObjectTransform, engine_font *Font, char *Text, v3 P,
                     r32 Height, v4 Color, text_align Align);
    text_dim (*GetTextDim)(render_group *Group, engine_font *Font, char *Text, r32 Height);

    // NOTE(pvlso): A tie (AssetCount > 1) is pushed on Arena, nothing is pushed for 0 or 1 matches.
    // Arena can be 0 when only one match is expected, a tie then returns only its count.
    asset_best_match_result (*GetBestMatchAssets)(engine_assets *Assets, asset_tag_vector *Vector, memory_arena *Arena);
    // NOTE(pvlso): The sprite at SpriteIndex (left to right) of the sprite sheet at SheetIndex (found with
    // GetBestMatchAssets), for PushBitmap. 0 when it isn't a sprite sheet or has no such sprite.
    bitmap_id (*GetSprite)(engine_assets *Assets, u32 SheetIndex, u32 SpriteIndex);
    // NOTE(pvlso): All zero when TilesetIndex isn't a tile set
    tileset_info (*GetTilesetInfo)(engine_assets *Assets, u32 TilesetIndex);
    // NOTE(pvlso): The tile at TileIndex of the tile set at TilesetIndex (found with GetBestMatchAssets, or a map's
    // GetMapTileset); tiles are numbered in the order the set lists them. Time in seconds picks an animated tile's
    // frame (any running clock). All zero when there's no such tile.
    tile_info (*GetTileInfo)(engine_assets *Assets, u32 TilesetIndex, u32 TileIndex, r32 Time);
    // NOTE(pvlso): A map at MapIndex (found with GetBestMatchAssets); all zero when it isn't one
    map_info (*GetMapInfo)(engine_assets *Assets, u32 MapIndex);
    // NOTE(pvlso): The map made of FileName in data/maps (as game_start names it), 0 when the asset file hasn't got it
    u32 (*FindMap)(engine_assets *Assets, char *FileName);
    map_layer_info (*GetMapLayer)(engine_assets *Assets, u32 MapIndex, u32 LayerIndex);
    // NOTE(pvlso): The asset index of the map's set SetIndex (from 0; a cell's Set - 1), 0 when it isn't in the KEA
    u32 (*GetMapTileset)(engine_assets *Assets, u32 MapIndex, u32 SetIndex);
    // NOTE(pvlso): All zero when there's no such entity
    map_entity_info (*GetMapEntity)(engine_assets *Assets, u32 MapIndex, u32 EntityIndex);
    // NOTE(pvlso): The polygon of the map's navigation mesh under P, on Level if there's one there, else the highest
    // one; -1 when P isn't on the mesh
    s32 (*FindNavPoly)(engine_assets *Assets, u32 MapIndex, v2 P, s32 Level);
    s32 (*GetNavPolyLevel)(engine_assets *Assets, u32 MapIndex, s32 Poly);
    // NOTE(pvlso): The corners of the mesh's polygon Poly (0 to map_info NavPolyCount - 1), convex, in cells from the
    // map's top left, up to MaxPoints of them; the count comes back, 0 when there's no such polygon
    u32 (*GetNavPolyPoints)(engine_assets *Assets, u32 MapIndex, s32 Poly, v2 *Points, u32 MaxPoints);
    // NOTE(pvlso): The shortest way over the mesh: Points gets its corners and the places where it changes level,
    // starting with From and ending with To (points past MaxPoints are left out, walk them and ask again). The count
    // comes back, 0 when there's no way or an end isn't on the mesh.
    u32 (*FindNavPath)(engine_assets *Assets, u32 MapIndex, v2 From, s32 FromLevel, v2 To, s32 ToLevel,
                       nav_path_point *Points, u32 MaxPoints);
    // NOTE(pvlso): Walks straight from From to To over the mesh, across links (Level follows) and sliding along walls;
    // gives where it got to. From off the mesh goes to the closest point first. Without a mesh it gives To.
    v2 (*MoveOnNav)(engine_assets *Assets, u32 MapIndex, v2 From, s32 *Level, v2 To);
    // NOTE(pvlso): The point of the mesh closest to P and its level in Level; P itself where it's on the mesh, on Level
    // if there's a polygon of that level there. Without a mesh it gives P.
    v2 (*ClosestNavPoint)(engine_assets *Assets, u32 MapIndex, v2 P, s32 *Level);
    // NOTE(pvlso): Only on Level: P where a polygon of Level is under it, else the closest point of Level's polygons,
    // even where another level is under P. P when Level has no polygons, or without a mesh.
    v2 (*ClosestNavPointOnLevel)(engine_assets *Assets, u32 MapIndex, v2 P, s32 Level);
    // NOTE(pvlso): The asset index of the definition saved as data/entities/<Name>.txt, or with that GUID; 0 when it
    // isn't in the KEA
    u32 (*FindEntity)(engine_assets *Assets, char *Name);
    u32 (*FindEntityByGUID)(engine_assets *Assets, u64 GUID);
    // NOTE(pvlso): All zero when EntityIndex isn't an entity, or it has no such clip. Time in seconds since the clip
    // started picks a sprite sheet's frame.
    entity_info (*GetEntityInfo)(engine_assets *Assets, u32 EntityIndex);
    entity_clip_info (*GetEntityClip)(engine_assets *Assets, u32 EntityIndex, u32 ClipIndex, r32 Time);
// NOTE(pvlso): The normal map of a sprite or tile, when its sheet had one (<name>_n next to it), else 0
    bitmap_id (*GetNormalMap)(engine_assets *Assets, bitmap_id Bitmap);
};

// NOTE(pvlso): Owned by the engine, so it survives hot reloads of the game code. The arenas
// start empty and grow through Platform.AllocateMemory, the engine frees them when the game stops.
struct game_start
{
    char MapFileName[64];
    b32 HasP;
    v2 P;
    s32 Level;
};

struct game_memory
{
    memory_arena PermanentArena;
    memory_arena TransientArena;

    // NOTE(pvlso): The game's own state, usually pushed on the permanent arena on the first frame
    void *GameState;

    // NOTE(pvlso): Goes up by one every time the engine swaps the running game to a newly built KEA (a map saved in the
    // map editor, a picture added). Asset indices and pointers taken from the old one are wrong then: a game that
    // keeps any compares this with the count it last saw and takes them again (reloads its map).
    u32 AssetSwapCount;

    // NOTE(pvlso): Set when the map editor's Play (F5) started the game, empty when Run did: the map being edited by
    // its file name in data/maps (FindMap gives its asset) and, when HasP, where to start on it: P in cells from the
    // map's top left (y down), on Level. It stays for Restart.
    game_start Start;

    // NOTE(pvlso): Refilled before every call, the engine code can be reloaded as well
    platform_api PlatformAPI;
    engine_api EngineAPI;

    // NOTE(pvlso): The profiler's event table, 0 in release builds
    debug_table *DebugTable;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *Memory, engine_input *Input, editor_render_commands *RenderCommands, \
                                               engine_assets *Assets, struct nk_context *Nk)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#if COMPILER_MSVC
#define GAME_EXPORT extern "C" __declspec(dllexport)
#else
#define GAME_EXPORT extern "C" __attribute__((visibility("default")))
#endif
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): GAME SIDE, calls through the engine_api with the same names and defaults the engine uses
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#if !defined(ENGINE_BUILD)
extern engine_api Engine;

inline render_group
BeginRenderGroup(engine_assets *Assets, editor_render_commands *Commands)
{
    render_group Result = Engine.BeginRenderGroup(Assets, Commands);
    return(Result);
}

inline void EndRenderGroup(render_group *Group) {Engine.EndRenderGroup(Group);}

inline void
Perspective(render_group *Group, r32 MetersToPixels, r32 FocalLength, r32 DistanceAboveTarget)
{
    Engine.Perspective(Group, MetersToPixels, FocalLength, DistanceAboveTarget);
}

inline void Orthographic(render_group *Group, r32 MetersToPixels) {Engine.Orthographic(Group, MetersToPixels);}
inline void Clear(render_group *Group, v4 Color) {Engine.Clear(Group, Color);}

inline pixel_world
BeginPixelWorld(render_group *Group, s32 MinWidth, s32 MinHeight)
{
    pixel_world Result = Engine.BeginPixelWorld(Group, MinWidth, MinHeight);
    return(Result);
}

inline void EndPixelWorld(render_group *Group) {Engine.EndPixelWorld(Group);}
inline void SetPixelWorldScroll(render_group *Group, v2 Scroll) {Engine.SetPixelWorldScroll(Group, Scroll);}
inline void
SetPixelWorldShader(render_group *Group, u32 ShaderIndex, render_shader_params Params)
{
    Engine.SetPixelWorldShader(Group, ShaderIndex, Params);
}

inline void
PushBitmap(render_group *Group, object_transform *ObjectTransform, bitmap_id ID, r32 Height, v3 Offset,
           v4 Color = V4(1, 1, 1, 1), r32 CAlign = 1.0f, r32 Angle = 0.0f, bitmap_id NormalMap = {})
{
    Engine.PushBitmap(Group, ObjectTransform, ID, Height, Offset, Color, CAlign, Angle, NormalMap);
}

inline void
SetPixelWorldLighting(render_group *Group, v3 Ambient)
{
    Engine.SetPixelWorldLighting(Group, Ambient);
}

inline void
PushLight(render_group *Group, object_transform *ObjectTransform, v3 P, r32 Radius, v3 Color)
{
    Engine.PushLight(Group, ObjectTransform, P, Radius, Color);
}

inline bitmap_id
GetNormalMap(engine_assets *Assets, bitmap_id Bitmap)
{
    bitmap_id Result = Engine.GetNormalMap(Assets, Bitmap);
    return(Result);
}

inline void
PushRect(render_group *Group, object_transform *ObjectTransform, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1))
{
    Engine.PushRect(Group, ObjectTransform, Offset, Dim, Color);
}

inline void
PushLine(render_group *Group, object_transform *ObjectTransform, v3 P0, v3 P1, v4 Color = V4(1, 1, 1, 1))
{
    Engine.PushLine(Group, ObjectTransform, P0, P1, Color);
}

inline void
PushCircleOutline(render_group *Group, object_transform *ObjectTransform, v3 Center, f32 Radius, s32 Segments,
                  v4 Color = V4(1, 1, 1, 1))
{
    Engine.PushCircleOutline(Group, ObjectTransform, Center, Radius, Segments, Color);
}

inline void
PushTriangle(render_group *Group, object_transform *ObjectTransform, v3 A, v3 B, v3 C, v4 Color = V4(1, 1, 1, 1))
{
    Engine.PushTriangle(Group, ObjectTransform, A, B, C, Color);
}

inline void
PushTile(render_group *Group, object_transform *ObjectTransform, tile_info *Tile, v3 Corner, r32 Size = 1.0f,
         v4 Color = V4(1, 1, 1, 1))
{
    Engine.PushTile(Group, ObjectTransform, Tile, Corner, Size, Color);
}

inline asset_best_match_result
GetBestMatchAssets(engine_assets *Assets, asset_tag_vector *Vector, memory_arena *Arena = 0)
{
    asset_best_match_result Result = Engine.GetBestMatchAssets(Assets, Vector, Arena);
    return(Result);
}

inline bitmap_id
GetSprite(engine_assets *Assets, u32 SheetIndex, u32 SpriteIndex)
{
    bitmap_id Result = Engine.GetSprite(Assets, SheetIndex, SpriteIndex);
    return(Result);
}

inline tileset_info
GetTilesetInfo(engine_assets *Assets, u32 TilesetIndex)
{
    tileset_info Result = Engine.GetTilesetInfo(Assets, TilesetIndex);
    return(Result);
}

inline tile_info
GetTileInfo(engine_assets *Assets, u32 TilesetIndex, u32 TileIndex, r32 Time = 0.0f)
{
    tile_info Result = Engine.GetTileInfo(Assets, TilesetIndex, TileIndex, Time);
    return(Result);
}

inline map_info GetMapInfo(engine_assets *Assets, u32 MapIndex) {return(Engine.GetMapInfo(Assets, MapIndex));}
inline u32 FindMap(engine_assets *Assets, char *FileName) {return(Engine.FindMap(Assets, FileName));}
inline map_layer_info
GetMapLayer(engine_assets *Assets, u32 MapIndex, u32 LayerIndex)
{
    map_layer_info Result = Engine.GetMapLayer(Assets, MapIndex, LayerIndex);
    return(Result);
}
inline u32
GetMapTileset(engine_assets *Assets, u32 MapIndex, u32 SetIndex)
{
    u32 Result = Engine.GetMapTileset(Assets, MapIndex, SetIndex);
    return(Result);
}

inline map_entity_info
GetMapEntity(engine_assets *Assets, u32 MapIndex, u32 EntityIndex)
{
    map_entity_info Result = Engine.GetMapEntity(Assets, MapIndex, EntityIndex);
    return(Result);
}

inline s32 FindNavPoly(engine_assets *Assets, u32 MapIndex, v2 P, s32 Level) {return(Engine.FindNavPoly(Assets, MapIndex, P, Level));}
inline s32 GetNavPolyLevel(engine_assets *Assets, u32 MapIndex, s32 Poly) {return(Engine.GetNavPolyLevel(Assets, MapIndex, Poly));}
inline u32
GetNavPolyPoints(engine_assets *Assets, u32 MapIndex, s32 Poly, v2 *Points, u32 MaxPoints)
{
    return(Engine.GetNavPolyPoints(Assets, MapIndex, Poly, Points, MaxPoints));
}
inline u32
FindNavPath(engine_assets *Assets, u32 MapIndex, v2 From, s32 FromLevel, v2 To, s32 ToLevel, nav_path_point *Points,
            u32 MaxPoints)
{
    return(Engine.FindNavPath(Assets, MapIndex, From, FromLevel, To, ToLevel, Points, MaxPoints));
}
inline v2 MoveOnNav(engine_assets *Assets, u32 MapIndex, v2 From, s32 *Level, v2 To) {return(Engine.MoveOnNav(Assets, MapIndex, From, Level, To));}
inline v2 ClosestNavPoint(engine_assets *Assets, u32 MapIndex, v2 P, s32 *Level) {return(Engine.ClosestNavPoint(Assets, MapIndex, P, Level));}
inline v2
ClosestNavPointOnLevel(engine_assets *Assets, u32 MapIndex, v2 P, s32 Level)
{
    return(Engine.ClosestNavPointOnLevel(Assets, MapIndex, P, Level));
}
inline u32 FindEntity(engine_assets *Assets, char *Name) {return(Engine.FindEntity(Assets, Name));}
inline u32 FindEntityByGUID(engine_assets *Assets, u64 GUID) {return(Engine.FindEntityByGUID(Assets, GUID));}
inline entity_info GetEntityInfo(engine_assets *Assets, u32 EntityIndex) {return(Engine.GetEntityInfo(Assets, EntityIndex));}
inline entity_clip_info
GetEntityClip(engine_assets *Assets, u32 EntityIndex, u32 ClipIndex, r32 Time = 0.0f)
{
    entity_clip_info Result = Engine.GetEntityClip(Assets, EntityIndex, ClipIndex, Time);
    return(Result);
}
// NOTE(pvlso): A clip of a definition by its name, -1 when it has none of that name
inline s32
GetEntityClipIndex(engine_assets *Assets, u32 EntityIndex, char *Name)
{
    s32 Result = -1;
    entity_info Info = GetEntityInfo(Assets, EntityIndex);
    for(u32 ClipIndex = 0; (Result < 0) && (ClipIndex < Info.ClipCount); ++ClipIndex)
    {
        if(StringsAreEqual(GetEntityClip(Assets, EntityIndex, ClipIndex).Name, Name)) {Result = (s32)ClipIndex;}
    }

    return(Result);
}
inline engine_font *GetFont(char *FileName) {return(Engine.GetFont(FileName));}

inline void
PushText(render_group *Group, object_transform *ObjectTransform, engine_font *Font, char *Text, v3 P, r32 Height,
         v4 Color = V4(1, 1, 1, 1), text_align Align = TextAlign_Left)
{
    Engine.PushText(Group, ObjectTransform, Font, Text, P, Height, Color, Align);
}

inline text_dim
GetTextDim(render_group *Group, engine_font *Font, char *Text, r32 Height)
{
    text_dim Result = Engine.GetTextDim(Group, Font, Text, Height);
    return(Result);
}
#endif
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#define ENGINE_API_H
#endif

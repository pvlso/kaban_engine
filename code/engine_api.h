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

    void (*PushBitmap)(render_group *Group, object_transform *ObjectTransform, bitmap_id ID, r32 Height, v3 Offset,
                       v4 Color, r32 CAlign, r32 Angle);
    void (*PushRect)(render_group *Group, object_transform *ObjectTransform, v3 Offset, v2 Dim, v4 Color);
    void (*PushLine)(render_group *Group, object_transform *ObjectTransform, v3 P0, v3 P1, v4 Color);
    void (*PushCircleOutline)(render_group *Group, object_transform *ObjectTransform, v3 Center, f32 Radius,
                              s32 Segments, v4 Color);

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
};

// NOTE(pvlso): Owned by the engine, so it survives hot reloads of the game code. The arenas
// start empty and grow through Platform.AllocateMemory, the engine frees them when the game stops.
struct game_memory
{
    memory_arena PermanentArena;
    memory_arena TransientArena;

    // NOTE(pvlso): The game's own state, usually pushed on the permanent arena on the first frame
    void *GameState;

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

inline void
PushBitmap(render_group *Group, object_transform *ObjectTransform, bitmap_id ID, r32 Height, v3 Offset,
           v4 Color = V4(1, 1, 1, 1), r32 CAlign = 1.0f, r32 Angle = 0.0f)
{
    Engine.PushBitmap(Group, ObjectTransform, ID, Height, Offset, Color, CAlign, Angle);
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

inline asset_best_match_result
GetBestMatchAssets(engine_assets *Assets, asset_tag_vector *Vector, memory_arena *Arena = 0)
{
    asset_best_match_result Result = Engine.GetBestMatchAssets(Assets, Vector, Arena);
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

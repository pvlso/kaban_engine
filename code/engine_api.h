#if !defined(ENGINE_API_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#include "engine_types.h"
#include "engine_defines.h"

struct bitmap_id;
struct nk_context;
struct engine_assets;
struct editor_render_commands;
struct engine_input;
struct game_state;

#include "engine_render_group.h"
#include "engine_intrinsics.h"
#include "engine_math.h"

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

struct engine_api
{
    render_group (*BeginRenderGroup)(engine_assets *Assets, editor_render_commands *Commands, u32 GenerationID, b32 RendersInBackground,
                                     s32 PixelWidth, s32 PixelHeight);
    void (*EndRenderGroup)(render_group *RenderGroup);
    void (*Perspective)(render_group *RenderGroup, real32 MetersToPixels, real32 FocalLength, real32 DistanceAboveTarget);
    void (*Orthographic)(render_group *RenderGroup, real32 MetersToPixels);
    void (*Clear)(render_group *Group, v4 Color);
    void (*PushBitmap)(render_group *Group, object_transform *ObjectTransform,
                       bitmap_id ID, real32 Height, v3 Offset, v4 Color, r32 CAlign);

    u32 (*BeginGeneration)(engine_assets *Assets);
    void (*EndGeneration)(engine_assets *Assets, u32 GenerationID);
    asset_best_match_result (*GetBestMatchAssets)(engine_assets *Assets, asset_tag_vector *Vector);

    
};

#define ARKHAM_UPDATE_AND_RENDER(name) b32 name(engine_api EngineAPI, engine_assets *Assets, \
                                                editor_render_commands *Commands, nk_context *Nk, \
                                                u32 RenderWidth, u32 RenderHeight)

typedef ARKHAM_UPDATE_AND_RENDER(arkham_update_and_render);

#define ENGINE_API_H
#endif

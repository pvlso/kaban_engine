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

#include "engine_render_group.h"
#include "engine_intrinsics.h"
#include "engine_math.h"

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
};

#define ARKHAM_UPDATE_AND_RENDER(name) void name(engine_api EngineAPI, engine_assets *Assets, editor_render_commands *Commands, \
                                                 nk_context *Nk, u32 RenderWidth, u32 RenderHeight)
typedef ARKHAM_UPDATE_AND_RENDER(arkham_update_and_render);

#define ENGINE_API_H
#endif

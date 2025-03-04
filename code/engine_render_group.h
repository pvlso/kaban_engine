#if !defined(EDITOR_RENDER_GROUP_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

/* NOTE:

   1) Everywhere outside the renderer, Y _always_ goes upward, X to the right.

   2) All bitmaps including the render target are assumed to be bottom-up
      (meaning that the first row pointer points to the bottom-most row
       when viewed on screen).

   3) It is mandatory that all inputs to the renderer are in world
      coordinates ("meters"), NOT pixels.  If for some reason something
      absolutely has to be specified in pixels, that will be explicitly
      marked in the API, but this should occur exceedingly sparingly.

   4) Z is a special coordinate because it is broken up into discrete slices,
      and the renderer actually understands these slices.  Z slices are what
      control the _scaling_ of things, whereas Z offsets inside a slice are
      what control Y offsetting.

   5) All color values specified to the renderer as V4's are in
      NON-premulitplied alpha.

*/

struct loaded_bitmap
{
    void *Memory;
    v2 AlignPercentage;
    r32 WidthOverHeight;    
    s32 Width;
    s32 Height;
    // TODO: Get rid of pitch!
    s32 Pitch;
    void *TextureHandle;
};

enum render_group_entry_type
{
    RenderGroupEntryType_render_entry_bitmap,
    RenderGroupEntryType_render_entry_rectangle,
    RenderGroupEntryType_render_entry_cliprect,
    RenderGroupEntryType_render_entry_line,
    RenderGroupEntryType_render_entry_triangle,
    RenderGroupEntryType_render_entry_blend_render_target,
};
struct render_group_entry_header // TODO(casey): Don't store type here, store in sort index?
{
    u16 Type;
    u16 ClipRectIndex;
};

struct render_entry_cliprect
{
    render_entry_cliprect *Next;
    rectangle2i Rect;
    u32 RenderTargetIndex;
};

struct render_entry_bitmap
{
    loaded_bitmap *Bitmap;
    
    v4 Color;
    v2 P;
    v2 Size;
};

struct render_entry_rectangle
{
    v4 Color;
    v2 P;
    v2 Dim;
};

struct render_entry_line
{
    v4 Color;
    v2 P0;
    v2 P1;
};

struct render_entry_triangle
{
    v4 Color;
    v2 A;
    v2 B;
    v2 C;
};

struct render_entry_blend_render_target
{
    u32 SourceTargetIndex;
    r32 Alpha;
};

struct object_transform
{
    // TODO(casey): Move this out to its own thang
    b32 Upright;
    v3 OffsetP;
    r32 Scale;
    s32 ChunkZ;

    v4 tColor;
    v4 Color;
};

struct camera_transform
{
    b32 Orthographic;

    // NOTE(casey): Camera parameters
    r32 MetersToPixels; // NOTE(casey): This translates meters _on the monitor_ into pixels _on the monitor_
    v2 ScreenCenter;

    r32 FocalLength;
    r32 DistanceAboveTarget;
};

struct render_group
{
    struct editor_assets *Assets; 
    real32 GlobalAlpha;

    rectangle2 ScreenArea;

    v2 MonitorHalfDimInMeters;

    camera_transform CameraTransform;

    uint32 MissingResourceCount;
    b32 RendersInBackground;

    u32 CurrentClipRectIndex;
    
    u32 GenerationID;
    editor_render_commands *Commands;
};

struct entity_basis_p_result
{
    v2 P;
    r32 Scale;
    b32 Valid;
    r32 SortKey;
};

struct used_bitmap_dim
{
    entity_basis_p_result Basis;
    v2 Size;
    v2 Align;
    v3 P;
};

struct push_buffer_result
{
    sort_sprite_bound *SortEntry;
    render_group_entry_header *Header;
};
                      
inline object_transform
DefaultUprightTransform(void)
{
    object_transform Result = {};

    Result.Upright = true;
    Result.Scale = 1.0f;

    return(Result);
}

inline object_transform
DefaultFlatTransform(void)
{
    object_transform Result = {};

    Result.Scale = 1.0f;

    return(Result);
}

#define EDITOR_RENDER_GROUP_H
#endif

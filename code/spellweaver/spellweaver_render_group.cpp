/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: Paul Solodrai  $
   $Notice: A large part of the code is borrowed from Handmade Hero series 
            that was created by Casey Muratori $
   ======================================================================== */

inline void
Perspective(render_group *RenderGroup, real32 MetersToPixels, real32 FocalLength, real32 DistanceAboveTarget)
{
    // TODO(casey): Need to adjust this based on buffer size
    real32 PixelsToMeters = SafeRatio1(1.0f, MetersToPixels);

    v2 ScreenPixelDim = GetDim(RenderGroup->ScreenArea);
    r32 PixelWidth = ScreenPixelDim.x;
    r32 PixelHeight = ScreenPixelDim.y;
    
    RenderGroup->MonitorHalfDimInMeters = {0.5f*PixelWidth*PixelsToMeters,
                                           0.5f*PixelHeight*PixelsToMeters};
    
    RenderGroup->CameraTransform.MetersToPixels = MetersToPixels;
    RenderGroup->CameraTransform.FocalLength =  FocalLength; // NOTE(casey): Meters the person is sitting from their monitor
    RenderGroup->CameraTransform.DistanceAboveTarget = DistanceAboveTarget;
    RenderGroup->CameraTransform.ScreenCenter = V2(0.5f*PixelWidth, 0.5f*PixelHeight);
    RenderGroup->CameraTransform.Orthographic = false;
}

inline void
Orthographic(render_group *RenderGroup, real32 MetersToPixels)
{
    real32 PixelsToMeters = SafeRatio1(1.0f, MetersToPixels);

    v2 ScreenPixelDim = GetDim(RenderGroup->ScreenArea);
    r32 PixelWidth = ScreenPixelDim.x;
    r32 PixelHeight = ScreenPixelDim.y;

    RenderGroup->MonitorHalfDimInMeters = {0.5f*PixelWidth*PixelsToMeters,
                                           0.5f*PixelHeight*PixelsToMeters};
    
    RenderGroup->CameraTransform.MetersToPixels = MetersToPixels;
    RenderGroup->CameraTransform.FocalLength =  1.0f; // NOTE(casey): Meters the person is sitting from their monitor
    RenderGroup->CameraTransform.DistanceAboveTarget = 1.0f;
    RenderGroup->CameraTransform.ScreenCenter = V2(0.5f*PixelWidth, 0.5f*PixelHeight);
    RenderGroup->CameraTransform.Orthographic = true;
}

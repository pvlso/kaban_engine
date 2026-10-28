/* ========================================================================
   $File: $
   $Date: 2026 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

/*
  NOTE(pvlso): The cut tool in the assets mode (map editor plan, phase 5a). The packs' sprites sit on big sheets, a
  bitmap asset is a whole image: this cuts them out. Any image under data/ opens on the canvas, panned with the middle
  button and zoomed with the wheel; a left drag selects a rectangle in pixels (in cells of Snap pixels when it isn't
  0), a right click clears it, the arrows move it by a pixel (Shift: by Snap, or 8). Saved as a PNG in bmps/ it's a
  static sprite; frames (all the same size) saved side by side in spritesheets/ are an animation. Frames are best cut
  by Split: a selection over the whole row, from the first frame's left to the last one's right, cut into equal frames,
  is the sheet's own pitch; added one by one, each frame is wherever the selection is. The files are then added as assets in Edit Bitmaps and
  Edit SpriteSheets, where their foot is set.
*/

// NOTE(pvlso): PNGs are written with stb_image_write, its allocations go through the platform. Only the cut tool uses it.
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_STATIC
#define STBIW_MALLOC(Size) Platform.AllocateMemory(Size)
#define STBIW_REALLOC_SIZED(Memory, OldSize, NewSize) Platform.ReallocateMemory(Memory, (u32)(OldSize), (u32)(NewSize))
#define STBIW_FREE(Memory) Platform.DeallocateMemory(Memory)
#define STBIW_ASSERT(Expression) do {Assert(Expression);} while(0)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include "stb_image_write.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

internal u32 CleanFileName(char *Name, char *Dest, u32 DestSize);
internal void RelistSourceFiles(editor_mode_assets *AssetsMode, kesa_type Type);

struct cut_png_write
{
    char *FileName;
    platform_file_type Type;
    b32 Written;
};

// NOTE(pvlso): stb_image_write hands a PNG over in one piece
internal void
WriteCutPNGBytes(void *Context, void *Data, int Size)
{
    cut_png_write *Write = (cut_png_write *)Context;
    Write->Written = (Platform.WriteEntireFile(Write->FileName, Write->Type, (u8 *)Data, (u32)Size) != 0);
}

internal b32
WriteCutPNG(char *FileName, platform_file_type Type, u8 *Pixels, s32 Width, s32 Height)
{
    cut_png_write Write = {FileName, Type, false};
    b32 Result = (stbi_write_png_to_func(WriteCutPNGBytes, &Write, Width, Height, 4, Pixels, 4*Width) && Write.Written);
    if(!Result)
    {
        LOG_ERROR("Can't write %s", FileName);
    }

    return(Result);
}

// NOTE(pvlso): Rect's pixels copied into Dest, whose rows are DestPitch bytes, at (DestX, 0)
internal void
CopyCutRect(cut_mode *CutMode, cut_rect Rect, u8 *Dest, s32 DestPitch, s32 DestX)
{
    for(s32 Y = 0; Y < Rect.H; ++Y)
    {
        u8 *SourceRow = CutMode->Pixels + 4*(CutMode->PixelsWidth*(Rect.Y + Y) + Rect.X);
        u8 *DestRow = Dest + DestPitch*Y + 4*DestX;
        Copy(4*Rect.W, SourceRow, DestRow);
    }
}

inline cut_rect
ClampCutRect(cut_rect Rect, s32 Width, s32 Height)
{
    cut_rect Result;
    Result.X = ClampCell(0, Rect.X, Maximum(0, Width - 1));
    Result.Y = ClampCell(0, Rect.Y, Maximum(0, Height - 1));
    Result.W = ClampCell(1, Rect.W, Maximum(1, Width - Result.X));
    Result.H = ClampCell(1, Rect.H, Maximum(1, Height - Result.Y));

    return(Result);
}

// NOTE(pvlso): The rectangle a drag from pixel A to pixel B covers, both included; whole cells of Snap pixels when
// Snap isn't 0
internal cut_rect
CutRectBetween(s32 AX, s32 AY, s32 BX, s32 BY, s32 Snap, s32 Width, s32 Height)
{
    s32 X0 = Minimum(AX, BX);
    s32 X1 = Maximum(AX, BX);
    s32 Y0 = Minimum(AY, BY);
    s32 Y1 = Maximum(AY, BY);
    if(Snap > 0)
    {
        X0 = (X0 / Snap)*Snap;
        Y0 = (Y0 / Snap)*Snap;
        X1 = (X1 / Snap)*Snap + Snap - 1;
        Y1 = (Y1 / Snap)*Snap + Snap - 1;
    }
    X1 = Minimum(X1, Width - 1);
    Y1 = Minimum(Y1, Height - 1);

    cut_rect Result = {X0, Y0, X1 - X0 + 1, Y1 - Y0 + 1};
    return(Result);
}

inline rectangle2
CutRectOnScreen(tileset_grid_view *View, cut_rect Rect)
{
    v2 Min = View->TopLeft + V2((r32)Rect.X*View->Cell, -(r32)(Rect.Y + Rect.H)*View->Cell);
    v2 Max = View->TopLeft + V2((r32)(Rect.X + Rect.W)*View->Cell, -(r32)Rect.Y*View->Cell);
    rectangle2 Result = RectMinMax(Min, Max);

    return(Result);
}

// NOTE(pvlso): The selection shrunk to the pixels in it that aren't fully transparent
internal void
TrimCutSelection(cut_mode *CutMode)
{
    cut_rect Rect = CutMode->Selection;
    s32 X0 = Rect.X + Rect.W, X1 = Rect.X - 1;
    s32 Y0 = Rect.Y + Rect.H, Y1 = Rect.Y - 1;
    for(s32 Y = Rect.Y; Y < Rect.Y + Rect.H; ++Y)
    {
        u8 *Pixel = CutMode->Pixels + 4*(CutMode->PixelsWidth*Y + Rect.X);
        for(s32 X = Rect.X; X < Rect.X + Rect.W; ++X, Pixel += 4)
        {
            if(Pixel[3])
            {
                X0 = Minimum(X0, X);
                X1 = Maximum(X1, X);
                Y0 = Minimum(Y0, Y);
                Y1 = Maximum(Y1, Y);
            }
        }
    }

    if(X1 >= X0)
    {
        cut_rect Trimmed = {X0, Y0, X1 - X0 + 1, Y1 - Y0 + 1};
        CutMode->Selection = Trimmed;
    }
    else
    {
        LOG_ERROR("Nothing to trim to: the selection is all transparent");
    }
}

internal void
ClearCutImage(platform_texture_op_queue *TextureOpQueue, cut_mode *CutMode)
{
    DeallocateBitmap(TextureOpQueue, &CutMode->Sheet);
    CutMode->Sheet = {};
    if(CutMode->Pixels)
    {
        stbi_image_free(CutMode->Pixels);
        CutMode->Pixels = 0;
    }
}

internal void
ClearCutMode(platform_texture_op_queue *TextureOpQueue, cut_mode *CutMode)
{
    ClearCutImage(TextureOpQueue, CutMode);
    Clear(&CutMode->Arena);
}

// NOTE(pvlso): Read twice: once to show it like every other image, once as it is in the file to cut it
internal void
OpenCutImage(platform_texture_op_queue *TextureOpQueue, cut_mode *CutMode, char *Path, rectangle2 ViewRect)
{
    ClearCutImage(TextureOpQueue, CutMode);
    CutMode->HasSelection = false;
    CutMode->Selecting = false;
    CutMode->FrameCount = 0;

    read_file_result File = Platform.ReadEntireFile(Path, PlatformFileType_DataRoot, 0, 0);
    u8 *Bytes = (u8 *)File.Contents;
    if((File.Size >= 8) && (Bytes[0] == 0x89) && (Bytes[1] == 'P') && (Bytes[2] == 'N') && (Bytes[3] == 'G'))
    {
        s32 Components = 0;
        CutMode->Pixels = stbi_load_from_memory(Bytes, (int)File.Size, &CutMode->PixelsWidth, &CutMode->PixelsHeight,
                                                &Components, 4);
    }
    if(File.Contents)
    {
        Platform.FreeFileMemory(File.Contents);
    }

    if(CutMode->Pixels)
    {
        CutMode->Sheet = LoadImageFile(Path, PlatformFileType_DataRoot, 0);
        if(CutMode->Sheet.Memory)
        {
            AllocateBitmap(TextureOpQueue, &CutMode->Sheet);
        }

        // NOTE(pvlso): The whole image fits the view when it opens
        v2 Dim = GetDim(ViewRect);
        CutMode->Zoom = Clamp(0.125f, Minimum(Dim.x / (r32)CutMode->PixelsWidth, Dim.y / (r32)CutMode->PixelsHeight),
                              4.0f);
        CutMode->Pan = V2(0, 0);
    }
    else
    {
        LOG_ERROR("The cut tool reads PNGs, can't read %s", Path);
    }
}

// NOTE(pvlso): Rect of the image drawn as large as fits in Box, in whole screen pixels per image pixel when it's larger
internal void
DrawCutPreview(render_group *RenderGroup, object_transform *Flat, cut_mode *CutMode, cut_rect Rect, rectangle2 Box)
{
    PushRect(RenderGroup, Flat, Box, 5.0f, V4(0.12f, 0.12f, 0.14f, 1.0f));
    v2 BoxDim = GetDim(Box) - V2(8.0f, 8.0f);
    r32 Scale = Minimum(BoxDim.x / (r32)Rect.W, BoxDim.y / (r32)Rect.H);
    if(Scale >= 1.0f)
    {
        Scale = (r32)FloorReal32ToInt32(Scale);
    }

    v2 BoxCenter = GetCenter(Box);
    v2 TopLeft = BoxCenter + V2(-Scale*((r32)Rect.X + 0.5f*(r32)Rect.W), Scale*((r32)Rect.Y + 0.5f*(r32)Rect.H));
    r32 Height = Scale*(r32)CutMode->Sheet.Height;
    v2 Center = TopLeft + V2(0.5f*Scale*(r32)CutMode->Sheet.Width, -0.5f*Height);

    u32 SavedClip = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat,
                                                     RectCenterDim(BoxCenter, Scale*V2((r32)Rect.W, (r32)Rect.H)), 0.0f, 0);
    PushBitmap(RenderGroup, Flat, &CutMode->Sheet, Height, V3(Center, 6.0f));
    RenderGroup->CurrentClipRectIndex = SavedClip;
}

// NOTE(pvlso): The image on the left: middle drag pans, the wheel zooms, a left drag selects, a right click clears
internal void
UpdateAndRenderCutView(cut_mode *CutMode, engine_input *Input, render_group *RenderGroup, object_transform *Flat,
                       v2 MouseP, rectangle2 Rect, r32 Wheel)
{
    loaded_bitmap *Sheet = &CutMode->Sheet;
    s32 Width = CutMode->PixelsWidth;
    s32 Height = CutMode->PixelsHeight;
    b32 Inside = IsInRectangle(Rect, MouseP);
    engine_button_state Left = Input->MouseButtons[PlatformMouseButton_Left];
    engine_button_state Middle = Input->MouseButtons[PlatformMouseButton_Middle];

    if(Inside)
    {
        ZoomGridView(&CutMode->Zoom, &CutMode->Pan, Rect, MouseP, Wheel, 0.125f, 48.0f);
        if(WasPressed(Middle)) {CutMode->Panning = true; CutMode->PanFrom = MouseP;}
    }
    if(CutMode->Panning)
    {
        CutMode->Pan += V2(MouseP.x - CutMode->PanFrom.x, CutMode->PanFrom.y - MouseP.y);
        CutMode->PanFrom = MouseP;
        if(!Middle.EndedDown) {CutMode->Panning = false;}
    }

    tileset_grid_view View = GridView(Rect, CutMode->Pan, CutMode->Zoom);
    s32 PixelX, PixelY;
    CellUnder(&View, MouseP, &PixelX, &PixelY);
    PixelX = ClampCell(0, PixelX, Maximum(0, Width - 1));
    PixelY = ClampCell(0, PixelY, Maximum(0, Height - 1));
    if(Inside && CutMode->Pixels)
    {
        if(WasPressed(Left))
        {
            CutMode->Selecting = true;
            CutMode->HasSelection = true;
            CutMode->AnchorX = PixelX;
            CutMode->AnchorY = PixelY;
        }
        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Right]))
        {
            CutMode->HasSelection = false;
        }
    }
    if(CutMode->Selecting)
    {
        CutMode->Selection = CutRectBetween(CutMode->AnchorX, CutMode->AnchorY, PixelX, PixelY, CutMode->Snap,
                                            Width, Height);
        if(!Left.EndedDown) {CutMode->Selecting = false;}
    }

    u32 SavedClip = RenderGroup->CurrentClipRectIndex;
    RenderGroup->CurrentClipRectIndex = PushClipRect(RenderGroup, Flat, Rect, 0.0f, 0);
    PushRect(RenderGroup, Flat, Rect, 5.0f, V4(0.18f, 0.18f, 0.2f, 1.0f));
    if(Sheet->Memory)
    {
        r32 ImageHeight = View.Cell*(r32)Sheet->Height;
        v2 Center = View.TopLeft + V2(0.5f*View.Cell*(r32)Sheet->Width, -0.5f*ImageHeight);
        PushBitmap(RenderGroup, Flat, Sheet, ImageHeight, V3(Center, 6.0f));
        cut_rect Whole = {0, 0, Width, Height};
        PushRectOutline(RenderGroup, Flat, CutRectOnScreen(&View, Whole), 7.0f, V4(1, 1, 1, 0.3f), 1.0f);

        // NOTE(pvlso): The pixels once they're large enough, the snap's cells
        if(View.Cell >= 8.0f)
        {
            DrawGridLines(RenderGroup, Flat, &View, 7.0f, V4(1, 1, 1, 0.06f));
        }
        if(CutMode->Snap > 0)
        {
            tileset_grid_view SnapView = GridView(Rect, CutMode->Pan, CutMode->Zoom*(r32)CutMode->Snap);
            DrawGridLines(RenderGroup, Flat, &SnapView, 7.0f, V4(1, 1, 1, 0.15f));
        }

        for(u32 FrameIndex = 0; FrameIndex < CutMode->FrameCount; ++FrameIndex)
        {
            PushRectOutline(RenderGroup, Flat, CutRectOnScreen(&View, CutMode->Frames[FrameIndex]), 8.0f,
                            V4(0, 0.8f, 1, 1), 1.0f);
        }
        if(CutMode->HasSelection)
        {
            PushRectOutline(RenderGroup, Flat, CutRectOnScreen(&View, CutMode->Selection), 8.0f,
                            V4(1, 0.85f, 0.2f, 1), 2.0f);
        }
        if(Inside && !CutMode->Selecting && (View.Cell >= 4.0f))
        {
            cut_rect Hover = {PixelX, PixelY, 1, 1};
            PushRectOutline(RenderGroup, Flat, CutRectOnScreen(&View, Hover), 8.0f, V4(1, 1, 1, 0.6f), 1.0f);
        }
    }
    RenderGroup->CurrentClipRectIndex = SavedClip;
}

internal b32
CutFileName(cut_mode *CutMode, platform_file_type Type, char *FileName, u32 FileNameSize)
{
    b32 Result = false;
    char Clean[64];
    if(!CleanFileName(CutMode->Name, Clean, sizeof(Clean)))
    {
        LOG_ERROR("The cut needs a name");
    }
    else
    {
        FormatString(FileNameSize, FileName, "%s.png", Clean);
        if(!CutMode->Overwrite && Platform.FileExists(FileName, Type))
        {
            LOG_ERROR("%s already exists, tick Overwrite to replace it", FileName);
        }
        else
        {
            Result = true;
        }
    }

    return(Result);
}

internal void
SaveCutBitmap(editor_mode_assets *AssetsMode, cut_mode *CutMode)
{
    char FileName[80];
    if(CutFileName(CutMode, PlatformFileType_BMP, FileName, sizeof(FileName)))
    {
        cut_rect Rect = CutMode->Selection;
        u8 *Pixels = (u8 *)Platform.AllocateMemory((umm)4*Rect.W*Rect.H);
        CopyCutRect(CutMode, Rect, Pixels, 4*Rect.W, 0);
        if(WriteCutPNG(FileName, PlatformFileType_BMP, Pixels, Rect.W, Rect.H))
        {
            RelistSourceFiles(AssetsMode, KESA_Bitmap);
            LOG("Cut bmps/%s, %d x %d, from %s", FileName, Rect.W, Rect.H, CutMode->Images[CutMode->ImageIndex]);
        }
        Platform.DeallocateMemory(Pixels);
    }
}

// NOTE(pvlso): The frames side by side, the strip a sprite sheet asset reads
internal void
SaveCutSpriteSheet(editor_mode_assets *AssetsMode, cut_mode *CutMode)
{
    char FileName[80];
    if(CutFileName(CutMode, PlatformFileType_SSBMP, FileName, sizeof(FileName)))
    {
        s32 FrameWidth = CutMode->Frames[0].W;
        s32 FrameHeight = CutMode->Frames[0].H;
        s32 StripWidth = FrameWidth*(s32)CutMode->FrameCount;
        u8 *Pixels = (u8 *)Platform.AllocateMemory((umm)4*StripWidth*FrameHeight);
        for(u32 FrameIndex = 0; FrameIndex < CutMode->FrameCount; ++FrameIndex)
        {
            CopyCutRect(CutMode, CutMode->Frames[FrameIndex], Pixels, 4*StripWidth, FrameWidth*(s32)FrameIndex);
        }
        if(WriteCutPNG(FileName, PlatformFileType_SSBMP, Pixels, StripWidth, FrameHeight))
        {
            RelistSourceFiles(AssetsMode, KESA_SpriteSheet);
            LOG("Cut spritesheets/%s, %u frames of %d x %d, from %s", FileName, CutMode->FrameCount, FrameWidth,
                FrameHeight, CutMode->Images[CutMode->ImageIndex]);
        }
        Platform.DeallocateMemory(Pixels);
    }
}

// NOTE(pvlso): The selection becomes the next frame, then moves on by its width (to the next row at the image's
// edge), where the next frame usually is
internal void
AddCutFrame(cut_mode *CutMode)
{
    cut_rect Rect = CutMode->Selection;
    if(CutMode->FrameCount >= CUT_MAX_FRAMES)
    {
        LOG_ERROR("An animation is cut from at most %d frames", CUT_MAX_FRAMES);
    }
    else if(CutMode->FrameCount && ((Rect.W != CutMode->Frames[0].W) || (Rect.H != CutMode->Frames[0].H)))
    {
        LOG_ERROR("Every frame is %d x %d like the first, the selection is %d x %d", CutMode->Frames[0].W,
                  CutMode->Frames[0].H, Rect.W, Rect.H);
    }
    else
    {
        CutMode->Frames[CutMode->FrameCount++] = Rect;
        if(Rect.X + 2*Rect.W <= CutMode->PixelsWidth)
        {
            CutMode->Selection.X += Rect.W;
        }
        else if(Rect.Y + 2*Rect.H <= CutMode->PixelsHeight)
        {
            CutMode->Selection.X = CutMode->Frames[0].X;
            CutMode->Selection.Y += Rect.H;
        }
    }
}

// NOTE(pvlso): The selection, a whole row of frames, cut into SplitCount equal frames that replace the animation's.
// Pixels left over when the width doesn't divide are dropped on the right.
internal void
SplitCutSelection(cut_mode *CutMode)
{
    cut_rect Rect = CutMode->Selection;
    s32 Count = ClampCell(1, CutMode->SplitCount, CUT_MAX_FRAMES);
    s32 FrameWidth = Rect.W / Count;
    if(FrameWidth < 1)
    {
        LOG_ERROR("The selection is %d pixels wide, too narrow for %d frames", Rect.W, Count);
    }
    else
    {
        if(Rect.W % Count)
        {
            LOG_WARNING("%d pixels don't split into %d frames: %d x %d each, %d pixels on the right left out", Rect.W,
                        Count, FrameWidth, Rect.H, Rect.W % Count);
        }
        CutMode->FrameCount = (u32)Count;
        for(s32 FrameIndex = 0; FrameIndex < Count; ++FrameIndex)
        {
            cut_rect Frame = {Rect.X + FrameIndex*FrameWidth, Rect.Y, FrameWidth, Rect.H};
            CutMode->Frames[FrameIndex] = Frame;
        }
    }
}

internal void
UpdateAndRenderCutEditMode(editor_mode_assets *AssetsMode, engine_input *Input, platform_texture_op_queue *TextureOpQueue,
                           render_group *RenderGroup, object_transform *Flat, v2 MouseP, rectangle2 CanvasRect,
                           nk_context *Nk)
{
    cut_mode *CutMode = &AssetsMode->CutMode;

    // NOTE(pvlso): The view on the left, the selection and the animation shown large on the right
    v2 Half = 0.5f*GetDim(CanvasRect);
    r32 PreviewSize = 252.0f;
    rectangle2 ViewRect = RectMinMax(V2(-Half.x, -Half.y), V2(Half.x - PreviewSize - 8.0f, Half.y));
    rectangle2 SelectionBox = RectMinMax(V2(Half.x - PreviewSize, Half.y - PreviewSize), V2(Half.x, Half.y));
    rectangle2 AnimationBox = RectMinMax(V2(Half.x - PreviewSize, Half.y - 2.0f*PreviewSize - 8.0f),
                                         V2(Half.x, Half.y - PreviewSize - 8.0f));

    if(!CutMode->Started)
    {
        CutMode->Started = true;
        CutMode->FramesPerSecond = 8.0f;
        CutMode->SplitCount = 4;
        CutMode->ImageCount = Platform.ListImagesUnderData(0, 0);
        CutMode->Images = PushArray(&CutMode->Arena, Maximum(1, CutMode->ImageCount), char *);
        Platform.ListImagesUnderData(CutMode->Images, &CutMode->Arena);
        CutMode->LastImageIndex = U32Maximum;
    }

    if(CutMode->ImageCount && (CutMode->ImageIndex != CutMode->LastImageIndex))
    {
        OpenCutImage(TextureOpQueue, CutMode, CutMode->Images[CutMode->ImageIndex], ViewRect);
        CutMode->LastImageIndex = CutMode->ImageIndex;
    }

    if(CutMode->Pixels)
    {
        if(CutMode->HasSelection && !CutMode->TypingName)
        {
            s32 Step = Input->ShiftDown ? ((CutMode->Snap > 0) ? CutMode->Snap : 8) : 1;
            if(nk_input_is_key_pressed(&Nk->input, NK_KEY_LEFT))  {CutMode->Selection.X -= Step;}
            if(nk_input_is_key_pressed(&Nk->input, NK_KEY_RIGHT)) {CutMode->Selection.X += Step;}
            if(nk_input_is_key_pressed(&Nk->input, NK_KEY_UP))    {CutMode->Selection.Y -= Step;}
            if(nk_input_is_key_pressed(&Nk->input, NK_KEY_DOWN))  {CutMode->Selection.Y += Step;}
        }
        if(CutMode->HasSelection)
        {
            CutMode->Selection = ClampCutRect(CutMode->Selection, CutMode->PixelsWidth, CutMode->PixelsHeight);
        }
        if(CutMode->TrimRequested && CutMode->HasSelection) {TrimCutSelection(CutMode);}
        if(CutMode->AddFrameRequested && CutMode->HasSelection) {AddCutFrame(CutMode);}
        if(CutMode->SplitRequested && CutMode->HasSelection) {SplitCutSelection(CutMode);}
        if(CutMode->RemoveFrameRequested && CutMode->FrameCount) {--CutMode->FrameCount;}
        if(CutMode->ClearFramesRequested) {CutMode->FrameCount = 0;}
        if(CutMode->SaveBitmapRequested)
        {
            if(CutMode->HasSelection) {SaveCutBitmap(AssetsMode, CutMode);}
            else {LOG_ERROR("Select the sprite to cut first");}
        }
        if(CutMode->SaveSheetRequested)
        {
            if(CutMode->FrameCount) {SaveCutSpriteSheet(AssetsMode, CutMode);}
            else {LOG_ERROR("Add the animation's frames first");}
        }
    }
    CutMode->TrimRequested = CutMode->AddFrameRequested = CutMode->SplitRequested = CutMode->RemoveFrameRequested = false;
    CutMode->ClearFramesRequested = CutMode->SaveBitmapRequested = CutMode->SaveSheetRequested = false;

    r32 Wheel = Nk->input.mouse.scroll_delta.y;
    UpdateAndRenderCutView(CutMode, Input, RenderGroup, Flat, MouseP, ViewRect, Wheel);

    if(CutMode->Sheet.Memory)
    {
        if(CutMode->HasSelection)
        {
            DrawCutPreview(RenderGroup, Flat, CutMode, CutMode->Selection, SelectionBox);
        }
        if(CutMode->FrameCount)
        {
            u32 FrameIndex = (u32)(AssetsMode->Time*CutMode->FramesPerSecond) % CutMode->FrameCount;
            DrawCutPreview(RenderGroup, Flat, CutMode, CutMode->Frames[FrameIndex], AnimationBox);
        }
    }
}

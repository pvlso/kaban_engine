/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

inline entity_basis_p_result
GetRenderEntityBasisP(camera_transform CameraTransform, object_transform *ObjectTransform, v3 OriginalP)
{
    entity_basis_p_result Result = {};

    v3 P = V3(OriginalP.xy, 0.0f) + ObjectTransform->OffsetP;

    if(CameraTransform.Orthographic)
    {
        Result.P = CameraTransform.ScreenCenter + CameraTransform.MetersToPixels*P.xy;
        Result.Scale = CameraTransform.MetersToPixels;
        Result.Valid = true;
    }
    else
    {
        real32 OffsetZ = 0.0f;
    
        real32 DistanceAboveTarget = CameraTransform.DistanceAboveTarget;
    
        real32 DistanceToPZ = (DistanceAboveTarget - P.z);
        real32 NearClipPlane = 0.1f;
    
        v3 RawXY = V3(P.xy, 1.0f);

        if(DistanceToPZ > NearClipPlane)
        {
            v3 ProjectedXY = (1.0f / DistanceToPZ) * CameraTransform.FocalLength*RawXY;        
            Result.Scale = CameraTransform.MetersToPixels*ProjectedXY.z;
            Result.P = CameraTransform.ScreenCenter + CameraTransform.MetersToPixels*ProjectedXY.xy +
                V2(0.0f, Result.Scale*OffsetZ);
            Result.Valid = true;
        }
    }

    Result.SortKey = 10.0f*ObjectTransform->ChunkZ + (4096.0f*(2.0f*P.z + OriginalP.z + 1.0f*(r32)ObjectTransform->Upright) - P.y);
    
    return(Result);
}

#define PushRenderElement(Group, type, SortKey) (type *)PushRenderElement_(Group, sizeof(type), RenderGroupEntryType_##type, SortKey)
inline void *
PushRenderElement_(render_group *Group, uint32 Size, render_group_entry_type Type, r32 SortKey)
{
    editor_render_commands *Commands = Group->Commands;
    
    void *Result = 0;

    Size += sizeof(render_group_entry_header);
    
    if((Commands->PushBufferSize + Size) < (Commands->SortEntryAt - sizeof(sort_entry)))
    {
        render_group_entry_header *Header = (render_group_entry_header *)(Commands->PushBufferBase + Commands->PushBufferSize);
        Header->Type = (u16)Type;
        Header->ClipRectIndex = SafeTruncateToU16(Group->CurrentClipRectIndex);
        Result = (uint8 *)Header + sizeof(*Header);

        Commands->SortEntryAt -= sizeof(sort_entry);
        sort_entry *Entry = (sort_entry *)(Commands->PushBufferBase + Commands->SortEntryAt);
        Entry->SortKey = SortKey;
        Entry->Index = Commands->PushBufferSize;

        Commands->PushBufferSize += Size;
        ++Commands->PushBufferElementCount;
    }
    else
    {
        InvalidCodePath;
    }

    return(Result);
}

inline used_bitmap_dim
GetBitmapDim(render_group *Group, object_transform *ObjectTransform,
             loaded_bitmap *Bitmap, real32 Height, v3 Offset, r32 CAlign)
{
    used_bitmap_dim Dim;
    
    Dim.Size = V2(Height*Bitmap->WidthOverHeight, Height);
    Dim.Align = CAlign*Hadamard(Bitmap->AlignPercentage, Dim.Size);
    Dim.P = Offset - V3(Dim.Align, 0);
    Dim.Basis = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, Dim.P);

    return(Dim);
}

inline v4
StoreColor(object_transform *Transform, v4 Source)
{
    v4 Dest;
    v4 t = Transform->tColor;
    v4 C = Transform->Color;

    Dest.a = Lerp(Source.a, t.a, C.a);

    Dest.r = Dest.a*Lerp(Source.r, t.r, C.r);
    Dest.g = Dest.a*Lerp(Source.g, t.g, C.g);
    Dest.b = Dest.a*Lerp(Source.b, t.b, C.b);

    return(Dest);
}

inline void
PushBitmap(render_group *Group, object_transform *ObjectTransform,
           loaded_bitmap *Bitmap, real32 Height, v3 Offset, v4 Color = V4(1, 1, 1, 1), r32 CAlign = 1.0f)
{
    used_bitmap_dim Dim = GetBitmapDim(Group, ObjectTransform, Bitmap, Height, Offset, CAlign);
    if(Dim.Basis.Valid)
    {
        v2 Size = Dim.Basis.Scale*Dim.Size;
        rectangle2 ScreenArea = RectMinDim(Dim.Basis.P, Size);
        render_entry_bitmap *Entry = PushRenderElement(Group, render_entry_bitmap, Dim.Basis.SortKey);
        if(Entry)
        {
            Entry->Bitmap = Bitmap;
            Entry->P = Dim.Basis.P;
            Entry->Color = Color;
            Entry->Size = Size;
        }
    }
}

inline void
PushBitmap(render_group *Group, object_transform *ObjectTransform,
    bitmap_id ID, real32 Height, v3 Offset, v4 Color = V4(1, 1, 1, 1), r32 CAlign = 1.0f)
{
    
    loaded_bitmap *Bitmap = GetBitmap(Group->Assets, ID, Group->GenerationID);
    if(Group->RendersInBackground && !Bitmap)
    {
        LoadAsset(Group->Assets, AssetType_Bitmap, ID.Value, true);
        Bitmap = GetBitmap(Group->Assets, ID, Group->GenerationID);
    }
    
    if(Bitmap)
    {
        PushBitmap(Group, ObjectTransform, Bitmap, Height, Offset, Color, CAlign);
    }
    else
    {
        Assert(!Group->RendersInBackground);
        LoadAsset(Group->Assets, AssetType_Bitmap, ID.Value, false);
        ++Group->MissingResourceCount;
    }
}

inline loaded_font *
PushFont(render_group *Group, font_id ID)
{
    loaded_font *Font = GetFont(Group->Assets, ID, Group->GenerationID);    
    if(Font)
    {
        // NOTE(casey): Nothing to do
    }
    else
    {
        Assert(!Group->RendersInBackground);
        LoadAsset(Group->Assets, AssetType_Font, ID.Value, false);
        ++Group->MissingResourceCount;
    }

    return(Font);
}

inline void
PushLine(render_group *Group, object_transform *ObjectTransform, v3 Point0, v3 Point1, v4 Color = V4(1, 1, 1, 1))
{
    v3 P0 = Point0;
    v3 P1 = Point1;
    entity_basis_p_result Basis0 = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P0);
    entity_basis_p_result Basis1 = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P1);
    if(Basis0.Valid && Basis1.Valid)
    {
        rectangle2 ScreenArea = RectMinMax(Basis0.P, Basis1.P);
        render_entry_line *Line = PushRenderElement(Group, render_entry_line, Basis0.SortKey);
        if(Line)
        {
            Line->P0 = Basis0.P;
            Line->P1 = Basis1.P;
            Line->Color = Color;
        }
    }
}

inline void
PushTriangle(render_group *Group, object_transform *ObjectTransform, v3 A, v3 B, v3 C, v4 Color = V4(1, 1, 1, 1))
{
    v3 P0 = A;
    v3 P1 = B;
    v3 P2 = C;
    entity_basis_p_result Basis0 = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P0);
    entity_basis_p_result Basis1 = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P1);
    entity_basis_p_result Basis2 = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P2);
    if(Basis0.Valid && Basis1.Valid)
    {
        render_entry_triangle *Triangle = PushRenderElement(Group, render_entry_triangle, Basis0.SortKey);
        if(Triangle)
        {
            Triangle->A = Basis0.P;
            Triangle->B = Basis1.P;
            Triangle->C = Basis2.P;
            Triangle->Color = Color;
        }
    }
}

inline void
PushTriangle(render_group *Group, object_transform *ObjectTransform, triangle Triangle, r32 Z, v4 Color = V4(1, 1, 1, 1))
{
    PushTriangle(Group, ObjectTransform, V3(Triangle.Vertices[0], Z), V3(Triangle.Vertices[1], Z), V3(Triangle.Vertices[2], Z), Color);
}

inline void
PushRect(render_group *Group, object_transform *ObjectTransform, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1))
{
    v3 P = (Offset - V3(0.5f*Dim, 0));
    entity_basis_p_result Basis = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P);
    if(Basis.Valid)
    {
        v2 ScaledDim = Basis.Scale*Dim;
        rectangle2 ScreenArea = RectMinDim(Basis.P, ScaledDim);
        render_entry_rectangle *Rect = PushRenderElement(Group, render_entry_rectangle, Basis.SortKey);
        if(Rect)
        {
            Rect->P = Basis.P;
            Rect->Color = Color;
            Rect->Dim = ScaledDim;
        }
    }
}

inline void
PushRect(render_group *Group, object_transform *ObjectTransform, rectangle2 Rectangle, r32 Z, v4 Color = V4(1, 1, 1, 1))
{
    PushRect(Group, ObjectTransform, V3(GetCenter(Rectangle), Z), GetDim(Rectangle), Color);
}

inline void
PushRectOutline(render_group *Group, object_transform *ObjectTransform, v3 Offset, v2 Dim, v4 Color = V4(1, 1, 1, 1), real32 Thickness = 0.1f)
{
    // NOTE(casey): Top and bottom
    PushRect(Group, ObjectTransform, Offset - V3(0, 0.5f*Dim.y, 0), V2(Dim.x - Thickness, Thickness), Color);
    PushRect(Group, ObjectTransform, Offset + V3(0, 0.5f*Dim.y, 0), V2(Dim.x - Thickness, Thickness), Color);

    // NOTE(casey): Left and right
    PushRect(Group, ObjectTransform, Offset - V3(0.5f*Dim.x, 0, 0), V2(Thickness, Dim.y + Thickness), Color);
    PushRect(Group, ObjectTransform, Offset + V3(0.5f*Dim.x, 0, 0), V2(Thickness, Dim.y + Thickness), Color);
}

inline void
PushRectOutline(render_group *Group, object_transform *ObjectTransform, rectangle2 Rectangle, r32 Z, v4 Color = V4(1, 1, 1, 1), real32 Thickness = 0.1f)
{
    PushRectOutline(Group, ObjectTransform, V3(GetCenter(Rectangle), Z), GetDim(Rectangle), Color, Thickness);
}

inline void
Clear(render_group *Group, v4 Color)
{
    Group->Commands->ClearColor = SRGB1ToLinear1(Color);
}

inline u32
PushClipRect(render_group *Group, u32 X, u32 Y, u32 W, u32 H, u32 RenderTargetIndex)
{
    u32 Result = 0;

    editor_render_commands *Commands = Group->Commands;
    
    u32 Size = sizeof(render_entry_cliprect);
    if((Commands->PushBufferSize + Size) < (Commands->SortEntryAt - sizeof(sort_entry)))
    {
        render_entry_cliprect *Rect = (render_entry_cliprect *)
            (Commands->PushBufferBase + Commands->PushBufferSize);
        Commands->PushBufferSize += Size;
        
        Result = Group->Commands->ClipRectCount++;
        
        if(Group->Commands->LastRect)
        {
            Group->Commands->LastRect = Group->Commands->LastRect->Next = Rect;
        }
        else
        {
            Group->Commands->LastRect = Group->Commands->FirstRect = Rect;
        }
        Rect->Next = 0;
        
        Rect->Rect.Min.x = X;
        Rect->Rect.Min.y = Y;
        Rect->Rect.Max.x = X + W;
        Rect->Rect.Max.y = Y + H;

        Rect->RenderTargetIndex = RenderTargetIndex;
        if(Group->Commands->MaxRenderTargetIndex < RenderTargetIndex)
        {
            Group->Commands->MaxRenderTargetIndex = RenderTargetIndex;
        }
    }

    return(Result);
}

inline u32
PushClipRect(render_group *Group, object_transform *ObjectTransform, v3 Offset, v2 Dim, u32 RenderTargetIndex)
{
    u32 Result = 0;
    
    v3 P = (Offset - V3(0.5f*Dim, 0));
    entity_basis_p_result Basis = GetRenderEntityBasisP(Group->CameraTransform, ObjectTransform, P);
    if(Basis.Valid)
    {
        v2 BasisP = Basis.P;
        v2 DimB = Basis.Scale*Dim;
        
        Result = PushClipRect(Group, RoundReal32ToInt32(BasisP.x), RoundReal32ToInt32(BasisP.y),
                              RoundReal32ToInt32(DimB.x), RoundReal32ToInt32(DimB.y), RenderTargetIndex);
    }
    
    return(Result);
}

inline u32
PushClipRect(render_group *Group, object_transform *ObjectTransform, rectangle2 Rectangle, r32 Z,
             u32 RenderTargetIndex)
{
    u32 Result = PushClipRect(Group, ObjectTransform, V3(GetCenter(Rectangle), Z), GetDim(Rectangle),
                              RenderTargetIndex);
    return(Result);
}

inline void
PushBlendRenderTarget(render_group *Group, r32 Alpha, u32 SourceRenderTargetIndex)
{
    r32 SortKey = Real32Maximum;
    render_entry_blend_render_target *Blend =
        PushRenderElement(Group, render_entry_blend_render_target, SortKey);

    Blend->SourceTargetIndex = SourceRenderTargetIndex;
    Blend->Alpha = Alpha;
}

inline loaded_tileset *
PushTileset(render_group *Group, tileset_id ID, b32 Immidiate = false)
{
    loaded_tileset *Tileset = GetTileset(Group->Assets, ID, Group->GenerationID);
    if((Group->RendersInBackground || Immidiate) && !Tileset)
    {
        LoadTileset(Group->Assets, ID, true);
        Tileset = GetTileset(Group->Assets, ID, Group->GenerationID);
    }
    
    if(Tileset)
    {
        // NOTE(casey): Nothing to do
    }
    else
    {
        Assert(!Group->RendersInBackground);
        LoadTileset(Group->Assets, ID, false);
        ++Group->MissingResourceCount;
    }

    return(Tileset);
}

inline loaded_world_map *
PushSSWM(editor_assets *Assets, u32 GenerationID, sswm_id ID, b32 Immidiate = false)
{
    loaded_world_map *SSWM = GetSSWM(Assets, ID, GenerationID);
    if(Immidiate && !SSWM)
    {
        LoadSSWM(Assets, ID, true);
        SSWM = GetSSWM(Assets, ID, GenerationID);
    }
    
    if(SSWM)
    {
        // NOTE(casey): Nothing to do
    }
    else
    {
        LoadSSWM(Assets, ID, false);
    }

    return(SSWM);
}

inline loaded_file *
PushFile(transient_state *TranState, file_id ID, b32 Immidiate = false)
{
    loaded_file *BinaryFile = GetBinaryFile(TranState->Assets, ID, TranState->MainGenerationID);
    if((Immidiate) && !BinaryFile)
    {
        LoadBinaryFile(TranState->Assets, ID, true);
        BinaryFile = GetBinaryFile(TranState->Assets, ID, TranState->MainGenerationID);
    }
    
    if(BinaryFile)
    {
        // NOTE(casey): Nothing to do
    }
    else
    {
        Assert(!Immidiate);
        LoadBinaryFile(TranState->Assets, ID, false);
    }

    return(BinaryFile);
}

inline loaded_spritesheet *
PushSpriteSheet(render_group *Group, spritesheet_id ID, b32 Immidiate = false)
{
    loaded_spritesheet *Spritesheet = GetSpriteSheet(Group->Assets, ID, Group->GenerationID);
    if((Group->RendersInBackground || Immidiate) && !Spritesheet)
    {
        LoadSpriteSheet(Group->Assets, ID, true);
        Spritesheet = GetSpriteSheet(Group->Assets, ID, Group->GenerationID);
    }
    
    if(Spritesheet)
    {
        // NOTE(casey): Nothing to do
    }
    else
    {
        Assert(!Group->RendersInBackground);
        LoadSpriteSheet(Group->Assets, ID, false);
        ++Group->MissingResourceCount;
    }

    return(Spritesheet);
}

inline loaded_text *
PushText(render_group *Group, text_id ID, b32 Immidiate = false)
{
    loaded_text *Text = GetText(Group->Assets, ID, Group->GenerationID);
    if((Group->RendersInBackground || Immidiate) && !Text)
    {
        LoadText(Group->Assets, ID, true);
        Text = GetText(Group->Assets, ID, Group->GenerationID);
    }
    
    if(Text)
    {
        // NOTE(casey): Nothing to do
    }
    else
    {
        Assert(!Group->RendersInBackground);
        LoadText(Group->Assets, ID, false);
        ++Group->MissingResourceCount;
    }

    return(Text);
}

inline v3
Unproject(render_group *Group, object_transform *ObjectTransform, v2 PixelsXY)
{
    camera_transform Transform = Group->CameraTransform;
    
    v2 UnprojectedXY;
    if(Transform.Orthographic)
    {
        UnprojectedXY = (1.0f / Transform.MetersToPixels)*(PixelsXY - Transform.ScreenCenter);
    }
    else
    {
        v2 A = (PixelsXY - Transform.ScreenCenter) * (1.0f / Transform.MetersToPixels);
        UnprojectedXY = ((Transform.DistanceAboveTarget - ObjectTransform->OffsetP.z)/Transform.FocalLength) * A; 
    }

    v3 Result = V3(UnprojectedXY, ObjectTransform->OffsetP.z);
    Result -= ObjectTransform->OffsetP;

    return(Result);
}

inline v2
UnprojectOld(render_group *Group, v2 ProjectedXY, real32 AtDistanceFromCamera)
{
    v2 WorldXY = (AtDistanceFromCamera / Group->CameraTransform.FocalLength)*ProjectedXY;
    return(WorldXY);
}

inline rectangle2
GetCameraRectangleAtDistance(render_group *Group, real32 DistanceFromCamera)
{
    v2 RawXY = UnprojectOld(Group, Group->MonitorHalfDimInMeters, DistanceFromCamera);

    rectangle2 Result = RectCenterHalfDim(V2(0, 0), RawXY);
    
    return(Result);
}

inline rectangle2
GetCameraRectangleAtTarget(render_group *Group)
{
    rectangle2 Result = GetCameraRectangleAtDistance(Group, Group->CameraTransform.DistanceAboveTarget);
    
    return(Result);
}

inline bool32
AllResourcesPresent(render_group *Group)
{
    bool32 Result = (Group->MissingResourceCount == 0);

    return(Result);
}

inline render_group
BeginRenderGroup(editor_assets *Assets, editor_render_commands *Commands, u32 GenerationID, b32 RendersInBackground,
                 s32 PixelWidth, s32 PixelHeight)
{
    render_group Result = {};

    Result.Assets = Assets;
    Result.RendersInBackground = RendersInBackground;
    Result.GlobalAlpha = 1.0f;
    Result.MissingResourceCount = 0;
    Result.GenerationID = GenerationID;
    Result.Commands = Commands;
    Result.ScreenArea = RectMinDim(V2(0, 0), V2(PixelWidth, PixelHeight));
    
    Result.CurrentClipRectIndex = PushClipRect(&Result, 0, 0, PixelWidth, PixelHeight, 0);
    
    return(Result);
}

inline void
EndRenderGroup(render_group *RenderGroup)
{
    // TODO(casey): 
    // RenderGroup->Commands->MissingResourceCount += RenderGroup->MissingResourceCount;
}

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

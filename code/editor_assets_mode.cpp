/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "editor_ssa_file_builder.cpp"
#include "engine_assets_mode_ui.cpp"

internal void
PlayAssetsMode(editor_state *EditorState, transient_state *TranState)
{
    SetEditorMode(EditorState, TranState, EditorMode_AssetsMode);
    
    editor_mode_assets *Result = PushStruct(&EditorState->ModeArena, editor_mode_assets);
    Result->EditMode = EditMode_None;
    SubArena(&Result->UtilityTempArena, &EditorState->ModeArena, Megabytes(1));
    SubArena(&Result->UtilityArena, &EditorState->ModeArena, Megabytes(1));

    // NOTE(babykaban): Setting it to one because first stored asset is always zero
    Result->ShowStoredAssetIndex = 1;

    Result->BitmapFileCount = Platform.ListFilesInDirectory(PlatformFileType_BMP, 0, 0);
    Result->BitmapFiles = PushArray(&EditorState->ModeArena, Result->BitmapFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_BMP, Result->BitmapFiles, &EditorState->ModeArena);

    Result->SpriteSheetFileCount = Platform.ListFilesInDirectory(PlatformFileType_SSBMP, 0, 0);
    Result->SpriteSheetFiles = PushArray(&EditorState->ModeArena, Result->SpriteSheetFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_SSBMP, Result->SpriteSheetFiles, &EditorState->ModeArena);

    Result->TilesetFileCount = Platform.ListFilesInDirectory(PlatformFileType_TSBMP, 0, 0);
    Result->TilesetFiles = PushArray(&EditorState->ModeArena, Result->TilesetFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_TSBMP, Result->TilesetFiles, &EditorState->ModeArena);

    Result->SolidTileFileCount = Platform.ListFilesInDirectory(PlatformFileType_STBMP, 0, 0);
    Result->SolidTileFiles = PushArray(&EditorState->ModeArena, Result->SolidTileFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_STBMP, Result->SolidTileFiles, &EditorState->ModeArena);

    Result->SoundFileCount = Platform.ListFilesInDirectory(PlatformFileType_WAV, 0, 0);
    Result->SoundFiles = PushArray(&EditorState->ModeArena, Result->SoundFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_WAV, Result->SoundFiles, &EditorState->ModeArena);

    Result->TextFileCount = Platform.ListFilesInDirectory(PlatformFileType_TXT, 0, 0);
    Result->TextFiles = PushArray(&EditorState->ModeArena, Result->TextFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_TXT, Result->TextFiles, &EditorState->ModeArena);

    Result->FontFileCount = Platform.ListFilesInDirectory(PlatformFileType_TTF, 0, 0);
    Result->FontFiles = PushArray(&EditorState->ModeArena, Result->FontFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_TTF, Result->FontFiles, &EditorState->ModeArena);

    Result->BinaryFileCount = Platform.ListFilesInDirectory(PlatformFileType_BIN, 0, 0);
    Result->BinaryFiles = PushArray(&EditorState->ModeArena, Result->BinaryFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_BIN, Result->BinaryFiles, &EditorState->ModeArena);

    Result->SSWMFileCount = Platform.ListFilesInDirectory(PlatformFileType_SSWM, 0, 0);
    Result->SSWMFiles = PushArray(&EditorState->ModeArena, Result->SSWMFileCount, char *);
    Platform.ListFilesInDirectory(PlatformFileType_SSWM, Result->SSWMFiles, &EditorState->ModeArena);

    Result->JsonStringsHead = ParseJson("enum_strings.json", &EditorState->ModeArena);

    EditorState->AssetsMode = Result;
}

internal void
CutSpriteSheet(editor_assets *Assets, spritesheet_mode *SpriteSheetMode,
               stored_asset_spritesheet *Asset)
{
    loaded_bitmap *SpriteSheet = &SpriteSheetMode->SpriteSheetBitmap;
    for(u32 SpriteIndex = 0;
        SpriteIndex < Asset->SpriteCount;
        ++SpriteIndex)
    {
        loaded_bitmap *Sprite = SpriteSheetMode->Sprites + SpriteIndex;
        Sprite->Width = Asset->SpriteWidth;
        Sprite->Height = Asset->SpriteHeight;
        Sprite->WidthOverHeight = (r32)Sprite->Width/(r32)Sprite->Height;
        Sprite->Pitch = Asset->SpriteWidth*BITMAP_BYTES_PER_PIXEL;
        Sprite->AlignPercentage = V2(0.5f, 0.5f);
        u32 MemorySize = Sprite->Pitch*Sprite->Height;
        Sprite->Memory = Platform.AllocateMemory(MemorySize);

        u8 *SourceRow = (u8 *)(SpriteSheet->Memory) + SpriteIndex*Sprite->Pitch;
        u8 *DestRow = (u8 *)(Sprite->Memory);
        for(s32 Y = 0;
            Y < Sprite->Height;
            ++Y)
        {
            u32 *Source = (u32 *)SourceRow;
            u32 *Dest = (u32 *)DestRow;
            for(s32 X = 0;
                X < Sprite->Width;
                ++X)
            {
                *Dest++ = *Source++;
            }

            SourceRow += SpriteSheet->Pitch;
            DestRow += Sprite->Pitch;
        }

        texture_op Op = {};
        Op.IsAllocate = true;
        Op.Allocate.Width = Sprite->Width;
        Op.Allocate.Height = Sprite->Height;
        Op.Allocate.Data = Sprite->Memory;
        Op.Allocate.ResultHandle = &Sprite->TextureHandle;
        AddOp(Assets->TextureOpQueue, &Op);
    }
}

internal void
LoadTileBitmap(editor_assets *Assets, tileset_mode *TilesetMode, stored_asset_tileset *Tileset,
               loaded_bitmap *TilesetBitmap, u32 TileIndex, loaded_bitmap *MergeTile = 0)
{
    loaded_bitmap *Tile = TilesetMode->Tiles + TileIndex;
    Tile->Width = Tileset->TileWidth;
    Tile->Height = Tileset->TileHeight;
    Tile->WidthOverHeight = (r32)Tile->Width/(r32)Tile->Height;
    Tile->AlignPercentage = V2(0.5f, 0.5f);
    Tile->Pitch = Tile->Width*BITMAP_BYTES_PER_PIXEL;
    u32 MemorySize = Tile->Height*Tile->Pitch;
    Tile->Memory = Platform.AllocateMemory(MemorySize);

    u8 *DestRow = (u8 *)Tile->Memory;

    u32 MainSurfaceIndexX = Tileset->TileOffsetsX[TileIndex]*Tile->Width;
    u32 MainSurfaceIndexY = Tileset->TileOffsetsY[TileIndex]*Tile->Height;
    u8 *MainSurfaceSource = (u8 *)TilesetBitmap->Memory + (MainSurfaceIndexY*TilesetBitmap->Pitch +
                                                          MainSurfaceIndexX*BITMAP_BYTES_PER_PIXEL);
    if(MergeTile)
    {
        u8 *MergeSurfaceSource = (u8 *)MergeTile->Memory;
        for(s32 Y = 0;
            Y < Tile->Height;
            ++Y)
        {
            u32 *MergeSource = (u32 *)MergeSurfaceSource;
            u32 *MainSource = (u32 *)MainSurfaceSource;
            u32 *Dest = (u32 *)DestRow;
            for(s32 X = 0;
                X < Tile->Width;
                ++X)
            {
                if(*MainSource)
                {
                    *Dest = *MainSource;
                }
                else
                {
                    *Dest = *MergeSource;
                }

                ++MainSource;
                ++MergeSource;
                ++Dest;
            }

            MainSurfaceSource += TilesetBitmap->Pitch;
            MergeSurfaceSource += MergeTile->Pitch;
            DestRow += Tile->Pitch;
        }
    }
    else
    {
        for(s32 Y = 0;
            Y < Tile->Height;
            ++Y)
        {
            u32 *MainSource = (u32 *)MainSurfaceSource;
            u32 *Dest = (u32 *)DestRow;
            for(s32 X = 0;
                X < Tile->Width;
                ++X)
            {
                *Dest = *MainSource;

                ++MainSource;
                ++Dest;
            }

            MainSurfaceSource += TilesetBitmap->Pitch;
            DestRow += Tile->Pitch;
        }
    }

    texture_op Op = {};
    Op.IsAllocate = true;
    Op.Allocate.Width = Tile->Width;
    Op.Allocate.Height = Tile->Height;
    Op.Allocate.Data = Tile->Memory;
    Op.Allocate.ResultHandle = &Tile->TextureHandle;
    AddOp(Assets->TextureOpQueue, &Op);
}

internal void
CutTileset(editor_assets *Assets, tileset_mode *TilesetMode, stored_asset_tileset *Asset, b32 CutWithMerge)
{
    stored_asset_tileset *Tileset = Asset;
    loaded_bitmap *TilesetBitmap = &TilesetMode->TilesetBitmap;
    u32 TileCountX = (TilesetBitmap->Width / Tileset->TileWidth);
    u32 TileCountY = (TilesetBitmap->Height / Tileset->TileHeight);
    Tileset->TileCount = TileCountX*TileCountY;

    u32 TileIndex = 0;
    for(u32 Y = 0;
        Y < TileCountY;
        ++Y)
    {
        for(u32 X = 0;
            X < TileCountX;
            ++X)
        {
            u32 *Pixel = (u32 *)(TilesetBitmap->Memory) + (Y*TilesetBitmap->Width*Tileset->TileHeight +
                                                           X*Tileset->TileWidth);
            if(*Pixel != 0xFFFF0000)
            {
                Tileset->TileOffsetsX[TileIndex] = X;
                Tileset->TileOffsetsY[TileIndex] = Y;
                ++TileIndex;
            }
            else
            {
                --Tileset->TileCount;
            }
        }
    }

    loaded_bitmap *MergeTile = 0;
    if(CutWithMerge)
    {
        MergeTile = &TilesetMode->MergeTileBitmap;
        Tileset->MergedTile = true;
    }
    else
    {
        Tileset->MergedTile = false;
    }
    
    for(u32 TileIndex = 0;
        TileIndex < Tileset->TileCount;
        ++TileIndex)
    {
        LoadTileBitmap(Assets, TilesetMode, Tileset, TilesetBitmap, TileIndex, MergeTile);
    }
}

internal void
AddTagToCurrentAsset(editor_mode_assets *AssetsMode, stored_asset *CurrentAsset)
{
    ssa_tag *Tag = CurrentAsset->AssetTags + CurrentAsset->TagCount++;
    Tag->ID = AssetsMode->CurrentTagID;
    Tag->Value = AssetsMode->CurrentTagValue;
}

internal void
AddCurrentAsset(editor_mode_assets *AssetsMode)
{
    AssetsMode->AssetsToAdd[AssetsMode->AddAssetCount].ID = AssetsMode->NextStoredAssetID;
    AssetsMode->AssetsToAdd[AssetsMode->AddAssetCount + 1] = AssetsMode->AssetsToAdd[AssetsMode->AddAssetCount];
    AssetsMode->AssetsToAdd[AssetsMode->AddAssetCount + 1].ID = 0;
    ++AssetsMode->AddAssetCount;
    ++AssetsMode->NextStoredAssetID;
}

internal void
RemoveCurrentAssetTag(editor_mode_assets *AssetsMode, stored_asset *CurrentAsset)
{
    if(CurrentAsset->TagCount)
    {
        for(u32 TagIndex = AssetsMode->CurrentTag;
            TagIndex < (ArrayCount(CurrentAsset->AssetTags) - 1);
            ++TagIndex)
        {
            CurrentAsset->AssetTags[TagIndex] = CurrentAsset->AssetTags[TagIndex + 1];        
        }

        ssa_tag *Tag = CurrentAsset->AssetTags + (ArrayCount(CurrentAsset->AssetTags) - 1);
        Tag->ID = 0;
        Tag->Value = 0;

        --CurrentAsset->TagCount;
    }
}

internal void
DeallocateBitmap(editor_assets *Assets, loaded_bitmap *Bitmap)
{
    if(Bitmap)
    {
        if(Bitmap->TextureHandle)
        {
            texture_op Op = {};
            Op.IsAllocate = false;
            Op.Deallocate.Handle = Bitmap->TextureHandle;
            AddOp(Assets->TextureOpQueue, &Op);
        }

        Platform.FreeFileMemory(Bitmap->Memory);
    }
}

internal void
AllocateBitmap(editor_assets *Assets, loaded_bitmap *Bitmap)
{
    texture_op Op = {};
    Op.IsAllocate = true;
    Op.Allocate.Width = Bitmap->Width;
    Op.Allocate.Height = Bitmap->Height;
    Op.Allocate.Data = Bitmap->Memory;
    Op.Allocate.ResultHandle = &Bitmap->TextureHandle;
    AddOp(Assets->TextureOpQueue, &Op);
}

inline void
DrawTileGrid(render_group *RenderGroup, object_transform *Transform, r32 HalfCanvasSize,
             r32 TileSize, r32 Z, v4 Color)
{
    for(r32 Y = HalfCanvasSize;
        Y > -HalfCanvasSize;
        Y -= TileSize)
    {
        PushLine(RenderGroup, Transform, V3(-HalfCanvasSize, Y, Z), V3(HalfCanvasSize, Y, Z), Color);
    }

    for(r32 X = HalfCanvasSize;
        X > -HalfCanvasSize;
        X -= TileSize)
    {
        PushLine(RenderGroup, Transform, V3(X, HalfCanvasSize, Z), V3(X, -HalfCanvasSize, Z), Color);
    }
}

inline v2
CalculateAlingment(v2 Point, u32 BitmapWidth, u32 BitmapHeight)
{
    v2 Result = V2(Point.x * (1.0f / (r32)(BitmapWidth - 1)),
                   Point.y * (1.0f / (r32)(BitmapHeight - 1)));

    return(Result);
}

inline void
DrawCanvasOutline(render_group *RenderGroup, object_transform *Transform, v2 CanvasDim, v2 Offset)
{
    PushRect(RenderGroup, Transform, V3(Offset, 4.0f), CanvasDim + V2(4.0f, 4.0f),
             V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1.0f));

    PushRect(RenderGroup, Transform, V3(Offset, 3.0f), CanvasDim + V2(8.0f, 8.0f),
             V4(0.403921568627f, 0.254901960784f, 0.172549019608f, 1.0f));

    PushRect(RenderGroup, Transform, V3(Offset, 2.0f), CanvasDim + V2(12.0f, 12.0f),
             V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1.0f));

    PushRect(RenderGroup, Transform, V3(Offset, 1.0f), CanvasDim + V2(16.0f, 16.0f),
             V4(0.725490196078f, 0.478431372549f, 0.341176470588f, 1.0f));

    PushRect(RenderGroup, Transform, V3(Offset, 0.0f), CanvasDim + V2(20.0f, 20.0f),
             V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1.0f));
}

inline void
ClearSpriteSheetSprites(editor_assets *Assets, spritesheet_mode *SpriteSheetMode)
{
    for(u32 SpriteIndex = 0;
        SpriteIndex < ArrayCount(SpriteSheetMode->Sprites);
        ++SpriteIndex)
    {
        loaded_bitmap *Sprite = SpriteSheetMode->Sprites + SpriteIndex;
        DeallocateBitmap(Assets, Sprite);
    }
                                
    ZeroArray(ArrayCount(SpriteSheetMode->Sprites), SpriteSheetMode->Sprites);
}

inline void
ClearTilesetTiles(editor_assets *Assets, tileset_mode *TilesetMode)
{
    for(u32 TileIndex = 0;
        TileIndex < ArrayCount(TilesetMode->Tiles);
        ++TileIndex)
    {
        loaded_bitmap *Tile = TilesetMode->Tiles + TileIndex;
        DeallocateBitmap(Assets, Tile);
    }
                                
    ZeroArray(ArrayCount(TilesetMode->Tiles), TilesetMode->Tiles);
}

inline assets_edit_mode
AssetsEditModeFromStoredType(u32 StoredType)
{
    assets_edit_mode Result = EditMode_None;
    switch(StoredType)
    {
        case StoredAssetType_None:        {}                               break;
        case StoredAssetType_Bitmap:      {Result = EditMode_Bitmap;}      break;
        case StoredAssetType_SpriteSheet: {Result = EditMode_SpriteSheet;} break;
        case StoredAssetType_Tileset:     {Result = EditMode_Tileset;}     break;
        case StoredAssetType_Sound:       {Result = EditMode_Sound;}       break;
        case StoredAssetType_Text:        {Result = EditMode_Text;}        break;
        case StoredAssetType_Font:        {Result = EditMode_Font;}        break;
        case StoredAssetType_File:        {Result = EditMode_File;}        break;
        case StoredAssetType_SSWM:        {Result = EditMode_SSWM;}        break;
        InvalidDefaultCase;
    }

    return(Result);
}

inline stored_asset_type
StoredAssetTypeFromEditMode(u32 EditMode)
{
    stored_asset_type Result = StoredAssetType_None;
    switch(EditMode)
    {
        case EditMode_None:        {}                                      break;
        case EditMode_Bitmap:      {Result = StoredAssetType_Bitmap;}      break;
        case EditMode_SpriteSheet: {Result = StoredAssetType_SpriteSheet;} break;
        case EditMode_Tileset:     {Result = StoredAssetType_Tileset;}     break;
        case EditMode_Sound:       {Result = StoredAssetType_Sound;}       break;
        case EditMode_Text:        {Result = StoredAssetType_Text;}        break;
        case EditMode_Font:        {Result = StoredAssetType_Font;}        break;
        case EditMode_File:        {Result = StoredAssetType_File;}        break;
        case EditMode_SSWM:        {Result = StoredAssetType_SSWM;}        break;
        InvalidDefaultCase;
    }

    return(Result);
}

internal void
ClearStoredAssetData(editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset_type Type,
                     audio_state *AudioState = 0)
{
//    Assert(Type != StoredAssetType_Font);
    switch(Type)
    {
        case StoredAssetType_None:
        {
        } break;

        case StoredAssetType_Bitmap:
        {
            // NOTE(paul): Clear Bitmap Mode data
            bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
            DeallocateBitmap(Assets, &BitmapMode->Bitmap);
        } break;

        case StoredAssetType_SpriteSheet:
        {
            // NOTE(paul): Clear SpriteSheet Mode data
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;

            DeallocateBitmap(Assets, &SpriteSheetMode->SpriteSheetBitmap);
            ClearSpriteSheetSprites(Assets, SpriteSheetMode);
        } break;

        case StoredAssetType_Tileset:
        {
            // NOTE(paul): Clear Tileset Mode data
            tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
            DeallocateBitmap(Assets, &TilesetMode->MergeTileBitmap);
            DeallocateBitmap(Assets, &TilesetMode->TilesetBitmap);

            ClearTilesetTiles(Assets, TilesetMode);
        } break;

        case StoredAssetType_Sound:
        {
            // NOTE(paul): Clear Sound Mode data 
            sound_mode *SoundMode = &AssetsMode->SoundMode;
            Assert(AudioState);
            MuteAndTerminateSound(AudioState, SoundMode->PlayingSound, 0.0f);
            Platform.DeallocateMemory(SoundMode->FreeSound);
        } break;
        
        case StoredAssetType_Text:
        {
            // NOTE(paul): Clear Text Mode Data
            text_mode *TextMode = &AssetsMode->TextMode;
            Platform.DeallocateMemory(TextMode->Text.String);
        } break;

        case StoredAssetType_Font:
        {
#if 0
            // NOTE(paul): Clear Font Mode Data
            font_mode *FontMode = &AssetsMode->FontMode;
            for(u32 GlyphIndex = 1;
                GlyphIndex < FontMode->Font.GlyphCount;
                ++GlyphIndex)
            {
                loaded_bitmap *Bitmap = FontMode->Font.Glyphs + GlyphIndex;
                if(Bitmap)
                {
                    DeallocateBitmap(Assets, Bitmap);
                }
            }

            Platform.DeallocateMemory(FontMode->Font.UnicodeCodePoints);
            Platform.DeallocateMemory(FontMode->Font.HorizontalAdvance);
            Platform.DeallocateMemory(FontMode->Font.UnicodeMap);
#endif
        } break;
        
        case StoredAssetType_File:
        {
            // NOTE(paul): Clear File Mode Data
            binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
            Platform.DeallocateMemory(FileMode->FileData);
        } break;
        
        case StoredAssetType_SSWM:
        {
            // NOTE(paul): Clear File Mode Data
            sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
            Platform.DeallocateMemory(SSWMMode->FileData);
        } break;
    }

    u32 ModeSizes[StoredAssetType_Count] =
        {
            sizeof(bitmap_mode), sizeof(spritesheet_mode), sizeof(tileset_mode),
            sizeof(sound_mode), sizeof(text_mode), sizeof(font_mode), sizeof(binary_file_mode),
            sizeof(sswm_mode),
        };

    u32 LargestModeSize = 0;
    for(u32 ModeIndex = 0;
        ModeIndex < StoredAssetType_Count;
        ++ModeIndex)
    {
        if(ModeSizes[ModeIndex] > LargestModeSize)
        {
            LargestModeSize = ModeSizes[ModeIndex];
        }
    }

    ZeroSize(LargestModeSize, &AssetsMode->BitmapMode);
}

inline void
ClearEditModeData(editor_state *EditorState, editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset *Asset)
{
    stored_asset_type Type = Asset->Type;

    AssetsMode->AssetsToAdd[AssetsMode->AddAssetCount] = {};
    AssetsMode->AddAsset = false;
    AssetsMode->RemoveTag = false;
    AssetsMode->AddTag = false;

    AssetsMode->LastTagID = 0;
    AssetsMode->CurrentTagID = 0;
    AssetsMode->CurrentTagValue = 0;
    AssetsMode->CurrentTag = 0;

    AssetsMode->FileIndex = 0;
    AssetsMode->LastFileIndex = 0;
    AssetsMode->SubFileIndex = 0;
    AssetsMode->LastSubFileIndex = 0;
    AssetsMode->PixelPosition = {};
    AssetsMode->Time = 0.0f;

    ClearStoredAssetData(AssetsMode, Assets, Type, &EditorState->AudioState);
}

inline b32
AbleToCut(u32 TilesetBitmapWidth, u32 TilesetBitmapHeight, stored_asset_tileset *Asset)
{
    b32 Result = ((Asset->TileWidth >= 32) && (Asset->TileHeight >= 32) &&
                  ((TilesetBitmapWidth % Asset->TileWidth) == 0) &&
                  ((TilesetBitmapHeight % Asset->TileHeight) == 0));

    return(Result);
}

inline void
LoadStoredAssetData(editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset *StoredAsset,
                    b32 RememberEditMode = false)
{
    if(RememberEditMode)
    {
        AssetsMode->EditMode = AssetsEditModeFromStoredType(StoredAsset->Type);
    }

//    Assert(StoredAsset->Type != StoredAssetType_Font);
    switch(StoredAsset->Type)
    {
        case StoredAssetType_None:
        {
        } break;
        
        case StoredAssetType_Bitmap:
        {
            bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
            stored_asset_bitmap *StoredBitmap = &StoredAsset->Bitmap;
    
            BitmapMode->Bitmap = LoadBMP(StoredBitmap->FileName, PlatformFileType_BMP, 0);
            loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
            AllocateBitmap(Assets, Bitmap);
        } break;

        case StoredAssetType_SpriteSheet:
        {
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
            stored_asset_spritesheet *SpriteSheet = &StoredAsset->SpriteSheet;

            SpriteSheetMode->SpriteSheetBitmap = LoadBMP(SpriteSheet->SourceFileName, PlatformFileType_SSBMP, 0);

            loaded_bitmap *Bitmap = &SpriteSheetMode->SpriteSheetBitmap;
            AllocateBitmap(Assets, Bitmap);
            CutSpriteSheet(Assets, SpriteSheetMode, SpriteSheet);
        } break;

        case StoredAssetType_Tileset:
        {
            tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
            stored_asset_tileset *StoredTileset = &StoredAsset->Tileset;
    
            TilesetMode->TilesetBitmap = LoadBMP(StoredTileset->SourceFileName, PlatformFileType_TSBMP, 0);
            loaded_bitmap *Bitmap = &TilesetMode->TilesetBitmap;
            AllocateBitmap(Assets, Bitmap);

            TilesetMode->MergeTileBitmap = LoadBMP(StoredTileset->MergeTileFileName, PlatformFileType_STBMP, 0);
            Bitmap = &TilesetMode->MergeTileBitmap;
            AllocateBitmap(Assets, Bitmap);

            if(AbleToCut(TilesetMode->TilesetBitmap.Width, TilesetMode->TilesetBitmap.Height, StoredTileset))
            {
                CutTileset(Assets, TilesetMode, StoredTileset, StoredTileset->MergedTile);
            }
        } break;

        case StoredAssetType_Sound:
        {
            sound_mode *SoundMode = &AssetsMode->SoundMode;
            stored_asset_sound *StoredSound = &StoredAsset->Sound;
            SoundMode->Sound = LoadWAV(StoredSound->SourceFileName, StoredSound->FirstSampleIndex, 0,
                                       &SoundMode->FreeSound, 0);
        } break;

        case StoredAssetType_Text:
        {
            text_mode *TextMode = &AssetsMode->TextMode;
            stored_asset_text *StoredText = &StoredAsset->Text;
            TextMode->Text = LoadText(StoredText->SourceFileName, 0);
        } break;

        case StoredAssetType_Font:
        {
#if 0
            font_mode *FontMode = &AssetsMode->FontMode;
            stored_asset_font *StoredFont = &StoredAsset->Font;
            u32 StandardFontSize = 24;

            FontMode->Font = Platform.LoadFontAsset(StoredFont->SourceFileName, StandardFontSize, 0);
            for(u32 GlyphIndex = 1;
                GlyphIndex < FontMode->Font.GlyphCount;
                ++GlyphIndex)
            {
                loaded_bitmap *Bitmap = FontMode->Font.Glyphs + GlyphIndex;
                AllocateBitmap(Assets, Bitmap);
            }
#endif
        } break;

        case StoredAssetType_File:
        {
            binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
            stored_asset_binary_file *StoredFile = &StoredAsset->File;
            read_file_result ReadResult =
                Platform.ReadEntireFile(StoredFile->SourceFileName, PlatformFileType_BIN, 0);    
            FileMode->FileData = (u8 *)ReadResult.Contents;
            StoredFile->FileSize = ReadResult.Size;
        } break;

        case StoredAssetType_SSWM:
        {
            sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
            stored_asset_sswm_file *StoredFile = &StoredAsset->SSWM;
            read_file_result ReadResult =
                Platform.ReadEntireFile(StoredFile->SourceFileName, PlatformFileType_SSWM, 0);    
            SSWMMode->FileData = (u8 *)ReadResult.Contents;
            StoredFile->FileSize = ReadResult.Size;
        } break;

        InvalidDefaultCase;
    }
}

internal void
LoadNewStoredAsset(editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset *Asset)
{
//    Assert(Asset->Type != StoredAssetType_Font);
    switch(Asset->Type)
    {
        case StoredAssetType_None:
        {
        } break;

        case StoredAssetType_Bitmap:
        {
            char *FileName = AssetsMode->BitmapFiles[AssetsMode->FileIndex];
            StringCopy(FileName, Asset->Bitmap.FileName);

            LoadStoredAssetData(AssetsMode, Assets, Asset);
            Asset->Bitmap.AlignPercentage = AssetsMode->BitmapMode.Bitmap.AlignPercentage;
        } break;

        case StoredAssetType_SpriteSheet:
        {
            char *FileName = AssetsMode->SpriteSheetFiles[AssetsMode->FileIndex];
            StringCopy(FileName, Asset->SpriteSheet.SourceFileName);

            LoadStoredAssetData(AssetsMode, Assets, Asset);
            Asset->SpriteSheet.SpriteHeight = AssetsMode->SpriteSheetMode.SpriteSheetBitmap.Height;
        } break;

        case StoredAssetType_Tileset:
        {
            char *FileName = AssetsMode->TilesetFiles[AssetsMode->FileIndex];
            StringCopy(FileName, Asset->Tileset.SourceFileName);

            char *SolidTileFileName = AssetsMode->SolidTileFiles[AssetsMode->SubFileIndex];
            StringCopy(SolidTileFileName, Asset->Tileset.MergeTileFileName);

            LoadStoredAssetData(AssetsMode, Assets, Asset);
            Asset->Tileset.TileWidth = Asset->Tileset.TileHeight = 32;
        } break;

        case StoredAssetType_Sound:
        {
            char *FileName = AssetsMode->SoundFiles[AssetsMode->FileIndex];
            StringCopy(FileName, Asset->Sound.SourceFileName);

            LoadStoredAssetData(AssetsMode, Assets, Asset);
        } break;

        case StoredAssetType_Text:
        {
            char *FileName = AssetsMode->TextFiles[AssetsMode->FileIndex];
            StringCopy(FileName, Asset->Text.SourceFileName);

            LoadStoredAssetData(AssetsMode, Assets, Asset);
        } break;

        case StoredAssetType_Font:
        {
#if 0
            char *FileName = AssetsMode->FontFiles[AssetsMode->FileIndex];
            StringCopy(FileName, Asset->Font.SourceFileName);

            LoadStoredAssetData(AssetsMode, Assets, Asset);
            font_mode *FontMode = &AssetsMode->FontMode;
            Asset->Font.CodePointCount = FontMode->Font.GlyphCount - 1;
            Asset->Font.FirstCodePoint = FontMode->Font.UnicodeCodePoints[1];
            Asset->Font.LastCodePoint = FontMode->Font.UnicodeCodePoints[FontMode->Font.GlyphCount - 1];
            Asset->Font.FontSizeInPixels = 24;
#endif
        } break;

        case StoredAssetType_File:
        {
            char *FileName = AssetsMode->BinaryFiles[AssetsMode->FileIndex];
            StringCopy(FileName, Asset->File.SourceFileName);
            LoadStoredAssetData(AssetsMode, Assets, Asset);
        } break;

        case StoredAssetType_SSWM:
        {
            char *FileName = AssetsMode->SSWMFiles[AssetsMode->FileIndex];
            StringCopy(FileName, Asset->SSWM.SourceFileName);
            LoadStoredAssetData(AssetsMode, Assets, Asset);
        } break;

        InvalidDefaultCase;
    }
}

inline void
InitAssetsMode(editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset *Asset)
{
    Asset->Type = StoredAssetTypeFromEditMode(AssetsMode->EditMode);
    LoadNewStoredAsset(AssetsMode, Assets, Asset);
}

inline void
UpdateAlignmentCursor(render_group *RenderGroup, object_transform *Flat, engine_input *Input,
                      v2 MouseP, rectangle2 CanvasRect, rectangle2 BitmapRect,
                      u32 BitmapPixelWidth, u32 BitmapPixelHeight, r32 Scale, v2 *Result)
{
    if(IsInRectangle(CanvasRect, MouseP))
    {
        v2 P = (MouseP - BitmapRect.Min - 0.5f*V2(Scale, Scale)) * (1.0f / Scale);
        s32 X = RoundReal32ToInt32(P.x);
        s32 Y = RoundReal32ToInt32(P.y);

        v2 CanvasCenter = GetCenter(BitmapRect);
        v2 PointP = BitmapRect.Min + Scale*V2i(X, Y);
        PushRect(RenderGroup, Flat, V3(PointP + 0.5f*V2(Scale, Scale), 8.0f), V2(Scale, Scale));

        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
        {
            *Result = CalculateAlingment(V2i(X, Y), BitmapPixelWidth, BitmapPixelHeight);
        }
    }
}

internal void
UpdateAndRenderBitmapEditMode(editor_mode_assets *AssetsMode, engine_input *Input, editor_assets *Assets,
                              render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                              rectangle2 CanvasRect, r32 TileDim, stored_asset *Asset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    v2 CanvasCenter = GetCenter(CanvasRect);
    r32 CanvasSize = CanvasDim.x;
    r32 HalfCanvasSize = 0.5f*CanvasSize;
    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, CanvasCenter);

    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
    stored_asset_bitmap *StoredBitmap = &Asset->Bitmap;
                
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, StoredAssetType_Bitmap);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }

    loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
    PushRect(RenderGroup, Flat, V3(0, 0, 5.0f), CanvasDim, V4(0.25f, 0.25f, 0.25f, 1.0f));
                
    r32 Scale = CalculateBitmapScaleForSquareCanvas(CanvasSize, Bitmap->Width, Bitmap->Height);
    r32 TileSize = Scale*TileDim;
    DrawTileGrid(RenderGroup, Flat, HalfCanvasSize, TileSize, 6.0f, V4(0, 1, 0, 1));

    r32 BitmapHeight = Scale*Bitmap->Height;
    r32 BitmapWidth = Bitmap->WidthOverHeight*BitmapHeight;
    rectangle2 BitmapRect = RectCenterDim(V2(0, 0), V2(BitmapWidth, BitmapHeight));

    UpdateAlignmentCursor(RenderGroup, Flat, Input, MouseP, CanvasRect, BitmapRect,
                          Bitmap->Width, Bitmap->Height, Scale, &StoredBitmap->AlignPercentage);

    PushBitmap(RenderGroup, Flat, Bitmap, BitmapHeight, V3(0, 0, 7.0f));
    PushRectOutline(RenderGroup, Flat, BitmapRect, 7.0f, V4(0, 0, 1, 1), 1.0f);
}

inline void
DrawSpriteOutlines(render_group *RenderGroup, object_transform *Flat, r32 Scale,
                   r32 SpriteWidthOverHeight, u32 SpritePixelHeight, u32 SpriteCount,
                   r32 SpriteSheetWidth)
{
    r32 SpriteHeight = Scale*SpritePixelHeight;
    r32 SpriteWidth = SpriteHeight*SpriteWidthOverHeight;
    v2 Dim = V2(SpriteWidth, SpriteHeight);
    v2 Offset = V2(-0.5f*SpriteSheetWidth + 0.5f*Dim.x, 0.0f);
    for(u32 SpriteIndex = 0;
        SpriteIndex < SpriteCount;
        ++SpriteIndex)
    {
        PushRectOutline(RenderGroup, Flat, V3(Offset, 10.0f), Dim, V4(1, 0, 0, 1), 1.0f);
        Offset.x += Dim.x;
    }
}

internal void
UpdateAndRenderSpriteSheetEditMode(editor_mode_assets *AssetsMode, engine_input *Input, editor_assets *Assets,
                                   render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                                   rectangle2 CanvasRect, r32 TileDim, stored_asset *CurrentAsset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    v2 CanvasCenter = GetCenter(CanvasRect);
    r32 CanvasSize = CanvasDim.x;
    r32 HalfCanvasSize = 0.5f*CanvasSize;
    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, CanvasCenter);

    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
    stored_asset_spritesheet *SpriteSheet = &CurrentAsset->SpriteSheet;

    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, StoredAssetType_SpriteSheet);
        SpriteSheet->SpriteCount = 0;

        LoadNewStoredAsset(AssetsMode, Assets, CurrentAsset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
        SpriteSheetMode->ShowAnimated = false;
    }

    loaded_bitmap *SpriteSheetBitmap = &SpriteSheetMode->SpriteSheetBitmap;
    if(SpriteSheetMode->CutSpriteSheet)
    {
        if(SpriteSheet->SpriteWidth >= 16)
        {
            if((SpriteSheetBitmap->Width % SpriteSheet->SpriteWidth) == 0)
            {
                ClearSpriteSheetSprites(Assets, SpriteSheetMode);

                SpriteSheet->SpriteCount = (SpriteSheetBitmap->Width / SpriteSheet->SpriteWidth);
                CutSpriteSheet(Assets, SpriteSheetMode, SpriteSheet);
            }
        }

        SpriteSheetMode->CutSpriteSheet = false;
    }
                
    PushRect(RenderGroup, Flat, V3(0, 0, 5.0f), CanvasDim, V4(0.25f, 0.25f, 0.25f, 1.0f));

    if(SpriteSheetMode->ShowAnimated)
    {
        if(SpriteSheet->SpriteCount)
        {
            u32 SpriteIndex = (FloorReal32ToInt32(AssetsMode->Time*SpriteSheet->SpriteCount) %
                               SpriteSheet->SpriteCount);

            loaded_bitmap *SpriteBitmap = SpriteSheetMode->Sprites + SpriteIndex;
                
            r32 Scale = CalculateBitmapScaleForSquareCanvas(CanvasSize, SpriteBitmap->Width, SpriteBitmap->Height);
            r32 TileSize = Scale*TileDim;
            DrawTileGrid(RenderGroup, Flat, HalfCanvasSize, TileSize, 6.0f, V4(0, 1, 0, 1));

            if(SpriteBitmap)
            {
                r32 SpriteHeight = Scale*SpriteBitmap->Height;
                r32 SpriteWidth = SpriteBitmap->WidthOverHeight*SpriteHeight;
                rectangle2 SpriteBitmapRect = RectCenterDim(V2(0, 0), V2(SpriteWidth, SpriteHeight));
                    
                PushBitmap(RenderGroup, Flat, SpriteBitmap, SpriteHeight, V3(0, 0, 7.0f));
                PushRectOutline(RenderGroup, Flat, SpriteBitmapRect, 7.0f, V4(0, 0, 1, 1), 1.0f);

                UpdateAlignmentCursor(RenderGroup, Flat, Input, MouseP, CanvasRect, SpriteBitmapRect,
                                      SpriteBitmap->Width, SpriteBitmap->Height, Scale,
                                      &SpriteSheet->SpriteAlignPercentage);
            }
        }
    }
    else
    {
        r32 Scale = CalculateBitmapScaleForSquareCanvas(CanvasSize, SpriteSheetBitmap->Width,
                                             SpriteSheetBitmap->Height);
        r32 TileSize = Scale*TileDim;
        DrawTileGrid(RenderGroup, Flat, HalfCanvasSize, TileSize, 6.0f, V4(0, 1, 0, 1));

        r32 Height = Scale*SpriteSheetBitmap->Height;
        r32 Width = SpriteSheetBitmap->WidthOverHeight*Height;
        rectangle2 BitmapRect = RectCenterDim(V2(0, 0), V2(Width, Height));
                    
        PushBitmap(RenderGroup, Flat, SpriteSheetBitmap, Height, V3(0, 0, 7.0f));
        PushRectOutline(RenderGroup, Flat, BitmapRect, 7.0f, V4(0, 0, 1, 1), 1.0f);

        DrawSpriteOutlines(RenderGroup, Flat, Scale, SpriteSheetMode->Sprites[0].WidthOverHeight,
                           SpriteSheet->SpriteHeight, SpriteSheet->SpriteCount, Width);
    }
}

inline void
DrawTileOutlines(render_group *RenderGroup, object_transform *Flat, r32 TilesetBitmapScale,
                 stored_asset_tileset *Tileset, r32 TileWidthOverHeight, v2 TilesetDim)
{
    r32 TileHeight = TilesetBitmapScale*Tileset->TileHeight;
    r32 TileWidth = TileHeight*TileWidthOverHeight;
    v2 Dim = V2(TileWidth, TileHeight);
    v2 Offset = V2(-0.5f*TilesetDim.x + 0.5f*Dim.x, -0.5f*TilesetDim.y + 0.5f*Dim.y);

    for(u32 TileIndex = 0;
        TileIndex < Tileset->TileCount;
        ++TileIndex)
    {
        r32 TileX = (r32)Tileset->TileOffsetsX[TileIndex]*TileWidth;
        r32 TileY = (r32)Tileset->TileOffsetsY[TileIndex]*TileHeight;
        PushRectOutline(RenderGroup, Flat, V3(Offset + V2(TileX, TileY), 10.0f), Dim, V4(1, 0, 0, 1), 1.0f);
    }
}

internal void
UpdateAndRenderTilesetEditMode(editor_mode_assets *AssetsMode, engine_input *Input, editor_assets *Assets,
                                   render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                                   rectangle2 CanvasRect, r32 TileDim, stored_asset *Asset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    v2 CanvasCenter = GetCenter(CanvasRect);
    r32 CanvasSize = CanvasDim.x;
    r32 HalfCanvasSize = 0.5f*CanvasSize;

    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, V2(0, 0));

    stored_asset_tileset *StoredTileset = &Asset->Tileset;
    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
    if((AssetsMode->FileIndex != AssetsMode->LastFileIndex) ||
       (AssetsMode->SubFileIndex != AssetsMode->LastSubFileIndex))
    {
        ClearStoredAssetData(AssetsMode, Assets, StoredAssetType_Tileset);
        StoredTileset->TileCount = 0;

        LoadNewStoredAsset(AssetsMode, Assets, Asset);

        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
        AssetsMode->LastSubFileIndex = AssetsMode->SubFileIndex;
        TilesetMode->ShowTiles = false;
        TilesetMode->CurrentTileIndex = 0;
    }

    loaded_bitmap *TilesetBitmap = &TilesetMode->TilesetBitmap;
    if(TilesetMode->CutTileset || TilesetMode->CutWithMergeTileset)
    {
        if(AbleToCut(TilesetBitmap->Width, TilesetBitmap->Height, StoredTileset))
        {
            ClearTilesetTiles(Assets, TilesetMode);
            CutTileset(Assets, TilesetMode, StoredTileset, TilesetMode->CutWithMergeTileset);
        }

        TilesetMode->CutTileset = false;
        TilesetMode->CutWithMergeTileset = false;
    }

    if(TilesetMode->ShowTiles)
    {
        loaded_bitmap *TileBitmap = TilesetMode->Tiles + TilesetMode->CurrentTileIndex;
        if(TileBitmap->Memory)
        {
            r32 TileCanvasSize = CanvasSize - 96.0f;
            r32 TileBitmapScale =
                CalculateBitmapScaleForSquareCanvas(TileCanvasSize, TileBitmap->Width, TileBitmap->Height);

            r32 TileHeight = TileBitmapScale*TileBitmap->Height;
            r32 TileWidth = TileBitmap->WidthOverHeight*TileHeight;
                    
            PushRect(RenderGroup, Flat, V3(0, 0, 5.0f), CanvasDim, UI_COLOR_RGBA1_EFE4B0FF);

            PushRect(RenderGroup, Flat, V3(0, 0, 6.0f), V2(TileWidth, TileHeight) + V2(4.0f, 4.0f),
                     UI_COLOR_RGBA1_4D3020FF);

            PushBitmap(RenderGroup, Flat, TileBitmap, TileHeight, V3(0, 0, 7.0f), V4(1, 1, 1, 1.0f));
        }
    }
    else
    {
        PushRect(RenderGroup, Flat, V3(0, 0, 5.0f), V2(CanvasSize, CanvasSize), V4(0.25f, 0.25f, 0.25f, 1.0f));

        r32 TilesetBitmapScale =
            CalculateBitmapScaleForSquareCanvas(CanvasSize, TilesetBitmap->Width, TilesetBitmap->Height);

        r32 TileSize = TilesetBitmapScale*TileDim;
        DrawTileGrid(RenderGroup, Flat, HalfCanvasSize, TileSize, 6.0f, V4(0, 1, 0, 1));

        r32 TilesetHeight = TilesetBitmapScale*TilesetBitmap->Height;
        r32 TilesetWidth = TilesetBitmap->WidthOverHeight*TilesetHeight;
        rectangle2 BitmapRect = RectCenterDim(V2(0, 0), V2(TilesetWidth, TilesetHeight));
                    
        PushBitmap(RenderGroup, Flat, TilesetBitmap, TilesetHeight, V3(0, 0, 7.0f));
        PushRectOutline(RenderGroup, Flat, BitmapRect, 7.0f, V4(0, 0, 1, 1), 1.0f);

        DrawTileOutlines(RenderGroup, Flat, TilesetBitmapScale, StoredTileset,
                         TilesetMode->Tiles[0].WidthOverHeight, V2(TilesetWidth, TilesetHeight));
    }
}

internal void
UpdateAndRenderSoundEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets,
                             audio_state *AudioState, stored_asset *Asset)
{
    sound_mode *SoundMode = &AssetsMode->SoundMode;
    stored_asset_sound *StoredSound = &Asset->Sound;
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, StoredAssetType_Sound, AudioState);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }

    if(SoundMode->PlaySound)
    {
        MuteAndTerminateSound(AudioState, SoundMode->PlayingSound, 0.0f);

        sound_id NullID = {};
        SoundMode->PlayingSound = PlaySound(AudioState, NullID, &SoundMode->Sound);

        SoundMode->PlaySound = false;
    }

    if(SoundMode->StopSound)
    {
        MuteAndTerminateSound(AudioState, SoundMode->PlayingSound, 0.0f);
        SoundMode->StopSound = false;
    }
}

internal void
UpdateAndRenderTextEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset *Asset)
{
    text_mode *TextMode = &AssetsMode->TextMode;
    stored_asset_text *StoredText = &Asset->Text;
                
    if((AssetsMode->FileIndex != AssetsMode->LastFileIndex) || TextMode->Reload)
    {
        ClearStoredAssetData(AssetsMode, Assets, StoredAssetType_Text);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);

        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
        TextMode->Reload = false;
    }

    if(TextMode->EditTextFile)
    {
        char *FileName = AssetsMode->TextFiles[AssetsMode->FileIndex];
        char CommandLine[512];
        FormatString(ArrayCount(CommandLine), CommandLine,
                     "openwithnotepad.bat txts/%s", FileName);
        Platform.DEBUGExecuteSystemCommand(0, 0, CommandLine);

        TextMode->EditTextFile = false;
    }
}

internal void
UpdateAndRenderFontEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset *Asset)
{
    font_mode *FontMode = &AssetsMode->FontMode;
    stored_asset_font *StoredFont = &Asset->Font;
                
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, StoredAssetType_Font);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);

        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
UpdateAndRenderFileEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset *Asset)
{
    stored_asset_binary_file *StoredFile = &Asset->File;
    binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
    
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, StoredAssetType_File);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
UpdateAndRenderSSWMEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets, stored_asset *Asset)
{
    stored_asset_sswm_file *StoredFile = &Asset->SSWM;
    sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
    
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, StoredAssetType_SSWM);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
ReadStoredAssets(editor_state *EditorState, editor_mode_assets *AssetsMode)
{
    stored_asset_file_header *StoredHeader = &AssetsMode->StoredHeader;

    working_version *Version = &EditorState->Version;
    char StoredFileName[256];
    FormatString(ArrayCount(StoredFileName), StoredFileName, "sseas\\stored_assets_%d.%d.%d.%d.ssea",
                 Version->MajorHigh, Version->MajorLow, Version->MinorHigh, Version->MinorLow);

    u32 FullVersion = (u32)((Version->MajorHigh << 24) | (Version->MajorLow << 16) | (Version->MinorHigh << 8) | Version->MinorLow);
    
    FILE *StoredAssetsFile;
    fopen_s(&StoredAssetsFile, StoredFileName, "rb");
    if(StoredAssetsFile)
    {
        fread(StoredHeader, sizeof(stored_asset_file_header), 1, StoredAssetsFile);

        Assert(StoredHeader->SizeOfStoredAsset == sizeof(stored_asset));
        Assert(StoredHeader->Version == FullVersion);
        Assert(StoredHeader->AssetCount);
        
        u32 AssetsSize = StoredHeader->AssetCount*StoredHeader->SizeOfStoredAsset;

        Platform.DeallocateMemory(AssetsMode->StoredAssets);
        AssetsMode->StoredAssets = (stored_asset *)Platform.AllocateMemory(AssetsSize);
        
        fread(AssetsMode->StoredAssets, StoredHeader->SizeOfStoredAsset,
              StoredHeader->AssetCount, StoredAssetsFile);
        
        fclose(StoredAssetsFile);
    }
    else
    {
        if(FullVersion == 0)
        {
            fopen_s(&StoredAssetsFile, StoredFileName, "wb");
            StoredHeader->SizeOfStoredAsset = sizeof(stored_asset);
            StoredHeader->Version = FullVersion;
            StoredHeader->AssetCount = 1;
            AssetsMode->StoredAssets = (stored_asset *)Platform.AllocateMemory(StoredHeader->SizeOfStoredAsset);

            fwrite(StoredHeader, sizeof(stored_asset_file_header), 1, StoredAssetsFile);

            stored_asset NullAsset = {};
            fwrite(&NullAsset, sizeof(stored_asset), 1, StoredAssetsFile);

            fclose(StoredAssetsFile);
        }
        else
        {
            Assert(!"No version found");
        }
    }

    AssetsMode->NextStoredAssetID = StoredHeader->AssetCount;
}

internal void
WriteStoredAssets(editor_state *EditorState, editor_mode_assets *AssetsMode)
{
    if(AssetsMode->AddAssetCount || AssetsMode->StoredAssetChanged)
    {
        char FileName[256];
        stored_asset_file_header *StoredHeader = &AssetsMode->StoredHeader;

        u32 FullVersion = UpdateEditorVersionFile(EditorState);
    
        FormatString(ArrayCount(FileName), FileName, "sseas\\stored_assets_%d.%d.%d.%d.ssea",
                     EditorState->Version.MajorHigh, EditorState->Version.MajorLow,
                     EditorState->Version.MinorHigh, EditorState->Version.MinorLow);

        u32 ExistingAssetsCount = StoredHeader->AssetCount;
        StoredHeader->AssetCount += AssetsMode->AddAssetCount;
        StoredHeader->Version = FullVersion;

        FILE *StoredAssetsFile;
        fopen_s(&StoredAssetsFile, FileName, "wb");

        fwrite(StoredHeader, sizeof(stored_asset_file_header), 1, StoredAssetsFile);

        if(ExistingAssetsCount)
        {
            fwrite(AssetsMode->StoredAssets, StoredHeader->SizeOfStoredAsset, ExistingAssetsCount, StoredAssetsFile);
        }

        fwrite(AssetsMode->AssetsToAdd, StoredHeader->SizeOfStoredAsset, AssetsMode->AddAssetCount, StoredAssetsFile);
        fclose(StoredAssetsFile);
    }

    AssetsMode->AddAssetCount = 0;
    ZeroArray(ArrayCount(AssetsMode->AssetsToAdd), AssetsMode->AssetsToAdd);
    ReadStoredAssets(EditorState, AssetsMode);
}

internal void
RemoveStoredAsset(editor_mode_assets *AssetsMode)
{
    if(AssetsMode->ShowStoredAssetIndex)
    {
        stored_asset *AssetToRemove = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
        *AssetToRemove = {};
        for(u32 StoredIndex = AssetsMode->ShowStoredAssetIndex;
            StoredIndex < AssetsMode->StoredHeader.AssetCount - 1;
            ++StoredIndex)
        {
            AssetsMode->StoredAssets[StoredIndex] = AssetsMode->StoredAssets[StoredIndex + 1];           
        }

        --AssetsMode->StoredHeader.AssetCount;
        if(AssetsMode->StoredHeader.AssetCount == 1)
        {
            AssetsMode->ShowStoredAssetIndex = 0;
        }

        if(AssetsMode->ShowStoredAssetIndex > AssetsMode->StoredHeader.AssetCount - 1)
        {
            AssetsMode->ShowStoredAssetIndex = AssetsMode->StoredHeader.AssetCount - 1;
        }
    }
}

internal b32
UpdateAndRenderAssetsMode(editor_state *EditorState, transient_state *TranState,
                          nk_context *Nk, render_group *RenderGroup,
                          engine_input *Input, u32 RenderWidth, u32 RenderHeight,
                          editor_mode_assets *AssetsMode)
{
    editor_assets *Assets = TranState->Assets;
    b32 Result = false;//CheckForMetaInput(EditorState, TranState, Input);
    if(!Result)
    {
        f32 Ratio = (f32)Nk->BaseHeight/(f32)Nk->BaseWidth;
        Orthographic(RenderGroup, Nk->Scale.x);
//        Perspective(RenderGroup, Ratio, 2.0f, 2.16f);
        Clear(RenderGroup, UI_COLOR_RGBA1_4D3020FF);

        object_transform Flat = DefaultFlatTransform();
        v2 MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;
        if(!AssetsMode->AssetsInitialized)
        {
            ReadStoredAssets(EditorState, AssetsMode);
            AssetsMode->AssetsInitialized = true;
        }

        if(!AssetsMode->Exit)
        {
            if(AssetsMode->LastEditMode != AssetsMode->EditMode)
            {
                AssetsMode->EditStoredAsset = false;

                ClearEditModeData(EditorState, AssetsMode, Assets, AssetsMode->CurrentAsset);
                AssetsMode->LastEditMode = AssetsMode->EditMode;            

                AssetsMode->CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
                if(AssetsMode->CurrentAsset->Type == StoredAssetType_None)
                {
                    InitAssetsMode(AssetsMode, Assets, AssetsMode->CurrentAsset);
                }
            }

            if(AssetsMode->RemoveTag)
            {
                RemoveCurrentAssetTag(AssetsMode, AssetsMode->CurrentAsset);
                AssetsMode->RemoveTag = false;
            }
        
            if(AssetsMode->AddTag)
            {
                AddTagToCurrentAsset(AssetsMode, AssetsMode->CurrentAsset);
                AssetsMode->AddTag = false;
            }

            if(AssetsMode->AddAsset)
            {
                AddCurrentAsset(AssetsMode);
                AssetsMode->CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
                AssetsMode->AddAsset = false;
            }
        
            r32 TileDim = 32.0f;
            r32 CanvasSize = 960.0f;
            r32 HalfCanvasSize = 0.5f*CanvasSize;
            v2 CanvasDim = V2(CanvasSize, CanvasSize);
            v2 CanvasHalfDim = 0.5f*V2(CanvasSize, CanvasSize);
            rectangle2 CanvasRect = RectCenterDim(V2(0, 0), CanvasDim);


            nk_ui *UI = &Platform.UI;
            switch(AssetsMode->EditMode)
            {
                case EditMode_None:
                {
                    if(AssetsMode->EditStoredAsset)
                    {
                        AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;

                        ClearStoredAssetData(AssetsMode, Assets, AssetsMode->CurrentAsset->Type,
                                             &EditorState->AudioState);
                        LoadStoredAssetData(AssetsMode, Assets, AssetsMode->CurrentAsset, true);

                        AssetsMode->LastEditMode = AssetsMode->EditMode;            
                        AssetsMode->StoredAssetChanged = true;
                    }
                    else if((AssetsMode->LastShowStoredAssetIndex != AssetsMode->ShowStoredAssetIndex) ||
                            AssetsMode->RemoveStoredAsset)
                    {
                        stored_asset *PreviousAsset = AssetsMode->StoredAssets + AssetsMode->LastShowStoredAssetIndex;
                        AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;

                        ClearStoredAssetData(AssetsMode, Assets, PreviousAsset->Type,
                                             &EditorState->AudioState);
                        LoadStoredAssetData(AssetsMode, Assets, AssetsMode->CurrentAsset);

                        AssetsMode->LastShowStoredAssetIndex = AssetsMode->ShowStoredAssetIndex;
                    }
                    
                    if(AssetsMode->WriteAssets)
                    {
                        WriteStoredAssets(EditorState, AssetsMode);
                        AssetsMode->StoredAssetChanged = false;
                        AssetsMode->WriteAssets = false;
                    }

                    if(AssetsMode->RemoveStoredAsset)
                    {
                        RemoveStoredAsset(AssetsMode);
                        AssetsMode->StoredAssetChanged = true;
                        AssetsMode->RemoveStoredAsset = false;
                    }

                    if(AssetsMode->WriteSSA)
                    {
                        temporary_memory TempMemory = BeginTemporaryMemory(&TranState->TranArena);
                        BuildSSAFile(AssetsMode, EditorState->Version, TempMemory.Arena);
                        EndTemporaryMemory(TempMemory);
                        AssetsMode->WriteSSA = false;
                    }

                } break;

                case EditMode_Bitmap:
                {
                    UpdateAndRenderBitmapEditMode(AssetsMode, Input, Assets, RenderGroup, &Flat, MouseP,
                                                  CanvasRect, TileDim, AssetsMode->CurrentAsset);
                } break;

                case EditMode_SpriteSheet:
                {
                    UpdateAndRenderSpriteSheetEditMode(AssetsMode, Input, Assets, RenderGroup, &Flat, MouseP,
                                                       CanvasRect, TileDim, AssetsMode->CurrentAsset);
                } break;

                case EditMode_Tileset:
                {
                    UpdateAndRenderTilesetEditMode(AssetsMode, Input, Assets, RenderGroup, &Flat, MouseP,
                                                   CanvasRect, TileDim, AssetsMode->CurrentAsset);
                } break;

                case EditMode_Sound:
                {
                    UpdateAndRenderSoundEditMode(AssetsMode, Assets, &EditorState->AudioState,
                                                 AssetsMode->CurrentAsset);
                } break;

                case EditMode_Text:
                {
                    UpdateAndRenderTextEditMode(AssetsMode, Assets, AssetsMode->CurrentAsset);
                } break;

                case EditMode_Font:
                {
                    UpdateAndRenderFontEditMode(AssetsMode, Assets, AssetsMode->CurrentAsset);
                } break;

                case EditMode_File:
                {
                    UpdateAndRenderFileEditMode(AssetsMode, Assets, AssetsMode->CurrentAsset);
                } break;

                case EditMode_SSWM:
                {
                    UpdateAndRenderSSWMEditMode(AssetsMode, Assets, AssetsMode->CurrentAsset);
                } break;
            
                InvalidDefaultCase;
            }

            DrawAssetsModeUI(AssetsMode, UI, Nk);


            AssetsMode->Time += Input->dtForFrame;
        }
        else
        {
            PlayTitleScreen(EditorState, TranState);
        }
    }

    return(Result);
}

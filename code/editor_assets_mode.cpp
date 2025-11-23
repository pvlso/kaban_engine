/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "editor_ssa_file_builder.cpp"
#include "engine_assets_mode_ui.cpp"

inline u64
rotl64(u64 x, s32 r)
{
    u64 Result = (x << r) | (x >> (64 - r));
    return(Result);
}

internal u64
xxhash64(void *input, size_t len, u64 seed)
{
    u8 *p = (u8 *)input;
    u8 *end = p + len;
    u64 Result = seed + len;

    while(p < end)
    {
        Result ^= (*p++);
        Result = rotl64(Result, 13);
        Result *= 0x9E3779B185EBCA87ULL;
        Result ^= (Result >> 7);
    }

    return(Result);
}

inline u64
GUIDFromString(char *s)
{
    size_t len = 0;
    while (s[len])
        len++;

    u64 Result = xxhash64(s, len, 0xDEADBEEFCAFEBABEULL);
    return(Result); 
}

inline void
FreeTagMap(editor_mode_assets *AssetsMode)
{
    if(AssetsMode->TagMapListHead)
    {
        tag_map_list *Current = 0;
        while(AssetsMode->TagMapListHead)
        {
            Current = AssetsMode->TagMapListHead->Next;
            Platform.DeallocateMemory(AssetsMode->TagMapListHead);
            AssetsMode->TagMapListHead = Current;
        }
    }
}

internal void
BuildTagMap(editor_mode_assets *AssetsMode, memory_arena *ModeArena, memory_arena *Arena)
{
    FreeTagMap(AssetsMode);

        temporary_memory TempMem = BeginTemporaryMemory(Arena);
    json_object *TagsObject = ParseJson("..\\kea_tags.json", TempMem.Arena, true);
    json_value *TagArray = JsonLookupObjectElement(TagsObject, "tag_array");

    AssetsMode->TagMapListCount = 1;
    AssetsMode->TagMapListHead = (tag_map_list *)Platform.AllocateMemory(sizeof(tag_map_list));
    tag_map_list *Current = 0;
    for(u32 I = 0; I < TagArray->Array.Count; ++I)
    {
        char *TagKey = TagArray->Array.Items[I]->String;
        json_value *TagObject = JsonLookupObjectElement(TagsObject, TagKey);
        json_value *TagGUID = JsonLookupObjectElement(&TagObject->Object, "guid");
        json_value *TagValueArray = JsonLookupObjectElement(&TagObject->Object, "values");

        if(I == 0)
        {
            Assert(TagGUID->Int == 0);
            Assert(TagValueArray->Array.Count == 1);

            AssetsMode->TagMapListHead->Tag.GUID = TagGUID->Int;            
            AssetsMode->TagMapListHead->Tag.ValueCount = 1;            
            StringCopy(TagKey, AssetsMode->TagMapListHead->Tag.Key);
            StringCopy(TagValueArray->Array.Items[0]->String,
                       AssetsMode->TagMapListHead->Tag.Values[0]);
        }
        else
        {
            Current = (tag_map_list *)Platform.AllocateMemory(sizeof(tag_map_list));
            if(TagGUID->Int == 0)
                Current->Tag.GUID = GUIDFromString(TagKey);
            else
                Current->Tag.GUID = TagGUID->Int;            

            StringCopy(TagKey, Current->Tag.Key);
            Current->Tag.ValueCount = TagValueArray->Array.Count;
            for(u32 J = 0; J < TagValueArray->Array.Count; ++J)
            {
                StringCopy(TagValueArray->Array.Items[J]->String,
                           Current->Tag.Values[J]);
            }

            Current->Next = AssetsMode->TagMapListHead->Next;
            AssetsMode->TagMapListHead->Next = Current; 
            ++AssetsMode->TagMapListCount;
        }
    }

    EndTemporaryMemory(TempMem);
}

internal void
PlayAssetsMode(editor_state *EditorState, transient_state *TranState)
{
    SetEditorMode(EditorState, TranState, EditorMode_AssetsMode);
    
    editor_mode_assets *Result = PushStruct(&EditorState->ModeArena, editor_mode_assets);
    Result->EditMode = EditMode_None;
    SubArena(&Result->UtilityTempArena, &EditorState->ModeArena, Megabytes(1));
    SubArena(&Result->UtilityArena, &EditorState->ModeArena, Megabytes(1));

    // NOTE(pvlso): Setting it to one because first stored asset is always zero
    Result->ShowStoredAssetIndex = 1;

    for(u32 I = 1; I < KESA_Count; ++I)
    {
        Result->SourceFileCounts[I] =
            Platform.ListFilesInDirectory(StoredToSourceTypeMap[I], 0, 0);
        if(Result->SourceFileCounts[I])
        {
            Result->SourceFiles[I] = PushArray(&EditorState->ModeArena, Result->SourceFileCounts[I], char *);
            Platform.ListFilesInDirectory(StoredToSourceTypeMap[I], Result->SourceFiles[I], &EditorState->ModeArena);
        }

        if(I == KESA_Tileset)
        {
            Result->SolidTileFileCount = Platform.ListFilesInDirectory(PlatformFileType_STBMP, 0, 0);
            if(Result->SolidTileFileCount)
            {
                Result->SolidTileFiles = PushArray(&EditorState->ModeArena, Result->SolidTileFileCount, char *);
                Platform.ListFilesInDirectory(PlatformFileType_STBMP, Result->SolidTileFiles, &EditorState->ModeArena);
            }
        }
    }

    
    BuildTagMap(Result, &EditorState->ModeArena, &TranState->TranArena);
    BuildTagMap(Result, &EditorState->ModeArena, &TranState->TranArena);

    EditorState->AssetsMode = Result;
}

internal void
CutSpriteSheet(editor_assets *Assets, spritesheet_mode *SpriteSheetMode,
               kesa_spritesheet *Asset)
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
LoadTileBitmap(editor_assets *Assets, tileset_mode *TilesetMode, kesa_tileset *Tileset,
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
CutTileset(editor_assets *Assets, tileset_mode *TilesetMode, kesa_tileset *Asset, b32 CutWithMerge)
{
    kesa_tileset *Tileset = Asset;
    loaded_bitmap *TilesetBitmap = &TilesetMode->TilesetBitmap;
    u32 TileCountX = (TilesetBitmap->Width / Tileset->TileWidth);
    u32 TileCountY = (TilesetBitmap->Height / Tileset->TileHeight);
    Tileset->TileCount = TileCountX*TileCountY;

    u32 TileIndex = 0;
    for(u8 Y = 0;
        Y < TileCountY;
        ++Y)
    {
        for(u8 X = 0;
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
AddTagToCurrentAsset(editor_mode_assets *AssetsMode, kesa_asset *CurrentAsset)
{
    kea_tag *Tag = CurrentAsset->AssetTags + CurrentAsset->TagCount++;
    tag_map_list *Map = GetTagMapByIndex(AssetsMode, AssetsMode->CurrentTagID);

    StringCopy(Map->Tag.Key, Tag->Key);
    StringCopy(Map->Tag.Values[AssetsMode->CurrentTagValue], Tag->Value);

    char Buffer[256];
    FormatString(ArrayCount(Buffer), Buffer, "%s%s", Tag->Key, Tag->Value);
    Tag->GUID = GUIDFromString(Buffer);
}

internal void
AddCurrentAsset(editor_mode_assets *AssetsMode)
{
    AssetsMode->AssetsToAdd[AssetsMode->AddAssetCount].GUID = GUIDFromString(AssetsMode->CurrentAsset->SourceFileName);
    ++AssetsMode->AddAssetCount;
}

internal void
RemoveCurrentAssetTag(editor_mode_assets *AssetsMode, kesa_asset *CurrentAsset)
{
    if(CurrentAsset->TagCount)
    {
        for(u32 TagIndex = AssetsMode->CurrentTag;
            TagIndex < (ArrayCount(CurrentAsset->AssetTags) - 1);
            ++TagIndex)
        {
            CurrentAsset->AssetTags[TagIndex] = CurrentAsset->AssetTags[TagIndex + 1];        
        }

        kea_tag *Tag = CurrentAsset->AssetTags + (ArrayCount(CurrentAsset->AssetTags) - 1);
        Tag->GUID = 0;
        ZeroSize(StringLength(Tag->Key), Tag->Key);
        ZeroSize(StringLength(Tag->Value), Tag->Value);

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

internal void
ClearStoredAssetData(editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_type Type,
                     audio_state *AudioState = 0)
{
    switch(Type)
    {
        case KESA_None:
        {
        } break;

        case KESA_Bitmap:
        {
            // NOTE(paul): Clear Bitmap Mode data
            bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
            DeallocateBitmap(Assets, &BitmapMode->Bitmap);
        } break;

        case KESA_SpriteSheet:
        {
            // NOTE(paul): Clear SpriteSheet Mode data
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;

            DeallocateBitmap(Assets, &SpriteSheetMode->SpriteSheetBitmap);
            ClearSpriteSheetSprites(Assets, SpriteSheetMode);
        } break;

        case KESA_Tileset:
        {
            // NOTE(paul): Clear Tileset Mode data
            tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
            DeallocateBitmap(Assets, &TilesetMode->MergeTileBitmap);
            DeallocateBitmap(Assets, &TilesetMode->TilesetBitmap);

            ClearTilesetTiles(Assets, TilesetMode);
        } break;

        case KESA_Sound:
        {
            // NOTE(paul): Clear Sound Mode data 
            sound_mode *SoundMode = &AssetsMode->SoundMode;
            Assert(AudioState);
            MuteAndTerminateSound(AudioState, SoundMode->PlayingSound, 0.0f);
            Platform.DeallocateMemory(SoundMode->FreeSound);
        } break;
        
        case KESA_Text:
        {
            // NOTE(paul): Clear Text Mode Data
            text_mode *TextMode = &AssetsMode->TextMode;
            Platform.DeallocateMemory(TextMode->Text.String);
        } break;

        case KESA_Font:
        {
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
        } break;
        
        case KESA_File:
        {
            // NOTE(paul): Clear File Mode Data
            binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
            Platform.DeallocateMemory(FileMode->FileData);
        } break;
        
        case KESA_SSWM:
        {
            // NOTE(paul): Clear File Mode Data
            sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
            Platform.DeallocateMemory(SSWMMode->FileData);
        } break;
    }

    u32 ModeSizes[KESA_Count] =
        {
            sizeof(bitmap_mode), sizeof(spritesheet_mode), sizeof(tileset_mode),
            sizeof(sound_mode), sizeof(text_mode), sizeof(font_mode), sizeof(binary_file_mode),
            sizeof(sswm_mode),
        };

    u32 LargestModeSize = 0;
    for(u32 ModeIndex = 0;
        ModeIndex < KESA_Count;
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
ClearEditModeData(editor_state *EditorState, editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_asset *Asset)
{
    kesa_type Type = Asset->Type;

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
AbleToCut(u32 TilesetBitmapWidth, u32 TilesetBitmapHeight, kesa_tileset *Asset)
{
    b32 Result = ((Asset->TileWidth >= 32) && (Asset->TileHeight >= 32) &&
                  ((TilesetBitmapWidth % Asset->TileWidth) == 0) &&
                  ((TilesetBitmapHeight % Asset->TileHeight) == 0));

    return(Result);
}

inline void
LoadStoredAssetData(editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_asset *StoredAsset,
                    b32 RememberEditMode = false)
{
    if(RememberEditMode)
    {
        AssetsMode->EditMode = AssetsEditModeFromStoredType(StoredAsset->Type);
    }

    switch(StoredAsset->Type)
    {
        case KESA_None:
        {
        } break;
        
        case KESA_Bitmap:
        {
            bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
            kesa_bitmap *StoredBitmap = &StoredAsset->Bitmap;
    
            BitmapMode->Bitmap = LoadBMP(StoredAsset->SourceFileName, PlatformFileType_BMP, 0);
            if(BitmapMode->Bitmap.Memory)
            {
                loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
                AllocateBitmap(Assets, Bitmap);
            }
        } break;

        case KESA_SpriteSheet:
        {
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
            kesa_spritesheet *SpriteSheet = &StoredAsset->SpriteSheet;

            SpriteSheetMode->SpriteSheetBitmap = LoadBMP(StoredAsset->SourceFileName, PlatformFileType_SSBMP, 0);

            loaded_bitmap *Bitmap = &SpriteSheetMode->SpriteSheetBitmap;
            AllocateBitmap(Assets, Bitmap);
            CutSpriteSheet(Assets, SpriteSheetMode, SpriteSheet);
        } break;

        case KESA_Tileset:
        {
            tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
            kesa_tileset *StoredTileset = &StoredAsset->Tileset;
    
            TilesetMode->TilesetBitmap = LoadBMP(StoredAsset->SourceFileName, PlatformFileType_TSBMP, 0);
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

        case KESA_Sound:
        {
            sound_mode *SoundMode = &AssetsMode->SoundMode;
            kesa_sound *StoredSound = &StoredAsset->Sound;
            SoundMode->Sound = LoadWAV(StoredAsset->SourceFileName, StoredSound->FirstSampleIndex, 0,
                                       &SoundMode->FreeSound, 0);
        } break;

        case KESA_Text:
        {
            text_mode *TextMode = &AssetsMode->TextMode;
            TextMode->Text = LoadText(StoredAsset->SourceFileName, 0);
        } break;

        case KESA_Font:
        {
            font_mode *FontMode = &AssetsMode->FontMode;
            kesa_font *StoredFont = &StoredAsset->Font;
            u32 StandardFontSize = 24;

            if(*StoredAsset->SourceFileName)
            {
                FontMode->Font = Platform.LoadFontAsset(StoredAsset->SourceFileName, StandardFontSize, 0);
                for(u32 GlyphIndex = 1;
                    GlyphIndex < FontMode->Font.GlyphCount;
                    ++GlyphIndex)
                {
                    loaded_bitmap *Bitmap = FontMode->Font.Glyphs + GlyphIndex;
                    AllocateBitmap(Assets, Bitmap);
                }
            }
        } break;

        case KESA_File:
        {
            binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
            kesa_binary_file *StoredFile = &StoredAsset->File;
            read_file_result ReadResult =
                Platform.ReadEntireFile(StoredAsset->SourceFileName, PlatformFileType_BIN, 0, 0);    
            FileMode->FileData = (u8 *)ReadResult.Contents;
            StoredFile->FileSize = ReadResult.Size;
        } break;

        case KESA_SSWM:
        {
            sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
            kesa_sswm_file *StoredFile = &StoredAsset->SSWM;
            read_file_result ReadResult =
                Platform.ReadEntireFile(StoredAsset->SourceFileName, PlatformFileType_KEWM, 0, 0);    
            SSWMMode->FileData = (u8 *)ReadResult.Contents;
            StoredFile->FileSize = ReadResult.Size;
        } break;

        InvalidDefaultCase;
    }
}

internal void
LoadNewStoredAsset(editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_asset *Asset)
{
    if(AssetsMode->SourceFileCounts[Asset->Type])
        StringCopy(AssetsMode->SourceFiles[Asset->Type][AssetsMode->FileIndex], Asset->SourceFileName);

    switch(Asset->Type)
    {
        case KESA_None:
            break;

        case KESA_Bitmap:
        {
            LoadStoredAssetData(AssetsMode, Assets, Asset);
            Asset->Bitmap.AlignPercentage = AssetsMode->BitmapMode.Bitmap.AlignPercentage;
        } break;

        case KESA_SpriteSheet:
        {
            LoadStoredAssetData(AssetsMode, Assets, Asset);
            Asset->SpriteSheet.SpriteHeight = AssetsMode->SpriteSheetMode.SpriteSheetBitmap.Height;
        } break;

        case KESA_Tileset:
        {
            if(AssetsMode->SolidTileFileCount)
            {
                char *SolidTileFileName = AssetsMode->SolidTileFiles[AssetsMode->SubFileIndex];
                StringCopy(SolidTileFileName, Asset->Tileset.MergeTileFileName);
            }
            
            LoadStoredAssetData(AssetsMode, Assets, Asset);
            Asset->Tileset.TileWidth = Asset->Tileset.TileHeight = 32;
        } break;

        case KESA_Font:
        {
            LoadStoredAssetData(AssetsMode, Assets, Asset);
            font_mode *FontMode = &AssetsMode->FontMode;
            if(FontMode->Font.Glyphs)
            {
                Asset->Font.CodePointCount = FontMode->Font.GlyphCount - 1;
                Asset->Font.FirstCodePoint = FontMode->Font.UnicodeCodePoints[1];
                Asset->Font.LastCodePoint = FontMode->Font.UnicodeCodePoints[FontMode->Font.GlyphCount - 1];
                Asset->Font.FontSizeInPixels = 24;
            }
        } break;

        case KESA_Sound:
        case KESA_Text:
        case KESA_File:
        case KESA_SSWM:
        {
            LoadStoredAssetData(AssetsMode, Assets, Asset);
        } break;

        InvalidDefaultCase;
    }
}

inline void
InitAssetsMode(editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_asset *Asset)
{
    Asset->Type = KESAFromEditMode(AssetsMode->EditMode);
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
        v2 PointP = BitmapRect.Min + Scale*V2(X, Y);
        PushRect(RenderGroup, Flat, V3(PointP + 0.5f*V2(Scale, Scale), 8.0f), V2(Scale, Scale));

        if(WasPressed(Input->MouseButtons[PlatformMouseButton_Left]))
        {
            *Result = CalculateAlingment(V2(X, Y), BitmapPixelWidth, BitmapPixelHeight);
        }
    }
}

internal void
UpdateAndRenderBitmapEditMode(editor_mode_assets *AssetsMode, engine_input *Input, editor_assets *Assets,
                              render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                              rectangle2 CanvasRect, r32 TileDim, kesa_asset *Asset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    v2 CanvasCenter = GetCenter(CanvasRect);
    r32 CanvasSize = CanvasDim.x;
    r32 HalfCanvasSize = 0.5f*CanvasSize;
    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, CanvasCenter);

    bitmap_mode *BitmapMode = &AssetsMode->BitmapMode;
    kesa_bitmap *StoredBitmap = &Asset->Bitmap;
                
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, KESA_Bitmap);
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
                                   rectangle2 CanvasRect, r32 TileDim, kesa_asset *CurrentAsset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    v2 CanvasCenter = GetCenter(CanvasRect);
    r32 CanvasSize = CanvasDim.x;
    r32 HalfCanvasSize = 0.5f*CanvasSize;
    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, CanvasCenter);

    spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
    kesa_spritesheet *SpriteSheet = &CurrentAsset->SpriteSheet;

    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, KESA_SpriteSheet);
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
                 kesa_tileset *Tileset, r32 TileWidthOverHeight, v2 TilesetDim)
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
                                   rectangle2 CanvasRect, r32 TileDim, kesa_asset *Asset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    v2 CanvasCenter = GetCenter(CanvasRect);
    r32 CanvasSize = CanvasDim.x;
    r32 HalfCanvasSize = 0.5f*CanvasSize;

    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, V2(0, 0));

    kesa_tileset *StoredTileset = &Asset->Tileset;
    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
    if((AssetsMode->FileIndex != AssetsMode->LastFileIndex) ||
       (AssetsMode->SubFileIndex != AssetsMode->LastSubFileIndex))
    {
        ClearStoredAssetData(AssetsMode, Assets, KESA_Tileset);
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
                             audio_state *AudioState, kesa_asset *Asset)
{
    sound_mode *SoundMode = &AssetsMode->SoundMode;
    kesa_sound *StoredSound = &Asset->Sound;
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, KESA_Sound, AudioState);
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
UpdateAndRenderTextEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_asset *Asset)
{
    text_mode *TextMode = &AssetsMode->TextMode;
    kesa_text *StoredText = &Asset->Text;
                
    if((AssetsMode->FileIndex != AssetsMode->LastFileIndex) || TextMode->Reload)
    {
        ClearStoredAssetData(AssetsMode, Assets, KESA_Text);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);

        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
        TextMode->Reload = false;
    }

    if(TextMode->EditTextFile && AssetsMode->SourceFileCounts[KESA_Text])
    {
        char *FileName = AssetsMode->SourceFiles[KESA_Text][AssetsMode->FileIndex];
        char CommandLine[512];
        FormatString(ArrayCount(CommandLine), CommandLine,
                     "openwithnotepad.bat txts/%s", FileName);
        Platform.DEBUGExecuteSystemCommand(0, 0, CommandLine);

        TextMode->EditTextFile = false;
    }
}

internal void
UpdateAndRenderFontEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_asset *Asset)
{
    font_mode *FontMode = &AssetsMode->FontMode;
    kesa_font *StoredFont = &Asset->Font;
                
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, KESA_Font);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);

        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
UpdateAndRenderFileEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_asset *Asset)
{
    kesa_binary_file *StoredFile = &Asset->File;
    binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
    
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, KESA_File);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
UpdateAndRenderSSWMEditMode(editor_mode_assets *AssetsMode, editor_assets *Assets, kesa_asset *Asset)
{
    kesa_sswm_file *StoredFile = &Asset->SSWM;
    sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
    
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, Assets, KESA_SSWM);
        LoadNewStoredAsset(AssetsMode, Assets, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal b32
ReadStoredAssets(editor_state *EditorState, editor_mode_assets *AssetsMode)
{
    b32 Result = true;
    kesa_header *StoredHeader = &AssetsMode->StoredHeader;

    editor_meta *EditorMeta = &EditorState->EditorMeta;
    char StoredFileName[256];
    FormatString(ArrayCount(StoredFileName), StoredFileName, "stored_assets_%d.%d.%d.%d.kesa",
                 EditorMeta->KESAVersion[0], EditorMeta->KESAVersion[1],
                 EditorMeta->KESAVersion[2], EditorMeta->KESAVersion[3]);

    u32 FullVersion = (((u32)EditorMeta->KESAVersion[0] << 24) |
                       ((u32)EditorMeta->KESAVersion[1] << 16) |
                       ((u32)EditorMeta->KESAVersion[2] <<  8) |
                       ((u32)EditorMeta->KESAVersion[3]));
    
    platform_file_handle KESAHandle =
        Platform.OpenFile(StoredFileName, PlatformFileType_KESA, PlatformFileOp_Read);
    if(PlatformNoFileErrors(&KESAHandle))
    {
        Platform.ReadDataFromFile(&KESAHandle, 0, sizeof(kesa_header), StoredHeader);

        Assert(StoredHeader->SizeOfStoredAsset == sizeof(kesa_asset));
        Assert(StoredHeader->Version == FullVersion);
        Assert(StoredHeader->AssetCount);
        
        u32 TagArraySize = sizeof(u64)*StoredHeader->TagCount;
        Platform.DeallocateMemory(AssetsMode->TagGUIDs);
        AssetsMode->TagGUIDs = (u64 *)Platform.AllocateMemory(TagArraySize);

        Platform.ReadDataFromFile(&KESAHandle, StoredHeader->TagsOffset,
                                  TagArraySize, AssetsMode->TagGUIDs);

        u32 AssetsSize = StoredHeader->AssetCount*StoredHeader->SizeOfStoredAsset;
        Platform.DeallocateMemory(AssetsMode->StoredAssets);
        AssetsMode->StoredAssets = (kesa_asset *)Platform.AllocateMemory(AssetsSize);

        Platform.ReadDataFromFile(&KESAHandle, StoredHeader->AssetsOffset,
                                  AssetsSize, AssetsMode->StoredAssets);
        
        Platform.CloseFile(&KESAHandle);
    }
    else
    {
        Result = false;
        KESAHandle = Platform.OpenFile(StoredFileName, PlatformFileType_KESA, PlatformFileOp_Write);
        if(PlatformNoFileErrors(&KESAHandle))
        {
            StoredHeader->MagicValue = KESA_MAGIC_VALUE;
            StoredHeader->Version = FullVersion;
            StoredHeader->SizeOfStoredAsset = sizeof(kesa_asset);
            StoredHeader->AssetCount = 1;
            StoredHeader->TagCount = AssetsMode->TagMapListCount;
            StoredHeader->TagsOffset = sizeof(kesa_header);
            u32 TagArraySize = sizeof(u64)*StoredHeader->TagCount;
            StoredHeader->AssetsOffset = StoredHeader->TagsOffset + TagArraySize;

            temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);
            u64 *GUIDArray = PushArray(TempMem.Arena, StoredHeader->TagCount, u64); 
        
            u32 I = 0;
            for(tag_map_list *Iter = AssetsMode->TagMapListHead;
                Iter;
                Iter = Iter->Next)
            {
                GUIDArray[I] = Iter->Tag.GUID;
            }

            InsertionSort(StoredHeader->TagCount, GUIDArray);

            Platform.WriteDataToFile(&KESAHandle, 0, sizeof(kesa_header), StoredHeader);
            Platform.WriteDataToFile(&KESAHandle, StoredHeader->TagsOffset,
                                     TagArraySize, GUIDArray);
        
            kesa_asset NullAsset = {};
            Platform.WriteDataToFile(&KESAHandle, StoredHeader->AssetsOffset,
                                     sizeof(kesa_asset), &NullAsset);

            Platform.CloseFile(&KESAHandle);
        
            EndTemporaryMemory(TempMem);
        }
        else
        {
            // TODO(pvlso): Logging
        }
    }

    return(Result);
}



internal void
WriteStoredAssets(editor_state *EditorState, editor_mode_assets *AssetsMode)
{
    if(AssetsMode->AddAssetCount || AssetsMode->StoredAssetChanged)
    {
        char FileName[256];
        kesa_header *StoredHeader = &AssetsMode->StoredHeader;

        u32 FullVersion = UpdateEditorVersionFile(EditorState);
    
        editor_meta *EditorMeta = &EditorState->EditorMeta;
        FormatString(ArrayCount(FileName), FileName, "stored_assets_%d.%d.%d.%d.kesa",
                     EditorMeta->KESAVersion[0], EditorMeta->KESAVersion[1],
                     EditorMeta->KESAVersion[2], EditorMeta->KESAVersion[3]);

        platform_file_handle KESAHandle =
            Platform.OpenFile(FileName, PlatformFileType_KESA, PlatformFileOp_Write);
        if(PlatformNoFileErrors(&KESAHandle))
        {
            StoredHeader->Version = FullVersion;

            u32 ExistingAssetsCount = StoredHeader->AssetCount;
            StoredHeader->AssetCount += AssetsMode->AddAssetCount;
            u32 ExistingTagCount = StoredHeader->TagCount;
            StoredHeader->TagCount = AssetsMode->TagMapListCount;

            temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);
            u64 *GUIDArray = PushArray(TempMem.Arena, StoredHeader->TagCount, u64); 
        
            u32 I = 0;
            for(tag_map_list *Iter = AssetsMode->TagMapListHead;
                Iter;
                Iter = Iter->Next)
            {
                GUIDArray[I] = Iter->Tag.GUID;
            }

            InsertionSort(StoredHeader->TagCount, GUIDArray);

            u32 TagArraySize = StoredHeader->TagCount*sizeof(u64);
            Platform.WriteDataToFile(&KESAHandle, 0, sizeof(kesa_header), StoredHeader);
            Platform.WriteDataToFile(&KESAHandle, StoredHeader->TagsOffset,
                                     TagArraySize, GUIDArray);


            u32 ExistingAssetsSize = ExistingAssetsCount*sizeof(kesa_asset);
            if(ExistingAssetsCount)
            {
                Platform.WriteDataToFile(&KESAHandle, StoredHeader->AssetsOffset,
                                         ExistingAssetsSize, AssetsMode->StoredAssets);
            }

            u32 AssetsSize = AssetsMode->AddAssetCount*sizeof(kesa_asset);
            u64 Offset = StoredHeader->AssetsOffset + ExistingAssetsSize;
            Platform.WriteDataToFile(&KESAHandle, Offset, AssetsSize, AssetsMode->AssetsToAdd);

            Platform.CloseFile(&KESAHandle);

            EndTemporaryMemory(TempMem);
        }
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
        kesa_asset *AssetToRemove = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
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
UpdateAndRenderAssetsMode(editor_state *EditorState, transient_state *TranState, engine_input *Input)
{
    editor_assets *Assets = TranState->Assets;

    editor_mode_assets *AssetsMode = EditorState->AssetsMode;
    ui_state *UIState = &EditorState->UIState;

    b32 Result = false;//CheckForMetaInput(EditorState, TranState, Input);
    if(!Result)
    {
        render_group *RenderGroup = &UIState->RenderGroup;
        object_transform Flat = DefaultFlatTransform();
        v2 MouseP = Unproject(RenderGroup, &Flat, V2(Input->MouseX, Input->MouseY)).xy;
        if(!AssetsMode->AssetsInitialized)
        {
            if(!ReadStoredAssets(EditorState, AssetsMode))
            {
                Assert(ReadStoredAssets(EditorState, AssetsMode));
            }
            AssetsMode->AssetsInitialized = true;
        }

        if(!AssetsMode->Exit)
        {
            if(AssetsMode->LastEditMode != AssetsMode->EditMode)
            {
                ClearEditModeData(EditorState, AssetsMode, Assets, AssetsMode->CurrentAsset);
                AssetsMode->EditStoredAsset = false;

                if(AssetsMode->LastEditMode == EditMode_None)
                {
                    AssetsMode->CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
                    if(AssetsMode->CurrentAsset->Type == KESA_None)
                    {
                        InitAssetsMode(AssetsMode, Assets, AssetsMode->CurrentAsset);
                    }
                }
                else
                {
                    AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
                    LoadStoredAssetData(AssetsMode, Assets, AssetsMode->CurrentAsset);
                }

                AssetsMode->LastEditMode = AssetsMode->EditMode;            
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

            if(AssetsMode->AddAsset && !AssetsMode->EditStoredAsset)
            {
                AddCurrentAsset(AssetsMode);
                AssetsMode->CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
                AssetsMode->AddAsset = false;
                WriteStoredAssets(EditorState, AssetsMode);
            }
        
            r32 TileDim = 32.0f;
            r32 CanvasSize = 960.0f;
            r32 HalfCanvasSize = 0.5f*CanvasSize;
            v2 CanvasDim = V2(CanvasSize, CanvasSize);
            v2 CanvasHalfDim = 0.5f*V2(CanvasSize, CanvasSize);
            rectangle2 CanvasRect = RectCenterDim(V2(0, 0), CanvasDim);


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
                        AssetsMode->LastShowStoredAssetIndex = AssetsMode->ShowStoredAssetIndex;
                    }
                    else if((AssetsMode->LastShowStoredAssetIndex != AssetsMode->ShowStoredAssetIndex) ||
                            AssetsMode->RemoveStoredAsset)
                    {
                        kesa_asset *PreviousAsset = AssetsMode->StoredAssets + AssetsMode->LastShowStoredAssetIndex;
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
//                        BuildSSAFile(AssetsMode, EditorState->Version, TempMemory.Arena);
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

            DrawAssetsModeUI(AssetsMode, UIState);


            AssetsMode->Time += Input->dtForFrame;
        }
        else
        {
            ClearStoredAssetData(AssetsMode, Assets, AssetsMode->CurrentAsset->Type,
                                 &EditorState->AudioState);
            Platform.DeallocateMemory(AssetsMode->StoredAssets);
            PlayTitleScreen(EditorState, TranState);
        }
    }

    return(Result);
}

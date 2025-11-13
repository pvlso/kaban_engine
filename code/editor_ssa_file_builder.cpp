/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#define BITMAP_BYTES_PER_PIXEL 4

internal loaded_bitmap
LoadBMP(char *FileName, platform_file_type Type, memory_arena *Arena)
{
    loaded_bitmap Result = {};
    
    read_file_result ReadResult = Platform.ReadEntireFile(FileName, Type, Arena);    
    if(ReadResult.Size != 0)
    {
        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        Result.Memory = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;
        Result.WidthOverHeight = (r32)Result.Width / (r32)Result.Height;
        Result.AlignPercentage = V2(0.5f, 0.5f);
        
        Assert(Result.Height >= 0);
        Assert(Header->Compression == 3);

        // NOTE(casey): If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EITHER DIRECTION and
        // the height will be negative for top-down.
        // (Also, there can be compression, etc., etc... DON'T think this
        // is complete BMP loading code because it isn't!!)

        // NOTE(casey): Byte order in memory is determined by the Header itself,
        // so we have to read out the masks and convert the pixels ourselves.
        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);        
        
        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);
        
        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        int32 RedShiftDown = (int32)RedScan.Index;
        int32 GreenShiftDown = (int32)GreenScan.Index;
        int32 BlueShiftDown = (int32)BlueScan.Index;
        int32 AlphaShiftDown = (int32)AlphaScan.Index;
        
        uint32 *SourceDest = Pixels;
        for(int32 Y = 0;
            Y < Header->Height;
            ++Y)
        {
            for(int32 X = 0;
                X < Header->Width;
                ++X)
            {
                uint32 C = *SourceDest;

                v4 Texel  =
                    {
                        (real32)((C & RedMask) >> RedShiftDown),
                        (real32)((C & GreenMask) >> GreenShiftDown),
                        (real32)((C & BlueMask) >> BlueShiftDown),
                        (real32)((C & AlphaMask) >> AlphaShiftDown)
                    };

                Texel = SRGB255ToLinear1(Texel);

                Texel.rgb *= Texel.a;
                Texel = Linear1ToSRGB255(Texel);
                
                *SourceDest++ = (((uint32)(Texel.a + 0.5f) << 24) |
                                 ((uint32)(Texel.r + 0.5f) << 16) |
                                 ((uint32)(Texel.g + 0.5f) << 8) |
                                 ((uint32)(Texel.b + 0.5f) << 0));
            }
        }
    }

    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
    
    return(Result);
}

struct riff_iterator
{
    uint8 *At;
    uint8 *Stop;
};

inline riff_iterator
ParseChunkAt(void *At, void *Stop)
{
    riff_iterator Iter;

    Iter.At = (uint8 *)At;
    Iter.Stop = (uint8 *)Stop;

    return(Iter);
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Size = (Chunk->Size + 1) & ~1;
    Iter.At += sizeof(WAVE_chunk) + Size;

    return(Iter);
}

inline bool32
IsValid(riff_iterator Iter)
{
    bool32 Result = (Iter.At < Iter.Stop);

    return(Result);
}

inline void *
GetChunkData(riff_iterator Iter)
{
    void *Result = (Iter.At + sizeof(WAVE_chunk)); 

    return(Result);
}

inline uint32
GetType(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->ID;

    return(Result);
}

inline uint32
GetChunkDataSize(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->Size;

    return(Result);
}

internal loaded_sound
LoadWAV(char *FileName, u32 SectionFirstSampleIndex, u32 SectionSampleCount, void **Free, memory_arena *Arena)
{
    loaded_sound Result = {};
    
    read_file_result ReadResult = Platform.ReadEntireFile(FileName, PlatformFileType_WAV, Arena);    
    if(ReadResult.Size != 0)
    {
        if(!Arena)
        {
            *Free = ReadResult.Contents;
        }

        WAVE_header *Header = (WAVE_header *)ReadResult.Contents;
        Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
        Assert(Header->WAVEID == WAVE_ChunkID_WAVE);

        uint32 ChannelCount = 0;
        uint32 SampleDataSize = 0;
        int16 *SampleData = 0;
        for(riff_iterator Iter = ParseChunkAt(Header + 1, (uint8 *)(Header + 1) + Header->Size - 4);
            IsValid(Iter);
            Iter = NextChunk(Iter))
        {
            switch(GetType(Iter))
            {
                case WAVE_ChunkID_fmt:
                {
                    WAVE_fmt *fmt = (WAVE_fmt *)GetChunkData(Iter);
                    Assert(fmt->wFormatTag == 1); // NOTE(casey): Only support PCM
                    Assert(fmt->nSamplesPerSec == 48000);
                    Assert(fmt->wBitsPerSample == 16);
                    Assert(fmt->nBlockAlign == sizeof(int16)*fmt->nChannels);
                    ChannelCount = fmt->nChannels;
                } break;

                case WAVE_ChunkID_data:
                {
                    SampleData = (int16 *)GetChunkData(Iter);
                    SampleDataSize = GetChunkDataSize(Iter);
                } break;
            }
        }

        Assert(ChannelCount && SampleData);

        Result.ChannelCount = ChannelCount;
        u32 SampleCount = SampleDataSize / (ChannelCount*sizeof(int16));
        if(ChannelCount == 1)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = 0;
        }
        else if(ChannelCount == 2)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = SampleData + SampleCount;

            for(uint32 SampleIndex = 0;
                SampleIndex < SampleCount;
                ++SampleIndex)
            {
                int16 Source = SampleData[2*SampleIndex];
                SampleData[2*SampleIndex] = SampleData[SampleIndex];
                SampleData[SampleIndex] = Source;
            }
        }
        else
        {
            Assert(!"Invalid channel count in WAV file");
        }

        // TODO(casey): Load right channels!
        b32 AtEnd = true;
        Result.ChannelCount = 1;
        if(SectionSampleCount)
        {
            Assert((SectionFirstSampleIndex + SectionSampleCount) <= SampleCount);
            AtEnd = ((SectionFirstSampleIndex + SectionSampleCount) == SampleCount);
            SampleCount = SectionSampleCount;
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ++ChannelIndex)
            {
                Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
            }
        }

        if(AtEnd)
        {
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ++ChannelIndex)
            {
                for(u32 SampleIndex = SampleCount;
                    SampleIndex < (SampleCount + 8);
                    ++SampleIndex)
                {
                    Result.Samples[ChannelIndex][SampleIndex] = 0;
                }
            }
        }

        Result.SampleCount = SampleCount;
    }

    return(Result);
}

internal void
LoadSprites(builder_assets *Assets, builder_loaded_spritesheet *Sheet, memory_arena *Arena)
{
    loaded_bitmap SpriteSheet = LoadBMP(Sheet->StoredSheet->SourceFileName, PlatformFileType_SSBMP, Arena);
    for(u32 SpriteIndex = 0;
        SpriteIndex < Sheet->StoredSheet->SpriteCount;
        ++SpriteIndex)
    {
        loaded_bitmap *Sprite = Sheet->Sprites + SpriteIndex;
        Sprite->Width = Sheet->StoredSheet->SpriteWidth;
        Sprite->Height = Sheet->StoredSheet->SpriteHeight;
        Sprite->WidthOverHeight = (r32)Sprite->Width/(r32)Sprite->Height;
        Sprite->Pitch = Sprite->Width*BITMAP_BYTES_PER_PIXEL;
        Sprite->AlignPercentage = V2(0.5f, 0.5f);
        u32 MemorySize = Sprite->Pitch*Sprite->Height;
        Sprite->Memory = (Arena ? PushSize(Arena, MemorySize): Platform.AllocateMemory(MemorySize));

        u8 *SourceRow = (u8 *)(SpriteSheet.Memory) + SpriteIndex*Sprite->Pitch;
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

            SourceRow += SpriteSheet.Pitch;
            DestRow += Sprite->Pitch;
        }
    }
}

internal loaded_text
LoadText(char *FileName, memory_arena *Arena)
{
    loaded_text Result = {};
    read_file_result ReadResult = Platform.ReadEntireFile(FileName, PlatformFileType_TXT, Arena);    
    Result.String = (char *)ReadResult.Contents;

    return(Result);
}

#if 0
struct builder_added_asset
{
    u32 ID;
    ssa_asset *SSA;
    builder_asset_source *Source;
};

internal builder_added_asset
BuilderAddAsset(builder_assets *Assets, asset_type_id TypeID)
{
    ssa_asset_type *AssetType = Assets->AssetTypes + TypeID;
    Assert(AssetType->OnePastLastAssetIndex < VERY_LARGE_NUMBER);

    if((AssetType->FirstAssetIndex == 0))
    {
        Assets->TypeAddingOrder[Assets->TypeAddingCount++] = AssetType;
        AssetType->TypeID = TypeID;
        AssetType->FirstAssetIndex = Assets->AssetCount;
        AssetType->OnePastLastAssetIndex = AssetType->FirstAssetIndex;
    }

    u32 FirstToFix = 1;
    u32 OnePastLastToFix = Assets->TypeAddingCount;
    b32 TypeDataShouldBeFixed = false;
    for(u32 OrderIndex = 0;
        OrderIndex < Assets->TypeAddingCount;
        ++OrderIndex)
    {
        ssa_asset_type *FirstInOrder = Assets->TypeAddingOrder[OrderIndex];
        if(FirstInOrder->TypeID == (u32)TypeID)
        {
            TypeDataShouldBeFixed = ((FirstInOrder->TypeID == Asset_Tileset) |
                                     (FirstInOrder->TypeID == Asset_SpriteSheet) |
                                     (FirstInOrder->TypeID == Asset_Font) &
                                     (FirstToFix != OnePastLastToFix));
            break;
        }
        else
        {
            ++FirstToFix;
        }
    }

    if(FirstToFix != OnePastLastToFix)
    {
        ssa_asset_type *FirstTypeToFix = Assets->TypeAddingOrder[FirstToFix];
        for(u32 AssetIndex = Assets->AssetCount;
            AssetIndex > FirstTypeToFix->FirstAssetIndex;
            --AssetIndex)
        {
            u32 Index = (AssetIndex - 1);
            Assets->AssetSources[AssetIndex] = Assets->AssetSources[Index];
            Assets->Assets[AssetIndex] = Assets->Assets[Index];
            if(Index == FirstTypeToFix->FirstAssetIndex)
            {
                Assets->AssetSources[Index] = {};
                Assets->Assets[Index] = {};
            }
        }

        for(u32 OrderIndex = FirstToFix;
            OrderIndex < OnePastLastToFix;
            ++OrderIndex)
        {
            ssa_asset_type *Type = Assets->TypeAddingOrder[OrderIndex]; 
            Type->FirstAssetIndex += 1;
            Type->OnePastLastAssetIndex += 1;

            switch(Type->TypeID)
            {
                case Asset_Tile:
                {
                    ssa_asset_type *TypeToFix = Assets->AssetTypes + Asset_Tileset;
                    for(u32 AssetIndex = TypeToFix->FirstAssetIndex;
                        AssetIndex < TypeToFix->OnePastLastAssetIndex;
                        ++AssetIndex)
                    {
                        builder_asset_source *Source = Assets->AssetSources + AssetIndex;
                        for(u32 TileIndex = 0;
                            TileIndex < Source->Tileset.Tileset->StoredTileset->TileCount;
                            ++TileIndex)
                        {
                            ssa_tile *Tile = Source->Tileset.Tileset->Tiles + TileIndex;
                            Tile->BitmapID.Value += 1;
                        }
                    }
                } break;

                case Asset_Sprite:
                {
                    ssa_asset_type *TypeToFix = Assets->AssetTypes + Asset_SpriteSheet;
                    for(u32 AssetIndex = TypeToFix->FirstAssetIndex;
                        AssetIndex < TypeToFix->OnePastLastAssetIndex;
                        ++AssetIndex)
                    {
                        builder_asset_source *Source = Assets->AssetSources + AssetIndex;
                        for(u32 SpriteIndex = 0;
                            SpriteIndex < Source->SpriteSheet.Sheet->StoredSheet->SpriteCount;
                            ++SpriteIndex)
                        {
                            bitmap_id *Sprite = Source->SpriteSheet.Sheet->SpriteIDs + SpriteIndex;
                            Sprite->Value += 1;
                        }
                    }
                } break;

                case Asset_FontGlyph:
                {
                    ssa_asset_type *TypeToFix = Assets->AssetTypes + Asset_Font;
                    for(u32 AssetIndex = TypeToFix->FirstAssetIndex;
                        AssetIndex < TypeToFix->OnePastLastAssetIndex;
                        ++AssetIndex)
                    {
                        builder_asset_source *Source = Assets->AssetSources + AssetIndex;
                        for(u32 GlyphIndex = 0;
                            GlyphIndex < Source->Font.Font->GlyphCount;
                            ++GlyphIndex)
                        {
                            ssa_font_glyph *Glyph = Source->Font.Glyphs + GlyphIndex;
                            Glyph->BitmapID.Value += 1;
                        }
                    }
                } break;
            }
        }
    }
    
    u32 Index = AssetType->OnePastLastAssetIndex++;
    builder_asset_source *Source = Assets->AssetSources + Index;
    ssa_asset *SSA = Assets->Assets + Index;
    SSA->FirstTagIndex = Assets->TagCount;
    SSA->OnePastLastTagIndex = SSA->FirstTagIndex;

    Assets->AssetIndex = Index;
    Assets->AssetCount += 1;
    
    builder_added_asset Result;
    Result.ID = Index;
    Result.SSA = SSA;
    Result.Source = Source;
    
    return(Result);
}
#endif

internal loaded_bitmap
LoadTileBitmap(builder_loaded_tileset *Tileset, u32 TileIndex, memory_arena *TempArena)
{
    loaded_bitmap Tile = {};
    Tile.Width = Tileset->StoredTileset->TileWidth;
    Tile.Height = Tileset->StoredTileset->TileHeight;
    Tile.WidthOverHeight = (r32)Tile.Width/(r32)Tile.Height;
    Tile.AlignPercentage = V2(0.5f, 0.5f);
    Tile.Pitch = Tile.Width*BITMAP_BYTES_PER_PIXEL;
    u32 MemorySize = Tile.Height*Tile.Pitch;
    Tile.Memory = PushSize(TempArena, MemorySize);

    u8 *DestRow = (u8 *)Tile.Memory;

    u32 MainSurfaceIndexX = Tileset->StoredTileset->TileOffsetsX[TileIndex]*Tile.Width;
    u32 MainSurfaceIndexY = Tileset->StoredTileset->TileOffsetsY[TileIndex]*Tile.Height;
    u8 *MainSurfaceSource = (u8 *)Tileset->TilesetBitmap.Memory + (MainSurfaceIndexY*Tileset->TilesetBitmap.Pitch +
                                                                   MainSurfaceIndexX*BITMAP_BYTES_PER_PIXEL);
    if(Tileset->StoredTileset->MergedTile)
    {
        loaded_bitmap *MergeTile = &Tileset->MergeTile;
        u8 *MergeSurfaceSource = (u8 *)MergeTile->Memory;
        for(s32 Y = 0;
            Y < Tile.Height;
            ++Y)
        {
            u32 *MergeSource = (u32 *)MergeSurfaceSource;
            u32 *MainSource = (u32 *)MainSurfaceSource;
            u32 *Dest = (u32 *)DestRow;
            for(s32 X = 0;
                X < Tile.Width;
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

            MainSurfaceSource += Tileset->TilesetBitmap.Pitch;
            MergeSurfaceSource += MergeTile->Pitch;
            DestRow += Tile.Pitch;
        }
    }
    else
    {
        for(s32 Y = 0;
            Y < Tile.Height;
            ++Y)
        {
            u32 *MainSource = (u32 *)MainSurfaceSource;
            u32 *Dest = (u32 *)DestRow;
            for(s32 X = 0;
                X < Tile.Width;
                ++X)
            {
                *Dest = *MainSource;

                ++MainSource;
                ++Dest;
            }

            MainSurfaceSource += Tileset->TilesetBitmap.Pitch;
            DestRow += Tile.Pitch;
        }
    }

    return(Tile);
}

#if 0
internal void
AddTag(builder_assets *Assets, asset_tag_id ID, u32 Value)
{
    Assert(Assets->AssetIndex);

    ssa_asset *Asset = Assets->Assets + Assets->AssetIndex;
    ++Asset->OnePastLastTagIndex;
    Assert(Asset->OnePastLastTagIndex < VERY_LARGE_NUMBER);
    ssa_tag *Tag = Assets->Tags + Assets->TagCount++;

    Tag->ID = ID;
    Tag->Value = Value;
}

inline void
AddTag(builder_assets *Assets, ssa_tag Tag)
{
    AddTag(Assets, (asset_tag_id)Tag.ID, Tag.Value);
}

inline void
AddStoredAssetTags(builder_assets *Assets, stored_asset *StoredAsset)
{
    for(u32 TagIndex = 0;
        TagIndex < StoredAsset->TagCount;
        ++TagIndex)
    {
        ssa_tag Tag = StoredAsset->AssetTags[TagIndex];
        AddTag(Assets, Tag);
    }    
}

internal void
InitializeBuilder(builder_assets *Assets, memory_arena *Arena)
{
    Assets->TagCount = 1;
    Assets->Tags = PushArray(Arena, VERY_LARGE_NUMBER, ssa_tag, NoClear());
    Assets->AssetCount = 1;
    Assets->AssetSources = PushArray(Arena, VERY_LARGE_NUMBER, builder_asset_source, NoClear());
    Assets->Assets = PushArray(Arena, VERY_LARGE_NUMBER, ssa_asset, NoClear());
    Assets->AssetIndex = 0;
    
    Assets->AssetTypeCount = Asset_Count;
    ZeroArray(Assets->AssetTypeCount, Assets->AssetTypes);
}

internal bitmap_id
AddTileAsset(builder_assets *Assets, builder_loaded_tileset *Tileset, u32 TileIndex, memory_arena *TempArena)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, Asset_Tile);

    Asset.SSA->Bitmap.AlignPercentage[0] = 0.5f;
    Asset.SSA->Bitmap.AlignPercentage[1] = 0.5f;
    Asset.Source->Type = BuilderAssetType_Tile;
    Asset.Source->Tile.TileIndex = TileIndex;
    Asset.Source->Tile.Tileset = Tileset;
    
    bitmap_id Result = {};
    Result.Value = Asset.ID;

    ssa_tile *Tile = Tileset->Tiles + TileIndex;
    loaded_bitmap Bitmap = LoadTileBitmap(Tileset, TileIndex, TempArena);
    Tile->BitmapID = Result;
    Tile->CheckSum = CRC32((u8 *)Bitmap.Memory, Bitmap.Pitch*Bitmap.Height);
    
    return(Result);
}

internal tileset_id
AddTilesetAsset(builder_assets *Assets, builder_loaded_tileset *Tileset)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, Asset_Tileset);

    Asset.SSA->Tileset.TileCount = Tileset->StoredTileset->TileCount;
    Asset.Source->Type = BuilderAssetType_Tileset;
    Asset.Source->Tileset.Tileset = Tileset;
    
    tileset_id Result = {};
    Result.Value = Asset.ID;
    
    return(Result);
}

internal bitmap_id
AddBitmapAsset(builder_assets *Assets, stored_asset_bitmap *StoredBitmap, u32 TypeID)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, (asset_type_id)TypeID);

    Asset.SSA->Bitmap.AlignPercentage[0] = StoredBitmap->AlignPercentage.x;
    Asset.SSA->Bitmap.AlignPercentage[1] = StoredBitmap->AlignPercentage.y;
    Asset.Source->Type = BuilderAssetType_Bitmap;
    Asset.Source->Bitmap.Bitmap = StoredBitmap;

    bitmap_id Result = {};
    Result.Value = Asset.ID;

    return(Result);
}

internal sound_id
AddSoundAsset(builder_assets *Assets, stored_asset_sound *StoredSound, u32 TypeID)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, (asset_type_id)TypeID);

    Asset.SSA->Sound.SampleCount = 0;
    Asset.SSA->Sound.Chain = StoredSound->Chain;
    Asset.Source->Type = BuilderAssetType_Sound;
    Asset.Source->Sound.Sound = StoredSound;

    sound_id Result = {};
    Result.Value = Asset.ID;

    return(Result);
}

internal text_id
AddTextAsset(builder_assets *Assets, stored_asset_text *StoredText, u32 TypeID)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, (asset_type_id)TypeID);

    Asset.Source->Type = BuilderAssetType_Text;
    Asset.Source->Text.Text = StoredText;

    text_id Result = {};
    Result.Value = Asset.ID;

    return(Result);
}

internal file_id
AddFileAsset(builder_assets *Assets, stored_asset_binary_file *StoredFile, u32 TypeID)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, (asset_type_id)TypeID);

    Asset.Source->Type = BuilderAssetType_BinaryFile;
    Asset.Source->File.File = StoredFile;

    file_id Result = {};
    Result.Value = Asset.ID;

    return(Result);
}

internal sswm_id
AddSSWMAsset(builder_assets *Assets, stored_asset_sswm_file *StoredSSWM, u32 TypeID)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, (asset_type_id)TypeID);

    Asset.Source->Type = BuilderAssetType_SSWM;
    Asset.Source->SSWM.File = StoredSSWM;

    sswm_id Result = {};
    Result.Value = Asset.ID;

    return(Result);
}

internal bitmap_id
AddSpriteAsset(builder_assets *Assets, builder_loaded_spritesheet *SpriteSheet, u32 SpriteIndex)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, Asset_Sprite);

    Asset.SSA->Bitmap.AlignPercentage[0] = SpriteSheet->StoredSheet->SpriteAlignPercentage.x;
    Asset.SSA->Bitmap.AlignPercentage[1] = SpriteSheet->StoredSheet->SpriteAlignPercentage.y;
    Asset.Source->Type = BuilderAssetType_Sprite;
    Asset.Source->Sprite.Bitmap = SpriteSheet->Sprites[SpriteIndex];
 
    bitmap_id Result = {};
    Result.Value = Asset.ID;
    SpriteSheet->SpriteIDs[SpriteIndex] = Result;
    
    return(Result);
}

internal spritesheet_id
AddSpriteSheetAsset(builder_assets *Assets, builder_loaded_spritesheet *Sheet)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, Asset_SpriteSheet);

    Asset.Source->Type = BuilderAssetType_SpriteSheet;
    Asset.Source->SpriteSheet.Sheet = Sheet;

    spritesheet_id Result = {};
    Result.Value = Asset.ID;
    
    return(Result);
}

internal bitmap_id
AddCharacterAsset(builder_assets *Assets, builder_loaded_font *Font, ssa_font_glyph *Glyphs, u32 CodePoint)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, Asset_FontGlyph);

    u32 GlyphIndex = Font->UnicodeMap[CodePoint];
    loaded_bitmap *GlyphBitmap = Font->Glyphs + GlyphIndex;
    Asset.SSA->Bitmap.AlignPercentage[0] = GlyphBitmap->AlignPercentage.x;
    Asset.SSA->Bitmap.AlignPercentage[1] = GlyphBitmap->AlignPercentage.y;
    Asset.Source->Type = BuilderAssetType_FontGlyph;
    Asset.Source->Glyph.CodePoint = CodePoint;
    Asset.Source->Glyph.Bitmap = *GlyphBitmap;

    bitmap_id Result = {};
    Result.Value = Asset.ID;

    ssa_font_glyph *Glyph = Glyphs + GlyphIndex;
    Glyph->UnicodeCodePoint = CodePoint;
    Glyph->BitmapID = Result;

    return(Result);
}

internal font_id
AddFontAsset(builder_assets *Assets, builder_loaded_font *Font, ssa_font_glyph *Glyphs)
{
    builder_added_asset Asset = BuilderAddAsset(Assets, Asset_Font);

    Asset.SSA->Font.OnePastHighestCodePoint = Font->OnePastHighestCodePoint;
    Asset.SSA->Font.GlyphCount = Font->GlyphCount;
    Asset.SSA->Font.AscenderHeight = (r32)Font->AscenderHeight;
    Asset.SSA->Font.DescenderHeight = (r32)Font->DescenderHeight;
    Asset.SSA->Font.ExternalLeading = (r32)Font->ExternalLeading;
    Asset.Source->Type = BuilderAssetType_Font;
    Asset.Source->Font.Glyphs = Glyphs;
    Asset.Source->Font.Font = Font;

    font_id Result = {};
    Result.Value = Asset.ID;

    return(Result);
}
#endif

inline void
BeginWritingLog(FILE *LogFile, char *FileName = 0)
{
    char LogBuffer[512];
    u32 Length = 0;
    if(FileName)
    {
        Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                   "\nWriting %s asset...", FileName);
        fwrite(LogBuffer, Length, 1, LogFile);
    }

    Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                               "\n===============================================================\n");
    fwrite(LogBuffer, Length, 1, LogFile);
}

inline void
WriteLogForHeader(FILE *LogFile, ssa_header Header)
{
    char LogBuffer[512];
    u32 Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                   "Header written... \nMagic Value: %#x\nVersion: %d\n"\
                                   "TagCount: %d\nAssetTypeCount: %d\nAssetCount: %d\n",
                                   Header.MagicValue, Header.Version, Header.TagCount,
                                   Header.AssetTypeCount, Header.AssetCount);
    fwrite(LogBuffer, Length, 1, LogFile);
}

inline void
WriteLogForAsset(FILE *LogFile, builder_asset_source *Source, char *Text = 0)
{
    u32 Length = 0;
    char LogBuffer[512];
    Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer, "Source Data:\n");
    fwrite(LogBuffer, Length, 1, LogFile);
    switch(Source->Type)
    {
        case BuilderAssetType_Bitmap:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    AlignPercentage: V2(%.02f, %.02f)\n",
                                       Source->Bitmap.Bitmap->AlignPercentage.x,
                                       Source->Bitmap.Bitmap->AlignPercentage.y);
        } break;

        case BuilderAssetType_Sound:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    FirstSampleIndex: %d\n"\
                                       "    Chain Type: none\n",
                                       Source->Sound.Sound->FirstSampleIndex);
        } break;

        case BuilderAssetType_Font:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    OnePastHighestCodePoint: %d\n"\
                                       "    GlyphCount: %d\n"\
                                       "    AscenderHeight: %.02f\n"\
                                       "    DescenderHeight: %.02f\n"\
                                       "    ExternalLeading: %.02f\n",
                                       Source->Font.Font->OnePastHighestCodePoint,
                                       Source->Font.Font->GlyphCount,
                                       Source->Font.Font->AscenderHeight,
                                       Source->Font.Font->DescenderHeight,
                                       Source->Font.Font->ExternalLeading);
        } break;

        case BuilderAssetType_FontGlyph:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    CodePoint: %d\n"\
                                       "    Width: %d\n"\
                                       "    Height: %d\n"\
                                       "    Pitch: %d\n"\
                                       "    AlignPercentage: V2(%.02f, %.02f)\n",
                                       Source->Glyph.CodePoint,
                                       Source->Glyph.Bitmap.Width,
                                       Source->Glyph.Bitmap.Height,
                                       Source->Glyph.Bitmap.Pitch,
                                       Source->Glyph.Bitmap.AlignPercentage.x,
                                       Source->Glyph.Bitmap.AlignPercentage.y);
        } break;

        case BuilderAssetType_Tileset:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    Tileset Bitmap: %s\n"\
                                       "        Width: %d\n"\
                                       "        Height: %d\n"\
                                       "        Pitch: %d\n"\
                                       "    Merge Tile Bitmap: %s\n"\
                                       "        Width: %d\n"\
                                       "        Height: %d\n"\
                                       "        Pitch: %d\n"\
                                       "    Merged: %s\n"\
                                       "    Tile Count: %d\n"\
                                       "    Tile Width: %d\n"\
                                       "    Tile Height: %d\n",
                                       Source->Tileset.Tileset->StoredTileset->SourceFileName,
                                       Source->Tileset.Tileset->TilesetBitmap.Width,
                                       Source->Tileset.Tileset->TilesetBitmap.Height,
                                       Source->Tileset.Tileset->TilesetBitmap.Pitch,
                                       Source->Tileset.Tileset->StoredTileset->MergeTileFileName,
                                       Source->Tileset.Tileset->MergeTile.Width,
                                       Source->Tileset.Tileset->MergeTile.Height,
                                       Source->Tileset.Tileset->MergeTile.Pitch,
                                       Source->Tileset.Tileset->StoredTileset->MergedTile ? "true" : "false",
                                       Source->Tileset.Tileset->StoredTileset->TileCount,
                                       Source->Tileset.Tileset->StoredTileset->TileWidth,
                                       Source->Tileset.Tileset->StoredTileset->TileHeight);
        } break;

        case BuilderAssetType_Tile:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    Parent Tileset: %s\n"\
                                       "    Tile Index: %d\n"\
                                       "    CheckSum: %#x\n",
                                       Source->Tile.Tileset->StoredTileset->SourceFileName,
                                       Source->Tile.TileIndex, Source->Tile.Tileset->Tiles[Source->Tile.TileIndex].CheckSum);
        } break;

        case BuilderAssetType_Sprite:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    Width: %d\n"\
                                       "    Height: %d\n"\
                                       "    Pitch: %d\n"\
                                       "    AlignPercentage: V2(%.02f, %.02f)\n",
                                       Source->Sprite.Bitmap.Width,
                                       Source->Sprite.Bitmap.Height,
                                       Source->Sprite.Bitmap.Pitch,
                                       Source->Sprite.Bitmap.AlignPercentage.x,
                                       Source->Sprite.Bitmap.AlignPercentage.y);
        } break;

        case BuilderAssetType_SpriteSheet:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    Sprite Count: %d\n"\
                                       "    Sprite Width: %d\n"\
                                       "    Sprite Height: %d\n"\
                                       "    Sprite AlignPercentage: V2(%.02f, %.02f)\n",
                                       Source->SpriteSheet.Sheet->StoredSheet->SpriteCount,
                                       Source->SpriteSheet.Sheet->StoredSheet->SpriteWidth,
                                       Source->SpriteSheet.Sheet->StoredSheet->SpriteHeight,
                                       Source->SpriteSheet.Sheet->StoredSheet->SpriteAlignPercentage.x,
                                       Source->SpriteSheet.Sheet->StoredSheet->SpriteAlignPercentage.y);
        } break;

        case BuilderAssetType_Text:
        {
            if(Text)
            {
                Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                           "    %s\n", Text);
            }
        } break;

        case BuilderAssetType_BinaryFile:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    File Size: %d\n", Source->File.File->FileSize);
        } break;

        case BuilderAssetType_SSWM:
        {
            Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                       "    File Size: %d\n", Source->SSWM.File->FileSize);
        } break;
        
    }

    fwrite(LogBuffer, Length, 1, LogFile);
}

internal void
EndWritingLog(FILE *LogFile, char *FileName = 0)
{
    char LogBuffer[512];
    u32 Length = 0;

    Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                               "===============================================================\n");
    fwrite(LogBuffer, Length, 1, LogFile);

    if(FileName)
    {
        Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer,
                                   "Writing %s finished.\n", FileName);
        fwrite(LogBuffer, Length, 1, LogFile);
    }
}

internal void
BuilderWriteSSA(builder_assets *Assets, working_version Version, memory_arena *TempArena)
{
    u32 Length = 0;
    char LogBuffer[512];
    FILE *LogFile;
    FormatString(ArrayCount(LogBuffer), LogBuffer, "logs\\ssa_writing_log_%d.%d.%d.%d.txt",
                 Version.MajorHigh, Version.MajorLow, Version.MinorHigh, Version.MinorLow);
    fopen_s(&LogFile, LogBuffer, "wb");
    
    char SSAFileName[256];
    FormatString(ArrayCount(SSAFileName), SSAFileName, "ssas\\game_data_%d.%d.%d.%d.ssa",
                 Version.MajorHigh, Version.MajorLow, Version.MinorHigh, Version.MinorLow);

    Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer, "Writing to: %s\n", SSAFileName);
    fwrite(LogBuffer, Length, 1, LogFile);

    FILE *Out;
    fopen_s(&Out, SSAFileName, "wb");

    if(Out)
    {
        ssa_header Header = {};
        Header.MagicValue = SSA_MAGIC_VALUE;
        Header.Version = SSA_VERSION;
        Header.TagCount = Assets->TagCount;
        Header.AssetTypeCount = Asset_Count;
        Header.AssetCount = Assets->AssetCount;

        u32 TagArraySize = Header.TagCount*sizeof(ssa_tag);
        u32 AssetTypeArraySize = Header.AssetTypeCount*sizeof(ssa_asset_type);
        u32 AssetArraySize = Header.AssetCount*sizeof(ssa_asset);
        
        Header.Tags = sizeof(Header);
        Header.AssetTypes = Header.Tags + TagArraySize;
        Header.Assets = Header.AssetTypes + AssetTypeArraySize;

        BeginWritingLog(LogFile);
        WriteLogForHeader(LogFile, Header);
        EndWritingLog(LogFile);
        
        fwrite(&Header, sizeof(Header), 1, Out);
        fwrite(Assets->Tags, TagArraySize, 1, Out);
        fwrite(Assets->AssetTypes, AssetTypeArraySize, 1, Out);
        fseek(Out, AssetArraySize, SEEK_CUR);

        for(u32 AssetIndex = 1;
            AssetIndex < Header.AssetCount;
            ++AssetIndex)
        {
            builder_asset_source *Source = Assets->AssetSources + AssetIndex;
            ssa_asset *Dest = Assets->Assets + AssetIndex;

            Dest->DataOffset = ftell(Out);

            if(Source->Type == BuilderAssetType_Sound)
            {
                BeginWritingLog(LogFile, Source->Sound.Sound->SourceFileName);
                WriteLogForAsset(LogFile, Source);
                loaded_sound WAV = LoadWAV(Source->Sound.Sound->SourceFileName,
                                           Source->Sound.Sound->FirstSampleIndex,
                                           Dest->Sound.SampleCount, 0, TempArena);
                Dest->Sound.SampleCount = WAV.SampleCount;
                Dest->Sound.ChannelCount = WAV.ChannelCount;
                for(u32 ChannelIndex = 0;
                    ChannelIndex < WAV.ChannelCount;
                    ++ChannelIndex)
                {
                    fwrite(WAV.Samples[ChannelIndex], Dest->Sound.SampleCount*sizeof(s16), 1, Out);
                }

                EndWritingLog(LogFile, Source->Sound.Sound->SourceFileName);
            }
            else if(Source->Type == BuilderAssetType_Font)
            {
                BeginWritingLog(LogFile, "font");
                WriteLogForAsset(LogFile, Source);

                builder_loaded_font *Font = Source->Font.Font;

                u32 GlyphSize = Font->GlyphCount*sizeof(ssa_font_glyph);
                fwrite(Source->Font.Glyphs, GlyphSize, 1, Out);

                u32 HorizontalAdvanceSize = sizeof(r32)*Font->GlyphCount*Font->GlyphCount;
                fwrite(Font->HorizontalAdvance, HorizontalAdvanceSize, 1, Out);

                EndWritingLog(LogFile, "font");
            }
            else if(Source->Type == BuilderAssetType_Tileset)
            {
                builder_loaded_tileset *Tileset = Source->Tileset.Tileset;
                BeginWritingLog(LogFile, Tileset->StoredTileset->SourceFileName);
                WriteLogForAsset(LogFile, Source);

                u32 TilesSize = Tileset->StoredTileset->TileCount*sizeof(ssa_tile);
                fwrite(Tileset->Tiles, TilesSize, 1, Out);
                
                Dest->Tileset.TileCount = Tileset->StoredTileset->TileCount;

                EndWritingLog(LogFile, Tileset->StoredTileset->SourceFileName);
            }
            else if(Source->Type == BuilderAssetType_SpriteSheet)
            {
                builder_loaded_spritesheet *SpriteSheet = Source->SpriteSheet.Sheet;
                BeginWritingLog(LogFile, SpriteSheet->StoredSheet->SourceFileName);
                WriteLogForAsset(LogFile, Source);

                Dest->SpriteSheet.SpriteCount = SpriteSheet->StoredSheet->SpriteCount;
                u32 SpritesSize = SpriteSheet->StoredSheet->SpriteCount*sizeof(bitmap_id);
                fwrite(SpriteSheet->SpriteIDs, SpritesSize, 1, Out);

                EndWritingLog(LogFile, SpriteSheet->StoredSheet->SourceFileName);
            }
            else if(Source->Type == BuilderAssetType_Text)
            {
                BeginWritingLog(LogFile, Source->Text.Text->SourceFileName);
                loaded_text Text = LoadText(Source->Text.Text->SourceFileName, TempArena);
                WriteLogForAsset(LogFile, Source, Text.String);

                Dest->Text.Length = StringLength(Text.String);
                u32 TextSize = Dest->Text.Length;
                fwrite(Text.String, TextSize, 1, Out);

                EndWritingLog(LogFile, Source->Text.Text->SourceFileName);
            }
            else if(Source->Type == BuilderAssetType_BinaryFile)
            {
                BeginWritingLog(LogFile, Source->File.File->SourceFileName);
                WriteLogForAsset(LogFile, Source);
                read_file_result ReadResult =
                    Platform.ReadEntireFile(Source->File.File->SourceFileName, PlatformFileType_BIN, TempArena);    

                Dest->BinaryFile.Size = ReadResult.Size;
                fwrite(ReadResult.Contents, ReadResult.Size, 1, Out);

                EndWritingLog(LogFile, Source->File.File->SourceFileName);
            }
            else if(Source->Type == BuilderAssetType_SSWM)
            {
                BeginWritingLog(LogFile, Source->SSWM.File->SourceFileName);
                WriteLogForAsset(LogFile, Source);
                read_file_result ReadResult =
                    Platform.ReadEntireFile(Source->SSWM.File->SourceFileName, PlatformFileType_SSWM, TempArena);    

                Dest->SSWMFile.Size = ReadResult.Size;
                fwrite(ReadResult.Contents, ReadResult.Size, 1, Out);

                EndWritingLog(LogFile, Source->SSWM.File->SourceFileName);
            }
            else
            {
                loaded_bitmap Bitmap = {};
                if(Source->Type == BuilderAssetType_FontGlyph)
                {
                    BeginWritingLog(LogFile, "glyph");
                    WriteLogForAsset(LogFile, Source);
                    Bitmap = Source->Glyph.Bitmap;
                    EndWritingLog(LogFile, "glyph");
                }
                else if(Source->Type == BuilderAssetType_Tile)
                {
                    BeginWritingLog(LogFile, "tile");
                    WriteLogForAsset(LogFile, Source);
                    Bitmap = LoadTileBitmap(Source->Tile.Tileset, Source->Tile.TileIndex, TempArena);
                    EndWritingLog(LogFile, "tile");
                }
                else if(Source->Type == BuilderAssetType_Sprite)
                {
                    BeginWritingLog(LogFile, "sprite");
                    WriteLogForAsset(LogFile, Source);
                    Bitmap = Source->Sprite.Bitmap;
                    EndWritingLog(LogFile, "sprite");
                }
                else
                {
                    BeginWritingLog(LogFile, Source->Bitmap.Bitmap->FileName);
                    WriteLogForAsset(LogFile, Source);
                    Assert(Source->Type == AssetType_Bitmap);
                    Bitmap = LoadBMP(Source->Bitmap.Bitmap->FileName, PlatformFileType_BMP, TempArena);
                    EndWritingLog(LogFile, Source->Bitmap.Bitmap->FileName);
                }

                Dest->Bitmap.Dim[0] = Bitmap.Width;
                Dest->Bitmap.Dim[1] = Bitmap.Height;

                Assert((Bitmap.Width * BITMAP_BYTES_PER_PIXEL) == Bitmap.Pitch);
                fwrite(Bitmap.Memory, Bitmap.Height*Bitmap.Pitch, 1, Out);
            }
        }

        fseek(Out, (u32)Header.Assets, SEEK_SET);
        fwrite(Assets->Assets, AssetArraySize, 1, Out);
        
        fclose(Out);
        Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer, "Writing SSA comleted\n");
        fwrite(LogBuffer, Length, 1, LogFile);
    }
    else
    {
        Length = (u32)FormatString(ArrayCount(LogBuffer), LogBuffer, "ERROR: Fail to open %s\n", SSAFileName);
        fwrite(LogBuffer, Length, 1, LogFile);
    }

    fclose(LogFile);
}

#if 0
internal void
BuildSSAFile(editor_mode_assets *AssetsMode, working_version Version, memory_arena *TempArena)
{
    builder_assets *Assets = PushStruct(TempArena, builder_assets);
    InitializeBuilder(Assets, TempArena);

    
    stored_asset_file_header *StoredHeader = &AssetsMode->StoredHeader;
    stored_asset *StoredAssets = AssetsMode->StoredAssets;
    u32 TileID = 1;
    for(u32 StoredAssetIndex = 1;
        StoredAssetIndex < StoredHeader->AssetCount;
        ++StoredAssetIndex)
    {
        stored_asset *StoredAsset = StoredAssets + StoredAssetIndex;
        switch(StoredAsset->Type)
        {
            case StoredAssetType_Bitmap:
            {
                AddBitmapAsset(Assets, &StoredAsset->Bitmap, StoredAsset->TypeID);
                AddStoredAssetTags(Assets, StoredAsset);
            } break;

            case StoredAssetType_SpriteSheet:
            {
                builder_loaded_spritesheet *Sheet = PushStruct(TempArena, builder_loaded_spritesheet);
                Sheet->SpriteIDs = PushArray(TempArena, StoredAsset->SpriteSheet.SpriteCount, bitmap_id);
                Sheet->Sprites = PushArray(TempArena, StoredAsset->SpriteSheet.SpriteCount, loaded_bitmap);
                Sheet->StoredSheet = &StoredAsset->SpriteSheet;
                LoadSprites(Assets, Sheet, TempArena);

                AddSpriteSheetAsset(Assets, Sheet);
                AddStoredAssetTags(Assets, StoredAsset);

                for(u32 SpriteIndex = 0;
                    SpriteIndex < StoredAsset->SpriteSheet.SpriteCount;
                    ++SpriteIndex)
                {
                    AddSpriteAsset(Assets, Sheet, SpriteIndex);
                    AddTag(Assets, Tag_SpriteIndex, SpriteIndex);
                    AddStoredAssetTags(Assets, StoredAsset);
                }
            } break;

            case StoredAssetType_Tileset:
            {
                builder_loaded_tileset *Tileset = PushStruct(TempArena, builder_loaded_tileset);
                Tileset->Tiles = PushArray(TempArena, StoredAsset->Tileset.TileCount, ssa_tile);
                Tileset->TilesetBitmap = LoadBMP(StoredAsset->Tileset.SourceFileName, PlatformFileType_TSBMP, TempArena);
                Tileset->MergeTile = LoadBMP(StoredAsset->Tileset.MergeTileFileName, PlatformFileType_STBMP, TempArena);
                Tileset->StoredTileset = &StoredAsset->Tileset;

                stored_asset_tileset *StoredTileset = Tileset->StoredTileset;

                AddTilesetAsset(Assets, Tileset);
                AddStoredAssetTags(Assets, StoredAsset);

                for(u32 TileIndex = 0;
                    TileIndex < StoredTileset->TileCount;
                    ++TileIndex)
                {
                    ssa_tile *Tile = Tileset->Tiles + TileIndex;
                    AddTileAsset(Assets, Tileset, TileIndex, TempArena);
                    AddTag(Assets, Tag_TileID, TileID);
                    AddTag(Assets, Tag_TileChecksum, Tile->CheckSum);
                    AddStoredAssetTags(Assets, StoredAsset);

                    ++TileID;
                }
            } break;

            case StoredAssetType_Sound:
            {
                AddSoundAsset(Assets, &StoredAsset->Sound, StoredAsset->TypeID);
                AddStoredAssetTags(Assets, StoredAsset);
            } break;

            case StoredAssetType_Text:
            {
                AddTextAsset(Assets, &StoredAsset->Text, StoredAsset->TypeID);
                AddStoredAssetTags(Assets, StoredAsset);
            } break;

            case StoredAssetType_Font:
            {
                stored_asset_font *StoredFont = &StoredAsset->Font;
                builder_loaded_font Font = Platform.LoadFontAsset(StoredFont->SourceFileName,
                                                                  StoredFont->FontSizeInPixels, TempArena);
                ssa_font_glyph *Glyphs = PushArray(TempArena, Font.GlyphCount, ssa_font_glyph);

                AddFontAsset(Assets, &Font, Glyphs);
                AddStoredAssetTags(Assets, StoredAsset);

                for(u32 GlyphIndex = 0;
                    GlyphIndex < Font.GlyphCount;
                    ++GlyphIndex)
                {
                    u32 CodePoint = Font.UnicodeCodePoints[GlyphIndex];
                    AddCharacterAsset(Assets, &Font, Glyphs, CodePoint);
                    AddTag(Assets, Tag_UnicodeCodepoint, CodePoint);
                    AddStoredAssetTags(Assets, StoredAsset);
                }
            } break;

            case StoredAssetType_File:
            {
                AddFileAsset(Assets, &StoredAsset->File, StoredAsset->TypeID);
                AddStoredAssetTags(Assets, StoredAsset);
            } break;

            case StoredAssetType_SSWM:
            {
                AddSSWMAsset(Assets, &StoredAsset->SSWM, StoredAsset->TypeID);
                AddStoredAssetTags(Assets, StoredAsset);
            } break;

            InvalidDefaultCase;
        }
    }

    BuilderWriteSSA(Assets, Version, TempArena);
}

#endif

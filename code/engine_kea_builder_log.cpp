/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#if 0
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
#endif

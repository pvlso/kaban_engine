/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#include "engine_kea_builder.h"

#include "engine_kea_builder_log.cpp"
#include "engine_kea_builder_load.cpp"

/*
  TODO(pvlso):
   - Logging
*/

internal kea_builder *
InitKEABuilder(editor_mode_assets *AssetsMode, memory_arena *TempMem)
{
    kea_builder *KEABuilder = PushStruct(TempMem, kea_builder);

    KEABuilder->TempMem = TempMem;
    KEABuilder->KESAHeader = &AssetsMode->StoredHeader;
    KEABuilder->KESAAssets = AssetsMode->StoredAssets;
    KEABuilder->KETHeader = &AssetsMode->TagHeader;
    KEABuilder->TagMaps = AssetsMode->Tags;

    KEABuilder->Tags = PushStruct(TempMem, kea_builder_tag_list);
    KEABuilder->TagCount = 1;
    KEABuilder->BuilderAssets = PushStruct(TempMem, kea_builder_added_asset_list);

    return(KEABuilder);
}

inline s32
GetTagMapByGUID(kea_tag_map *TagMaps, u32 Count, u64 GUID)
{
    s32 ResultIndex = 0;

    s32 Low = 0;
    s32 High = Count - 1;
    while(Low <= High)
    {
        s32 Mid = Low + ((High - Low) >> 1);
        kea_tag_map *MidMap = TagMaps + Mid;
        if(MidMap->GUID < GUID)
            Low = Mid + 1;
        else if(MidMap->GUID > GUID)
            High = Mid - 1;
        else
        {
            ResultIndex = Mid;
            break;
        }
    }

    return(ResultIndex);
}

inline kea_builder_added_asset_list *
GetAssetByGUID(kea_builder *Builder, u64 GUID)
{
    kea_builder_added_asset_list *Result = 0;

    s32 Low = 0;
    s32 High = Builder->AssetCount - 1;
    while(Low <= High)
    {
        s32 Mid = Low + ((High - Low) >> 1);
        kea_builder_added_asset_list *MidMap = Builder->SortedBuilderAssets[Mid];
        if(MidMap->KEAAsset.GUID < GUID)
            Low = Mid + 1;
        else if(MidMap->KEAAsset.GUID > GUID)
            High = Mid - 1;
        else
        {
            Result = MidMap;
            break;
        }
    }

    return(Result);
}

internal kea_builder_added_asset_list *
BuilderAddAsset(kea_builder *Builder, kesa_asset *Asset)
{
    Builder->CurrentAsset = PushStruct(Builder->TempMem, kea_builder_added_asset_list);

    Builder->CurrentAsset->StoredAsset = Asset;
    Builder->CurrentAsset->KEAAsset.FirstTagIndex = Builder->TagCount;
    Builder->CurrentAsset->KEAAsset.OnePastLastTagIndex =
        Builder->CurrentAsset->KEAAsset.FirstTagIndex;
    Builder->AssetCount += 1;
    
    Builder->CurrentAsset->Next = Builder->BuilderAssets->Next;
    Builder->BuilderAssets->Next = Builder->CurrentAsset;

    return(Builder->CurrentAsset);
}

internal void
AddTileAsset(kea_builder *Builder, kesa_asset *Tileset, added_asset_additional_data *Data, u32 TileIndex,
             loaded_bitmap *TileBitmap)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Tileset);

    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    kesa_asset *KESAAsset = BuilderAsset->StoredAsset;

    if(KESAAsset->Tileset.MergedTile)
    {
        FormatString(ArrayCount(Builder->Buffer), Builder->Buffer, "%s%s%s%d",
                     Tileset->SourceFileName, Tileset->Tileset.MergeTileFileName, "Tile", TileIndex);
    }
    else
    {
        FormatString(ArrayCount(Builder->Buffer), Builder->Buffer, "%s%s%d",
                     Tileset->SourceFileName, "Tile", TileIndex);
    }
    
    KEAAsset->GUID = GUIDFromString(Builder->Buffer);
    KEAAsset->Type = KEAType_Tile;
    KEAAsset->Bitmap.AlignPercentage[0] = 0.5f;
    KEAAsset->Bitmap.AlignPercentage[1] = 0.5f;
    
    Data->Tileset.TileGUIDs[TileIndex] = KEAAsset->GUID;
    BuilderAsset->Data.Tile.TileBitmap = TileBitmap;
    
    Builder->CurrentAsset = 0;
}

internal void
AddTilesetAsset(kea_builder *Builder, kesa_asset *Asset)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);
    
    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    kesa_asset *KESAAsset = BuilderAsset->StoredAsset;
    
    KEAAsset->GUID = KESAAsset->GUID;
    KEAAsset->Type = KEAType_Tileset;
    KEAAsset->Tileset.TileCount = KESAAsset->Tileset.TileCount;

    BuilderAsset->Data.Tileset.TileGUIDs = PushArray(Builder->TempMem, KEAAsset->Tileset.TileCount, u64);
    

    Builder->CurrentAsset = 0;

    loaded_bitmap TilesetBitmap = LoadBMP(KESAAsset->SourceFileName, PlatformFileType_TSBMP, Builder->TempMem);
    loaded_bitmap MergeTileBitmap = {};
    if(KESAAsset->Tileset.MergedTile)
        MergeTileBitmap = LoadBMP(KESAAsset->Tileset.MergeTileFileName, PlatformFileType_STBMP, Builder->TempMem);
        
    builder_loaded_tiles TileBitmaps = {};
    TileBitmaps.Count = KESAAsset->Tileset.TileCount;
    TileBitmaps.TileBitmaps = PushArray(Builder->TempMem, KESAAsset->Tileset.TileCount, loaded_bitmap);
    for(u32 I = 0;
        I < KESAAsset->Tileset.TileCount;
        ++I)
    {
        TileBitmaps.TileBitmaps[I] = LoadTileBitmap(&KESAAsset->Tileset, &TilesetBitmap, &MergeTileBitmap, I, Builder->TempMem);
        AddTileAsset(Builder, Asset, &BuilderAsset->Data, I, TileBitmaps.TileBitmaps + I);
    }

    Builder->CurrentAsset = BuilderAsset;
}

internal void
AddSpriteAsset(kea_builder *Builder, kesa_asset *SpriteSheet, added_asset_additional_data *Data,
               u32 SpriteIndex, loaded_bitmap *Sprite)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, SpriteSheet);

    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    kesa_asset *KESAAsset = BuilderAsset->StoredAsset;

    FormatString(ArrayCount(Builder->Buffer), Builder->Buffer, "%s%s%d",
                 SpriteSheet->SourceFileName, "Sprite", SpriteIndex);

    KEAAsset->GUID = GUIDFromString(Builder->Buffer);
    KEAAsset->Type = KEAType_Sprite;
    KEAAsset->Bitmap.AlignPercentage[0] = KESAAsset->SpriteSheet.SpriteAlignPercentage.x;
    KEAAsset->Bitmap.AlignPercentage[1] = KESAAsset->SpriteSheet.SpriteAlignPercentage.y;
    
    Data->SpriteSheet.SpriteGUIDs[SpriteIndex] = KEAAsset->GUID;
    BuilderAsset->Data.Sprite.SpriteBitmap = Sprite;
    
    Builder->CurrentAsset = 0;
}

internal void
AddSpriteSheetAsset(kea_builder *Builder, kesa_asset *Asset)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);
    
    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    kesa_asset *KESAAsset = BuilderAsset->StoredAsset;
    
    KEAAsset->GUID = KESAAsset->GUID;
    KEAAsset->Type = KEAType_SpriteSheet;
    KEAAsset->SpriteSheet.SpriteCount = KESAAsset->SpriteSheet.SpriteCount;

    BuilderAsset->Data.SpriteSheet.SpriteGUIDs = PushArray(Builder->TempMem, KEAAsset->SpriteSheet.SpriteCount, u64);

    Builder->CurrentAsset = 0;

    builder_loaded_sprites Sprites = LoadSprites(KESAAsset, Builder->TempMem);
    for(u32 I = 0;
        I < KESAAsset->SpriteSheet.SpriteCount;
        ++I)
    {
        AddSpriteAsset(Builder, Asset, &BuilderAsset->Data, I, Sprites.Sprites + I);
    }

    Builder->CurrentAsset = BuilderAsset;
}

internal void
AddTextAsset(kea_builder *Builder, kesa_asset *Asset)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);

    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    kesa_asset *KESAAsset = BuilderAsset->StoredAsset;
    
    KEAAsset->GUID = KESAAsset->GUID;
    KEAAsset->Type = KEAType_TXT;
}

internal void
AddFileAsset(kea_builder *Builder, kesa_asset *Asset)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);

    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    kesa_asset *KESAAsset = BuilderAsset->StoredAsset;
    
    KEAAsset->GUID = KESAAsset->GUID;
    KEAAsset->Type = KEAType_BIN;
}

internal void
AddSoundAsset(kea_builder *Builder, kesa_asset *Asset)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);

    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    kesa_asset *KESAAsset = BuilderAsset->StoredAsset;
    
    KEAAsset->GUID = KESAAsset->GUID;
    KEAAsset->Type = KEAType_Sound;
    KEAAsset->Sound.Chain = KESAAsset->Sound.Chain;
}

internal void
AddBitmapAsset(kea_builder *Builder, kesa_asset *Asset)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);

    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    kesa_asset *KESAAsset = BuilderAsset->StoredAsset;
    
    KEAAsset->GUID = KESAAsset->GUID;
    KEAAsset->Type = KEAType_Bitmap;
    KEAAsset->Bitmap.AlignPercentage[0] = KESAAsset->Bitmap.AlignPercentage.x;
    KEAAsset->Bitmap.AlignPercentage[1] = KESAAsset->Bitmap.AlignPercentage.y;
}

internal void
AddTag(kea_builder *Builder, u32 TagIndex, u32 TagValueIndex, u64 TagGUID)
{
    Assert(Builder->CurrentAsset);

    kea_asset *Asset = &Builder->CurrentAsset->KEAAsset;
    ++Asset->OnePastLastTagIndex;
    Builder->CurrentTag = PushStruct(Builder->TempMem, kea_builder_tag_list);
    Builder->CurrentTag->TagIndex = Builder->TagCount;
    Builder->CurrentTag->Tag.GUID = TagGUID;
    ++Builder->TagCount;

    kea_builder_tag_lookup_entry_list *Search = Builder->TagTable.Next;    
    b32 Found = false;
    while(Search)
    {
        if(Search->Entry.GUID == TagGUID)
        {
            Search->Entry.Count += 1;
            Found = true;
            break;
        }

        Search = Search->Next;
    }

    if(!Found)
    {
        kea_builder_tag_lookup_entry_list *TagEntry = PushStruct(Builder->TempMem, kea_builder_tag_lookup_entry_list);    
        TagEntry->Entry.GUID = TagGUID;
        TagEntry->Entry.TagIndex = TagIndex;
        TagEntry->Entry.TagValueIndex = TagValueIndex;
        TagEntry->Entry.Count = 1;

        TagEntry->Next = Builder->TagTable.Next;
        Builder->TagTable.Next = TagEntry;    

        ++Builder->EntryCount;
    }
    
    Builder->CurrentTag->Next = Builder->Tags->Next;
    Builder->Tags->Next = Builder->CurrentTag;
    Builder->CurrentTag = 0;
}

inline void
AddStoredAssetTags(kea_builder *Builder, kesa_asset *StoredAsset)
{
    for(u32 I = 0;
        I < StoredAsset->TagCount;
        ++I)
    {
        kesa_tag Tag = StoredAsset->AssetTags[I];
        u32 TagIndex = GetTagMapByGUID(Builder->TagMaps, Builder->KETHeader->TagCount, Tag.TagGUID);
        AddTag(Builder, TagIndex, Tag.TagValueIndex, Builder->TagMaps[TagIndex].ValueGUIDs[Tag.TagValueIndex]);
    }    
}

internal void
BuilderWriteKEA(kea_builder *Builder)
{
    kesa_header *KESAHeader = Builder->KESAHeader;

    char KEAFileName[256];
    FormatString(ArrayCount(KEAFileName), KEAFileName, "../data/keas/game_data_%d.kea",
                 KESAHeader->Version);
    FILE *Out;
    fopen_s(&Out, KEAFileName, "wb");
    if(Out)
    {
        kea_header Header = {};
        Header.MagicValue = KEA_MAGIC_VALUE;
        Header.Version = KESAHeader->Version;
        Header.TagCount = Builder->KETHeader->TagCount;
        Header.UsedTagsCount = Builder->TagCount;
        Header.HashTable = Builder->TagHashTable;
        Header.TagedAssetsIndeciesCount = Builder->TagAssetsIndeciesCount;
        Header.AssetTypeCount = KEAType_Count;
        Header.AssetCount = Builder->AssetCount;

        u32 TagMapArraySize = Header.TagCount*sizeof(ket_tag);
        u32 UsedTagsArraySize = Header.UsedTagsCount*sizeof(kea_tag);
        u32 TagHashTableDataSize = Header.HashTable.Capacity*sizeof(kea_tag_lookup_entry);
        u32 TagAssetsIndeciesSize = Header.TagedAssetsIndeciesCount*sizeof(u32);
        u32 AssetTypeArraySize = Header.AssetTypeCount*sizeof(kea_asset_type_table_entry);
        u32 AssetArraySize = Header.AssetCount*sizeof(kea_asset);
        
        Header.TagMapsOffset = sizeof(Header);

        ket_tag *KETTags = (ket_tag *)PushSize(Builder->TempMem, TagMapArraySize);
        fseek(Out, (u32)(Header.TagMapsOffset + TagMapArraySize), SEEK_SET);
        for(u32 TagMapIndex = 0;
            TagMapIndex < Header.TagCount;
            ++TagMapIndex)
        {
            kea_tag_map *TagMap = Builder->TagMaps + TagMapIndex;
            ket_tag *Tag = KETTags + TagMapIndex;
            Tag->GUID = TagMap->GUID;
            Copy(KET_TAG_KEY_LENGTH, TagMap->Key, Tag->Key);
            Tag->ValueCount = TagMap->ValueCount;
            Tag->DataOffset = ftell(Out);

            fwrite(TagMap->ValueGUIDs, Tag->ValueCount*sizeof(u64), 1, Out);
            for(u32 StringIndex = 0;
                StringIndex < Tag->ValueCount;
                ++StringIndex)
            {
                fwrite(TagMap->Values[StringIndex], KET_TAG_KEY_LENGTH, 1, Out);
            }
        }

        Header.UsedTagsArrayOffset = ftell(Out);
        Header.HashTable.TableOffset = Header.UsedTagsArrayOffset + UsedTagsArraySize;
        Header.TagedAssetsIndeciesOffset = Header.HashTable.TableOffset + TagHashTableDataSize;
        Header.AssetTypeTableOffset = Header.TagedAssetsIndeciesOffset + TagAssetsIndeciesSize;

        u32 AssetTypeTableSize = 0;
        for(u32 Type = 0;
            Type < KEAType_Count;
            ++Type)
        {
            AssetTypeTableSize += Builder->AssetTypeTable[Type].TypeCount*sizeof(u32);
        }

        Header.AssetsOffset = Header.AssetTypeTableOffset + AssetTypeArraySize + AssetTypeTableSize;
        
        fseek(Out, 0, SEEK_SET);
        fwrite(&Header, sizeof(kea_header), 1, Out);
        fwrite(KETTags, TagMapArraySize, 1, Out);
        fseek(Out, (u32)Header.UsedTagsArrayOffset, SEEK_SET);
        fwrite(Builder->UsedTags, UsedTagsArraySize, 1, Out);
        fwrite(Builder->TableData, TagHashTableDataSize, 1, Out);
        fwrite(Builder->TagAssetsIndecies, TagAssetsIndeciesSize, 1, Out);

        fseek(Out, AssetTypeArraySize, SEEK_CUR);
        for(u32 Type = 0;
            Type < KEAType_Count;
            ++Type)
        {
            Builder->AssetTypeTable[Type].AssetsIndeciesOffset = ftell(Out);
            fwrite(Builder->AssetTypeTableData[Type], sizeof(u32)*Builder->AssetTypeTable[Type].TypeCount, 1, Out);
        }

        fseek(Out, (u32)Header.AssetTypeTableOffset, SEEK_SET);
        fwrite(Builder->AssetTypeTable, AssetTypeArraySize, 1, Out);
        fseek(Out, (u32)Header.AssetsOffset + AssetArraySize, SEEK_SET);

        for(u32 AssetIndex = 1;
            AssetIndex < Header.AssetCount;
            ++AssetIndex)
        {
            kea_builder_added_asset_list *BuilderAsset = Builder->SortedBuilderAssets[AssetIndex];
            kea_asset *Dest = &BuilderAsset->KEAAsset;
            kesa_asset *Source = BuilderAsset->StoredAsset;
            Dest->DataOffset = ftell(Out);

            if(Dest->Type == KEAType_Sound)
            {
                loaded_sound WAV = LoadWAV(Source->SourceFileName,
                                           Source->Sound.FirstSampleIndex,
                                           0, 0, Builder->TempMem);
                Dest->Sound.SampleCount = WAV.SampleCount;
                Dest->Sound.ChannelCount = WAV.ChannelCount;
                for(u32 ChannelIndex = 0;
                    ChannelIndex < WAV.ChannelCount;
                    ++ChannelIndex)
                {
                    fwrite(WAV.Samples[ChannelIndex], Dest->Sound.SampleCount*sizeof(s16), 1, Out);
                }
            }
            else if(Dest->Type == KEAType_Tileset)
            {
                u32 TilesSize = Source->Tileset.TileCount*sizeof(u64);
                fwrite(BuilderAsset->Data.Tileset.TileGUIDs, TilesSize, 1, Out);
                
                Dest->Tileset.TileCount = Source->Tileset.TileCount;
            }
            else if(Dest->Type == KEAType_SpriteSheet)
            {
                u32 SpritesSize = Source->SpriteSheet.SpriteCount*sizeof(u64);
                fwrite(BuilderAsset->Data.SpriteSheet.SpriteGUIDs, SpritesSize, 1, Out);

                Dest->SpriteSheet.SpriteCount = Source->SpriteSheet.SpriteCount;
            }
            else if(Dest->Type == KEAType_TXT)
            {
                loaded_text Text = LoadText(Source->SourceFileName, Builder->TempMem);

                Dest->Text.Length = StringLength(Text.String);
                u32 TextSize = Dest->Text.Length;
                fwrite(Text.String, TextSize, 1, Out);
            }
            else if(Dest->Type == KEAType_BIN)
            {
                read_file_result ReadResult =
                    Platform.ReadEntireFile(Source->SourceFileName, PlatformFileType_BIN, Builder->TempMem, true);    

                Dest->BinaryFile.Size = ReadResult.Size;
                fwrite(ReadResult.Contents, ReadResult.Size, 1, Out);
            }
            else
            {
                loaded_bitmap Bitmap = {};
                if(Dest->Type == KEAType_Tile)
                {
                    Bitmap = *BuilderAsset->Data.Tile.TileBitmap;
                }
                else if(Dest->Type == KEAType_Sprite)
                {
                    Bitmap = *BuilderAsset->Data.Sprite.SpriteBitmap;
                }
                else
                {
                    Assert(Source->Type == AssetType_Bitmap);
                    Bitmap = LoadBMP(Source->SourceFileName, PlatformFileType_BMP, Builder->TempMem);
                }

                Dest->Bitmap.Dim[0] = Bitmap.Width;
                Dest->Bitmap.Dim[1] = Bitmap.Height;

                Assert((Bitmap.Width * BITMAP_BYTES_PER_PIXEL) == Bitmap.Pitch);
                fwrite(Bitmap.Memory, Bitmap.Height*Bitmap.Pitch, 1, Out);
            }
        }

        fseek(Out, (u32)Header.AssetsOffset, SEEK_SET);

        kea_asset *KEAAssets = PushArray(Builder->TempMem, Builder->AssetCount, kea_asset);
        for(u32 AssetIndex = 0;
            AssetIndex < Builder->AssetCount;
            ++AssetIndex)
        {
            KEAAssets[AssetIndex] = Builder->SortedBuilderAssets[AssetIndex]->KEAAsset;
        }
        
        fwrite(KEAAssets, AssetArraySize, 1, Out);
        fclose(Out);
    }
    else
    {
        // TODO(pvlso): Logging
    }
}

internal inline void
TagTableInsert(kea_tag_lookup_entry *Table, u32 Capacity, kea_tag_lookup_entry Entry)
{
    u32 Mask = Capacity - 1;
    u32 Index = (u32)HashU64(Entry.GUID) & Mask;
    for(;;)
    {
        kea_tag_lookup_entry *Insert = Table + Index;
        if(Insert->GUID == 0)
        {
            Copy(sizeof(kea_tag_lookup_entry), &Entry, Insert);
            break;
        }

        Assert(Insert->GUID != Entry.GUID);
        Index = (Index + 1) & Mask;
    }
}

internal b32
BuildKEA(editor_mode_assets *AssetsMode, memory_arena *TempMem)
{
    b32 Result = true;

    kea_builder *Builder = InitKEABuilder(AssetsMode, TempMem);

    // NOTE(pvlso): Create raw asset list
    kesa_header *StoredHeader = Builder->KESAHeader;
    for(u32 StoredAssetIndex = 1;
        StoredAssetIndex < StoredHeader->AssetCount;
        ++StoredAssetIndex)
    {
        kesa_asset *StoredAsset = Builder->KESAAssets + StoredAssetIndex;
        switch(StoredAsset->Type)
        {
            case KESA_Bitmap:
                AddBitmapAsset(Builder, StoredAsset);
                break;

            case KESA_SpriteSheet:
                AddSpriteSheetAsset(Builder, StoredAsset);
                break;

            case KESA_Tileset:
                AddTilesetAsset(Builder, StoredAsset);
                break;

            case KESA_Sound:
                AddSoundAsset(Builder, StoredAsset);
                break;

            case KESA_Text:
                AddTextAsset(Builder, StoredAsset);
                break;

            case KESA_File:
                AddFileAsset(Builder, StoredAsset);
                break;

            case KESA_SSWM:
                Assert(!"Not implemented");
                break;

                InvalidDefaultCase;
        }

        AddStoredAssetTags(Builder, StoredAsset);
        Builder->CurrentAsset = 0;
    }

    kea_builder_added_asset_list *CurrentAsset = Builder->BuilderAssets;
    u32 SortedCount = 0;
    Builder->AssetCount += 1; // NOTE(pvlso): to acount for a null asset
    Builder->SortedBuilderAssets = PushArray(Builder->TempMem, Builder->AssetCount, kea_builder_added_asset_list *);
    while(CurrentAsset)
    {
        Builder->SortedBuilderAssets[SortedCount++] = CurrentAsset;
        CurrentAsset = CurrentAsset->Next;
    }

    // NOTE(pvlso): Sort builder asset pointer array
    for(u32 I = 1; I < Builder->AssetCount; ++I)
    {
        kea_builder_added_asset_list *Node = Builder->SortedBuilderAssets[I];
        s32 J = I - 1;
        while((J >= 0) && (Node->KEAAsset.GUID < Builder->SortedBuilderAssets[J]->KEAAsset.GUID))
        {
            Builder->SortedBuilderAssets[J + 1] = Builder->SortedBuilderAssets[J];
            J = J - 1;
        }

        Builder->SortedBuilderAssets[J + 1] = Node;
        Node->KEAAsset.AssetIndex = J;
    }
        
    // NOTE(pvlso): asign an index and count asset types
    Builder->AssetTypeTable = PushArray(Builder->TempMem, KEAType_Count, kea_asset_type_table_entry);
    for(u32 I = 0; I < KEAType_Count; ++I)
        Builder->AssetTypeTable[I].Type = I;    

    for(u32 I = 0;
        I < SortedCount;
        ++I)
    {
        kea_builder_added_asset_list *Asset = Builder->SortedBuilderAssets[I];

        Asset->KEAAsset.AssetIndex = I;
        Builder->AssetTypeTable[Asset->KEAAsset.Type].TypeCount++; 
    }

    Builder->AssetTypeTableData = PushArray(Builder->TempMem, KEAType_Count, u32 *);
    for(u32 I = 0; I < KEAType_Count; ++I)
        Builder->AssetTypeTableData[I] = PushArray(Builder->TempMem, Builder->AssetTypeTable[I].TypeCount, u32);

    u32 Itters[KEAType_Count] = {};
    for(u32 I = 0;
        I < SortedCount;
        ++I)
    {
        kea_builder_added_asset_list *Asset = Builder->SortedBuilderAssets[I];
        u32 Type = Asset->KEAAsset.Type;
        Builder->AssetTypeTableData[Type][Itters[Type]++] = Asset->KEAAsset.AssetIndex; 
    }

    Builder->UsedTags = PushArray(Builder->TempMem, Builder->TagCount, kea_tag);
    kea_builder_tag_list *Itter = Builder->Tags->Next;
    while(Itter)
    {
        Builder->UsedTags[Itter->TagIndex] = Itter->Tag;
        Itter = Itter->Next;
    }

    // NOTE(pvlso): count how many asset indecies we need
    u32 IndeciesArrayCount = 0;
    kea_builder_tag_lookup_entry_list *Search = Builder->TagTable.Next;    
    while(Search)
    {
        Search->Entry.AssetsFirstIndex = IndeciesArrayCount;
        IndeciesArrayCount += Search->Entry.Count;
        Search = Search->Next;
    }

    // NOTE(pvlso): Fill the index array
    Builder->TagAssetsIndeciesCount = IndeciesArrayCount;
    Builder->TagAssetsIndecies = PushArray(Builder->TempMem, Builder->TagAssetsIndeciesCount, u32);
    u32 FirstIndex = 0;
    for(u32 I = 0;
        I < SortedCount;
        ++I)
    {
        kea_builder_added_asset_list *Asset = Builder->SortedBuilderAssets[I];
        kea_asset *KEA = &Asset->KEAAsset;
        for(u32 TagIndex = KEA->FirstTagIndex;
            TagIndex < KEA->OnePastLastTagIndex;
            ++TagIndex)
        {
            u64 TagGUID = Builder->UsedTags[TagIndex].GUID;
            kea_builder_tag_lookup_entry_list *Search = Builder->TagTable.Next;    
            while(Search)
            {
                if(Search->Entry.GUID == TagGUID)
                {
                    Assert(Search->IndexOffset < Search->Entry.Count);

                    Builder->TagAssetsIndecies[Search->Entry.AssetsFirstIndex + Search->IndexOffset] = KEA->AssetIndex;
                    Search->IndexOffset += 1;
                    break;
                }

                Search = Search->Next;
            }
        }
    }

    // NOTE(pvlso): Build tag hash table
    u32 HashTableCapacity = NextPow2(CeilReal32ToInt32((f32)Builder->EntryCount / 0.7f));
    Builder->TagHashTable.Capacity = HashTableCapacity;
    Builder->TableData = PushArray(Builder->TempMem, HashTableCapacity, kea_tag_lookup_entry);

    Search = Builder->TagTable.Next;    
    while(Search)
    {
        TagTableInsert(Builder->TableData, HashTableCapacity, Search->Entry);
        Search = Search->Next;
    }
    
    BuilderWriteKEA(Builder);
    
    return(Result);
}

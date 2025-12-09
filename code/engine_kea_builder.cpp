/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#include "engine_kea_builder.h"

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

struct bitset_index_offset_pair
{
    u32 Index;
    u32 Offset;
};

inline bitset_index_offset_pair
GetBitsetPair(u32 BitIndex)
{
    bitset_index_offset_pair Result = {};
    Result.Index = BitIndex >> 5; // NOTE(pvlso): equals to BitIndex / 32
    Result.Offset = BitIndex & 31; // NOTE(pvlso): equals to BitIndex % 32

    return(Result);
}

inline void
SetBitsetBit(u32 *Bitset, u32 BitIndex)
{
    bitset_index_offset_pair Pair = GetBitsetPair(BitIndex);
    Bitset[Pair.Index] |= (1u << Pair.Offset);
}

inline b32
TestBitsetBit(u32 *Bitset, u32 BitIndex)
{
    bitset_index_offset_pair Pair = GetBitsetPair(BitIndex);
    
    b32 Result = (Bitset[Pair.Index] >> Pair.Offset) & 1u;
    return(Result);
}

internal kea_tag *
GetUsedTags(kea_builder *KEABuilder, memory_arena *TempMem)
{
    kea_tag *Result = 0;

    // NOTE(pvlso): Count how many unique tags are used
    u32 *TagBitsetOffsets = PushArray(TempMem, KEABuilder->KETHeader->TagCount, u32);
    u32 PossiableUniqueCount = 0;
    for(u32 I = 0;
        I < KEABuilder->KETHeader->TagCount;
        ++I)
    {
        kea_tag_map *Map = KEABuilder->TagMaps + I;
        TagBitsetOffsets[I] = PossiableUniqueCount;
        PossiableUniqueCount += Map->ValueCount;
    }

    u32 BitsetBlockCount = (PossiableUniqueCount + 31) / 32;
    u32 UniqueCount = 0;
    u32 *UniqueBitset = PushArray(TempMem, BitsetBlockCount, u32);
    
    for(u32 I = 0;
        I < KEABuilder->KESAHeader->AssetCount;
        ++I)
    {
        kesa_asset *KESAAsset = KEABuilder->KESAAssets + I;
        for(u32 J = 0;
            J < KESAAsset->TagCount;
            ++J)
        {
            kesa_tag *Tag = KESAAsset->AssetTags + J;
            s32 TagIndex = GetTagMapByGUID(KEABuilder->TagMaps,
                                           KEABuilder->KETHeader->TagCount,
                                           Tag->TagGUID);
            
            u32 BitIndex = TagBitsetOffsets[TagIndex] + Tag->TagValueIndex;
            if(!TestBitsetBit(UniqueBitset, BitIndex))
            {
                SetBitsetBit(UniqueBitset, BitIndex);
                ++UniqueCount;
            }
        }
    }

    u32 UsedTagsCount = 0;
    kea_tag *UsedTags = PushArray(TempMem, UniqueCount, kea_tag);
    for(u32 I = 0;
        I < PossiableUniqueCount;
        ++I)
    {
        if(TestBitsetBit(UniqueBitset, I))
        {
            s32 Low = 0;
            s32 High = KEABuilder->KETHeader->TagCount;
            s32 ResultIndex = -1;
            while(Low <= High)
            {
                s32 Mid = Low + ((High - Low) >> 1);
                if(TagBitsetOffsets[Mid] <= I)
                {
                    ResultIndex = Mid;
                    Low = Mid + 1;
                }
                else
                    High = Mid - 1;
            }

            UsedTags[UsedTagsCount].TagIndex = ResultIndex;
            UsedTags[UsedTagsCount].TagValueIndex = I - TagBitsetOffsets[ResultIndex];
        }
    }
    
    return(Result);
}

internal void
PrepareKEABuilder(kea_builder *KEABuilder, memory_arena *TempMem)
{
    kea_tag *UsedTags = GetUsedTags(KEABuilder, TempMem);
}

struct builder_added_asset
{
    u32 ID;
    ssa_asset *SSA;
    builder_asset_source *Source;
};

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
AddTileAsset(kea_builder *Builder, kesa_asset *Tileset, added_asset_additional_data *Data, u32 TileIndex)
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
    for(u32 I = 0;
        I < KESAAsset->Tileset.TileCount;
        ++I)
    {
        AddTileAsset(Builder, Asset, &BuilderAsset->Data, I);
    }

    Builder->CurrentAsset = BuilderAsset;
}

internal void
AddSpriteAsset(kea_builder *Builder, kesa_asset *SpriteSheet, added_asset_additional_data *Data, u32 SpriteIndex)
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
    for(u32 I = 0;
        I < KESAAsset->SpriteSheet.SpriteCount;
        ++I)
    {
        AddSpriteAsset(Builder, Asset, &BuilderAsset->Data, I);
    }

    Builder->CurrentAsset = BuilderAsset;
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
AddTag(kea_builder *Builder, u32 TagIndex, u32 TagValueIndex)
{
    Assert(Builder->CurrentAsset);

    kea_asset *Asset = &Builder->CurrentAsset->KEAAsset;
    ++Asset->OnePastLastTagIndex;
    Builder->CurrentTag = PushStruct(Builder->TempMem, kea_builder_tag_list);
    Builder->CurrentTag->TagIndex = Builder->TagCount;
    Builder->CurrentTag->Tag.TagIndex = TagIndex;
    Builder->CurrentTag->Tag.TagValueIndex = TagValueIndex;
    ++Builder->TagCount;

    Builder->CurrentTag->Next = Builder->Tags->Next;
    Builder->Tags->Next = Builder->CurrentTag;
    Builder->CurrentTag = 0;
}

inline void
AddTag(kea_builder *Builder, kea_tag Tag)
{
    AddTag(Builder, Tag.TagIndex, Tag.TagValueIndex);
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
        AddTag(Builder, TagIndex, Tag.TagValueIndex);
    }    
}

internal b32
BuildKEA(editor_mode_assets *AssetsMode, memory_arena *TempMem)
{
    b32 Result = true;

    kea_builder *KEABuilder = InitKEABuilder(AssetsMode, TempMem);

    kesa_header *StoredHeader = KEABuilder->KESAHeader;
    for(u32 StoredAssetIndex = 1;
        StoredAssetIndex < StoredHeader->AssetCount;
        ++StoredAssetIndex)
    {
        kesa_asset *StoredAsset = KEABuilder->KESAAssets + StoredAssetIndex;
        switch(StoredAsset->Type)
        {
            case KESA_Bitmap:
            {
                AddBitmapAsset(KEABuilder, StoredAsset);
                AddStoredAssetTags(KEABuilder, StoredAsset);
            } break;

            case KESA_SpriteSheet:
            {
                AddSpriteSheetAsset(KEABuilder, StoredAsset);
                AddStoredAssetTags(KEABuilder, StoredAsset);
            } break;

            case KESA_Tileset:
            {
                AddTilesetAsset(KEABuilder, StoredAsset);
                AddStoredAssetTags(KEABuilder, StoredAsset);
            } break;

            case KESA_Sound:
            {
            } break;

            case KESA_Text:
            {
            } break;

            case KESA_File:
            {
            } break;

            case KESA_SSWM:
            {
            } break;

            InvalidDefaultCase;
        }

        KEABuilder->CurrentAsset = 0;
    }

//    PrepareKEABuilder(KEABuilder, TempMem);

    return(Result);
}

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

// NOTE(pvlso): The normal map of a sprite or tile (Image), from its sheet's _n file
internal void
AddNormalMapAsset(kea_builder *Builder, kesa_asset *Asset, kea_asset *Image, loaded_bitmap *Normals)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);

    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    KEAAsset->GUID = NormalMapGUID(Image->GUID);
    KEAAsset->Type = KEAType_NormalMap;
    KEAAsset->Bitmap.AlignPercentage[0] = Image->Bitmap.AlignPercentage[0];
    KEAAsset->Bitmap.AlignPercentage[1] = Image->Bitmap.AlignPercentage[1];
    BuilderAsset->Data.Sprite.SpriteBitmap = Normals;

    Builder->CurrentAsset = 0;
}

// NOTE(pvlso): One cell of a curated tile set's sheets, cut into its own bitmap
internal void
AddTileAsset(kea_builder *Builder, kesa_asset *Tileset, u64 GUID, loaded_bitmap *TileBitmap)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Tileset);

    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    KEAAsset->GUID = GUID;
    KEAAsset->Type = KEAType_Tile;
    KEAAsset->Bitmap.AlignPercentage[0] = 0.5f;
    KEAAsset->Bitmap.AlignPercentage[1] = 0.5f;
    BuilderAsset->Data.Tile.TileBitmap = TileBitmap;

    Builder->CurrentAsset = 0;
}

// NOTE(pvlso): The bitmap GUID of a sheet cell in a set: the same cell used twice in a set (a tile and another's frame)
// is one bitmap, two sets never share one
internal u64
TileCellGUID(kea_builder *Builder, kesa_asset *Asset, tileset_file *File, tileset_file_cell Cell)
{
    FormatString(ArrayCount(Builder->Buffer), Builder->Buffer, "%s|%s|%d,%d",
                 Asset->SourceFileName, File->Sheets[Cell.Sheet], Cell.X, Cell.Y);
    u64 Result = GUIDFromString(Builder->Buffer);

    return(Result);
}

// NOTE(pvlso): A curated tile set (engine_tileset_file.cpp): its text file read, every cell it uses cut from its sheets
// into a tile bitmap, the tiles' records (place in the set's grid, flags, name, frames) as the set's data
internal void
AddTilesetAsset(kea_builder *Builder, kesa_asset *Asset)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);
    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    KEAAsset->GUID = Asset->GUID;
    KEAAsset->Type = KEAType_Tileset;
    Builder->CurrentAsset = 0;

    tile_flag_names FlagNames = ReadTileFlagNames(Builder->TempMem);
    tileset_file File = ReadTilesetFile(Asset->SourceFileName, &FlagNames, Builder->TempMem);

    loaded_bitmap *Sheets = PushArray(Builder->TempMem, TILESET_MAX_SHEETS, loaded_bitmap);
    for(u32 SheetIndex = 0; SheetIndex < File.SheetCount; ++SheetIndex)
    {
        Sheets[SheetIndex] = LoadImageFile(File.Sheets[SheetIndex], PlatformFileType_DataRoot, Builder->TempMem);
        if(!Sheets[SheetIndex].Memory)
        {
            LOG_ERROR("Tile set %s: can't read the sheet %s", Asset->SourceFileName, File.Sheets[SheetIndex]);
        }
    }

    u32 FrameCount = 0;
    for(u32 TileIndex = 0; TileIndex < File.TileCount; ++TileIndex)
    {
        if(File.Tiles[TileIndex].FrameCount > 1) {FrameCount += File.Tiles[TileIndex].FrameCount;}
    }

    KEAAsset->Tileset.TileCount = File.TileCount;
    KEAAsset->Tileset.GridWidth = (u16)File.GridWidth;
    KEAAsset->Tileset.GridHeight = (u16)File.GridHeight;
    KEAAsset->Tileset.FrameCount = FrameCount;
    KEAAsset->Tileset.TileSize = File.TileSize;
    builder_added_tileset *Data = &BuilderAsset->Data.Tileset;
    Data->Tiles = PushArray(Builder->TempMem, Maximum(1, File.TileCount), kea_tile);
    Data->FrameGUIDs = PushArray(Builder->TempMem, Maximum(1, FrameCount), u64);

    u32 AddedCount = 0;
    u64 *Added = PushArray(Builder->TempMem, File.TileCount*TILESET_MAX_TILE_FRAMES + 1, u64);
    u32 FrameAt = 0;
    for(u32 TileIndex = 0; TileIndex < File.TileCount; ++TileIndex)
    {
        tileset_file_tile *Source = File.Tiles + TileIndex;
        kea_tile *Tile = Data->Tiles + TileIndex;
        Tile->Flags = Source->Flags;
        Tile->Kind = Source->Kind;
        Tile->GridX = Source->GridX;
        Tile->GridY = Source->GridY;
        Tile->FramesPerSecond = Source->FramesPerSecond;
        FormatString(KEA_TILE_NAME_LENGTH, Tile->Name, "%s", Source->Name);
        if(Source->FrameCount > 1)
        {
            Tile->FirstFrame = FrameAt;
            Tile->FrameCount = Source->FrameCount;
        }

        for(u32 FrameIndex = 0; FrameIndex < Source->FrameCount; ++FrameIndex)
        {
            tileset_file_cell Cell = Source->Frames[FrameIndex];
            u64 GUID = TileCellGUID(Builder, Asset, &File, Cell);
            if(FrameIndex == 0) {Tile->BitmapGUID = GUID;}
            if(Source->FrameCount > 1) {Data->FrameGUIDs[FrameAt++] = GUID;}

            b32 AlreadyAdded = false;
            for(u32 Index = 0; Index < AddedCount; ++Index)
            {
                if(Added[Index] == GUID) {AlreadyAdded = true;}
            }
            if(!AlreadyAdded)
            {
                loaded_bitmap *Bitmap = PushStruct(Builder->TempMem, loaded_bitmap);
                *Bitmap = CutTilesetCell(Sheets + Cell.Sheet, File.TileSize, Cell.X, Cell.Y, Builder->TempMem);
                if(!Bitmap->Memory)
                {
                    LOG_ERROR("Tile set %s: cell %d,%d is outside %s, the tile is left empty", Asset->SourceFileName,
                              Cell.X, Cell.Y, File.Sheets[Cell.Sheet]);
                    Bitmap->Width = Bitmap->Height = File.TileSize;
                    Bitmap->WidthOverHeight = 1.0f;
                    Bitmap->AlignPercentage = V2(0.5f, 0.5f);
                    Bitmap->Pitch = File.TileSize*BITMAP_BYTES_PER_PIXEL;
                    Bitmap->Memory = PushSize(Builder->TempMem, Bitmap->Height*Bitmap->Pitch);
                }
                AddTileAsset(Builder, Asset, GUID, Bitmap);
                Added[AddedCount++] = GUID;
            }
        }
    }
    LOG("Tile set %s: %u tiles from %u sheets, %u bitmaps%s", Asset->SourceFileName, File.TileCount, File.SheetCount,
        AddedCount, File.ErrorCount ? " (with mistakes, see above)" : "");

    Builder->CurrentAsset = BuilderAsset;
}

// NOTE(pvlso): A map (engine_map_file.h): its sets become their tile set assets' GUIDs (made of their file names, as
// the stored tile sets' are), each cell's grid place in its set becomes the tile's index there
internal void
AddMapAsset(kea_builder *Builder, kesa_asset *Asset)
{
    kea_builder_added_asset_list *BuilderAsset = BuilderAddAsset(Builder, Asset);
    kea_asset *KEAAsset = &BuilderAsset->KEAAsset;
    KEAAsset->GUID = Asset->GUID;
    KEAAsset->Type = KEAType_Map;
    Builder->CurrentAsset = 0;

    map_file Map = ReadMapFile(Asset->SourceFileName, Builder->TempMem);
    KEAAsset->Map.Width = Map.Width;
    KEAAsset->Map.Height = Map.Height;
    KEAAsset->Map.LayerCount = (u16)Map.LayerCount;
    KEAAsset->Map.SetCount = (u16)Map.SetCount;
    builder_added_map *Data = &BuilderAsset->Data.Map;
    Data->SetGUIDs = PushArray(Builder->TempMem, Maximum(1, Map.SetCount), u64);
    Data->Layers = PushArray(Builder->TempMem, Maximum(1, Map.LayerCount), kea_map_layer);
    umm CellCount = (umm)Map.Width*Map.Height;
    Data->Cells = PushArray(Builder->TempMem, Maximum(1, Map.LayerCount*CellCount), kea_map_cell);

    // NOTE(pvlso): Each set's tiles by their place in its grid, in the order the set's builder numbers them
    tile_flag_names FlagNames = ReadTileFlagNames(Builder->TempMem);
    s32 *TileAt[MAP_FILE_MAX_SETS] = {};
    u32 GridWidth[MAP_FILE_MAX_SETS] = {};
    u32 GridHeight[MAP_FILE_MAX_SETS] = {};
    for(u32 SetIndex = 0; SetIndex < Map.SetCount; ++SetIndex)
    {
        Data->SetGUIDs[SetIndex] = GUIDFromString(Map.Sets[SetIndex]);
        b32 Stored = false;
        for(u32 Index = 1; Index < Builder->KESAHeader->AssetCount; ++Index)
        {
            if(Builder->KESAAssets[Index].GUID == Data->SetGUIDs[SetIndex]) {Stored = true;}
        }
        if(!Stored)
        {
            LOG_WARNING("Map %s uses the tile set %s, which isn't in the stored assets: add it, or the game can't draw "
                        "its tiles", Asset->SourceFileName, Map.Sets[SetIndex]);
        }

        tileset_file Set = ReadTilesetFile(Map.Sets[SetIndex], &FlagNames, Builder->TempMem);
        GridWidth[SetIndex] = Set.GridWidth;
        GridHeight[SetIndex] = Set.GridHeight;
        TileAt[SetIndex] = PushArray(Builder->TempMem, Maximum(1, Set.GridWidth*Set.GridHeight), s32);
        for(u32 Index = 0; Index < Set.GridWidth*Set.GridHeight; ++Index) {TileAt[SetIndex][Index] = -1;}
        for(u32 TileIndex = 0; TileIndex < Set.TileCount; ++TileIndex)
        {
            TileAt[SetIndex][Set.Tiles[TileIndex].GridY*Set.GridWidth + Set.Tiles[TileIndex].GridX] = (s32)TileIndex;
        }
    }

    u32 Missing = 0;
    for(u32 LayerIndex = 0; LayerIndex < Map.LayerCount; ++LayerIndex)
    {
        map_file_layer *Source = Map.Layers + LayerIndex;
        kea_map_layer *Layer = Data->Layers + LayerIndex;
        FormatString(sizeof(Layer->Name), Layer->Name, "%s", Source->Name);
        Layer->Z = Source->Z;
        Layer->Flags = Source->Hidden ? KEAMapLayer_Hidden : 0;
        kea_map_cell *Cells = Data->Cells + LayerIndex*CellCount;
        for(umm Index = 0; Index < CellCount; ++Index)
        {
            map_cell Cell = Source->Cells[Index];
            if(Cell.Set)
            {
                u32 Set = Cell.Set - 1;
                s32 Tile = ((Cell.X < GridWidth[Set]) && (Cell.Y < GridHeight[Set])) ?
                    TileAt[Set][Cell.Y*GridWidth[Set] + Cell.X] : -1;
                if(Tile >= 0)
                {
                    Cells[Index].Set = Cell.Set;
                    Cells[Index].Tile = (u16)Tile;
                }
                else
                {
                    ++Missing;
                }
            }
        }
    }
    if(Missing)
    {
        LOG_WARNING("Map %s: %u cells point at places where their set has no tile any more, left empty",
                    Asset->SourceFileName, Missing);
    }
    LOG("Map %s: %u x %u, %u layers, %u tile sets", Asset->SourceFileName, Map.Width, Map.Height, Map.LayerCount,
        Map.SetCount);

    Builder->CurrentAsset = BuilderAsset;
}

internal kea_asset *
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

    return(KEAAsset);
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

    // NOTE(pvlso): The sheet's normal map, when the art has one
    char NormalsFileName[256];
    NormalMapFileName(KESAAsset->SourceFileName, NormalsFileName, sizeof(NormalsFileName));
    builder_loaded_sprites Normals = {};
    if(Platform.FileExists(NormalsFileName, PlatformFileType_SSBMP))
    {
        Normals = LoadSprites(KESAAsset, Builder->TempMem, NormalsFileName, true);
    }

    for(u32 I = 0;
        I < KESAAsset->SpriteSheet.SpriteCount;
        ++I)
    {
        kea_asset *Sprite = AddSpriteAsset(Builder, Asset, &BuilderAsset->Data, I, Sprites.Sprites + I);
        if(I < Normals.Count)
        {
            AddNormalMapAsset(Builder, Asset, Sprite, Normals.Sprites + I);
        }
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
    TIMED_FUNCTION();

    kesa_header *KESAHeader = Builder->KESAHeader;

    char KEAFileName[256];
    FormatString(ArrayCount(KEAFileName), KEAFileName, "game_data_%d.kea",
                 KESAHeader->Version);
    platform_file_handle Out = Platform.OpenFile(KEAFileName, PlatformFileType_KEA, PlatformFileOp_Write);
    if(PlatformNoFileErrors(&Out))
    {
        // NOTE(pvlso): Every write goes to an explicit offset, At tracks where the next one lands
        u64 At = 0;

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
        At = Header.TagMapsOffset + TagMapArraySize;
        for(u32 TagMapIndex = 0;
            TagMapIndex < Header.TagCount;
            ++TagMapIndex)
        {
            kea_tag_map *TagMap = Builder->TagMaps + TagMapIndex;
            ket_tag *Tag = KETTags + TagMapIndex;
            Tag->GUID = TagMap->GUID;
            Copy(KET_TAG_KEY_LENGTH, TagMap->Key, Tag->Key);
            Tag->ValueCount = TagMap->ValueCount;
            Tag->DataOffset = At;

            Platform.WriteDataToFile(&Out, At, Tag->ValueCount*sizeof(u64), TagMap->ValueGUIDs);
            At += Tag->ValueCount*sizeof(u64);
            for(u32 StringIndex = 0;
                StringIndex < Tag->ValueCount;
                ++StringIndex)
            {
                Platform.WriteDataToFile(&Out, At, KET_TAG_KEY_LENGTH, TagMap->Values[StringIndex]);
                At += KET_TAG_KEY_LENGTH;
            }
        }

        Header.UsedTagsArrayOffset = At;
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
        
        // NOTE(pvlso): The header is written last, once every offset in it is known
        Platform.WriteDataToFile(&Out, Header.TagMapsOffset, TagMapArraySize, KETTags);
        At = Header.UsedTagsArrayOffset;
        Platform.WriteDataToFile(&Out, At, UsedTagsArraySize, Builder->UsedTags);
        At += UsedTagsArraySize;
        Platform.WriteDataToFile(&Out, At, TagHashTableDataSize, Builder->TableData);
        At += TagHashTableDataSize;
        Platform.WriteDataToFile(&Out, At, TagAssetsIndeciesSize, Builder->TagAssetsIndecies);
        At += TagAssetsIndeciesSize;

        At += AssetTypeArraySize;
        for(u32 Type = 0;
            Type < KEAType_Count;
            ++Type)
        {
            u32 TypeTableSize = sizeof(u32)*Builder->AssetTypeTable[Type].TypeCount;
            Builder->AssetTypeTable[Type].AssetsIndeciesOffset = At;
            Platform.WriteDataToFile(&Out, At, TypeTableSize, Builder->AssetTypeTableData[Type]);
            At += TypeTableSize;
        }

        Platform.WriteDataToFile(&Out, Header.AssetTypeTableOffset, AssetTypeArraySize, Builder->AssetTypeTable);
        At = Header.AssetsOffset + AssetArraySize;

        for(u32 AssetIndex = 1;
            AssetIndex < Header.AssetCount;
            ++AssetIndex)
        {
            kea_builder_added_asset_list *BuilderAsset = Builder->SortedBuilderAssets[AssetIndex];
            kea_asset *Dest = &BuilderAsset->KEAAsset;
            kesa_asset *Source = BuilderAsset->StoredAsset;
            Dest->DataOffset = At;

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
                    u32 ChannelSize = Dest->Sound.SampleCount*sizeof(s16);
                    Platform.WriteDataToFile(&Out, At, ChannelSize, WAV.Samples[ChannelIndex]);
                    At += ChannelSize;
                }
            }
            else if(Dest->Type == KEAType_Tileset)
            {
                u32 TilesSize = Dest->Tileset.TileCount*sizeof(kea_tile);
                Platform.WriteDataToFile(&Out, At, TilesSize, BuilderAsset->Data.Tileset.Tiles);
                At += TilesSize;

                u32 FramesSize = Dest->Tileset.FrameCount*sizeof(u64);
                Platform.WriteDataToFile(&Out, At, FramesSize, BuilderAsset->Data.Tileset.FrameGUIDs);
                At += FramesSize;
            }
            else if(Dest->Type == KEAType_Map)
            {
                kea_map *Map = &Dest->Map;
                umm CellCount = (umm)Map->LayerCount*Map->Width*Map->Height;
                u32 SetsSize = Map->SetCount*sizeof(u64);
                u32 LayersSize = Map->LayerCount*sizeof(kea_map_layer);
                u32 CellsSize = (u32)(CellCount*sizeof(kea_map_cell));
                Platform.WriteDataToFile(&Out, At, SetsSize, BuilderAsset->Data.Map.SetGUIDs);
                At += SetsSize;
                Platform.WriteDataToFile(&Out, At, LayersSize, BuilderAsset->Data.Map.Layers);
                At += LayersSize;
                Platform.WriteDataToFile(&Out, At, CellsSize, BuilderAsset->Data.Map.Cells);
                At += CellsSize;
            }
            else if(Dest->Type == KEAType_SpriteSheet)
            {
                u32 SpritesSize = Source->SpriteSheet.SpriteCount*sizeof(u64);
                Platform.WriteDataToFile(&Out, At, SpritesSize, BuilderAsset->Data.SpriteSheet.SpriteGUIDs);
                At += SpritesSize;

                Dest->SpriteSheet.SpriteCount = Source->SpriteSheet.SpriteCount;
            }
            else if(Dest->Type == KEAType_TXT)
            {
                loaded_text Text = LoadText(Source->SourceFileName, Builder->TempMem);

                Dest->Text.Length = StringLength(Text.String);
                u32 TextSize = Dest->Text.Length;
                Platform.WriteDataToFile(&Out, At, TextSize, Text.String);
                At += TextSize;
            }
            else if(Dest->Type == KEAType_BIN)
            {
                read_file_result ReadResult =
                    Platform.ReadEntireFile(Source->SourceFileName, PlatformFileType_BIN, Builder->TempMem, true);    

                Dest->BinaryFile.Size = ReadResult.Size;
                Platform.WriteDataToFile(&Out, At, ReadResult.Size, ReadResult.Contents);
                At += ReadResult.Size;
            }
            else
            {
                loaded_bitmap Bitmap = {};
                if(Dest->Type == KEAType_Tile)
                {
                    Bitmap = *BuilderAsset->Data.Tile.TileBitmap;
                }
                else if((Dest->Type == KEAType_Sprite) || (Dest->Type == KEAType_NormalMap))
                {
                    Bitmap = *BuilderAsset->Data.Sprite.SpriteBitmap;
                }
                else
                {
                    Assert(Source->Type == AssetType_Bitmap);
                    Bitmap = LoadImageFile(Source->SourceFileName, PlatformFileType_BMP, Builder->TempMem);
                }

                Dest->Bitmap.Dim[0] = Bitmap.Width;
                Dest->Bitmap.Dim[1] = Bitmap.Height;

                Assert((Bitmap.Width * BITMAP_BYTES_PER_PIXEL) == Bitmap.Pitch);
                u32 BitmapSize = Bitmap.Height*Bitmap.Pitch;
                Platform.WriteDataToFile(&Out, At, BitmapSize, Bitmap.Memory);
                At += BitmapSize;
            }
        }

        kea_asset *KEAAssets = PushArray(Builder->TempMem, Builder->AssetCount, kea_asset);
        for(u32 AssetIndex = 0;
            AssetIndex < Builder->AssetCount;
            ++AssetIndex)
        {
            KEAAssets[AssetIndex] = Builder->SortedBuilderAssets[AssetIndex]->KEAAsset;
        }
        
        Platform.WriteDataToFile(&Out, Header.AssetsOffset, AssetArraySize, KEAAssets);
        Platform.WriteDataToFile(&Out, 0, sizeof(kea_header), &Header);

        if(PlatformNoFileErrors(&Out))
        {
            LOG("Wrote %s: %u assets, %u tags", KEAFileName, Builder->AssetCount, Builder->TagCount);
        }
        else
        {
            LOG_ERROR("Writing %s failed", KEAFileName);
        }
    }
    else
    {
        LOG_ERROR("Could not create %s", KEAFileName);
    }

    Platform.CloseFile(&Out);
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
    TIMED_FUNCTION();

    b32 Result = true;

    kea_builder *Builder = InitKEABuilder(AssetsMode, TempMem);
    LOG("Building the KEA from %u stored assets", Builder->KESAHeader->AssetCount - 1);

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

            case KESA_Map:
                AddMapAsset(Builder, StoredAsset);
                break;

            case KESA_SSWM:
                LOG_ERROR("%s: SSWM assets can't be built yet", StoredAsset->SourceFileName);
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

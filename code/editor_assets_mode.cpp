/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

#include "engine_tileset_file.cpp"
#include "engine_kea_builder.cpp"
#include "engine_assets_mode_ui.cpp"

inline void
DeallocateAssetsModeTags(editor_mode_assets *AssetsMode)
{
    ket_header *Header = &AssetsMode->TagHeader;
    if(AssetsMode->TagKeys)
    {
        Platform.DeallocateMemory(AssetsMode->TagKeys);    
    }

    if(AssetsMode->Tags)
    {
        for(u32 I = 0; I < Header->TagCount; ++I)
        {
            kea_tag_map *TagMap = AssetsMode->Tags + I;
            Platform.DeallocateMemory(TagMap->ValueGUIDs);
            for(u32 J = 0; J < TagMap->ValueCount; ++J)
            {
                Platform.DeallocateMemory(TagMap->Values[J]);
            }
            Platform.DeallocateMemory(TagMap->Values);
        }

        Platform.DeallocateMemory(AssetsMode->Tags);
    }
}

internal b32
ReadTags(editor_mode_assets *AssetsMode)
{
    b32 Result = true;
    Assert(AssetsMode->StoredHeader.MagicValue);
    ket_header *Header = &AssetsMode->TagHeader;

    DeallocateAssetsModeTags(AssetsMode);
    
    u32 TagFullVersion = AssetsMode->StoredHeader.TagsVersion;
    u8 TagVersion[4] =
        {
            (u8)((TagFullVersion >> 24) & 0xff),
            (u8)((TagFullVersion >> 16) & 0xff),
            (u8)((TagFullVersion >>  8) & 0xff),
            (u8)((TagFullVersion >>  0) & 0xff),
        };

    char TagsFileName[256];
    FormatString(ArrayCount(TagsFileName), TagsFileName, "asset_tags_%d.%d.%d.%d.ket",
                 TagVersion[0], TagVersion[1],
                 TagVersion[2], TagVersion[3]);
    
    platform_file_handle KETHandle =
        Platform.OpenFile(TagsFileName, PlatformFileType_KET, PlatformFileOp_Read);
    if(PlatformNoFileErrors(&KETHandle))
    {
        Platform.ReadDataFromFile(&KETHandle, 0, sizeof(ket_header), Header);

        Assert(Header->Version == AssetsMode->StoredHeader.TagsVersion);
        Assert(Header->TagCount);

        u32 TagsSize = Header->TagCount*sizeof(ket_tag);

        temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);
        ket_tag *RawTags = PushArray(TempMem.Arena, Header->TagCount, ket_tag);
        Platform.ReadDataFromFile(&KETHandle, Header->TagArrayOffset, TagsSize, RawTags);

        AssetsMode->TagKeys = (char **)Platform.AllocateMemory(Header->TagCount*sizeof(char *));
        AssetsMode->Tags = (kea_tag_map *)Platform.AllocateMemory(Header->TagCount*sizeof(kea_tag_map));
        for(u32 I = 0; I < Header->TagCount; ++I)
        {
            ket_tag *Tag = RawTags + I;
            kea_tag_map *Map = AssetsMode->Tags + I;
            Map->Values = (char **)Platform.AllocateMemory(Tag->ValueCount*sizeof(char *));
            Map->ValueGUIDs = (u64 *)Platform.AllocateMemory(Tag->ValueCount*sizeof(u64));
            for(u32 J = 0; J < Tag->ValueCount; ++J)
            {
                Map->Values[J] = (char *)Platform.AllocateMemory(KET_TAG_KEY_LENGTH);
            }

            Map->GUID = Tag->GUID;
            Copy(KET_TAG_KEY_LENGTH, Tag->Key, Map->Key);
            Map->ValueCount = Tag->ValueCount;

            u32 SizeOfGUIDs = Map->ValueCount*sizeof(u64);
            Platform.ReadDataFromFile(&KETHandle, Tag->DataOffset, SizeOfGUIDs, Map->ValueGUIDs);
            
            u32 StringLength = Tag->ValueCount*KET_TAG_KEY_LENGTH;
            char *ValuesString = PushArray(TempMem.Arena, StringLength, char);
            Platform.ReadDataFromFile(&KETHandle, Tag->DataOffset + SizeOfGUIDs, StringLength, ValuesString);
            char *StringAt = ValuesString;

            for(u32 J = 0; J < Map->ValueCount; ++J)
            {
                Copy(KET_TAG_KEY_LENGTH, StringAt, Map->Values[J]);
                StringAt += KET_TAG_KEY_LENGTH;
            }

            AssetsMode->TagKeys[I] = Map->Key;
        }

        EndTemporaryMemory(TempMem);
        
        Platform.CloseFile(&KETHandle);

        LOG("Read %s: %u tags", TagsFileName, Header->TagCount);
    }
    else
    {
        Result = false;
        LOG_WARNING("No %s, writing one with only Tag_None", TagsFileName);
        KETHandle = Platform.OpenFile(TagsFileName, PlatformFileType_KET, PlatformFileOp_Write);
        if(PlatformNoFileErrors(&KETHandle))
        {
            Header->MagicValue = KET_MAGIC_VALUE;
            Header->Version = TagFullVersion;
            Header->TagCount = 1;
            Header->TagArrayOffset = sizeof(ket_header);

            ket_tag NullTag = {};
            FormatString(ArrayCount(NullTag.Key), NullTag.Key, "Tag_None");
            NullTag.ValueCount = 1;
            NullTag.DataOffset = Header->TagArrayOffset + sizeof(ket_tag);

            
            char Value[KET_TAG_KEY_LENGTH] = {};
            FormatString(ArrayCount(Value), Value, "None");
            u64 ValueGUID = GUIDFromString(Value);

            Platform.WriteDataToFile(&KETHandle, 0, sizeof(ket_header), Header);
            Platform.WriteDataToFile(&KETHandle, Header->TagArrayOffset, sizeof(ket_tag), &NullTag);
            Platform.WriteDataToFile(&KETHandle, NullTag.DataOffset, sizeof(u64), &ValueGUID);
            Platform.WriteDataToFile(&KETHandle, NullTag.DataOffset + sizeof(u64), KET_TAG_KEY_LENGTH, Value);

            Platform.CloseFile(&KETHandle);
        }
        else
        {
            LOG_ERROR("Could not create %s", TagsFileName);
        }
    }

    return(Result);
}

internal b32
ReadStoredAssets(editor_state *EditorState, editor_mode_assets *AssetsMode)
{
    TIMED_FUNCTION();

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

        u32 AssetsSize = StoredHeader->AssetCount*StoredHeader->SizeOfStoredAsset;
        Platform.DeallocateMemory(AssetsMode->StoredAssets);
        AssetsMode->StoredAssets = (kesa_asset *)Platform.AllocateMemory(AssetsSize);

        Platform.ReadDataFromFile(&KESAHandle, StoredHeader->AssetsOffset,
                                  AssetsSize, AssetsMode->StoredAssets);
        
        Platform.CloseFile(&KESAHandle);

        // NOTE(pvlso): The first stored asset is the null asset
        LOG("Read %s: %u stored assets", StoredFileName, StoredHeader->AssetCount - 1);
    }
    else
    {
        Result = false;
        LOG_WARNING("No %s, writing an empty one", StoredFileName);
        KESAHandle = Platform.OpenFile(StoredFileName, PlatformFileType_KESA, PlatformFileOp_Write);
        if(PlatformNoFileErrors(&KESAHandle))
        {
            StoredHeader->MagicValue = KESA_MAGIC_VALUE;
            StoredHeader->Version = FullVersion;
            StoredHeader->TagsVersion = 0;
            StoredHeader->SizeOfStoredAsset = sizeof(kesa_asset);
            StoredHeader->AssetCount = 1;
            StoredHeader->AssetsOffset = sizeof(kesa_header);

            Platform.WriteDataToFile(&KESAHandle, 0, sizeof(kesa_header), StoredHeader);
        
            kesa_asset NullAsset = {};
            Platform.WriteDataToFile(&KESAHandle, StoredHeader->AssetsOffset,
                                     sizeof(kesa_asset), &NullAsset);

            Platform.CloseFile(&KESAHandle);
        }
        else
        {
            LOG_ERROR("Could not create %s", StoredFileName);
        }
    }

    return(Result);
}



internal void
WriteStoredAssets(editor_state *EditorState, editor_mode_assets *AssetsMode)
{
    TIMED_FUNCTION();

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
            StoredHeader->TagsVersion = AssetsMode->TagHeader.Version;

            u32 ExistingAssetsCount = StoredHeader->AssetCount;
            StoredHeader->AssetCount += AssetsMode->AddAssetCount;

            Platform.WriteDataToFile(&KESAHandle, 0, sizeof(kesa_header), StoredHeader);

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

            LOG("Wrote %s: %u stored assets, %u new", FileName, StoredHeader->AssetCount - 1, AssetsMode->AddAssetCount);
        }
        else
        {
            LOG_ERROR("Could not write %s", FileName);
        }
    }
    else
    {
        LOG("Write Assets: nothing added or changed");
    }

    AssetsMode->AddAssetCount = 0;
    ZeroArray(ArrayCount(AssetsMode->AssetsToAdd), AssetsMode->AssetsToAdd);
    ReadStoredAssets(EditorState, AssetsMode);
}

internal void
PlayAssetsMode(editor_state *EditorState, transient_state *TranState, char *ProjectName)
{
    // NOTE(pvlso): ProjectName lives in the mode arena, which SetEditorMode frees
    char Name[PROJECT_NAME_COUNT];
    FormatString(sizeof(Name), Name, "%s", ProjectName);

    SetEditorMode(EditorState, TranState, EditorMode_AssetsMode);
    LOG("Assets Mode for %s", Name);

    // NOTE(pvlso): Every file below (source files, stored assets, tags, the KEA it writes) is the project's
    UseDataOf(EditorState, Name);

    editor_mode_assets *Result = PushStruct(&EditorState->ModeArena, editor_mode_assets);
    FormatString(sizeof(Result->ProjectName), Result->ProjectName, "%s", Name);
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
    }

    EditorState->AssetsMode = Result;
}

internal void
CutSpriteSheet(platform_texture_op_queue *TextureOpQueue, spritesheet_mode *SpriteSheetMode,
               kesa_spritesheet *Asset)
{
    TIMED_FUNCTION();

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
        AddOp(TextureOpQueue, &Op);
    }
}

internal void AllocateBitmap(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Bitmap);

// NOTE(pvlso): Reads a curated tile set and cuts each tile's first frame from its sheets, to show the set
internal void
LoadTilesetView(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode, kesa_asset *Asset)
{
    TIMED_FUNCTION();

    memory_arena *Arena = &TilesetMode->Arena;
    TilesetMode->FlagNames = ReadTileFlagNames(Arena);
    TilesetMode->File = ReadTilesetFile(Asset->SourceFileName, &TilesetMode->FlagNames, Arena);
    TilesetMode->HoverTile = -1;
    tileset_file *File = &TilesetMode->File;

    loaded_bitmap *Sheets = PushArray(Arena, TILESET_MAX_SHEETS, loaded_bitmap);
    for(u32 SheetIndex = 0; SheetIndex < File->SheetCount; ++SheetIndex)
    {
        Sheets[SheetIndex] = LoadImageFile(File->Sheets[SheetIndex], PlatformFileType_DataRoot, Arena);
        if(!Sheets[SheetIndex].Memory)
        {
            LOG_ERROR("Tile set %s: can't read the sheet %s", Asset->SourceFileName, File->Sheets[SheetIndex]);
        }
    }

    for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
    {
        tileset_file_cell Cell = File->Tiles[TileIndex].Frames[0];
        loaded_bitmap *Tile = TilesetMode->Tiles + TileIndex;
        *Tile = CutTilesetCell(Sheets + Cell.Sheet, File->TileSize, Cell.X, Cell.Y, Arena);
        if(Tile->Memory)
        {
            AllocateBitmap(TextureOpQueue, Tile);
        }
    }

    Asset->Tileset.TileCount = File->TileCount;
    Asset->Tileset.GridWidth = File->GridWidth;
    Asset->Tileset.GridHeight = File->GridHeight;
    Asset->Tileset.TileSize = File->TileSize;
    TilesetMode->Loaded = File->Loaded;
}

// NOTE(pvlso): The tiles' pixels are in the view's arena, so only their textures are released one by one
internal void
ClearTilesetView(platform_texture_op_queue *TextureOpQueue, tileset_mode *TilesetMode)
{
    for(u32 TileIndex = 0; TileIndex < ArrayCount(TilesetMode->Tiles); ++TileIndex)
    {
        loaded_bitmap *Tile = TilesetMode->Tiles + TileIndex;
        if(Tile->TextureHandle)
        {
            texture_op Op = {};
            Op.IsAllocate = false;
            Op.Deallocate.Handle = Tile->TextureHandle;
            AddOp(TextureOpQueue, &Op);
        }
    }
    Clear(&TilesetMode->Arena);
}

internal void
AddTagToCurrentAsset(editor_mode_assets *AssetsMode, kesa_asset *CurrentAsset)
{
    kesa_tag *Tag = CurrentAsset->AssetTags + CurrentAsset->TagCount++;
    kea_tag_map *Map = AssetsMode->Tags + AssetsMode->CurrentTagIndex;

    Tag->TagGUID = Map->GUID;
    Tag->TagValueIndex = AssetsMode->CurrentTagValue;
}

internal void
AddCurrentAsset(editor_mode_assets *AssetsMode)
{
    AssetsMode->AssetsToAdd[AssetsMode->AddAssetCount].GUID = GUIDFromString(AssetsMode->CurrentAsset->SourceFileName);
    ++AssetsMode->AddAssetCount;

    LOG("Added %s, %u asset(s) waiting for Write Assets", AssetsMode->CurrentAsset->SourceFileName,
        AssetsMode->AddAssetCount);
}

inline void
TagInsertionSort(u32 Count, ket_tag *Nodes)
{
    for(u32 I = 1; I < Count; ++I)
    {
        ket_tag Node = Nodes[I];
        s32 J = I - 1;

        while((J >= 0) && (Node.GUID < Nodes[J].GUID))
        {
            Nodes[J + 1] = Nodes[J];
            J = J - 1;
        }

        Nodes[J + 1] = Node;
    }
}

enum update_tags_op
{
    UpdateTagsOp_AddNew,
    UpdateTagsOp_Edit,
    UpdateTagsOp_Remove,
};

internal void
UpdateEditorTags(editor_mode_assets *AssetsMode, update_tags_op Op)
{
    TIMED_FUNCTION();

    temporary_memory TempMem = BeginTemporaryMemory(&AssetsMode->UtilityTempArena);
    b32 Result = true;
    Assert(AssetsMode->StoredHeader.MagicValue);
    ket_header *Header = &AssetsMode->TagHeader;
   
    u32 TagFullVersion = ++AssetsMode->StoredHeader.TagsVersion;
    u8 TagVersion[4] =
        {
            (u8)((TagFullVersion >> 24) & 0xff),
            (u8)((TagFullVersion >> 16) & 0xff),
            (u8)((TagFullVersion >>  8) & 0xff),
            (u8)((TagFullVersion >>  0) & 0xff),
        };

    char FileName[256];
    FormatString(ArrayCount(FileName), FileName, "asset_tags_%d.%d.%d.%d.ket",
                 TagVersion[0], TagVersion[1],
                 TagVersion[2], TagVersion[3]);
    
    platform_file_handle KETHandle =
        Platform.OpenFile(FileName, PlatformFileType_KET, PlatformFileOp_Write);
    if(PlatformNoFileErrors(&KETHandle))
    {
        ket_tag *ExistingTags = 0;
        new_tag_map *NewTag = &AssetsMode->NewTag;

        char Buffer[KET_TAG_KEY_LENGTH*2];

        Header->TagCount += (Op == UpdateTagsOp_AddNew) ? 1 : 0;

        u32 AllocTagCount = (Op == UpdateTagsOp_Remove) ? (Header->TagCount - 1) : Header->TagCount;
        ExistingTags = PushArray(TempMem.Arena, AllocTagCount, ket_tag);
        u64 DataOffset = Header->TagArrayOffset + AllocTagCount*sizeof(ket_tag);

        u32 *TagStringSizes = PushArray(TempMem.Arena, AllocTagCount, u32);
        char **TagStrings = PushArray(TempMem.Arena, AllocTagCount, char *);
        u32 IndexDecrement = 0;
        for(u32 I = 0; I < Header->TagCount; ++I)
        {
            u32 Index = I - IndexDecrement;

            kea_tag_map *TagMap = AssetsMode->Tags + I;
            ket_tag *Tag = ExistingTags + Index;

            if(StringsAreEqual(TagMap->Key, "Remove"))
            {
                IndexDecrement = 1;
                continue;
            }

            Tag->DataOffset = DataOffset;
            if(((Op == UpdateTagsOp_Edit) && (TagMap->GUID == NewTag->Tag.GUID)) ||
               ((Op == UpdateTagsOp_AddNew) && (I == (Header->TagCount - 1))))
            {
                Assert(I == Index);

                NewTag->Tag.GUID = GUIDFromString(NewTag->Tag.Key);
                TagStringSizes[Index] = NewTag->Tag.ValueCount*KET_TAG_KEY_LENGTH;
                TagStrings[Index] = PushArray(TempMem.Arena, TagStringSizes[Index], char);
                NewTag->Tag.ValueGUIDs = PushArray(TempMem.Arena, NewTag->Tag.ValueCount, u64);

                
                new_tag_map_value *Prev = 0;
                new_tag_map_value *Cur = NewTag->ValuesHead.Next;
                while(Cur != 0)
                {
                    new_tag_map_value *Next = Cur->Next;
                    Cur->Next = Prev;
                    Prev = Cur;
                    Cur = Next;
                        
                }
                NewTag->ValuesHead.Next = Prev;
                
                char *TagStringAt = TagStrings[Index];
                for(u32 ValueIndex = 0;
                    ValueIndex < NewTag->Tag.ValueCount;
                    ++ValueIndex)
                {
                    new_tag_map_value *Remove = NewTag->ValuesHead.Next;
                    NewTag->ValuesHead.Next = Remove->Next;                        

                    Remove->Next = NewTag->Free;
                    NewTag->Free = Remove;

                    Copy(KET_TAG_KEY_LENGTH, Remove->Value, TagStringAt);
                    TagStringAt += KET_TAG_KEY_LENGTH;

                    FormatString(ArrayCount(Buffer), Buffer, "%s%s", NewTag->Tag.Key, Remove->Value);
                    NewTag->Tag.ValueGUIDs[ValueIndex] = GUIDFromString(Buffer);

                    ZeroStruct(*Remove);
                }

                Tag->GUID = NewTag->Tag.GUID;
                Copy(KET_TAG_KEY_LENGTH, NewTag->Tag.Key, Tag->Key);
                Tag->ValueCount = NewTag->Tag.ValueCount;

                u32 GUIDsSize = Tag->ValueCount*sizeof(u64);
                Platform.WriteDataToFile(&KETHandle, Tag->DataOffset, GUIDsSize, NewTag->Tag.ValueGUIDs);
                DataOffset += GUIDsSize;

                ZeroStruct(NewTag->Tag);
            }
            else
            {
                TagStringSizes[Index] = TagMap->ValueCount*KET_TAG_KEY_LENGTH;
                TagStrings[Index] = PushArray(TempMem.Arena, TagStringSizes[Index], char);

                char *TagStringAt = TagStrings[Index];
                for(u32 ValueIndex = 0;
                    ValueIndex < TagMap->ValueCount;
                    ++ValueIndex)
                {
                    Copy(KET_TAG_KEY_LENGTH, TagMap->Values[ValueIndex], TagStringAt);
                    TagStringAt += KET_TAG_KEY_LENGTH;
                }

                Tag->GUID = TagMap->GUID;
                Copy(KET_TAG_KEY_LENGTH, TagMap->Key, Tag->Key);
                Tag->ValueCount = TagMap->ValueCount;

                u32 GUIDsSize = Tag->ValueCount*sizeof(u64);
                Platform.WriteDataToFile(&KETHandle, Tag->DataOffset, GUIDsSize, TagMap->ValueGUIDs);
                DataOffset += GUIDsSize;
            }

            Platform.WriteDataToFile(&KETHandle, DataOffset, TagStringSizes[Index], TagStrings[Index]);
            DataOffset += TagStringSizes[Index];
        }

        Header->TagCount -= IndexDecrement;
        
        TagInsertionSort(Header->TagCount, ExistingTags);
            
        Header->Version = TagFullVersion;
        Platform.WriteDataToFile(&KETHandle, 0, sizeof(ket_header), Header);
        Platform.WriteDataToFile(&KETHandle, sizeof(ket_header), sizeof(ket_tag)*Header->TagCount, ExistingTags);

        Platform.CloseFile(&KETHandle);
    }
    else
    {
        // TODO(pvlso): Logging
    }
    
    EndTemporaryMemory(TempMem);

    u8 StoredVersion[4] =
        {
            (u8)((AssetsMode->StoredHeader.Version >> 24) & 0xff),
            (u8)((AssetsMode->StoredHeader.Version >> 16) & 0xff),
            (u8)((AssetsMode->StoredHeader.Version >>  8) & 0xff),
            (u8)((AssetsMode->StoredHeader.Version >>  0) & 0xff),
        };

    FormatString(ArrayCount(FileName), FileName, "stored_assets_%d.%d.%d.%d.kesa",
                 StoredVersion[0], StoredVersion[1],
                 StoredVersion[2], StoredVersion[3]);

    //NOTE(pvlso): Update stored assets file header
    platform_file_handle KESAHandle =
        Platform.OpenFile(FileName, PlatformFileType_KESA, PlatformFileOp_WriteExisting);
    if(PlatformNoFileErrors(&KESAHandle))
    {
        Platform.WriteDataToFile(&KESAHandle, 0, sizeof(kesa_header), &AssetsMode->StoredHeader);
        Platform.CloseFile(&KESAHandle);
    }
    else
    {
        LOG_ERROR("Could not update the header of %s", FileName);
    }

    char *OpNames[] = {"added", "edited", "removed"};
    LOG("Tag %s, tags version is now %u", (Op < ArrayCount(OpNames)) ? OpNames[Op] : "?", TagFullVersion);

    AssetsMode->CurrentTagIndex = 0;
    
    //NOTE(pvlso): Update tags
    ReadTags(AssetsMode);
}

internal void
RemoveCurrentAssetTag(editor_mode_assets *AssetsMode, kesa_asset *CurrentAsset)
{
    if(CurrentAsset->TagCount)
    {
        for(u32 TagIndex = AssetsMode->CurrentStoredTagIndex;
            TagIndex < (ArrayCount(CurrentAsset->AssetTags) - 1);
            ++TagIndex)
        {
            CurrentAsset->AssetTags[TagIndex] = CurrentAsset->AssetTags[TagIndex + 1];        
        }

        kesa_tag *Tag = CurrentAsset->AssetTags + (ArrayCount(CurrentAsset->AssetTags) - 1);
        *Tag = {};

        --CurrentAsset->TagCount;
    }
}

internal void
DeallocateBitmap(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Bitmap)
{
    if(Bitmap)
    {
        if(Bitmap->TextureHandle)
        {
            texture_op Op = {};
            Op.IsAllocate = false;
            Op.Deallocate.Handle = Bitmap->TextureHandle;
            AddOp(TextureOpQueue, &Op);
        }

        Platform.FreeFileMemory(Bitmap->Memory);
    }
}

internal void
AllocateBitmap(platform_texture_op_queue *TextureOpQueue, loaded_bitmap *Bitmap)
{
    texture_op Op = {};
    Op.IsAllocate = true;
    Op.Allocate.Width = Bitmap->Width;
    Op.Allocate.Height = Bitmap->Height;
    Op.Allocate.Data = Bitmap->Memory;
    Op.Allocate.ResultHandle = &Bitmap->TextureHandle;
    AddOp(TextureOpQueue, &Op);
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
ClearSpriteSheetSprites(platform_texture_op_queue *TextureOpQueue, spritesheet_mode *SpriteSheetMode)
{
    for(u32 SpriteIndex = 0;
        SpriteIndex < ArrayCount(SpriteSheetMode->Sprites);
        ++SpriteIndex)
    {
        loaded_bitmap *Sprite = SpriteSheetMode->Sprites + SpriteIndex;
        DeallocateBitmap(TextureOpQueue, Sprite);
    }
                                
    ZeroArray(ArrayCount(SpriteSheetMode->Sprites), SpriteSheetMode->Sprites);
}

internal void
ClearStoredAssetData(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue,
                     kesa_type Type, audio_state *AudioState = 0)
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
            DeallocateBitmap(TextureOpQueue, &BitmapMode->Bitmap);
        } break;

        case KESA_SpriteSheet:
        {
            // NOTE(paul): Clear SpriteSheet Mode data
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;

            DeallocateBitmap(TextureOpQueue, &SpriteSheetMode->SpriteSheetBitmap);
            ClearSpriteSheetSprites(TextureOpQueue, SpriteSheetMode);
        } break;

        case KESA_Tileset:
        {
            ClearTilesetView(TextureOpQueue, &AssetsMode->TilesetMode);
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
            sizeof(sound_mode), sizeof(text_mode), sizeof(binary_file_mode),
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
ClearEditModeData(editor_state *EditorState, editor_mode_assets *AssetsMode,
                  platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    kesa_type Type = Asset->Type;

    AssetsMode->AssetsToAdd[AssetsMode->AddAssetCount] = {};
    RemoveAction(AssetsMode, AM_AddAsset); 
    RemoveAction(AssetsMode, AM_RemoveAssetTag); 
    RemoveAction(AssetsMode, AM_AddAssetTag); 

    AssetsMode->LastTagIndex = 0;
    AssetsMode->CurrentTagIndex = 0;
    AssetsMode->CurrentTagValue = 0;
    AssetsMode->CurrentStoredTagIndex = 0;

    AssetsMode->FileIndex = 0;
    AssetsMode->LastFileIndex = 0;
    AssetsMode->SubFileIndex = 0;
    AssetsMode->LastSubFileIndex = 0;
    AssetsMode->PixelPosition = {};
    AssetsMode->Time = 0.0f;

    ClearStoredAssetData(AssetsMode, TextureOpQueue, Type, &EditorState->AudioState);
}

inline void
LoadStoredAssetData(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *StoredAsset,
                    b32 RememberEditMode = false)
{
    TIMED_FUNCTION();

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
    
            BitmapMode->Bitmap = LoadImageFile(StoredAsset->SourceFileName, PlatformFileType_BMP, 0);
            if(BitmapMode->Bitmap.Memory)
            {
                loaded_bitmap *Bitmap = &BitmapMode->Bitmap;
                AllocateBitmap(TextureOpQueue, Bitmap);
            }
        } break;

        case KESA_SpriteSheet:
        {
            spritesheet_mode *SpriteSheetMode = &AssetsMode->SpriteSheetMode;
            kesa_spritesheet *SpriteSheet = &StoredAsset->SpriteSheet;

            SpriteSheetMode->SpriteSheetBitmap = LoadImageFile(StoredAsset->SourceFileName, PlatformFileType_SSBMP, 0);

            loaded_bitmap *Bitmap = &SpriteSheetMode->SpriteSheetBitmap;
            AllocateBitmap(TextureOpQueue, Bitmap);
            CutSpriteSheet(TextureOpQueue, SpriteSheetMode, SpriteSheet);
        } break;

        case KESA_Tileset:
        {
            LoadTilesetView(TextureOpQueue, &AssetsMode->TilesetMode, StoredAsset);
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
LoadNewStoredAsset(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    if(AssetsMode->SourceFileCounts[Asset->Type])
        StringCopy(AssetsMode->SourceFiles[Asset->Type][AssetsMode->FileIndex], Asset->SourceFileName);

    switch(Asset->Type)
    {
        case KESA_None:
            break;

        case KESA_Bitmap:
        {
            LoadStoredAssetData(AssetsMode, TextureOpQueue, Asset);
            Asset->Bitmap.AlignPercentage = AssetsMode->BitmapMode.Bitmap.AlignPercentage;
        } break;

        case KESA_SpriteSheet:
        {
            LoadStoredAssetData(AssetsMode, TextureOpQueue, Asset);
            Asset->SpriteSheet.SpriteHeight = AssetsMode->SpriteSheetMode.SpriteSheetBitmap.Height;
        } break;

        case KESA_Tileset:
        {
            LoadStoredAssetData(AssetsMode, TextureOpQueue, Asset);
        } break;

        case KESA_Text:
        {
            LoadStoredAssetData(AssetsMode, TextureOpQueue, Asset);
            text_mode *TextMode = &AssetsMode->TextMode;
            
            TextMode->EditBufferLength = StringLength(TextMode->Text.String);
            Copy(TextMode->EditBufferLength, TextMode->Text.String, TextMode->EditBuffer);
            TextMode->EditBufferMaxLength = 4096;
        } break;

        case KESA_Sound:
        case KESA_File:
        case KESA_SSWM:
        {
            LoadStoredAssetData(AssetsMode, TextureOpQueue, Asset);
        } break;

        InvalidDefaultCase;
    }
}

inline void
InitAssetsMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    Asset->Type = KESAFromEditMode(AssetsMode->EditMode);
    LoadNewStoredAsset(AssetsMode, TextureOpQueue, Asset);
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
UpdateAndRenderBitmapEditMode(editor_mode_assets *AssetsMode, engine_input *Input, platform_texture_op_queue *TextureOpQueue,
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
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Bitmap);
        LoadNewStoredAsset(AssetsMode, TextureOpQueue, Asset);
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
UpdateAndRenderSpriteSheetEditMode(editor_mode_assets *AssetsMode, engine_input *Input, platform_texture_op_queue *TextureOpQueue,
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
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_SpriteSheet);
        SpriteSheet->SpriteCount = 0;

        LoadNewStoredAsset(AssetsMode, TextureOpQueue, CurrentAsset);
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
                ClearSpriteSheetSprites(TextureOpQueue, SpriteSheetMode);

                SpriteSheet->SpriteCount = (SpriteSheetBitmap->Width / SpriteSheet->SpriteWidth);
                CutSpriteSheet(TextureOpQueue, SpriteSheetMode, SpriteSheet);
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

// NOTE(pvlso): The set in its own grid, fitted to the canvas, the tile under the mouse outlined (its details are in the
// side panel), animated tiles outlined in blue
internal void
UpdateAndRenderTilesetEditMode(editor_mode_assets *AssetsMode, engine_input *Input, platform_texture_op_queue *TextureOpQueue,
                                   render_group *RenderGroup, object_transform *Flat, v2 MouseP,
                                   rectangle2 CanvasRect, r32 TileDim, kesa_asset *Asset)
{
    v2 CanvasDim = GetDim(CanvasRect);
    r32 CanvasSize = CanvasDim.x;

    DrawCanvasOutline(RenderGroup, Flat, CanvasDim, V2(0, 0));

    tileset_mode *TilesetMode = &AssetsMode->TilesetMode;
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Tileset);
        LoadNewStoredAsset(AssetsMode, TextureOpQueue, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }

    PushRect(RenderGroup, Flat, V3(0, 0, 5.0f), V2(CanvasSize, CanvasSize), V4(0.25f, 0.25f, 0.25f, 1.0f));

    tileset_file *File = &TilesetMode->File;
    TilesetMode->HoverTile = -1;
    if(TilesetMode->Loaded && File->GridWidth && File->GridHeight)
    {
        u32 Cells = Maximum(File->GridWidth, File->GridHeight);
        r32 Cell = CanvasSize / (r32)Cells;
        v2 TopLeft = V2(-0.5f*Cell*(r32)File->GridWidth, 0.5f*Cell*(r32)File->GridHeight);
        for(u32 TileIndex = 0; TileIndex < File->TileCount; ++TileIndex)
        {
            tileset_file_tile *Tile = File->Tiles + TileIndex;
            v2 Center = TopLeft + V2(((r32)Tile->GridX + 0.5f)*Cell, -((r32)Tile->GridY + 0.5f)*Cell);
            loaded_bitmap *Bitmap = TilesetMode->Tiles + TileIndex;
            if(Bitmap->Memory)
            {
                PushBitmap(RenderGroup, Flat, Bitmap, Cell, V3(Center, 7.0f));
            }
            rectangle2 Rect = RectCenterDim(Center, V2(Cell, Cell));
            if(IsInRectangle(Rect, MouseP))
            {
                TilesetMode->HoverTile = (s32)TileIndex;
                PushRectOutline(RenderGroup, Flat, Rect, 8.0f, V4(1, 1, 0, 1), 2.0f);
            }
            else if(Tile->FrameCount > 1)
            {
                PushRectOutline(RenderGroup, Flat, Rect, 8.0f, V4(0, 0.8f, 1, 1), 1.0f);
            }
        }
    }
}

internal void
UpdateAndRenderSoundEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue,
                             audio_state *AudioState, kesa_asset *Asset)
{
    sound_mode *SoundMode = &AssetsMode->SoundMode;
    kesa_sound *StoredSound = &Asset->Sound;
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Sound, AudioState);
        LoadNewStoredAsset(AssetsMode, TextureOpQueue, Asset);
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
UpdateAndRenderTextEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    text_mode *TextMode = &AssetsMode->TextMode;
    kesa_text *StoredText = &Asset->Text;
                
    if((AssetsMode->FileIndex != AssetsMode->LastFileIndex) || TextMode->Reload)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_Text);
        LoadNewStoredAsset(AssetsMode, TextureOpQueue, Asset);

        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
        TextMode->Reload = false;
    }

    if(TextMode->EditTextFile && AssetsMode->SourceFileCounts[KESA_Text])
    {
#if EDITOR_INTERNAL
        char *FileName = AssetsMode->SourceFiles[KESA_Text][AssetsMode->FileIndex];
        char CommandLine[512];
        FormatString(ArrayCount(CommandLine), CommandLine,
                     "openwithnotepad.bat txts/%s", FileName);
        Platform.DEBUGExecuteSystemCommand(0, 0, CommandLine);
#endif

        TextMode->EditTextFile = false;
    }
}

internal void
UpdateAndRenderFileEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    kesa_binary_file *StoredFile = &Asset->File;
    binary_file_mode *FileMode = &AssetsMode->BinaryFileMode;
    
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_File);
        LoadNewStoredAsset(AssetsMode, TextureOpQueue, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
UpdateAndRenderSSWMEditMode(editor_mode_assets *AssetsMode, platform_texture_op_queue *TextureOpQueue, kesa_asset *Asset)
{
    kesa_sswm_file *StoredFile = &Asset->SSWM;
    sswm_mode *SSWMMode = &AssetsMode->SSWMMode;
    
    if(AssetsMode->FileIndex != AssetsMode->LastFileIndex)
    {
        ClearStoredAssetData(AssetsMode, TextureOpQueue, KESA_SSWM);
        LoadNewStoredAsset(AssetsMode, TextureOpQueue, Asset);
        AssetsMode->LastFileIndex = AssetsMode->FileIndex;
    }
}

internal void
RemoveStoredAsset(editor_mode_assets *AssetsMode)
{
    if(AssetsMode->ShowStoredAssetIndex)
    {
        kesa_asset *AssetToRemove = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
        LOG("Removed stored asset %s (not written until Write Assets)", AssetToRemove->SourceFileName);
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
UpdateAndRenderAssetsMode(editor_state *EditorState, transient_state *TranState, engine_input *Input,
                          platform_texture_op_queue *TextureOpQueue)
{
    TIMED_FUNCTION();

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
                Assert(ReadStoredAssets(EditorState, AssetsMode));

            if(!ReadTags(AssetsMode))
                Assert(ReadTags(AssetsMode));
            AssetsMode->AssetsInitialized = true;
        }

        if(!IsAction(AssetsMode, AM_Exit))
        {
            if(AssetsMode->LastEditMode != AssetsMode->EditMode)
            {
                if(AssetsMode->CurrentAsset)
                    ClearEditModeData(EditorState, AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                RemoveAction(AssetsMode, AM_EditStoredAsset);

                if(AssetsMode->LastEditMode == EditMode_None)
                {
                    AssetsMode->CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
                    if(AssetsMode->CurrentAsset->Type == KESA_None)
                    {
                        InitAssetsMode(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                    }
                }
                else
                {
                    AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
                    LoadStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                }

                AssetsMode->LastEditMode = AssetsMode->EditMode;            
            }

            if(CheckRemoveAction(AssetsMode, AM_RemoveAssetTag))
            {
                RemoveCurrentAssetTag(AssetsMode, AssetsMode->CurrentAsset);
                LOG("Removed a tag from %s", AssetsMode->CurrentAsset->SourceFileName);
            }
        
            if(CheckRemoveAction(AssetsMode, AM_AddAssetTag))
            {
                AddTagToCurrentAsset(AssetsMode, AssetsMode->CurrentAsset);
                LOG("Added a tag to %s", AssetsMode->CurrentAsset->SourceFileName);
            }

            if(CheckRemoveAction(AssetsMode, AM_AddNewTag))
                UpdateEditorTags(AssetsMode, UpdateTagsOp_AddNew);
            else if(CheckRemoveAction(AssetsMode, AM_AddEditedTag))
                UpdateEditorTags(AssetsMode, UpdateTagsOp_Edit);
            else if(CheckRemoveAction(AssetsMode, AM_RemoveEditedTag))
                UpdateEditorTags(AssetsMode, UpdateTagsOp_Remove);

            if(IsAction(AssetsMode, AM_AddAsset) &&
               !IsAction(AssetsMode, AM_EditStoredAsset))
            {
                AddCurrentAsset(AssetsMode);
                AssetsMode->CurrentAsset = AssetsMode->AssetsToAdd + AssetsMode->AddAssetCount;
                AssetsMode->CurrentAsset->Type = KESAFromEditMode(AssetsMode->EditMode);

                RemoveAction(AssetsMode, AM_AddAsset);
//                WriteStoredAssets(EditorState, AssetsMode);
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
                    if(IsAction(AssetsMode, AM_EditStoredAsset))
                    {
                        AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;

                        ClearStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset->Type,
                                             &EditorState->AudioState);
                        LoadStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset, true);

                        AssetsMode->LastEditMode = AssetsMode->EditMode;            
                        AssetsMode->StoredAssetChanged = true;
                        AssetsMode->LastShowStoredAssetIndex = AssetsMode->ShowStoredAssetIndex;
                    }
                    else if(AssetsMode->LastShowStoredAssetIndex != AssetsMode->ShowStoredAssetIndex)
                    {
                        kesa_asset *PreviousAsset = AssetsMode->StoredAssets + AssetsMode->LastShowStoredAssetIndex;
                        AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;

                        ClearStoredAssetData(AssetsMode, TextureOpQueue, PreviousAsset->Type,
                                             &EditorState->AudioState);
                        LoadStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);

                        AssetsMode->LastShowStoredAssetIndex = AssetsMode->ShowStoredAssetIndex;
                    }
                    
                    if(CheckRemoveAction(AssetsMode, AM_WriteAssets))
                    {
                        WriteStoredAssets(EditorState, AssetsMode);
                        AssetsMode->StoredAssetChanged = false;
                    }

                    if(CheckRemoveAction(AssetsMode, AM_RemoveStoredAsset))
                    {
                        ClearStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset->Type,
                                             &EditorState->AudioState);
                        RemoveStoredAsset(AssetsMode);

                        AssetsMode->CurrentAsset = AssetsMode->StoredAssets + AssetsMode->ShowStoredAssetIndex;
                        LoadStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);

                        AssetsMode->StoredAssetChanged = true;
                    }

                    if(CheckRemoveAction(AssetsMode, AM_WriteKEA))
                    {
                        temporary_memory TempMemory = BeginTemporaryMemory(&TranState->TranArena);
                        BuildKEA(AssetsMode, TempMemory.Arena);
                        EndTemporaryMemory(TempMemory);
                    }

                } break;

                case EditMode_Bitmap:
                {
                    UpdateAndRenderBitmapEditMode(AssetsMode, Input, TextureOpQueue, RenderGroup, &Flat, MouseP,
                                                  CanvasRect, TileDim, AssetsMode->CurrentAsset);
                } break;

                case EditMode_SpriteSheet:
                {
                    UpdateAndRenderSpriteSheetEditMode(AssetsMode, Input, TextureOpQueue, RenderGroup, &Flat, MouseP,
                                                       CanvasRect, TileDim, AssetsMode->CurrentAsset);
                } break;

                case EditMode_Tileset:
                {
                    UpdateAndRenderTilesetEditMode(AssetsMode, Input, TextureOpQueue, RenderGroup, &Flat, MouseP,
                                                   CanvasRect, TileDim, AssetsMode->CurrentAsset);
                } break;

                case EditMode_Sound:
                {
                    UpdateAndRenderSoundEditMode(AssetsMode, TextureOpQueue, &EditorState->AudioState,
                                                 AssetsMode->CurrentAsset);
                } break;

                case EditMode_Text:
                {
                    UpdateAndRenderTextEditMode(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                } break;

                case EditMode_File:
                {
                    UpdateAndRenderFileEditMode(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                } break;

                case EditMode_SSWM:
                {
                    UpdateAndRenderSSWMEditMode(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset);
                } break;
            
                InvalidDefaultCase;
            }

            DrawAssetsModeUI(AssetsMode, UIState);

            AssetsMode->Time += Input->dtForFrame;
        }
        else
        {
            ClearStoredAssetData(AssetsMode, TextureOpQueue, AssetsMode->CurrentAsset->Type,
                                 &EditorState->AudioState);
            Platform.DeallocateMemory(AssetsMode->StoredAssets);
            DeallocateAssetsModeTags(AssetsMode);
            
            PlayTitleScreen(EditorState, TranState);
        }
    }

    return(Result);
}

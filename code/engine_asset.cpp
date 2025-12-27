/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */

enum finalize_asset_operation
{
    FinalizeAsset_None,
    FinalizeAsset_Bitmap,
};

struct load_asset_work
{
    task_with_memory *Task;
    asset *Asset;    

    platform_file_handle *Handle;
    u64 Offset;
    u64 Size;
    void *Destination;

    finalize_asset_operation FinalizeOperation;
    u32 FinalState;

    platform_texture_op_queue *TextureOpQueue;
};

internal void
AddOp(platform_texture_op_queue *Queue, texture_op *Source)
{
    BeginTicketMutex(&Queue->Mutex);
    Assert(Queue->FirstFree);
    texture_op *Dest = Queue->FirstFree;
    Queue->FirstFree = Dest->Next;

    *Dest = *Source;
    Assert(Dest->Next == 0);

    if(Queue->Last)
    {
        Queue->Last = Queue->Last->Next = Dest;
    }
    else
    {
        Queue->First = Queue->Last = Dest;
    }
    
    EndTicketMutex(&Queue->Mutex);
}

internal void
LoadAssetWorkDirectly(load_asset_work *Work)
{
    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);
    if(PlatformNoFileErrors(Work->Handle))
    {
        switch(Work->FinalizeOperation)
        {
            case FinalizeAsset_None:
            {
                // NOTE(casey): Nothing to do.
            } break;

            case FinalizeAsset_Bitmap:
            {
                loaded_bitmap *Bitmap = &Work->Asset->Header->Bitmap;
                texture_op Op = {};
                Op.IsAllocate = true;
                Op.Allocate.Width = Bitmap->Width;
                Op.Allocate.Height = Bitmap->Height;
                Op.Allocate.Data = Bitmap->Memory;
                Op.Allocate.ResultHandle = &Bitmap->TextureHandle;
                AddOp(Work->TextureOpQueue, &Op);
            } break;

            InvalidDefaultCase;
        }
    }

    CompletePreviousWritesBeforeFutureWrites;

    if(!PlatformNoFileErrors(Work->Handle))
    {
        ZeroSize(Work->Size, Work->Destination);
    }

    Work->Asset->State = Work->FinalState;
}

internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
    load_asset_work *Work = (load_asset_work *)Data;

    LoadAssetWorkDirectly(Work);

    EndTaskWithMemory(Work->Task);
}

inline asset_file *
GetFile(engine_assets *Assets, u32 FileIndex)
{
    Assert(FileIndex < Assets->FileCount);
    asset_file *Result = Assets->Files + FileIndex;

    return(Result);
}

inline platform_file_handle *
GetFileHandleFor(engine_assets *Assets, u32 FileIndex)
{
    platform_file_handle *Result = &GetFile(Assets, FileIndex)->Handle;

    return(Result);
}

internal asset_memory_block *
InsertBlock(asset_memory_block *Prev, u64 Size, void *Memory)
{
    Assert(Size > sizeof(asset_memory_block));
    asset_memory_block *Block = (asset_memory_block *)Memory;
    Block->Flags = 0;
    Block->Size = Size - sizeof(asset_memory_block);
    Block->Prev = Prev;
    Block->Next = Prev->Next;
    Block->Prev->Next = Block;
    Block->Next->Prev = Block;
    return(Block);
}

internal asset_memory_block *
FindBlockForSize(engine_assets *Assets, umm Size)
{
    asset_memory_block *Result = 0;

    for(asset_memory_block *Block = Assets->MemorySentinel.Next;
        Block != &Assets->MemorySentinel;
        Block = Block->Next)
    {
        if(!(Block->Flags & AssetMemory_Used))
        {
            if(Block->Size >= Size)
            {
                Result = Block;
                break;
            }
        }
    }

    return(Result);
}

internal b32
MergeIfPossible(engine_assets *Assets, asset_memory_block *First, asset_memory_block *Second)
{
    b32 Result = false;

    if((First != &Assets->MemorySentinel) &&
       (Second != &Assets->MemorySentinel))
    {
        if(!(First->Flags & AssetMemory_Used) &&
           !(Second->Flags & AssetMemory_Used))
        {
            u8 *ExpectedSecond = (u8 *)First + sizeof(asset_memory_block) + First->Size;
            if((u8 *)Second == ExpectedSecond)
            {
                Second->Next->Prev = Second->Prev;
                Second->Prev->Next = Second->Next;

                First->Size += sizeof(asset_memory_block) + Second->Size;

                Result = true;
            }
        }
    }

    return(Result);
}

internal b32
GenerationHasCompleted(engine_assets *Assets, u32 CheckID)
{
    b32 Result = true;

    for(u32 Index = 0;
        Index < Assets->InFlightGenerationCount;
        ++Index)
    {
        if(Assets->InFlightGenerations[Index] == CheckID)
        {
            Result = false;
            break;
        }
    }

    return(Result);
}

internal asset_memory_header *
AcquireAssetMemory(engine_assets *Assets, u32 Size, u32 AssetIndex, asset_header_type AssetType)
{
    asset_memory_header *Result = 0;

    BeginAssetLock(Assets);

    asset_memory_block *Block = FindBlockForSize(Assets, Size);
    for(;;)
    {
        if(Block && (Size <= Block->Size))
        {
            Block->Flags |= AssetMemory_Used;

            Result = (asset_memory_header *)(Block + 1);

            umm RemainingSize = Block->Size - Size;
            umm BlockSplitThreshold = 4096;
            if(RemainingSize > BlockSplitThreshold)
            {
                Block->Size -= RemainingSize;
                InsertBlock(Block, RemainingSize, (u8 *)Result + Size);
            }

            break;
        }
        else
        {
            for(asset_memory_header *Header = Assets->LoadedAssetSentinel.Prev;
                Header != &Assets->LoadedAssetSentinel;
                Header = Header->Prev)
            {
                asset *Asset = Assets->Assets + Header->AssetIndex;
                if((Asset->State >= AssetState_Loaded) &&
                   (GenerationHasCompleted(Assets, Asset->Header->GenerationID)))
                {
                    u32 AssetIndex_ = Header->AssetIndex;
                    asset *Asset_ = Assets->Assets + AssetIndex_;

                    Assert(Asset_->State == AssetState_Loaded);

                    RemoveAssetHeaderFromList(Header);
                    
                    if(Asset->Header->AssetType == AssetType_Bitmap)
                    {
                        texture_op Op = {};
                        Op.IsAllocate = false;
                        Op.Deallocate.Handle = Asset->Header->Bitmap.TextureHandle;
                        AddOp(Assets->TextureOpQueue, &Op);
                    }

                    Block = (asset_memory_block *)Asset->Header - 1;
                    Block->Flags &= ~AssetMemory_Used;

                    if(MergeIfPossible(Assets, Block->Prev, Block))
                    {
                        Block = Block->Prev;
                    }

                    MergeIfPossible(Assets, Block, Block->Next);

                    Asset->State = AssetState_Unloaded;
                    Asset->Header = 0;    
                    break;
                }
            }
        }
    }

    if(Result)
    {
        Result->AssetType = AssetType;
        Result->AssetIndex = AssetIndex;
        Result->TotalSize = Size;
        InsertAssetHeaderAtFront(Assets, Result);
    }

    EndAssetLock(Assets);

    return(Result);
}

struct asset_memory_size
{
    u32 Total;
    u32 Data;
    u32 Section;
};

struct preloaded_asset
{
    asset_memory_size Size;

    void *LoadDest;
    u32 FinalizeOp;
    u32 FinalState;
    b32 TextureOpNeeded;
};

inline asset_memory_size
GetAssetMemorySize(asset *Asset, asset_header_type HType)
{
    asset_memory_size Size = {};
    switch(HType)
    {
        case AssetType_None: {} break;

        case AssetType_Bitmap:
        {
            kea_bitmap *Info = &Asset->KEA.Bitmap;
            u32 Width = Info->Dim[0];
            u32 Height = Info->Dim[1];
            Size.Section = 4*Width;
            Size.Data = Height*Size.Section;
            Size.Total = Size.Data + sizeof(asset_memory_header);
        } break;

        case AssetType_Sound:
        {
            kea_sound *Info = &Asset->KEA.Sound;
            Size.Section = Info->SampleCount*sizeof(int16);
            Size.Data = Info->ChannelCount*Size.Section;
            Size.Total = Size.Data + sizeof(asset_memory_header);
        } break;

        case AssetType_Tileset:
        {
            kea_tileset *Info = &Asset->KEA.Tileset;
            u32 TilesSize = sizeof(u64)*Info->TileCount;
            Size.Data = TilesSize;
            Size.Total = Size.Data + sizeof(asset_memory_header);
        } break;

        case AssetType_SpriteSheet:
        {
            kea_spritesheet *Info = &Asset->KEA.SpriteSheet;
            u32 TilesSize = sizeof(bitmap_id)*Info->SpriteCount;
            Size.Data = TilesSize;
            Size.Total = Size.Data + sizeof(asset_memory_header);
        } break;

        case AssetType_Text:
        {
            kea_text *Info = &Asset->KEA.Text;
            u32 StringSize = Info->Length;
            Size.Data = StringSize;
            Size.Total = Size.Data + sizeof(asset_memory_header);
        } break;

        case AssetType_BinaryFile:
        {
            kea_binary_file *Info = &Asset->KEA.BinaryFile;
            Size.Data = Info->Size;
            Size.Total = Size.Data + sizeof(asset_memory_header);
        } break;

        InvalidDefaultCase;
    }

    return(Size);
}

inline preloaded_asset
PrepareAssetForLoading(engine_assets *Assets, asset *Asset,
                       asset_header_type HType)
{
    preloaded_asset Result = {};
    Result.Size = GetAssetMemorySize(Asset, HType);
    Asset->Header = AcquireAssetMemory(Assets, Result.Size.Total, Asset->KEA.AssetIndex, HType);

    switch(HType)
    {
        case AssetType_None: {} break;

        case AssetType_Bitmap:
        {
            kea_bitmap *Info = &Asset->KEA.Bitmap;

            loaded_bitmap *Bitmap = &Asset->Header->Bitmap;            
            Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
            Bitmap->WidthOverHeight = (r32)Info->Dim[0] / (r32)Info->Dim[1];
            Bitmap->Width = Info->Dim[0];
            Bitmap->Height = Info->Dim[1];
            Bitmap->Pitch = Result.Size.Section;
            Bitmap->TextureHandle = 0;
            Bitmap->Memory = (Asset->Header + 1);

            Result.LoadDest = Bitmap->Memory;
            Result.FinalizeOp = FinalizeAsset_Bitmap;
            Result.FinalState = AssetState_Loaded;
            Result.TextureOpNeeded = true;
        } break;

        case AssetType_Sound:
        {
            kea_sound *Info = &Asset->KEA.Sound;

            loaded_sound *Sound = &Asset->Header->Sound;
            Sound->SampleCount = Info->SampleCount;
            Sound->ChannelCount = Info->ChannelCount;

            u32 ChannelSize = Result.Size.Section;
            void *Memory = (Asset->Header + 1);
            int16 *SoundAt = (int16 *)Memory;
            for(u32 ChannelIndex = 0;
                ChannelIndex < Sound->ChannelCount;
                ++ChannelIndex)
            {
                Sound->Samples[ChannelIndex] = SoundAt;
                SoundAt += ChannelSize;
            }


            Result.LoadDest = Memory;
            Result.FinalizeOp = FinalizeAsset_None;
            Result.FinalState = AssetState_Loaded;
        } break;

        case AssetType_Tileset:
        {
            loaded_tileset *Tileset = &Asset->Header->Tileset;
            Tileset->TileGUIDs = (bitmap_id *)(Asset->Header + 1);
                
            Result.LoadDest = Tileset->TileGUIDs;
            Result.FinalizeOp = FinalizeAsset_None;
            Result.FinalState = AssetState_Loaded;
        } break;

        case AssetType_SpriteSheet:
        {
            loaded_spritesheet *SpriteSheet = &Asset->Header->SpriteSheet;
            SpriteSheet->SpriteGUIDs = (bitmap_id *)(Asset->Header + 1);
                
            Result.LoadDest = SpriteSheet->SpriteGUIDs;
            Result.FinalizeOp = FinalizeAsset_None;
            Result.FinalState = AssetState_Loaded;
        } break;

        case AssetType_Text:
        {
            loaded_text *Text = &Asset->Header->Text;
            Text->String = (char *)(Asset->Header + 1);

            Result.LoadDest = Text->String;
            Result.FinalizeOp = FinalizeAsset_None;
            Result.FinalState = AssetState_Loaded;
        } break;

        case AssetType_BinaryFile:
        {
            kea_binary_file *Info = &Asset->KEA.BinaryFile;
            loaded_file *BinaryFile = &Asset->Header->BinaryFile;            
            BinaryFile->Size = Info->Size;
            BinaryFile->Data = (Asset->Header + 1);

            Result.LoadDest = BinaryFile->Data;
            Result.FinalizeOp = FinalizeAsset_None;
            Result.FinalState = AssetState_Loaded;
        } break;

        InvalidDefaultCase;
    }

    return(Result);
}

internal void
LoadAsset(engine_assets *Assets, asset_header_type HType, u64 GUID, b32 Immediate)
{
    
    asset *Asset = GetAssetByGUID(Assets, GUID);        
    if(GUID)
    {
        if(AtomicCompareExchangeUInt32((uint32 *)&Asset->State, AssetState_Queued, AssetState_Unloaded) ==
           AssetState_Unloaded)
        {
            task_with_memory *Task = 0;

            if(!Immediate)
            {
                Task = BeginTaskWithMemory(Assets->TranState, false);
            }

            if(Immediate || Task)        
            {
                preloaded_asset PreAsset =
                    PrepareAssetForLoading(Assets, Asset, HType);

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + Asset->KEA.AssetIndex;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->KEA.DataOffset;
                Work.Size = PreAsset.Size.Data;

                Work.Destination = PreAsset.LoadDest;
                Work.FinalizeOperation = (finalize_asset_operation)PreAsset.FinalizeOp;
                Work.FinalState = PreAsset.FinalState;            
                Work.TextureOpQueue =
                    (PreAsset.TextureOpNeeded ? Assets->TextureOpQueue : 0);

                if(Task)
                {
                    load_asset_work *TaskWork = PushStruct(&Task->Arena, load_asset_work, NoClear());
                    *TaskWork = Work;
                    Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, TaskWork);
                }
                else
                {
                    LoadAssetWorkDirectly(&Work);
                }
            }
            else
            {
                Asset->State = AssetState_Unloaded;
            }
        }
        else if(Immediate)
        {
            asset_state volatile *State = (asset_state volatile *)&Asset->State;
            while(*State == AssetState_Queued) {}
        }
    }    
}

#if 0
internal uint32
GetBestMatchAssetFrom(engine_assets *Assets, asset_type_id TypeID,
                      asset_vector *MatchVector, asset_vector *WeightVector)
{
//    TIMED_FUNCTION();

    uint32 Result = 0;

    real32 BestDiff = Real32Maximum;
    asset_type *Type = Assets->AssetTypes + TypeID;
    for(uint32 AssetIndex = Type->FirstAssetIndex;
        AssetIndex < Type->OnePastLastAssetIndex;
        ++AssetIndex)
    {
        asset *Asset = Assets->Assets + AssetIndex;

        real32 TotalWeightedDiff = 0.0f;
        for(uint32 TagIndex = Asset->KEA.FirstTagIndex;
            TagIndex < Asset->KEA.OnePastLastTagIndex;
            ++TagIndex)
        {
            kea_tag *Tag = Assets->Tags + TagIndex;

            s32 A = MatchVector->E[Tag->ID];
            real32 B = (r32)Tag->Value;
            real32 D0 = AbsoluteValue(A - B);
            real32 D1 = AbsoluteValue((A - Assets->TagRange[Tag->ID]*SignOf(A)) - B);
            real32 Difference = Minimum(D0, D1);

            real32 Weighted = WeightVector->E[Tag->ID]*Difference;
            TotalWeightedDiff += Weighted;
        }

        if(BestDiff > TotalWeightedDiff)
        {
            BestDiff = TotalWeightedDiff;
            Result = AssetIndex;
        }
    }

    return(Result);
}

internal bitmap_id
GetTileBitmapByChecksumTag(engine_assets *Assets, u32 Checksum)
{
    bitmap_id Result = {};

    b32 Found = false;
    asset_type *Type = Assets->AssetTypes + Asset_Tile;
    for(uint32 AssetIndex = Type->FirstAssetIndex;
        AssetIndex < Type->OnePastLastAssetIndex && !Found;
        ++AssetIndex)
    {
        asset *Asset = Assets->Assets + AssetIndex;
        for(uint32 TagIndex = Asset->KEA.FirstTagIndex;
            TagIndex < Asset->KEA.OnePastLastTagIndex;
            ++TagIndex)
        {
            kea_tag *Tag = Assets->Tags + TagIndex;
            if((Tag->ID == Tag_TileChecksum) && (Tag->Value == Checksum))
            {
                Result = {AssetIndex};
                Found = true;
                break;
            }
        }
    }

    return(Result);
}

internal uint32
GetRandomAssetFrom(engine_assets *Assets, asset_type_id TypeID, random_series *Series)
{
//    TIMED_FUNCTION();

    uint32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
        uint32 Count = (Type->OnePastLastAssetIndex - Type->FirstAssetIndex);
        uint32 Choice = RandomChoice(Series, Count);
        Result = Type->FirstAssetIndex + Choice;
    }

    return(Result);
}

internal uint32
GetFirstAssetFrom(engine_assets *Assets, asset_type_id TypeID)
{
//    TIMED_FUNCTION();

    uint32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
        Result = Type->FirstAssetIndex;
    }

    return(Result);
}

inline sswm_id
GetBestMatchSSWMFrom(engine_assets *Assets, asset_type_id TypeID,
                     asset_vector *MatchVector, asset_vector *WeightVector)
{
    sswm_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline bitmap_id
GetBestMatchBitmapFrom(engine_assets *Assets, asset_type_id TypeID,
                       asset_vector *MatchVector, asset_vector *WeightVector)
{
    bitmap_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline bitmap_id
GetFirstBitmapFrom(engine_assets *Assets, asset_type_id TypeID)
{
    bitmap_id Result = {GetFirstAssetFrom(Assets, TypeID)};
    return(Result);
}

inline bitmap_id
GetRandomBitmapFrom(engine_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    bitmap_id Result = {GetRandomAssetFrom(Assets, TypeID, Series)};
    return(Result);
}

inline sound_id
GetBestMatchSoundFrom(engine_assets *Assets, asset_type_id TypeID,
                       asset_vector *MatchVector, asset_vector *WeightVector)
{
    sound_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline sound_id
GetFirstSoundFrom(engine_assets *Assets, asset_type_id TypeID)
{
    sound_id Result = {GetFirstAssetFrom(Assets, TypeID)};
    return(Result);
}

inline sound_id
GetRandomSoundFrom(engine_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    sound_id Result = {GetRandomAssetFrom(Assets, TypeID, Series)};
    return(Result);
}

inline font_id
GetBestMatchFontFrom(engine_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    font_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline tileset_id
GetBestMatchTilesetFrom(engine_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    tileset_id Result = {};
    Result.Value = GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector);

    return(Result);
}

inline spritesheet_id
GetBestMatchSpriteSheetFrom(engine_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    spritesheet_id Result = {};
    Result.Value = GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector);

    return(Result);
}

inline spritesheet_id
GetFirstSpriteSheetFrom(engine_assets *Assets, asset_type_id TypeID)
{
    spritesheet_id Result = {};
    Result.Value = GetFirstAssetFrom(Assets, TypeID);
    
    return(Result);
}


inline text_id
GetBestMatchTextFrom(engine_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    text_id Result = {};
    Result.Value = GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector);

    return(Result);
}

inline file_id
GetBestMatchFileFrom(engine_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    file_id Result = {};
    Result.Value = GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector);

    return(Result);
}
#endif

internal engine_assets *
AllocateAssets(memory_arena *Arena, umm Size, transient_state *TranState,
               platform_texture_op_queue *TextureOpQueue)
{
//    TIMED_FUNCTION();

    engine_assets *Assets = PushStruct(Arena, engine_assets);
    Assets->TextureOpQueue = TextureOpQueue;

    Assets->NextGenerationID = 0;
    Assets->InFlightGenerationCount = 0;    

    Assets->MemorySentinel.Flags = 0;
    Assets->MemorySentinel.Size = 0;
    Assets->MemorySentinel.Prev = &Assets->MemorySentinel;
    Assets->MemorySentinel.Next = &Assets->MemorySentinel;

    InsertBlock(&Assets->MemorySentinel, Size, PushSize(Arena, Size, NoClear()));

    Assets->TranState = TranState;

    Assets->LoadedAssetSentinel.Next = 
        Assets->LoadedAssetSentinel.Prev =
        &Assets->LoadedAssetSentinel;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;

    {
        platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin(PlatformFileType_KEA);
        Assets->FileCount = FileGroup.FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for(u32 FileIndex = 0;
            FileIndex < 1; // TODO(pvlso): Merge multiple files
            ++FileIndex)
        {
            asset_file *File = Assets->Files + FileIndex;

            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(&FileGroup);
            Platform.ReadDataFromFile(&File->Handle, 0, sizeof(File->Header), &File->Header);
            
            if(File->Header.MagicValue != KEA_MAGIC_VALUE)
            {
                Platform.FileError(&File->Handle, "KEA file has an invalid magic value.");
            }

            u32 TagMapsSize = File->Header.TagCount*sizeof(kea_tag_map);
            Assets->TagMapCount = File->Header.TagCount;
            Assets->TagMaps = (kea_tag_map *)PushSize(Arena, TagMapsSize);
            Platform.ReadDataFromFile(&File->Handle, File->Header.TagMapsOffset,
                                      TagMapsSize, Assets->TagMaps);

            u32 TagsSize = File->Header.UsedTagsCount*sizeof(kea_tag);
            Assets->Tags = (kea_tag *)PushSize(Arena, TagsSize);
            Platform.ReadDataFromFile(&File->Handle, File->Header.UsedTagsArrayOffset,
                                      TagsSize, Assets->Tags);

            u32 AssetTypeTableSize = File->Header.AssetTypeCount*sizeof(kea_asset_type_table_entry);
            Assets->TypeTable = (kea_asset_type_table_entry *)PushSize(Arena, AssetTypeTableSize);
            Platform.ReadDataFromFile(&File->Handle, File->Header.AssetTypeTableOffset,
                                      AssetTypeTableSize, Assets->TypeTable);

            for(u32 Type = 0;
                Type < KEAType_Count;
                ++Type)
            {
                kea_asset_type_table_entry *TypeEntry = Assets->TypeTable + Type;
                u32 TypeDataSize = TypeEntry->TypeCount*sizeof(u32);

                Assets->TypeTableData[Type] = (u32 *)PushSize(Arena, TypeDataSize);
                Platform.ReadDataFromFile(&File->Handle, TypeEntry->AssetsIndeciesOffset,
                                          TypeDataSize, Assets->TypeTableData[Type]);
            }

            if(PlatformNoFileErrors(&File->Handle))
            {
                // NOTE(casey): The first asset and tag slot in every
                // KEA is a null (reserved) so we don't count it as
                // something we will need space for!
                Assets->TagCount += (File->Header.UsedTagsCount - 1);
                Assets->AssetCount += (File->Header.AssetCount - 1);
            }
            else
            {
                // TODO(casey): Eventually, have some way of notifying users of bogus files?
                InvalidCodePath;
            }
        }

        Platform.GetAllFilesOfTypeEnd(&FileGroup);
    }

    // NOTE(casey): Allocate all metadata space
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
    Assets->Tags = PushArray(Arena, Assets->TagCount, kea_tag);

    // NOTE(casey): Reserve one null tag at the beginning
    ZeroStruct(Assets->Tags[0]);

    // NOTE(casey): Reserve one null asset at the beginning
    u32 AssetCount = 0;
    ZeroStruct(*(Assets->Assets + AssetCount));
    ++AssetCount;

    temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
    kea_asset *KEAAssetArray = PushArray(&TranState->TranArena, Assets->AssetCount, kea_asset);

    asset_file *File = Assets->Files + 0;
    Platform.ReadDataFromFile(&File->Handle, File->Header.AssetsOffset,
                              Assets->AssetCount*sizeof(kea_asset),
                              KEAAssetArray);
    for(u32 AssetIndex = 0;
        AssetIndex < Assets->AssetCount;
        ++AssetIndex)
    {
        asset *Asset = Assets->Assets + AssetIndex;
        Asset->KEA = KEAAssetArray[AssetIndex];
    }
    EndTemporaryMemory(TempMem);
    
    return(Assets);
}

#if 0
internal bitmap_id
GetBitmapForTile(engine_assets *Assets, kea_tileset *Info, loaded_tileset *Tileset,
                 u32 TileIndex)
{
    Assert(TileIndex < Info->TileCount);
    bitmap_id Result = Tileset->Tiles[TileIndex].BitmapID;
    Result.Value += Tileset->BitmapIDOffset;
    
    return(Result);
}

inline sound_id
GetSoundEffectForType(engine_assets *Assets, sound_effect_type Type, u32 Variety = 0)
{
    sound_id Result = {};

    asset_vector WeightVector = {};
    WeightVector.E[Tag_Variety] = 1;
    WeightVector.E[Tag_SoundEffectType] = 1;

    asset_vector MatchVector = {};
    MatchVector.E[Tag_Variety] = Variety;
    MatchVector.E[Tag_SoundEffectType] = Type;

    Result = GetBestMatchSoundFrom(Assets, Asset_EffectSound, &MatchVector, &WeightVector); 
    
    return(Result);
}
#endif

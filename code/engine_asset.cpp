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
    FinalizeAsset_Font,
    FinalizeAsset_Bitmap,
    FinalizeAsset_SSWM,
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
//    TIMED_FUNCTION();

    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);
    if(PlatformNoFileErrors(Work->Handle))
    {
        switch(Work->FinalizeOperation)
        {
            case FinalizeAsset_None:
            {
                // NOTE(casey): Nothing to do.
            } break;

            case FinalizeAsset_Font:
            {
                loaded_font *Font = &Work->Asset->Header->Font;
                ssa_font *SSA = &Work->Asset->SSA.Font;
                for(u32 GlyphIndex = 1;
                    GlyphIndex < SSA->GlyphCount;
                    ++GlyphIndex)
                {
                    ssa_font_glyph *Glyph = Font->Glyphs + GlyphIndex;

                    Assert(Glyph->UnicodeCodePoint < SSA->OnePastHighestCodePoint);
                    Assert((u32)(u16)GlyphIndex == GlyphIndex);
                    Font->UnicodeMap[Glyph->UnicodeCodePoint] = (u16)GlyphIndex;
                }
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

            case FinalizeAsset_SSWM:
            {
                loaded_world_map *SSWM = &Work->Asset->Header->SSWM;
                sswm_header *Header = SSWM->Header;
                SSWM->GroundTiles = (sswm_ground_tile *)((u8 *)SSWM->Header + SSWM->Header->Tiles);
                SSWM->Entities = (sswm_entity *)((u8 *)SSWM->GroundTiles + SSWM->Header->Entities);
            } break;
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
GetFile(editor_assets *Assets, u32 FileIndex)
{
    Assert(FileIndex < Assets->FileCount);
    asset_file *Result = Assets->Files + FileIndex;

    return(Result);
}

inline platform_file_handle *
GetFileHandleFor(editor_assets *Assets, u32 FileIndex)
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
FindBlockForSize(editor_assets *Assets, umm Size)
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
MergeIfPossible(editor_assets *Assets, asset_memory_block *First, asset_memory_block *Second)
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
GenerationHasCompleted(editor_assets *Assets, u32 CheckID)
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
AcquireAssetMemory(editor_assets *Assets, u32 Size, u32 AssetIndex, asset_header_type AssetType)
{
//    TIMED_FUNCTION();

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

internal void
LoadBitmap(editor_assets *Assets, bitmap_id ID, b32 Immediate)
{
//    TIMED_FUNCTION();

    asset *Asset = Assets->Assets + ID.Value;        
    if(ID.Value)
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
                ssa_bitmap *Info = &Asset->SSA.Bitmap;

                asset_memory_size Size = {};
                u32 Width = Info->Dim[0];
                u32 Height = Info->Dim[1];
                Size.Section = 4*Width;
                Size.Data = Height*Size.Section;
                Size.Total = Size.Data + sizeof(asset_memory_header);

                Asset->Header = AcquireAssetMemory(Assets, Size.Total, ID.Value, AssetType_Bitmap);

                loaded_bitmap *Bitmap = &Asset->Header->Bitmap;            
                Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
                Bitmap->WidthOverHeight = (r32)Info->Dim[0] / (r32)Info->Dim[1];
                Bitmap->Width = Info->Dim[0];
                Bitmap->Height = Info->Dim[1];
                Bitmap->Pitch = Size.Section;
                Bitmap->TextureHandle = 0;
                Bitmap->Memory = (Asset->Header + 1);

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->SSA.DataOffset;
                Work.Size = Size.Data;
                Work.Destination = Bitmap->Memory;
                Work.FinalizeOperation = FinalizeAsset_Bitmap;
                Work.FinalState = AssetState_Loaded;            
                Work.TextureOpQueue = Assets->TextureOpQueue;
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

internal void
LoadBinaryFile(editor_assets *Assets, file_id ID, b32 Immediate)
{
//    TIMED_FUNCTION();

    asset *Asset = Assets->Assets + ID.Value;        
    if(ID.Value)
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
                ssa_binary_file *Info = &Asset->SSA.BinaryFile;

                asset_memory_size Size = {};
                Size.Data = Info->Size;
                Size.Total = Size.Data + sizeof(asset_memory_header);

                Asset->Header = AcquireAssetMemory(Assets, Size.Total, ID.Value, AssetType_BinaryFile);

                loaded_file *BinaryFile = &Asset->Header->BinaryFile;            
                BinaryFile->Size = Info->Size;
                BinaryFile->Data = (Asset->Header + 1);

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->SSA.DataOffset;
                Work.Size = Size.Data;
                Work.Destination = BinaryFile->Data;
                Work.FinalizeOperation = FinalizeAsset_None;
                Work.FinalState = AssetState_Loaded;            
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

internal void
LoadSSWM(editor_assets *Assets, sswm_id ID, b32 Immediate)
{
//    TIMED_FUNCTION();

    asset *Asset = Assets->Assets + ID.Value;        
    if(ID.Value)
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
                ssa_sswm_file *Info = &Asset->SSA.SSWMFile;

                asset_memory_size Size = {};
                Size.Data = Info->Size;
                Size.Total = Size.Data + sizeof(asset_memory_header);

                Asset->Header = AcquireAssetMemory(Assets, Size.Total, ID.Value, AssetType_SSWM);

                loaded_world_map *SSWM = &Asset->Header->SSWM;            
                SSWM->Header = (sswm_header *)(Asset->Header + 1);

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->SSA.DataOffset;
                Work.Size = Size.Data;
                Work.Destination = SSWM->Header;
                Work.FinalizeOperation = FinalizeAsset_SSWM;
                Work.FinalState = AssetState_Loaded;            
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

internal void
LoadTileset(editor_assets *Assets, tileset_id ID, b32 Immediate)
{
    asset *Asset = Assets->Assets + ID.Value;
    if(ID.Value)
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
                ssa_tileset *Info = &Asset->SSA.Tileset;

                u32 TilesSize = sizeof(ssa_tile)*Info->TileCount;
                u32 SizeData = TilesSize;
                u32 SizeTotal = SizeData + sizeof(asset_memory_header);

                Asset->Header = AcquireAssetMemory(Assets, SizeTotal, ID.Value, AssetType_Tileset);

                loaded_tileset *Tileset = &Asset->Header->Tileset;
                Tileset->BitmapIDOffset = GetFile(Assets, Asset->FileIndex)->AssetTypeOffsets[Asset_Tile];
                Tileset->Tiles = (ssa_tile *)(Asset->Header + 1);
                
                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->SSA.DataOffset;
                Work.Size = SizeData;
                Work.Destination = Tileset->Tiles;
                Work.FinalizeOperation = FinalizeAsset_None;
                Work.FinalState = AssetState_Loaded;

                if(Task)
                {
                    load_asset_work *TaskWork = PushStruct(&Task->Arena, load_asset_work);
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

internal void
LoadSpriteSheet(editor_assets *Assets, spritesheet_id ID, b32 Immediate)
{
    asset *Asset = Assets->Assets + ID.Value;
    if(ID.Value)
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
                ssa_spritesheet *Info = &Asset->SSA.SpriteSheet;

                u32 TilesSize = sizeof(bitmap_id)*Info->SpriteCount;
                u32 SizeData = TilesSize;
                u32 SizeTotal = SizeData + sizeof(asset_memory_header);

                Asset->Header = AcquireAssetMemory(Assets, SizeTotal, ID.Value, AssetType_SpriteSheet);

                loaded_spritesheet *SpriteSheet = &Asset->Header->SpriteSheet;
                SpriteSheet->BitmapIDOffset = GetFile(Assets, Asset->FileIndex)->AssetTypeOffsets[Asset_Sprite];
                SpriteSheet->SpriteIDs = (bitmap_id *)(Asset->Header + 1);
                
                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->SSA.DataOffset;
                Work.Size = SizeData;
                Work.Destination = SpriteSheet->SpriteIDs;
                Work.FinalizeOperation = FinalizeAsset_None;
                Work.FinalState = AssetState_Loaded;

                if(Task)
                {
                    load_asset_work *TaskWork = PushStruct(&Task->Arena, load_asset_work);
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

internal void
LoadText(editor_assets *Assets, text_id ID, b32 Immediate)
{
    asset *Asset = Assets->Assets + ID.Value;
    if(ID.Value)
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
                ssa_text *Info = &Asset->SSA.Text;

                u32 StringSize = Info->Length;
                u32 SizeData = StringSize;
                u32 SizeTotal = SizeData + sizeof(asset_memory_header);

                Asset->Header = AcquireAssetMemory(Assets, SizeTotal, ID.Value, AssetType_Text);

                loaded_text *Text = &Asset->Header->Text;
                Text->String = (char *)(Asset->Header + 1);
                
                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->SSA.DataOffset;
                Work.Size = SizeData;
                Work.Destination = Text->String;
                Work.FinalizeOperation = FinalizeAsset_None;
                Work.FinalState = AssetState_Loaded;

                if(Task)
                {
                    load_asset_work *TaskWork = PushStruct(&Task->Arena, load_asset_work);
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

internal void
LoadSound(editor_assets *Assets, sound_id ID)
{
//    TIMED_FUNCTION();

    asset *Asset = Assets->Assets + ID.Value;        
    if(ID.Value &&
       (AtomicCompareExchangeUInt32((uint32 *)&Asset->State, AssetState_Queued, AssetState_Unloaded) ==
        AssetState_Unloaded))
    {    
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState, false);
        if(Task)        
        {
            ssa_sound *Info = &Asset->SSA.Sound;

            asset_memory_size Size = {};
            Size.Section = Info->SampleCount*sizeof(int16);
            Size.Data = Info->ChannelCount*Size.Section;
            Size.Total = Size.Data + sizeof(asset_memory_header);

            Asset->Header = (asset_memory_header *)AcquireAssetMemory(Assets, Size.Total, ID.Value, AssetType_Sound);
            loaded_sound *Sound = &Asset->Header->Sound;

            Sound->SampleCount = Info->SampleCount;
            Sound->ChannelCount = Info->ChannelCount;
            u32 ChannelSize = Size.Section;

            void *Memory = (Asset->Header + 1);
            int16 *SoundAt = (int16 *)Memory;
            for(u32 ChannelIndex = 0;
                ChannelIndex < Sound->ChannelCount;
                ++ChannelIndex)
            {
                Sound->Samples[ChannelIndex] = SoundAt;
                SoundAt += ChannelSize;
            }

            load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Asset = Assets->Assets + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = Asset->SSA.DataOffset;
            Work->Size = Size.Data;
            Work->Destination = Memory;
            Work->FinalizeOperation = FinalizeAsset_None;
            Work->FinalState = (AssetState_Loaded);

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        }
        else
        {
            Assets->Assets[ID.Value].State = AssetState_Unloaded;
        }
    }
}

internal void
LoadFont(editor_assets *Assets, font_id ID, b32 Immediate)
{
//    TIMED_FUNCTION();

    // TODO(casey): Merge all this boilerplate!!!!  Same between LoadBitmap, LoadSound, and LoadFont
    asset *Asset = Assets->Assets + ID.Value;        
    if(ID.Value)
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
                ssa_font *Info = &Asset->SSA.Font;

                u32 HorizontalAdvanceSize = sizeof(r32)*Info->GlyphCount*Info->GlyphCount;
                u32 GlyphsSize = Info->GlyphCount*sizeof(ssa_font_glyph);
                u32 UnicodeMapSize = sizeof(u16)*Info->OnePastHighestCodePoint;
                u32 SizeData = GlyphsSize + HorizontalAdvanceSize;
                u32 SizeTotal = SizeData + sizeof(asset_memory_header) + UnicodeMapSize;

                Asset->Header = AcquireAssetMemory(Assets, SizeTotal, ID.Value, AssetType_Font);

                loaded_font *Font = &Asset->Header->Font;
                Font->BitmapIDOffset = GetFile(Assets, Asset->FileIndex)->AssetTypeOffsets[Asset_FontGlyph];
                Font->Glyphs = (ssa_font_glyph *)(Asset->Header + 1);
                Font->HorizontalAdvance = (r32 *)((u8 *)Font->Glyphs + GlyphsSize);
                Font->UnicodeMap = (u16 *)((u8 *)Font->HorizontalAdvance + HorizontalAdvanceSize);

                ZeroSize(UnicodeMapSize, Font->UnicodeMap);

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset = Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->SSA.DataOffset;
                Work.Size = SizeData;
                Work.Destination = Font->Glyphs;
                Work.FinalizeOperation = FinalizeAsset_Font;
                Work.FinalState = AssetState_Loaded;            
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

internal uint32
GetBestMatchAssetFrom(editor_assets *Assets, asset_type_id TypeID,
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
        for(uint32 TagIndex = Asset->SSA.FirstTagIndex;
            TagIndex < Asset->SSA.OnePastLastTagIndex;
            ++TagIndex)
        {
            ssa_tag *Tag = Assets->Tags + TagIndex;

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
GetTileBitmapByChecksumTag(editor_assets *Assets, u32 Checksum)
{
    bitmap_id Result = {};

    b32 Found = false;
    asset_type *Type = Assets->AssetTypes + Asset_Tile;
    for(uint32 AssetIndex = Type->FirstAssetIndex;
        AssetIndex < Type->OnePastLastAssetIndex && !Found;
        ++AssetIndex)
    {
        asset *Asset = Assets->Assets + AssetIndex;
        for(uint32 TagIndex = Asset->SSA.FirstTagIndex;
            TagIndex < Asset->SSA.OnePastLastTagIndex;
            ++TagIndex)
        {
            ssa_tag *Tag = Assets->Tags + TagIndex;
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
GetRandomAssetFrom(editor_assets *Assets, asset_type_id TypeID/*, random_series *Series*/)
{
    // TODO(paul): Fix once the engine has random generation

//    TIMED_FUNCTION();

    uint32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
        uint32 Count = (Type->OnePastLastAssetIndex - Type->FirstAssetIndex);
        uint32 Choice = 0;//RandomChoice(Series, Count);
        Result = Type->FirstAssetIndex + Choice;
    }

    return(Result);
}

internal uint32
GetFirstAssetFrom(editor_assets *Assets, asset_type_id TypeID)
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
GetBestMatchSSWMFrom(editor_assets *Assets, asset_type_id TypeID,
                     asset_vector *MatchVector, asset_vector *WeightVector)
{
    sswm_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline bitmap_id
GetBestMatchBitmapFrom(editor_assets *Assets, asset_type_id TypeID,
                       asset_vector *MatchVector, asset_vector *WeightVector)
{
    bitmap_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline bitmap_id
GetFirstBitmapFrom(editor_assets *Assets, asset_type_id TypeID)
{
    bitmap_id Result = {GetFirstAssetFrom(Assets, TypeID)};
    return(Result);
}

inline bitmap_id
GetRandomBitmapFrom(editor_assets *Assets, asset_type_id TypeID)
{
    bitmap_id Result = {GetRandomAssetFrom(Assets, TypeID)};
    return(Result);
}

inline sound_id
GetBestMatchSoundFrom(editor_assets *Assets, asset_type_id TypeID,
                       asset_vector *MatchVector, asset_vector *WeightVector)
{
    sound_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline sound_id
GetFirstSoundFrom(editor_assets *Assets, asset_type_id TypeID)
{
    sound_id Result = {GetFirstAssetFrom(Assets, TypeID)};
    return(Result);
}

inline sound_id
GetRandomSoundFrom(editor_assets *Assets, asset_type_id TypeID)
{
    sound_id Result = {GetRandomAssetFrom(Assets, TypeID)};
    return(Result);
}

inline font_id
GetBestMatchFontFrom(editor_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    font_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline tileset_id
GetBestMatchTilesetFrom(editor_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    tileset_id Result = {};
    Result.Value = GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector);

    return(Result);
}

inline spritesheet_id
GetBestMatchSpriteSheetFrom(editor_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    spritesheet_id Result = {};
    Result.Value = GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector);

    return(Result);
}

inline spritesheet_id
GetFirstSpriteSheetFrom(editor_assets *Assets, asset_type_id TypeID)
{
    spritesheet_id Result = {};
    Result.Value = GetFirstAssetFrom(Assets, TypeID);
    
    return(Result);
}


inline text_id
GetBestMatchTextFrom(editor_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    text_id Result = {};
    Result.Value = GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector);

    return(Result);
}

inline file_id
GetBestMatchFileFrom(editor_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    file_id Result = {};
    Result.Value = GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector);

    return(Result);
}

internal editor_assets *
AllocateEditorAssets(memory_arena *Arena, umm Size, transient_state *TranState,
                     platform_texture_op_queue *TextureOpQueue)
{
//    TIMED_FUNCTION();

    editor_assets *Assets = PushStruct(Arena, editor_assets);
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

    for(uint32 TagType = 0;
        TagType < Tag_Count;
        ++TagType)
    {
        Assets->TagRange[TagType] = 1000000.0f;
    }
    Assets->TagRange[Tag_FacingDirection] = Tau32;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;

    // NOTE(casey): This code was written using Snuffleupagus-Oriented Programming (SOP)
    {
        platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin(PlatformFileType_AssetFile);
        Assets->FileCount = FileGroup.FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for(u32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            ++FileIndex)
        {
            asset_file *File = Assets->Files + FileIndex;

            ZeroArray(Asset_Count, File->AssetTypeOffsets);
            File->TagBase = Assets->TagCount;

            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(&FileGroup);
            Platform.ReadDataFromFile(&File->Handle, 0, sizeof(File->Header), &File->Header);

            u32 AssetTypeArraySize = File->Header.AssetTypeCount*sizeof(ssa_asset_type);
            File->AssetTypeArray = (ssa_asset_type *)PushSize(Arena, AssetTypeArraySize);
            Platform.ReadDataFromFile(&File->Handle, File->Header.AssetTypes,
                                      AssetTypeArraySize, File->AssetTypeArray);

            if(File->Header.MagicValue != SSA_MAGIC_VALUE)
            {
                Platform.FileError(&File->Handle, "SSA file has an invalid magic value.");
            }

            if(File->Header.Version > SSA_VERSION)
            {
                Platform.FileError(&File->Handle, "SSA file is of a later version.");
            }

            if(PlatformNoFileErrors(&File->Handle))
            {
                // NOTE(casey): The first asset and tag slot in every
                // SSA is a null (reserved) so we don't count it as
                // something we will need space for!
                Assets->TagCount += (File->Header.TagCount - 1);
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
    Assets->Tags = PushArray(Arena, Assets->TagCount, ssa_tag);

    // NOTE(casey): Reserve one null tag at the beginning
    ZeroStruct(Assets->Tags[0]);

    // NOTE(casey): Load tags
    for(u32 FileIndex = 0;
        FileIndex < Assets->FileCount;
        ++FileIndex)
    {
        asset_file *File = Assets->Files + FileIndex;
        if(PlatformNoFileErrors(&File->Handle))
        {
            // NOTE(casey): Skip the first tag, since it's null
            u32 TagArraySize = sizeof(ssa_tag)*(File->Header.TagCount - 1);
            Platform.ReadDataFromFile(&File->Handle, File->Header.Tags + sizeof(ssa_tag),
                                      TagArraySize, Assets->Tags + File->TagBase);
        }
    }

    // NOTE(casey): Reserve one null asset at the beginning
    u32 AssetCount = 0;
    ZeroStruct(*(Assets->Assets + AssetCount));
    ++AssetCount;

    for(u32 DestTypeID = 0;
        DestTypeID < Asset_Count;
        ++DestTypeID)
    {
        asset_type *DestType = Assets->AssetTypes + DestTypeID;
        DestType->FirstAssetIndex = AssetCount;

        for(u32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            ++FileIndex)
        {
            asset_file *File = Assets->Files + FileIndex;
            if(PlatformNoFileErrors(&File->Handle))
            {
                for(u32 SourceIndex = 0;
                    SourceIndex < File->Header.AssetTypeCount;
                    ++SourceIndex)
                {
                    ssa_asset_type *SourceType = File->AssetTypeArray + SourceIndex;

                    if(SourceType->TypeID == DestTypeID)
                    {
                        u32 AssetCountForType = (SourceType->OnePastLastAssetIndex -
                                                 SourceType->FirstAssetIndex);

                        File->AssetTypeOffsets[SourceType->TypeID] = AssetCount - SourceType->FirstAssetIndex;

                        temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
                        ssa_asset *SSAAssetArray = PushArray(&TranState->TranArena,
                                                             AssetCountForType, ssa_asset);
                        Platform.ReadDataFromFile(&File->Handle,
                                                  File->Header.Assets +
                                                  SourceType->FirstAssetIndex*sizeof(ssa_asset),
                                                  AssetCountForType*sizeof(ssa_asset),
                                                  SSAAssetArray);
                        for(u32 AssetIndex = 0;
                            AssetIndex < AssetCountForType;
                            ++AssetIndex)
                        {
                            ssa_asset *SSAAsset = SSAAssetArray + AssetIndex;

                            Assert(AssetCount < Assets->AssetCount);
                            asset *Asset = Assets->Assets + AssetCount++;

                            Asset->FileIndex = FileIndex;
                            Asset->SSA = *SSAAsset;
                            if(Asset->SSA.FirstTagIndex == 0)
                            {
                                Asset->SSA.FirstTagIndex = Asset->SSA.OnePastLastTagIndex = 0;
                            }
                            else
                            {
                                Asset->SSA.FirstTagIndex += (File->TagBase - 1);
                                Asset->SSA.OnePastLastTagIndex += (File->TagBase - 1);
                            }
                        }

                        EndTemporaryMemory(TempMem);
                    }
                }
            }
        }

        DestType->OnePastLastAssetIndex = AssetCount;
    }

    Assert(AssetCount == Assets->AssetCount);
    
    return(Assets);
}

inline u32
GetGlyphFromCodePoint(ssa_font *Info, loaded_font *Font, u32 CodePoint)
{
    u32 Result = 0;
    if(CodePoint < Info->OnePastHighestCodePoint)
    {
        Result = Font->UnicodeMap[CodePoint];
        Assert(Result < Info->GlyphCount);
    }

    return(Result);
}

internal r32
GetHorizontalAdvanceForPair(ssa_font *Info, loaded_font *Font, u32 DesiredPrevCodePoint, u32 DesiredCodePoint)
{
    u32 PrevGlyph = GetGlyphFromCodePoint(Info, Font, DesiredPrevCodePoint);
    u32 Glyph = GetGlyphFromCodePoint(Info, Font, DesiredCodePoint);

    r32 Result = Font->HorizontalAdvance[PrevGlyph*Info->GlyphCount + Glyph];

    return(Result);
}

internal bitmap_id
GetBitmapForGlyph(editor_assets *Assets, ssa_font *Info, loaded_font *Font, u32 DesiredCodePoint)
{
    u32 Glyph = GetGlyphFromCodePoint(Info, Font, DesiredCodePoint);    
    bitmap_id Result = Font->Glyphs[Glyph].BitmapID;
    Result.Value += Font->BitmapIDOffset;

    return(Result);
}

internal r32

GetLineAdvanceFor(ssa_font *Info)
{
    r32 Result = Info->AscenderHeight + Info->DescenderHeight + Info->ExternalLeading;

    return(Result);
}

internal r32
GetStartingBaselineY(ssa_font *Info)
{
    r32 Result = Info->AscenderHeight;

    return(Result);
}

internal bitmap_id
GetBitmapForTile(editor_assets *Assets, ssa_tileset *Info, loaded_tileset *Tileset,
                 u32 TileIndex)
{
    Assert(TileIndex < Info->TileCount);
    bitmap_id Result = Tileset->Tiles[TileIndex].BitmapID;
    Result.Value += Tileset->BitmapIDOffset;
    
    return(Result);
}

inline sound_id
GetSoundEffectForType(editor_assets *Assets, sound_effect_type Type, u32 Variety = 0)
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

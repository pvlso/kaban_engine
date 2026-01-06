#if !defined(ENGINE_ASSET_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct loaded_sound
{
    int16 *Samples[2];
    u32 SampleCount; // NOTE(casey): This is the sample count divided by 8
    u32 ChannelCount;
};

struct loaded_tileset
{
    bitmap_id *TileGUIDs;
};

struct loaded_spritesheet
{
    bitmap_id *SpriteGUIDs;
};

struct loaded_text
{
    char *String;
};

struct loaded_file
{
    u32 Size;
    void *Data;
};

enum asset_state
{
    AssetState_Unloaded,
    AssetState_Queued,
    AssetState_Loaded,
};

enum asset_header_type
{
    AssetType_None,
    AssetType_Bitmap,
    AssetType_Sound,
    AssetType_Tileset,
    AssetType_SpriteSheet,
    AssetType_Text,
    AssetType_BinaryFile,
};

struct asset_memory_header
{
    asset_memory_header *Next;
    asset_memory_header *Prev;
    
    u32 AssetType;
    u32 AssetIndex;
    u32 TotalSize;
    u32 GenerationID;
    union
    {
        loaded_bitmap Bitmap;
        loaded_sound Sound;
        loaded_tileset Tileset;
        loaded_spritesheet SpriteSheet;
        loaded_text Text;
        loaded_file BinaryFile;
    };
};

struct asset
{
    u32 State;
    asset_memory_header *Header;

    kea_asset KEA;
    u32 FileIndex;
};

struct asset_file
{
    platform_file_handle Handle;
    kea_header Header;
};

enum asset_memory_block_flags
{
    AssetMemory_Used = 0x1,
};

struct asset_memory_block
{
    asset_memory_block *Prev;
    asset_memory_block *Next;
    u64 Flags;
    umm Size;
};

struct assets_tag_table
{
    u32 TagedAssetsIndeciesCount;
    u32 *TagedAssetsIndecies;

    u32 Capacity;
    kea_tag_lookup_entry *Entries;
};

struct engine_assets
{
    platform_texture_op_queue *TextureOpQueue;

    u32 TaskCount;
    task_with_memory *Tasks;
    platform_work_queue *LowPriorityQueue;

    u32 NextGenerationID;
    asset_memory_block MemorySentinel;
    asset_memory_header LoadedAssetSentinel;

    asset_file File;

    u32 TagMapCount;
    kea_tag_map *TagMaps;
    
    u32 TagCount;
    kea_tag *Tags;

    kea_asset_type_table_entry *TypeTable;
    u32 *TypeTableData[KEAType_Count];

    assets_tag_table TagTable;
    
    u32 AssetCount;
    asset *Assets;
    
    u32 OperationLock;
    u32 InFlightGenerationCount;
    u32 InFlightGenerations[16];
};

inline void
BeginAssetLock(engine_assets *Assets)
{
    for(;;)
    {
        if(AtomicCompareExchangeUInt32(&Assets->OperationLock, 1, 0) == 0)
        {
            break;
        }
    }
}

inline void
EndAssetLock(engine_assets *Assets)
{
    CompletePreviousWritesBeforeFutureWrites;
    Assets->OperationLock = 0;
}

inline void
InsertAssetHeaderAtFront(engine_assets *Assets, asset_memory_header *Header)
{
    asset_memory_header *Sentinel = &Assets->LoadedAssetSentinel;

    Header->Prev = Sentinel;
    Header->Next = Sentinel->Next;

    Header->Next->Prev = Header;
    Header->Prev->Next = Header;
}

inline void
RemoveAssetHeaderFromList(asset_memory_header *Header)
{
    Header->Prev->Next = Header->Next;
    Header->Next->Prev = Header->Prev;

    Header->Next = Header->Prev = 0;
}

inline asset *
GetAssetByGUID(engine_assets *Assets, u64 GUID)
{
    asset *Result = 0;

    s32 Low = 0;
    s32 High = Assets->AssetCount - 1;
    while(Low <= High)
    {
        s32 Mid = Low + ((High - Low) >> 1);
        asset *Asset = Assets->Assets + Mid;
        if(Asset->KEA.GUID < GUID)
            Low = Mid + 1;
        else if(Asset->KEA.GUID > GUID)
            High = Mid - 1;
        else
        {
            Result = Asset;
            break;
        }
    }

    return(Result);
}

inline asset_memory_header *
GetAsset(engine_assets *Assets, u64 ID, u32 GenerationID)
{
    asset *Asset = GetAssetByGUID(Assets, ID);
    
    asset_memory_header *Result = 0;

    BeginAssetLock(Assets);

    if(Asset->State == AssetState_Loaded)
    {        
        Result = Asset->Header;
        RemoveAssetHeaderFromList(Result);
        InsertAssetHeaderAtFront(Assets, Result);

        if(Asset->Header->GenerationID < GenerationID)
        {
            Asset->Header->GenerationID = GenerationID;
        }

        CompletePreviousWritesBeforeFutureWrites;
    }

    EndAssetLock(Assets);
    
    return(Result);
}

inline loaded_bitmap *
GetBitmap(engine_assets *Assets, bitmap_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_bitmap *Result = Header ? &Header->Bitmap : 0;

    return(Result);
}

inline kea_bitmap *
GetBitmapInfo(engine_assets *Assets, bitmap_id ID)
{
    asset *Asset = GetAssetByGUID(Assets, ID.Value);
    kea_bitmap *Result = &Asset->KEA.Bitmap;

    return(Result);
}

inline loaded_sound *
GetSound(engine_assets *Assets, sound_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_sound *Result = Header ? &Header->Sound : 0;

    return(Result);
}

inline kea_sound *
GetSoundInfo(engine_assets *Assets, sound_id ID)
{
    asset *Asset = GetAssetByGUID(Assets, ID.Value);
    kea_sound *Result = &Asset->KEA.Sound;

    return(Result);
}

inline loaded_tileset *
GetTileset(engine_assets *Assets, tileset_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_tileset *Result = Header ? &Header->Tileset : 0;

    return(Result);
}

inline kea_tileset *
GetTilesetInfo(engine_assets *Assets, tileset_id ID)
{
    asset *Asset = GetAssetByGUID(Assets, ID.Value);
    kea_tileset *Result = &Asset->KEA.Tileset;

    return(Result);
}

inline loaded_spritesheet *
GetSpriteSheet(engine_assets *Assets, spritesheet_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_spritesheet *Result = Header ? &Header->SpriteSheet : 0;

    return(Result);
}

inline kea_spritesheet *
GetSpriteSheetInfo(engine_assets *Assets, spritesheet_id ID)
{
    asset *Asset = GetAssetByGUID(Assets, ID.Value);
    kea_spritesheet *Result = &Asset->KEA.SpriteSheet;

    return(Result);
}

inline loaded_text *
GetText(engine_assets *Assets, text_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_text *Result = Header ? &Header->Text : 0;

    return(Result);
}

inline kea_text *
GetTextInfo(engine_assets *Assets, text_id ID)
{
    asset *Asset = GetAssetByGUID(Assets, ID.Value);
    kea_text *Result = &Asset->KEA.Text;

    return(Result);
}

inline loaded_file *
GetBinaryFile(engine_assets *Assets, file_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_file *Result = Header ? &Header->BinaryFile : 0;

    return(Result);
}

inline kea_binary_file *
GetBinaryFileInfo(engine_assets *Assets, file_id ID)
{
    asset *Asset = GetAssetByGUID(Assets, ID.Value);
    kea_binary_file *Result = &Asset->KEA.BinaryFile;

    return(Result);
}

inline bool32
IsValid(bitmap_id ID)
{
    bool32 Result = (ID.Value != 0);

    return(Result);
}

inline bool32
IsValid(sound_id ID)
{
    bool32 Result = (ID.Value != 0);

    return(Result);
}

inline bool32
IsValid(spritesheet_id ID)
{
    bool32 Result = (ID.Value != 0);

    return(Result);
}

internal void LoadAsset(engine_assets *Assets, asset_header_type HType, u64 ID, b32 Immediate);
inline void PrefetchAsset(engine_assets *Assets, asset_header_type HType, u32 ID, b32 Immediate = false) {LoadAsset(Assets, HType, ID, Immediate);}

inline void LoadBitmap(engine_assets *Assets, bitmap_id ID, b32 Immediate)
{
    LoadAsset(Assets, AssetType_Bitmap, ID.Value, Immediate);
}

inline void LoadSound(engine_assets *Assets, sound_id ID, b32 Immediate)
{
    LoadAsset(Assets, AssetType_Sound, ID.Value, Immediate);
}

inline void LoadTileset(engine_assets *Assets, tileset_id ID, b32 Immediate)
{
    LoadAsset(Assets, AssetType_Tileset, ID.Value, Immediate);
}

inline void LoadSpriteSheet(engine_assets *Assets, spritesheet_id ID, b32 Immediate)
{
    LoadAsset(Assets, AssetType_SpriteSheet, ID.Value, Immediate);
}

inline void LoadText(engine_assets *Assets, text_id ID, b32 Immediate)
{
    LoadAsset(Assets, AssetType_Text, ID.Value, Immediate);
}

inline void LoadBinaryFile(engine_assets *Assets, file_id ID, b32 Immediate)
{
    LoadAsset(Assets, AssetType_BinaryFile, ID.Value, Immediate);
}

inline void PrefetchBitmap(engine_assets *Assets, bitmap_id ID, b32 Immediate = false) {LoadBitmap(Assets, ID, Immediate);}
inline void PrefetchSound(engine_assets *Assets, sound_id ID) {LoadSound(Assets, ID, false);}
inline void PrefetchTileset(engine_assets *Assets, tileset_id ID) {LoadTileset(Assets, ID, false);}
inline void PrefetchSpriteSheet(engine_assets *Assets, spritesheet_id ID) {LoadSpriteSheet(Assets, ID, false);}
inline void PrefetchText(engine_assets *Assets, text_id ID) {LoadText(Assets, ID, false);}
inline void PrefetchBinaryFile(engine_assets *Assets, file_id ID) {LoadBinaryFile(Assets, ID, false);}

inline sound_id GetNextSoundInChain(engine_assets *Assets, sound_id ID)
{
    sound_id Result = {};

    kea_sound *Info = GetSoundInfo(Assets, ID);
    switch(Info->Chain)
    {
        case KEASoundChain_None:
        {
            // NOTE(casey): Nothing to do.
        } break;

        case KEASoundChain_Loop:
        {
            Result = ID;
        } break;

        case KEASoundChain_Advance:
        {
            // TODO(pvlso): Instead of incrementing the id we need to get the
            // GUID of the next piece
//            Result.Value = ID.Value + 1;
            Assert(!"Not Implemented");
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }

    return(Result);
}

inline u32
BeginGeneration(engine_assets *Assets)
{
    BeginAssetLock(Assets);

    Assert(Assets->InFlightGenerationCount < ArrayCount(Assets->InFlightGenerations));
    u32 Result = Assets->NextGenerationID++;
    Assets->InFlightGenerations[Assets->InFlightGenerationCount++] = Result;

    EndAssetLock(Assets);
    
    return(Result);
}

inline void
EndGeneration(engine_assets *Assets, u32 GenerationID)
{
    BeginAssetLock(Assets);

    for(u32 Index = 0;
        Index < Assets->InFlightGenerationCount;
        ++Index)
    {
        if(Assets->InFlightGenerations[Index] == GenerationID)
        {
            Assets->InFlightGenerations[Index] =
                Assets->InFlightGenerations[--Assets->InFlightGenerationCount];
            break;
        }
    }    

    EndAssetLock(Assets);    
}

#define ENGINE_ASSET_H
#endif

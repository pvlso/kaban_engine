#if !defined(EDITOR_ASSETS_H)
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

struct loaded_font
{
    ssa_font_glyph *Glyphs;
    r32 *HorizontalAdvance;
    s32 BitmapIDOffset;
    u16 *UnicodeMap;
};

struct loaded_tileset
{
    ssa_tile *Tiles;
    s32 BitmapIDOffset;
};

struct loaded_spritesheet
{
    bitmap_id *SpriteIDs;
    s32 BitmapIDOffset;
};

struct loaded_quest
{
    text_id *TextIDs;
    s32 TextIDOffset;
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

struct loaded_world_map
{
    sswm_header *Header;
    sswm_ground_tile *GroundTiles;
    sswm_entity *Entities;
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
    AssetType_Font,
    AssetType_Tileset,
    AssetType_SpriteSheet,
    AssetType_Text,
    AssetType_Quest,
    AssetType_BinaryFile,
    AssetType_SSWM,
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
        loaded_font Font;
        loaded_tileset Tileset;
        loaded_spritesheet SpriteSheet;
        loaded_text Text;
        loaded_quest Quest;
        loaded_file BinaryFile;
        loaded_world_map SSWM;
    };
};

struct asset
{
    u32 State;
    asset_memory_header *Header;

    ssa_asset SSA;
    u32 FileIndex;
};

struct asset_vector
{
    u32 E[Tag_Count];
};

struct asset_type
{
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

struct asset_file
{
    platform_file_handle Handle;

    // TODO(casey): If we ever do thread stacks, AssetTypeArray
    // doesn't actually need to be kept here probably.
    ssa_header Header;
    ssa_asset_type *AssetTypeArray;

    u32 TagBase;
    u32 AssetTypeOffsets[Asset_Count];
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

struct editor_assets
{
    platform_texture_op_queue *TextureOpQueue;
    u32 NextGenerationID;
    
    // TODO(casey): Not thrilled about this back-pointer
    struct transient_state *TranState;

    asset_memory_block MemorySentinel;
    asset_memory_header LoadedAssetSentinel;
    
    real32 TagRange[Tag_Count];

    u32 FileCount;
    asset_file *Files;
    
    uint32 TagCount;
    ssa_tag *Tags;

    uint32 AssetCount;
    asset *Assets;
    
    asset_type AssetTypes[Asset_Count];
    
    u32 OperationLock;

    u32 InFlightGenerationCount;
    u32 InFlightGenerations[16];
};

inline void
BeginAssetLock(editor_assets *Assets)
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
EndAssetLock(editor_assets *Assets)
{
    CompletePreviousWritesBeforeFutureWrites;
    Assets->OperationLock = 0;
}

inline void
InsertAssetHeaderAtFront(editor_assets *Assets, asset_memory_header *Header)
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

inline asset_memory_header *
GetAsset(editor_assets *Assets, u32 ID, u32 GenerationID)
{
    Assert(ID <= Assets->AssetCount);
    asset *Asset = Assets->Assets + ID;
    
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

inline loaded_world_map *
GetSSWM(editor_assets *Assets, sswm_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_world_map *Result = Header ? &Header->SSWM : 0;

    return(Result);
}

inline loaded_bitmap *
GetBitmap(editor_assets *Assets, bitmap_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_bitmap *Result = Header ? &Header->Bitmap : 0;

    return(Result);
}

inline ssa_bitmap *
GetBitmapInfo(editor_assets *Assets, bitmap_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    ssa_bitmap *Result = &Assets->Assets[ID.Value].SSA.Bitmap;

    return(Result);
}

inline loaded_sound *
GetSound(editor_assets *Assets, sound_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_sound *Result = Header ? &Header->Sound : 0;

    return(Result);
}

inline ssa_sound *
GetSoundInfo(editor_assets *Assets, sound_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    ssa_sound *Result = &Assets->Assets[ID.Value].SSA.Sound;

    return(Result);
}

inline loaded_font *
GetFont(editor_assets *Assets, font_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_font *Result = Header ? &Header->Font : 0;

    return(Result);
}

inline ssa_font *
GetFontInfo(editor_assets *Assets, font_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    ssa_font *Result = &Assets->Assets[ID.Value].SSA.Font;

    return(Result);
}

inline loaded_tileset *
GetTileset(editor_assets *Assets, tileset_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_tileset *Result = Header ? &Header->Tileset : 0;

    return(Result);
}

inline ssa_tileset *
GetTilesetInfo(editor_assets *Assets, tileset_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    ssa_tileset *Result = &Assets->Assets[ID.Value].SSA.Tileset;

    return(Result);
}

inline loaded_spritesheet *
GetSpriteSheet(editor_assets *Assets, spritesheet_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_spritesheet *Result = Header ? &Header->SpriteSheet : 0;

    return(Result);
}

inline ssa_spritesheet *
GetSpriteSheetInfo(editor_assets *Assets, spritesheet_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    ssa_spritesheet *Result = &Assets->Assets[ID.Value].SSA.SpriteSheet;

    return(Result);
}

inline loaded_text *
GetText(editor_assets *Assets, text_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_text *Result = Header ? &Header->Text : 0;

    return(Result);
}

inline ssa_text *
GetTextInfo(editor_assets *Assets, text_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    ssa_text *Result = &Assets->Assets[ID.Value].SSA.Text;

    return(Result);
}

inline loaded_file *
GetBinaryFile(editor_assets *Assets, file_id ID, u32 GenerationID)
{
    asset_memory_header *Header = GetAsset(Assets, ID.Value, GenerationID);

    loaded_file *Result = Header ? &Header->BinaryFile : 0;

    return(Result);
}

inline ssa_binary_file *
GetBinaryFileInfo(editor_assets *Assets, file_id ID)
{
    Assert(ID.Value <= Assets->AssetCount);
    ssa_binary_file *Result = &Assets->Assets[ID.Value].SSA.BinaryFile;

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

internal void LoadBitmap(editor_assets *Assets, bitmap_id ID, b32 Immediate);
inline void PrefetchBitmap(editor_assets *Assets, bitmap_id ID, b32 Immediate = false) {LoadBitmap(Assets, ID, Immediate);}

internal void LoadSound(editor_assets *Assets, sound_id ID);
inline void PrefetchSound(editor_assets *Assets, sound_id ID) {LoadSound(Assets, ID);}

internal void LoadFont(editor_assets *Assets, font_id ID, b32 Immediate);
inline void PrefetchFont(editor_assets *Assets, font_id ID) {LoadFont(Assets, ID, false);}

internal void LoadTileset(editor_assets *Assets, tileset_id ID, b32 Immediate);
inline void PrefetchTileset(editor_assets *Assets, tileset_id ID) {LoadTileset(Assets, ID, false);}

internal void LoadSpriteSheet(editor_assets *Assets, spritesheet_id ID, b32 Immediate);
inline void PrefetchSpriteSheet(editor_assets *Assets, spritesheet_id ID) {LoadSpriteSheet(Assets, ID, false);}

internal void LoadText(editor_assets *Assets, text_id ID, b32 Immediate);
inline void PrefetchText(editor_assets *Assets, text_id ID) {LoadText(Assets, ID, false);}

internal void LoadBinaryFile(editor_assets *Assets, file_id ID, b32 Immediate);
inline void PrefetchBinaryFile(editor_assets *Assets, file_id ID) {LoadBinaryFile(Assets, ID, false);}

internal void LoadSSWM(editor_assets *Assets, sswm_id ID, b32 Immediate);
inline void PrefetchSSWM(editor_assets *Assets, sswm_id ID) {LoadSSWM(Assets, ID, false);}

inline sound_id GetNextSoundInChain(editor_assets *Assets, sound_id ID)
{
    sound_id Result = {};

    ssa_sound *Info = GetSoundInfo(Assets, ID);
    switch(Info->Chain)
    {
        case SSASoundChain_None:
        {
            // NOTE(casey): Nothing to do.
        } break;

        case SSASoundChain_Loop:
        {
            Result = ID;
        } break;

        case SSASoundChain_Advance:
        {
            Result.Value = ID.Value + 1;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }

    return(Result);
}

inline u32
BeginGeneration(editor_assets *Assets)
{
    BeginAssetLock(Assets);

    Assert(Assets->InFlightGenerationCount < ArrayCount(Assets->InFlightGenerations));
    u32 Result = Assets->NextGenerationID++;
    Assets->InFlightGenerations[Assets->InFlightGenerationCount++] = Result;

    EndAssetLock(Assets);
    
    return(Result);
}

inline void
EndGeneration(editor_assets *Assets, u32 GenerationID)
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

#define EDITOR_ASSETS_H
#endif

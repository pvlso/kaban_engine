#if !defined(EDITOR_ASSETS_MODE_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */
#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;
    uint32 FileSize;
    uint16 Reserved1;
    uint16 Reserved2;
    uint32 BitmapOffset;
    uint32 Size;
    int32 Width;
    int32 Height;
    uint16 Planes;
    uint16 BitsPerPixel;
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorsImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};

struct WAVE_header
{
    uint32 RIFFID;
    uint32 Size;
    uint32 WAVEID;
};

#define RIFF_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
enum
{
    WAVE_ChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
    WAVE_ChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
    WAVE_ChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
    WAVE_ChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E'),
};

struct WAVE_chunk
{
    uint32 ID;
    uint32 Size;
};

struct WAVE_fmt
{
    uint16 wFormatTag;
    uint16 nChannels;
    uint32 nSamplesPerSec;
    uint32 nAvgBytesPerSec;
    uint16 nBlockAlign;
    uint16 wBitsPerSample;
    uint16 cbSize;
    uint16 wValidBitsPerSample;
    uint32 dwChannelMask;
    uint8 SubFormat[16];
};

#pragma pack(pop)

enum stored_asset_type
{
    StoredAssetType_None,

    StoredAssetType_Bitmap,
    StoredAssetType_SpriteSheet,
    StoredAssetType_Tileset,
    StoredAssetType_Sound,
    StoredAssetType_Text,
    StoredAssetType_Font,
    StoredAssetType_File,
    StoredAssetType_SSWM,

    StoredAssetType_Count,
};

struct stored_asset_bitmap
{
    char FileName[256];
    v2 AlignPercentage;
};

struct stored_asset_spritesheet
{
    char SourceFileName[256];
    u32 SpriteCount;
    u32 SpriteWidth;
    u32 SpriteHeight;
    v2 SpriteAlignPercentage;
};

struct stored_asset_tileset
{
    char SourceFileName[256];
    char MergeTileFileName[256];

    b32 MergedTile;
    u32 TileCount;

    u32 TileWidth;
    u32 TileHeight;

    u32 TileOffsetsX[512];
    u32 TileOffsetsY[512];
};

struct stored_asset_sound
{
    char SourceFileName[256];
    u32 FirstSampleIndex;
    u32 Chain;
};

struct stored_asset_text
{
    char SourceFileName[256];
};

struct stored_asset_font
{
    char SourceFileName[256];
    u32 CodePointCount;
    u32 FirstCodePoint;
    u32 LastCodePoint;
    u32 FontSizeInPixels;
};

struct stored_asset_binary_file
{
    char SourceFileName[256];
    u32 FileSize;
};

struct stored_asset_sswm_file
{
    char SourceFileName[256];
    u32 FileSize;
};

struct stored_asset
{
    u32 GUID;

    u32 TypeID;
    stored_asset_type Type;

    u32 TagCount;
    kea_tag AssetTags[32];
    union
    {
        stored_asset_bitmap Bitmap;
        stored_asset_spritesheet SpriteSheet;
        stored_asset_tileset Tileset;
        stored_asset_sound Sound;
        stored_asset_text Text;
        stored_asset_font Font;
        stored_asset_binary_file File;
        stored_asset_sswm_file SSWM;
    };
};

struct stored_asset_file_header
{
    u32 SizeOfStoredAsset;
    u32 AssetCount;
    u32 Version;
};

enum assets_edit_mode
{
    EditMode_None,
    EditMode_Bitmap,
    EditMode_SpriteSheet,
    EditMode_Tileset,
    EditMode_Sound,
    EditMode_Text,
    EditMode_Font,
    EditMode_File,
    EditMode_SSWM,
};

// NOTE(paul): Bitmaps
struct bitmap_mode
{
    loaded_bitmap Bitmap;
};

// NOTE(paul): SpriteSheets
struct spritesheet_mode
{
    b32 ShowAnimated;
    b32 CutSpriteSheet;
    b32 SpriteSheetLoaded;
    loaded_bitmap SpriteSheetBitmap;
    loaded_bitmap Sprites[256];
};

// NOTE(paul): Tilesets
struct tileset_mode
{
    b32 CutTileset;
    b32 CutWithMergeTileset;
    b32 ShowTiles;
    u32 CurrentTileIndex;

    loaded_bitmap MergeTileBitmap;
    loaded_bitmap TilesetBitmap;
    loaded_bitmap Tiles[512];
};

// NOTE(paul): Sounds
struct sound_mode
{
    b32 PlaySound;
    b32 StopSound;
    loaded_sound Sound;
    void *FreeSound;

    playing_sound *PlayingSound;
};

// NOTE(paul): Texts
struct text_mode
{
    b32 EditTextFile;
    b32 Reload;
    loaded_text Text;
};

// NOTE(paul): Fonts
struct font_mode
{
    builder_loaded_font Font;
};

// NOTE(paul): Files
struct binary_file_mode
{
    u8 *FileData;
};

// NOTE(paul): SSWMs
struct sswm_mode
{
    u8 *FileData;
};

static platform_file_type StoredToSourceTypeMap[StoredAssetType_Count] =
{
    PlatformFileType_None, PlatformFileType_BMP,
    PlatformFileType_SSBMP, PlatformFileType_TSBMP,
    PlatformFileType_WAV, PlatformFileType_TXT,
    PlatformFileType_TTF, PlatformFileType_BIN,
    PlatformFileType_SSWM
};

struct tag_map_list
{
    kea_tag_map Tag;
    tag_map_list *Next;
};

struct editor_mode_assets
{
    b32 AssetsInitialized;
    
    b32 Exit;
    assets_edit_mode LastEditMode;
    assets_edit_mode EditMode;

    memory_arena UtilityArena;
    memory_arena UtilityTempArena;
    
    stored_asset_file_header StoredHeader;
    b32 StoredAssetChanged;
    b32 EditStoredAsset;
    b32 RemoveStoredAsset;

    b32 ShowStoredAssets;
    u32 ShowStoredAssetIndex;
    u32 LastShowStoredAssetIndex;
    stored_asset *StoredAssets;

    u32 NextStoredAssetID;
    u32 AddAssetCount;
    stored_asset AssetsToAdd[256];
    
    b32 WriteSSA;
    b32 WriteAssets;

    stored_asset *CurrentAsset;
    
    b32 AddAsset;
    b32 RemoveTag;
    b32 AddTag;

    u32 LastTagID;
    u32 CurrentTagID;
    u32 CurrentTagValue;
    u32 CurrentTag;

    u32 TagMapListCount;
    tag_map_list *TagMapListHead;
    
    u32 FileIndex;
    u32 LastFileIndex;
    u32 SubFileIndex;
    u32 LastSubFileIndex;

    u32 SourceFileCounts[StoredAssetType_Count];
    char **SourceFiles[StoredAssetType_Count];
    u32 SolidTileFileCount;
    char **SolidTileFiles;
    
    v2 PixelPosition;
    r32 Time;

    union
    {
        bitmap_mode BitmapMode;
        spritesheet_mode SpriteSheetMode;
        tileset_mode TilesetMode;
        sound_mode SoundMode;
        text_mode TextMode;
        font_mode FontMode;
        binary_file_mode BinaryFileMode;
        sswm_mode SSWMMode;
    };
};

inline assets_edit_mode
AssetsEditModeFromStoredType(u32 StoredType)
{
    assets_edit_mode Result = EditMode_None;
    switch(StoredType)
    {
        case StoredAssetType_None:        {}                               break;
        case StoredAssetType_Bitmap:      {Result = EditMode_Bitmap;}      break;
        case StoredAssetType_SpriteSheet: {Result = EditMode_SpriteSheet;} break;
        case StoredAssetType_Tileset:     {Result = EditMode_Tileset;}     break;
        case StoredAssetType_Sound:       {Result = EditMode_Sound;}       break;
        case StoredAssetType_Text:        {Result = EditMode_Text;}        break;
        case StoredAssetType_Font:        {Result = EditMode_Font;}        break;
        case StoredAssetType_File:        {Result = EditMode_File;}        break;
        case StoredAssetType_SSWM:        {Result = EditMode_SSWM;}        break;
        InvalidDefaultCase;
    }

    return(Result);
}

inline stored_asset_type
StoredAssetTypeFromEditMode(u32 EditMode)
{
    stored_asset_type Result = StoredAssetType_None;
    switch(EditMode)
    {
        case EditMode_None:        {}                                      break;
        case EditMode_Bitmap:      {Result = StoredAssetType_Bitmap;}      break;
        case EditMode_SpriteSheet: {Result = StoredAssetType_SpriteSheet;} break;
        case EditMode_Tileset:     {Result = StoredAssetType_Tileset;}     break;
        case EditMode_Sound:       {Result = StoredAssetType_Sound;}       break;
        case EditMode_Text:        {Result = StoredAssetType_Text;}        break;
        case EditMode_Font:        {Result = StoredAssetType_Font;}        break;
        case EditMode_File:        {Result = StoredAssetType_File;}        break;
        case EditMode_SSWM:        {Result = StoredAssetType_SSWM;}        break;
        InvalidDefaultCase;
    }

    return(Result);
}

inline tag_map_list *
GetTagMapByIndex(editor_mode_assets *AssetsMode, u32 Index)
{
    tag_map_list *Result = AssetsMode->TagMapListHead;
    for(u32 I = 0;
        I < AssetsMode->TagMapListCount;
        ++I)
    {
        if(I == Index)
            break;
        Result = Result->Next;
    }

    return(Result);
}

internal void PlayAssetsMode(editor_state *EditorState, transient_state *TranState);

#define EDITOR_ASSETS_MODE_H
#endif

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
    u32 ID;

    u32 TypeID;
    stored_asset_type Type;

    u32 TagCount;
    ssa_tag AssetTags[32];
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

struct builder_loaded_font
{
    u32 Nothing;
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

struct editor_mode_assets
{
    b32 AssetsInitialized;
    
    b32 Exit;
    assets_edit_mode LastEditMode;
    assets_edit_mode EditMode;

    memory_arena UtilityArena;
    
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

    u32 FileIndex;
    u32 LastFileIndex;
    u32 SubFileIndex;
    u32 LastSubFileIndex;

    u32 BitmapFileCount;
    char **BitmapFiles;
    u32 SpriteSheetFileCount;
    char **SpriteSheetFiles;
    u32 SolidTileFileCount;
    char **SolidTileFiles;
    u32 TilesetFileCount;
    char **TilesetFiles;
    u32 SoundFileCount;
    char **SoundFiles;
    u32 TextFileCount;
    char **TextFiles;
    u32 FontFileCount;
    char **FontFiles;
    u32 BinaryFileCount;
    char **BinaryFiles;
    u32 SSWMFileCount;
    char **SSWMFiles;

    v2 PixelPosition;
    r32 Time;

    json_element *JsonStringsHead;    
    string_array *EnumStringArraysHash[4096];

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

internal void PlayAssetsMode(editor_state *EditorState, transient_state *TranState);

#define EDITOR_ASSETS_MODE_H
#endif

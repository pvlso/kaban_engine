#if !defined(ENGINE_FILE_FORMATS__H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: pvlso $
   $Notice: $
   ======================================================================== */

#define TAG_KEY_LENGTH 64
#define MAX_NUMBER_OF_TAGS 32

/*
  NOTE(paul): Current Formats

  - Bitmap (BMP)

  - Waveform Audio (WAV)

  - Kaban Engine Assets (KEA)

  - Kaban Engine Stored Assets (KESA)

  - Kaban Engine World Map (KEWM)
*/

#define ENCODE_MAGIC(a, b, c, d) (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))


#pragma pack(push, 1)
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): BMP
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): WAV
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): TTF
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#define ONE_PAST_MAX_FONT_CODEPOINT (0x10FFFF + 1)
#define MAX_FONT_WIDTH 1024
#define MAX_FONT_HEIGHT 1024

struct ttf_offset_subtable
{
    u32 ScalerType;
    u16 NumTables;
    u16 SearchRange;
    u16 EntrySelector;
    u16 RangeShift;
};

struct ttf_table_directory
{
    u32 Tag;
    u32 CheckSum;
    u32 Offset;
    u32 Length;
};

struct ttf_name_record
{
    u16 PlatformID;
    u16 EncodingID;
    u16 LanguageID;
    u16 NameID;
    u16 Length;
    u16 Offset;
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): KEA
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define KEA_MAGIC_VALUE ENCODE_MAGIC('k', 'e', 'a', 'f')

enum kea_asset_type
{
    KEAType_None,
    KEAType_Bitmap,
    KEAType_Sound,
    KEAType_Font,
    KEAType_FontGlyph,
    KEAType_SpriteSheet,
    KEAType_Sprite,
    KEAType_Tileset,
    KEAType_Tile,
    KEAType_BIN,
    KEAType_SSWM,

    KEAType_Count,
};

struct kea_tag
{
    u64 GUID; // NOTE(pvlso): made of Key + Value
    char Key[TAG_KEY_LENGTH];
    char Value[TAG_KEY_LENGTH];
};

struct kea_tag_map
{
    u64 GUID; // NOTE(pvlso): made of Key
    char Key[TAG_KEY_LENGTH];

    u32 ValueCount;
    char Values[32][TAG_KEY_LENGTH];
//    u64 TagValueGUIDs[32]; // NOTE(pvlso): Made of TagMap->Key + TagMap->Values[I]
};

struct kea_tag_table_entry
{
    u64 GUID; // NOTE(pvlso): Tag pair GUID: Tag->Key + Tag->Value
    u64 BitSetOffset;
};

struct kea_asset_type_table_entry
{
    u32 Type;
    u32 TypeCount;
    u64 AssetsIndeciesOffset;
};

struct kea_header
{
    u32 MagicValue;
    u32 Version;

    u32 TagCount;
    u64 TagMapsOffset; // NOTE(pvlso): Sorted array of kea_tag_map by GUID

    u32 UsedTagsCount;
    u64 UsedTagsArrayOffset; // NOTE(pvlso): Sorted array of kea_tag by GUID
    u64 TagTableOffset; // NOTE(pvlso): Sorted array of kea_tag_table_entry by GUID

    u32 AssetTypeCount;
    u64 AssetTypeTableOffset; // NOTE(pvlso): Sorted array of kea_asset_type_table_entry by Type -> enum

    u32 AssetCount;
    u64 AssetsOffset; // NOTE(pvlso): Sorted array of kea_asset by GUID
};

enum kea_sound_chain
{
    KEASoundChain_None,
    KEASoundChain_Loop,
    KEASoundChain_Advance,
    KEASoundChain_Count,
};

static char *SoundChainStringArray[KEASoundChain_Count] =
{
    "KEASoundChain_None",
    "KEASoundChain_Loop",
    "KEASoundChain_Advance"
};

struct kea_bitmap
{
    u32 Dim[2];
    r32 AlignPercentage[2];

    /* NOTE(casey): Data is:

       u32 Pixels[Dim[1]][Dim[0]]
    */
};

struct kea_sound
{
    u32 SampleCount;
    u32 ChannelCount;
    u32 Chain; // NOTE(casey): ssa_sound_chain

    /* NOTE(casey): Data is:

       s16 Channels[ChannelCount][SampleCount]
    */
};

struct kea_font_glyph
{
    u32 UnicodeCodePoint;
    u32 BitmapGUID;
};

struct kea_font
{
    u32 OnePastHighestCodePoint;
    u32 GlyphCount;
    r32 AscenderHeight;
    r32 DescenderHeight;
    r32 ExternalLeading;

    /* NOTE(casey): Data is:

       ssa_font_glyph CodePoints[GlyphCount];
       r32 HorizontalAdvance[GlyphCount][GlyphCount];
    */
};

struct kea_tile
{
    u32 BitmapGUID;
    u32 CheckSum;
};

struct kea_tileset
{
    u32 TileCount;

    /* NOTE(paul): Data is:

       kea_tile Tiles[TileCount];
    */
};

struct kea_spritesheet
{
    u32 SpriteCount;

    /* NOTE(paul): Data is:

       u32 SpriteGUIDs[SpriteCount];
    */
};

struct kea_text
{
    u32 Length;

    /* NOTE(paul): Data is:

       u8 Symbols[Length];
    */
};

struct kea_binary_file
{
    u32 Size;
    /* NOTE(paul): Data is:
       u8 Data[Size];
    */
};

struct kea_asset
{
    u64 DataOffset;
    u64 GUID;

    u32 AssetIndex;
    u32 TagIndecies[MAX_NUMBER_OF_TAGS];

    u32 Type;
    union
    {
        kea_bitmap Bitmap;
        kea_sound Sound;
        kea_font Font;
        kea_tileset Tileset;
        kea_spritesheet SpriteSheet;
        kea_text Text;
        kea_binary_file BinaryFile;
    };
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): KET
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define KET_MAGIC_VALUE ENCODE_MAGIC('k', 'e', 't', 'f')

struct ket_header
{
    u32 MagicValue;
    u32 Version;
    
    u32 TagCount;

    u64 TagArrayOffset;
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): KESA
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define KESA_MAGIC_VALUE ENCODE_MAGIC('k', 'e', 's', 'a')

struct kesa_header
{
    u32 MagicValue;
    u32 Version;
    
    u32 TagsVersion;
    u32 SizeOfStoredAsset;
    u32 AssetCount;

    u64 AssetsOffset;
};

enum kesa_type
{
    KESA_None,

    KESA_Bitmap,
    KESA_SpriteSheet,
    KESA_Tileset,
    KESA_Sound,
    KESA_Text,
    KESA_Font,
    KESA_File,
    KESA_SSWM,

    KESA_Count,
};

struct kesa_bitmap
{
    v2 AlignPercentage;
};

struct kesa_spritesheet
{
    u32 SpriteCount;
    u32 SpriteWidth;
    u32 SpriteHeight;
    v2 SpriteAlignPercentage;
};

struct kesa_tileset
{
    char MergeTileFileName[256];

    b32 MergedTile;
    u32 TileCount;

    u32 TileWidth;
    u32 TileHeight;

    u8 TileOffsetsX[512];
    u8 TileOffsetsY[512];
};

struct kesa_sound
{
    u32 FirstSampleIndex;
    u32 Chain;
};

struct kesa_text
{
    u32 PLACEHOLDER;
};

struct kesa_font
{
    u32 CodePointCount;
    u32 FirstCodePoint;
    u32 LastCodePoint;
    u32 FontSizeInPixels;
};

struct kesa_binary_file
{
    u32 FileSize;
};

struct kesa_sswm_file
{
    u32 FileSize;
};

struct kesa_tag
{
    u64 TagGUID;
    u32 TagValueIndex;
};

struct kesa_asset
{
    // NOTE(pvlso): made of SourceFileName, for
    // Tileset made of SourceFileName + MergeTileFileName
    u64 GUID; 

    kesa_type Type;

    u32 TagCount;
    kesa_tag AssetTags[MAX_NUMBER_OF_TAGS];
    char SourceFileName[256];
    union
    {
        kesa_bitmap Bitmap;
        kesa_spritesheet SpriteSheet;
        kesa_tileset Tileset;
        kesa_sound Sound;
        kesa_text Text;
        kesa_font Font;
        kesa_binary_file File;
        kesa_sswm_file SSWM;
    };
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma pack(pop)

#define ENGINE_FILE_FORMATS__H
#endif

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
    KEAType_SpriteSheet,
    KEAType_Sprite,
    KEAType_Tileset,
    KEAType_Tile,
    KEAType_TXT,
    KEAType_BIN,
    KEAType_SSWM,
    KEAType_NormalMap, // NOTE(pvlso): a sprite's or tile's normal map, its GUID is NormalMapGUID(the image's GUID)
    KEAType_Map,       // NOTE(pvlso): a map made in the map editor (engine_map_file.h)
    KEAType_Entity,    // NOTE(pvlso): an entity made in the entity editor (engine_entity_file.h)

    KEAType_Count,
};

struct kea_tag
{
    u64 GUID;
};

struct kea_asset_type_table_entry
{
    u32 Type;
    u32 TypeCount;
    u64 AssetsIndeciesOffset;
};

struct kea_tag_lookup_entry
{
    u64 GUID;

    u32 TagIndex;
    u32 TagValueIndex;

    u32 AssetsFirstIndex;
    u32 Count;
};

struct kea_tag_hash_table
{
    u32 Capacity;
    u64 TableOffset;
};

struct kea_header
{
    u32 MagicValue;
    u32 Version;
    
    u32 TagCount;
    u64 TagMapsOffset; // NOTE(pvlso): Sorted array of ket_tag by GUID (defined below)

    u32 UsedTagsCount;
    u64 UsedTagsArrayOffset; // NOTE(pvlso): array of kea_tag

    kea_tag_hash_table HashTable;
    u32 TagedAssetsIndeciesCount;
    u64 TagedAssetsIndeciesOffset; // NOTE(pvlso): array of asset indecies for kea_tag_lookup
    
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

// NOTE(pvlso): A curated tile set (data/tilesets/<name>.txt): tiles gathered from any sheets, laid out in the set's
// own grid, each with the project's flags (data/txts/tile_flags.txt), a name and maybe animation frames
#define KEA_TILE_NAME_LENGTH 32

struct kea_tile
{
    u64 BitmapGUID; // NOTE(pvlso): the tile, the first frame when it's animated
    u32 Flags;
    u32 Kind;       // NOTE(pvlso): 0 for none, else the n-th name in the project's tile_kinds.txt, from 1
    u16 GridX;
    u16 GridY;
    u32 FirstFrame; // NOTE(pvlso): into the set's FrameGUIDs
    u32 FrameCount; // NOTE(pvlso): 0 when it isn't animated
    r32 FramesPerSecond;
    char Name[KEA_TILE_NAME_LENGTH];
};

struct kea_tileset
{
    u32 TileCount;
    u16 GridWidth;
    u16 GridHeight;
    u32 FrameCount;
    u32 TileSize;

    /* NOTE(pvlso): Data is:

       kea_tile Tiles[TileCount];
       u64 FrameGUIDs[FrameCount];
    */
};

// NOTE(pvlso): A map: its tile sets by their asset GUID, its layers, and every layer's cells with the tile's index in
// its set (the builder looked the grid places up)
struct kea_map_layer
{
    char Name[32];
    s32 Z;
    u32 Flags; // NOTE(pvlso): KEAMapLayer_*
};
enum kea_map_layer_flag
{
    KEAMapLayer_Hidden = (1 << 0),
};

struct kea_map_cell
{
    u16 Set;  // NOTE(pvlso): 0 for an empty cell, else the map's set + 1
    u16 Tile; // NOTE(pvlso): the tile's index in its set
};

// NOTE(pvlso): An entity placed on the map: its own GUID, its definition's, its foot in cells from the map's top left
// (y down), and its overrides, Overrides[FirstOverride] on
struct kea_map_entity
{
    u64 GUID;
    u64 Definition;
    r32 P[2];
    u32 Flags; // NOTE(pvlso): KEAMapEntity_*
    u32 FirstOverride;
    u32 OverrideCount;
};
enum kea_map_entity_flag
{
    KEAMapEntity_Flip = (1 << 0),
};

struct kea_map
{
    u32 Width;
    u32 Height;
    u16 LayerCount;
    u16 SetCount;
    u32 EntityCount;
    u32 OverrideCount;

    /* NOTE(pvlso): Data is:

       u64 SetGUIDs[SetCount];
       kea_map_entity Entities[EntityCount];
       kea_map_override Overrides[OverrideCount];
       kea_map_layer Layers[LayerCount];
       kea_map_cell Cells[LayerCount][Height][Width];
    */
};

// NOTE(pvlso): An entity definition (engine_entity_file.h), the project's names written out so a game needs no data
// file: its clips that have a look (by name, the look as its stored asset's GUID), the blocks it uses and their
// fields. Its asset GUID is the definition's own.
#define KEA_ENTITY_NAME_LENGTH 32
#define KEA_ENTITY_DISPLAY_LENGTH 64
#define KEA_ENTITY_TEXT_LENGTH 128

struct kea_entity_shape
{
    u32 Shape; // NOTE(pvlso): entity_collision_shape: none, circle, box
    r32 Center[2]; // NOTE(pvlso): from the foot, pixels, y up
    r32 Size[2];   // NOTE(pvlso): a box's width and height, a circle's radius in [0]
};

struct kea_entity_clip
{
    char Name[KEA_ENTITY_NAME_LENGTH];
    u64 LookGUID; // NOTE(pvlso): a bitmap's or sprite sheet's asset
    r32 FramesPerSecond;
    u32 Loop;
};

struct kea_entity_block
{
    char Name[KEA_ENTITY_NAME_LENGTH];
    u32 FirstField;
    u32 FieldCount;
};

struct kea_entity_field
{
    char Name[KEA_ENTITY_NAME_LENGTH];
    u32 Kind; // NOTE(pvlso): entity_field_kind
    s32 Int;
    r32 Number;
    b32 Bool;
    u64 Entity;
    char Text[KEA_ENTITY_TEXT_LENGTH]; // NOTE(pvlso): a text, or a choice's word
};

struct kea_entity
{
    char Name[KEA_ENTITY_NAME_LENGTH]; // NOTE(pvlso): its file's, without .txt, so it's found without loading it
    u16 ClipCount;
    u16 BlockCount;
    u16 FieldCount;
    u16 Flags;
    kea_entity_shape Footprint;
    kea_entity_shape HitArea;

    /* NOTE(pvlso): Data is:

       char DisplayName[KEA_ENTITY_DISPLAY_LENGTH];
       char Category[KEA_ENTITY_NAME_LENGTH];
       kea_entity_clip Clips[ClipCount];
       kea_entity_block Blocks[BlockCount];
       kea_entity_field Fields[FieldCount];
    */
};

// NOTE(pvlso): A map's entity starting with another value for one field of its definition's blocks
struct kea_map_override
{
    char Block[KEA_ENTITY_NAME_LENGTH];
    kea_entity_field Field;
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

    u32 FirstTagIndex;
    u32 OnePastLastTagIndex;

    u32 Type;
    union
    {
        kea_bitmap Bitmap;
        kea_sound Sound;
        kea_tileset Tileset;
        kea_spritesheet SpriteSheet;
        kea_text Text;
        kea_binary_file BinaryFile;
        kea_map Map;
        kea_entity Entity;
    };
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// NOTE(pvlso): KET
// -----------------------------------------------------------------------------------------------------------------------------------------------------------
#define KET_MAGIC_VALUE ENCODE_MAGIC('k', 'e', 't', 'f')
#define KET_TAG_KEY_LENGTH 64

struct kea_tag_map
{
    u64 GUID; // NOTE(pvlso): made of Key
    char Key[TAG_KEY_LENGTH];

    u32 ValueCount;
    char **Values;
    u64 *ValueGUIDs; // NOTE(pvlso): Made of TagMap->Key + TagMap->Values[I]
};

struct ket_header
{
    u32 MagicValue;
    u32 Version;
    
    u32 TagCount;

    u64 TagArrayOffset;
};

struct ket_tag
{
    u64 GUID; // NOTE(pvlso): made of Key
    char Key[KET_TAG_KEY_LENGTH];

    u32 ValueCount;

    u64 DataOffset;
    // NOTE(pvlso): DATA
    // u64 ValueGUIDs[ValueCount];
    // char Values[ValueCount][KET_TAG_KEY_LENGTH];
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
    KESA_File,
    KESA_SSWM,
    KESA_Map,

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

// NOTE(pvlso): The set itself is its text file (SourceFileName in data/tilesets), this only remembers what it held
// when it was added or last saved. Kept at the size the old tile set had, so stored assets written before still read.
struct kesa_tileset
{
    u32 TileCount;
    u32 GridWidth;
    u32 GridHeight;
    u32 TileSize;

    u8 Unused[1280];
};
static_assert(sizeof(kesa_tileset) == 1296, "kesa_asset's size would change, and older stored assets stop reading");

struct kesa_sound
{
    u32 FirstSampleIndex;
    u32 Chain;
};

struct kesa_text
{
    u32 PLACEHOLDER;
};

struct kesa_binary_file
{
    u32 FileSize;
};

struct kesa_sswm_file
{
    u32 FileSize;
};

// NOTE(pvlso): The map is its text file (SourceFileName in data/maps); this remembers what it held when it was added
struct kesa_map
{
    u32 Width;
    u32 Height;
    u32 LayerCount;
    u32 SetCount;
};

struct kesa_tag
{
    u64 TagGUID;
    u32 TagValueIndex;
};

struct kesa_asset
{
    // NOTE(pvlso): made of SourceFileName
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
        kesa_binary_file File;
        kesa_sswm_file SSWM;
        kesa_map Map;
    };
};

// -----------------------------------------------------------------------------------------------------------------------------------------------------------
// ...........................................................................................................................................................
// -----------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma pack(pop)

#define ENGINE_FILE_FORMATS__H
#endif

#if !defined(ENGINE_ASSET_NEW_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */
#define ENCODE_MAGIC(a, b, c, d) (((u32)(a) << 0) | ((u32)(b) << 8) | ((u32)(c) << 16) | ((u32)(d) << 24))

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

#pragma pack(push, 1)

struct kea_asset_map
{
    u64 GUID;
    char Key[128];
};

struct kea_tag_map
{
    u64 GUID;
    char Key[128];
    u32 ValueCount;
    char Values[32][128];
};

struct kea_header
{
#define KEA_MAGIC_VALUE ENCODE_MAGIC('k', 'e', 'a', 'f')
    u32 MagicValue;
#define KEA_VERSION 0
    u32 Version;

    u32 AssetCount;
    u32 TagCount;
    kea_asset_map AssetMap;
    kea_tag_map TagMap;

    u64 AssetMapOffset;
    u64 AssetsOffset;

    u64 TagMapOffset;
    u64 TagsOffset;
};

struct kea_tag
{
    u64 GUID;
    char Key[128];
    char Value[128];
};

enum kea_sound_chain
{
    KEASoundChain_None,
    KEASoundChain_Loop,
    KEASoundChain_Advance,
    KEASoundChain_Count,
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

struct kea_sswm_file
{
    u32 Size;
    /* NOTE(paul): Data is:
       u8 Data[Size];
    */
};

struct kea_asset
{
    u64 DataOffset;

    u32 FirstTagIndex;
    u32 OnePastLastTagIndex;
    union
    {
        kea_bitmap Bitmap;
        kea_sound Sound;
        kea_font Font;
        kea_tileset Tileset;
        kea_spritesheet SpriteSheet;
        kea_text Text;
        kea_binary_file BinaryFile;
        kea_sswm_file SSWMFile;
    };
};

#pragma pack(pop)

#define ENGINE_ASSET_NEW_H
#endif

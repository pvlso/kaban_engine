#if !defined(EDITOR_SSA_FILE_BUILDER_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

enum builder_asset_type
{
    BuilderAssetType_Sound,
    BuilderAssetType_Bitmap,
    BuilderAssetType_Font,
    BuilderAssetType_FontGlyph,
    BuilderAssetType_Tile,
    BuilderAssetType_Tileset,
    BuilderAssetType_SpriteSheet,
    BuilderAssetType_Sprite,
    BuilderAssetType_Text,
    BuilderAssetType_BinaryFile,
    BuilderAssetType_SSWM,
};

struct builder_loaded_tileset
{
    ssa_tile *Tiles;
    loaded_bitmap TilesetBitmap;
    loaded_bitmap MergeTile;
    stored_asset_tileset *StoredTileset;
};

struct builder_loaded_spritesheet
{
    bitmap_id *SpriteIDs;
    loaded_bitmap *Sprites;
    stored_asset_spritesheet *StoredSheet;
};

struct builder_asset_source_font
{
    ssa_font_glyph *Glyphs;
    builder_loaded_font *Font;
};

struct builder_asset_source_font_glyph
{
    u32 CodePoint;
    loaded_bitmap Bitmap;
};

struct builder_asset_source_sound
{
    stored_asset_sound *Sound;
};

struct builder_asset_source_bitmap
{
    stored_asset_bitmap *Bitmap;
};

struct builder_asset_source_tile
{
    u32 TileIndex;
    builder_loaded_tileset *Tileset;
};

struct builder_asset_source_tileset
{
    builder_loaded_tileset *Tileset;
};

struct builder_asset_source_sprite
{
    loaded_bitmap Bitmap;
};

struct builder_asset_source_spritesheet
{
    builder_loaded_spritesheet *Sheet;
};

struct builder_asset_source_text
{
    stored_asset_text *Text;
};

struct builder_asset_source_binary_file
{
    stored_asset_binary_file *File;
};

struct builder_asset_source_sswm_file
{
    stored_asset_sswm_file *File;
};

struct builder_asset_source
{
    builder_asset_type Type;
    union
    {
        builder_asset_source_bitmap Bitmap;
        builder_asset_source_sound Sound;
        builder_asset_source_font Font;
        builder_asset_source_font_glyph Glyph;
        builder_asset_source_tile Tile;
        builder_asset_source_tileset Tileset;
        builder_asset_source_sprite Sprite;
        builder_asset_source_spritesheet SpriteSheet;
        builder_asset_source_text Text;
        builder_asset_source_binary_file File;
        builder_asset_source_sswm_file SSWM;
    };
};

#define VERY_LARGE_NUMBER 4096*32
struct builder_assets
{
    u32 TagCount;
    ssa_tag *Tags;

    u32 AssetTypeCount;
    ssa_asset_type AssetTypes[Asset_Count];

    u32 AssetCount;
    builder_asset_source *AssetSources;
    ssa_asset *Assets;

    u32 AssetIndex;

    u32 StoredAssetCounts[StoredAssetType_Count];
    u32 TypeAddingCount;
    ssa_asset_type *TypeAddingOrder[Asset_Count];
};

#define EDITOR_SSA_FILE_BUILDER_H
#endif

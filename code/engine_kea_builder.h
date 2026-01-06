#if !defined(ENGINE_KEA_BUILDER_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct builder_loaded_tiles
{
    u32 Count;
    loaded_bitmap *TileBitmaps;
};

struct builder_loaded_sprites
{
    u32 Count;
    loaded_bitmap *Sprites;
};

struct builder_added_spritesheet
{
    u64 *SpriteGUIDs;
};

struct builder_added_tileset
{
    u64 *TileGUIDs;
};

struct builder_added_tile
{
    loaded_bitmap *TileBitmap;
};

struct builder_added_sprite
{
    loaded_bitmap *SpriteBitmap;
};

union added_asset_additional_data
{
    builder_added_spritesheet SpriteSheet;
    builder_added_tileset Tileset;
    builder_added_tile Tile; 
    builder_added_sprite Sprite; 
};

struct kea_builder_added_asset_list
{
    kea_asset KEAAsset;
    kesa_asset *StoredAsset;

    added_asset_additional_data Data;

    kea_builder_added_asset_list *Next;    
};

struct kea_builder_tag_list
{
    u32 TagIndex;
    kea_tag Tag;

    kea_builder_tag_list *Next;
};

struct kea_builder_tag_lookup_entry_list
{
    kea_tag_lookup_entry Entry;
    u32 IndexOffset;
    kea_builder_tag_lookup_entry_list *Next;
};

struct kea_builder
{
    memory_arena *TempMem;

    char Buffer[1024];
    
    kesa_header *KESAHeader;
    kesa_asset *KESAAssets; // NOTE(pvlso): Sorted array of stored assets

    ket_header *KETHeader;
    kea_tag_map *TagMaps;
    
    u32 TagCount;
    kea_builder_tag_list *Tags;
    kea_builder_tag_list *CurrentTag;
    kea_tag *UsedTags;

    kea_asset_type_table_entry *AssetTypeTable;
    u32 **AssetTypeTableData;

    u32 EntryCount;
    kea_builder_tag_lookup_entry_list TagTable;    
    kea_tag_hash_table TagHashTable;
    kea_tag_lookup_entry *TableData;

    u32 TagAssetsIndeciesCount;
    u32 *TagAssetsIndecies;
    
    u32 AssetCount;
    kea_builder_added_asset_list *BuilderAssets;
    kea_builder_added_asset_list **SortedBuilderAssets;
    kea_builder_added_asset_list *CurrentAsset;
};

#define ENGINE_KEA_BUILDER_H
#endif

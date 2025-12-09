#if !defined(ENGINE_KEA_BUILDER_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct builder_added_spritesheet
{
    u64 *SpriteGUIDs;
};

struct builder_added_tileset
{
    u64 *TileGUIDs;
};

union added_asset_additional_data
{
    u64 PlaceHolder;
    builder_added_spritesheet SpriteSheet;
    builder_added_tileset Tileset;
};

struct kea_builder_added_asset_list
{
    kea_asset KEAAsset;
    kesa_asset *StoredAsset;

    kea_builder_added_asset_list *Next;    

    added_asset_additional_data Data;
};

struct kea_builder_tag_list
{
    u32 TagIndex;
    kea_tag Tag;

    kea_builder_tag_list *Next;
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

    u32 AssetCount;
    kea_builder_added_asset_list *BuilderAssets;
    kea_builder_added_asset_list *CurrentAsset;
};

#define ENGINE_KEA_BUILDER_H
#endif

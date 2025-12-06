#if !defined(ENGINE_KEA_BUILDER_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

struct kea_builder
{
    kesa_header *KESAHeader;
    kesa_asset *KESAAssets;

    ket_header *KETHeader;
    kea_tag_map *TagMaps;

    kea_header KEAHeader;
    kea_tag *UsedTags;
    kea_tag_table_entry *TagTable;

    kea_asset_type_table_entry *AssetTypeTable;

    kea_asset *KEAAssets;
};

#define ENGINE_KEA_BUILDER_H
#endif

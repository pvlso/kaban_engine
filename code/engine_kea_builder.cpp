/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
#include "engine_kea_builder.h"

internal kea_builder *
InitKEABuilder(editor_mode_assets *AssetsMode, memory_arena *TempMem)
{
    kea_builder *KEABuilder = PushStruct(TempMem, kea_builder);

    KEABuilder->KESAHeader = &AssetsMode->StoredHeader;
    KEABuilder->KESAAssets = AssetsMode->StoredAssets;
    KEABuilder->KETHeader = &AssetsMode->TagHeader;
    KEABuilder->TagMaps = AssetsMode->Tags;

    return(KEABuilder);
}


struct kesa_tag_list
{
    kesa_tag Tag;
    kesa_tag_list *Next;
};

internal kea_tag *
GetUsedTags(kea_builder *KEABuilder, memory_arena *TempMem)
{
    kea_tag *Result = 0;
    // NOTE(pvlso): Count how many unique tags are used
    u32 ListCount = 1;
    kesa_tag_list *Head = PushStruct(TempMem, kesa_tag_list);
    kesa_tag_list *Current = Head;
    for(u32 I = 0;
        I < KEABuilder->KESAHeader->AssetCount;
        ++I)
    {
        kesa_asset *KESAAsset = KEABuilder->KESAAssets + I;
        for(u32 J = 0;
            J < KESAAsset->TagCount;
            ++J)
        {
            kesa_tag *Tag = KESAAsset->AssetTags + J;
            b32 IsNew = true;
            for(kesa_tag_list *Search = Head;
                Search;
                Search = Search->Next)
            {
                if(Tag->TagGUID == Search->Tag.TagGUID)
                {
                    IsNew = false;
                    break;
                }
            }

            if(IsNew)
            {
                Current->Tag = *Tag;
                Current->Next = PushStruct(TempMem, kesa_tag_list);;
                Current = Current->Next;
                ++ListCount;
            }
        }
    }

    return(Result);
}

internal void
PrepareKEABuilder(kea_builder *KEABuilder, memory_arena *TempMem)
{
    kea_tag *UsedTags = GetUsedTags(KEABuilder, TempMem);
}


internal b32
BuildKEA(editor_mode_assets *AssetsMode, memory_arena *TempMem)
{
    b32 Result = true;

    kea_builder *KEABuilder = InitKEABuilder(AssetsMode, TempMem);
    PrepareKEABuilder(KEABuilder, TempMem);

    return(Result);
}

#if !defined(EDITOR_FILE_FORMATS_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

/*
  NOTE(paul): IMPORTANT:

  -The enum element name must be as following Name_Value, Name_Value0 ...

  -The enum must have the first element to be Name_None and the last element to be Name_Count
  
  -The elements of enums before its count that already exist must not be moved or deleted

  -The new element must be added before the enum's count but after all existing elements

  -The elements that are not used must be replaced with as following Name_UNUSED0, Name_UNUSED1 and so on

*/

/*
  NOTE(paul): Current Formats

  - Spellweaver Saga Assets (SSA)

  - Spellweaver Saga World Map (SSWM)

  - Spellweaver Saga Editor Version (SSEV)

  - Spellweaver Saga Editor Assets (SSEA)

*/

enum asset_font_type
{
    FontType_None,

    FontType_Default,
    FontType_Nice,
    FontType_Debug,
    FontType_DebugSmall,

    FontType_Count,
};

enum biome_type
{
    BiomeType_None,

    BiomeType_AncientForest,
    BiomeType_DeepCave,
    BiomeType_DesertV1,
    BiomeType_DesertV2,
    BiomeType_GrassLand,
    BiomeType_MarshLandV1,
    BiomeType_MarshLandV2,
    BiomeType_Mountains,
    BiomeType_Strangeland,
    BiomeType_Wetland,

    BiomeType_Count,
};

enum tile_surface
{
    TileSurface_None,

    TileSurface_Grass0,
    TileSurface_Grass1,
    TileSurface_Grass2,
    TileSurface_Grass3,

    TileSurface_Ground0,
    TileSurface_Ground1,
    TileSurface_Ground2,
    TileSurface_Ground3,

    TileSurface_Water,

    TileSurface_WaterCliffBottom,
    TileSurface_CliffTop,
    TileSurface_CliffMiddle,
    TileSurface_WaterCliffTop0,
    TileSurface_WaterCliffTop1,

    TileSurface_CastleTopTopBarier,
    TileSurface_CastleTopFloor,
    TileSurface_CastleTopBottomBarier,
    TileSurface_CastleWallLev3,
    TileSurface_CastleWallLev2,
    TileSurface_CastleBottomBarier,
    TileSurface_StoneFance,
    TileSurface_CliffLev3Bottom,

    TileSurface_CliffLev3TopCustleTopBarier,
    TileSurface_CastleSmallTowerLeftSide,
    TileSurface_CastleBigTowerRightSide,
    TileSurface_CastleBigTowerLeftSide,
    TileSurface_CastleLeftBarier,
    TileSurface_CastleRightBarier,

    TileSurface_WaterCliffType2_0,
    TileSurface_WaterCliffType2_1,
    TileSurface_WaterCliffType2_2,
    TileSurface_WaterCliffType2_3,
    TileSurface_WaterCliffType2_4,
    
    TileSurface_Count,
};

enum tile_type
{
    TileType_None,

    TileType_Surface,
    TileType_Hill,
    TileType_Cliff,
    TileType_WaterCliff,
    TileType_Wall,
    TileType_WaterBridge,
    TileType_StoneFance,
    TileType_WoodenFance,
    TileType_CastleWalls,
    TileType_CastleTower,
    
    TileType_Count,
};

enum height
{
    Height_None,
    
    Height_Lev0,
    Height_Lev1,
    Height_Lev2,
    Height_Lev3,

    Height_Count,
};

enum cliff_hill_type
{
    CliffHillType_None,

    CliffHillType_Climb,
    CliffHillType_External,
    CliffHillType_Internal,

    CliffHillType_Count,
};

enum animation_type
{
    AnimationType_None,

    AnimationType_Idle,
    AnimationType_IdleWithWeapon,
    AnimationType_Move,
    AnimationType_MoveWithWeapon,
    AnimationType_Attack0,
    AnimationType_Attack1,
    AnimationType_Attack2,
    AnimationType_Attack3,
    AnimationType_Death,
    AnimationType_CastSpell0,
    AnimationType_CastSpell1,
    AnimationType_CastSpell2,
    AnimationType_CastSpell3,

    AnimationType_Count,
};

enum tree_type
{
    TreeType_None,

    TreeType_Leaves,
    TreeType_Fallen,
    TreeType_Mashroom,
    TreeType_Leaveless,
    TreeType_Giant,

    TreeType_Count,
};

enum light_level
{
    LightLevel_None,

    LightLevel_0,
    LightLevel_1,
    LightLevel_2,
    LightLevel_3,
    LightLevel_4,

    LightLevel_Count,
};

enum size_level
{
    SizeLevel_None,

    SizeLevel_0,
    SizeLevel_1,
    SizeLevel_2,
    SizeLevel_3,
    SizeLevel_4,
    SizeLevel_5,
    SizeLevel_6,
    SizeLevel_7,
    SizeLevel_8,
    SizeLevel_9,
    SizeLevel_10,
    SizeLevel_11,
    SizeLevel_12,
    SizeLevel_13,
    SizeLevel_14,
    SizeLevel_15,
    SizeLevel_16,
    SizeLevel_17,

    SizeLevel_Count,
};

enum variety_type
{
    VarietyType_None,

    VarietyType_0,
    VarietyType_1,
    VarietyType_2,
    VarietyType_3,
    VarietyType_4,
    VarietyType_5,
    VarietyType_6,
    VarietyType_7,
    VarietyType_8,
    VarietyType_9,
    VarietyType_10,
    VarietyType_11,
    VarietyType_12,
    VarietyType_13,
    VarietyType_14,
    VarietyType_15,
    VarietyType_16,
    VarietyType_17,
    VarietyType_18,
    VarietyType_19,
    VarietyType_20,
    VarietyType_21,
    VarietyType_22,
    VarietyType_23,
    VarietyType_24,
    VarietyType_25,
    VarietyType_26,
    VarietyType_27,
    VarietyType_28,
    VarietyType_29,
    VarietyType_30,
    VarietyType_31,
    VarietyType_32,
    VarietyType_33,
    VarietyType_34,
    VarietyType_35,
    VarietyType_36,
    VarietyType_37,
    VarietyType_38,
    VarietyType_39,
    VarietyType_40,
    VarietyType_41,
    VarietyType_42,
    VarietyType_43,
    VarietyType_44,
    VarietyType_45,
    VarietyType_46,
    VarietyType_47,
    VarietyType_48,
    VarietyType_49,
    VarietyType_50,

    VarietyType_Count,
};

enum magic_element
{
    MagicElement_None,

    MagicElement_Water,
    MagicElement_Wind,
    MagicElement_Fire,
    MagicElement_Dark,
    MagicElement_Ice,
    MagicElement_Light,
    MagicElement_Energy,

    MagicElement_Count,
};

enum color
{
    Color_None,

    Color_Red,
    Color_Pink,
    Color_Yellow,
    Color_Black,
    Color_DarkBlack,
    Color_LightBlack,
    Color_Brown,
    Color_DarkBrown,
    Color_LightBrown,
    Color_Blond,
    Color_Gray,
    Color_Green,
    Color_DarkPink,
    Color_Blue,
    Color_Magenta,
    Color_Orange,
    Color_White,

    Color_Count,
};

enum sex
{
    Sex_None,

    Sex_Male,
    Sex_Female,

    Sex_Count,
};

enum age
{
    Age_None,

    Age_Kid,
    Age_Teen,
    Age_Young,
    Age_Parent,
    Age_Old,

    Age_Count,
};

enum beard
{
    Beard_None,

    Beard_Thick,
    Beard_Normal,

    Beard_Count,
};

enum haircut
{
    Haircut_None,

    Haircut_Ponytail,
    Haircut_Bald,
    Haircut_Tuft,

    Haircut_Count,
};

enum accessories
{
    Accessories_None,

    Accessories_Glasses,
    Accessories_Hat,
    Accessories_HatAndGlasses,
    Accessories_Bow,
    Accessories_HairPins,

    Accessories_Count,
};

enum top_outfit
{
    TopOutfit_None,

    TopOutfit_Jacket,
    TopOutfit_JacketNoSleeves,
    TopOutfit_Coat,
    TopOutfit_CoatNoSleeves,
    TopOutfit_TShort,
    TopOutfit_Drass,

    TopOutfit_Count,
};

enum bottom_outfit
{
    BottomOutfit_None,

    BottomOutfit_Pans,
    BottomOutfit_Skirt,

    BottomOutfit_Count,
};

enum quest_type
{
    QuestType_None,

    QuestType_Main,
    QuestType_Side,
    QuestType_General,

    QuestType_Count,
};

enum npc_name
{
    NPCName_None,

    NPCName_ElderTavor,
    NPCName_Elara,
    NPCName_Jacob,

    NPCName_Count,
};

enum quest_name
{
    QuestName_None,

    QuestName_FindTavor,
    QuestName_TheLostTome,
    QuestName_FindHerbalist,
    QuestName_HerbalistsPlea,
    QuestName_FindJacob,
    QuestName_JacobTalk,
    QuestName_SkeletonKing,
    QuestName_HidenMap,
    QuestName_BrokenBridge,

    QuestName_Count,
};

enum spell_name
{
    Spell_None,

    Spell_FireBall,
    Spell_DarkBolt,
    Spell_MagicSword,
    Spell_WaterBall,
    Spell_IceBall,
    Spell_HealCross,
    Spell_LightBall,
    Spell_IceSword,
    Spell_FireSword,
    Spell_EnergyBall,
    Spell_BirdStrike,

    Spell_Count,
};

enum magic_effect
{
    MagicEffect_None,

    MagicEffect_FireExplosion,
    MagicEffect_IceExplosion,
    MagicEffect_Thunder,
    MagicEffect_Water,
    MagicEffect_Charge,
    MagicEffect_Heal,

    MagicEffect_Count,
};

enum item_name
{
    ItemName_None,

    ItemName_MapToTheTrees,
    ItemName_HealPotion,
    ItemName_Amulet,
    ItemName_Relic,

    ItemName_Count,
};

enum file_data_type
{
    FileData_None,

    FileData_Decorations,
    FileData_Collisions,
    FileData_Tiles,

    FileData_Count,
};

enum sound_effect_type
{
    SoundEffect_None,

    SoundEffect_Walk,
    SoundEffect_BeastPossesedAttack,
    SoundEffect_BowAttack,
    SoundEffect_BowImpact,
    SoundEffect_FireBallCast,
    SoundEffect_FireBallImpact,
    SoundEffect_HealCast,
    SoundEffect_HealPickUp,
    SoundEffect_SpellCast,
    SoundEffect_HeroCastSpellDefault,
    SoundEffect_Hit,
    SoundEffect_IceImpact,
    SoundEffect_IceBallCast,
    SoundEffect_NecromancerSummons,
    SoundEffect_SkeletonKingHeal,
    SoundEffect_SwordAttack,
    SoundEffect_SwordImpact,
    SoundEffect_WaterImpact,
    SoundEffect_EnergyImpact,
    SoundEffect_ThunderImpact,
    SoundEffect_Whoosh,
    SoundEffect_SmallMonsterAttack,
    SoundEffect_ItemPickUp,
    SoundEffect_Click,

    SoundEffect_Death,
    SoundEffect_Idle,
    SoundEffect_Attack,

    SoundEffect_Count,
};

enum music_type
{
    MusicType_None,

    MusicType_Forest,
    MusicType_Village,
    MusicType_WaterStream,
    MusicType_Ambient,
    MusicType_DarkAmbient,
    MusicType_Action,
    MusicType_GameStartFX,
    MusicType_GameEndDeathFX,
    MusicType_GameVictoryMusic,

    MusicType_Count,
};

enum prop_type
{
    PropType_None,

    PropType_Grave,
    PropType_Box,
    PropType_BrokenBox,
    PropType_Barel,
    PropType_Mix,
    PropType_Bag,
    PropType_Well,
    PropType_ChoppingBole,
    PropType_Bags,
    PropType_Log,
    PropType_Logs,
    PropType_Package,
    PropType_SmallBox,
    PropType_SmallBoxes,

    PropType_Count,
};

enum value
{
    Value_0,
    Value_1,
    Value_2,
    Value_3,
    Value_4,
    Value_5,
    Value_6,
    Value_7,
    Value_8,
    Value_9,
    Value_10,
    Value_11,
    Value_12,
    Value_13,
    Value_14,
    Value_15,
    Value_16,
    Value_17,
    Value_18,
    Value_19,
    Value_20,
    Value_21,
    Value_22,
    Value_23,
    Value_24,
    Value_25,
    Value_26,
    Value_27,
    Value_28,
    Value_29,
    Value_30,
    Value_31,
    Value_32,
    Value_33,
    Value_34,
    Value_35,
    Value_36,
    Value_37,
    Value_38,
    Value_39,
    Value_40,
    Value_41,
    Value_42,
    Value_43,
    Value_44,
    Value_45,
    Value_46,
    Value_47,
    Value_48,
    Value_49,
    Value_50,
    Value_51,
    Value_52,
    Value_53,
    Value_54,
    Value_55,
    Value_56,
    Value_57,
    Value_58,
    Value_59,
    Value_60,
    Value_61,
    Value_62,
    Value_63,
    Value_64,

    Value_Count,
};

enum asset_tag_id
{
    Tag_FacingDirection, // NOTE(casey): Angle in radians off of due right
    Tag_SpriteIndex,
    Tag_AnimationType,
    Tag_AssetType,

    Tag_UnicodeCodepoint,
    Tag_FontType,

    Tag_TileID,
    Tag_BiomeType,
    Tag_TileType,
    Tag_Height,
    Tag_CliffHillType,
    Tag_TileMainSurface,
    Tag_TileMergeSurface,

    Tag_TreeType,
    Tag_LightLevel,
    Tag_SizeLevel,
    Tag_Color,

    Tag_Variety,

    Tag_MagicElement,

    Tag_Sex,
    Tag_Age,
    Tag_HairColor,
    Tag_Beard,
    Tag_Accessories,
    Tag_TopOutfit,
    Tag_TopOutfitColor,
    Tag_BottomOutFit,
    Tag_BottomOutFitColor,

    Tag_NPCName,
    Tag_QuestType,
    Tag_QuestName,

    Tag_Haircut,

    Tag_SpellName,
    Tag_MagicEffect,

    Tag_ItemName,
    Tag_DataType,

    Tag_MusicType,
    Tag_SoundEffectType,
    Tag_PropType,

    Tag_UNUSED0,
    Tag_UNUSED1,

    Tag_TileChecksum,

    Tag_VersionMajorHigh,
    Tag_VersionMajorLow,
    Tag_VersionMinorHigh,
    Tag_VersionMinorLow,
    
    Tag_Count,
};

enum asset_type_id
{
    Asset_None,

    //
    // NOTE(casey): Bitmaps!
    //

    Asset_FontGlyph,
    Asset_Font,

    Asset_Tile,
    Asset_Tileset,

    Asset_Hero,
    Asset_Golem,
    Asset_Cultist,
    
    Asset_Sprite,
    Asset_SpriteSheet,

    Asset_UNUSED0,

    Asset_Bole,
    Asset_Bush,
    Asset_MixDecor,
    Asset_Reed,
    Asset_StoneReed,
    Asset_Log,
    Asset_StoneHerb,
    Asset_Herb,
    Asset_Tree,

    Asset_House,

    Asset_Sprout,
    Asset_SproutReed,
    Asset_WaterEffect,

    
    //
    // NOTE(casey): Sounds!
    //

    Asset_Bloop,
    Asset_Crack,
    Asset_Drop,
    Asset_Glide,
    Asset_Music,
    Asset_Puhp,

    //
    //
    //
    
    Asset_TitleImage,
    Asset_CursorHover,
    Asset_CursorClick,
    Asset_HeroHealthBar,
    Asset_SpellBar,

    Asset_MagicSphere,

    Asset_NPCCharecter,
    Asset_Text,

    Asset_DialogueBorder,

    Asset_Quest,

    Asset_WindMill,
    Asset_HerbSeed,
    Asset_Lighter,

    Asset_Necromancer,

    Asset_Spell,
    Asset_SpellEffect,

    Asset_Possesed,

    Asset_EffectSound,

    Asset_GoblinBeast,
    Asset_GoblinBerserker,
    Asset_GoblinRider,
    Asset_SkeletonWithSword,
    Asset_SkeletonWithBow,
    Asset_Arrow,
    Asset_SkeletonKing,

    Asset_Item,
    Asset_Stone,
    Asset_Obelisk,
    Asset_AncientMage,

    Asset_SphereBar,
    Asset_QuestBorder,
    Asset_BubbleEffect,
    Asset_BinaryFile,
    Asset_QuestMark,    

    Asset_Prop,    
    Asset_MouseLeftButton,    
    Asset_SSWM,    
    
    //
    //
    //
    
    Asset_Count
};

#define SSA_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
#define SSWM_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))

#pragma pack(push, 1)

struct sswm_header
{
#define SSWM_MAGIC_VALUE SSWM_CODE('s', 's', 'w', 'm')
    u32 MagicValue;
    u32 Version;
    
    char Name[256];

    u32 MapWidth;
    u32 MapHeight;
    u32 GroundLayer_ZLayerCount;
    
    u32 EntityCount;

    u64 Tiles;
    u64 Entities;
};

struct sswm_ground_tile
{
    u32 BitmapID[16];
    u32 TileX;
    u32 TileY;

    u32 CheckSum[16];
};

struct sswm_entity
{
    u32 Type;
};

struct sswm_id
{
    u32 Value;
};

struct spritesheet_id
{
    u32 Value;
};

struct tileset_id
{
    u32 Value;
};

struct bitmap_id
{
    u32 Value;
};

struct sound_id
{
    u32 Value;
};

struct font_id
{
    u32 Value;
};

struct text_id
{
    u32 Value;
};

struct file_id
{
    u32 Value;
};

struct ssa_header
{
#define SSA_MAGIC_VALUE SSA_CODE('s', 's', 'a', 'f')
    u32 MagicValue;
#define SSA_VERSION 0
    u32 Version;

    u32 TagCount;
    u32 AssetTypeCount;
    u32 AssetCount;

    u64 Tags; // ssa_tag[TagCount]
    u64 AssetTypes; // ssa_asset_type[AssetTypeCount]
    u64 Assets; // ssa_asset[AssetCount]
};

struct ssa_tag
{
    u32 ID;
    u32 Value;
};

struct ssa_asset_type
{
    u32 TypeID;
    u32 FirstAssetIndex;
    u32 OnePastLastAssetIndex;
};

enum ssa_sound_chain
{
    SSASoundChain_None,
    SSASoundChain_Loop,
    SSASoundChain_Advance,
    SSASoundChain_Count,
};

struct ssa_bitmap
{
    u32 Dim[2];
    r32 AlignPercentage[2];

    /* NOTE(casey): Data is:

       u32 Pixels[Dim[1]][Dim[0]]
    */
};

struct ssa_sound
{
    u32 SampleCount;
    u32 ChannelCount;
    u32 Chain; // NOTE(casey): ssa_sound_chain

    /* NOTE(casey): Data is:

       s16 Channels[ChannelCount][SampleCount]
    */
};

struct ssa_font_glyph
{
    u32 UnicodeCodePoint;
    bitmap_id BitmapID;
};

struct ssa_font
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

struct ssa_tile
{
    bitmap_id BitmapID;
    u32 CheckSum;
};

struct ssa_tileset
{
    u32 TileCount;

    /* NOTE(paul): Data is:

       ssa_tile Tiles[TileCount];
    */
};

struct ssa_spritesheet
{
    u32 SpriteCount;

    /* NOTE(paul): Data is:

       bitmap_id Sprites[SpriteCount];
    */
};

struct ssa_text
{
    u32 Length;

    /* NOTE(paul): Data is:

       u8 Symbols[Length];
    */
};

struct ssa_binary_file
{
    u32 Size;
    /* NOTE(paul): Data is:
       u8 Data[Size];
    */
};

struct ssa_sswm_file
{
    u32 Size;
    /* NOTE(paul): Data is:
       u8 Data[Size];
    */
};

struct ssa_asset
{
    u64 DataOffset;

    u32 FirstTagIndex;
    u32 OnePastLastTagIndex;
    union
    {
        ssa_bitmap Bitmap;
        ssa_sound Sound;
        ssa_font Font;
        ssa_tileset Tileset;
        ssa_spritesheet SpriteSheet;
        ssa_text Text;
        ssa_binary_file BinaryFile;
        ssa_sswm_file SSWMFile;
    };
};

#pragma pack(pop)

#define EDITOR_FILE_FORMATS_H
#endif

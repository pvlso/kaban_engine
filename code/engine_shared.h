#if !defined(ENGINE_SHARED_H)
/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice: $
   ======================================================================== */

#include "engine_intrinsics.h"
#include "engine_math.h"
#include "engine_random.h"
#include "engine_memory.h"
#include "engine_string.h"
#include "engine_crc.h"

inline u32
SortKeyToU32(r32 SortKey)
{
    // NOTE(casey): We need to turn our 32-bit floating point value
    // into some strictly ascending 32-bit unsigned integer value
    u32 Result = *(u32 *)&SortKey;
    if(Result & 0x80000000)
    {
        Result = ~Result;
    }
    else
    {
        Result |= 0x80000000;
    }

    return(Result);
}

global_variable v3 DebugColorTable[] =
{
    {1, 0, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 1, 0},
    {0, 1, 1},
    {1, 0, 1},
    {1, 0.5f, 0},
    {1, 0, 0.5f},
    {0.5f, 1, 0},
    {0, 1, 0.5f},
    {0.5f, 0, 1},
    {0.1028, 0.5312, 0.8078},
    {0.3125, 0.8844, 0.4784},
    {0.1868, 0.2624, 0.8449},
    {0.0626, 0.0854, 0.8665},
    {0.6298, 0.8287, 0.8049},
    {0.5582, 0.7462, 0.5137},
    {0.7372, 0.5665, 0.9878},
    {0.0481, 0.5489, 0.9372},
    {0.1656, 0.0822, 0.0605},
    {0.0562, 0.0390, 0.9506},
    {0.4321, 0.2841, 0.9994},
    {0.0752, 0.9016, 0.8405},
    {0.5898, 0.9335, 0.0619},
    {0.3688, 0.4160, 0.9041},
    {0.4484, 0.9473, 0.1858},
    {0.5330, 0.6409, 0.0581},
    {0.1431, 0.1299, 0.2374},
    {0.2019, 0.6299, 0.4140},
    {0.4713, 0.6883, 0.4025},
    {0.4912, 0.1313, 0.4535},
    {0.2124, 0.3513, 0.8837},
    {0.3189, 0.7460, 0.2089},
    {0.0168, 0.8861, 0.5419},
    {0.7720, 0.4520, 0.7699},
    {0.5791, 0.2399, 0.0992},
    {0.6426, 0.5293, 0.5107},
    {0.9323, 0.6897, 0.8025},
    {0.1079, 0.6979, 0.2374},
    {0.2686, 0.7170, 0.6474},
    {0.8829, 0.0909, 0.8297},
    {0.1476, 0.9972, 0.1183},
    {0.9105, 0.5648, 0.3042},
    {0.0007, 0.3076, 0.3010},
    {0.9104, 0.3937, 0.1161},
    {0.5510, 0.3267, 0.4397},
    {0.7701, 0.3489, 0.4813},
    {0.5665, 0.7097, 0.1812},
    {0.7207, 0.0840, 0.4588},
    {0.2051, 0.5209, 0.5612},
    {0.0493, 0.4915, 0.9108},
    {0.1411, 0.7480, 0.0155},
    {0.2527, 0.3339, 0.3942},
    {0.0710, 0.1963, 0.2849},
    {0.0127, 0.6260, 0.6980},
    {0.4880, 0.8594, 0.3048},
    {0.2869, 0.9362, 0.3384},
    {0.1113, 0.5194, 0.4928},
    {0.3103, 0.1108, 0.6696},
    {0.3163, 0.1742, 0.5541},
    {0.5243, 0.4920, 0.2905},
    {0.3973, 0.9598, 0.1572},
    {0.4384, 0.1351, 0.7984},
    {0.2959, 0.9694, 0.9492},
    {0.7462, 0.4598, 0.7370},
    {0.3733, 0.8101, 0.9447},
    {0.9680, 0.3013, 0.9917},
    {0.4595, 0.7455, 0.6151},
    {0.5945, 0.1787, 0.4894},
    {0.1900, 0.7216, 0.1632},
    {0.6748, 0.4077, 0.3198},
    {0.8792, 0.1834, 0.6540},
    {0.5933, 0.4781, 0.3027},
    {0.6502, 0.8242, 0.3608},
    {0.3904, 0.8362, 0.3996},
};

struct sort_entry
{
    r32 SortKey;
    u32 Index;

    union
    {
        v3 P;
    };
};

inline void
Swap(sort_entry *A, sort_entry *B)
{
    sort_entry Temp = *B;
    *B = *A;
    *A = Temp;
}

inline r32
CalculateBitmapScaleForSquareCanvas(r32 CanvasSize, u32 Width, u32 Height)
{
    r32 Result = 1.0f;
    if(Width > Height)
    {
        Result = CanvasSize / Width;
    }
    else
    {
        Result = CanvasSize / Height;
    }

    return(Result);
}

#define UI_COLOR_RGB1_3F3F3F V3(0.247058823529f, 0.247058823529f, 0.247058823529f)
#define UI_COLOR_RGB1_4D3020 V3(0.301960784314f, 0.188235294118f, 0.125490196078f)
#define UI_COLOR_RGB1_67412C V3(0.403921568627f, 0.254901960784f, 0.172549019608f)
#define UI_COLOR_RGB1_784A32 V3(0.470588235294f, 0.290196078431f, 0.196078431373f)
#define UI_COLOR_RGB1_7f7f7f V3(0.498039215686f, 0.498039215686f, 0.498039215686f)
#define UI_COLOR_RGB1_880015 V3(0.533333333333f, 0.0f,            0.082352941176f)
#define UI_COLOR_RGB1_ED1C24 V3(0.929411764706f, 0.109803921569f, 0.141176470588f)
#define UI_COLOR_RGB1_FF7F27 V3(1.0f,            0.498039215686f, 0.152941176471f)
#define UI_COLOR_RGB1_B97A57 V3(0.725490196078f, 0.478431372549f, 0.341176470588f)
#define UI_COLOR_RGB1_22B14C V3(0.133333333333f, 0.694117647059f, 0.298039215686f)
#define UI_COLOR_RGB1_B5E61D V3(0.709803921569f, 0.901960784314f, 0.113725490196f)
#define UI_COLOR_RGB1_3F48CC V3(0.247058823529f, 0.282352941176f, 0.8f           )
#define UI_COLOR_RGB1_7092BE V3(0.439215686275f, 0.572549019608f, 0.745098039216f)
#define UI_COLOR_RGB1_00A2E8 V3(0.0f,            0.635294117647f, 0.909803921569f)
#define UI_COLOR_RGB1_CB9C83 V3(0.796078431373f, 0.611764705882f, 0.513725490196f)
#define UI_COLOR_RGB1_F78C92 V3(0.96862745098f,  0.549019607843f, 0.572549019608f)
#define UI_COLOR_RGB1_DCBCAB V3(0.862745098039f, 0.737254901961f, 0.670588235294f)
#define UI_COLOR_RGB1_DDF398 V3(0.866666666667f, 0.952941176471f, 0.596078431373f)
#define UI_COLOR_RGB1_EFE4B0 V3(0.937254901961f, 0.894117647059f, 0.690196078431f)
#define UI_COLOR_RGB1_99D9EA V3(0.6f,            0.850980392157f, 0.917647058824f)
#define UI_COLOR_RGB1_C3C3C3 V3(0.764705882353f, 0.764705882353f, 0.764705882353f)
#define UI_COLOR_RGB1_CFEEF5 V3(0.811764705882f, 0.933333333333f, 0.960784313725f)
#define UI_COLOR_RGB1_E1E1E1 V3(0.882352941176f, 0.882352941176f, 0.882352941176f)
#define UI_COLOR_RGB1_FFFFFF V3(1.0f,            1.0f,            1.0f           )

#define UI_COLOR_RGBA1_3F3F3FFF V4(0.247058823529f, 0.247058823529f, 0.247058823529f, 1.0f)
#define UI_COLOR_RGBA1_4D3020FF V4(0.301960784314f, 0.188235294118f, 0.125490196078f, 1.0f)
#define UI_COLOR_RGBA1_67412CFF V4(0.403921568627f, 0.254901960784f, 0.172549019608f, 1.0f)
#define UI_COLOR_RGBA1_784A32FF V4(0.470588235294f, 0.290196078431f, 0.196078431373f, 1.0f)
#define UI_COLOR_RGBA1_7F7F7FFF V4(0.498039215686f, 0.498039215686f, 0.498039215686f, 1.0f)
#define UI_COLOR_RGBA1_880015FF V4(0.533333333333f, 0.0f,            0.082352941176f, 1.0f)
#define UI_COLOR_RGBA1_ED1C24FF V4(0.929411764706f, 0.109803921569f, 0.141176470588f, 1.0f)
#define UI_COLOR_RGBA1_FF7F27FF V4(1.0f,            0.498039215686f, 0.152941176471f, 1.0f)
#define UI_COLOR_RGBA1_B97A57FF V4(0.725490196078f, 0.478431372549f, 0.341176470588f, 1.0f)
#define UI_COLOR_RGBA1_22B14CFF V4(0.133333333333f, 0.694117647059f, 0.298039215686f, 1.0f)
#define UI_COLOR_RGBA1_B5E61DFF V4(0.709803921569f, 0.901960784314f, 0.113725490196f, 1.0f)
#define UI_COLOR_RGBA1_3F48CCFF V4(0.247058823529f, 0.282352941176f, 0.8f           , 1.0f)
#define UI_COLOR_RGBA1_7092BEFF V4(0.439215686275f, 0.572549019608f, 0.745098039216f, 1.0f)
#define UI_COLOR_RGBA1_00A2E8FF V4(0.0f,            0.635294117647f, 0.909803921569f, 1.0f)
#define UI_COLOR_RGBA1_CB9C83FF V4(0.796078431373f, 0.611764705882f, 0.513725490196f, 1.0f)
#define UI_COLOR_RGBA1_F78C92FF V4(0.96862745098f,  0.549019607843f, 0.572549019608f, 1.0f)
#define UI_COLOR_RGBA1_DCBCABFF V4(0.862745098039f, 0.737254901961f, 0.670588235294f, 1.0f)
#define UI_COLOR_RGBA1_DDF398FF V4(0.866666666667f, 0.952941176471f, 0.596078431373f, 1.0f)
#define UI_COLOR_RGBA1_EFE4B0FF V4(0.937254901961f, 0.894117647059f, 0.690196078431f, 1.0f)
#define UI_COLOR_RGBA1_99D9EAFF V4(0.6f,            0.850980392157f, 0.917647058824f, 1.0f)
#define UI_COLOR_RGBA1_C3C3C3FF V4(0.764705882353f, 0.764705882353f, 0.764705882353f, 1.0f)
#define UI_COLOR_RGBA1_CFEEF5FF V4(0.811764705882f, 0.933333333333f, 0.960784313725f, 1.0f)
#define UI_COLOR_RGBA1_E1E1E1FF V4(0.882352941176f, 0.882352941176f, 0.882352941176f, 1.0f)
#define UI_COLOR_RGBA1_FFFFFFFF V4(1.0f,            1.0f,            1.0f           , 1.0f)

global_variable u32 TagValueCounts[Tag_Count] =
{
    Value_Count,        Value_Count,         AnimationType_Count,
    Asset_Count,        Value_Count,         FontType_Count,
    Value_Count,        BiomeType_Count,     TileType_Count,
    Height_Count,       CliffHillType_Count, TileSurface_Count,
    TileSurface_Count,  TreeType_Count,      LightLevel_Count,
    SizeLevel_Count,    Color_Count,         VarietyType_Count,
    MagicElement_Count, Sex_Count,           Age_Count,
    Color_Count,        Beard_Count,         Accessories_Count,
    TopOutfit_Count,    Color_Count,         BottomOutfit_Count,
    Color_Count,        NPCName_Count,       
    QuestType_Count,
    QuestName_Count,    Haircut_Count,       Spell_Count,
    MagicEffect_Count,  ItemName_Count,      FileData_Count,
    MusicType_Count,    SoundEffect_Count,   PropType_Count,
};

#define ENGINE_SHARED_H
#endif

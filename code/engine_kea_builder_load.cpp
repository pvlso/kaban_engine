/* ========================================================================
   $File: $
   $Date: 2024 $
   $Revision: $
   $Creator: BabyKaban $
   $Notice:  $
   ======================================================================== */
// NOTE(pvlso): PNGs are read with stb_image, its allocations go through the platform
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#define STBI_MALLOC(Size) Platform.AllocateMemory(Size)
#define STBI_REALLOC_SIZED(Memory, OldSize, NewSize) Platform.ReallocateMemory(Memory, (u32)(OldSize), (u32)(NewSize))
#define STBI_FREE(Memory) Platform.DeallocateMemory(Memory)
#define STBI_ASSERT(Expression) do {Assert(Expression);} while(0)
#define STB_IMAGE_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include "stb_image.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// NOTE(pvlso): Premultiplied alpha, in the bitmaps' u32 layout (A R G B from the top byte)
inline u32
PremultipliedTexel(v4 Texel)
{
    Texel = SRGB255ToLinear1(Texel);
    Texel.rgb *= Texel.a;
    Texel = Linear1ToSRGB255(Texel);

    u32 Result = (((u32)(Texel.a + 0.5f) << 24) |
                  ((u32)(Texel.r + 0.5f) << 16) |
                  ((u32)(Texel.g + 0.5f) << 8) |
                  ((u32)(Texel.b + 0.5f) << 0));
    return(Result);
}

// NOTE(pvlso): Bottom row first, like a BMP. The pixels go on Arena, or Platform.AllocateMemory without one. A normal
// map's pixels are directions, not colours: they're kept as they are.
internal loaded_bitmap
ParsePNG(char *FileName, read_file_result File, memory_arena *Arena, b32 NormalMap)
{
    loaded_bitmap Result = {};

    s32 Width = 0;
    s32 Height = 0;
    s32 Components = 0;
    u8 *Pixels = stbi_load_from_memory((u8 *)File.Contents, (int)File.Size, &Width, &Height, &Components, 4);
    if(Pixels)
    {
        Result.Width = Width;
        Result.Height = Height;
        Result.WidthOverHeight = (r32)Width / (r32)Height;
        Result.AlignPercentage = V2(0.5f, 0.5f);
        Result.Pitch = Width*BITMAP_BYTES_PER_PIXEL;

        umm Size = (umm)Result.Pitch*(umm)Height;
        Result.Memory = (Arena ? PushSize(Arena, Size) : Platform.AllocateMemory(Size));

        u32 *Dest = (u32 *)Result.Memory;
        for(s32 Y = 0;
            Y < Height;
            ++Y)
        {
            u8 *Source = Pixels + 4*Width*(Height - 1 - Y);
            for(s32 X = 0;
                X < Width;
                ++X)
            {
                if(NormalMap)
                {
                    *Dest++ = (((u32)Source[3] << 24) | ((u32)Source[0] << 16) | ((u32)Source[1] << 8) | (u32)Source[2]);
                }
                else
                {
                    *Dest++ = PremultipliedTexel(V4(Source[0], Source[1], Source[2], Source[3]));
                }
                Source += 4;
            }
        }

        stbi_image_free(Pixels);
    }
    else
    {
        LOG_ERROR("%s isn't a PNG stb_image can read (%s)", FileName, stbi_failure_reason());
    }

    return(Result);
}

internal loaded_bitmap
ParseBMP(read_file_result ReadResult)
{
    loaded_bitmap Result = {};

    if(ReadResult.Size != 0)
    {
        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        Result.Memory = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;
        Result.WidthOverHeight = (r32)Result.Width / (r32)Result.Height;
        Result.AlignPercentage = V2(0.5f, 0.5f);
        
        Assert(Result.Height >= 0);
        Assert(Header->Compression == 3);

        // NOTE(casey): If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EITHER DIRECTION and
        // the height will be negative for top-down.
        // (Also, there can be compression, etc., etc... DON'T think this
        // is complete BMP loading code because it isn't!!)

        // NOTE(casey): Byte order in memory is determined by the Header itself,
        // so we have to read out the masks and convert the pixels ourselves.
        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);        
        
        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);
        
        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        int32 RedShiftDown = (int32)RedScan.Index;
        int32 GreenShiftDown = (int32)GreenScan.Index;
        int32 BlueShiftDown = (int32)BlueScan.Index;
        int32 AlphaShiftDown = (int32)AlphaScan.Index;
        
        uint32 *SourceDest = Pixels;
        for(int32 Y = 0;
            Y < Header->Height;
            ++Y)
        {
            for(int32 X = 0;
                X < Header->Width;
                ++X)
            {
                uint32 C = *SourceDest;

                v4 Texel  =
                    {
                        (real32)((C & RedMask) >> RedShiftDown),
                        (real32)((C & GreenMask) >> GreenShiftDown),
                        (real32)((C & BlueMask) >> BlueShiftDown),
                        (real32)((C & AlphaMask) >> AlphaShiftDown)
                    };

                *SourceDest++ = PremultipliedTexel(Texel);
            }
        }
    }

    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;

    return(Result);
}

// NOTE(pvlso): A .bmp or a .png of a data directory (told apart by their first bytes), bottom row first and
// premultiplied. Without an Arena the memory comes from the platform, Platform.FreeFileMemory frees a PNG's
// (a BMP's pixels are inside its file's memory). Normal maps are PNGs.
internal loaded_bitmap
LoadImageFile(char *FileName, platform_file_type Type, memory_arena *Arena, b32 NormalMap = false)
{
    loaded_bitmap Result = {};

    read_file_result ReadResult = Platform.ReadEntireFile(FileName, Type, Arena, 0);
    u8 *Bytes = (u8 *)ReadResult.Contents;
    if((ReadResult.Size >= 8) &&
       (Bytes[0] == 0x89) && (Bytes[1] == 'P') && (Bytes[2] == 'N') && (Bytes[3] == 'G'))
    {
        Result = ParsePNG(FileName, ReadResult, Arena, NormalMap);
        if(!Arena)
        {
            Platform.FreeFileMemory(ReadResult.Contents);
        }
    }
    else
    {
        Result = ParseBMP(ReadResult);
    }

    return(Result);
}

struct riff_iterator
{
    uint8 *At;
    uint8 *Stop;
};

inline riff_iterator
ParseChunkAt(void *At, void *Stop)
{
    riff_iterator Iter;

    Iter.At = (uint8 *)At;
    Iter.Stop = (uint8 *)Stop;

    return(Iter);
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Size = (Chunk->Size + 1) & ~1;
    Iter.At += sizeof(WAVE_chunk) + Size;

    return(Iter);
}

inline bool32
IsValid(riff_iterator Iter)
{
    bool32 Result = (Iter.At < Iter.Stop);

    return(Result);
}

inline void *
GetChunkData(riff_iterator Iter)
{
    void *Result = (Iter.At + sizeof(WAVE_chunk)); 

    return(Result);
}

inline uint32
GetType(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->ID;

    return(Result);
}

inline uint32
GetChunkDataSize(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->Size;

    return(Result);
}

internal loaded_sound
LoadWAV(char *FileName, u32 SectionFirstSampleIndex, u32 SectionSampleCount, void **Free, memory_arena *Arena)
{
    loaded_sound Result = {};
    
    read_file_result ReadResult = Platform.ReadEntireFile(FileName, PlatformFileType_WAV, Arena, 0);    
    if(ReadResult.Size != 0)
    {
        if(!Arena)
        {
            *Free = ReadResult.Contents;
        }

        WAVE_header *Header = (WAVE_header *)ReadResult.Contents;
        Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
        Assert(Header->WAVEID == WAVE_ChunkID_WAVE);

        uint32 ChannelCount = 0;
        uint32 SampleDataSize = 0;
        int16 *SampleData = 0;
        for(riff_iterator Iter = ParseChunkAt(Header + 1, (uint8 *)(Header + 1) + Header->Size - 4);
            IsValid(Iter);
            Iter = NextChunk(Iter))
        {
            switch(GetType(Iter))
            {
                case WAVE_ChunkID_fmt:
                {
                    WAVE_fmt *fmt = (WAVE_fmt *)GetChunkData(Iter);
                    Assert(fmt->wFormatTag == 1); // NOTE(casey): Only support PCM
                    Assert(fmt->nSamplesPerSec == 48000);
                    Assert(fmt->wBitsPerSample == 16);
                    Assert(fmt->nBlockAlign == sizeof(int16)*fmt->nChannels);
                    ChannelCount = fmt->nChannels;
                } break;

                case WAVE_ChunkID_data:
                {
                    SampleData = (int16 *)GetChunkData(Iter);
                    SampleDataSize = GetChunkDataSize(Iter);
                } break;
            }
        }

        Assert(ChannelCount && SampleData);

        Result.ChannelCount = ChannelCount;
        u32 SampleCount = SampleDataSize / (ChannelCount*sizeof(int16));
        if(ChannelCount == 1)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = 0;
        }
        else if(ChannelCount == 2)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = SampleData + SampleCount;

            for(uint32 SampleIndex = 0;
                SampleIndex < SampleCount;
                ++SampleIndex)
            {
                int16 Source = SampleData[2*SampleIndex];
                SampleData[2*SampleIndex] = SampleData[SampleIndex];
                SampleData[SampleIndex] = Source;
            }
        }
        else
        {
            Assert(!"Invalid channel count in WAV file");
        }

        // TODO(casey): Load right channels!
        b32 AtEnd = true;
        Result.ChannelCount = 1;
        if(SectionSampleCount)
        {
            Assert((SectionFirstSampleIndex + SectionSampleCount) <= SampleCount);
            AtEnd = ((SectionFirstSampleIndex + SectionSampleCount) == SampleCount);
            SampleCount = SectionSampleCount;
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ++ChannelIndex)
            {
                Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
            }
        }

        if(AtEnd)
        {
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ++ChannelIndex)
            {
                for(u32 SampleIndex = SampleCount;
                    SampleIndex < (SampleCount + 8);
                    ++SampleIndex)
                {
                    Result.Samples[ChannelIndex][SampleIndex] = 0;
                }
            }
        }

        Result.SampleCount = SampleCount;
    }

    return(Result);
}

// NOTE(pvlso): "oak.png" -> "oak_n.png", the normal map the art scripts write next to an image
internal void
NormalMapFileName(char *FileName, char *Dest, u32 DestCount)
{
    char *Dot = 0;
    for(char *At = FileName; *At; ++At)
    {
        if(*At == '.') {Dot = At;}
    }

    if(Dot)
    {
        FormatString(DestCount, Dest, "%.*s_n%s", (int)(Dot - FileName), FileName, Dot);
    }
    else
    {
        FormatString(DestCount, Dest, "%s_n", FileName);
    }
}

// NOTE(pvlso): The sheet's sprites, or with FileName (and NormalMap) the same cut of another image, its normal map
internal builder_loaded_sprites
LoadSprites(kesa_asset *KESAAsset, memory_arena *Arena, char *FileName = 0, b32 NormalMap = false)
{
    builder_loaded_sprites Result = {};
    if(!FileName)
    {
        FileName = KESAAsset->SourceFileName;
    }
    loaded_bitmap SpriteSheetBitmap = LoadImageFile(FileName, PlatformFileType_SSBMP, Arena, NormalMap);

    kesa_spritesheet *Sheet = &KESAAsset->SpriteSheet;
    if(SpriteSheetBitmap.Memory &&
       ((SpriteSheetBitmap.Width < (s32)(Sheet->SpriteCount*Sheet->SpriteWidth)) ||
        (SpriteSheetBitmap.Height < (s32)Sheet->SpriteHeight)))
    {
        LOG_ERROR("%s is smaller than %u sprites of %ux%u", FileName, Sheet->SpriteCount, Sheet->SpriteWidth,
                  Sheet->SpriteHeight);
    }
    else if(SpriteSheetBitmap.Memory)
    {
        Result.Count = KESAAsset->SpriteSheet.SpriteCount;
        Result.Sprites = PushArray(Arena, KESAAsset->SpriteSheet.SpriteCount, loaded_bitmap);
        for(u32 SpriteIndex = 0;
            SpriteIndex < KESAAsset->SpriteSheet.SpriteCount;
            ++SpriteIndex)
        {
            loaded_bitmap *Sprite = Result.Sprites + SpriteIndex;
            Sprite->Width = KESAAsset->SpriteSheet.SpriteWidth;
            Sprite->Height = KESAAsset->SpriteSheet.SpriteHeight;
            Sprite->WidthOverHeight = (r32)Sprite->Width/(r32)Sprite->Height;
            Sprite->Pitch = Sprite->Width*BITMAP_BYTES_PER_PIXEL;
            Sprite->AlignPercentage = V2(0.5f, 0.5f);
            u32 MemorySize = Sprite->Pitch*Sprite->Height;
            Sprite->Memory = (Arena ? PushSize(Arena, MemorySize): Platform.AllocateMemory(MemorySize));

            u8 *SourceRow = (u8 *)(SpriteSheetBitmap.Memory) + SpriteIndex*Sprite->Pitch;
            u8 *DestRow = (u8 *)(Sprite->Memory);
            for(s32 Y = 0;
                Y < Sprite->Height;
                ++Y)
            {
                u32 *Source = (u32 *)SourceRow;
                u32 *Dest = (u32 *)DestRow;
                for(s32 X = 0;
                    X < Sprite->Width;
                    ++X)
                {
                    *Dest++ = *Source++;
                }

                SourceRow += SpriteSheetBitmap.Pitch;
                DestRow += Sprite->Pitch;
            }
        }
    }

    return(Result);
}

internal loaded_text
LoadText(char *FileName, memory_arena *Arena)
{
    loaded_text Result = {};
    read_file_result ReadResult = Platform.ReadEntireFile(FileName, PlatformFileType_TXT, Arena, true);
    Result.String = (char *)ReadResult.Contents;

    return(Result);
}

